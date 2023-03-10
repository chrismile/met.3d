/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2017-2018 Bianca Tost
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
    met3dAppIsInitialized(false),
    connectedToMetview(false),
    batchModeIsActive(false),
    batchModeTimeRange_sec(0),
    batchModeQuitWhenCompleted(false),
    batchModeOverwriteImages(true),
    handleSize(.5),
    mainWindow(nullptr)
{
    LOG4CPLUS_DEBUG(mlog, "Initialising system manager...");
    ui->setupUi(this);

    groupPropertyManager           = new QtGroupPropertyManager(this);
    boolPropertyManager            = new QtBoolPropertyManager(this);
    decoratedDoublePropertyManager = new QtDecoratedDoublePropertyManager(this);
    doublePropertyManager          = new QtDoublePropertyManager(this);
    scientificDoublePropertyManager= new QtScientificDoublePropertyManager(this);
    enumPropertyManager            = new QtEnumPropertyManager(this);
    stringPropertyManager          = new QtStringPropertyManager(this);
    clickPropertyManager           = new QtClickPropertyManager(this);
    colorPropertyManager           = new QtColorPropertyManager(this);
    pointFPropertyManager          = new QtPointFPropertyManager(this);

    // The sceneViewPropertiesBrowser needs "GUI editor factories" that provide
    // the required GUI elements (spin boxes, line edits, combo boxes, ...) for
    // editing the properties. NOTE: The factories are deleted by Qt (automatic
    // child destruction..).
    QtCheckBoxFactory *checkBoxFactory = new QtCheckBoxFactory(this);
    QtDecoratedDoubleSpinBoxFactory *decoratedDoubleSpinBoxFactory =
            new QtDecoratedDoubleSpinBoxFactory(this);
    QtDoubleSpinBoxFactory *doubleSpinBoxFactory =
            new QtDoubleSpinBoxFactory(this);
    QtScientificDoubleSpinBoxFactory *scientificDoubleSpinBoxFactory =
            new QtScientificDoubleSpinBoxFactory(this);
    QtEnumEditorFactory *enumEditorFactory = new QtEnumEditorFactory(this);
    QtToolButtonFactory *toolButtonFactory = new QtToolButtonFactory(this);
    QtColorEditorFactory *colorEditorFactory = new QtColorEditorFactory(this);

    // Properties of the scene view are displayed in a tree property browser
    // widget. Connect with the respective property managers (see
    // http://doc.qt.nokia.com/solutions/4/qtpropertybrowser/index.html).
    systemPropertiesBrowser = new QtTreePropertyBrowser(this);
    systemPropertiesBrowser->setFactoryForManager(
                boolPropertyManager, checkBoxFactory);
    systemPropertiesBrowser->setFactoryForManager(
                decoratedDoublePropertyManager, decoratedDoubleSpinBoxFactory);
    systemPropertiesBrowser->setFactoryForManager(doublePropertyManager,
                                                  doubleSpinBoxFactory);
    systemPropertiesBrowser->setFactoryForManager(
                scientificDoublePropertyManager, scientificDoubleSpinBoxFactory);
    systemPropertiesBrowser->setFactoryForManager(
                enumPropertyManager, enumEditorFactory);
    systemPropertiesBrowser->setFactoryForManager(
                clickPropertyManager, toolButtonFactory);
    systemPropertiesBrowser->setFactoryForManager(
                colorPropertyManager, colorEditorFactory);
    systemPropertiesBrowser->setFactoryForManager(
                pointFPropertyManager->subDoublePropertyManager(),
                doubleSpinBoxFactory);

    // Mode for resizing the columns. Useful are QtTreePropertyBrowser::
    // ::ResizeToContents and ::Interactive
    systemPropertiesBrowser->setResizeMode(QtTreePropertyBrowser::ResizeToContents);
    systemPropertiesBrowser->setAlternatingRowColors(true);

    ui->sceneViewPropertiesLayout->addWidget(systemPropertiesBrowser);

    // Insert a dummy "None" entry into the list of waypoints models.
    waypointsTableModelPool.insert("None", nullptr);
    syncControlPool.insert("None", nullptr);
    boundingBoxPool.insert(QString("None"), nullptr);

    // Add group containing general application configurations.
    appConfigGroupProperty =
            groupPropertyManager->addProperty("Application configuration");
    addProperty(appConfigGroupProperty);
    collapsePropertyTree(appConfigGroupProperty);

    // Add group containing click properties to load and save the window layout.
    windowLayoutGroupProperty =
            groupPropertyManager->addProperty("window layout");
    appConfigGroupProperty->addSubProperty(windowLayoutGroupProperty);

    loadWindowLayoutProperty = clickPropertyManager->addProperty("load");
    windowLayoutGroupProperty->addSubProperty(loadWindowLayoutProperty);
    saveWindowLayoutProperty = clickPropertyManager->addProperty("save");
    windowLayoutGroupProperty->addSubProperty(saveWindowLayoutProperty);

    // Add group containing .
    allSceneViewsGroupProperty =
            groupPropertyManager->addProperty("All scene views");
    addProperty(allSceneViewsGroupProperty);
    collapsePropertyTree(allSceneViewsGroupProperty);

    handleSizeProperty =
            doublePropertyManager->addProperty("handle size");
    doublePropertyManager->setMinimum(handleSizeProperty, 0.01);
    doublePropertyManager->setValue(handleSizeProperty, handleSize);
    doublePropertyManager->setSingleStep(handleSizeProperty, 0.1);
    allSceneViewsGroupProperty->addSubProperty(handleSizeProperty);

    // Connect double property to actOnQtPropertyChange to handle user
    // interaction with the double properties added.
    connect(doublePropertyManager,
            SIGNAL(propertyChanged(QtProperty*)),
            SLOT(actOnQtPropertyChanged(QtProperty*)));
    // Connect click property to actOnQtPropertyChange to handle user
    // interaction with the click properties added.
    connect(clickPropertyManager,
            SIGNAL(propertyChanged(QtProperty*)),
            SLOT(actOnQtPropertyChanged(QtProperty*)));

    // Determine the Met.3D home directory (the base directory to find
    // shader files and data files that do not change).
    met3DHomeDir = QDir(QProcessEnvironment::systemEnvironment().value(
                            "MET3D_HOME"));
    LOG4CPLUS_DEBUG(mlog, "  > MET3D_HOME set to "
                    << met3DHomeDir.absolutePath().toStdString());

    defaultMemoryManagers.insert("NWP", "");
    defaultMemoryManagers.insert("Analysis", "");
    defaultMemoryManagers.insert("Trajectories", "");
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
    foreach (MAbstractDataSource *dataSource, dataSourcePool)
    {
        std::string key = dataSourcePool.key(dataSource).toStdString();
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting \''" << key << "''");
        delete dataSource;
    }

    LOG4CPLUS_DEBUG(mlog, "\tsynchronization control pool");
    foreach (MSyncControl *syncControl, syncControlPool)//auto it = syncControlPool.begin(); it != syncControlPool.end(); ++it)
    {
        std::string key = syncControlPool.key(syncControl).toStdString();
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting \''" << key << "''");
        delete syncControl;
    }

    LOG4CPLUS_DEBUG(mlog, "\twaypoints model pool");
    foreach (MWaypointsTableModel *waypointsTableModel, waypointsTableModelPool)
    {
        std::string key =
                waypointsTableModelPool.key(waypointsTableModel).toStdString();
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting \''" << key << "''");
        delete waypointsTableModel;
    }

    LOG4CPLUS_DEBUG(mlog, "\tbounding box pool");
    for (auto it = boundingBoxPool.begin(); it != boundingBoxPool.end(); ++it)
    {
        std::string key = it.key().toStdString();
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting \''" << key << "''");
        delete it.value();
    }

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

    // Parse command line arguments to check if application has been started
    // from Metview.
    foreach (QString arg, commandLineArguments)
    {
        if (arg.startsWith("--metview"))
        {
            connectedToMetview = true;
            QString msg = QString("Starting in Metview mode. ");
            LOG4CPLUS_INFO(mlog, msg.toStdString());
        }
    }
}


