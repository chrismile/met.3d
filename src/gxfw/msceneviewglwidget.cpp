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
#include "msceneviewglwidget.h"

// standard library imports
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

// related third party imports
#include <QtGui>
#include <QtOpenGL>
#include <log4cplus/loggingmacros.h>
#include <QFileInfo>

// local application imports
#include "util/mutil.h"
#include "gxfw/mtypes.h"
#include "gxfw/textmanager.h"
#include "mainwindow.h"

using namespace std;


namespace Met3D
{

// Static counter for all scene views.
unsigned int MSceneViewGLWidget::idCounter = 0;


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSceneViewGLWidget::MSceneViewGLWidget()
    : QGLWidget(MGLResourcesManager::getInstance()->format(), 0,
                MGLResourcesManager::getInstance()),
      scene(nullptr),
      lastPoint(QVector3D(0,0,0)),
      sceneNavigationMode(MOVE_CAMERA),
      sceneNavigationMode_NoActorInteraction(MOVE_CAMERA),
      sceneRotationCentre(QVector3D(0,0,1020)),
      cameraAutorotationMode(false),
      freezeMode(0),
      sceneNavigationSensitivity(1.),
      posLabelIsEnabled(true),
      multisamplingEnabled(true),
      antialiasingEnabled(false),
      measureFPS(false),
      measureFPSFrameCount(0),
      sceneNameLabel(nullptr),
      visualizationParameterChange(false),
      cameraSyncronizedWith(nullptr),
      singleInteractionActor(nullptr),
      enablePropertyEvents(true),
      resizeViewDialog(new MResizeWindowDialog),
      overwriteImageSerie(false)
{
    viewIsInitialised = false;
    focusShader = nullptr;

    // Obtain a "personal" identification number.
    this->myID = MSceneViewGLWidget::idCounter++;

    backgroundColour = QColor(255, 255, 255);

    pbot    = 1050.; // hPa
    ptop    = 20.;
    logpbot = log(pbot);
    zbot    = 0.;
    ztop    = 36.;
    slopePtoZ = (ztop - zbot) / (log(ptop) - log(pbot));

    actorInteractionMode      = false;
    analysisMode         = false;
    userIsInteracting    = false;
    userIsScrolling      = false;
    viewportResized      = false;

    pickedActor.actor    = 0;
    pickedActor.handleID = -1;

    // Initial camera position.
//    camera.setOrigin(QVector3D(180., 0., 0.));
    camera.rotate(20., 0., 0., 1.);
    camera.rotate(40., 1., 0., 0.);
    camera.moveForward(-160.);
    camera.moveUp(-30.);
    camera.moveRight(-20);
    sceneRotationMatrix.setToIdentity();

    lightDirection = TOP;
    sceneNorthWestRotationMatrix.rotate(45, 1, 0, 0);
    sceneNorthWestRotationMatrix.rotate(135, 0, 1, 0);

    // Focus policy: Accept focus by both tab and click.
    setFocusPolicy(Qt::StrongFocus);

    fpsStopwatch = new MStopwatch();
    frameCount   = 0;

    if (myID == 0)
    {
        // Scene view with ID 0 measures system frame rate performace.
        fpsTimer = new QTimer(this);
        connect(fpsTimer, SIGNAL(timeout()), SLOT(updateFPSTimer()));
        fpsTimer->start(1000); // update fps display every 1000ms
        splitNextFrame = false;
        fpsTimeseriesSize = 60;
        fpsTimeseriesIndex = 0;
        fpsTimeseries = new float[fpsTimeseriesSize];
        for (int i = 0; i < fpsTimeseriesSize; i++) fpsTimeseries[i] = -1;
    }

    checkScrollTimer.setInterval(250);
    connect(&checkScrollTimer, SIGNAL(timeout()),this, SLOT(checkUserScrolling()));
    checkScrollTimer.start();

    MSystemManagerAndControl *systemControl = MSystemManagerAndControl::getInstance();

    // Create a property group for this scene view's properties. The group will
    // be displayed in the properties browser in the scene view control.
    propertyGroup = systemControl->getGroupPropertyManager()
            ->addProperty(QString("Scene view #%1").arg(myID+1));

    // Configuration properties.
    configurationSupGroup = systemControl->getGroupPropertyManager()
            ->addProperty("configuration");
    propertyGroup->addSubProperty(configurationSupGroup);

    loadConfigProperty = systemControl->getClickPropertyManager()
            ->addProperty("load");
    configurationSupGroup->addSubProperty(loadConfigProperty);

    saveConfigProperty = systemControl->getClickPropertyManager()
            ->addProperty("save");
    configurationSupGroup->addSubProperty(saveConfigProperty);


    // Camera position.
    cameraPositionProperty = systemControl->getStringPropertyManager()
            ->addProperty("camera position");
    propertyGroup->addSubProperty(cameraPositionProperty);

    // Camera group.
    cameraGroupProperty = systemControl->getGroupPropertyManager()
            ->addProperty("modify camera");
    propertyGroup->addSubProperty(cameraGroupProperty);

    cameraLoadFromFileProperty = systemControl->getClickPropertyManager()
            ->addProperty("load");
    cameraGroupProperty->addSubProperty(cameraLoadFromFileProperty);

    cameraSaveToFileProperty = systemControl->getClickPropertyManager()
            ->addProperty("save");
    cameraGroupProperty->addSubProperty(cameraSaveToFileProperty);

    cameraSetNorthUpProperty = systemControl->getClickPropertyManager()
            ->addProperty("set north up");
    cameraGroupProperty->addSubProperty(cameraSetNorthUpProperty);

    cameraSetUprightProperty = systemControl->getClickPropertyManager()
            ->addProperty("set upright");
    cameraGroupProperty->addSubProperty(cameraSetUprightProperty);

    cameraSetTopViewProperty = systemControl->getClickPropertyManager()
            ->addProperty("set top view");
    cameraGroupProperty->addSubProperty(cameraSetTopViewProperty);

    // Interaction group.
    interactionGroupProperty = systemControl->getGroupPropertyManager()
            ->addProperty("interaction");
    propertyGroup->addSubProperty(interactionGroupProperty);

    resizeProperty = systemControl->getClickPropertyManager()
            ->addProperty("resize");
    interactionGroupProperty->addSubProperty(resizeProperty);

    sceneSaveToImageProperty = systemControl->getClickPropertyManager()
            ->addProperty("save to image file");
    interactionGroupProperty->addSubProperty(sceneSaveToImageProperty);

    sceneNavigationModeProperty = systemControl->getEnumPropertyManager()
            ->addProperty("scene navigation");
    systemControl->getEnumPropertyManager()->setEnumNames(
                sceneNavigationModeProperty,
                QStringList() << "move camera" << "rotate scene"
                << "2D top view");
    interactionGroupProperty->addSubProperty(sceneNavigationModeProperty);

    sceneRotationCenterProperty = systemControl->getGroupPropertyManager()
            ->addProperty("scene rotation centre");
    sceneRotationCenterProperty->setEnabled(false);

    QtExtensions::QtDecoratedDoublePropertyManager *doublePropertyManager =
            MSystemManagerAndControl::getInstance()->getDecoratedDoublePropertyManager();
    sceneRotationCentreLonProperty = doublePropertyManager->addProperty("longitude");
    sceneRotationCenterProperty->addSubProperty(sceneRotationCentreLonProperty);
    sceneRotationCentreLatProperty = doublePropertyManager->addProperty("latitude");
    sceneRotationCenterProperty->addSubProperty(sceneRotationCentreLatProperty);
    sceneRotationCentreElevationProperty = doublePropertyManager->addProperty("elevation");

    doublePropertyManager->setSuffix(sceneRotationCentreLonProperty,
                                     QString::fromUtf8("\u00B0"));
    doublePropertyManager->setSuffix(sceneRotationCentreLatProperty,
                                     QString::fromUtf8("\u00B0"));
    doublePropertyManager->setSuffix(sceneRotationCentreElevationProperty, " hPa");
    doublePropertyManager->setValue(sceneRotationCentreElevationProperty, 1020);
    doublePropertyManager->setMinimum(sceneRotationCentreElevationProperty, 20);
    doublePropertyManager->setMaximum(sceneRotationCentreElevationProperty, 1020);

    sceneRotationCenterProperty->addSubProperty(sceneRotationCentreElevationProperty);
    interactionGroupProperty->addSubProperty(sceneRotationCenterProperty);

    selectSceneRotationCentreProperty  = systemControl->getClickPropertyManager()
            ->addProperty("interactively select rotation centre");
    sceneRotationCenterProperty->addSubProperty(selectSceneRotationCentreProperty);
    selectSceneRotationCentreProperty->setEnabled(false);

    sceneNavigationSensitivityProperty = systemControl->getDecoratedDoublePropertyManager()
            ->addProperty("navigation sensitivity");
    systemControl->getDecoratedDoublePropertyManager()
            ->setValue(sceneNavigationSensitivityProperty, sceneNavigationSensitivity);
    systemControl->getDecoratedDoublePropertyManager()
            ->setMinimum(sceneNavigationSensitivityProperty, 1.);
    systemControl->getDecoratedDoublePropertyManager()
            ->setMaximum(sceneNavigationSensitivityProperty, 100.);
    interactionGroupProperty->addSubProperty(sceneNavigationSensitivityProperty);

    cameraAutoRotationModeProperty = systemControl->getBoolPropertyManager()
            ->addProperty("auto-rotate camera");
    cameraAutoRotationModeProperty->setEnabled(false);
    interactionGroupProperty->addSubProperty(cameraAutoRotationModeProperty);

    QList<MSceneViewGLWidget*> otherViews = systemControl->getRegisteredViews();
    QStringList otherViewLabels;
    otherViewLabels << "None";
    for (int i = 0; i < otherViews.size(); i++)
        otherViewLabels << QString("view #%1").arg(otherViews[i]->getID()+1);
    syncCameraWithViewProperty = systemControl->getEnumPropertyManager()
            ->addProperty("sync camera with view");
    systemControl->getEnumPropertyManager()
            ->setEnumNames(syncCameraWithViewProperty, otherViewLabels);
    systemControl->getEnumPropertyManager()
            ->setValue(syncCameraWithViewProperty, 0);
    interactionGroupProperty->addSubProperty(syncCameraWithViewProperty);

    // Register modify mode property.
    actorInteractionProperty = systemControl->getBoolPropertyManager()
            ->addProperty("actor interaction mode");
    systemControl->getBoolPropertyManager()
            ->setValue(actorInteractionProperty, actorInteractionMode);
    interactionGroupProperty->addSubProperty(actorInteractionProperty);

    analysisModeProperty = systemControl->getBoolPropertyManager()
            ->addProperty("analysis mode");
    systemControl->getBoolPropertyManager()
            ->setValue(analysisModeProperty, analysisMode);
    interactionGroupProperty->addSubProperty(analysisModeProperty);

    // Position label properties.
    posLabelEnableProperty = systemControl->getBoolPropertyManager()
            ->addProperty("draw position labels");
    systemControl->getBoolPropertyManager()->setValue(posLabelEnableProperty,
                                                      posLabelIsEnabled);
    interactionGroupProperty->addSubProperty(posLabelEnableProperty);

    // Rendering group.
    renderingGroupProperty = systemControl->getGroupPropertyManager()
            ->addProperty("rendering");
    propertyGroup->addSubProperty(renderingGroupProperty);

    backgroundColourProperty = systemControl->getColorPropertyManager()
            ->addProperty("background colour");
    systemControl->getColorPropertyManager()
            ->setValue(backgroundColourProperty, backgroundColour);
    renderingGroupProperty->addSubProperty(backgroundColourProperty);

    multisamplingProperty = systemControl->getBoolPropertyManager()
            ->addProperty("multisampling");
    systemControl->getBoolPropertyManager()
            ->setValue(multisamplingProperty, multisamplingEnabled);
    renderingGroupProperty->addSubProperty(multisamplingProperty);

    antialiasingProperty = systemControl->getBoolPropertyManager()
            ->addProperty("antialiasing");
    systemControl->getBoolPropertyManager()
            ->setValue(antialiasingProperty, antialiasingEnabled);
    renderingGroupProperty->addSubProperty(antialiasingProperty);

    renderLabelsWithDepthTest = true;
    labelDepthTestProperty = systemControl->getBoolPropertyManager()
            ->addProperty("depth test for labels");
    systemControl->getBoolPropertyManager()
            ->setValue(labelDepthTestProperty, renderLabelsWithDepthTest);
    renderingGroupProperty->addSubProperty(labelDepthTestProperty);

    QStringList lightingOptions;
    lightingOptions << "World North-West"
                    << "Scene North-West"
                    << "View Direction"
                    << "Top";
    lightingProperty = systemControl->getEnumPropertyManager()
            ->addProperty("lighting");
    systemControl->getEnumPropertyManager()
            ->setEnumNames(lightingProperty, lightingOptions);
    systemControl->getEnumPropertyManager()
            ->setValue(lightingProperty, lightDirection);
    renderingGroupProperty->addSubProperty(lightingProperty);

    verticalScalingProperty = systemControl->getDecoratedDoublePropertyManager()
            ->addProperty("vertical scaling");
    systemControl->getDecoratedDoublePropertyManager()
            ->setValue(verticalScalingProperty, ztop);
    systemControl->getDecoratedDoublePropertyManager()
            ->setMinimum(verticalScalingProperty, 1.);
    systemControl->getDecoratedDoublePropertyManager()
            ->setMaximum(verticalScalingProperty, 999.);
    renderingGroupProperty->addSubProperty(verticalScalingProperty);

#ifndef CONTINUOUS_GL_UPDATE
    measureFPSProperty = systemControl->getClickPropertyManager()
            ->addProperty("30s FPS measurement");
    renderingGroupProperty->addSubProperty(measureFPSProperty);
#endif

    // North arrow.
    northArrow.enabled = false;
    northArrow.horizontalScale = 5.;
    northArrow.verticalScale = 5.;
    northArrow.colour = QColor(222, 46, 30, 255);
    northArrow.lon = 0.;
    northArrow.lat = 80.;
    northArrow.worldZ = 1.;
    northArrow.groupProperty = systemControl->getGroupPropertyManager()
            ->addProperty("arrow pointing north");
    propertyGroup->addSubProperty(northArrow.groupProperty);

    northArrow.enabledProperty = systemControl->getBoolPropertyManager()
            ->addProperty("enabled");
    systemControl->getBoolPropertyManager()
            ->setValue(northArrow.enabledProperty, northArrow.enabled);
    northArrow.groupProperty->addSubProperty(northArrow.enabledProperty);

    northArrow.horizontalScaleProperty =
            systemControl->getDecoratedDoublePropertyManager()->addProperty(
                "horizontal scale");
    systemControl->getDecoratedDoublePropertyManager()
            ->setValue(northArrow.horizontalScaleProperty,
                       northArrow.horizontalScale);
    systemControl->getDecoratedDoublePropertyManager()
            ->setSingleStep(northArrow.horizontalScaleProperty, .1);
    systemControl->getDecoratedDoublePropertyManager()
            ->setDecimals(northArrow.horizontalScaleProperty, 2);
    systemControl->getDecoratedDoublePropertyManager()
            ->setMinimum(northArrow.horizontalScaleProperty, 0.01);
    systemControl->getDecoratedDoublePropertyManager()
            ->setMaximum(northArrow.horizontalScaleProperty, 100.);
    northArrow.groupProperty->addSubProperty(northArrow.horizontalScaleProperty);

    northArrow.verticalScaleProperty =
            systemControl->getDecoratedDoublePropertyManager()->addProperty(
                "vertical scale");
    systemControl->getDecoratedDoublePropertyManager()
            ->setValue(northArrow.verticalScaleProperty,
                       northArrow.verticalScale);
    systemControl->getDecoratedDoublePropertyManager()
            ->setSingleStep(northArrow.verticalScaleProperty, .1);
    systemControl->getDecoratedDoublePropertyManager()
            ->setDecimals(northArrow.verticalScaleProperty, 2);
    systemControl->getDecoratedDoublePropertyManager()
            ->setMinimum(northArrow.verticalScaleProperty, 0.01);
    systemControl->getDecoratedDoublePropertyManager()
            ->setMaximum(northArrow.verticalScaleProperty, 100.);
    northArrow.groupProperty->addSubProperty(northArrow.verticalScaleProperty);

    northArrow.lonPositionProperty =
            systemControl->getDecoratedDoublePropertyManager()
            ->addProperty("longitude");
    systemControl->getDecoratedDoublePropertyManager()
            ->setValue(northArrow.lonPositionProperty, northArrow.lon);
    systemControl->getDecoratedDoublePropertyManager()
            ->setSingleStep(northArrow.lonPositionProperty, .1);
    northArrow.groupProperty->addSubProperty(northArrow.lonPositionProperty);

    northArrow.latPositionProperty =
            systemControl->getDecoratedDoublePropertyManager()
            ->addProperty("latitude");
    systemControl->getDecoratedDoublePropertyManager()
            ->setValue(northArrow.latPositionProperty, northArrow.lat);
    systemControl->getDecoratedDoublePropertyManager()
            ->setSingleStep(northArrow.latPositionProperty, .1);
    northArrow.groupProperty->addSubProperty(northArrow.latPositionProperty);

    northArrow.worldZPositionProperty =
            systemControl->getDecoratedDoublePropertyManager()
            ->addProperty("z");
    systemControl->getDecoratedDoublePropertyManager()
            ->setValue(northArrow.worldZPositionProperty, northArrow.worldZ);
    systemControl->getDecoratedDoublePropertyManager()
            ->setSingleStep(northArrow.worldZPositionProperty, .1);
    northArrow.groupProperty->addSubProperty(northArrow.worldZPositionProperty);

    northArrow.colourProperty = systemControl->getColorPropertyManager()
            ->addProperty("colour");
    systemControl->getColorPropertyManager()
            ->setValue(northArrow.colourProperty, northArrow.colour);
    northArrow.groupProperty->addSubProperty(northArrow.colourProperty);


    // Inform the scene view control about this scene view and connect to its
    // propertyChanged() signal.
    systemControl->registerSceneView(this);
    connect(systemControl->getBoolPropertyManager(),
            SIGNAL(propertyChanged(QtProperty*)),
            SLOT(onPropertyChanged(QtProperty*)));
    connect(systemControl->getEnumPropertyManager(),
            SIGNAL(propertyChanged(QtProperty*)),
            SLOT(onPropertyChanged(QtProperty*)));
    connect(systemControl->getDecoratedDoublePropertyManager(),
            SIGNAL(propertyChanged(QtProperty*)),
            SLOT(onPropertyChanged(QtProperty*)));
    connect(systemControl->getClickPropertyManager(),
            SIGNAL(propertyChanged(QtProperty*)),
            SLOT(onPropertyChanged(QtProperty*)));
    connect(systemControl->getColorPropertyManager(),
            SIGNAL(propertyChanged(QtProperty*)),
            SLOT(onPropertyChanged(QtProperty*)));

    // Set up a timer for camera auto-rotation.
    cameraAutoRotationTimer = new QTimer();
    cameraAutoRotationTimer->setInterval(20);
    connect(cameraAutoRotationTimer, SIGNAL(timeout()),
            this, SLOT(autoRotateCamera()));
}


MSceneViewGLWidget::~MSceneViewGLWidget()
{
    delete fpsStopwatch;
    if (focusShader) delete focusShader;

    if (myID == 0) {
        LOG4CPLUS_DEBUG(mlog, " ====== FPS timeseries ======");
        QString s;
        for (int i = 0; i < fpsTimeseriesSize; i++)
            s += QString("%1 ").arg(fpsTimeseries[i]);
        LOG4CPLUS_DEBUG(mlog, s.toStdString());
        delete[] fpsTimeseries;
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MSceneViewGLWidget::setScene(MSceneControl *scene)
{
    removeCurrentScene();

    LOG4CPLUS_DEBUG(mlog, "scene view " << myID+1 << " connects to scene "
                    << scene->getName().toStdString());
    this->scene = scene;
    this->scene->registerSceneView(this);

#ifndef CONTINUOUS_GL_UPDATE
    connect(this->scene,
            SIGNAL(sceneChanged()),
            SLOT(updateGL()));
#endif

    if (!viewIsInitialised) return;

    updateSceneLabel();
//    updateDisplayTime();

#ifndef CONTINUOUS_GL_UPDATE
    updateGL();
#endif
}


void MSceneViewGLWidget::removeCurrentScene()
{
    // If this view is currently connected to a scene, disconnect from this
    // scene.
    if (scene)
    {
        LOG4CPLUS_DEBUG(mlog, "scene view " << myID+1
                        << " disconnects from scene "
                        << scene->getName().toStdString());

        this->scene->unregisterSceneView(this);

#ifndef CONTINUOUS_GL_UPDATE
        disconnect(scene,
                   SIGNAL(sceneChanged()),
                   this,
                   SLOT(updateGL()));
#endif
    }

    scene = nullptr;
}


QSize MSceneViewGLWidget::minimumSizeHint() const
{
    return QSize(80, 60);
}


QSize MSceneViewGLWidget::sizeHint() const
{
    return QSize(80, 60);
}


void MSceneViewGLWidget::setBackgroundColour(const QColor &color)
{
    backgroundColour = color;
#ifndef CONTINUOUS_GL_UPDATE
    updateGL();
#endif
}


double MSceneViewGLWidget::worldZfromPressure(double p_hPa)
{
    return worldZfromPressure(p_hPa, logpbot, slopePtoZ);
}


double MSceneViewGLWidget::worldZfromPressure(
        double p_hPa, double log_pBottom_hPa, double deltaZ_deltaLogP)
{
    return (log(p_hPa)-log_pBottom_hPa) * deltaZ_deltaLogP;
}


double MSceneViewGLWidget::pressureFromWorldZ(double z)
{
    return exp(z / slopePtoZ + logpbot);
}


QVector2D MSceneViewGLWidget::pressureToWorldZParameters()
{
    return QVector2D(logpbot, slopePtoZ);
}


QVector3D MSceneViewGLWidget::lonLatPToClipSpace(const QVector3D& lonlatp)
{
    QVector3D worldspace = QVector3D(lonlatp.x(),
                                     lonlatp.y(),
                                     worldZfromPressure(lonlatp.z()));
    return modelViewProjectionMatrix * worldspace;
}


QVector3D MSceneViewGLWidget::clipSpaceToLonLatWorldZ(const QVector3D &clipPos)
{
    QVector3D worldSpacePos = modelViewProjectionMatrix.inverted() * clipPos;
    return worldSpacePos;
}


QVector3D MSceneViewGLWidget::clipSpaceToLonLatP(const QVector3D &clipPos)
{
    QVector3D lonLatPPos = clipSpaceToLonLatWorldZ(clipPos);
    lonLatPPos.setZ(pressureFromWorldZ(lonLatPPos.z()));
    return lonLatPPos;
}


QVector3D MSceneViewGLWidget::getLightDirection()
{
    switch (lightDirection)
    {
    case SCENE_NORTHWEST:
        {
            // Rotation of camera view direction.
            QVector3D lightDir = sceneNorthWestRotationMatrix * camera.getZAxis();
            return -1. * lightDir.normalized();
        }
        break;
    case VIEWDIRECTION:
        return camera.getZAxis();
        break;
    case TOP:
        return QVector3D(0, 0, -1);
        break;
    case WORLD_NORTHWEST:
    default:
        return QVector3D(1, -1, -1).normalized(); // specific to cyl proj!
        break;
    }
}


void MSceneViewGLWidget::setVerticalScaling(float scaling)
{
    MSystemManagerAndControl::getInstance()->getDecoratedDoublePropertyManager()
            ->setValue(verticalScalingProperty, scaling);
}


void MSceneViewGLWidget::setInteractionMode(bool enabled)
{
    // Analysis mode cannot be active at the same time.
    if (enabled && analysisMode)
        MSystemManagerAndControl::getInstance()->getBoolPropertyManager()
                ->setValue(analysisModeProperty, false);

    MSystemManagerAndControl::getInstance()->getBoolPropertyManager()
            ->setValue(actorInteractionProperty, enabled);
}


void MSceneViewGLWidget::setAnalysisMode(bool enabled)
{
    // Interaction mode cannot be active at the same time.
    if (enabled && actorInteractionMode)
        MSystemManagerAndControl::getInstance()->getBoolPropertyManager()
                ->setValue(actorInteractionProperty, false);

    MSystemManagerAndControl::getInstance()->getBoolPropertyManager()
            ->setValue(analysisModeProperty, enabled);
}


void MSceneViewGLWidget::setAutoRotationMode(bool enabled)
{
    // Auto-rotation can only be set in ROTATE_SCENE mode.
    if (sceneNavigationMode != ROTATE_SCENE) return;

    MSystemManagerAndControl::getInstance()->getBoolPropertyManager()
            ->setValue(cameraAutoRotationModeProperty, enabled);
}


void MSceneViewGLWidget::setFreeze(bool enabled)
{
    if (enabled)
    {
        freezeMode++;
    }
    else
    {
        freezeMode--;
        if (freezeMode < 0) freezeMode = 0;
    }

#ifndef CONTINUOUS_GL_UPDATE
    if ( viewIsInitialised && (!freezeMode) ) updateGL();
#endif
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MSceneViewGLWidget::executeCameraAction(int action,
                                             bool ignoreWithoutFocus)
{
    // Only act on this signal if we have input focus.
    if ( ignoreWithoutFocus && (!hasFocus()) ) return;

    // Get current camera axes.
    QVector3D yAxis = camera.getYAxis();
    QVector3D zAxis = camera.getZAxis();
    QVector3D origin = camera.getOrigin();

    // Modify axes.
    switch (action)
    {
    case CAMERA_NORTHUP:
        yAxis.setX(0);
        zAxis.setX(0);
        camera.setYAxis(yAxis);
        camera.setZAxis(zAxis);
        break;
    case CAMERA_TOPVIEW:
        origin.setZ(250.);
        camera.setOrigin(origin);
        camera.setYAxis(QVector3D(0, 1., 0));
        camera.setZAxis(QVector3D(0, 0, -1.));
        break;
    case CAMERA_UPRIGHT:
        camera.setYAxis(QVector3D(0, 0, 1.));
        camera.setZAxis(QVector3D::crossProduct(
                            QVector3D(0, 0, 1.), camera.getXAxis()));
        break;
    case CAMERA_SAVETOFILE:
        camera.saveToFile(QFileDialog::getSaveFileName(
                              MGLResourcesManager::getInstance(),
                              "Save current camera",
                              "data/camera",
                              "Camera configuration files (*.camera.conf)"));
        break;
    case CAMERA_LOADFROMFILE:
        camera.loadFromFile(QFileDialog::getOpenFileName(
                                MGLResourcesManager::getInstance(),
                                "Open camera",
                                "data/camera",
                                "Camera configuration files (*.camera.conf)"));
        break;
    }

    updateCameraPositionDisplay();

#ifndef CONTINUOUS_GL_UPDATE
    if (viewIsInitialised && (!freezeMode)) updateGL();
#endif
}


void MSceneViewGLWidget::onPropertyChanged(QtProperty *property)
{

    if (!enablePropertyEvents) return;

    if (property == loadConfigProperty)
    {
        loadConfigurationFromFile();
    }

    else if (property == saveConfigProperty)
    {
        saveConfigurationToFile();
    }

    else if (property == actorInteractionProperty)
    {
        // Toggle actor interaction mode.
        actorInteractionMode = MSystemManagerAndControl::getInstance()
                ->getBoolPropertyManager()->value(actorInteractionProperty);

        if ( actorInteractionMode )
        {
            // "actorInteractionMode" was switched from "false" to "true". Save
            // the current scene navigation mode and switch to MOVE_CAMERA.
            sceneNavigationMode_NoActorInteraction = sceneNavigationMode;

            if (sceneNavigationMode == ROTATE_SCENE)
            {
                sceneNavigationMode = MOVE_CAMERA;
                sceneNavigationModeProperty->setEnabled(false);
                selectSceneRotationCentreProperty->setEnabled(false);
                sceneRotationCenterProperty->setEnabled(false);
            }
        }
        else
        {
            // "actorInteractionMode" was switched from "true" to "false".
            // Restore scene navigation mode.
            sceneNavigationMode = sceneNavigationMode_NoActorInteraction;
            sceneNavigationModeProperty->setEnabled(true);
            MSystemManagerAndControl::getInstance()->getEnumPropertyManager()
                    ->setValue(sceneNavigationModeProperty, sceneNavigationMode);
        }

        // In actor interaction mode, mouse tracking is enabled to have
        // mouseMoveEvent() executed on every mouse move, not only when a
        // button is pressed.
        setMouseTracking(actorInteractionMode);
        updateSceneLabel();
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == analysisModeProperty)
    {
        analysisMode = MSystemManagerAndControl::getInstance()->getBoolPropertyManager()
                ->value(analysisModeProperty);
        updateSceneLabel();
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == backgroundColourProperty)
    {
        setBackgroundColour(
                MSystemManagerAndControl::getInstance()
                ->getColorPropertyManager()->value(backgroundColourProperty));
    }

    else if (property == multisamplingProperty)
    {
        multisamplingEnabled = MSystemManagerAndControl::getInstance()
                ->getBoolPropertyManager()->value(multisamplingProperty);
        // Toggle antialiasing by multisampling.
        if (multisamplingEnabled)
        {
            glEnable(GL_MULTISAMPLE);
        }
        else
        {
            glDisable(GL_MULTISAMPLE);
        }
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == antialiasingProperty)
    {
        antialiasingEnabled = MSystemManagerAndControl::getInstance()
                ->getBoolPropertyManager()->value(antialiasingProperty);
        // Toggle antialiasing by point/line/polygon smoothing.
        if (antialiasingEnabled)
        {
            glEnable(GL_POINT_SMOOTH);
            glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
            glEnable(GL_LINE_SMOOTH);
            glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
            glEnable(GL_POLYGON_SMOOTH);
            glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
        }
        else
        {
            glDisable(GL_LINE_SMOOTH);
            glDisable(GL_POINT_SMOOTH);
            glDisable(GL_POLYGON_SMOOTH);
        }
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == labelDepthTestProperty)
    {
        // Toggle depth test for labels.
        renderLabelsWithDepthTest = MSystemManagerAndControl::getInstance()
                ->getBoolPropertyManager()->value(labelDepthTestProperty);
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == lightingProperty)
    {
        // Set lighting direction.
        lightDirection = LightDirection(
                    MSystemManagerAndControl::getInstance()->getEnumPropertyManager()
                    ->value(lightingProperty));
        LOG4CPLUS_DEBUG(mlog, "Setting light direction to" << lightDirection);
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == verticalScalingProperty)
    {
        // Recompute pressure-to-worldZ slope.
        ztop = MSystemManagerAndControl::getInstance()->getDecoratedDoublePropertyManager()
                ->value(verticalScalingProperty);
        slopePtoZ = (ztop - zbot) / (log(ptop) - log(pbot));
        // This will be set to false at the end of the next render cycle.
        visualizationParameterChange = true;

#ifndef CONTINUOUS_GL_UPDATE
        if (viewIsInitialised) updateGL();
#endif
    }

    else if (property == syncCameraWithViewProperty)
    {
        if (cameraSyncronizedWith != nullptr)
        {
            cameraSyncronizedWith->removeCameraSync(this);
            cameraSyncronizedWith = nullptr;
        }

        int index = MSystemManagerAndControl::getInstance()->getEnumPropertyManager()
                ->value(syncCameraWithViewProperty);

        if (index > 0)
        {
            MSceneViewGLWidget* otherView =
                    MSystemManagerAndControl::getInstance()->getRegisteredViews()[index-1];
            otherView->addCameraSync(this);
            cameraSyncronizedWith = otherView;
        }

#ifndef CONTINUOUS_GL_UPDATE
        if (viewIsInitialised) updateGL();
#endif
    }

    else if (property == resizeProperty)
    {
        resizeView();
    }

    else if (property == sceneSaveToImageProperty)
    {
        saveScreenshot();
    }

    else if (property == sceneNavigationModeProperty)
    {
        // Disable auto-rotation when scene navigation is changed.
        setAutoRotationMode(false);

        sceneNavigationMode = (SceneNavigationMode)MSystemManagerAndControl::getInstance()
                ->getEnumPropertyManager()->value(sceneNavigationModeProperty);

        enablePropertyEvents = false;
        if (sceneNavigationMode == MOVE_CAMERA)
        {
            sceneRotationCenterProperty->setEnabled(false);
            selectSceneRotationCentreProperty->setEnabled(false);
            cameraAutoRotationModeProperty->setEnabled(false);
        }
        else if (sceneNavigationMode == ROTATE_SCENE)
        {
            sceneRotationCenterProperty->setEnabled(true);
            selectSceneRotationCentreProperty->setEnabled(true);
            cameraAutoRotationModeProperty->setEnabled(true);
        }
        else if (sceneNavigationMode == TOPVIEW_2D)
        {
            // !TODO: disable camera zooming

            QVector3D eyePos = QVector3D(-10, 50, 100);//camera.getOrigin();

            camera.setOrigin(eyePos);
            camera.setZAxis(QVector3D(0, 0, -1));
            camera.setYAxis(QVector3D(0, 1, 0));
        }

        enablePropertyEvents = true;
        updateSceneLabel();
        updateGL();
    }

    else if (property == sceneRotationCentreElevationProperty ||
             property == sceneRotationCentreLatProperty       ||
             property == sceneRotationCentreLonProperty)
    {
        QtExtensions::QtDecoratedDoublePropertyManager *doublePropertyManager =
                MSystemManagerAndControl::getInstance()->getDecoratedDoublePropertyManager();
        double p   = doublePropertyManager->value(sceneRotationCentreElevationProperty);
        double lon = doublePropertyManager->value(sceneRotationCentreLonProperty);
        double lat = doublePropertyManager->value(sceneRotationCentreLatProperty);
        sceneRotationCentre = QVector3D(lon, lat, p);
    }

    else if ( property == selectSceneRotationCentreProperty &&
              selectSceneRotationCentreProperty->isEnabled() )
    {

        selectSceneRotationCentreProperty->setEnabled(false);
        MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
        MLabel *pickText  = glRM->getSceneRotationCentreSelectionLabel();
        MMovablePoleActor *pickActor = glRM->getSceneRotationCentreSelectionPoleActor();

        pickActor->removeAllPoles();
        pickActor->addPole(sceneRotationCentre.toPointF());

        sceneNavigationMode = MOVE_CAMERA;

        scene->addActor(pickActor);
        scene->setSingleInteractionActor(pickActor);

        foreach (MSceneViewGLWidget *sceneView, scene->getRegisteredSceneViews())
        {
            sceneView->staticLabels.append(pickText);
            sceneView->updateSceneLabel();
        }
    }

    else if (property == cameraAutoRotationModeProperty)
    {
        cameraAutorotationMode = MSystemManagerAndControl::getInstance()
                ->getBoolPropertyManager()->value(cameraAutoRotationModeProperty);

        if (!cameraAutorotationMode) cameraAutoRotationTimer->stop();

        updateSceneLabel();
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == cameraSetNorthUpProperty)
    {
        executeCameraAction(CAMERA_NORTHUP, false);
    }

    else if (property == cameraSetUprightProperty)
    {
        executeCameraAction(CAMERA_UPRIGHT, false);
    }

    else if (property == cameraSetTopViewProperty)
    {
        executeCameraAction(CAMERA_TOPVIEW, false);
    }

    else if (property == cameraSaveToFileProperty)
    {
        executeCameraAction(CAMERA_SAVETOFILE, false);
    }

    else if (property == cameraLoadFromFileProperty)
    {
        executeCameraAction(CAMERA_LOADFROMFILE, false);
    }

    else if (property == sceneNavigationSensitivityProperty)
    {
        sceneNavigationSensitivity = MSystemManagerAndControl::getInstance()
                ->getDecoratedDoublePropertyManager()->value(sceneNavigationSensitivityProperty);
    }

    else if (property == posLabelEnableProperty)
    {
        posLabelIsEnabled = MSystemManagerAndControl::getInstance()
                ->getBoolPropertyManager()->value(posLabelEnableProperty);
    }

#ifndef CONTINUOUS_GL_UPDATE
    else if (property == measureFPSProperty)
    {
        LOG4CPLUS_DEBUG(mlog, "measuring FPS for 30 seconds...");
        // Enable FPS measurement.
        measureFPS = true;
        measureFPSFrameCount = 0;
        // Record measurements starting at the front of "fpsTimeseries".
        fpsTimeseriesIndex = 0;
        QTimer::singleShot(30000, this, SLOT(stopFPSMeasurement()));
        updateGL();
    }
#endif

    else if (property == northArrow.enabledProperty)
    {
        northArrow.enabled = MSystemManagerAndControl::getInstance()
                ->getBoolPropertyManager()->value(northArrow.enabledProperty);
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == northArrow.horizontalScaleProperty)
    {
        northArrow.horizontalScale = MSystemManagerAndControl::getInstance()
                ->getDecoratedDoublePropertyManager()
                ->value(northArrow.horizontalScaleProperty);
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == northArrow.verticalScaleProperty)
    {
        northArrow.verticalScale = MSystemManagerAndControl::getInstance()
                ->getDecoratedDoublePropertyManager()
                ->value(northArrow.verticalScaleProperty);
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == northArrow.lonPositionProperty)
    {
        northArrow.lon = MSystemManagerAndControl::getInstance()
                ->getDecoratedDoublePropertyManager()
                ->value(northArrow.lonPositionProperty);
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == northArrow.latPositionProperty)
    {
        northArrow.lat = MSystemManagerAndControl::getInstance()
                ->getDecoratedDoublePropertyManager()
                ->value(northArrow.latPositionProperty);
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == northArrow.worldZPositionProperty)
    {
        northArrow.worldZ = MSystemManagerAndControl::getInstance()
                ->getDecoratedDoublePropertyManager()
                ->value(northArrow.worldZPositionProperty);
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }

    else if (property == northArrow.colourProperty)
    {
        northArrow.colour = MSystemManagerAndControl::getInstance()
                ->getColorPropertyManager()->value(northArrow.colourProperty);
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
    }
}


//void MSceneViewGLWidget::updateDisplayTime()
//{
//    MTextManager* tm = MGLResourcesManager::getInstance()->getTextManager();

//    for (int i = 0; i < staticLabels.size(); i++)
//        if (staticLabels[i] == sceneTimeLabel) {
//            staticLabels.removeAt(i);
//            tm->removeText(sceneTimeLabel);
//            break;
//        }

//    QDateTime validTime = scene->validDateTimeEdit()->dateTime();
//    QDateTime initTime  = scene->initDateTimeEdit()->dateTime();

//    QString str = QString("Valid: %1 (step %2 hrs from %3)")
//            .arg(validTime.toString("ddd yyyy-MM-dd hh:mm UTC"))
//            .arg(int(initTime.secsTo(validTime) / 3600.))
//            .arg(initTime.toString("ddd yyyy-MM-dd hh:mm UTC"));

//    sceneTimeLabel = tm->addText(
//                str,
//                MTextManager::CLIPSPACE, -0.99, 0.99, -0.99, 24,
//                QColor(0, 0, 0),
//                MTextManager::UPPERLEFT,
//                true, QColor(255, 255, 255, 200));
//    staticLabels.append(sceneTimeLabel);
//}


void MSceneViewGLWidget::updateFPSTimer()
{
    // Perform a stopwatch split next frame.
    splitNextFrame = true;
}


void MSceneViewGLWidget::stopFPSMeasurement()
{
    measureFPS = false;

    float avgRenderTimeMeasurementPeriod = 30000. / float(measureFPSFrameCount);
    float avgFPSMeasurementPeriod = float(measureFPSFrameCount) / 30.;

    LOG4CPLUS_DEBUG(mlog, "fps measurement is stopped"
                    << "; number of frames in 30s: " << measureFPSFrameCount
                    << "; average render time over 30s: "
                    << avgRenderTimeMeasurementPeriod << " ms ("
                    << avgFPSMeasurementPeriod << " fps)");
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MSceneViewGLWidget::initializeGL()
{
    LOG4CPLUS_DEBUG(mlog, "initialising OpenGL context of scene view " << myID);
    LOG4CPLUS_DEBUG(mlog, "\tOpenGL context is "
                    << (context()->isValid() ? "" : "NOT ") << "valid.");
    LOG4CPLUS_DEBUG(mlog, "\tOpenGL context is "
                    << (context()->isSharing() ? "" : "NOT ") << "sharing.");

    // Create the widget's only shader: To draw the focus rectangle.
    QGLShader *vshader = new QGLShader(QGLShader::Vertex, this);
    const char *vsrc =
        "#version 130\n"
        "in vec2 vertex;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = vec4(vertex.xy, -1, 1);\n"
        "}\n";
    vshader->compileSourceCode(vsrc);

    QGLShader *fshader = new QGLShader(QGLShader::Fragment, this);
    const char *fsrc =
        "#version 130\n"
        "uniform vec4 colourValue;\n"
        "out vec4 fragColour;\n"
        "void main(void)\n"
        "{\n"
        "    fragColour = colourValue;\n"
        "}\n";
    fshader->compileSourceCode(fsrc);

    focusShader = new QGLShaderProgram(this);
    focusShader->addShader(vshader);
    focusShader->addShader(fshader);
#define FOCUSSHADER_VERTEX_ATTRIBUTE 0
    focusShader->bindAttributeLocation("vertex", FOCUSSHADER_VERTEX_ATTRIBUTE);
    focusShader->link();

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    bool loadShaders = false;
    loadShaders |= glRM->generateEffectProgram("north_arrow", northArrowShader);
    if (loadShaders)
    {
        northArrowShader->compileFromFile_Met3DHome(
                    "src/glsl/north_arrow.fx.glsl");
    }

    // Initial OpenGL settings.
    glEnable(GL_DEPTH_TEST);
    if (multisamplingEnabled)
        glEnable(GL_MULTISAMPLE);
    else
        glDisable(GL_MULTISAMPLE);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    // Initialise the not shared OpenGL resources of the scene's actors.
    if (scene) {
        LOG4CPLUS_DEBUG(mlog, "initialising not shared OpenGL resources of "
                        << "the scene's actors..");
        const QVector<MActor*>& renderQueue = scene->getRenderQueue();
        for (int i = 0; i < renderQueue.size(); i++)
            renderQueue[i]->initializePerGLContextResources(this);
    }

    // Add static scene labels.
    MTextManager* tm = MGLResourcesManager::getInstance()->getTextManager();
//    staticLabels.append(tm->addText(
//                            "Met.3D",
//                            MTextManager::CLIPSPACE, 0.99, -0.99, -0.99, 32, QColor(255, 0, 0),
//                            MTextManager::BASELINERIGHT)
//                        );
    // Create a new scene description label (view number and scene name in
    // lower left corner of the view).
    sceneNameLabel = tm->addText(
                QString("view %1 (%2)").arg(myID+1).arg(scene->getName()),
                MTextManager::CLIPSPACE, -0.99, -0.99, -0.99, 20,
                QColor(0, 0, 255, 150));

    staticLabels.append(sceneNameLabel);

    // Show scene time.
//    updateDisplayTime();
    updateCameraPositionDisplay();

    viewIsInitialised = true;
    LOG4CPLUS_DEBUG(mlog, "initialisation done\n");
}


void MSceneViewGLWidget::updateGL()
{
    // Don't update GL if no scene is attached to the scene view.
    if (scene != nullptr)
    {
        QGLWidget::updateGL();
    }
}


void MSceneViewGLWidget::paintGL()
{
    // Only render this widget if it is visible.
    if (!isVisible()) return;
    if (freezeMode) return;

    if (multisamplingEnabled)
    {
        glEnable(GL_MULTISAMPLE);
    }
    else
    {
        glDisable(GL_MULTISAMPLE);
    }

    if (antialiasingEnabled)
    {
        glEnable(GL_POINT_SMOOTH);
        glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    }
    else
    {
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_POINT_SMOOTH);
        glDisable(GL_POLYGON_SMOOTH);
    }

    qglClearColor(backgroundColour);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Status information: The "main" scene view instance measures frame rate.
    if (myID == 0)
    {
        if (splitNextFrame)
        {
            fpsStopwatch->split();

            double frameTime = fpsStopwatch->getLastSplitTime(MStopwatch::SECONDS);
            QString fpsString = QString("%1 ms (%2 fps)")
                    .arg(frameTime/frameCount*1000., 0, 'f', 1)
                    .arg(frameCount/frameTime, 0, 'f', 1);

            MSystemManagerAndControl::getInstance()->renderTimeLabel()->setText(fpsString);
            if (measureFPS) LOG4CPLUS_DEBUG(mlog, fpsString.toStdString());

            fpsTimeseries[fpsTimeseriesIndex++] = frameCount/frameTime;
            if (fpsTimeseriesIndex == fpsTimeseriesSize) fpsTimeseriesIndex = 0;

            frameCount = 0;
            splitNextFrame = false;
        }

        frameCount++;
    }


    // In ROTATE_SCENE mode, rotate the camera around the current scene centre.
    // Compute the new camera position.
    if (sceneNavigationMode == ROTATE_SCENE)
    {
        // Compute worldZ coordinate from sceneRotationCentre pressure
        // coordinate.
        double z = worldZfromPressure(sceneRotationCentre.z());

        // Compute a matrix to translate the camera position in world space to
        // the sceneRotationCentre.
        QMatrix4x4 updateCameraMatrix;
        updateCameraMatrix.setToIdentity();
        updateCameraMatrix.translate(
                    sceneRotationCentre.x(), sceneRotationCentre.y(), z);

        // Rotate the camera around the current centre by the rotation defined
        // by the sceneRotationMatrix.
        updateCameraMatrix *= sceneRotationMatrix;

        // Translate the camera position back to its origin.
        updateCameraMatrix.translate(
                    -sceneRotationCentre.x(), -sceneRotationCentre.y(), -z);

        updateCameraMatrix = updateCameraMatrix.inverted();
        sceneRotationMatrix = sceneRotationMatrix.inverted();

        // Update camera position.
        camera.setOrigin(updateCameraMatrix * camera.getOrigin());
        camera.setYAxis(sceneRotationMatrix * camera.getYAxis());
        camera.setZAxis(sceneRotationMatrix * camera.getZAxis());

        // Reset current rotation.
        sceneRotationMatrix.setToIdentity();
    }

    // Compute model-view-projection matrix.
    // Specify a perspective projection. NOTE: Near and far clipping planes are
    // positive (not zero or negative) values that represent distances in front
    // of the eye. See
    // http://www.opengl.org/resources/faq/technical/depthbuffer.htm
    QVector3D co = camera.getOrigin();
    modelViewProjectionMatrix.setToIdentity();
    const double ratio = static_cast<double>(viewPortWidth) / viewPortHeight;

    if (sceneNavigationMode == TOPVIEW_2D)
    {
        const float zBack = co.z();
        float dyHalf = std::tan(M_PI / 8.0) * zBack;
        float dxHalf = ratio * dyHalf;

        float minX = - dxHalf;
        float maxX = + dxHalf;
        float minY = - dyHalf;
        float maxY = + dyHalf;

        modelViewProjectionMatrix.ortho(minX, maxX, minY, maxY, 0., 500.);
    }
    else
    {
        modelViewProjectionMatrix.perspective(45., ratio, abs(co.z())/10., 500.);
    }

    modelViewProjectionMatrix *= camera.getViewMatrix();

    QList<MLabel*> labelList;
    labelList.append(staticLabels);

    if (scene)
    {
        bool _interactionMode = actorInteractionMode;
        // Get a reference to the scene's render queue and render the actors.
        // Collect the actor's labels -- they are rendered in the next step.
        foreach (MActor* actor, scene->getRenderQueue())
        {
            if (singleInteractionActor != nullptr &&
                    singleInteractionActor->getName() == actor->getName())
            {
                actorInteractionMode = true;
            }

            actor->render(this);
            if (actor == pickedActor.actor)
            {
                labelList.append(actor->getPositionLabelToRender());
            }
            labelList.append(actor->getLabelsToRender());

            if (singleInteractionActor != nullptr &&
                    singleInteractionActor->getName() == actor->getName())
            {
                actorInteractionMode = false;
            }
        }

        actorInteractionMode = _interactionMode;

        // Render text labels.
        if (!renderLabelsWithDepthTest) glDisable(GL_DEPTH_TEST);
        MGLResourcesManager::getInstance()->getTextManager()
                ->renderLabelList(this, labelList);
        if (!renderLabelsWithDepthTest) glEnable(GL_DEPTH_TEST);
    }

    // Draw focus rectangle.
    if (hasFocus())
    {
        float rect[8] = {-1, -1, 1, -1, 1, 1, -1, 1};
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        focusShader->bind();
        focusShader->enableAttributeArray(FOCUSSHADER_VERTEX_ATTRIBUTE);
        focusShader->setAttributeArray(FOCUSSHADER_VERTEX_ATTRIBUTE, rect, 2);
        focusShader->setUniformValue("colourValue", QColor(Qt::red));
        glLineWidth(2);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    if (northArrow.enabled)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        northArrowShader->bind();
        northArrowShader->setUniformValue("colour", northArrow.colour);
        northArrowShader->setUniformValue("lightDirection", getLightDirection());
        northArrowShader->setUniformValue("horizontalScale",
                                          GLfloat(northArrow.horizontalScale));
        northArrowShader->setUniformValue("verticalScale",
                                          GLfloat(northArrow.verticalScale));
        northArrowShader->setUniformValue("lon", GLfloat(northArrow.lon));
        northArrowShader->setUniformValue("lat", GLfloat(northArrow.lat));
        northArrowShader->setUniformValue("worldZ", GLfloat(northArrow.worldZ));
        northArrowShader->setUniformValue("rotationMatrix", sceneRotationMatrix);
        northArrowShader->setUniformValue("mvpMatrix",
                                          modelViewProjectionMatrix);
        glDrawArrays(GL_POINTS, 0, 1);
    }

    // All actors have been rendered; they won't query this variable until the
    // next render cylce.
    visualizationParameterChange = false;

    // This update is triggered by a click on "measureFPSProperty". The scene
    // is updated for 30 seconds for fps measurements.
    if (measureFPS)
    {
        measureFPSFrameCount++;
#ifndef CONTINUOUS_GL_UPDATE
        update();
#endif
    }

#ifdef CONTINUOUS_GL_UPDATE
    // For framerate measurements, graphics rendering is performed in a
    // continous loop. Similar to glutPostRedisplay(), update() sets a flag
    // that tells Qt's main loop to redraw this widget in the next loop cycle.
    // See http://lists.trolltech.com/qt-interest/2005-03/thread00136-0.html
    update();
#endif

#ifdef LOG_EVENT_TIMES
    LOG4CPLUS_DEBUG(mlog, "scene rendering completed at "
                    << MSystemManagerAndControl::getInstance()
                    ->elapsedTimeSinceSystemStart(MStopwatch::MILLISECONDS)
                    << " ms");
#endif
}


void MSceneViewGLWidget::resizeGL(int width, int height)
{
    viewPortWidth = width;
    viewPortHeight = height;
    glViewport(0, 0, width, height);

    // viewport was resized, set timer and variable
    resizeTimer.restart();
    viewportResized = true;
    QVector3D co = camera.getOrigin();
    modelViewProjectionMatrix.setToIdentity();
    const double ratio = static_cast<double>(viewPortWidth) / viewPortHeight;

    if (sceneNavigationMode == TOPVIEW_2D)
    {
        const float zBack = co.z();
        float dyHalf = std::tan(M_PI / 8.0) * zBack;
        float dxHalf = ratio * dyHalf;

        float minX = - dxHalf;
        float maxX = + dxHalf;
        float minY = - dyHalf;
        float maxY = + dyHalf;

        modelViewProjectionMatrix.ortho(minX, maxX, minY, maxY, 0., 500.);
    }
    else
    {
        modelViewProjectionMatrix.perspective(
                45.,
                ratio,
                co.z()/10.,
                500.);
    }

    modelViewProjectionMatrix *= camera.getViewMatrix();
}


bool MSceneViewGLWidget::isViewPortResized()
{
    float elapsedTime = resizeTimer.elapsed() / 1000.0f;

    if(elapsedTime > 0.1f)
    {
        viewportResized = false;
    }

    return viewportResized;
}


void MSceneViewGLWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    // Toggle interaction mode.
    setInteractionMode(!actorInteractionMode);

    if (posLabelIsEnabled && actorInteractionMode
            && pickedActor.actor != nullptr
            && (event->buttons() & Qt::LeftButton))
    {
        float clipX = -1. + 2.*(float(event->x()) / float(viewPortWidth));
        float clipY =  1. - 2.*(float(event->y()) / float(viewPortHeight));
        pickedActor.actor->addPositionLabel(this, pickedActor.handleID,
                                            clipX, clipY);
    }
}


void MSceneViewGLWidget::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();
    float clipX = -1. + 2.*(float(event->x()) / float(viewPortWidth));
    float clipY =  1. - 2.*(float(event->y()) / float(viewPortHeight));
    lastPoint = QVector3D(clipX, clipY, 0);
    float length = sqrt(lastPoint.x() * lastPoint.x()
                        + lastPoint.y() * lastPoint.y());
    length = (length < 1.0) ? length : 1.0;
    lastPoint.setZ(cos((M_PI/2.0) * length));
    userIsInteracting = true;

    if (posLabelIsEnabled && actorInteractionMode
            && pickedActor.actor != nullptr
            && (event->buttons() & Qt::LeftButton))
    {
        pickedActor.actor->addPositionLabel(this, pickedActor.handleID,
                                            clipX, clipY);
    }
}


void MSceneViewGLWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (freezeMode) return;

    // A) INTERACTION MODE.
    // ========================================================================

    // NOTE: If the interaction mode is enabled, Qt mouse tracking is enabled
    // (in propertyChange()). Hence, mouseMoveEvent() is called always when the
    // mouse is moved on the OpenGL canvas. If mouse tracking is disabled,
    // mouseMoveEvent() is only called when a button has been pressed.

    if (actorInteractionMode)
    {
        // No scene registered? Return.
        if (!scene) return;

        // Transform the mouse cursor position to 2D clip space.
        float clipX = -1. + 2.*(float(event->x()) / float(viewPortWidth));
        float clipY =  1. - 2.*(float(event->y()) / float(viewPortHeight));

        // The left mouse button is pressed: This is a drag event.
        if (event->buttons() & Qt::LeftButton)
        {
            // No actor has been picked to be dragged: Return.
            if (!pickedActor.actor) return;

            pickedActor.actor->dragEvent(
                        this, pickedActor.handleID, clipX, clipY);
        }
        // No mouse button is pressed. Track the mouse to find pickable
        // elements.
        else
        {
            // Reset the currently picked actor.
            pickedActor.actor = 0;
            pickedActor.handleID = -1;

            // Loop over all actors in the scene and let them check whether
            // the mouse cursor coincides with one of their handles.
            foreach (MActor* actor, scene->getRenderQueue())
            {
                // Only check actors that are pickable.
                if (actor->isPickable())
                {
                    if (singleInteractionActor == nullptr ||
                           singleInteractionActor->getName() == actor->getName())
                    {
                        pickedActor.handleID = actor->checkIntersectionWithHandle(
                                    this, clipX, clipY, 0.5f);
                        // If the test returned a valid handle ID, store the actor
                        // and its handle as the currently picked actor.
                        if (pickedActor.handleID >= 0)
                        {
                            pickedActor.actor = actor;
                            break;
                        }
                    }
                }
            }

            // Redraw (the actors might draw any highlighted handles).
#ifndef CONTINUOUS_GL_UPDATE
            updateGL();
#endif
        }

        return;
    }


    // B) ANALYSIS MODE.
    // ========================================================================
    if (analysisMode) return;


    // C) CAMERA MOVEMENTS.
    // ========================================================================

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    int dx = event->x() - lastPos.x();
    int dy = event->y() - lastPos.y();

    float sensitivity = sceneNavigationSensitivity;
    if (event->modifiers() == Qt::ShiftModifier) sensitivity *= 10.;

    if (event->buttons() & glRM->globalMouseButtonRotate)
    {
        if (sceneNavigationMode == MOVE_CAMERA)
        {
            // The left mouse button rotates the camera, the camera position
            // remains unchanged.
            // dx maps to a rotation around the world space z-axis
            camera.rotateWorldSpace(-dx/10./sensitivity, 0, 0, 1);
            // dy maps to a rotation around the camera x-axis
            camera.rotate(-dy/10./sensitivity, 1, 0, 0);
        }
        else if (sceneNavigationMode == ROTATE_SCENE)
        {
            sceneRotationMatrix.setToIdentity();
            // Rotation is implemented with a rotation matrix and is combined
            // with a translation in "paintGL()". The calculation uses an
            // arcball rotation; see:
            // https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Arcball
            float clipX = -1. + 2.*(float(event->x()) / float(viewPortWidth));
            float clipY =  1. - 2.*(float(event->y()) / float(viewPortHeight));
            QVector3D curPoint = QVector3D(clipX, clipY,0);
            float length = sqrt(curPoint.x() * curPoint.x() +
                                curPoint.y() * curPoint.y());
            length = (length < 1.0) ? length : 1.0;
            curPoint.setZ(cos((M_PI/2.0) * length));
            QVector3D difPosition = (lastPoint - curPoint);
            float angle = difPosition.length() * 45.0;

            // The rotation vector is also rotated by the worldMatrix
            // to perform the rotation regarding the actual world rotation.
            QVector3D rotAxis = QVector3D::crossProduct(lastPoint, curPoint);
            sceneRotationMatrix.rotate(angle,rotAxis);
            lastPoint = curPoint;

            if (cameraAutorotationMode)
            {
                cameraAutoRotationAxis = rotAxis;
                cameraAutoRotationAngle = angle / 10.;
            }
        }
    }

    else if (event->buttons() & glRM->globalMouseButtonPan)
    {
        // The right mouse button moves the camera around in the scene.

        if (glRM->isReverseCameraPan)
        {
            // Move camera with respect to the mouse movement.
            camera.moveUp(dy/10./sensitivity, 1.);
            camera.moveRight(-dx/10./sensitivity);
        }
        else
        {
            // Move scene with respect to the mouse movement.
            camera.moveUp(-dy/10./sensitivity, 1.);
            camera.moveRight(dx/10./sensitivity);
        }
    }

    else if (event->buttons() & glRM->globalMouseButtonZoom)
    {
        // "Pure" mouse wheel: zoom (move camera forward/backward)
        float zoomFactor = -1./sensitivity;

        if (sceneNavigationMode == ROTATE_SCENE) zoomFactor = -zoomFactor;
        if (glRM->isReverseCameraZoom) zoomFactor = -zoomFactor;

        camera.moveForward(dy * zoomFactor);
    }

    lastPos = event->pos();

    updateCameraPositionDisplay();
    updateSynchronizedCameras();

#ifndef CONTINUOUS_GL_UPDATE
    updateGL();
#endif
}


void MSceneViewGLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (freezeMode) return;

