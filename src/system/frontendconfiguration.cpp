/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015-2017 Bianca Tost
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
#include "frontendconfiguration.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "data/scheduler.h"
#include "data/lrumemorymanager.h"
#include "gxfw/msystemcontrol.h"
#include "mainwindow.h"
#include "gxfw/synccontrol.h"
#include "gxfw/mscenecontrol.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/textmanager.h"
#include "data/waypoints/waypointstablemodel.h"

#include "gxfw/nwpactorvariable.h"
#include "actors/basemapactor.h"
#include "actors/graticuleactor.h"
#include "actors/volumebboxactor.h"
#include "actors/transferfunction1d.h"
#include "actors/movablepoleactor.h"
#include "actors/trajectoryactor.h"
#include "actors/nwphorizontalsectionactor.h"
#include "actors/nwpverticalsectionactor.h"
#include "actors/nwpvolumeraycasteractor.h"
#include "actors/nwpsurfacetopographyactor.h"

#include "data/regioncontributionanalysis.h"


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MFrontendConfiguration::MFrontendConfiguration()
    : MAbstractApplicationConfiguration()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MFrontendConfiguration::configure()
{
    // If you develop new modules for Met.3D, it might be easier to use
    // a hard-coded frontend intialization during development.
//    initializeDevelopmentFrontend();
//    return;

    // Indicates whether Met.3D was called by Metview.
    bool metviewConnection = false;

    // Scan global application command line arguments for pipeline definitions.
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    foreach (QString arg, sysMC->getApplicationCommandLineArguments())
    {
        if (arg.startsWith("--frontend="))
        {
            QString filename = arg.remove("--frontend=");
			filename = expandEnvironmentVariables(filename);

            // Production builds should use the config-from-file mechanism.
            initializeFrontendFromConfigFile(filename);
            return;
        }
        if (arg.startsWith("--metview"))
        {
            metviewConnection = true;
        }
    }

    QString errMsg = "";
    // If Met.3D is called by Metview and no configuration files are given,
    // use default configuration files stored at
    // $MET3D_HOME/config/metview/default_frontend.cfg .
    if (metviewConnection)
    {
        QString filename = "$MET3D_HOME/config/metview/default_frontend.cfg";
        filename = expandEnvironmentVariables(filename);
        QFileInfo fileInfo(filename);
        if (fileInfo.isFile())
        {
            // Production builds should use the config-from-file mechanism.
            initializeFrontendFromConfigFile(filename);
            return;
        }
        errMsg = QString(
                    "ERROR: Default Metview frontend configuration file does"
                    " not exist. Location: ") + filename;
        LOG4CPLUS_ERROR(mlog, errMsg.toStdString());
    }
    else
    {
        errMsg = QString(
                    "ERROR: No frontend configuration file specified."
                    "Use the '--frontend=<file>' command line argument.");
        LOG4CPLUS_ERROR(mlog, errMsg.toStdString());
    }
    throw MInitialisationError(errMsg.toStdString(), __FILE__, __LINE__);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MFrontendConfiguration::initializeFrontendFromConfigFile(
        const QString &filename)
{
    LOG4CPLUS_INFO(mlog, "Loading frontend configuration from file "
                   << filename.toStdString() << "...");

    if ( !QFile::exists(filename) )
    {
        QString errMsg = QString(
                    "Cannot open file %1: file does not exist.").arg(filename);
        LOG4CPLUS_ERROR(mlog, errMsg.toStdString());
        throw MInitialisationError(errMsg.toStdString(), __FILE__, __LINE__);
    }

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    QSettings config(filename, QSettings::IniFormat);

    // Initialize text rendering.
    // ==========================
    config.beginGroup("Text");

    QString fontfile = expandEnvironmentVariables(config.value("fontfile")
                                                  .toString());
    if ( !QFile::exists(fontfile) )
    {
        QString msg = QString("ERROR: cannot find font file %1").arg(fontfile);
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }

    int fontsize = config.value("fontsize").toInt();
    glRM->getTextManager()->setFont(fontfile, fontsize);

    config.endGroup();


    // Initialize coastlines and country borderlines geometry.
    // =======================================================
    config.beginGroup("CoastCountryLines");

    QString coastfile = expandEnvironmentVariables(config.value("coastfile")
                                                   .toString());
    QString countryfile = expandEnvironmentVariables(config.value("countryfile").toString());
    sysMC->getNaturalEarthDataLoader()->setDataSources(coastfile, countryfile);

    config.endGroup();


    // Initialize synchronization control(s).
    // ======================================
    int size = config.beginReadArray("Synchronization");

    for (int i = 0; i < size; i++)
    {
        config.setArrayIndex(i);

        // Read settings from file.
        QString name = config.value("name").toString();
        QString dataSourceIDs = config.value("initialiseFromDatasource").toString();

        LOG4CPLUS_DEBUG(mlog, "initializing synchronization control #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  name = " << name.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  dataSources = " << dataSourceIDs.toStdString());

        // Check parameter validity.
        if ( name.isEmpty())
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        // Create new synchronization control.
        initializeSynchronization(
                    name, dataSourceIDs.split("/",QString::SkipEmptyParts));
    }

    config.endArray();


    // Configure scene navigation.
    // ===========================

    config.beginGroup("SceneNavigation");

    QString mouseButtonRotate = config.value("mouseButtonRotate").toString();
    QString mouseButtonPan = config.value("mouseButtonPan").toString();
    QString mouseButtonZoom = config.value("mouseButtonZoom").toString();
    bool reverseDefaultZoomDirection = config.value(
                "reverseDefaultZoomDirection").toBool();
    bool reverseDefaultPanDirection = config.value(
                "reverseDefaultPanDirection", false).toBool();

    if (mouseButtonRotate == "left")
        glRM->setGlobalMouseButtonRotate(Qt::LeftButton);
    else if (mouseButtonRotate == "middle")
        glRM->setGlobalMouseButtonRotate(Qt::MiddleButton);
    else if (mouseButtonRotate == "right")
        glRM->setGlobalMouseButtonRotate(Qt::RightButton);

    if (mouseButtonPan == "left")
        glRM->setGlobalMouseButtonPan(Qt::LeftButton);
    else if (mouseButtonPan == "middle")
        glRM->setGlobalMouseButtonPan(Qt::MiddleButton);
    else if (mouseButtonPan == "right")
        glRM->setGlobalMouseButtonPan(Qt::RightButton);

    if (mouseButtonZoom == "left")
        glRM->setGlobalMouseButtonZoom(Qt::LeftButton);
    else if (mouseButtonZoom == "middle")
        glRM->setGlobalMouseButtonZoom(Qt::MiddleButton);
    else if (mouseButtonZoom == "right")
        glRM->setGlobalMouseButtonZoom(Qt::RightButton);

    glRM->reverseDefaultZoomDirection(reverseDefaultZoomDirection);
    glRM->reverseDefaultPanDirection(reverseDefaultPanDirection);

    config.endGroup();


    // Configure scene views.
    // ======================

    size = config.beginReadArray("SceneViews");

    for (int i = 0; i < min(size, MET3D_MAX_SCENEVIEWS); i++)
    {
        config.setArrayIndex(i);

        // Read scene navigation mode.
        QString sceneNavigationMode = config.value("sceneNavigation").toString();

        // Default setting is MOVE_CAMERA.
        MSceneViewGLWidget::SceneNavigationMode navMode =
                (sceneNavigationMode == "ROTATE_SCENE") ?
                    MSceneViewGLWidget::ROTATE_SCENE :
                    MSceneViewGLWidget::MOVE_CAMERA;

        // Get pointer to MSceneViewGLWidget with index i.
        MSceneViewGLWidget* glWidget =
                sysMC->getMainWindow()->getGLWidgets().at(i);
        glWidget->setSceneNavigationMode(navMode);

        QStringList rotationCentreStrL =
                config.value("sceneRotationCentre",
                             QString("0./45./1050.")).toString().split("/");
        QVector3D rotationCentre = QVector3D(
                    rotationCentreStrL.at(0).toDouble(),
                    rotationCentreStrL.at(1).toDouble(),
                    rotationCentreStrL.at(2).toDouble());
        glWidget->setSceneRotationCentre(rotationCentre);

        LOG4CPLUS_DEBUG(mlog, "initializing view #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  navigation mode = "
                        << ((navMode == MSceneViewGLWidget::MOVE_CAMERA) ?
                            "MOVE_CAMERA" : "ROTATE_SCENE"));
        LOG4CPLUS_DEBUG(mlog, "  rotation centre = "
                        << "latitude: " << rotationCentre.x() << " deg, "
                        << "longitude: " << rotationCentre.y() << " deg, "
                        << "pressure: " << rotationCentre.z() << " hPa");
    }

    config.endArray();


    // Create scene controls.
    // ======================

    size = config.beginReadArray("Scenes");

    for (int i = 0; i < size; i++)
    {
        config.setArrayIndex(i);

        // Read settings from file.
        QString name = config.value("name").toString();

        LOG4CPLUS_DEBUG(mlog, "initializing scene #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  name = " << name.toStdString());

        // Check parameter validity.
        if ( name.isEmpty() )
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        // Create new scene.
        MSceneControl *scene = new MSceneControl(name);
        glRM->registerScene(scene);
        sysMC->getMainWindow()->dockSceneControl(scene);
    }

    config.endArray();

    sysMC->getMainWindow()->setSceneViewLayout(1);


    // Waypoints model.
    // ================

    size = config.beginReadArray("WaypointsModel");

    for (int i = 0; i < size; i++)
    {
        config.setArrayIndex(i);

        // Read settings from file.
        QString name = config.value("name").toString();
        QString datafile = expandEnvironmentVariables(config.value("datafile")
                                                      .toString());

        LOG4CPLUS_DEBUG(mlog, "initializing waypoints model #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  name = " << name.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  data file = " << datafile.toStdString());

        // Check parameter validity.
        if ( name.isEmpty()
             || datafile.isEmpty() )
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        // Create a waypoints table model and a corresponding table view.
        MWaypointsTableModel *waypointsModel = new MWaypointsTableModel(name);
        waypointsModel->loadFromFile(datafile);
        sysMC->registerWaypointsModel(waypointsModel);
        sysMC->getMainWindow()->dockWaypointsModel(waypointsModel);
    }

    config.endArray();


    // Add predefined actors to the scenes.
    // ====================================

    size = config.beginReadArray("PredefinedActors");

    for (int i = 0; i < size; i++)
    {
        config.setArrayIndex(i);

        // Read settings from file.
        QString type = config.value("type").toString();
        QString dataSourceID = config.value("dataSource").toString();
        QString datafile = expandEnvironmentVariables(config.value("datafile")
                                                      .toString());
        QString bboxStr = config.value("bbox").toString();
        QString scenesStr = config.value("scenes").toString();
        QString levelTypeStr = config.value("levelType").toString();
        QString nwpDataSourceID = config.value("NWPDataSource").toString();

        LOG4CPLUS_DEBUG(mlog, "initializing predefined actor(s) #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  type = " << type.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  dataSource = " << dataSourceID.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  datafile = " << datafile.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  bbox = " << bboxStr.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  scenes = " << scenesStr.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  levelType = " << levelTypeStr.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  NWPDataSource = " << nwpDataSourceID.toStdString());

        QStringList bboxValues = bboxStr.split("/");
        QRectF bbox;
        if (bboxValues.size() == 4)
            bbox = QRectF(bboxValues[0].toFloat(), bboxValues[1].toFloat(),
                    bboxValues[2].toFloat(), bboxValues[3].toFloat());

        QList<MSceneControl*> scenes;
        foreach (QString sceneName, scenesStr.split("/"))
        {
            MSceneControl* scene = glRM->getScene(sceneName);
            if (scene != nullptr) scenes << scene;
        }

        MVerticalLevelType levelType =
                MStructuredGrid::verticalLevelTypeFromConfigString(levelTypeStr);

        // Check parameter validity.
        if ( type.isEmpty()
             || scenes.empty() )
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        // Create new predefined actor(s).
        if (type == "Basemap")
            initializeDefaultActors_Basemap(datafile, bbox, scenes);
        else if (type == "VolumeBox")
            initializeDefaultActors_VolumeBox(bbox, scenes);
        else if (type == "HSec_MSLP")
            initializeDefaultActors_MSLP(dataSourceID, bbox, scenes);
        else if (type == "Surface")
            initializeDefaultActors_Surface(dataSourceID, bbox, scenes);
        else if (type == "VSec_PV")
            initializeDefaultActors_VSec_PV(dataSourceID, scenes);
        else if (type == "VSec_Clouds")
            initializeDefaultActors_VSec_Clouds(dataSourceID, scenes);
        else if (type == "HSec")
            initializeDefaultActors_HSec(dataSourceID, bbox, scenes);
        else if (type == "HSec_Difference")
            initializeDefaultActors_HSec_Difference(dataSourceID, bbox, scenes);
        else if (type == "PressurePoles")
            initializeDefaultActors_PressurePoles(scenes);
        else if (type == "WCB_Probability")
            initializeDefaultActors_VolumeProbability(dataSourceID, levelType,
                                                      nwpDataSourceID, bbox,
                                                      scenes);
        else if (type == "Volume")
            initializeDefaultActors_Volume(dataSourceID, bbox, scenes);
        else if (type == "Trajectories")
            initializeDefaultActors_Trajectories(dataSourceID, scenes);
    }

    config.endArray();


    // Add actors from config files to the scenes.
    // ===========================================

    size = config.beginReadArray("Actors");

    for (int i = 0; i < size; i++)
    {
        config.setArrayIndex(i);

        // Read settings from file.
        QString configfile = expandEnvironmentVariables(config.value("config")
                                                        .toString());
        QString scenesStr = config.value("scenes").toString();

        LOG4CPLUS_DEBUG(mlog, "initializing actor #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  config = " << configfile.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  scenes = " << scenesStr.toStdString());

        QList<MSceneControl*> scenes;
        foreach (QString sceneName, scenesStr.split("/"))
        {
            MSceneControl* scene = glRM->getScene(sceneName);
            if (scene != nullptr) scenes << scene;
        }

        // Check parameter validity.
        if ( configfile.isEmpty()
             || scenes.empty() )
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        if ( !QFile::exists(configfile) )
        {
            LOG4CPLUS_WARN(mlog, "cannot find actor configuration file; skipping.");
            continue;
        }

        // Find actor that can be created with the specified config file and
        // create a new instance.
        foreach (MAbstractActorFactory* factory, glRM->getActorFactories())
        {
            if (factory->acceptSettings(configfile))
            {
                LOG4CPLUS_DEBUG(mlog, "  config corresponds to actor of type "
                                << factory->getName().toStdString());

                MActor *actor = factory->create(configfile);
                if (!actor)
                {
                    break;
                }

                QString actorName = actor->getName();
                bool ok = true;
                // Check whether name already exists.
                while (actorName.isEmpty() || glRM->getActorByName(actorName))
                {
                    actorName = QInputDialog::getText(
                                nullptr, "Change actor name",
                                "The given actor name already exists, please "
                                "enter a new one:",
                                QLineEdit::Normal,
                                actorName, &ok);

                    if (!ok)
                    {
                        // The user has pressed the "Cancel" button.
                        delete actor;
                        break;
                    }

                    actor->setName(actorName);
                }
                // The user has pressed the "Cancel" button.
                if (!ok)
                {
                    break;
                }

                glRM->registerActor(actor);
                foreach (MSceneControl *scene, scenes)
                {
                    scene->addActor(actor);
                }

                break;
            }
        }
    }

    config.endArray();

    LOG4CPLUS_INFO(mlog, "Frontend has been configured.");
}


void MFrontendConfiguration::initializeSynchronization(
        QString syncName,
        QStringList initializeFromDataSources)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();

    MSyncControl* syncControl = new MSyncControl(syncName, sysMC->getMainWindow());
    sysMC->registerSyncControl(syncControl);
    sysMC->getMainWindow()->dockSyncControl(syncControl);

    syncControl->restrictToDataSourcesFromFrontend(initializeFromDataSources);
}


void MFrontendConfiguration::initializeDefaultActors_Basemap(
        const QString &mapfile,
        const QRectF &bbox,
        QList<MSceneControl *> scenes)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MBaseMapActor *mapActor = new MBaseMapActor();
    mapActor->setFilename(mapfile);
    mapActor->setEnabled(true);
    glRM->registerActor(mapActor);
    foreach (MSceneControl *scene, scenes) scene->addActor(mapActor);

    MGraticuleActor *graticuleActor = new MGraticuleActor();
    graticuleActor->setBBox(bbox);
    graticuleActor->setColour(QColor(128, 128, 128, 255));
    glRM->registerActor(graticuleActor);
    foreach (MSceneControl *scene, scenes) scene->addActor(graticuleActor);
}


void MFrontendConfiguration::initializeDefaultActors_VolumeBox(
        const QRectF &bbox,
        QList<MSceneControl *> scenes)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MVolumeBoundingBoxActor *volumeBoxActor = new MVolumeBoundingBoxActor();
    volumeBoxActor->setBBox(bbox);
    glRM->registerActor(volumeBoxActor);
    foreach (MSceneControl *scene, scenes) scene->addActor(volumeBoxActor);
}


void MFrontendConfiguration::initializeDefaultActors_MSLP(
        const QString& dataSourceID,
        const QRectF& bbox,
        QList<MSceneControl*> scenes)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MNWPHorizontalSectionActor *mslpActor =
            new MNWPHorizontalSectionActor();
    mslpActor->setName("HSec: MSLP");

    MNWP2DHorizontalActorVariable* var =
            static_cast<MNWP2DHorizontalActorVariable*>(
                mslpActor->createActorVariable2(
                    dataSourceID,
                    SURFACE_2D,
                    "Mean_sea_level_pressure_surface")
                );
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    var->addContourSet(true, 1.2, false, QColor(139, 102, 139, 255),
                          "[90000,105000,100]");
    var->addContourSet(true, 2., false, QColor(139, 102, 139, 255),
                          "[90000,105000,400]");
    // Remove first contour set which was inserted during creation.
    var->removeContourSet(0);
    mslpActor->addActorVariable(var, "Synchronization");

    mslpActor->setBBox(bbox);
    mslpActor->setSlicePosition(1045.);
    mslpActor->getGraticuleActor()->setColour(QColor(128, 128, 128, 255));
    mslpActor->setLabelsEnabled(false);
    mslpActor->setSurfaceShadowEnabled(false);
    glRM->registerActor(mslpActor);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(mslpActor);
}


