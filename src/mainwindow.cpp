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
#include "mainwindow.h"
#include "ui_mainwindow.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "gxfw/msystemcontrol.h"
#include "ui_scenemanagementdialog.h"
#include "system/applicationconfiguration.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MMainWindow::MMainWindow(QStringList commandLineArguments, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      sceneManagementDialog(new MSceneManagementDialog)
{
    // Qt Designer specific initialization.
    ui->setupUi(this);

    LOG4CPLUS_DEBUG(mlog, "Initialising Met.3D system ... please wait.");

    // OpenGL settings.
    // =========================================================================

    QGLFormat glformat;  
    // glformat.setVersion(4, 1);
    glformat.setProfile(QGLFormat::CoreProfile);
    glformat.setSampleBuffers(true);

    // System control (dock widget).
    // =========================================================================

    // Get the system control and create a dock widget. Note that this is the
    // first time that MSystemControl::getInstance() is called, hence this
    // window is passed as the parent to the constructor.
    systemManagerAndControl = MSystemManagerAndControl::getInstance(this);
    systemManagerAndControl->setMainWindow(this);
    systemManagerAndControl->storeApplicationCommandLineArguments(
                commandLineArguments);
    QDockWidget* systemDock = new QDockWidget("System", this);
    systemDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    systemDock->setWidget(systemManagerAndControl);

    // Global dock widget settings -- tabs shall appear on the "west" side of
    // the widgets, nesting is allowed.
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::West);
    setDockNestingEnabled(true);

    // OpenGL resources manager -- the "invisible" OpenGL context.
    // =========================================================================

    // Create a hidden QGLWidget as resources manager.
    MGLResourcesManager::initialize(glformat, this);
    glResourcesManager = MGLResourcesManager::getInstance();

    // SCENE VIEWS -- the "visible" OpenGL contexts.
    //==========================================================================

    // Create MET3D_MAX_SCENEVIEWS scene views (defined in mutils.h).
    // The views will exist in memory, but they won't always be visible to the
    // user. The OpenGL context of glResourcesManager is shared to access
    // "global" shaders and GPU memory.
    for (int i = 0; i < MET3D_MAX_SCENEVIEWS; i++)
        sceneViewGLWidgets.append(new MSceneViewGLWidget());

    // See loopview.py about how to use splitters.
    mainSplitter = new QSplitter(Qt::Horizontal);
    rightSplitter = new QSplitter(Qt::Vertical);
    topSplitter = new QSplitter(Qt::Horizontal);
    bottomSplitter = new QSplitter(Qt::Horizontal);

    mainSplitter->addWidget(rightSplitter);
    mainSplitter->addWidget(topSplitter);
    mainSplitter->addWidget(bottomSplitter);

    // Display the widget inside a QLayout instance in the central frame of
    // the window.
    QHBoxLayout *layout = new QHBoxLayout;
    // glResourcesManager hides itself at the end of its "initializeGL()"
    // function and will thus never be shown. However, it needs to be added as
    // a "visible" widget to the layout manager so that a valid OpenGL context
    // is created that can be shared with the actually visible widgets.
    layout->addWidget(glResourcesManager); // this will never be shown
    layout->addWidget(mainSplitter); // contains visible GL device
    ui->centralframe->setLayout(layout);

    // Initial layout settings.
    setSceneViewLayout(1);
    sceneViewGLWidgets[2]->executeCameraAction(MSceneViewGLWidget::TopView,
                                               false);
    sceneViewGLWidgets[2]->setVerticalScaling(1.);

    // Init application system resources.
    // =========================================================================

    MApplicationConfigurationManager appConfig;
    appConfig.loadConfiguration();

    // Assign scenes to scene views.
    //==========================================================================

    sceneViewGLWidgets[0]->setScene(glResourcesManager->getScenes()[0]);
    sceneViewGLWidgets[1]->setScene(glResourcesManager->getScenes()[1]);
    sceneViewGLWidgets[2]->setScene(glResourcesManager->getScenes()[2]);
    sceneViewGLWidgets[3]->setScene(glResourcesManager->getScenes()[3]);

    // Show the control widget of the first scene and add the system control
    // dock below.
    addDockWidget(Qt::LeftDockWidgetArea, systemDock);

    // Uncomment this loop if the system control should be tabified with the
    // scene controls.
    for (int i = 0; i < dockWidgets.size()-1; i++)
        if (!dockWidgets[i]->isFloating())
            tabifyDockWidget(dockWidgets[i], systemDock);

    dockWidgets[0]->raise();

    // Connect signals and slots.
    // ========================================================================

    connect(ui->actionFullScreen, SIGNAL(toggled(bool)),
            this, SLOT(setFullScreen(bool)));
    connect(ui->actionWaypoints, SIGNAL(toggled(bool)),
            this, SLOT(showWaypointsTable(bool)));
    connect(ui->actionSceneManagement, SIGNAL(triggered()),
            this, SLOT(sceneManagement()));

    // Signal mapper to map all camera related menu actions to a single
    // slot within MSceneViewGLWidget. See QSignalMapper documentation
    // for further details.
    signalMapperCamera = new QSignalMapper(this);
    signalMapperCamera->setMapping(ui->actionCameraNorthUp,
                                   MSceneViewGLWidget::NorthUp);
    signalMapperCamera->setMapping(ui->actionCameraTopView,
                                   MSceneViewGLWidget::TopView);
    signalMapperCamera->setMapping(ui->actionRememberCurrentCamera,
                                   MSceneViewGLWidget::RememberCurrentView);
    signalMapperCamera->setMapping(ui->actionRestoreRememberedCamera,
                                   MSceneViewGLWidget::RestoreRememberedView);
    signalMapperCamera->setMapping(ui->actionSaveCameraToFile,
                                   MSceneViewGLWidget::SaveViewToFile);
    signalMapperCamera->setMapping(ui->actionRestoreCameraFromFile,
                                   MSceneViewGLWidget::RestoreFromFile);

    connect(ui->actionCameraNorthUp, SIGNAL(triggered()),
            signalMapperCamera, SLOT(map()));
    connect(ui->actionCameraTopView, SIGNAL(triggered()),
            signalMapperCamera, SLOT(map()));
    connect(ui->actionRememberCurrentCamera, SIGNAL(triggered()),
            signalMapperCamera, SLOT(map()));
    connect(ui->actionRestoreRememberedCamera, SIGNAL(triggered()),
            signalMapperCamera, SLOT(map()));
    connect(ui->actionSaveCameraToFile, SIGNAL(triggered()),
            signalMapperCamera, SLOT(map()));
    connect(ui->actionRestoreCameraFromFile, SIGNAL(triggered()),
            signalMapperCamera, SLOT(map()));

    for (int i = 0; i < sceneViewGLWidgets.size(); i++)
        connect(signalMapperCamera, SIGNAL(mapped(int)),
                sceneViewGLWidgets[i], SLOT(executeCameraAction(int)));

    // Signal mapper to map all layout related menu actions to a single
    // slot (setSceneViewLayout()).
    signalMapperLayout = new QSignalMapper(this);
    signalMapperLayout->setMapping(ui->actionLayoutSingleView, 1);
    signalMapperLayout->setMapping(ui->actionLayoutDualView, 2);
    signalMapperLayout->setMapping(ui->actionLayoutDualViewVertical, 3);
    signalMapperLayout->setMapping(ui->actionLayoutOneLargeTwoSmallViews, 4);
    signalMapperLayout->setMapping(ui->actionLayoutOneLargeThreeSmallViews, 5);
    signalMapperLayout->setMapping(ui->actionLayoutQuadView, 6);

    connect(ui->actionLayoutSingleView, SIGNAL(triggered()),
            signalMapperLayout, SLOT(map()));
    connect(ui->actionLayoutDualView, SIGNAL(triggered()),
            signalMapperLayout, SLOT(map()));
    connect(ui->actionLayoutDualViewVertical, SIGNAL(triggered()),
            signalMapperLayout, SLOT(map()));
    connect(ui->actionLayoutOneLargeTwoSmallViews, SIGNAL(triggered()),
            signalMapperLayout, SLOT(map()));
    connect(ui->actionLayoutOneLargeThreeSmallViews, SIGNAL(triggered()),
            signalMapperLayout, SLOT(map()));
    connect(ui->actionLayoutQuadView, SIGNAL(triggered()),
            signalMapperLayout, SLOT(map()));

    connect(signalMapperLayout, SIGNAL(mapped(int)),
            this, SLOT(setSceneViewLayout(int)));
}