    if (actorInteractionMode && pickedActor.actor != nullptr)
    {
        pickedActor.actor->removePositionLabel();
    }

    userIsInteracting = false;
    emit clicked();

    // ANALYSIS MODE.
    // ==============

    if (analysisMode)
    {
        // No scene registered? Return.
        if (!scene) return;

        // Transform the mouse cursor position to 2D clip space.
        float clipX = -1. + 2.*(float(event->x()) / float(viewPortWidth));
        float clipY =  1. - 2.*(float(event->y()) / float(viewPortHeight));

        // The left mouse button is released: Trigger analysis.
        if (event->buttons() ^ Qt::LeftButton)
        {
            // Loop over all actors in the scene and let them check whether
            // the mouse cursor coincides with one of their objects.
            const QVector<MActor*>& actors = scene->getRenderQueue();
            for (int i = 0; i < actors.size(); i++)
                // Only check actors that are pickable.
                if (actors[i]->isPickable())
                    if (actors[i]->triggerAnalysisOfObjectAtPos(
                                this, clipX, clipY, 10. / float(viewPortWidth)))
                        break;
        }
    }

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // AUTO-ROTATION MODE.
    // ===================

    // If we are in auto-rotation mode, start the auto-rotation timer to
    // rotate the scene around its rotation centre.
    if (event->button() == glRM->globalMouseButtonRotate &&
        cameraAutorotationMode && sceneNavigationMode == ROTATE_SCENE)
    {
      cameraAutoRotationTimer->start();
      userIsInteracting = true;
    }

#ifndef CONTINUOUS_GL_UPDATE
    updateGL();
#endif
}


