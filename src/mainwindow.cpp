/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2016-2018 Bianca Tost
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
#include <float.h>

// related third party imports
#include <QDockWidget>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QCloseEvent>

#include <log4cplus/loggingmacros.h>
#include <log4cplus/version.h>
#include <eccodes.h>
#include <gsl/gsl_version.h>
#include <netcdf_meta.h>

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
      sceneManagementDialog(new MSceneManagementDialog),
      resizeWindowDialog(new MResizeWindowDialog),
      sessionManagerDialog(new MSessionManagerDialog),
      sceneViewLayout(1),
      sessionSettings(nullptr),
      systemDock(nullptr),
      sessionAutoSaveTimer(nullptr)
{
    // Qt Designer specific initialization.
    ui->setupUi(this);

    // Create the application window title.
    QString applicationTitle = QString("Met.3D version %1 (%2)")
            .arg(met3dVersionString).arg(met3dBuildDate);

    setWindowTitle(applicationTitle);

    LOG4CPLUS_DEBUG(mlog, "Initialising Met.3D system ... please wait.");

    // Timer used to handle automatic saving of current session.
    sessionAutoSaveTimer = new QTimer();
    sessionAutoSaveTimer->setInterval(30000);

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
    systemDock = new QDockWidget("System", this);
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

    // Initialise bounding box dock widget.
    //==========================================================================
    boundingBoxDock = new MBoundingBoxDockWidget();
    boundingBoxDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    // Remove "x" corner button so the user can only hide the dock widget
    // via the corresponding menu button.
    boundingBoxDock->setFeatures(QDockWidget::DockWidgetMovable
                                 | QDockWidget::DockWidgetFloatable);
    boundingBoxDock->setVisible(false);
    addDockWidget(Qt::BottomDockWidgetArea, boundingBoxDock);

    // Initialise application system resources.
    // =========================================================================

    MApplicationConfigurationManager appConfig;
    appConfig.loadConfiguration();

    // Set window icon.
    QString iconPath = systemManagerAndControl->getMet3DHomeDir(
                ).absoluteFilePath("config/met3d_icon.png");
    setWindowIcon(QIcon(iconPath));

    if (sessionManagerDialog->getLoadSessionOnStart())
    {
        sessionManagerDialog->loadSessionOnStart();
    }
    else
    {
    // Initial assignment of scenes to scene views.
    //==========================================================================

        int numScenes = glResourcesManager->getScenes().size();
        for (int i = 0; i < MET3D_MAX_SCENEVIEWS; i++)
        {
            sceneViewGLWidgets[i]->setScene(
                        glResourcesManager->getScenes()[min(i, numScenes - 1)]);
        }

    }


    // Show the control widget of the first scene and add the system control
    // dock below.
    systemDock->setObjectName("system");
    addDockWidget(Qt::LeftDockWidgetArea, systemDock);

    // Uncomment this loop if the system control should be tabified with the
    // scene controls.
    tabifyScenesAndSystem();

    // Connect signals and slots.
    // ========================================================================

    connect(ui->actionFullScreen, SIGNAL(toggled(bool)),
            this, SLOT(setFullScreen(bool)));
    connect(ui->actionWaypoints, SIGNAL(toggled(bool)),
            this, SLOT(showWaypointsTable(bool)));
    connect(ui->actionBoundingBoxes, SIGNAL(toggled(bool)),
            this, SLOT(showBoundingBoxTable(bool)));
    connect(ui->actionSceneManagement, SIGNAL(triggered()),
            this, SLOT(sceneManagement()));
    connect(ui->actionAddDataset, SIGNAL(triggered()),
            this, SLOT(addDataset()));
    connect(ui->actionResizeWindow, SIGNAL(triggered()),
            this, SLOT(resizeWindow()));

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

    connect(ui->actionSessionManager, SIGNAL(triggered()),
            this, SLOT(openSessionManager()));
    connect(ui->actionSaveSession, SIGNAL(triggered()),
            sessionManagerDialog, SLOT(saveSession()));
    connect(ui->menuSessions, SIGNAL(triggered(QAction*)),
            this, SLOT(switchSession(QAction*)));
    connect(ui->menuRevertCurrentSession, SIGNAL(triggered(QAction*)),
            this, SLOT(revertCurrentSession(QAction*)));
    connect(sessionAutoSaveTimer, SIGNAL(timeout()),
            sessionManagerDialog, SLOT(autoSaveSession()));
}


