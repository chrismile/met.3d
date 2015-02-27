/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**
**  Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  Met.3D is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Met.3D is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Met.3D.  If not, see <http://www.gnu.org/licenses/>.
**
*******************************************************************************/
#include "msystemcontrol.h"
#include "ui_msystemcontrol.h"

// standard library imports
#include <iostream>
#include <stdexcept>

// related third party imports
#include "qteditorfactory.h"
#include <log4cplus/loggingmacros.h>

// local application imports
#include "mainwindow.h"
#include "gxfw/msceneviewglwidget.h"
#include "util/mutil.h"
#include "data/lrumemorymanager.h"

using namespace QtExtensions;

namespace Met3D
{

MSystemManagerAndControl* MSystemManagerAndControl::instance = 0;

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSystemManagerAndControl::MSystemManagerAndControl(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MSystemControl),
    mainWindow(nullptr),
    naturalEarthDataLoader(nullptr)
{
    ui->setupUi(this);

    groupPropertyManager           = new QtGroupPropertyManager(this);
    boolPropertyManager            = new QtBoolPropertyManager(this);
    decoratedDoublePropertyManager = new QtDecoratedDoublePropertyManager(this);
    enumPropertyManager            = new QtEnumPropertyManager(this);
    stringPropertyManager          = new QtStringPropertyManager(this);
    clickPropertyManager           = new QtClickPropertyManager(this);

    // The sceneViewPropertiesBrowser needs "GUI editor factories" that provide
    // the required GUI elements (spin boxes, line edits, combo boxes, ...) for
    // editing the properties. NOTE: The factories are deleted by Qt (automatic
    // child destruction..).
    QtCheckBoxFactory *checkBoxFactory = new QtCheckBoxFactory(this);
    QtDecoratedDoubleSpinBoxFactory *decoratedDoubleSpinBoxFactory =
            new QtDecoratedDoubleSpinBoxFactory(this);
    QtEnumEditorFactory *enumEditorFactory = new QtEnumEditorFactory(this);
    QtToolButtonFactory *toolButtonFactory = new QtToolButtonFactory(this);

    // Properties of the scene view are displayed in a tree property browser
    // widget. Connect with the respective property managers (see
    // http://doc.qt.nokia.com/solutions/4/qtpropertybrowser/index.html).
    systemPropertiesBrowser = new QtTreePropertyBrowser(this);
    systemPropertiesBrowser->setFactoryForManager(
                boolPropertyManager, checkBoxFactory);
    systemPropertiesBrowser->setFactoryForManager(
                decoratedDoublePropertyManager, decoratedDoubleSpinBoxFactory);
    systemPropertiesBrowser->setFactoryForManager(
                enumPropertyManager, enumEditorFactory);
    systemPropertiesBrowser->setFactoryForManager(
                clickPropertyManager, toolButtonFactory);

    // Mode for resizing the columns. Useful are QtTreePropertyBrowser::
    // ::ResizeToContents and ::Interactive
    systemPropertiesBrowser->setResizeMode(QtTreePropertyBrowser::ResizeToContents);
    systemPropertiesBrowser->setAlternatingRowColors(true);

    ui->sceneViewPropertiesLayout->addWidget(systemPropertiesBrowser);

    // Insert a dummy "None" entry into the list of waypoints models.
    waypointsTableModelPool.insert("None", nullptr);
    syncControlPool.insert("None", nullptr);
}


MSystemManagerAndControl::~MSystemManagerAndControl()
{
    // release all registered resources
    LOG4CPLUS_DEBUG(mlog, "Freeing system resources...");

    LOG4CPLUS_DEBUG(mlog, "\tscheduler pool");
    for (auto it = schedulerPool.begin(); it != schedulerPool.end(); ++it)
    {
        std::string key = it.key().toStdString();
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting \''" << key << "''");
        delete it.value();
    }

    // currently not working because of data request failure

    /*LOG4CPLUS_DEBUG(mlog, "\tmemory manager pool" << flush);
    for (auto it = memoryManagerPool.begin();
               it != memoryManagerPool.end(); ++it)
    {
        std::string key = it.key().toStdString();
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleted ''" << key << "''" << flush);

        delete it.value();
    }*/

    LOG4CPLUS_DEBUG(mlog, "\tdata source pool");
    for (auto it = dataSourcePool.begin(); it != dataSourcePool.end(); ++it)
    {
        std::string key = it.key().toStdString();
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting \''" << key << "''");
        delete it.value();
    }

    LOG4CPLUS_DEBUG(mlog, "\tsynchronization control pool");
    for (auto it = syncControlPool.begin(); it != syncControlPool.end(); ++it)
    {
        std::string key = it.key().toStdString();
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting \''" << key << "''");
        delete it.value();
    }

    LOG4CPLUS_DEBUG(mlog, "\twaypoints model pool");
    for (auto it = waypointsTableModelPool.begin();
         it != waypointsTableModelPool.end(); ++it)
    {
        std::string key = it.key().toStdString();
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting \''" << key << "''");
        delete it.value();
    }

    if (naturalEarthDataLoader) delete naturalEarthDataLoader;
    delete systemPropertiesBrowser;
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MSystemManagerAndControl* MSystemManagerAndControl::getInstance(QWidget *parent)
{
    if (MSystemManagerAndControl::instance == nullptr)
    {
        MSystemManagerAndControl::instance = new MSystemManagerAndControl(parent);
    }

    return MSystemManagerAndControl::instance;
}


void MSystemManagerAndControl::storeApplicationCommandLineArguments(
        QStringList arguments)
{
    commandLineArguments = arguments;
}


const QStringList &MSystemManagerAndControl::getApplicationCommandLineArguments() const
{
    return commandLineArguments;
}


void MSystemManagerAndControl::registerSceneView(MSceneViewGLWidget *view)
{
    // Add the view's properties to the property browser.
    QtBrowserItem *item = systemPropertiesBrowser
            ->addProperty(view->getPropertyGroup());
    // Set a light blue as background colour (the colour is taken from the
    // QtDesigner property editor (screenshot and GIMP..)).
    systemPropertiesBrowser->setBackgroundColor(item, QColor(191, 255, 191));
    collapsePropertyTree(view->getPropertyGroup());

    registeredViews.append(view);
}


QLabel* MSystemManagerAndControl::renderTimeLabel()
{
    return ui->renderTimeLabel;
}


QtGroupPropertyManager* MSystemManagerAndControl::getGroupPropertyManager()
{
    return groupPropertyManager;
}


QtBoolPropertyManager* MSystemManagerAndControl::getBoolPropertyManager()
{
    return boolPropertyManager;
}


QtDecoratedDoublePropertyManager* MSystemManagerAndControl::getDecoratedDoublePropertyManager()
{
    return decoratedDoublePropertyManager;
}


QtEnumPropertyManager* MSystemManagerAndControl::getEnumPropertyManager()
{
    return enumPropertyManager;
}


QtStringPropertyManager* MSystemManagerAndControl::getStringPropertyManager()
{
    return stringPropertyManager;
}


QtClickPropertyManager* MSystemManagerAndControl::getClickPropertyManager()
{
    return clickPropertyManager;
}


void MSystemManagerAndControl::addProperty(QtProperty *property)
{
    QtBrowserItem *item = systemPropertiesBrowser->addProperty(property);
    systemPropertiesBrowser->setBackgroundColor(item, QColor(191, 255, 191));
    collapsePropertyTree(property);
}


void MSystemManagerAndControl::setMainWindow(MMainWindow *window)
{
    mainWindow = window;
}


MMainWindow* MSystemManagerAndControl::getMainWindow()
{
    return mainWindow;
}


void MSystemManagerAndControl::registerScheduler(
        const QString &id, MAbstractScheduler *scheduler)
{
    schedulerPool.insert(id, scheduler);
}


void MSystemManagerAndControl::registerMemoryManager(
        const QString& id, MAbstractMemoryManager* memoryManager)
{
    memoryManagerPool.insert(id, memoryManager);
}


void MSystemManagerAndControl::registerDataSource(
        const QString &id, MAbstractDataSource* dataSource)
{
    dataSourcePool.insert(id, dataSource);
}


QStringList MSystemManagerAndControl::getDataSourceIdentifiers() const
{
    return dataSourcePool.keys();
}


MAbstractScheduler *MSystemManagerAndControl::getScheduler(const QString& id) const
{
    return schedulerPool.value(id);
}


MAbstractMemoryManager* MSystemManagerAndControl::getMemoryManager(
        const QString& id) const
{
    return memoryManagerPool.value(id);
}


MAbstractDataSource* MSystemManagerAndControl::getDataSource(
        const QString& id) const
{
    if ( !dataSourcePool.contains(id) )
        return nullptr;
    else
        return dataSourcePool.value(id);
}


void MSystemManagerAndControl::registerSyncControl(MSyncControl *syncControl)
{
    syncControlPool.insert(syncControl->getID(), syncControl);
}


MSyncControl *MSystemManagerAndControl::getSyncControl(
        const QString &id) const
{
    if ( !syncControlPool.contains(id) )
    {
        LOG4CPLUS_ERROR(mlog, "Synchronization control with ID "
                        << id.toStdString()
                        << " is not available!");
        return nullptr;
    }
    return syncControlPool.value(id);
}


QStringList MSystemManagerAndControl::getSyncControlIdentifiers() const
{
    return syncControlPool.keys();
}


void MSystemManagerAndControl::registerWaypointsModel(MWaypointsTableModel *wps)
{
    waypointsTableModelPool.insert(wps->getID(), wps);
}


MWaypointsTableModel *MSystemManagerAndControl::getWaypointsModel(
        const QString &id) const
{
    if ( !waypointsTableModelPool.contains(id) )
    {
        LOG4CPLUS_ERROR(mlog, "Waypoints model with ID " << id.toStdString()
                        << " is not available!");
        return nullptr;
    }
    return waypointsTableModelPool.value(id);
}


QStringList MSystemManagerAndControl::getWaypointsModelsIdentifiers() const
{
    return waypointsTableModelPool.keys();
}


double MSystemManagerAndControl::elapsedTimeSinceSystemStart(
        const MStopwatch::TimeUnits units)
{
    systemStopwatch.split();
    return systemStopwatch.getElapsedTime(units);
}


MNaturalEarthDataLoader *MSystemManagerAndControl::getNaturalEarthDataLoader()
{
    if (naturalEarthDataLoader == nullptr)
        naturalEarthDataLoader = new MNaturalEarthDataLoader();
    return naturalEarthDataLoader;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MSystemManagerAndControl::collapsePropertyTree(QtProperty *property)
{
    QtBrowserItem *item = systemPropertiesBrowser->topLevelItem(property);

    // Collapse the item..
    systemPropertiesBrowser->setExpanded(item, false);

    // ..and collapse its children items.
    QList<QtBrowserItem*> childrenItems = item->children();
    while (!childrenItems.isEmpty())
    {
        QtBrowserItem* child = childrenItems.takeFirst();
        systemPropertiesBrowser->setExpanded(child, false);
        childrenItems.append(child->children());
    }
}

} // namespace Met3D