MMainWindow::~MMainWindow()
{
    LOG4CPLUS_DEBUG(mlog, "Freeing application resources.." << flush);

    delete sceneManagementDialog;

    for (int i = 0; i < sceneViewGLWidgets.size(); i++)
        delete sceneViewGLWidgets[i];

    // Deleting mainSplitter implicitly deletes the other splitters, as they
    // are children of mainSplitter.
    delete mainSplitter;

    delete glResourcesManager;
    delete systemManagerAndControl;

    for (int i = 0; i < dockWidgets.size(); i++)
        delete dockWidgets[i]; // aren't these deleted via the parent mechanism?

    delete ui;
    LOG4CPLUS_DEBUG(mlog, "..application resources have been deleted.");
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MMainWindow::dockSyncControl(MSyncControl *syncControl)
{
    // Create a synchronisation control as dock widget.
    QDockWidget* syncDock = new QDockWidget(syncControl->getID(), this);
    syncDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    syncDock->setWidget(syncControl);
    addDockWidget(Qt::LeftDockWidgetArea, syncDock);
}


void MMainWindow::dockSceneControl(MSceneControl *control)
{
    QDockWidget* dock = new QDockWidget(control->getName(), this);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setWidget(control);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    dockWidgets.append(dock);

    for (int i = 0; i < dockWidgets.size()-1; i++)
        if (!dockWidgets[i]->isFloating())
            tabifyDockWidget(dockWidgets[i], dock);
}


void MMainWindow::changeDockedSceneName(const QString& oldName,
                                        const QString& newName)
{
    for(auto& dockWidget : dockWidgets)
    {
        if (dockWidget->windowTitle() == oldName)
        {
            dockWidget->setWindowTitle(newName);
            break;
        }
    }
}


void MMainWindow::dockWaypointsModel(MWaypointsTableModel *waypointsModel)
{
//TODO (mr, 15Oct2014) -- This currently won't work when additional models
//                        are added during runtime.

    waypointsTableView = new MWaypointsView(this);
    waypointsTableView->setWaypointsTableModel(waypointsModel);
    waypointsTableDock = new QDockWidget("Waypoints", this);
    waypointsTableDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    waypointsTableDock->setWidget(waypointsTableView);
    waypointsTableDock->setVisible(false);
    addDockWidget(Qt::BottomDockWidgetArea, waypointsTableDock);
}


void MMainWindow::removeControl(QWidget *widget)
{
    for (int i = 0; i < dockWidgets.size(); i++)
        if (dockWidgets[i]->widget() == widget)
        {
            QDockWidget *dock = dockWidgets.takeAt(i);
            removeDockWidget(dock);
//TODO (mr, 20Oct2014) -- Dock widget is not deleted; deletion also deletes
//                        scene and breaks scene removal from glRM pool
        }
}


QVector<MSceneViewGLWidget*>& MMainWindow::getGLWidgets()
{
    return sceneViewGLWidgets;
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MMainWindow::setFullScreen(bool b)
{
    if (b)
        showFullScreen();
    else
        showNormal();
}


void MMainWindow::showWaypointsTable(bool b)
{
    waypointsTableDock->setVisible(b);
}


void MMainWindow::setSceneViewLayout(int layout)
{
    // Hide everything.
    rightSplitter->hide();
    topSplitter->hide();
    bottomSplitter->hide();
    for (int i = 0; i < sceneViewGLWidgets.size(); i++)
        sceneViewGLWidgets[i]->hide();

    // Determine the width of the main splitter widget (used below to set the
    // size of sub-splitters and labels).
    int w = mainSplitter->width();
    int h = mainSplitter->height();

    // Indices of the scene views that will be shown at the end of this
    // function.
    QVector<int> showSceneViews;
    // Display sizes of the scene views, QList required for
    // QSplitter.setSizes().
    QList<int> widgetSizes;

    switch (layout)
    {
    case 1:
        // Single view: Hide all sub-splitters and display one label.
        LOG4CPLUS_DEBUG(mlog, "switching to single view" << flush);
        mainSplitter->insertWidget(0, sceneViewGLWidgets[0]); // move to "top" position
        showSceneViews.append(0);
        break;

    case 2:
        // Dual view: Hide all sub-splitters and show two labels side by side.
        LOG4CPLUS_DEBUG(mlog, "switching to dual view, horizontal" << flush);
        mainSplitter->setOrientation(Qt::Horizontal);
        // Transfer ownership of the scene view widgets to the main splitter.
        // If the splitter already owns the widgets, nothing will be changed.
        // Otherwise the scene views will be removed from the splitter objects
        // that own the labels.
        mainSplitter->insertWidget(0, sceneViewGLWidgets[0]);
        mainSplitter->insertWidget(1, sceneViewGLWidgets[1]);
        showSceneViews.append(0);
        showSceneViews.append(1);
        // Distribute the available space evenly among the two labels.
        widgetSizes.append(w/2);
        widgetSizes.append(w/2);
        mainSplitter->setSizes(widgetSizes);
        break;

    case 3:
        // Dual view: Hide all sub-splitters and show two labels side by side.
        LOG4CPLUS_DEBUG(mlog, "switching to dual view, vertical" << flush);
        mainSplitter->setOrientation(Qt::Vertical);
        // Transfer ownership of the scene view widgets to the main splitter.
        // If the splitter already owns the widgets, nothing will be changed.
        // Otherwise the scene views will be removed from the splitter objects
        // that own the labels.
        mainSplitter->insertWidget(0, sceneViewGLWidgets[0]);
        mainSplitter->insertWidget(1, sceneViewGLWidgets[1]);
        showSceneViews.append(0);
        showSceneViews.append(1);
        // Distribute the available space evenly among the two labels.
        widgetSizes.append(h/2);
        widgetSizes.append(h/2);
        mainSplitter->setSizes(widgetSizes);
        break;

    case 4:
        // One large and two small views.
        LOG4CPLUS_DEBUG(mlog, "switching to one large, two small view");
        rightSplitter->setOrientation(Qt::Vertical);
        rightSplitter->insertWidget(0, sceneViewGLWidgets[1]);
        rightSplitter->insertWidget(1, sceneViewGLWidgets[2]);
        mainSplitter->setOrientation(Qt::Horizontal);
        mainSplitter->insertWidget(0, sceneViewGLWidgets[0]);
        mainSplitter->insertWidget(1, rightSplitter);
        showSceneViews.append(0);
        showSceneViews.append(1);
        showSceneViews.append(2);
        widgetSizes.append(h/2);
        widgetSizes.append(h/2);
        rightSplitter->setSizes(widgetSizes);
        widgetSizes.clear();
        widgetSizes.append(2*w/3);
        widgetSizes.append(w/3);
        mainSplitter->setSizes(widgetSizes);
        rightSplitter->show();
        break;

    case 5:
        // One large and three small views.
        LOG4CPLUS_DEBUG(mlog, "switching to one large, three small view");
        rightSplitter->insertWidget(0, sceneViewGLWidgets[1]);
        rightSplitter->insertWidget(1, sceneViewGLWidgets[2]);
        rightSplitter->insertWidget(2, sceneViewGLWidgets[3]);
        mainSplitter->setOrientation(Qt::Horizontal);
        mainSplitter->insertWidget(0, sceneViewGLWidgets[0]);
        mainSplitter->insertWidget(1, rightSplitter);
        rightSplitter->show();
        showSceneViews.append(0);
        showSceneViews.append(1);
        showSceneViews.append(2);
        showSceneViews.append(3);
        widgetSizes.append(h/3);
        widgetSizes.append(h/3);
        widgetSizes.append(h/3);
        rightSplitter->setSizes(widgetSizes);
        widgetSizes.clear();
        widgetSizes.append(2*w/3);
        widgetSizes.append(w/3);
        mainSplitter->setSizes(widgetSizes);
        break;

    case 6:
        // Four equally sized views.
        LOG4CPLUS_DEBUG(mlog, "switching to quad view");
        topSplitter->insertWidget(0, sceneViewGLWidgets[0]);
        topSplitter->insertWidget(1, sceneViewGLWidgets[1]);
        bottomSplitter->insertWidget(0, sceneViewGLWidgets[2]);
        bottomSplitter->insertWidget(1, sceneViewGLWidgets[3]);
        mainSplitter->setOrientation(Qt::Vertical);
        mainSplitter->insertWidget(0, topSplitter);
        mainSplitter->insertWidget(1, bottomSplitter);
        topSplitter->show();
        bottomSplitter->show();
        showSceneViews.append(0);
        showSceneViews.append(1);
        showSceneViews.append(2);
        showSceneViews.append(3);
        widgetSizes.append(w/2);
        widgetSizes.append(w/2);
        topSplitter->setSizes(widgetSizes);
        bottomSplitter->setSizes(widgetSizes);
        widgetSizes.clear();
        widgetSizes.append(h/2);
        widgetSizes.append(h/2);
        mainSplitter->setSizes(widgetSizes);
        break;
    }

    // Show the labels that are visible in the new layout.
    for (int i = 0; i < showSceneViews.size(); i++)
    {
        sceneViewGLWidgets[showSceneViews[i]]->show();
    }
}


void MMainWindow::sceneManagement()
{
    if ( sceneManagementDialog->exec() == QDialog::Accepted )
    {
        LOG4CPLUS_DEBUG(mlog, "updating scene management" << flush);
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


} // namespace Met3D