MMainWindow::~MMainWindow()
{
    LOG4CPLUS_DEBUG(mlog, "Freeing application resources.." << flush);

    delete sceneManagementDialog;
    delete resizeWindowDialog;

    for (int i = 0; i < sceneViewGLWidgets.size(); i++)
        delete sceneViewGLWidgets[i];

    // Deleting mainSplitter implicitly deletes the other splitters, as they
    // are children of mainSplitter.
    delete mainSplitter;

    delete glResourcesManager;
    delete systemManagerAndControl;

    for (int i = 0; i < sceneDockWidgets.size(); i++)
    {
        delete sceneDockWidgets[i]; // aren't these deleted via the parent mechanism?
    }

    delete systemDock;

    delete sessionAutoSaveTimer;

    delete ui;
    LOG4CPLUS_DEBUG(mlog, "..application resources have been deleted.");
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MMainWindow::dockSyncControl(MSyncControl *syncControl)
{
    // Create a synchronisation control as dock widget.
    QDockWidget *syncDock = new QDockWidget(syncControl->getID(), this);
    syncDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    // Remove "x" corner button so the user cannot close the dock widget.
    syncDock->setFeatures(QDockWidget::DockWidgetMovable
                          | QDockWidget::DockWidgetFloatable);
    syncDock->setWidget(syncControl);
    syncDock->setObjectName("sync_" + syncControl->getID());
    addDockWidget(Qt::LeftDockWidgetArea, syncDock);
    syncControlDockWidgets.append(syncDock);
}


void MMainWindow::dockSceneControl(MSceneControl *control)
{
    QDockWidget *dock = new QDockWidget(control->getName(), this);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    // Remove "x" corner button so the user cannot close the dock widget.
    dock->setFeatures(QDockWidget::DockWidgetMovable
                      | QDockWidget::DockWidgetFloatable);
    dock->setWidget(control);
    dock->setObjectName("scene_" + control->getName());
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    sceneDockWidgets.append(dock);

    for (int i = 0; i < sceneDockWidgets.size() - 1; i++)
    {
        if (!sceneDockWidgets[i]->isFloating())
        {
            tabifyDockWidget(sceneDockWidgets[i], dock);
        }
    }
}


void MMainWindow::changeDockedSceneName(const QString& oldName,
                                        const QString& newName)
{
    foreach (QDockWidget *dockWidget, sceneDockWidgets)
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
    waypointsTableDock->setObjectName("waypoints");
    addDockWidget(Qt::BottomDockWidgetArea, waypointsTableDock);
}


void MMainWindow::removeSceneControl(QWidget *widget)
{
    for (int i = 0; i < sceneDockWidgets.size(); i++)
    {
        if (sceneDockWidgets[i]->widget() == widget)
        {
            QDockWidget *dock = sceneDockWidgets.takeAt(i);
            removeDockWidget(dock);
            MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
            QString name = static_cast<MSceneControl*>(widget)->getName();
            glRM->deleteScene(name);
            delete dock;
        }
    }
}


void MMainWindow::removeSyncControl(MSyncControl *syncControl)
{
    for (int i = 0; i < syncControlDockWidgets.size(); i++)
    {
        if (syncControlDockWidgets[i]->widget() == syncControl)
        {
            QDockWidget *dock = syncControlDockWidgets.takeAt(i);
            removeDockWidget(dock);
            // Remove the sync control from the list of registered sync controls
            // and delete it.
            systemManagerAndControl->removeSyncControl(syncControl);
            delete dock;
        }
    }
}


QVector<MSceneViewGLWidget*>& MMainWindow::getGLWidgets()
{
    return sceneViewGLWidgets;
}


void MMainWindow::resizeSceneView(int newWidth, int newHeight,
                                  MSceneViewGLWidget *sceneView)
{
    unsigned int sceneViewID = sceneView->getID();

    // Exit full screen mode to be able to change size of window.
    // (Especially necessary if one wants to resize view in single view mode.)
    setFullScreen(false);

    // Store old sizes of the widgets which act as placeholders and will be
    // replaced by the new sizes.
    QList<int> mainSplitterSizes = mainSplitter->sizes();
    QList<int> rightSplitterSizes = rightSplitter->sizes();
    QList<int> topSplitterSizes = topSplitter->sizes();
    QList<int> bottomSplitterSizes = bottomSplitter->sizes();

    // Get size of screen containing the largest part of the sceneView to resize.
    QDesktopWidget widget;
    QRect screenSize = widget.screenGeometry(widget.screenNumber(sceneView));

    // Get size of window frame since resize affects the window without frame.
    // (Maximum value to apply: screenSize - (frameSize - windowSize))
    int frameWidth = frameGeometry().width() - width();
    int frameHeight = frameGeometry().height() - height();
    // Adjust size to fit new scene size, but avoid resizing larger than the
    // current screen (minus window frame). (Resizing to larger size than the
    // current screen results in a following resize event, adjusting the window
    // to the screen).
    int newWindowWidth = width() + (newWidth - sceneView->width());
    newWindowWidth = min(screenSize.width() - frameWidth, newWindowWidth);
    int newWindowHeight = height() + (newHeight - sceneView->height());
    newWindowHeight = min(screenSize.height() - frameHeight, newWindowHeight);

    // Resize window.
    this->resize(newWindowWidth, newWindowHeight);

    // Get main splitter widht and height.
    int w = mainSplitter->width();
    int h = mainSplitter->height();

    // Assign new width and/or height to scene view according to the layout used
    // and adjust sizes of the remaining scene views to fill remaining space.
    switch (sceneViewLayout)
    {
    case 1:
        // Single view: Nothing to do here since size is adjusted by resizing
        // the main window.
        break;

    case 2:
    {
        // Dual view: Horizontal order.
        mainSplitterSizes.replace(sceneViewID, newWidth);
        newWidth = max(w - 1 - newWidth, 0);
        mainSplitterSizes.replace((sceneViewID + 1) % 2, newWidth);
        mainSplitter->setSizes(mainSplitterSizes);
        break;
    }

    case 3:
    {
        // Dual view: Vertical order.
        mainSplitterSizes.replace(sceneViewID, newHeight);
        newHeight = max(h - 1 - newHeight, 0);
        mainSplitterSizes.replace((sceneViewID + 1) % 2, newHeight);
        mainSplitter->setSizes(mainSplitterSizes);
        break;
    }

    case 4:
    {
        // One large view(view0) and two small views(view1 and view2).
        // view1 and view2 positioned on the right and ordered vertically.
        if (sceneViewID > 0)
        {
            // ID = 1 -> upper widget; ID = 2 -> lower Widget.
            rightSplitterSizes.replace((sceneViewID + 1) % 2, newHeight);
            newHeight = max(h - 1 - newHeight, 0);
            rightSplitterSizes.replace(sceneViewID % 2, newHeight);
            rightSplitter->setSizes(rightSplitterSizes);
        }

        // Since view1 and view2 share the same width and positioned on the
        // righthand side, map their ids both to 1.
        unsigned int id = min(sceneViewID, uint(1));
        mainSplitterSizes.replace(id, newWidth);
        newWidth = max(w - 1 - newWidth, 0);
        mainSplitterSizes.replace((id + 1) % 2, newWidth);
        mainSplitter->setSizes(mainSplitterSizes);
        break;
    }

    case 5:
    {
        // One large view(view0) and three small views(view1, view2, view3).
        // view1, view2 and view3 positioned on the right and ordered vertically.
        if (sceneViewID > 0)
        {
            unsigned int id = sceneViewID - 1;
            // ID = 1 -> upper widget; ID = 2 -> middle widget;
            // ID = 2 -> lower widget.
            rightSplitterSizes.replace(id, newHeight);
            newHeight = max(h - 2 - newHeight, 0);
            rightSplitterSizes.replace((id + 1) % 3, floor(newHeight / 2.0));
            rightSplitterSizes.replace((id + 2) % 3, ceil(newHeight / 2.0));
            rightSplitter->setSizes(rightSplitterSizes);
        }

        // Since view1, view2 and view3 share the same width and positioned on
        // the righthand side, map their ids all to 1.
        unsigned int id = min(sceneViewID, uint(1));
        mainSplitterSizes.replace(id, newWidth);
        newWidth = max(w - 1 - newWidth, 0);
        mainSplitterSizes.replace((id + 1) % 2, newWidth);
        mainSplitter->setSizes(mainSplitterSizes);
        break;
    }

    case 6:
    {
        // Four views in 2x2 grid.
        // Top row with view 0 and view 1 ordered horizontally.
        if (sceneViewID <= 1)
        {
            unsigned int id = sceneViewID;
            // ID = 0 -> left widget; ID = 1 -> right Widget.
            topSplitterSizes.replace(id, newWidth);
            newWidth = max(w - 1 - newWidth, 0);
            topSplitterSizes.replace((id + 1) % 2, newWidth);
            topSplitter->setSizes(topSplitterSizes);
        }
        // Bottom row with view 2 and view 3 ordered horizontally.
        if (sceneViewID >= 2)
        {
            unsigned int id = sceneViewID - 2;
            // ID = 2 -> left widget; ID = 3 -> right Widget.
            bottomSplitterSizes.replace(id, newWidth);
            newWidth = max(w - 1 - newWidth, 0);
            bottomSplitterSizes.replace((id + 1) % 2, newWidth);
            bottomSplitter->setSizes(bottomSplitterSizes);
        }

        // Vertically view0 and view1 respectivly view2 and view3 share the same
        // height thus map these pairings to the same index.
        unsigned int id = uint(sceneViewID / 2);
        mainSplitterSizes.replace(id, newHeight);
        newHeight = max(h - 1 - newHeight, 0);
        mainSplitterSizes.replace((id + 1) % 2, newHeight);
        mainSplitter->setSizes(mainSplitterSizes);
        break;
    }

    default:
        break;
    }
}


void MMainWindow::saveConfigurationToFile(QString filename)
{
    if (filename.isEmpty())
    {
        QString directory =
                MSystemManagerAndControl::getInstance()
                ->getMet3DWorkingDirectory().absoluteFilePath("config/winlayout");
        QDir().mkpath(directory);
        filename = QFileDialog::getSaveFileName(
                    MGLResourcesManager::getInstance(),
                    "Save window layout configuration",
                    MSystemManagerAndControl::getInstance()
                    ->getMet3DWorkingDirectory().absoluteFilePath("default.winlayout.conf"),
                    "Window layout configuration files (*.winlayout.conf)");

        if (filename.isEmpty())
        {
            return;
        }
    }

    LOG4CPLUS_DEBUG(mlog, "Saving configuration to " << filename.toStdString());

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);

    // Overwrite if the file exists.
    if (QFile::exists(filename))
    {
        QStringList groups = settings->childGroups();
        // Only overwrite file if it contains already configuration for a
        // window layout.
        if ( !groups.contains("MWindowLayout") )
        {
            QMessageBox msg;
            msg.setWindowTitle("Error");
            msg.setText("The selected file contains a configuration other than "
                        "MWindowLayout.\n"
                        "This file will NOT be overwritten -- have you selected"
                        " the correct file?");
            msg.setIcon(QMessageBox::Warning);
            msg.exec();
            delete settings;
            return;
        }
        QFile::remove(filename);
    }

    settings->beginGroup("FileFormat");
    // Save version id of Met.3D.
    settings->setValue("met3dVersion", met3dVersionString);
    settings->endGroup();

    saveConfiguration(settings);

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been saved.");
}


