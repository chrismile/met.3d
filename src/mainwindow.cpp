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
#include "system/pipelineconfiguration.h"

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
    systemDock->setFeatures(QDockWidget::DockWidgetMovable
                            | QDockWidget::DockWidgetFloatable);
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

    // Init application system resources.
    // =========================================================================

    MApplicationConfigurationManager appConfig;
    appConfig.loadConfiguration();

    // Set window icon.
    QString iconPath = systemManagerAndControl->getMet3DHomeDir(
                ).absoluteFilePath("config/met3d_icon.png");
    setWindowIcon(QIcon(iconPath));


    // Initial assignment of scenes to scene views.
    //==========================================================================

    int numScenes = glResourcesManager->getScenes().size();
    for (int i = 0; i < MET3D_MAX_SCENEVIEWS; i++)
    {
        sceneViewGLWidgets[i]->setScene(
                    glResourcesManager->getScenes()[min(i, numScenes-1)]);
    }


    // Show the control widget of the first scene and add the system control
    // dock below.
    addDockWidget(Qt::LeftDockWidgetArea, systemDock);

    // Uncomment this loop if the system control should be tabified with the
    // scene controls.
    foreach (QDockWidget *dockWidget, dockWidgets)
        if (!dockWidget->isFloating()) tabifyDockWidget(dockWidget, systemDock);

    dockWidgets[0]->raise();

    // Connect signals and slots.
    // ========================================================================

    connect(ui->actionFullScreen, SIGNAL(toggled(bool)),
            this, SLOT(setFullScreen(bool)));
    connect(ui->actionWaypoints, SIGNAL(toggled(bool)),
            this, SLOT(showWaypointsTable(bool)));
    connect(ui->actionSceneManagement, SIGNAL(triggered()),
            this, SLOT(sceneManagement()));
    connect(ui->actionAddDataset, SIGNAL(triggered()),
            this, SLOT(addDataset()));

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

    connect(ui->actionOnlineManual, SIGNAL(triggered()),
            this, SLOT(openOnlineManual()));
    connect(ui->actionReportABug, SIGNAL(triggered()),
            this, SLOT(openOnlineIssueTracker()));
    connect(ui->actionAboutQt, SIGNAL(triggered()),
            this, SLOT(showAboutQtDialog()));
    connect(ui->actionAboutMet3D, SIGNAL(triggered()),
            this, SLOT(showAboutDialog()));
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
    // Remove "x" corner button so the user cannot close the dock widget.
    syncDock->setFeatures(QDockWidget::DockWidgetMovable
                          | QDockWidget::DockWidgetFloatable);
    syncDock->setWidget(syncControl);
    addDockWidget(Qt::LeftDockWidgetArea, syncDock);
}


void MMainWindow::dockSceneControl(MSceneControl *control)
{
    QDockWidget* dock = new QDockWidget(control->getName(), this);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    // Remove "x" corner button so the user cannot close the dock widget.
    dock->setFeatures(QDockWidget::DockWidgetMovable
                      | QDockWidget::DockWidgetFloatable);
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
    // Remove "x" corner button so the user can only hide the dock widget
    // via the corresponding menu button.
    waypointsTableDock->setFeatures(QDockWidget::DockWidgetMovable
                                    | QDockWidget::DockWidgetFloatable);
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


void MMainWindow::addDataset()
{
    MAddDatasetDialog addDatasetDialog;

    if ( addDatasetDialog.exec() == QDialog::Accepted )
    {
        MNWPPipelineConfigurationInfo pipelineConfig =
                addDatasetDialog.getNWPPipelineConfigurationInfo();

        LOG4CPLUS_DEBUG(mlog, "adding new dataset: "
                        << pipelineConfig.name.toStdString());

        MPipelineConfiguration newPipelineConfig;
        newPipelineConfig.initializeNWPPipeline(
                pipelineConfig.name,
                pipelineConfig.fileDir,
                pipelineConfig.fileFilter,
                pipelineConfig.schedulerID,
                pipelineConfig.memoryManagerID,
                (MPipelineConfiguration::MNWPReaderFileFormat)pipelineConfig.dataFormat,
                pipelineConfig.enableRegridding,
                pipelineConfig.enableProbabiltyRegionFilter);
    }
}


void MMainWindow::openOnlineManual()
{
    QDesktopServices::openUrl(QUrl("https://met3d.readthedocs.org"));
}


void MMainWindow::openOnlineIssueTracker()
{
    QDesktopServices::openUrl(QUrl("https://gitlab.com/wxmetvis/met.3d/issues"));
}


void MMainWindow::showAboutQtDialog()
{
    QMessageBox::aboutQt(this);
}


void MMainWindow::showAboutDialog()
{
    QString aboutString = QString(
            "<b>About Met.3D</b><br><br>"
            "This is Met.3D version %1, %2.<br><br>"
            "Met.3D is an open-source software for interactive visualization "
            "of 3D spatial fields from meteorological numerical simulations "
            "and observations. "
            "In particular, Met.3D features functionality for visualization of "
            "ensemble numerical weather prediction data. Please refer to the "
            "<a href='https://met3d.readthedocs.io/en/latest/about.html'>online "
            "manual</a> for further details.<br><br>"
            "Met.3D is free software under the GNU General Public License.<br>"
            "It is distributed in the hope that it will be useful, but WITHOUT "
            "ANY WARRANTY; without even the implied warranty of MERCHANTABILITY "
            "or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public "
            "License for more details.<br><br>"
            "Copyright 2015-2016 Met.3D authors:<br>"
            "Marc Rautenhaus(1), Michael Kern(1), Christoph Heidelmann(1), "
            "Bianca Tost(1), Alexander Kumpf(1), Fabian Sch&ouml;ttl(1).<br><br>"
            "(1) <a href='https://wwwcg.in.tum.de/'>Computer Graphics and "
            "Visualization Group</a>, "
            "Technical University of Munich, Garching, Germany<br><br>"
            "See Met.3D source files for license details.<br>"
                ).arg(met3dVersionString).arg(met3dBuildDate);

    QMessageBox::about(this, "About Met.3D", aboutString);
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


} // namespace Met3D