void MSceneViewGLWidget::wheelEvent(QWheelEvent *event)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    if (actorInteractionMode || analysisMode) return;
    if (freezeMode) return;
    if (glRM->globalMouseButtonZoom != Qt::MiddleButton) return;
    if (event->modifiers() == Qt::ControlModifier)
    {
        // Ctrl + mouse wheel: -- none --
    }
    else if (event->modifiers() == Qt::AltModifier)
    {
//        // Alt + mouse wheel: time navigation for scene.
//        if (event->delta() > 0) {
//            scene->timeForward();
//        }
//        else
//        {
//            scene->timeBackward();
//        }
    }
    else
    {
        // starts scroll timer and sets scrolling to true
        userIsScrolling = true;
        scrollTimer.restart();

        // "Pure" mouse wheel: zoom (move camera forward/backward)
        float zoomFactor = 10./sceneNavigationSensitivity;
        if (event->modifiers() == Qt::ShiftModifier) zoomFactor /= 10.;

        if (sceneNavigationMode == ROTATE_SCENE) zoomFactor = -zoomFactor;
        if (glRM->isReverseCameraZoom) zoomFactor = -zoomFactor;

        if (event->delta() > 0)
        {
            camera.moveForward(0.5 * zoomFactor);
        }
        else
        {
            camera.moveForward(-0.5 * zoomFactor);
        }
        updateCameraPositionDisplay();
        updateSynchronizedCameras();
    }