void MMainWindow::loadConfigurationFromFile(QString filename)
{
    if (filename.isEmpty())
    {
        filename = QFileDialog::getOpenFileName(
                    MGLResourcesManager::getInstance(),
                    "Load window layout configuration",
                    MSystemManagerAndControl::getInstance()
                    ->getMet3DWorkingDirectory().absoluteFilePath("config/winlayout"),
                    "Window layout configuration files (*.winlayout.conf)");

        if (filename.isEmpty())
        {
            return;
        }
    }

    LOG4CPLUS_DEBUG(mlog, "Loading configuration from "
                    << filename.toStdString());

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);

    loadConfiguration(settings);

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been loaded.");
}


void MMainWindow::saveConfiguration(QSettings *settings)
{
    settings->beginGroup("MWindowLayout");
    settings->setValue("isFullScreen", isFullScreen());
    settings->setValue("isMaximized", isMaximized());
    // Save window size.
    settings->setValue("windowWidth", width());
    settings->setValue("windowHeight", height());
    // Save state of check box for showing waypoints dock widget.
    // (Visibility and placement of dock widget is handled by "state")
    settings->setValue("showWaypoints",
                      waypointsTableDock->isVisible());
    // Save state of check box for showing waypoints dock widget.
    // (Visibility and placement of dock widget is handled by "state")
    settings->setValue("showBoundingBoxes",
                      boundingBoxDock->isVisible());
    // Save scene views layout by saving the list of sizes for each splitter
    // as a string list since it is not possible to save a int list directly.
    settings->setValue("viewLayout", sceneViewLayout);
    QStringList sizeList;
    foreach (int size, mainSplitter->sizes())
    {
        sizeList << (QString("%1").arg(size));
    }
    settings->setValue("mainSplitterSizes", sizeList);
    sizeList.clear();

    foreach (int size, rightSplitter->sizes())
    {
        sizeList << (QString("%1").arg(size));
    }
    settings->setValue("rightSplitterSizes", sizeList);
    sizeList.clear();

    foreach (int size, topSplitter->sizes())
    {
        sizeList << (QString("%1").arg(size));
    }
    settings->setValue("topSplitterSizes", sizeList);
    sizeList.clear();

    foreach (int size, bottomSplitter->sizes())
    {
        sizeList << (QString("%1").arg(size));
    }
    settings->setValue("bottomSplitterSizes", sizeList);
    settings->setValue("state", saveState());
    settings->endGroup();
}


