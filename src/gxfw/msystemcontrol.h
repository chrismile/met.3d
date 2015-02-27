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
#ifndef MSCENEVIEWCONTROL_H
#define MSCENEVIEWCONTROL_H

// standard library imports

// related third party imports
#include <QtCore>
#include <QtGui>
#include "qttreepropertybrowser.h"
#include "qtpropertymanager.h"

// local application imports
#include "qt_extensions/qtpropertymanager_extensions.h"
#include "data/scheduler.h"
#include "data/abstractmemorymanager.h"
#include "data/abstractdatasource.h"
#include "data/abstractdatareader.h"
#include "gxfw/synccontrol.h"
#include "data/waypoints/waypointstablemodel.h"
#include "util/mstopwatch.h"
#include "data/naturalearthdataloader.h"


namespace Ui {
    class MSystemControl;
}


namespace Met3D
{

class MMainWindow;
class MSceneViewGLWidget;


/**
  @brief MSystemManagerAndControl manages a number of system resources of the
  Met.3D system (including memory managers and task scheduler) and provides a
  GUI widget that allows the user to view and modify system properties.

  Only a single instance of this control exisits (singleton pattern).
  */
class MSystemManagerAndControl : public QWidget
{
    Q_OBJECT

public:
    ~MSystemManagerAndControl();

    /**
     Returns the (singleton) instance of the system control. If getInstance()
     is called for the first time an optional parent widget can be passed as
     argument.
     */
    static MSystemManagerAndControl* getInstance(QWidget *parent=0);

    void storeApplicationCommandLineArguments(QStringList arguments);

    const QStringList& getApplicationCommandLineArguments() const;

    /**
      Returns a pointer to the group property manager responsible for @ref
      QtProperty instances in the system property tree.
     */
    QtGroupPropertyManager* getGroupPropertyManager();

    QtBoolPropertyManager* getBoolPropertyManager();

    QtExtensions::QtDecoratedDoublePropertyManager* getDecoratedDoublePropertyManager();

    QtEnumPropertyManager* getEnumPropertyManager();

    QtStringPropertyManager * getStringPropertyManager();

    QtExtensions::QtClickPropertyManager* getClickPropertyManager();

    /**
      Returns a pointer to the label that displays render performances.
     */
    QLabel* renderTimeLabel();

    void addProperty(QtProperty* property);

    void setMainWindow(MMainWindow *window);

    MMainWindow* getMainWindow();

    void registerSceneView(MSceneViewGLWidget *view);

    QList<MSceneViewGLWidget*>& getRegisteredViews() { return registeredViews; }

    void registerScheduler(const QString& id, MAbstractScheduler* scheduler);

    MAbstractScheduler* getScheduler(const QString& id) const;

    void registerMemoryManager(const QString& id,
                               MAbstractMemoryManager* memoryManager);

    MAbstractMemoryManager* getMemoryManager(const QString& id) const;

    void registerDataSource(const QString& id,
                            MAbstractDataSource* dataSource);

    MAbstractDataSource* getDataSource(const QString& id) const;

    QStringList getDataSourceIdentifiers() const;

    void registerSyncControl(MSyncControl* syncControl);

    MSyncControl* getSyncControl(const QString& id) const;

    QStringList getSyncControlIdentifiers() const;

    void registerWaypointsModel(MWaypointsTableModel* wps);

    MWaypointsTableModel* getWaypointsModel(const QString& id) const;

    QStringList getWaypointsModelsIdentifiers() const;

    MStopwatch& getSystemStopwatch() { return systemStopwatch; }

    double elapsedTimeSinceSystemStart(const MStopwatch::TimeUnits units);

    MNaturalEarthDataLoader* getNaturalEarthDataLoader();

private:
    /**
     Constructor is private, as it should only be called from getInstance().
     See https://en.wikipedia.org/wiki/Singleton_pattern#Lazy_initialization.
     */
    MSystemManagerAndControl(QWidget *parent=0);

    /**
      Collapse all subproperties of @p propertiy in the system property tree.
     */
    void collapsePropertyTree(QtProperty *property);

    /** Singleton instance of the system control. */
    static MSystemManagerAndControl* instance;

    Ui::MSystemControl *ui;

    QStringList commandLineArguments;

    QtTreePropertyBrowser *systemPropertiesBrowser;

    QtGroupPropertyManager               *groupPropertyManager;
    QtBoolPropertyManager                *boolPropertyManager;
    QtExtensions::QtDecoratedDoublePropertyManager
                                         *decoratedDoublePropertyManager;
    QtEnumPropertyManager                *enumPropertyManager;
    QtStringPropertyManager              *stringPropertyManager;
    QtExtensions::QtClickPropertyManager *clickPropertyManager;

    QList<MSceneViewGLWidget*> registeredViews;

    MMainWindow *mainWindow;

    QMap<QString, MAbstractScheduler*>     schedulerPool;
    QMap<QString, MAbstractMemoryManager*> memoryManagerPool;
    QMap<QString, MAbstractDataSource*>    dataSourcePool;
    QMap<QString, MSyncControl*>           syncControlPool;
    QMap<QString, MWaypointsTableModel*>   waypointsTableModelPool;

    MStopwatch systemStopwatch;
    MNaturalEarthDataLoader *naturalEarthDataLoader;
};

} // namespace Met3D

#endif // MSCENEVIEWCONTROL_H
