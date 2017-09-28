/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2016-2017 Bianca Tost
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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// standard library imports

// related third party imports
#include <QMainWindow>
#include <QVector>

// local application imports
#include "util/mutil.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/mscenecontrol.h"
#include "gxfw/scenemanagementdialog.h"
#include "gxfw/adddatasetdialog.h"
#include "gxfw/sessionmanagerdialog.h"
#include "gxfw/synccontrol.h"
#include "data/waypoints/waypointstableview.h"
#include "gxfw/mresizewindowdialog.h"
#include "gxfw/boundingbox/bboxdockwidget.h"


namespace Ui {
class MainWindow;
}


namespace Met3D
{

class MSystemManagerAndControl;

/**
  Main window of Met.3D.
  */
class MMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MMainWindow(QStringList commandLineArguments, QWidget *parent = 0);
    ~MMainWindow();

    void dockSyncControl(MSyncControl *syncControl);

    void dockSceneControl(MSceneControl *control);

    void changeDockedSceneName(const QString& oldName,
                               const QString& newName);

    void dockWaypointsModel(MWaypointsTableModel *waypointsModel);

    void signalMapperConnect(QList<QComboBox> sceneViewComboBoxes);

    void removeSceneControl(QWidget *widget);

    void removeSyncControl(MSyncControl *syncControl);

    QVector<MSceneViewGLWidget*>& getGLWidgets();

    void resizeSceneView(int newWidth, int newHeight,
                         MSceneViewGLWidget *sceneView);

    MSceneManagementDialog *getSceneManagementDialog()
    { return sceneManagementDialog; }

    MBoundingBoxDockWidget *getBoundingBoxDock()
    { return boundingBoxDock; }

    MSessionManagerDialog *getSessionManagerDialog()
    { return sessionManagerDialog; }

    /**
     Save the window layout configuration to the file @p filename.
     */
    void saveConfigurationToFile(QString filename = "");
    /**
     Load the window layout configuration from the file @p filename.
     */
    void loadConfigurationFromFile(QString filename = "");

    /**
     Save window layout-specific configuration to the @ref QSettings object @p
     settings.
     */
    void saveConfiguration(QSettings *settings);
    /**
     Load window layout-specific configuration from the @ref QSettings object @p
     settings.
     */
    void loadConfiguration(QSettings *settings);
    /**
     Tabify system control with the scene controls.
     */
    void tabifyScenesAndSystem();

    void onSessionsListChanged(QStringList *sessionsList, QString currentSession);

    void onSessionSwitch(QString currentSession);

    void updateSessionTimerInterval(int interval);

public slots:
    void show();
    void closeEvent(QCloseEvent *event);

    void setFullScreen(bool b);

    void showWaypointsTable(bool b);

    void showBoundingBoxTable(bool b);

    void setSceneViewLayout(int layout);

    void sceneManagement();

    void addDataset();

    void openSessionManager();

    void openOnlineManual();

    void openOnlineIssueTracker();

    void showAboutQtDialog();

    void showAboutDialog();

    void resizeWindow();

    void switchSession(QAction *sessionAction);

private:
    Ui::MainWindow *ui;
    QSplitter *mainSplitter;
    QSplitter *rightSplitter;
    QSplitter *topSplitter;
    QSplitter *bottomSplitter;

    /** Hidden QGLWidget whose GL context is used to manage all resources. */
    MGLResourcesManager *glResourcesManager;

    /** QGLWidgets that display scenes and their control widget. */
    QVector<MSceneViewGLWidget*> sceneViewGLWidgets;
    MSystemManagerAndControl *systemManagerAndControl;
    MSceneManagementDialog *sceneManagementDialog;
    MResizeWindowDialog *resizeWindowDialog;
    MSessionManagerDialog *sessionManagerDialog;

    /** List of all dock widgets */
    QList<QDockWidget*> sceneDockWidgets;
    QList<QDockWidget*> syncControlDockWidgets;

    QSignalMapper *signalMapperLayout;

    MWaypointsView *waypointsTableView;
    QDockWidget    *waypointsTableDock;

    MBoundingBoxDockWidget *boundingBoxDock;

    /** Stores the index of the current view layout to simplify saving the
        current state to a config file.*/
    int sceneViewLayout;

    QSettings *sessionSettings;
    QDockWidget *systemDock;

    /**
      Timer used to handle saving the session automatically everytime after the
      time interval selected by the user has passed.
     */
    QTimer *sessionAutoSaveTimer;
};

} // namespace Met3D

#endif // MAINWINDOW_H