void MFrontendConfiguration::initializeDefaultActors_Surface(
        const QString& dataSourceID,
        const QRectF& bbox,
        QList<MSceneControl*> scenes)
{
    Q_UNUSED(bbox);

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MTransferFunction1D *transferFunctionTemp = new MTransferFunction1D();
    transferFunctionTemp->setName("Temperature");
    transferFunctionTemp->selectHCLColourmap(
                MTransferFunction1D::SEQUENTIAL_MULTIPLE_HUE,
                0., 90., 80., 5., 29., 86., 0.2, 2., 1., 0.85, 0.01, false);
    transferFunctionTemp->setMinimumValue(250.);
    transferFunctionTemp->setMaximumValue(330.);
    transferFunctionTemp->setValueDecimals(0);
    transferFunctionTemp->setSteps(15);
    transferFunctionTemp->setNumTicks(16);
    transferFunctionTemp->setNumLabels(6);
    transferFunctionTemp->setPosition(QRectF(-0.85, 0.9, 0.05, 0.5));
    glRM->registerActor(transferFunctionTemp);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(transferFunctionTemp);

    MNWPSurfaceTopographyActor *sfcActor = new MNWPSurfaceTopographyActor();
    sfcActor->setName("Surface: Temperature");

    MNWPActorVariable* var =
            sfcActor->createActorVariable2(
                dataSourceID,
                SURFACE_2D,
                "Surface_pressure_surface");
    sfcActor->addActorVariable(var, "Synchronization");

    var = sfcActor->createActorVariable2(
                dataSourceID,
                SURFACE_2D,
                "2_metre_temperature_surface");
    var->setTransferFunction("Temperature");
    sfcActor->addActorVariable(var, "Synchronization");

    glRM->registerActor(sfcActor);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(sfcActor);
}