void MMainWindow::loadConfiguration(QSettings *settings)
{
    settings->beginGroup("MWindowLayout");
    Qt::WindowState loadedWindowState = Qt::WindowNoState;
    QPoint position = this->pos();
    // First reset window state to no state since otherwise swichting
    // from full screen to maximized does not work correctly.
    setWindowState(loadedWindowState);
    if (settings->value("isFullScreen", false).toBool())
    {
        loadedWindowState = Qt::WindowFullScreen;
    }
    else if (settings->value("isMaximized", false).toBool())
    {
        loadedWindowState = Qt::WindowMaximized;
    }
    setWindowState(loadedWindowState);
    // Load window size.
    resize(settings->value("windowWidth", 1288).toInt(),
           settings->value("windowHeight", 610).toInt());
    // Changing the view layout during start to other than single view results
    // in black lines, one cannot get rid of. Therefore if loading session at
    // the start, leave scene view layout out.
    if (MSystemManagerAndControl::getInstance()->applicationIsInitialized())
    {
        // Load scene views layout by extracting the size lists of the string
        // lists saved since it is not possible to load a int list directly.
        setSceneViewLayout(settings->value("viewLayout", 1).toInt());
    }
    else
    {
        sceneViewLayout = settings->value("viewLayout", 1).toInt();
    }
    QList<int> sizeList;
    foreach (QString size, settings->value("mainSplitterSizes",
                                          "833, 0, 0, 0").toStringList())
    {
        sizeList.append(size.toInt());
    }
    mainSplitter->setSizes(sizeList);
    sizeList.clear();
    foreach (QString size, settings->value("rightSplitterSizes",
                                          "833, 0, 0, 0").toStringList())
    {
        sizeList.append(size.toInt());
    }
    rightSplitter->setSizes(sizeList);
    sizeList.clear();
    foreach (QString size, settings->value("topSplitterSizes",
                                          "833, 0, 0, 0").toStringList())
    {
        sizeList.append(size.toInt());
    }
    topSplitter->setSizes(sizeList);
    sizeList.clear();
    foreach (QString size, settings->value("bottomSplitterSizes",
                                          "833, 0, 0, 0").toStringList())
    {
        sizeList.append(size.toInt());
    }
    bottomSplitter->setSizes(sizeList);

    QByteArray state = settings->value("state", QByteArray()).toByteArray();
    // Don't change state if not defined in session configuration.
    if (!state.isEmpty())
    {
        restoreState(state);
        // Restoring the state of the window also affects the position of the
        // window thus reset the window's position to the position it was before
        // loading the configuration.
        if (!(loadedWindowState == Qt::WindowFullScreen
              || loadedWindowState == Qt::WindowMaximized))
        {
            this->setGeometry(position.x(), position.y(),
                              this->width(), this->height());
        }
    }
    // Load state of check box for showing waypoints dock widget.
    // (Visibility and placement of dock widget is handled by "state")
    ui->actionWaypoints->setChecked(
                settings->value("showWaypoints", false).toBool());
    // Load state of check box for showing bounding boxes dock widget.
    // (Visibility and placement of dock widget is handled by "state")
    ui->actionBoundingBoxes->setChecked(
                settings->value("showBoundingBoxes", false).toBool());
}