#ifndef CONTINUOUS_GL_UPDATE
    updateGL();
#endif
}


void MSceneViewGLWidget::checkUserScrolling()
{
    float elapsedTime = scrollTimer.elapsed() / 1000.0f;

    bool oldUserScrolling = userIsScrolling;

    if (elapsedTime > 0.5f) { userIsScrolling = false; }

    if (oldUserScrolling != userIsScrolling) { updateGL(); }
}


void MSceneViewGLWidget::autoRotateCamera()
{
    sceneRotationMatrix.rotate(cameraAutoRotationAngle, cameraAutoRotationAxis);
#ifndef CONTINUOUS_GL_UPDATE
    updateGL();
#endif
}


bool MSceneViewGLWidget::userIsScrollingWithMouse()
{
    return userIsScrolling;
}


void MSceneViewGLWidget::keyPressEvent(QKeyEvent *event)
{
    // Special case: The user interactively selects a new scene rotation
    // centre (see "selectSceneRotationCentreProperty"). The selection is
    // finished upon hitting the "ENTER" key.
    if (singleInteractionActor != nullptr)
    {
        if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        {
            MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
            MMovablePoleActor *pickActor =
                    glRM->getSceneRotationCentreSelectionPoleActor();
            MLabel *pickText = glRM->getSceneRotationCentreSelectionLabel();
            QVector3D rotationCenter = pickActor->getPoleVertices().at(0);

            sceneRotationCentre.setX(rotationCenter.x());
            sceneRotationCentre.setY(rotationCenter.y());

            setSceneRotationCentre(sceneRotationCentre);

            scene->setSingleInteractionActor(nullptr);
            scene->removeActorByName(pickActor->getName());
            foreach (MSceneViewGLWidget *sceneView, scene->getRegisteredSceneViews())
            {
                sceneView->staticLabels.removeOne(pickText);
                sceneView->updateSceneLabel();
            }

            sceneNavigationMode = ROTATE_SCENE;
            MSystemManagerAndControl::getInstance()->getEnumPropertyManager()->setValue(
                        sceneNavigationModeProperty, sceneNavigationMode);
            enablePropertyEvents = false;
            selectSceneRotationCentreProperty->setEnabled(true);
            enablePropertyEvents = true;
        }
    }

    if (freezeMode) return;

    switch (event->key())
    {
    case Qt::Key_L:
        // Shader reload.
        MGLResourcesManager::getInstance()->reloadActorShaders();
#ifndef CONTINUOUS_GL_UPDATE
        updateGL();
#endif
        break;
    case Qt::Key_I:
        // Toggle interaction mode.
        setInteractionMode(!actorInteractionMode);
        break;
    case Qt::Key_A:
        // Toggle analysis mode.
        setAnalysisMode(!analysisMode);
        break;
    case Qt::Key_R:
        if (event->modifiers() & Qt::AltModifier)
        {
            resizeView();
            break;
        }
        // Toggle auto-rotation mode.
        setAutoRotationMode(!cameraAutorotationMode);
        break;
    case Qt::Key_N:
        if (event->modifiers() & Qt::AltModifier)
        {
            executeCameraAction(CAMERA_NORTHUP);
        }
        break;
    case Qt::Key_U:
        if (event->modifiers() & Qt::AltModifier)
        {
            executeCameraAction(CAMERA_UPRIGHT);
        }
        break;
    case Qt::Key_T:
        if (event->modifiers() & Qt::AltModifier)
        {
            executeCameraAction(CAMERA_TOPVIEW);
        }
        break;
    case Qt::Key_S:
        if (QApplication::keyboardModifiers() == Qt::NoModifier)
        {
            saveScreenshot();
        }
        break;
    default:
        // If we do not act upon the key, pass event to base class
        // implementation.
        QGLWidget::keyPressEvent(event);
    }
}