void MFrontendConfiguration::initializeDefaultActors_HSec(
        const QString& dataSourceID,
        const QRectF& bbox,
        QList<MSceneControl*> scenes)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // -------------------------------------------------------------------------
    // Geopotential and Wind, HSec
    // -------------------------------------------------------------------------
    MTransferFunction1D *transferFunctionWind =
            new MTransferFunction1D();
    transferFunctionWind->setName("Wind Speed (m/s)");
    transferFunctionWind->selectHCLColourmap(
                MTransferFunction1D::SEQUENTIAL_MULTIPLE_HUE,
                0., 90., 80., 5., 29., 86., 0.2, 2., 1., 0.85, 0.01, false);
    transferFunctionWind->setMinimumValue(10.);
    transferFunctionWind->setMaximumValue(85.);
    transferFunctionWind->setValueDecimals(0);
    transferFunctionWind->setSteps(15);
    transferFunctionWind->setNumTicks(16);
    transferFunctionWind->setNumLabels(6);
    transferFunctionWind->setPosition(QRectF(-0.85, 0.9, 0.05, 0.5));
    glRM->registerActor(transferFunctionWind);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(transferFunctionWind);

    MNWPHorizontalSectionActor *geopWindActor =
            new MNWPHorizontalSectionActor();
    geopWindActor->setBBox(bbox);
    geopWindActor->setSlicePosition(250.);
    geopWindActor->setName("HSec: Geopotential Height and Wind Speed");

    MNWP2DHorizontalActorVariable* var =
            static_cast<MNWP2DHorizontalActorVariable*>(
                geopWindActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "wind_speed")
                );
    var->setTransferFunction("Wind Speed (m/s)");
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::FilledContours);
    geopWindActor->addActorVariable(var, "Synchronization");

    var = static_cast<MNWP2DHorizontalActorVariable*>(
                geopWindActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "geopotential_height")
                );
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    var->addContourSet(true, 1.2, false, QColor(Qt::darkGreen),
                          "[0,26000,40]");
    var->addContourSet(true, 2., false, QColor(Qt::darkGreen),
                          "[0,26000,200]");
    // Remove first contour set which was inserted during creation.
    var->removeContourSet(0);
    geopWindActor->addActorVariable(var, "Synchronization");

    var = static_cast<MNWP2DHorizontalActorVariable*>(
                geopWindActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "eastward_wind")
                );
    var->setTransferFunction("Wind Speed (m/s)");
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::Disabled);
    geopWindActor->addActorVariable(var, "Synchronization");

    var = static_cast<MNWP2DHorizontalActorVariable*>(
                geopWindActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "northward_wind")
                );
    var->setTransferFunction("Wind Speed (m/s)");
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::Disabled);
    geopWindActor->addActorVariable(var, "Synchronization");

    geopWindActor->setEnabled(true);
    glRM->registerActor(geopWindActor);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(geopWindActor);
}


