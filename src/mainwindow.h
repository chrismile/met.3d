/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2016 Marc Rautenhaus
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
#include "gxfw/synccontrol.h"
#include "data/waypoints/waypointstableview.h"


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

    void dockSyncControl(MSyncControl* syncControl);

    void dockSceneControl(MSceneControl *control);

    void changeDockedSceneName(const QString& oldName,
                               const QString& newName);

    void dockWaypointsModel(MWaypointsTableModel *waypointsModel);

    void signalMapperConnect(QList<QComboBox> sceneViewComboBoxes);

    void removeControl(QWidget *widget);

    QVector<MSceneViewGLWidget*>& getGLWidgets();

public slots:
    void setFullScreen(bool b);

    void showWaypointsTable(bool b);

    void setSceneViewLayout(int layout);

    void sceneManagement();

    void addDataset();

    void openOnlineManual();

    void openOnlineIssueTracker();

    void showAboutQtDialog();

    void showAboutDialog();

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

    /** List of all dock widgets */
    QList<QDockWidget*> dockWidgets;

    QSignalMapper *signalMapperLayout;

    MWaypointsView *waypointsTableView;
    QDockWidget    *waypointsTableDock;
};

} // namespace Met3D

#endif // MAINWINDOW_H