void MSceneViewGLWidget::updateSynchronizedCameras()
{
    // Synchronize cameras of connected scene views.
    QSetIterator<MSceneViewGLWidget*> i(syncCameras);
    while (i.hasNext())
    {
        MSceneViewGLWidget* otherView = i.next();
        MCamera *otherCamera = otherView->getCamera();
        otherCamera->setOrigin(camera.getOrigin());
        otherCamera->setYAxis(camera.getYAxis());
        otherCamera->setZAxis(camera.getZAxis());
        otherView->updateCameraPositionDisplay();
        otherView->updateGL();
    }
}


void MSceneViewGLWidget::updateSceneLabel()
{
    // Don't update scene label if no scene is attached to the scene view.
    if (scene == nullptr)
    {
        return;
    }
    // Remove the old scene description label from the list of static labels.
    for (int i = 0; i < staticLabels.size(); i++)
        if (staticLabels[i] == sceneNameLabel)
        {
            staticLabels.removeAt(i);
            MGLResourcesManager::getInstance()->getTextManager()->removeText(sceneNameLabel);
        }

    // Create a new scene description label (view number and scene name in
    // lower left corner of the view).
    QString label = QString("view %1 (%2)").arg(myID+1).arg(scene->getName());
    if (actorInteractionMode) label += " - actor interaction mode";
    if (analysisMode) label += " - analysis mode";
    if (cameraAutorotationMode) label += " - auto-rotate camera";

    sceneNameLabel = MGLResourcesManager::getInstance()->getTextManager()->addText(
                label, MTextManager::CLIPSPACE, -0.99, -0.99, -0.99, 20,
                QColor(0, 0, 255, 150));

    staticLabels.append(sceneNameLabel);
}