void MMainWindow::tabifyScenesAndSystem()
{
    foreach (QDockWidget *dockWidget, sceneDockWidgets)
    {
        if (!dockWidget->isFloating())
        {
            tabifyDockWidget(dockWidget, systemDock);
        }
    }
    // Necessary since otherwise the first scene tab won't be on top but the
    // system tab instead would be when loading a session.
    QTimer *timer = new QTimer(this);
    timer->singleShot(0, sceneDockWidgets[0], SLOT(raise()));
    delete timer;
}


void MMainWindow::onSessionsListChanged(QStringList *sessionsList,
                                        QString currentSession)
{
    // Clear actions.
    foreach(QAction *action, ui->menuSessions->actions())
    {
        ui->menuSessions->removeAction(action);
        delete action;
    }
    // Add new actions.
    foreach(QString session, *sessionsList)
    {
        QAction *action = ui->menuSessions->addAction(session);
        action->setData(session);
        if (session == currentSession)
        {
            QFont actionFont = action->font();
            actionFont.setBold(true);
            actionFont.setItalic(true);
            action->setFont(actionFont);
        }
    }
}


void MMainWindow::onCurrentSessionHistoryChanged(QStringList *sessionHistory,
                                                 QString sessionName)
{
    // Clear actions.
    foreach(QAction *action, ui->menuRevertCurrentSession->actions())
    {
        ui->menuRevertCurrentSession->removeAction(action);
        delete action;
    }
    ui->menuRevertCurrentSession->addSeparator()->setText(sessionName);
    // Add new actions.
    foreach(QString sessionRevision, *sessionHistory)
    {
        QAction *action = ui->menuRevertCurrentSession->addAction(sessionRevision);
        action->setData(sessionRevision);

    }
}