void MFrontendConfiguration::initializeDefaultActors_HSec_Difference(
        const QString& dataSourceID,
        const QRectF& bbox,
        QList<MSceneControl*> scenes)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MTransferFunction1D *transferFunctionDiff =
            new MTransferFunction1D();
    transferFunctionDiff->setName("Difference");
    transferFunctionDiff->selectHCLColourmap(
                MTransferFunction1D::SEQUENTIAL_MULTIPLE_HUE,
                0., 90., 80., 5., 29., 86., 0.2, 2., 1., 0.85, 0.01, false);
    transferFunctionDiff->setMinimumValue(0.);
    transferFunctionDiff->setMaximumValue(70.);
    transferFunctionDiff->setValueDecimals(0);
    transferFunctionDiff->setSteps(15);
    transferFunctionDiff->setNumTicks(16);
    transferFunctionDiff->setNumLabels(6);
    transferFunctionDiff->setPosition(QRectF(-0.85, 0.9, 0.05, 0.5));
    glRM->registerActor(transferFunctionDiff);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(transferFunctionDiff);

    MNWPHorizontalSectionActor *diffActor = new MNWPHorizontalSectionActor();
    diffActor->setBBox(bbox);
    diffActor->setSlicePosition(950.);
    diffActor->setName("HSec: Geopotential Height and Wind Speed Difference");

    MNWP2DHorizontalActorVariable* var =
            static_cast<MNWP2DHorizontalActorVariable*>(
                diffActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "wind_speed")
                );
    var->setTransferFunction("Difference");
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::FilledContours);
    diffActor->addActorVariable(var, "Synchronization");

    var = static_cast<MNWP2DHorizontalActorVariable*>(
                diffActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "wind_speed")
                );
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::Disabled);
    diffActor->addActorVariable(var, "Synchronization");

    var = static_cast<MNWP2DHorizontalActorVariable*>(
                diffActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "wind_speed")
                );
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    var->addContourSet(true, 1.2, false, QColor(Qt::blue),
                          "[0,100,10]");
    var->addContourSet(true, 2., false, QColor(Qt::blue),
                          "[0,100,20]");
    // Remove first contour set which was inserted during creation.
    var->removeContourSet(0);
    diffActor->addActorVariable(var, "Synchronization");

    var = static_cast<MNWP2DHorizontalActorVariable*>(
                diffActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "geopotential_height")
                );
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    var->addContourSet(true, 1.2, false, QColor(Qt::darkGreen),
                          "[0,26000,40]");
    var->addContourSet(true, 2., false, QColor(Qt::darkGreen),
                          "[0,26000,200]");
    // Remove first contour set which was inserted during creation.
    var->removeContourSet(0);
    diffActor->addActorVariable(var, "Synchronization");

    diffActor->setEnabled(true);
    glRM->registerActor(diffActor);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(diffActor);
}