void MSceneViewGLWidget::saveTimeAnimationImage(QString path, QString filename)
{
    filename = QDir(path).absoluteFilePath(filename);
    QFileInfo checkFile(filename);
    if (!overwriteImageSerie && checkFile.exists())
    {
// TODO (bt, 17Oct2016) Use operating system dependend filename splitting.
        QMessageBox::StandardButton reply = QMessageBox::question(
                    MGLResourcesManager::getInstance(),
                    "Save screenshot",
                    filename.split("/").last()
                    + " already exits.\n"
                    + "Do you want to replace it?",
                    QMessageBox::Yes|QMessageBox::YesAll|QMessageBox::No,
                    QMessageBox::No);
        // Don't save image if user rejects to overwrite the file it.
        if (reply == QMessageBox::No)
        {
            return;
        }
        if (reply == QMessageBox::YesAll)
        {
            overwriteImageSerie = true;
        }
    }
    saveScreenshotToFileName(filename);
}


void MSceneViewGLWidget::updateCameraPositionDisplay()
{
    QVector3D co = camera.getOrigin();
    MSystemManagerAndControl::getInstance()->getStringPropertyManager()->setValue(
                cameraPositionProperty,
                QString("%1/%2/%3").arg(co.x(), 0, 'f', 1)
                .arg(co.y(), 0, 'f', 1).arg(co.z(), 0, 'f', 1));
}