const QStringList &MSystemManagerAndControl::getApplicationCommandLineArguments() const
{
    return commandLineArguments;
}


const QDir& MSystemManagerAndControl::getMet3DHomeDir() const
{
    return met3DHomeDir;
}


void MSystemManagerAndControl::setMet3DWorkingDirectory(QString workingDir)
{
    this->met3DWorkingDirectory = QDir(workingDir);
}


void MSystemManagerAndControl::registerSceneView(MSceneViewGLWidget *view)
{
    // Add the view's properties to the property browser.
    addProperty(view->getPropertyGroup());
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


QtScientificDoublePropertyManager*
MSystemManagerAndControl::getScientificDoublePropertyManager()
{
    return scientificDoublePropertyManager;
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


QtColorPropertyManager* MSystemManagerAndControl::getColorPropertyManager()
{
    return colorPropertyManager;
}


QtPointFPropertyManager* MSystemManagerAndControl::getPointFPropertyManager()
{
    return pointFPropertyManager;
}


void MSystemManagerAndControl::addProperty(QtProperty *property)
{
    QtBrowserItem *item = systemPropertiesBrowser->addProperty(property);
    // Set the background colour of the entry in the system control's property
    // browser.
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

    // Let the main window monitor whether the schedule is busy.
    connect(scheduler, SIGNAL(schedulerIsProcessing(bool)),
            mainWindow, SLOT(partOfApplicationIsBusyEvent(bool)));
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


QStringList MSystemManagerAndControl::getMemoryManagerIdentifiers() const
{
    return memoryManagerPool.keys();
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

    foreach (MSceneViewGLWidget *sceneview, registeredViews)
    {
        sceneview->updateSyncControlProperty();
    }
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


void MSystemManagerAndControl::removeSyncControl(MSyncControl *syncControl)
{
    syncControlPool.remove(syncControl->getID());
    delete syncControl;

    foreach (MSceneViewGLWidget *sceneview, registeredViews)
    {
        sceneview->updateSyncControlProperty();
    }
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


void MSystemManagerAndControl::registerBoundingBox(MBoundingBox *bbox)
{
    boundingBoxPool.insert(bbox->getID(), bbox);
    emit boundingBoxCreated();
}


void MSystemManagerAndControl::deleteBoundingBox(const QString& id)
{
    delete boundingBoxPool.value(id);
    boundingBoxPool.remove(id);
    emit boundingBoxDeleted(id);
}


void MSystemManagerAndControl::renameBoundingBox(const QString& oldId,
                                                 MBoundingBox *bbox)
{
    boundingBoxPool.remove(oldId);
    boundingBoxPool.insert(bbox->getID(), bbox);
    emit boundingBoxRenamed();
}


MBoundingBox *MSystemManagerAndControl::getBoundingBox(const QString &id) const
{
    if ( !boundingBoxPool.contains(id) )
    {
        LOG4CPLUS_ERROR(mlog, "Bounding box with ID " << id.toStdString()
                        << " is not available!");
        return nullptr;
    }
    return boundingBoxPool.value(id);
}


QStringList MSystemManagerAndControl::getBoundingBoxesIdentifiers() const
{
    return boundingBoxPool.keys();
}


MBoundingBoxDockWidget *MSystemManagerAndControl::getBoundingBoxDock() const
{
    return mainWindow->getBoundingBoxDock();
}


double MSystemManagerAndControl::elapsedTimeSinceSystemStart(
        const MStopwatch::TimeUnits units)
{
    systemStopwatch.split();
    return systemStopwatch.getElapsedTime(units);
}


void MSystemManagerAndControl::setBatchMode(bool isActive, QString animType, QString syncName,
        QString dataSourceIDForStartTime, int timeRange_sec,
        bool quitWhenCompleted, bool overwriteImages)
{
    batchModeIsActive = isActive;
    batchModeAnimationType = animType;
    syncControlForBatchModeAnimation = syncName;
    batchModeDataSourceIDToGetStartTime = dataSourceIDForStartTime;
    batchModeTimeRange_sec = timeRange_sec;
    batchModeQuitWhenCompleted = quitWhenCompleted;
    batchModeOverwriteImages = overwriteImages;
}


bool MSystemManagerAndControl::isInBatchMode()
{
    return batchModeIsActive;
}


void MSystemManagerAndControl::executeBatchMode()
{
    LOG4CPLUS_DEBUG(mlog, "Starting batch mode execution.");

    // Check if sync control and data source configured for batch mode exist.
    MSyncControl *syncControl = getSyncControl(syncControlForBatchModeAnimation);
    if (syncControl == nullptr)
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("Batch mode execution: Synchronization control'"
                       + syncControlForBatchModeAnimation +
                       "' specified in frontend configuration is not available. "
                       "Batch mode will NOT be executed.");
        msgBox.exec();
        return;
    }

    if (getDataSource(batchModeDataSourceIDToGetStartTime) == nullptr)
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("Batch mode execution: Data source '"
                       + batchModeDataSourceIDToGetStartTime +
                       "' specified in frontend configuration is not available. "
                       "Batch mode will NOT be executed.");
        msgBox.exec();
        return;
    }

    // Restrict the sync control's allowed init/valid time to those available
    // from the data source. Also reset the init/valid time GUI elements to
    // the first available init/valid time of the data source (the second
    // "true" argument), and set the animation time range to the specified
    // value in seconds.
    QStringList dataSources;
    dataSources.append(batchModeDataSourceIDToGetStartTime);
    syncControl->restrictControlToDataSources(dataSources, true);
    syncControl->setAnimationTimeRange(batchModeTimeRange_sec);

    // W.r.t. animation type, currently only the 'timeAnimation' option is
    // implemented.
    if (batchModeAnimationType == "timeAnimation")
    {
        // If configured so, connect the sync control's "timeAnimationEnds"
        // signal to the "closeMainWindow" slot to automatically quit the
        // application after the batch animation has finished. Use
        // "Qt::QueuedConnection" as described here:
        // https://doc.qt.io/qt-5/qcoreapplication.html#quit
        if (batchModeQuitWhenCompleted)
        {
            connect(syncControl, SIGNAL(timeAnimationEnds()), this,
                    SLOT(closeMainWindow()), Qt::QueuedConnection);
        }

        // Force to overwrite image files that already exist?
        syncControl->setOverwriteAnimationImageSequence(batchModeOverwriteImages);

        // WORKAROUNG to avoid black images stored of first time step.
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        // NOTE: If this method is called from MMainWindow::show(), the data
        // requests emitted by the actor initalizations triggered from
        // MGLResourcesManager::initializeGL() may NOT HAVE COMPLETED at this
        // time! Unfortunately, if we store the first image as implemented in
        // MSyncControl::synchronizationCompleted() and ::startTimeAnimation()
        // after this first sync request has been completed, the image store
        // method is called before rendering has finished. As a workaround, we
        // delay the start of the animation by the animation delay specified
        // by the user in the animation pane in the sync control.

        // Start the time animation, delayed.
        unsigned int delay_ms = syncControl->getAnimationDelay_ms();
        LOG4CPLUS_DEBUG(mlog, "Delaying start of batch animation by "
                        << delay_ms << " ms so that first rendering cycle is "
                        "completed and first image is produced correctly.");
        QTimer::singleShot(delay_ms, syncControl,
                           SLOT(startTimeAnimationProgrammatically()));
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("Batch mode execution: Animation type '"
                       + batchModeAnimationType +
                       "' is not supported ('timeAnimation' is supported.). "
                       "Batch mode will NOT be executed.");
        msgBox.exec();
        return;
    }
}


void MSystemManagerAndControl::setApplicationConfigurationValue(
        QString key, QVariant item)
{
    applicationConfigurationValues.insert(key, item);
}


QVariant MSystemManagerAndControl::getApplicationConfigurationValue(QString key)
{
    return applicationConfigurationValues.value(key);
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MSystemManagerAndControl::actOnQtPropertyChanged(QtProperty *property)
{
    if (property == loadWindowLayoutProperty)
    {
        mainWindow->loadConfigurationFromFile("");

    }
    else if (property == saveWindowLayoutProperty)
    {
        mainWindow->saveConfigurationToFile("");
    }
    else if (property == handleSizeProperty)
    {
        handleSize = doublePropertyManager->value(handleSizeProperty);
        foreach (MSceneViewGLWidget *sceneView, registeredViews)
        {
            sceneView->onHandleSizeChanged();
        }
    }
}


void MSystemManagerAndControl::closeMainWindow()
{
    LOG4CPLUS_DEBUG(mlog, "System manager received command to quit the "
                    " application. Closing main window... ");

    mainWindow->close();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MSystemManagerAndControl::setApplicationIsInitialized()
{
    met3dAppIsInitialized = true;
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