void MMainWindow::onSessionSwitch(QString currentSession)
{
    // Change font of previous currentSession back to normal and emphasise
    // current session by italic and bold type.
    foreach(QAction *action, ui->menuSessions->actions())
    {
        QFont actionFont = action->font();
        QString session = action->data().toString();
        // Emphasise the entry which represents the current session.
        if (session == currentSession)
        {
            actionFont.setBold(true);
            actionFont.setItalic(true);
        }
        // Reset previous current session entry by reseting all entries which
        // do not represent the current session.
        else
        {
            actionFont.setBold(false);
            actionFont.setItalic(false);
        }
        action->setFont(actionFont);
    }

    // Remove old session name if present and append current session name to
    // window title.
    QString newWindowTitle = windowTitle().split(" -- ").first();
    if (!currentSession.isEmpty())
    {
        newWindowTitle.append(" -- " + currentSession);
    }
    setWindowTitle(newWindowTitle);
}


void MMainWindow::updateSessionTimerInterval(int interval)
{
    // Since auto save session interval is stored in seconds but QTimer interval
    // uses milliseconds it is necessary to multiply autoSaveSessionIntervall
    // by 1000.
    sessionAutoSaveTimer->setInterval(interval * 1000);
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MMainWindow::show()
{
    QWidget::show();
    // TODO (bt, 14Mar2017) Why need to set view layout after showing the window?
    // Need to set scene view layout after show since otherwise black lines are
    // rendered for layouts different from single view. Also need to load the
    // sizes of the views since setting the view layout resets these settings.
    if (sessionManagerDialog->getLoadSessionOnStart())
    {
        sessionManagerDialog->loadWindowLayout();
    }
    // Start timer only if auto save is active.
    if (sessionManagerDialog->getAutoSaveSession())
    {
        sessionAutoSaveTimer->start();
    }
}


void MMainWindow::closeEvent(QCloseEvent *event)
{
    if (sessionManagerDialog->saveSessionOnAppExit())
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}


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


void MMainWindow::showBoundingBoxTable(bool b)
{
    boundingBoxDock->setVisible(b);
}


void MMainWindow::setSceneViewLayout(int layout)
{
    sceneViewLayout = layout;

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
                    pipelineConfig.enableProbabiltyRegionFilter,
                    pipelineConfig.treatRotatedGridAsRegularGrid,
                    pipelineConfig.surfacePressureFieldType,
                    pipelineConfig.convertGeometricHeightToPressure_ICAOStandard,
                    pipelineConfig.auxiliary3DPressureField,
                    pipelineConfig.disableGridConsistencyCheck,
                    pipelineConfig.inputVarsForDerivedVars);
    }
}