void MSceneViewGLWidget::setSingleInteractionActor(MActor *actor)
{
    singleInteractionActor = actor;
    if (singleInteractionActor != nullptr)
    {
        setInteractionMode(true);
    }
    else
    {
        setInteractionMode(false);
    }
}


void MSceneViewGLWidget::setSceneNavigationMode(SceneNavigationMode mode)
{
    sceneNavigationMode = mode;
    MSystemManagerAndControl::getInstance()->getEnumPropertyManager()->setValue(
                sceneNavigationModeProperty, (int) mode);
}


void MSceneViewGLWidget::setSceneRotationCentre(QVector3D centre)
{
    sceneRotationCentre = centre;
    QtExtensions::QtDecoratedDoublePropertyManager *doublePropertyManager =
            MSystemManagerAndControl::getInstance()->getDecoratedDoublePropertyManager();
    enablePropertyEvents = false;
    doublePropertyManager->setValue(
                sceneRotationCentreLonProperty, centre.x());
    doublePropertyManager->setValue(
                sceneRotationCentreLatProperty, centre.y());
    doublePropertyManager->setValue(
                sceneRotationCentreElevationProperty, centre.z());
    enablePropertyEvents = true;
}


void MSceneViewGLWidget::saveConfigurationToFile(QString filename)
{
    if (filename.isEmpty())
        filename = QFileDialog::getSaveFileName(
                    MGLResourcesManager::getInstance(),
                    "Save scene view configuration",
                    QString("data/sceneview%1.sceneview.conf").arg(myID + 1),
                    "Scene view configuration files (*.sceneview.conf)");

    if (filename.isEmpty()) return;

    LOG4CPLUS_DEBUG(mlog, "Saving configuration to " << filename.toStdString());

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);

    // Overwrite if the file exists.
    if (QFile::exists(filename))
    {
        QStringList groups = settings->childGroups();
        // Only overwrite file if it contains already configuration for a
        // scene view.
        if ( !groups.contains("MSceneView") )
        {
            QMessageBox msg;
            msg.setWindowTitle("Error");
            msg.setText("The selected file contains a configuration other than "
                        "MSceneView.\n"
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

    // Save scene view configuration.
    settings->beginGroup("MSceneView");
    saveConfiguration(settings);
    settings->endGroup();

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been saved.");
}


void MSceneViewGLWidget::loadConfigurationFromFile(QString filename)
{
    if (filename.isEmpty())
        filename = QFileDialog::getOpenFileName(
                    MGLResourcesManager::getInstance(),
                    "Load scene view configuration",
                    "data/config",
                    "Scene view configuration files (*.sceneview.conf)");

    if (filename.isEmpty()) return;

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);

    QStringList groups = settings->childGroups();
    if ( !groups.contains("MSceneView") )
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("The selected file does not contain configuration data "
                    "for scene views.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        delete settings;
        return;
    }

    LOG4CPLUS_DEBUG(mlog, "Loading configuration from "
                    << filename.toStdString());

    settings->beginGroup("MSceneView");
    loadConfiguration(settings);
    settings->endGroup();

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been loaded.");
}


void MSceneViewGLWidget::saveConfiguration(QSettings *settings)
{
    MSystemManagerAndControl* sysMC = MSystemManagerAndControl::getInstance();

    camera.saveConfiguration(settings);

    // Save interaction properties.
    settings->beginGroup("Interaction");
    settings->setValue("sceneNavigation", sysMC->getEnumPropertyManager()
                      ->enumNames(sceneNavigationModeProperty)
                      .at(sceneNavigationMode));
    settings->setValue("sceneRotationCentreLongitude", sceneRotationCentre.x());
    settings->setValue("sceneRotationCentreLatitude", sceneRotationCentre.y());
    settings->setValue("sceneRotationCentreElevation",
                      sysMC->getDecoratedDoublePropertyManager()->value(
                          sceneRotationCentreElevationProperty));
    settings->setValue("NavigationSensitivity",
                      sysMC->getDecoratedDoublePropertyManager()->value(
                          sceneNavigationSensitivityProperty));
    settings->setValue("autoRotateCamera", cameraAutorotationMode);
    settings->setValue("SyncCameraWithView",
                      sysMC->getEnumPropertyManager()->enumNames(
                          syncCameraWithViewProperty).at(
                          sysMC->getEnumPropertyManager()
                          ->value(syncCameraWithViewProperty)));
    settings->setValue("actorInteractionMode", actorInteractionMode);
    settings->setValue("analysisMode", analysisMode);
    settings->endGroup(); // interaction

    // Save rendering properties.
    settings->beginGroup("Rendering");
    settings->setValue("backgroundColour",  sysMC->getColorPropertyManager()
                       ->value(backgroundColourProperty));
    settings->setValue("multisampling", multisamplingEnabled);
    settings->setValue("antialiasing", antialiasingEnabled);
    settings->setValue("depthTestForLabels", renderLabelsWithDepthTest);
    settings->setValue("lighting",
                      sysMC->getEnumPropertyManager()->enumNames(
                          lightingProperty).at(lightDirection));
    settings->setValue("verticalScaling", ztop);
    settings->endGroup(); // rendering

    // Load arrow pointing north properties.
    settings->beginGroup("ArrowPointingNorth");
    settings->setValue("enabled",  sysMC->getBoolPropertyManager()->value(
                           northArrow.enabledProperty));
    settings->setValue("horizontalScale", northArrow.horizontalScale);
    settings->setValue("verticalScale", northArrow.verticalScale);
    settings->setValue("lon",  sysMC->getDecoratedDoublePropertyManager()
                       ->value(northArrow.lonPositionProperty));
    settings->setValue("lat",  sysMC->getDecoratedDoublePropertyManager()
                       ->value(northArrow.latPositionProperty));
    settings->setValue("worldZPos",  sysMC->getDecoratedDoublePropertyManager()
                       ->value(northArrow.worldZPositionProperty));
    settings->setValue("colour",  sysMC->getColorPropertyManager()
                       ->value(northArrow.colourProperty));
    settings->endGroup(); // arrow pointing north
}


void MSceneViewGLWidget::loadConfiguration(QSettings *settings)
{
    MSystemManagerAndControl* sysMC = MSystemManagerAndControl::getInstance();

    camera.loadConfiguration(settings);

    // Load interaction properties.
    settings->beginGroup("Interaction");
    // Since scene navigation mode is stored as a string, get the corresponding
    // index.
    QString enumName = settings->value("sceneNavigation", "move camera").toString();
    int enumIndex = (sysMC->getEnumPropertyManager()->enumNames(
                sceneNavigationModeProperty)).indexOf(enumName);
    sysMC->getEnumPropertyManager()->setValue(sceneNavigationModeProperty,
                                              enumIndex);

    sysMC->getDecoratedDoublePropertyManager()->setValue(
                sceneRotationCentreLonProperty,
                settings->value("sceneRotationCentreLongitude", 0.).toDouble());
    sysMC->getDecoratedDoublePropertyManager()->setValue(
                sceneRotationCentreLatProperty,
                settings->value("sceneRotationCentreLatitude", 45.).toDouble());
    sysMC->getDecoratedDoublePropertyManager()->setValue(
                sceneRotationCentreElevationProperty,
                settings->value("sceneRotationCentreElevation", 1020.).toDouble());

    sysMC->getDecoratedDoublePropertyManager()->setValue(
                sceneNavigationSensitivityProperty,
                settings->value("NavigationSensitivity", 1.).toDouble());

    sysMC->getBoolPropertyManager()->setValue(
                cameraAutoRotationModeProperty,
                settings->value("autoRotateCamera", false).toBool());

    // Since scene sync camera with view is stored as a string, get the
    // corresponding index.
    enumName = settings->value("SyncCameraWithView", "None").toString();
    enumIndex = (sysMC->getEnumPropertyManager()->enumNames(
                    syncCameraWithViewProperty)).indexOf(enumName);
    sysMC->getEnumPropertyManager()->setValue(syncCameraWithViewProperty,
                                                  enumIndex);

    sysMC->getBoolPropertyManager()->setValue(
                actorInteractionProperty,
                settings->value("actorInteractionMode", false).toBool());
    sysMC->getBoolPropertyManager()->setValue(
                analysisModeProperty,
                settings->value("analysisMode", false).toBool());
    settings->endGroup(); // interaction

    // Load rendering properties.
    settings->beginGroup("Rendering");
    sysMC->getColorPropertyManager()->setValue(
                backgroundColourProperty,
                settings->value("backgroundColour", QColor(255, 255, 255))
                .value<QColor>());
    sysMC->getBoolPropertyManager()->setValue(
                multisamplingProperty,
                settings->value("multisampling", true).toBool());
    sysMC->getBoolPropertyManager()->setValue(
                antialiasingProperty,
                settings->value("antialiasing", false).toBool());
    sysMC->getBoolPropertyManager()->setValue(
                labelDepthTestProperty,
                settings->value("depthTestForLabels", true).toBool());

    // Since scene lighting mode is stored as a string, get the corresponding
    // index.
    enumName = settings->value("lighting", "Top").toString();
    enumIndex = (sysMC->getEnumPropertyManager()->enumNames(
                    lightingProperty)).indexOf(enumName);
    sysMC->getEnumPropertyManager()->setValue(lightingProperty, enumIndex);

    sysMC->getDecoratedDoublePropertyManager()->setValue(
                verticalScalingProperty,
                settings->value("verticalScaling", 36.).toDouble());
    settings->endGroup(); // rendering

    // Load arrow pointing north properties.
    settings->beginGroup("ArrowPointingNorth");
    sysMC->getBoolPropertyManager()->setValue(
                northArrow.enabledProperty,
                settings->value("enabled", false).toBool());
    settings->setValue("horizontalScale", northArrow.horizontalScale);
    settings->setValue("verticalScale", northArrow.verticalScale);
    sysMC->getDecoratedDoublePropertyManager()->setValue(
                northArrow.horizontalScaleProperty,
                settings->value("horizontalScale", 5.).toDouble());
    sysMC->getDecoratedDoublePropertyManager()->setValue(
                northArrow.verticalScaleProperty,
                settings->value("verticalScale", 5.).toDouble());
    sysMC->getDecoratedDoublePropertyManager()->setValue(
                northArrow.lonPositionProperty,
                settings->value("lon", 0.).toDouble());
    sysMC->getDecoratedDoublePropertyManager()->setValue(
                northArrow.latPositionProperty,
                settings->value("lat", 80.).toDouble());
    sysMC->getDecoratedDoublePropertyManager()->setValue(
                northArrow.worldZPositionProperty,
                settings->value("worldZPos", 1.).toDouble());
    sysMC->getColorPropertyManager()->setValue(
                northArrow.colourProperty,
                settings->value("colour", QColor(222, 46, 30)).value<QColor>());
    settings->endGroup(); // arrow pointing north
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MSceneViewGLWidget::saveScreenshot()
{
    // Filter containing all imagefile-extensions Qt is able to write (Oct2016).
    // All extensions are: .png .jpg .jpeg .bmp .ppm .tiff .xbm .xpm .
    QString filter = QString("png (*.png);;jpg (*.jpg);;jpeg (*.jpeg);;")
            + QString("bmp (*.bmp);;ppm (*.ppm);;tiff (*.tiff);;")
            + QString("xbm (*.xbm);;xpm (*.xpm)");
    // Filterlist contains all imagefile-extensions as a regular expression.
    QRegExp filterlist(".*\\.((png)|(jpe?g)|(bmp)|(ppm)|(tiff)|(x(b|p)m))");

    QString filetype;
    // Variable filename stores filename and path but for some operating systems
    // not the selected extension.
    QString filename = QFileDialog::getSaveFileName(
                MGLResourcesManager::getInstance(),
                "Save screenshot",
                "../screenshots",
                filter,
                &filetype);
    if (!filename.isEmpty())
    {
        // Check if filename doesn't end with an image file extension Qt can
        // write.
        // Repeat the test since the user could have changed the name in the
        // second FileDialog.
        while (!filterlist.exactMatch(filename))
        {
            // Extract selection file-extension from selected filter element.
            // Length of extension name can vary thus cant chop fixed number of
            // letters. 5 = number of letters occuring in every scheme besides
            // the file-extension.
            filetype.chop(((filetype.length() - 5) / 2) + 5);
            filetype = QString("." + filetype);
            // Append the selected file extension
            filename += filetype;
// NOTE (bt, 17Oct2016): Can be removed if Qt-bug is fixed.
            // Need to check if file already exists since QFileDialog
            // doesn't provide this functionality under Linux compared to
            // https://bugreports.qt.io/browse/QTBUG-11352 .
            QFileInfo checkFile(filename);
            if (checkFile.exists())
            {
// TODO (bt, 17Oct2016) Use operating system dependend filename splitting.
                QMessageBox::StandardButton reply = QMessageBox::question(
                                        MGLResourcesManager::getInstance(),
                                        "Save screenshot",
                                        filename.split("/").last()
                                        + " already exits.\n"
                                        + "Do you want to replace it?",
                                        QMessageBox::Yes|QMessageBox::No,
                                        QMessageBox::No);
                if (reply == QMessageBox::No)
                {
                    // Reopen FileDialog if file already exists and the user
                    // chooses not to overwrite it or closes the question box.
                    filename = QFileDialog::getSaveFileName(
                                    MGLResourcesManager::getInstance(),
                                    "Save screenshot",
                                    filename,
                                    filter,
                                    &filetype);
                    // Quit if user closes file dialog.
                    if(filename.isEmpty()) return;
                }
            }
        }
        saveScreenshotToFileName(filename);
    }
}


void MSceneViewGLWidget::saveScreenshotToFileName(QString filename)
{
    // Take Screenshot of current scene.
    QImage screenshot = this->grabFrameBuffer();
    // Chop red frame. (Only visible if view has focus.)
    if (this->hasFocus())
    {
        screenshot = screenshot.copy(1, 1, screenshot.width() - 2,
                                     screenshot.height() - 2);
    }

    if (!filename.isEmpty())
    {
        if (screenshot.save(filename))
        {
            QString str = "Saved screenshot of current view to " + filename
                    + "\n";
            LOG4CPLUS_INFO(mlog, str.toStdString());
        }
        else
        {
            QMessageBox::critical(MGLResourcesManager::getInstance(),
                                 "Error",
                                 "Could not save " + filename,
                                 QMessageBox::Ok,
                                 QMessageBox::NoButton);
            QString str = "Could not save " + filename + "\n";
            LOG4CPLUS_ERROR(mlog, str.toStdString());
        }
    }
}


void MSceneViewGLWidget::resizeView()
{
    resizeViewDialog->setWindowTitle("Resize View");
    // Initialise input boxes and ratio with current window size
    resizeViewDialog->setup(this->width(),this->height());

    if (resizeViewDialog->exec() == QDialog::Rejected)
    {
        return;
    }

    int newWidth = resizeViewDialog->getWidth();
    int newHeight = resizeViewDialog->getHeight();

    MMainWindow *mainWindow =
            MSystemManagerAndControl::getInstance()->getMainWindow();
    mainWindow->resizeSceneView(newWidth, newHeight, this);
}

} // namespace Met3D