void MFrontendConfiguration::initializeDefaultActors_VSec_PV(
        const QString& dataSourceID,
        QList<MSceneControl*> scenes)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MTransferFunction1D *transferFunctionPotVort =
            new MTransferFunction1D();
    transferFunctionPotVort->setName("Potential Vorticity");
    transferFunctionPotVort->selectPredefinedColourmap("pv_eth");
    transferFunctionPotVort->setMinimumValue(-2.);
    transferFunctionPotVort->setMaximumValue(8.);
    transferFunctionPotVort->setValueDecimals(1);
    transferFunctionPotVort->setNumTicks(11);
    transferFunctionPotVort->setPosition(QRectF(0.68, -0.45, 0.05, 0.5));
    glRM->registerActor(transferFunctionPotVort);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(transferFunctionPotVort);

    MNWPVerticalSectionActor *vsecActorPV =
            new MNWPVerticalSectionActor();
    vsecActorPV->setName("VSec: PV, PT, CIWC and CLWC");

    // Potential vorticity.
    MNWP2DVerticalActorVariable* var =
            static_cast<MNWP2DVerticalActorVariable*>(
                vsecActorPV->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "Potential_vorticity_hybrid")
                );
    var->setTransferFunction("Potential Vorticity");
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::FilledContours);
    vsecActorPV->addActorVariable(var, "Synchronization");

    // Potential Temperature.
    var = static_cast<MNWP2DVerticalActorVariable*>(
                vsecActorPV->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "Potential_temperature_hybrid")
                );
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    var->addContourSet(true, 1.2, false, QColor(Qt::black), "[270,450,10]");
    // Remove first contour set which was inserted during creation.
    var->removeContourSet(0);
    vsecActorPV->addActorVariable(var, "Synchronization");

    // CIWC.
    var = static_cast<MNWP2DVerticalActorVariable*>(
                vsecActorPV->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "Specific_cloud_ice_water_content_hybrid")
                );
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    var->addContourSet(true, 1.2, false, QColor(Qt::white),
                          "0.00001,0.00003,0.00005,0.00007,0.0001,0.0003,"
                          "0.0005,0.0007,0.001");
    // Remove first contour set which was inserted during creation.
    var->removeContourSet(0);
    vsecActorPV->addActorVariable(var, "Synchronization");

    // CLWC.
    var = static_cast<MNWP2DVerticalActorVariable*>(
                vsecActorPV->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "Specific_cloud_liquid_water_content_hybrid")
                );
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    var->addContourSet(true, 1.2, false, QColor(Qt::blue),
                          "0.00001,0.00003,0.00005,0.00007,0.0001,0.0003,"
                          "0.0005,0.0007,0.001");
    // Remove first contour set which was inserted during creation.
    var->removeContourSet(0);
    vsecActorPV->addActorVariable(var, "Synchronization");

    vsecActorPV->setWaypointsModel(sysMC->getWaypointsModel("Waypoints"));
    vsecActorPV->setEnabled(false);
    glRM->registerActor(vsecActorPV);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(vsecActorPV);
}


void MFrontendConfiguration::initializeDefaultActors_VSec_Clouds(
        const QString& dataSourceID,
        QList<MSceneControl*> scenes)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MTransferFunction1D *transferFunctionClds =
            new MTransferFunction1D();
    transferFunctionClds->setName("Cloud Cover");
    transferFunctionClds->selectPredefinedColourmap("mss_clouds");
    transferFunctionClds->setMinimumValue(0.);
    transferFunctionClds->setMaximumValue(1.);
    transferFunctionClds->setValueDecimals(1);
    transferFunctionClds->setPosition(QRectF(0.9, -0.45, 0.05, 0.5));
    glRM->registerActor(transferFunctionClds);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(transferFunctionClds);

    MNWPVerticalSectionActor *vsecActor =
            new MNWPVerticalSectionActor();
    vsecActor->setName("VSec: Cloud Cover and Temperature");

    // Cloud cover.
    MNWP2DVerticalActorVariable* var =
            static_cast<MNWP2DVerticalActorVariable*>(
                vsecActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "Fraction_of_cloud_cover_hybrid")
                );
    var->setTransferFunction("Cloud Cover");
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::FilledContours);
    vsecActor->addActorVariable(var, "Synchronization");

    // Temperature.
    var = static_cast<MNWP2DVerticalActorVariable*>(
                vsecActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "Equivalent_potential_temperature_hybrid")