void MMainWindow::openSessionManager()
{
    // Don't save session automatically while the user interacts with the
    // session manager.
    sessionAutoSaveTimer->stop();
    sessionManagerDialog->exec();
    // Start timer only if auto save is active.
    if (sessionManagerDialog->getAutoSaveSession())
    {
        sessionAutoSaveTimer->start();
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
            "Copyright 2015-2018 Met.3D authors:<br>"
            "Marc Rautenhaus(1), Bianca Tost(1), Michael Kern(1), Alexander "
            "Kumpf(1), Fabian Sch&ouml;ttl(1), Christoph Heidelmann(1).<br><br>"
            "(1) <a href='https://wwwcg.in.tum.de/'>Computer Graphics and "
            "Visualization Group</a>, "
            "Technical University of Munich, Garching, Germany<br><br>"
            "See Met.3D source files for license details.<br><hr>"
            "Versions of libraries used to compile Met.3D:<br>"
            "<table> "
            "<tr> "
            "<td> freetype: %3.%4.%5 </td> <td> GDAL: %6 </td>"
            " <td> GLEW: %7 </td>"
            "</tr>"
            "<tr> "
            "<td> GLFX: %8.%9.%10 </td> <td> GLU: %11 </td>"
            " <td> eccodes: %12 </td>"
            "</tr>"
            "<tr> "
            "<td> GSL: %13 </td> <td> LOG4CPLUS: %14 </td>"
            " <td> NetCDF: %15 </td>"
            "</tr>"
            "<tr> "
            "<td> NetCDF-4 C++: %16.%17.%18 </td>"
            " <td> QCustomPlot: %19.%20.%21 </td>"
            "</tr>"
            "</table>"
            "Note: If a version is listed as x.x.x, Met.3D wasn't able to find"
            " a version tag for this library."
                ).arg(met3dVersionString).arg(met3dBuildDate)
            .arg(FREETYPE_MAJOR).arg(FREETYPE_MINOR).arg(FREETYPE_PATCH)
            .arg(GDAL_RELEASE_NAME)
            .arg(QString(reinterpret_cast<const char *>(glewGetString(GLEW_VERSION))))
#if defined(GLFX_VERSION_MAJOR) && defined(GLFX_VERSION_MINOR) \
    && defined(GLFX_VERSION_PATCH)
            .arg(GLFX_VERSION_MAJOR).arg(GLFX_VERSION_MINOR)
            .arg(GLFX_VERSION_PATCH)
#else
            .arg("x").arg("x").arg("x")
#endif
            .arg(QString(reinterpret_cast<const char *>(gluGetString(GLU_VERSION))))
            .arg(ECCODES_VERSION_STR)
            .arg(GSL_VERSION)
            .arg(LOG4CPLUS_VERSION_STR)
            .arg(NC_VERSION)
#if defined(NetCDFCXX4_VERSION_MAJOR) && defined(NetCDFCXX4_VERSION_MINOR) \
    && defined(NetCDFCXX4_VERSION_PATCH)
            .arg(NetCDFCXX4_VERSION_MAJOR).arg(NetCDFCXX4_VERSION_MINOR)
            .arg(NetCDFCXX4_VERSION_PATCH)
#else
            .arg("x").arg("x").arg("x")
#endif
#if defined(QCPLOT_MAJOR_VERSION) && defined(QCPLOT_MINOR_VERSION) \
    && defined(QCPLOT_PATCH_VERSION)
            .arg(QCPLOT_MAJOR_VERSION).arg(QCPLOT_MINOR_VERSION)
            .arg(QCPLOT_PATCH_VERSION);
#else
    .arg("x").arg("x").arg("x");
#endif

    QMessageBox::about(this, "About Met.3D", aboutString);
}


void MMainWindow::resizeWindow()
{
    // Initialise input boxes and ratio with current window size
    resizeWindowDialog->setup(this->width(),this->height());

    if (resizeWindowDialog->exec() == QDialog::Rejected) return;

    int newWidth = resizeWindowDialog->getWidth();
    int newHeight = resizeWindowDialog->getHeight();
    // TODO (bt, 25Oct2016) At the moment only resize in one monitor possible
    this->resize(newWidth, newHeight);
}


void MMainWindow::switchSession(QAction *sessionAction)
{
    sessionManagerDialog->switchToSession(sessionAction->data().toString());
}


void MMainWindow::revertCurrentSession(QAction *sessionAction)
{
    QString sessionNumber = sessionAction->data().toString();
    sessionNumber = sessionNumber.split(":").first();
    sessionNumber.replace(QRegExp("\\D*"), "");
    sessionManagerDialog->revertCurrentSessionToRevision(sessionNumber);
}


void MMainWindow::keyPressEvent(QKeyEvent *key)
{
    switch (key->key())
    {
    default:
        return;
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


} // namespace Met3D