//                    "Temperature_hybrid")
                );
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
//    var->addContourSet(true, 1.2, false, QColor(211, 75, 71), "[200,300,4]");
//    var->addContourSet(true, 2., false, QColor(211, 75, 71), "234");
    var->addContourSet(true, 1.2, false, QColor(211, 75, 71), "[200,500,4]");
    var->addContourSet(true, 2., false, QColor(211, 75, 71), "[308,320,2]");
    // Remove first contour set which was inserted during creation.
    var->removeContourSet(0);
    vsecActor->addActorVariable(var, "Synchronization");

    // Potential Temperature.
    var = static_cast<MNWP2DVerticalActorVariable*>(
                vsecActor->createActorVariable2(
                    dataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "Potential_temperature_hybrid")
                );
    var->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    var->addContourSet(true, 1.2, false, QColor(Qt::gray), "[270,400,10]");
    // Remove first contour set which was inserted during creation.
    var->removeContourSet(0);
    vsecActor->addActorVariable(var, "Synchronization");

    vsecActor->setWaypointsModel(sysMC->getWaypointsModel("Waypoints"));
    glRM->registerActor(vsecActor);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(vsecActor);
}


void MFrontendConfiguration::initializeDefaultActors_Volume(
        const QString& dataSourceID,
        const QRectF& bbox,
        QList<MSceneControl*> scenes)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MTransferFunction1D *transferFunctionPressure = new MTransferFunction1D();
    transferFunctionPressure->setName("Pressure");
    transferFunctionPressure->setValueDecimals(0);
    transferFunctionPressure->selectPredefinedColourmap("gist_rainbow");
    transferFunctionPressure->setMinimumValue(100000.);
    transferFunctionPressure->setMaximumValue(5000.);
    transferFunctionPressure->setNumTicks(11);
    transferFunctionPressure->setSteps(250);
    transferFunctionPressure->setPosition(QRectF(0.9, -0.45, 0.05, 0.5));
    transferFunctionPressure->setEnabled(false);
    glRM->registerActor(transferFunctionPressure);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(transferFunctionPressure);

    // Horizontal wind speed.
    MNWPVolumeRaycasterActor *nwpVolumeActor =
            new MNWPVolumeRaycasterActor();
    nwpVolumeActor->setName("Volume: NWP");

    MNWPActorVariable* var = nwpVolumeActor->createActorVariable2(
                dataSourceID,
                HYBRID_SIGMA_PRESSURE_3D,
                "Windspeed_hybrid");
    var->setTransferFunction("Pressure");
    nwpVolumeActor->addActorVariable(var, "Synchronization");

    var = nwpVolumeActor->createActorVariable2(
                dataSourceID,
                HYBRID_SIGMA_PRESSURE_3D,
                "Pressure");
    var->setTransferFunction("Pressure");
    nwpVolumeActor->addActorVariable(var, "Synchronization");

    nwpVolumeActor->setBoundingBox(bbox, 1050., 100.);
    nwpVolumeActor->setEnabled(true);
    glRM->registerActor(nwpVolumeActor);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(nwpVolumeActor);

    // Cloud cover.
//    MNWPVolumeRaycasterActor *nwpVolumeActor2 =
//            new MNWPVolumeRaycasterActor();
//    nwpVolumeActor2->setName("Volume: Cloud Cover");

//    var = nwpVolumeActor2->createActorVariable2(
//                dataSourceID,
//                HYBRID_SIGMA_PRESSURE_3D,
//                "Fraction_of_cloud_cover_hybrid");
//    var->setTransferFunction("Pressure");
//    nwpVolumeActor2->addActorVariable(var, "Synchronization");

//    var = nwpVolumeActor2->createActorVariable2(
//                dataSourceID,
//                HYBRID_SIGMA_PRESSURE_3D,
//                "Pressure");
//    var->setTransferFunction("Pressure");
//    nwpVolumeActor2->addActorVariable(var, "Synchronization");

//    nwpVolumeActor2->setBoundingBox(bbox, 1050., 100.);
//    nwpVolumeActor2->setEnabled(true);
//    glRM->registerActor(nwpVolumeActor2);
//    foreach (MSceneControl *scene, scenes)
//        scene->addActor(nwpVolumeActor2);
}


void MFrontendConfiguration::initializeDefaultActors_VolumeProbability(
        const QString& dataSourceID,
        const MVerticalLevelType levelType,
        const QString& nwpDataSourceID,
        const QRectF& bbox,
        QList<MSceneControl*> scenes)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MTransferFunction1D *transferFunctionProb = new MTransferFunction1D();
    transferFunctionProb->setName("Probability (%)");
    transferFunctionProb->selectPredefinedColourmap("hot_wind_r");
    transferFunctionProb->setMinimumValue(0.);
    transferFunctionProb->setMaximumValue(1.);
    transferFunctionProb->setValueDecimals(2);
    transferFunctionProb->setSteps(10);
    transferFunctionProb->setNumTicks(16);
    transferFunctionProb->setNumLabels(6);
    transferFunctionProb->setPosition(QRectF(-0.85, 0.9, 0.05, 0.5));
    glRM->registerActor(transferFunctionProb);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(transferFunctionProb);

    // Probability vertical section actor.
    // =====================================

    MNWPVerticalSectionActor *vsecActorWCB = new MNWPVerticalSectionActor();
    vsecActorWCB->setName("VSec: Probability of WCB occurrence");

    // Potential vorticity.
    MNWP2DVerticalActorVariable* vvar =
            static_cast<MNWP2DVerticalActorVariable*>(
                vsecActorWCB->createActorVariable2(
                    dataSourceID,
                    levelType,
                    "ProbabilityOfTrajectoryOccurence")
                );
    vvar->setTransferFunction("Probability (%)");
    vvar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::FilledContours);
    vsecActorWCB->addActorVariable(vvar, "Synchronization");

    // Equivalent potential Temperature.
    vvar = static_cast<MNWP2DVerticalActorVariable*>(
                vsecActorWCB->createActorVariable2(
                    nwpDataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "Equivalent_potential_temperature_hybrid")
                );
    vvar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    vvar->addContourSet(true, 1.2, false, QColor(Qt::black), "[200,500,4]");
    vvar->addContourSet(true, 2., false, QColor(Qt::black), "[308,320,2]");
    // Remove first contour set which was inserted during creation.
    vvar->removeContourSet(0);
    vsecActorWCB->addActorVariable(vvar, "Synchronization");

    // CIWC.
    vvar = static_cast<MNWP2DVerticalActorVariable*>(
                vsecActorWCB->createActorVariable2(
                    nwpDataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "Specific_cloud_ice_water_content_hybrid")
                );
    vvar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    vvar->addContourSet(true, 1.2, false, QColor(Qt::white),
                          "0.00001,0.00003,0.00005,0.00007,0.0001,0.0003,"
                          "0.0005,0.0007,0.001");
    // Remove first contour set which was inserted during creation.
    vvar->removeContourSet(0);
    vsecActorWCB->addActorVariable(vvar, "Synchronization");

    // CLWC.
    vvar = static_cast<MNWP2DVerticalActorVariable*>(
                vsecActorWCB->createActorVariable2(
                    nwpDataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "Specific_cloud_liquid_water_content_hybrid")
                );
    vvar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    vvar->addContourSet(true, 1.2, false, QColor(Qt::blue),
                          "0.00001,0.00003,0.00005,0.00007,0.0001,0.0003,"
                          "0.0005,0.0007,0.001");
    // Remove first contour set which was inserted during creation.
    vvar->removeContourSet(0);
    vsecActorWCB->addActorVariable(vvar, "Synchronization");

    vsecActorWCB->setWaypointsModel(sysMC->getWaypointsModel("Waypoints"));
    glRM->registerActor(vsecActorWCB);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(vsecActorWCB);

    // Probability horizontal section actor.
    // =====================================

    MNWPHorizontalSectionActor *pwcbHSecActor = new MNWPHorizontalSectionActor();
    pwcbHSecActor->setName("HSec: Probability of WCB occurrence");

    MNWP2DHorizontalActorVariable* hvar =
            static_cast<MNWP2DHorizontalActorVariable*>(
                pwcbHSecActor->createActorVariable2(
                    dataSourceID,
                    levelType,
                    "ProbabilityOfTrajectoryOccurence")
                );
    hvar->setTransferFunction("Probability (%)");
    hvar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::FilledContours);
    pwcbHSecActor->addActorVariable(hvar, "Synchronization");

    hvar = static_cast<MNWP2DHorizontalActorVariable*>(
                pwcbHSecActor->createActorVariable2(
                    dataSourceID  + " ProbReg",
                    levelType,
                    "ProbabilityOfTrajectoryOccurence")
                );
    hvar->setTransferFunction("Probability (%)");
    hvar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::Disabled);
    pwcbHSecActor->addActorVariable(hvar, "Synchronization");

    hvar = static_cast<MNWP2DHorizontalActorVariable*>(
                pwcbHSecActor->createActorVariable2(
                    nwpDataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "geopotential_height")
                );
    hvar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::LineContours);
    hvar->addContourSet(true, 1.2, false, QColor(Qt::darkGreen), "[0,26000,40]");
    hvar->addContourSet(true, 2., false, QColor(Qt::darkGreen), "[0,26000,200]");
    // Remove first contour set which was inserted during creation.
    hvar->removeContourSet(0);
    pwcbHSecActor->addActorVariable(hvar, "Synchronization");

    hvar = static_cast<MNWP2DHorizontalActorVariable*>(
                pwcbHSecActor->createActorVariable2(
                    nwpDataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "eastward_wind")
                );
    hvar->setTransferFunction("Wind Speed (m/s)");
    hvar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::Disabled);
    pwcbHSecActor->addActorVariable(hvar, "Synchronization");

    hvar = static_cast<MNWP2DHorizontalActorVariable*>(
                pwcbHSecActor->createActorVariable2(
                    nwpDataSourceID,
                    HYBRID_SIGMA_PRESSURE_3D,
                    "northward_wind")
                );
    hvar->setTransferFunction("Wind Speed (m/s)");
    hvar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::Disabled);
    pwcbHSecActor->addActorVariable(hvar, "Synchronization");

    pwcbHSecActor->setBBox(bbox);
    pwcbHSecActor->setSlicePosition(390.);
    pwcbHSecActor->setEnabled(true);
    glRM->registerActor(pwcbHSecActor);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(pwcbHSecActor);

    // Probability volume actor.
    // =========================
    MNWPVolumeRaycasterActor *pwcbVolumeActor =
            new MNWPVolumeRaycasterActor();
    pwcbVolumeActor->setName("Volume: Probability of WCB occurrence");

    // p(WCB).
    MNWPActorVariable* var = pwcbVolumeActor->createActorVariable2(
                dataSourceID,
                levelType,
                "ProbabilityOfTrajectoryOccurence");
    var->setTransferFunction("Probability (%)");
    pwcbVolumeActor->addActorVariable(var, "Synchronization");

    // p(WCB) region contribution.
    var = pwcbVolumeActor->createActorVariable2(
                dataSourceID + " ProbReg",
                levelType,
                "ProbabilityOfTrajectoryOccurence");
    var->setTransferFunction("Probability (%)");
    pwcbVolumeActor->addActorVariable(var, "Synchronization");

    pwcbVolumeActor->setBoundingBox(bbox, 1050., 100.);
    pwcbVolumeActor->setEnabled(false);
    glRM->registerActor(pwcbVolumeActor);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(pwcbVolumeActor);

//    MValueExtractionAnalysisControl *valueExtractionAnalysis =
//            new MValueExtractionAnalysisControl(pnwpVolumeActor);
//    valueExtractionAnalysis->setMemoryManager(lruAnalysisManager);
//    valueExtractionAnalysis->setScheduler(scheduler);

    MRegionContributionAnalysisControl *regionContributionAnalysis =
            new MRegionContributionAnalysisControl(pwcbVolumeActor);
    regionContributionAnalysis->setMemoryManager(
                sysMC->getMemoryManager("Analysis"));
    regionContributionAnalysis->setScheduler(sysMC->getScheduler("SingleThread"));
}


void MFrontendConfiguration::initializeDefaultActors_PressurePoles(
        QList<MSceneControl*> scenes)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MMovablePoleActor *pressurePoleActor =
            new MMovablePoleActor();
    pressurePoleActor->setEnabled(false);
    pressurePoleActor->addPole(QPointF(10., 50.));
    glRM->registerActor(pressurePoleActor);
    foreach (MSceneControl *scene, scenes)
        scene->addActor(pressurePoleActor);
}


void MFrontendConfiguration::initializeDefaultActors_Trajectories(
        const QString& dataSourceID,
        QList<MSceneControl*> scenes)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    MTransferFunction1D *transferFunctionPressure = new MTransferFunction1D();
    transferFunctionPressure->setName("Colourbar pressure (trajectories predef)");
    transferFunctionPressure->setValueDecimals(0);
    transferFunctionPressure->selectHCLColourmap(
                MTransferFunction1D::QUALITATIVE,
                0., 360., 50., 50., 70., 70., 1., 1., 1., 1., 1., true);
    transferFunctionPressure->setMinimumValue(1000.);
    transferFunctionPressure->setMaximumValue(100.);
    transferFunctionPressure->setNumTicks(10);
    transferFunctionPressure->setSteps(250);
    transferFunctionPressure->setPosition(QRectF(0.9, -0.45, 0.05, 0.5));
    transferFunctionPressure->setEnabled(true);
    glRM->registerActor(transferFunctionPressure);
    foreach (MSceneControl *scene, scenes)
    {
        scene->addActor(transferFunctionPressure);
    }

    MTrajectoryActor *trajectoryActor =
            new MTrajectoryActor();
    trajectoryActor->setDataSourceID(dataSourceID);
    trajectoryActor->setDataSource(dataSourceID + QString(" Reader"));
    trajectoryActor->setNormalsSource(dataSourceID + QString(" Normals"));
    trajectoryActor->setTrajectoryFilter(dataSourceID
                                         + QString(" timestepFilter"));
    trajectoryActor->setTransferFunction(transferFunctionPressure->getName());
    trajectoryActor->synchronizeWith(sysMC->getSyncControl("Synchronization"));
    trajectoryActor->setEnabled(true);
    glRM->registerActor(trajectoryActor);
    foreach (MSceneControl *scene, scenes)
    {
        scene->addActor(trajectoryActor);
    }
}


void MFrontendConfiguration::initializeDevelopmentFrontend()
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    initializeSynchronization(
                "Synchronization",
                QStringList("ECMWF ENS EUR_LL10"));

    // Create scene controls.
    //==========================================================================

    // Create a scene (later the user should be able to add scenes).
    MSceneControl *scene = new MSceneControl("Scene 1");
    glRM->registerScene(scene);
    sysMC->getMainWindow()->dockSceneControl(scene);

    // A second scene, sharing some actors with the first.
    MSceneControl *scene2 = new MSceneControl("Scene 2");
    glRM->registerScene(scene2);
    sysMC->getMainWindow()->dockSceneControl(scene2);

    MSceneControl *scene3 = new MSceneControl("Scene 3");
    glRM->registerScene(scene3);
    sysMC->getMainWindow()->dockSceneControl(scene3);

    MSceneControl *scene4 = new MSceneControl("Scene 4");
    glRM->registerScene(scene4);
    sysMC->getMainWindow()->dockSceneControl(scene4);

    // Waypoints model.
    // =========================================================================

    // Create a waypoints table model and a corresponding table view.
    MWaypointsTableModel *waypointsModel = new MWaypointsTableModel("Waypoints");
    waypointsModel->loadFromFile("data/default_waypoints.ftml");
    sysMC->registerWaypointsModel(waypointsModel);
    sysMC->getMainWindow()->dockWaypointsModel(waypointsModel);

    // Add ACTORS to the scenes.
    //==========================================================================

    sysMC->getMainWindow()->setSceneViewLayout(1);

    initializeDefaultActors_Basemap(
                "/home/local/data/naturalearth/HYP_50M_SR_W/HYP_50M_SR_W.tif",
                QRectF(-90., 0., 180., 90.),
                QList<MSceneControl*>() << scene << scene2 << scene3 << scene4);

    initializeDefaultActors_VolumeBox(
                QRectF(-60., 30., 100., 40.),
                QList<MSceneControl*>() << scene << scene2 << scene3 << scene4);

    initializeDefaultActors_MSLP(
                "ECMWF ENS EUR_LL10 ENSFilter",
                QRectF(-60., 30., 100., 40.),
                QList<MSceneControl*>() << scene << scene2);

//    initializeDefaultActors_Surface(
//                "ECMWF ENS EUR_LL10 ENSFilter",
//                QRectF(-60., 30., 100., 40.),
//                QList<MSceneControl*>() << scene);

    initializeDefaultActors_VSec_PV(
                "ECMWF ENS EUR_LL10 ENSFilter",
                QList<MSceneControl*>() << scene);

//    initializeDefaultActors_VSec_Clouds(
//                "ECMWF ENS EUR_LL10 ENSFilter",
//                QList<MSceneControl*>() << scene);

//    initializeDefaultActors_HSec(
//                "ECMWF ENS EUR_LL10 ENSFilter",
//                QRectF(-60., 30., 100., 40.),
//                QList<MSceneControl*>() << scene);

//    initializeDefaultActors_HSec_Difference(
//                "ECMWF ENS EUR_LL10 ENSFilter",
//                QRectF(-60., 30., 100., 40.),
//                QList<MSceneControl*>() << scene);

//    initializeDefaultActors_PressurePoles(
//                QList<MSceneControl*>() << scene);

//    initializeDefaultActors_VolumeProbability(
//                "Lagranto ENS EUR_LL10 DF-T psfc_1000hPa_L62",
//                PRESSURE_LEVELS_3D,
//                "ECMWF ENS EUR_LL10 ENSFilter",
//                QRectF(-60., 30., 100., 40.),
//                QList<MSceneControl*>() << scene);

//    initializeDefaultActors_Volume(
//                "ECMWF ENS EUR_LL10 ENSFilter",
//                QRectF(-60., 30., 100., 40.),
//                QList<MSceneControl*>() << scene);

//    initializeDefaultActors_Trajectories(
//                "Lagranto ENS EUR_LL10 DF-T psfc_1000hPa_L62",
//                QList<MSceneControl*>() << scene);
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


} // namespace Met3D
