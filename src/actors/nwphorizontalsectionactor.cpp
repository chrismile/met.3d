/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2015      Michael Kern
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
#include "nwphorizontalsectionactor.h"

// standard library imports
#include <iostream>
#include <math.h>


// related third party imports
#include <QObject>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/selectdatasourcedialog.h"
#include "gxfw/gl/typedvertexbuffer.h"
#include "data/structuredgrid.h"
#include "actors/spatial1dtransferfunction.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNWPHorizontalSectionActor::MNWPHorizontalSectionActor()
    : MNWPMultiVarActor(),
      MBoundingBoxInterface(this),
      slicePosition_hPa(250.),
      slicePositionGranularity_hPa(5.0),
      slicePosSynchronizationActor(nullptr),
      updateRenderRegion(false),
      vbMouseHandlePoints(nullptr),
      selectedMouseHandle(-1),
      differenceMode(0),
      windBarbsVertexBuffer(nullptr),
      windBarbsSettings(),
      renderShadowQuad(true),
      shadowColor(QColor(60, 60, 60, 70)),
      shadowHeight(0.01f),
      offsetPickPositionToHandleCentre(QVector2D(0., 0.))
{
    enablePicking(true);
    bBoxConnection =
            new MBoundingBoxConnection(this, MBoundingBoxConnection::HORIZONTAL);

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setActorType(staticActorType());
    setName(getActorType());

    slicePosProperty = addProperty(DECORATEDDOUBLE_PROPERTY, "slice position",
                                   actorPropertiesSupGroup);
    properties->setDDouble(slicePosProperty, slicePosition_hPa, 0.01, 1050.,
                           2, slicePositionGranularity_hPa, " hPa");

    slicePosGranularityProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                              "slice position granularity",
                                              actorPropertiesSupGroup);
    properties->setDDouble(slicePosGranularityProperty,
                           slicePositionGranularity_hPa, 0.01, 50., 2, 1., " hPa");

    // Scan currently available actors for further hsec actors. Add hsecs to
    // the combo box of the synchronizeSlicePosWithOtherActorProperty.
    QStringList hsecActorNames;
    hsecActorNames << "None";
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    foreach (MActor *ma, glRM->getActors())
    {
        if (MNWPHorizontalSectionActor *hsec =
                dynamic_cast<MNWPHorizontalSectionActor*>(ma))
        {
            hsecActorNames << hsec->getName();
        }
    }
    synchronizeSlicePosWithOtherActorProperty = addProperty(
                ENUM_PROPERTY, "sync slice position with",
                actorPropertiesSupGroup);
    properties->mEnum()->setEnumNames(
                synchronizeSlicePosWithOtherActorProperty, hsecActorNames);

    QStringList differenceModeNames;
    differenceModeNames << "off" << "absolute" << "relative";
    differenceModeProperty = addProperty(ENUM_PROPERTY, "difference first two variables",
                                         actorPropertiesSupGroup);
    properties->mEnum()->setEnumNames(differenceModeProperty, differenceModeNames);

    // Horizontal bounding box of the actor.
    actorPropertiesSupGroup->addSubProperty(bBoxConnection->getProperty());

    // Wind barbs.
    windBarbsSettings = new WindBarbsSettings(this);
    actorPropertiesSupGroup->addSubProperty(windBarbsSettings->groupProperty);

    // Shadow properties.
    shadowPropGroup = addProperty(GROUP_PROPERTY, "ground shadow",
                                  actorPropertiesSupGroup);

    shadowEnabledProp = addProperty(BOOL_PROPERTY, "enabled", shadowPropGroup);
    properties->mBool()->setValue(shadowEnabledProp, renderShadowQuad);

    shadowColorProp = addProperty(COLOR_PROPERTY, "colour", shadowPropGroup);
    properties->mColor()->setValue(shadowColorProp, shadowColor);

    shadowHeightProp = addProperty(DOUBLE_PROPERTY, "height", shadowPropGroup);
    properties->setDouble(shadowHeightProp, 0.01, 0, 100, 3, 0.01);

    // Keep an instance of GraticuleActor as a "subactor" to draw a graticule
    // on top of the section. The graticule's vertical position and bounding
    // box will be synchronized with the horizontal section.
    graticuleActor = new MGraticuleActor(bBoxConnection);
    graticuleActor->setName("section graticule");
    graticuleActor->setVerticalPosition(slicePosition_hPa);
    actorPropertiesSupGroup->addSubProperty(graticuleActor->getPropertyGroup());

    endInitialiseQtProperties();
}


MNWPHorizontalSectionActor::WindBarbsSettings::WindBarbsSettings(
        MNWPHorizontalSectionActor *hostActor)
    : enabled(false),
      uComponentVarIndex(0),
      vComponentVarIndex(0),
      lineWidth(0.04),
      pennantTilt(9),
      color(QColor(0,0,127)),
      showCalmGlyphs(false),
      deltaBarbsLonLat(1.),
      clampDeltaBarbsToGrid(true),
      automaticScalingEnabled(true),
      oldScale(1),
      reduceFactor(15.0f),
      reduceSlope(0.0175f),
      sensitivity(1.0f)
{

    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProperty = a->addProperty(GROUP_PROPERTY, "wind barbs");

    enabledProperty = a->addProperty(BOOL_PROPERTY, "enabled", groupProperty);
    properties->mBool()->setValue(enabledProperty, enabled);

    uComponentVarProperty = a->addProperty(ENUM_PROPERTY, "longitudinal wind",
                                           groupProperty);
    vComponentVarProperty = a->addProperty(ENUM_PROPERTY, "latitudinal wind",
                                           groupProperty);

    appearanceGroupProperty = a->addProperty(GROUP_PROPERTY, "appearance",
                                             groupProperty);

    lineWidthProperty = a->addProperty(DOUBLE_PROPERTY, "line width",
                                       appearanceGroupProperty);
    properties->setDouble(lineWidthProperty, lineWidth, 0.001, 0.30, 3, 0.001);

    colorProperty = a->addProperty(COLOR_PROPERTY, "line color",
                                   appearanceGroupProperty);
    properties->mColor()->setValue(colorProperty, color);

    showCalmGlyphsProperty = a->addProperty(BOOL_PROPERTY, "show calm glyphs",
                                            appearanceGroupProperty);
    properties->mBool()->setValue(showCalmGlyphsProperty, showCalmGlyphs);

    pennantTiltProperty = a->addProperty(INT_PROPERTY, "pennant tilt",
                                         appearanceGroupProperty);
    properties->setInt(pennantTiltProperty, pennantTilt, 1, 20, 1);

    deltaBarbsLonLatProperty = a->addProperty(
                DOUBLE_PROPERTY, "barb distance (deg)",
                groupProperty);
    properties->setDouble(
                deltaBarbsLonLatProperty, deltaBarbsLonLat, 0.05, 45., 2, 0.1);
    deltaBarbsLonLatProperty->setToolTip(
                "Manually specify the distance between the wind barbs. If the "
                "distance if below grid point spacing, nearest-neighbour "
                "interpolation is used.");

    clampDeltaBarbsToGridProperty = a->addProperty(
                BOOL_PROPERTY, "restrict barb distance to grid",
                groupProperty);
    properties->mBool()->setValue(
                clampDeltaBarbsToGridProperty, clampDeltaBarbsToGrid);
    clampDeltaBarbsToGridProperty->setToolTip(
                "If enabled the manually set barb distance cannot be smaller "
                "than the grid point spacing of the wind field.");

    automaticScalingEnabledProperty = a->addProperty(BOOL_PROPERTY, "automatic scaling",
                                              groupProperty);
    properties->mBool()->setValue(automaticScalingEnabledProperty, automaticScalingEnabled);

    scalingGroupProperty = a->addProperty(GROUP_PROPERTY, "scaling",
                                          groupProperty);

    reduceFactorProperty = a->addProperty(DOUBLE_PROPERTY, "scaling factor",
                                          scalingGroupProperty);
    properties->setDouble(reduceFactorProperty, reduceFactor, 1., 400., 1, 0.1);

    reduceSlopeProperty = a->addProperty(DOUBLE_PROPERTY, "scaling slope",
                                         scalingGroupProperty);
    properties->setDouble(reduceSlopeProperty, reduceSlope, 0.001, 1.0, 4, 0.0001);

    sensitivityProperty = a->addProperty(DOUBLE_PROPERTY, "scaling sensitivity",
                                         scalingGroupProperty);
    properties->setDouble(sensitivityProperty, sensitivity, 1., 200., 1, 1.);
}


MNWPHorizontalSectionActor::~MNWPHorizontalSectionActor()
{
    // "graticuleActor" is deleted by the resourcesManager.

    delete windBarbsSettings;
    if (vbMouseHandlePoints) delete vbMouseHandlePoints;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE 0

void MNWPHorizontalSectionActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);

    beginCompileShaders(7);

    compileShadersFromFileWithProgressDialog(
                glVerticalInterpolationEffect,
                "src/glsl/hsec_verticalinterpolation.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                glFilledContoursShader,
                "src/glsl/hsec_filledcontours.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                glTexturedContoursShader,
                "src/glsl/hsec_texturedcontours.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                glPseudoColourShader,
                "src/glsl/hsec_pseudocolour.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                glMarchingSquaresShader,
                "src/glsl/hsec_marching_squares.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                glWindBarbsShader,
                "src/glsl/hsec_windbarbs.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                glShadowQuad,
                "src/glsl/hsec_shadow.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                positionSpheresShader,
                "src/glsl/trajectory_positions.fx.glsl");

    endCompileShaders();

    crossSectionGridsNeedUpdate = true;
}


void MNWPHorizontalSectionActor::setSurfaceShadowEnabled(bool enable)
{
    properties->mBool()->setValue(shadowEnabledProp, enable);
}


void MNWPHorizontalSectionActor::saveConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::saveConfiguration(settings);

    settings->beginGroup(MNWPHorizontalSectionActor::getSettingsID());

    settings->setValue("slicePosition_hPa", slicePosition_hPa);
    settings->setValue("differenceMode", differenceMode);
    MBoundingBoxInterface::saveConfiguration(settings);
    settings->setValue("shadowEnabled", renderShadowQuad);
    settings->setValue("shadowColor", shadowColor);
    settings->setValue("shadowHeight", shadowHeight);

    settings->beginGroup("Windbarbs");

    settings->setValue("enabled", windBarbsSettings->enabled);
    settings->setValue("automatic", windBarbsSettings->automaticScalingEnabled);
    settings->setValue("lineWidth", windBarbsSettings->lineWidth);
    settings->setValue("pennantTilt", windBarbsSettings->pennantTilt);
    settings->setValue("color", windBarbsSettings->color);
    settings->setValue("showCalmGlyphs", windBarbsSettings->showCalmGlyphs);
    settings->setValue("reduceFactor", windBarbsSettings->reduceFactor);
    settings->setValue("reduceSlope", windBarbsSettings->reduceSlope);
    settings->setValue("sensitivity", windBarbsSettings->sensitivity);
    settings->setValue("uComponent", windBarbsSettings->uComponentVarIndex);
    settings->setValue("vComponent", windBarbsSettings->vComponentVarIndex);
    settings->setValue("deltaBarbsLonLat", windBarbsSettings->deltaBarbsLonLat);
    settings->setValue("clampDeltaBarbsToGrid", windBarbsSettings->clampDeltaBarbsToGrid);

    settings->endGroup(); // Windbarbs

    graticuleActor->saveConfiguration(settings);
    settings->endGroup(); // MNWPHorizontalSectionActor
}


void MNWPHorizontalSectionActor::loadConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::loadConfiguration(settings);

    // Special case: If Met.3D is connected to Metview (i.e. run in "Metview
    // mode") AND the dataset passed from Metview contains only a single
    // variable AND by default an empty horizontal section is created, THEN the
    // variable in the dataset shall be automatically added to the cross-section.
    if (variables.size() == 0
            && MSystemManagerAndControl::getInstance()->isConnectedToMetview())
    {
         MSelectDataSourceDialog dialog(this->supportedLevelTypes());
         bool ok = false;

         MSelectableDataSource dataSource =
                 dialog.checkIfSingleDataSourceWithSingleVariableIsPresent(&ok);
         if (ok)
         {
             // Create new actor variable.
             MNWP2DHorizontalActorVariable *var =
                     static_cast<MNWP2DHorizontalActorVariable*>(
                         createActorVariable(dataSource));

             // Add the variable to the actor.
             addActorVariable(var, QString());
             var->setTransferFunctionToFirstAvailable();
             var->setRenderMode(
                         MNWP2DSectionActorVariable::RenderMode::FilledContours);
         }
    }

    settings->beginGroup(MNWPHorizontalSectionActor::getSettingsID());

    setSlicePosition(settings->value("slicePosition_hPa", 500.).toDouble());

    // Suppress actor updates when loading bounding box to avoid
    // computeRenderRegionParameters() method being called when no grid is
    // available.
    enableActorUpdates(false);
    MBoundingBoxInterface::loadConfiguration(settings);
    enableActorUpdates(true);

    properties->mEnum()->setValue(
                differenceModeProperty,
                settings->value("differenceMode", 0).toInt());

    properties->mBool()->setValue(
                shadowEnabledProp,
                settings->value("shadowEnabled", true).toBool());
    properties->mColor()->setValue(
                shadowColorProp,
                settings->value("shadowColor", QColor(60,60,60,70)).value<QColor>());
    properties->mBool()->setValue(
                shadowHeightProp,
                settings->value("shadowHeight", 0.01f).toFloat());

    settings->beginGroup("Windbarbs");

    properties->mBool()->setValue(
                windBarbsSettings->enabledProperty,
                settings->value("enabled", false).toBool());
    properties->mBool()->setValue(
                windBarbsSettings->automaticScalingEnabledProperty,
                settings->value("automatic", false).toBool());
    properties->mDouble()->setValue(
                windBarbsSettings->lineWidthProperty,
                settings->value("lineWidth", 0.04).toFloat());
    properties->mInt()->setValue(
                windBarbsSettings->pennantTiltProperty,
                settings->value("pennantTilt", 9).toInt());
    properties->mColor()->setValue(
                windBarbsSettings->colorProperty,
                settings->value("color", QColor(0,0,127)).value<QColor>());
    properties->mBool()->setValue(
                windBarbsSettings->showCalmGlyphsProperty,
                settings->value("showCalmGlyphs", false).toBool());
    properties->mDouble()->setValue(
                windBarbsSettings->deltaBarbsLonLatProperty,
                settings->value("deltaBarbsLonLat", 1.).toFloat());
    properties->mBool()->setValue(
                windBarbsSettings->clampDeltaBarbsToGridProperty,
                settings->value("clampDeltaBarbsToGrid", true).toBool());
    properties->mDouble()->setValue(
                windBarbsSettings->reduceFactorProperty,
                settings->value("reduceFactor", 15.).toFloat());
    properties->mDouble()->setValue(
                windBarbsSettings->reduceSlopeProperty,
                settings->value("reduceSlope", 0.0175).toFloat());
    properties->mDouble()->setValue(
                windBarbsSettings->sensitivityProperty,
                settings->value("sensitivity", 1.).toFloat());
    properties->mEnum()->setValue(
                windBarbsSettings->uComponentVarProperty,
                settings->value("uComponent", 0).toInt());
    properties->mEnum()->setValue(
                windBarbsSettings->vComponentVarProperty,
                settings->value("vComponent", 0).toInt());

    settings->endGroup(); // Windbarbs

    graticuleActor->loadConfiguration(settings);
    settings->endGroup(); // MNWPHorizontalSectionActor
}


int MNWPHorizontalSectionActor::checkIntersectionWithHandle(
        MSceneViewGLWidget *sceneView,
        float clipX, float clipY)
{
    // First call? Generate positions of corner points.
    if (mouseHandlePoints.size() == 0) updateMouseHandlePositions();

    selectedMouseHandle = -1;

    float clipRadius = MSystemManagerAndControl::getInstance()->getHandleSize();

    // Loop over all corner points and check whether the mouse cursor is inside
    // a circle with radius "clipRadius" around the corner point (in clip space).
    for (int i = 0; i < mouseHandlePoints.size(); i++)
    {
        // Compute the world position of the current pole
        QVector3D posPole = mouseHandlePoints.at(i);
        posPole.setZ(sceneView->worldZfromPressure(posPole.z()));

        // Obtain the camera position and the view direction
        const QVector3D& cameraPos = sceneView->getCamera()->getOrigin();
        QVector3D viewDir = posPole - cameraPos;

        // Scale the radius (in world space) with respect to the viewer distance
        float radius = static_cast<float>(clipRadius * viewDir.length() / 100.0);

        QMatrix4x4 *mvpMatrix = sceneView->getModelViewProjectionMatrix();
        // Compute the world position of the current mouse position
        QVector3D mouseWorldPos = mvpMatrix->inverted() * QVector3D(clipX, clipY, 1);

        // Get the ray direction from the camera to the mouse position
        QVector3D l = mouseWorldPos - cameraPos;
        l.normalize();

        // Compute (o - c) // ray origin (o) - sphere center (c)
        QVector3D oc = cameraPos - posPole;
        // Length of (o - c) = || o - c ||
        float lenOC = static_cast<float>(oc.length());
        // Compute l * (o - c)
        float loc = static_cast<float>(QVector3D::dotProduct(l, oc));

        // Solve equation:
        // d = - (l * (o - c) +- sqrt( (l * (o - c))² - || o - c ||² + r² )
        // Since the equation can be solved only if root discriminant is >= 0
        // just compute the discriminant
        float root = loc * loc - lenOC * lenOC + radius * radius;

        // If root discriminant is positive or zero, there's an intersection
        if (root >= 0)
        {
            selectedMouseHandle = i;
            mvpMatrix = sceneView->getModelViewProjectionMatrix();
            QVector3D posCentreClip = *mvpMatrix * posPole;
            offsetPickPositionToHandleCentre = QVector2D(posCentreClip.x() - clipX,
                                     posCentreClip.y() - clipY);
            break;
        }
    }

    return selectedMouseHandle;
}
void addPositionLabel(MSceneViewGLWidget *sceneView, int handleID);


void MNWPHorizontalSectionActor::addPositionLabel(MSceneViewGLWidget *sceneView,
                                                  int handleID, float clipX,
                                                  float clipY)
{
    // Select an arbitrary z-value to construct a point in clip space that,
    // transformed to world space, lies on the ray passing through the camera
    // and the location on the worldZ==0 plane "picked" by the mouse.
    // (See notes 22-23Feb2012).
    QVector3D mousePosClipSpace = QVector3D(clipX, clipY, 0.);

    // The point p at which the ray intersects the worldZ==0 plane is found by
    // computing the value d in p=d*l+l0, where l0 is a point on the ray and l
    // is a vector in the direction of the ray. d can be found with
    //        (p0 - l0) * n
    //   d = ----------------
    //            l * n
    // where p0 is a point on the worldZ==0 plane and n is the normal vector
    // of the plane.
    //       http://en.wikipedia.org/wiki/Line-plane_intersection

    // To compute l0, the MVP matrix has to be inverted.
    QMatrix4x4 *mvpMatrix = sceneView->getModelViewProjectionMatrix();
    QVector3D l0 = mvpMatrix->inverted() * mousePosClipSpace;

    // Compute l as the vector from l0 to the camera origin.
    QVector3D cameraPosWorldSpace = sceneView->getCamera()->getOrigin();
    QVector3D l = (l0 - cameraPosWorldSpace);

    // The plane's origin is the selected mouse handle.
    QVector3D p0 = mouseHandlePoints.at(handleID);
    // The normal vector is taken as the vector to the camera with a zero value
    // in the worldZ-direction -> a vector in the x/y plane.
    QVector3D n = sceneView->getCamera()->getOrigin() - p0;
    n.setZ(0);

    // Compute the mouse position in world space.
    float d = QVector3D::dotProduct(p0 - l0, n) / QVector3D::dotProduct(l, n);
    QVector3D mousePosWorldSpace = l0 + d * l;

    // Get properties for label font size and colour and bounding box.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);
    double elevation = properties->mDDouble()->value(slicePosProperty);

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    MTextManager* tm = glRM->getTextManager();
    positionLabel = tm->addText(
                QString("elevation:%1").arg(elevation, 0, 'f', 2),
                MTextManager::LONLATP, mouseHandlePoints[handleID].x(),
                mouseHandlePoints[handleID].y(), mouseHandlePoints[handleID].z(),
                labelsize, labelColour, MTextManager::LOWERRIGHT,
                labelbbox, labelBBoxColour);

    double dist = computePositionLabelDistanceWeight(sceneView->getCamera(),
                                                     mousePosWorldSpace);
    QVector3D anchorOffset = dist * sceneView->getCamera()->getXAxis();
    positionLabel->anchorOffset = -anchorOffset;

    emitActorChangedSignal();
}


void MNWPHorizontalSectionActor::dragEvent(
        MSceneViewGLWidget *sceneView,
        int handleID, float clipX, float clipY)
{
    // http://stackoverflow.com/questions/2093096/implementing-ray-picking

    if (mouseHandlePoints.size() == 0) return;

    // Select an arbitrary z-value to construct a point in clip space that,
    // transformed to world space, lies on the ray passing through the camera
    // and the location on the worldZ==0 plane "picked" by the mouse.
    // (See notes 22-23Feb2012).
    QVector3D mousePosClipSpace = QVector3D(clipX + offsetPickPositionToHandleCentre.x(),
                                            clipY + offsetPickPositionToHandleCentre.y(),
                                            0.);

    // The point p at which the ray intersects the worldZ==0 plane is found by
    // computing the value d in p=d*l+l0, where l0 is a point on the ray and l
    // is a vector in the direction of the ray. d can be found with
    //        (p0 - l0) * n
    //   d = ----------------
    //            l * n
    // where p0 is a point on the worldZ==0 plane and n is the normal vector
    // of the plane.
    //       http://en.wikipedia.org/wiki/Line-plane_intersection

    // To compute l0, the MVP matrix has to be inverted.
    QMatrix4x4 *mvpMatrix = sceneView->getModelViewProjectionMatrix();
    QVector3D l0 = mvpMatrix->inverted() * mousePosClipSpace;

    // Compute l as the vector from l0 to the camera origin.
    QVector3D cameraPosWorldSpace = sceneView->getCamera()->getOrigin();
    QVector3D l = (l0 - cameraPosWorldSpace);

    // The plane's origin is the selected mouse handle.
    QVector3D p0 = mouseHandlePoints.at(handleID);
    // The normal vector is taken as the vector to the camera with a zero value
    // in the worldZ-direction -> a vector in the x/y plane.
    QVector3D n = sceneView->getCamera()->getOrigin() - p0;
    n.setZ(0);

    // Compute the mouse position in world space.
    float d = QVector3D::dotProduct(p0 - l0, n) / QVector3D::dotProduct(l, n);
    QVector3D mousePosWorldSpace = l0 + d * l;

    // Transform world space Z to pressure (hPa) and round off to match
    // granularity requested by used.
    double p_hPa = sceneView->pressureFromWorldZ(mousePosWorldSpace.z());
    p_hPa = p_hPa - fmod(p_hPa, slicePositionGranularity_hPa);

    if (positionLabel != nullptr)
    {
        double elevation = max(p_hPa,
                               properties->mDDouble()->minimum(slicePosProperty));
        elevation = min(elevation,
                        properties->mDDouble()->maximum(slicePosProperty));
        // Get properties for label font size and colour and bounding box.
        int labelsize = properties->mInt()->value(labelSizeProperty);
        QColor labelColour = properties->mColor()->value(labelColourProperty);
        bool labelbbox = properties->mBool()->value(labelBBoxProperty);
        QColor labelBBoxColour =
                properties->mColor()->value(labelBBoxColourProperty);

        MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
        MTextManager* tm = glRM->getTextManager();
        positionLabel = tm->addText(
                    QString("elevation:%1").arg(elevation, 0, 'f', 2),
                    MTextManager::LONLATP, p0.x(), p0.y(), elevation,
                    labelsize, labelColour, MTextManager::LOWERRIGHT,
                    labelbbox, labelBBoxColour);

        double dist = computePositionLabelDistanceWeight(sceneView->getCamera(),
                                                 mousePosWorldSpace);
        QVector3D anchorOffset = dist * sceneView->getCamera()->getXAxis();
        positionLabel->anchorOffset = -anchorOffset;
    }

    // Set slice position to new pressure elevation.
    setSlicePosition(p_hPa);
}


const QList<MVerticalLevelType> MNWPHorizontalSectionActor::supportedLevelTypes()
{
    return (QList<MVerticalLevelType>()
            << HYBRID_SIGMA_PRESSURE_3D
            << AUXILIARY_PRESSURE_3D
            << PRESSURE_LEVELS_3D
            << LOG_PRESSURE_LEVELS_3D
            << SURFACE_2D);
}


MNWPActorVariable* MNWPHorizontalSectionActor::createActorVariable(
        const MSelectableDataSource& dataSource)
{
    MNWP2DHorizontalActorVariable* newVar =
            new MNWP2DHorizontalActorVariable(this);

    newVar->dataSourceID = dataSource.dataSourceID;
    newVar->levelType = dataSource.levelType;
    newVar->variableName = dataSource.variableName;
    newVar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::Disabled);

    return newVar;
}


bool MNWPHorizontalSectionActor::isConnectedTo(MActor *actor)
{
    if (MNWPMultiVarActor::isConnectedTo(actor))
    {
        return true;
    }
    if (slicePosSynchronizationActor == actor)
    {
        return true;
    }
    // This actor is connected to the argument actor if the argument actor is
    // the transfer function (scalar to texture) of any variable.
    foreach (MNWPActorVariable *var, variables)
    {
        if (static_cast<MNWP2DHorizontalActorVariable*>(var)
                ->spatialTransferFunction == actor)
        {
            return true;
        }
    }

    return false;
}


void MNWPHorizontalSectionActor::onBoundingBoxChanged()
{
    labels.clear();
    // Inform graticule actor about the bounding box change since it has no own
    // connection to the bounding box.
    graticuleActor->onBoundingBoxChanged();
    if (suppressActorUpdates())
    {
        return;
    }
    // Switching to no bounding box only needs a redraw, but no recomputation
    // because it disables rendering of the actor.
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        emitActorChangedSignal();
        return;
    }
    // The bbox position has changed. In the next render cycle, update the
    // render region, download target grid from GPU and update contours.
    computeRenderRegionParameters();
    updateMouseHandlePositions();
    crossSectionGridsNeedUpdate = true;
    emitActorChangedSignal();
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MNWPHorizontalSectionActor::setSlicePosition(double pressure_hPa)
{
    properties->mDDouble()->setValue(slicePosProperty, pressure_hPa);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MNWPHorizontalSectionActor::initializeActorResources()
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    windBarbsSettings->varNameList.clear();

    // Parent initialisation.
    MNWPMultiVarActor::initializeActorResources();

    for (int vi = 0; vi < variables.size(); vi++)
    {
        MNWPActorVariable* var = variables.at(vi);

        windBarbsSettings->varNameList << var->variableName;
    }

    properties->mEnum()->setEnumNames(windBarbsSettings->uComponentVarProperty,
                                      windBarbsSettings->varNameList);
    properties->mEnum()->setEnumNames(windBarbsSettings->vComponentVarProperty,
                                      windBarbsSettings->varNameList);
    properties->mEnum()->setValue(windBarbsSettings->uComponentVarProperty,
                                  windBarbsSettings->uComponentVarIndex);
    properties->mEnum()->setValue(windBarbsSettings->vComponentVarProperty,
                                  windBarbsSettings->vComponentVarIndex);

    // Set this status variable to download the target grid to CPU memory in
    // the first render cycle.
    crossSectionGridsNeedUpdate = true;

    // Compute the grid indices that correspond to the current bounding box
    // (the bounding box can have different extents than the data grid) during
    // the first render cycle.
    updateRenderRegion = true;

    // Load shader for filled contours and marching squares line contours.
    bool loadShaders = false;

    loadShaders |= glRM->generateEffectProgram("hsec_marchingsquares",
                                                glMarchingSquaresShader);
    loadShaders |= glRM->generateEffectProgram("hsec_filledcountours",
                                                glFilledContoursShader);
    loadShaders |= glRM->generateEffectProgram("hsec_texturedcountours",
                                                glTexturedContoursShader);
    loadShaders |= glRM->generateEffectProgram("hsec_interpolation",
                                                glVerticalInterpolationEffect);
    loadShaders |= glRM->generateEffectProgram("hsec_pseudocolor",
                                                glPseudoColourShader);
    loadShaders |= glRM->generateEffectProgram("hsec_windbarbs",
                                                glWindBarbsShader);
    loadShaders |= glRM->generateEffectProgram("hsec_shadow",
                                                glShadowQuad);
    loadShaders |= glRM->generateEffectProgram("vsec_positionsphere",
                                                positionSpheresShader);

    if (loadShaders) reloadShaderEffects();

    // Explicitly initialize the graticule actor here. This is needed to get a
    // valid reference to its "labels" list in the first
    // "computeRenderRegionParameters()" call. If the graticule actor is not
    // initialized here, no labels will be displayed until the next bbox
    // change.
    graticuleActor->initialize();
}


void MNWPHorizontalSectionActor::onQtPropertyChanged(QtProperty *property)
{
    // Parent signal processing.
    MNWPMultiVarActor::onQtPropertyChanged(property);

    if (property == slicePosProperty)
    {
        // The slice position has been changed.
        slicePosition_hPa = properties->mDDouble()->value(slicePosProperty);
        // Synchronize vertical position with graticule actor.
        graticuleActor->setVerticalPosition(slicePosition_hPa);

        // Interpolate to target grid in next render cycle.
        crossSectionGridsNeedUpdate = true;

        emit slicePositionChanged(slicePosition_hPa);

        if (suppressActorUpdates()
                || bBoxConnection->getBoundingBox() == nullptr)
        {
            return;
        }

        updateDescriptionLabel();
        updateMouseHandlePositions();
        emitActorChangedSignal();
    }

    else if (property == slicePosGranularityProperty)
    {
        slicePositionGranularity_hPa =
                properties->mDDouble()->value(slicePosGranularityProperty);

        properties->mDDouble()->setSingleStep(slicePosProperty,
                                              slicePositionGranularity_hPa);
    }

    else if (property == synchronizeSlicePosWithOtherActorProperty)
    {
        QString hsecName = properties->getEnumItem(
                    synchronizeSlicePosWithOtherActorProperty);

        // Disconnect from previous synchronization actor.
        if (slicePosSynchronizationActor != nullptr)
        {
            disconnect(slicePosSynchronizationActor,
                       SIGNAL(slicePositionChanged(double)),
                       this, SLOT(setSlicePosition(double)));
        }

        // Get pointer to new synchronization actor and connect to signal.
        MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
        slicePosSynchronizationActor = dynamic_cast<MNWPHorizontalSectionActor*>(
                    glRM->getActorByName(hsecName));

        if (slicePosSynchronizationActor != nullptr)
        {
            connect(slicePosSynchronizationActor,
                    SIGNAL(slicePositionChanged(double)),
                    this, SLOT(setSlicePosition(double)));
        }
    }

    else if ( (property == labelSizeProperty)
              || (property == labelColourProperty)
              || (property == labelBBoxProperty)
              || (property == labelBBoxColourProperty) )
    {
        if (suppressActorUpdates()) return;

        updateDescriptionLabel();
        emitActorChangedSignal();
    }

    else if (property == differenceModeProperty)
    {
        differenceMode = properties->mEnum()->value(differenceModeProperty);
        crossSectionGridsNeedUpdate = true;
        emitActorChangedSignal();
    }

    else if (property == windBarbsSettings->enabledProperty ||
             property == windBarbsSettings->automaticScalingEnabledProperty ||
             property == windBarbsSettings->lineWidthProperty ||
             property == windBarbsSettings->pennantTiltProperty ||
             property == windBarbsSettings->colorProperty ||
             property == windBarbsSettings->showCalmGlyphsProperty ||
             property == windBarbsSettings->deltaBarbsLonLatProperty ||
             property == windBarbsSettings->clampDeltaBarbsToGridProperty ||
             property == windBarbsSettings->reduceFactorProperty ||
             property == windBarbsSettings->reduceSlopeProperty ||
             property == windBarbsSettings->sensitivityProperty ||
             property == windBarbsSettings->uComponentVarProperty ||
             property == windBarbsSettings->vComponentVarProperty)
    {
        windBarbsSettings->enabled = properties->mBool()
                ->value(windBarbsSettings->enabledProperty);
        windBarbsSettings->automaticScalingEnabled = properties->mBool()
                ->value(windBarbsSettings->automaticScalingEnabledProperty);
        windBarbsSettings->lineWidth = properties->mDouble()
                ->value(windBarbsSettings->lineWidthProperty);
        windBarbsSettings->pennantTilt = properties->mInt()
                ->value(windBarbsSettings->pennantTiltProperty);
        windBarbsSettings->color = properties->mColor()
                ->value(windBarbsSettings->colorProperty);
        windBarbsSettings->showCalmGlyphs = properties->mBool()
                ->value(windBarbsSettings->showCalmGlyphsProperty);
        windBarbsSettings->deltaBarbsLonLat = properties->mDouble()
                ->value(windBarbsSettings->deltaBarbsLonLatProperty);
        windBarbsSettings->clampDeltaBarbsToGrid = properties->mBool()
                ->value(windBarbsSettings->clampDeltaBarbsToGridProperty);
        windBarbsSettings->reduceFactor = properties->mDouble()
                ->value(windBarbsSettings->reduceFactorProperty);
        windBarbsSettings->reduceSlope = properties->mDouble()
                ->value(windBarbsSettings->reduceSlopeProperty);
        windBarbsSettings->sensitivity = properties->mDouble()
                ->value(windBarbsSettings->sensitivityProperty);
        windBarbsSettings->uComponentVarIndex = properties->mEnum()
                ->value(windBarbsSettings->uComponentVarProperty);
        windBarbsSettings->vComponentVarIndex = properties->mEnum()
                ->value(windBarbsSettings->vComponentVarProperty);

        if (suppressActorUpdates()) return;

        emitActorChangedSignal();
    }

    else if (property == shadowEnabledProp ||
             property == shadowColorProp ||
             property == shadowHeightProp)
    {
        renderShadowQuad = properties->mBool()->value(shadowEnabledProp);
        shadowColor = properties->mColor()->value(shadowColorProp);
        shadowHeight = properties->mDouble()->value(shadowHeightProp);

        emitActorChangedSignal();
    }
}


void MNWPHorizontalSectionActor::onOtherActorCreated(MActor *actor)
{
    // If the new actor is a horizontal section, add it to the list of
    // available sync actors.
    if (MNWPHorizontalSectionActor *hsec =
            dynamic_cast<MNWPHorizontalSectionActor*>(actor))
    {
        // Don't render while the properties are being updated.
        enableEmissionOfActorChangedSignal(false);

        MQtProperties *properties = actor->getQtProperties();
        int index = properties->mEnum()->value(
                    synchronizeSlicePosWithOtherActorProperty);

        QStringList availableHSecs = properties->mEnum()->enumNames(
                    synchronizeSlicePosWithOtherActorProperty);
        availableHSecs << hsec->getName();
        properties->mEnum()->setEnumNames(
                    synchronizeSlicePosWithOtherActorProperty, availableHSecs);

        properties->mEnum()->setValue(
                    synchronizeSlicePosWithOtherActorProperty, index);

        enableEmissionOfActorChangedSignal(true);
    }
}


void MNWPHorizontalSectionActor::onOtherActorDeleted(MActor *actor)
{
    if (MNWPHorizontalSectionActor *hsec =
            dynamic_cast<MNWPHorizontalSectionActor*>(actor))
    {
        // Don't render while the properties are being updated.
        enableEmissionOfActorChangedSignal(false);

        MQtProperties *properties = actor->getQtProperties();

        // Remember the name of the currently sync'ed HSec.
        QString syncHSec = properties->getEnumItem(
                    synchronizeSlicePosWithOtherActorProperty);

        // If this actor is currently sync'ed with the one to be deleted
        // reset sync.
        if (hsec->getName() == syncHSec) syncHSec = "None";

        // Remove actor name from list.
        QStringList availableHSecs = properties->mEnum()->enumNames(
                    synchronizeSlicePosWithOtherActorProperty);
        availableHSecs.removeOne(hsec->getName());
        properties->mEnum()->setEnumNames(
                    synchronizeSlicePosWithOtherActorProperty, availableHSecs);

        // Restore currently selected sync actor.
        properties->setEnumItem(
                    synchronizeSlicePosWithOtherActorProperty, syncHSec);

        enableEmissionOfActorChangedSignal(true);
    }
}


void MNWPHorizontalSectionActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    // Draw nothing if no bounding box is available.
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }

    // UPDATE REGION PARAMETERS if horizontal bounding box has changed.
    // ================================================================
    if (updateRenderRegion)
    {
        // This method might already be called between initial data request and
        // all data fields being available. Return if not all variables
        // contain valid data yet.
        foreach (MNWPActorVariable *var, variables)
            if ( !var->hasData() ) return;

        computeRenderRegionParameters();
        updateRenderRegion = false;
    }

    // Render surface shadow.
    if (renderShadowQuad) { renderShadow(sceneView); }

    // LOOP over variables, render according to their settings.
    // ========================================================
    for (int vi = 0; vi < variables.size(); vi++)
    {
        MNWP2DHorizontalActorVariable* var =
                static_cast<MNWP2DHorizontalActorVariable*> (variables.at(vi));

        if ( !var->hasData() ) continue;

        // If the slice position is outside the data domain of this variable,
        // there is nothing to render. Special case: Don't restrict Surface
        // fields to there data volume pressure since this is zero and thus
        // surface fields won't be drawn anymore.
        if (var->grid->getLevelType() != MVerticalLevelType::SURFACE_2D
                && (var->grid->getBottomDataVolumePressure() < slicePosition_hPa
                    || var->grid->getTopDataVolumePressure() > slicePosition_hPa))
        {
            continue;
        }

        // If the bounding box is outside the model grid domain, there is
        // nothing to render (see computeRenderRegionParameters()).
        if (var->nlons == 0 || var->nlats == 0)
        {
            continue;
        }

        // Vertically interpolate and update this variable's cross-section grid
        // (for example, when the isopressure value changes or the data field
        // has changed).
        if (crossSectionGridsNeedUpdate)
        {
            if ((vi == 0) && (differenceMode > 0))
            {
                // DIFFERENCE MODE: Render difference between 1st & 2nd variable
                MNWP2DHorizontalActorVariable* varDiff =
                        static_cast<MNWP2DHorizontalActorVariable*> (variables.at(1));


                // If the slice position is outside the model grid domain of the
                // 2nd variable, there is nothing to render.
                if ((varDiff->grid->getBottomDataVolumePressure()
                     < slicePosition_hPa)
                        || (varDiff->grid->getTopDataVolumePressure()
                            > slicePosition_hPa))
                {
                    continue;
                }
                renderVerticalInterpolationDifference(var, varDiff);
            }
            else
            {
                renderVerticalInterpolation(var);
            }

            // If line contours are enabled re-compute the contour indices
            // (i.e. which isovalues actually will be visible, the others
            // don't need to be rendered).
            switch (var->renderSettings.renderMode)
            {
            case MNWP2DHorizontalActorVariable::RenderMode::LineContours:
            case MNWP2DHorizontalActorVariable::RenderMode::FilledAndLineContours:
            case MNWP2DHorizontalActorVariable::RenderMode::PseudoColourAndLineContours:
                var->updateContourIndicesFromTargetGrid(slicePosition_hPa);
                break;

            default:
                break;
            }
        }

        switch (var->renderSettings.renderMode)
        {
        case MNWP2DHorizontalActorVariable::RenderMode::FilledContours:
            renderFilledContours(sceneView, var);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::PseudoColour:
            renderPseudoColour(sceneView, var);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::LineContours:
            renderLineCountours(sceneView, var);
            renderContourLabels(sceneView, var);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::FilledAndLineContours:
            renderFilledContours(sceneView, var);
            renderLineCountours(sceneView, var);
            renderContourLabels(sceneView, var);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::PseudoColourAndLineContours:
            renderPseudoColour(sceneView, var);
            renderLineCountours(sceneView, var);
            renderContourLabels(sceneView, var);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::TexturedContours:
            renderTexturedContours(sceneView, var);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::FilledAndTexturedContours:
            renderFilledContours(sceneView, var);
            renderTexturedContours(sceneView, var);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::LineAndTexturedContours:
            renderTexturedContours(sceneView, var);
            renderLineCountours(sceneView, var);
            renderContourLabels(sceneView, var);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::PseudoColourAndTexturedContours:
            renderPseudoColour(sceneView, var);
            renderTexturedContours(sceneView, var);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::FilledAndLineAndTexturedContours:
            renderFilledContours(sceneView, var);
            renderTexturedContours(sceneView, var);
            renderLineCountours(sceneView, var);
            renderContourLabels(sceneView, var);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::PseudoColourAndLineAndTexturedContours:
            renderPseudoColour(sceneView, var);
            renderTexturedContours(sceneView, var);
            renderLineCountours(sceneView, var);
            renderContourLabels(sceneView, var);
            break;

        default:
            break;
        }

        // In difference mode, skip the second variable for rendering.
        if ((vi == 0) && (differenceMode > 0)) vi++;
    } // for (variables)

    // When the cross section grid has changed a second redraw is necessary
    // (see below) -- not sure why.
//TODO: Why is this necessary?
    bool actorNeedsRedraw = crossSectionGridsNeedUpdate;

    // Don't update the cross-section grids until the next update event occurs
    // (see actOnPropertyChange() and dataFieldChangedEvent().
    crossSectionGridsNeedUpdate = false;

    // Render the GRATICULE.
    // =====================
    graticuleActor->render(sceneView);

    if (labelsAreEnabled)
    {
//TODO (mr, Feb2015): Labels should not be rendered here but inserted into
//                    actor label pool.
//                    This becomes important if global label collision detection
//                    is implemented.
        MTextManager* tm = MGLResourcesManager::getInstance()->getTextManager();
        tm->renderLabelList(sceneView, graticuleActor->getLabelsToRender());
    }

    // Render the WINDBARBS.
    // =====================
    if (windBarbsSettings->enabled)
    {
        renderWindBarbs(sceneView);
    }

    // Render HANDLES in interaction mode.
    // ===================================
    if (sceneView->interactionModeEnabled() &&
            (vbMouseHandlePoints != nullptr))
    {
        positionSpheresShader->bindProgram("Normal");

        positionSpheresShader->setUniformValue(
                    "mvpMatrix",
                    *(sceneView->getModelViewProjectionMatrix()));
        positionSpheresShader->setUniformValue(
                    "pToWorldZParams",
                    sceneView->pressureToWorldZParameters());
        positionSpheresShader->setUniformValue(
                    "lightDirection",
                    sceneView->getLightDirection());
        positionSpheresShader->setUniformValue(
                    "cameraPosition",
                    sceneView->getCamera()->getOrigin());
        positionSpheresShader->setUniformValue(
                    "cameraUpDir",
                    sceneView->getCamera()->getYAxis());
        positionSpheresShader->setUniformValue(
                    "radius", GLfloat(MSystemManagerAndControl::getInstance()
                                      ->getHandleSize()));
        positionSpheresShader->setUniformValue(
                    "scaleRadius",
                    GLboolean(true));

        positionSpheresShader->setUniformValue(
                    "useTransferFunction", GLboolean(false));
        positionSpheresShader->setUniformValue(
                    "constColour", QColor(Qt::white));

        if (selectedMouseHandle >= 0)
        {
            positionSpheresShader->setUniformValue(
                    "constColour", QColor(Qt::red));
        }

        vbMouseHandlePoints->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

        glPolygonMode(GL_FRONT_AND_BACK,
                      renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
        glLineWidth(1); CHECK_GL_ERROR;

        glDrawArrays(GL_POINTS, 0, 4); CHECK_GL_ERROR;

        // Unbind VBO.
        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    }

    if (actorNeedsRedraw) emitActorChangedSignal();
}


void MNWPHorizontalSectionActor::dataFieldChangedEvent()
{
    crossSectionGridsNeedUpdate = true;
    emitActorChangedSignal();
}


void MNWPHorizontalSectionActor::computeRenderRegionParameters()
{
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }

    // Compute render region parameters for each variable.
    for (int vi = 0; vi < variables.size(); vi++)
    {
        MNWP2DHorizontalActorVariable* var =
                static_cast<MNWP2DHorizontalActorVariable*> (variables.at(vi));

        var->computeRenderRegionParameters(
                    bBoxConnection->westLon(), bBoxConnection->southLat(),
                    bBoxConnection->eastLon(), bBoxConnection->northLat());
    }

    // The label displaying the current pressure elevation needs to be put
    // at a new place.
    updateDescriptionLabel();
}


void MNWPHorizontalSectionActor::updateDescriptionLabel(bool deleteOldLabel)
{
    // No description label available if no bounding box is selected.
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }

    MTextManager* tm = MGLResourcesManager::getInstance()->getTextManager();

    if (deleteOldLabel && !labels.isEmpty()) // deleteOldLabel true by default
    {
//TODO: This assumes that there is only the "elevation description" label
//      contained in the list!
        tm->removeText(labels.takeLast());
    }

    // Get properties for label font size and colour and bounding box.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);

    labels.append(tm->addText(
                      QString("Elevation: %1 hPa").arg(slicePosition_hPa),
                      MTextManager::LONLATP,
                      bBoxConnection->westLon(),
                      bBoxConnection->northLat(), slicePosition_hPa,
                      labelsize, labelColour, MTextManager::BASELINELEFT,
                      labelbbox, labelBBoxColour, 0.3)
                  );
}


void MNWPHorizontalSectionActor::updateMouseHandlePositions()
{
    mouseHandlePoints.clear();

    // No handles available if no bounding box is selected.
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }

    mouseHandlePoints.append(
            QVector3D(bBoxConnection->westLon(),
                      bBoxConnection->southLat(),
                      slicePosition_hPa));
    mouseHandlePoints.append(
            QVector3D(bBoxConnection->eastLon(),
                      bBoxConnection->southLat(),
                      slicePosition_hPa));
    mouseHandlePoints.append(
            QVector3D(bBoxConnection->eastLon(),
                      bBoxConnection->northLat(),
                      slicePosition_hPa));
    mouseHandlePoints.append(
            QVector3D(bBoxConnection->westLon(),
                      bBoxConnection->northLat(),
                      slicePosition_hPa));

    // Send vertices of drag handle positions to video memory.
    if (!vbMouseHandlePoints)
    {
        vbMouseHandlePoints = new GL::MVector3DVertexBuffer(
                    QString("vbmhpos_%1").arg(myID),
                    mouseHandlePoints.size());
    }
    vbMouseHandlePoints->upload(mouseHandlePoints);
}


void MNWPHorizontalSectionActor::onDeleteActorVariable(MNWPActorVariable *var)
{
    // Correct wind barb indices.

    // Get index of variable that is about to be removed.
    int i = variables.indexOf(var);

    // Update vComponentVarIndex and uComponentVarIndex if these point to
    // the removed variable or to one with a lower index.
    if (i <= windBarbsSettings->uComponentVarIndex)
    {
        windBarbsSettings->uComponentVarIndex =
                std::max(-1, windBarbsSettings->uComponentVarIndex - 1);
    }
    if (i <= windBarbsSettings->vComponentVarIndex)
    {
        windBarbsSettings->vComponentVarIndex =
                std::max(-1, windBarbsSettings->vComponentVarIndex - 1);
    }

    // Temporarily save variable indices.
    int tmpuComponentVarIndex = windBarbsSettings->uComponentVarIndex;
    int tmpvComponentVarIndex = windBarbsSettings->vComponentVarIndex;

    // Remove the variable name from the enum lists.
    windBarbsSettings->varNameList.removeAt(i);

    // Update enum lists.
    properties->mEnum()->setEnumNames(
                windBarbsSettings->uComponentVarProperty,
                windBarbsSettings->varNameList);
    properties->mEnum()->setEnumNames(
                windBarbsSettings->vComponentVarProperty,
                windBarbsSettings->varNameList);

    properties->mEnum()->setValue(
                windBarbsSettings->uComponentVarProperty,
                tmpuComponentVarIndex);
    properties->mEnum()->setValue(
                windBarbsSettings->vComponentVarProperty,
                tmpvComponentVarIndex);
}


void MNWPHorizontalSectionActor::onAddActorVariable(MNWPActorVariable *var)
{
    windBarbsSettings->varNameList << var->variableName;

    // Temporarily save variable indices.
    int tmpuComponentVarIndex = windBarbsSettings->uComponentVarIndex;
    int tmpvComponentVarIndex = windBarbsSettings->vComponentVarIndex;

    properties->mEnum()->setEnumNames(windBarbsSettings->uComponentVarProperty,
                                      windBarbsSettings->varNameList);
    properties->mEnum()->setEnumNames(windBarbsSettings->vComponentVarProperty,
                                      windBarbsSettings->varNameList);

    properties->mEnum()->setValue(
                windBarbsSettings->uComponentVarProperty,
                tmpuComponentVarIndex);
    properties->mEnum()->setValue(
                windBarbsSettings->vComponentVarProperty,
                tmpvComponentVarIndex);

    crossSectionGridsNeedUpdate = true;
    updateRenderRegion = true;
}


void MNWPHorizontalSectionActor::onChangeActorVariable(MNWPActorVariable *var)
{
    int varIndex = variables.indexOf(var);
    // Update lists of variable names.
    windBarbsSettings->varNameList.replace(varIndex, var->variableName);

    // Temporarily save variable indices.
    int tmpuComponentVarIndex = windBarbsSettings->uComponentVarIndex;
    int tmpvComponentVarIndex = windBarbsSettings->vComponentVarIndex;

    enableActorUpdates(false);
    properties->mEnum()->setEnumNames(windBarbsSettings->uComponentVarProperty,
                                      windBarbsSettings->varNameList);
    properties->mEnum()->setEnumNames(windBarbsSettings->vComponentVarProperty,
                                      windBarbsSettings->varNameList);

    properties->mEnum()->setValue(
                windBarbsSettings->uComponentVarProperty,
                tmpuComponentVarIndex);

    properties->mEnum()->setValue(
                windBarbsSettings->vComponentVarProperty,
                tmpvComponentVarIndex);
    enableActorUpdates(true);

    crossSectionGridsNeedUpdate = true;
    updateRenderRegion = true;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MNWPHorizontalSectionActor::renderVerticalInterpolation(
        MNWP2DHorizontalActorVariable *var)
{
    glVerticalInterpolationEffect->bindProgram("Standard");

    // Reset optional required textures (to avoid draw errors).
    // ========================================================

    var->textureDummy1D->bindToTextureUnit(var->textureUnitUnusedTextures);
    glVerticalInterpolationEffect->setUniformValue(
                "hybridCoefficients", var->textureUnitUnusedTextures); CHECK_GL_ERROR;

    var->textureDummy2D->bindToTextureUnit(var->textureUnitUnusedTextures);
    glVerticalInterpolationEffect->setUniformValue(
                "surfacePressure", var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue(
                "dataField2D", var->textureUnitUnusedTextures); CHECK_GL_ERROR;

    var->textureDummy3D->bindToTextureUnit(var->textureUnitUnusedTextures);
    glVerticalInterpolationEffect->setUniformValue(
                "dataField", var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue(
                "auxPressureField_hPa", var->textureUnitUnusedTextures); CHECK_GL_ERROR;

    // Set shader variables.
    // =====================

    glVerticalInterpolationEffect->setUniformValue(
                "levelType", int(var->grid->getLevelType()));

    // Texture bindings for coordinate axes (1D texture).
    var->textureLonLatLevAxes->bindToTextureUnit(var->textureUnitLonLatLevAxes);
    glVerticalInterpolationEffect->setUniformValue(
                "latLonAxesData", var->textureUnitLonLatLevAxes);
    glVerticalInterpolationEffect->setUniformValue(
                "verticalOffset", GLint(var->grid->nlons + var->grid->nlats));

    if (var->grid->getLevelType() == SURFACE_2D)
    {
        // Texture bindings for data field (2D texture).
        var->textureDataField->bindToTextureUnit(var->textureUnitDataField);
        glVerticalInterpolationEffect->setUniformValue(
                    "dataField2D", var->textureUnitDataField);
    }
    else
    {
        // Texture bindings for data field (3D texture).
        var->textureDataField->bindToTextureUnit(var->textureUnitDataField);
        glVerticalInterpolationEffect->setUniformValue(
                    "dataField", var->textureUnitDataField);
    }

    if (var->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Texture bindings for surface pressure (2D texture) and model
        // level coefficients (1D texture).
        var->textureSurfacePressure->bindToTextureUnit(
                    var->textureUnitSurfacePressure);
        var->textureHybridCoefficients->bindToTextureUnit(
                    var->textureUnitHybridCoefficients);
        glVerticalInterpolationEffect->setUniformValue(
                    "surfacePressure", var->textureUnitSurfacePressure);
        glVerticalInterpolationEffect->setUniformValue(
                    "hybridCoefficients", var->textureUnitHybridCoefficients);
    }

    if (var->grid->getLevelType() == AUXILIARY_PRESSURE_3D)
    {
        // Texture binding for pressure field (3D texture).
        var->textureAuxiliaryPressure->bindToTextureUnit(
                    var->textureUnitAuxiliaryPressure);
        glVerticalInterpolationEffect->setUniformValue(
                    "auxPressureField_hPa", var->textureUnitAuxiliaryPressure);
    }

    // Pressure value and world z coordinate of the slice.
    glVerticalInterpolationEffect->setUniformValue(
                "pressure_hPa", GLfloat(slicePosition_hPa));

    glVerticalInterpolationEffect->setUniformValue(
                "crossSectionGrid", var->imageUnitTargetGrid);
    glBindImageTexture(var->imageUnitTargetGrid, // image unit
                       var->textureTargetGrid->getTextureObject(),
                                                 // texture object
                       0,                        // level
                       GL_FALSE,                 // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       // GL_WRITE_ONLY,         // shader access
                       GL_R32F); CHECK_GL_ERROR; // format

    // Grid offsets to render only the requested subregion.
    glVerticalInterpolationEffect->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;

    // We don't want to compute entries twice thus we need to use at most the
    // width of the grid. (var->nlons can be larger than the grid size)
    glDrawArraysInstanced(GL_POINTS,
                          0, min(var->nlons, int(var->grid->nlons + 1)),
                          var->nlats); CHECK_GL_ERROR;
}


void MNWPHorizontalSectionActor::renderVerticalInterpolationDifference(
        MNWP2DHorizontalActorVariable *var,
        MNWP2DHorizontalActorVariable *varDiff)
{
    if (var->nlons != varDiff->nlons || var->nlats != varDiff->nlats)
    {
        // Both variables need to be on the same grid.
        LOG4CPLUS_ERROR(mlog, "Difference can only be rendered if both "
                        << "variables share the same horizontal grid.");
        return;
    }

    glVerticalInterpolationEffect->bindProgram("Difference");

    // Reset optional required textures (to avoid draw errors).
    // ========================================================

    var->textureDummy1D->bindToTextureUnit(var->textureUnitUnusedTextures);
    glVerticalInterpolationEffect->setUniformValue(
                "hybridCoefficients1", var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue(
                "hybridCoefficients2", var->textureUnitUnusedTextures); CHECK_GL_ERROR;

    var->textureDummy2D->bindToTextureUnit(var->textureUnitUnusedTextures);
    glVerticalInterpolationEffect->setUniformValue(
                "surfacePressure1", var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue(
                "dataField2D1", var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue(
                "surfacePressure2", var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue(
                "dataField2D2", var->textureUnitUnusedTextures); CHECK_GL_ERROR;

    var->textureDummy3D->bindToTextureUnit(var->textureUnitUnusedTextures);
    glVerticalInterpolationEffect->setUniformValue(
                "dataField1", var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue(
                "dataField2", var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue(
                "auxPressureField1_hPa", var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue(
                "auxPressureField2_hPa", var->textureUnitUnusedTextures); CHECK_GL_ERROR;

    // Texture bindings for Lat/Lon axes (1D textures).
    var->textureLonLatLevAxes->bindToTextureUnit(
                var->textureUnitLonLatLevAxes);
    glVerticalInterpolationEffect->setUniformValue(
                "latLonAxesData1", var->textureUnitLonLatLevAxes);
    glVerticalInterpolationEffect->setUniformValue(
                "verticalOffset1", GLint(var->grid->nlons + var->grid->nlats));

    varDiff->textureLonLatLevAxes->bindToTextureUnit(
                varDiff->textureUnitLonLatLevAxes);
    glVerticalInterpolationEffect->setUniformValue(
                "latLonAxesData2", varDiff->textureUnitLonLatLevAxes);
    glVerticalInterpolationEffect->setUniformValue(
                "verticalOffset2", GLint(varDiff->grid->nlons + varDiff->grid->nlats));

    if (var->grid->getLevelType() == SURFACE_2D)
    {
        // Texture bindings for data field (2D texture).
        var->textureDataField->bindToTextureUnit(var->textureUnitDataField);
        glVerticalInterpolationEffect->setUniformValue(
                    "dataField2D1", var->textureUnitDataField);
    }
    else
    {
        // Texture bindings for data field (3D texture).
        var->textureDataField->bindToTextureUnit(var->textureUnitDataField);
        glVerticalInterpolationEffect->setUniformValue(
                    "dataField1", var->textureUnitDataField);
    }

    if (varDiff->grid->getLevelType() == SURFACE_2D)
    {
        // Texture bindings for data field (2D texture).
        varDiff->textureDataField->bindToTextureUnit(varDiff->textureUnitDataField);
        glVerticalInterpolationEffect->setUniformValue(
                    "dataField2D2", varDiff->textureUnitDataField);
    }
    else
    {
        // Texture bindings for data field (3D texture).
        varDiff->textureDataField->bindToTextureUnit(varDiff->textureUnitDataField);
        glVerticalInterpolationEffect->setUniformValue(
                    "dataField2", varDiff->textureUnitDataField);
    }

    // Vertical level type dependent arguments:
    glVerticalInterpolationEffect->setUniformValue(
                "levelType1", int(var->grid->getLevelType()));

    if (var->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Texture bindings for surface pressure (2D texture) and model
        // level coefficients (1D texture).
        var->textureSurfacePressure->bindToTextureUnit(
                    var->textureUnitSurfacePressure);
        var->textureHybridCoefficients->bindToTextureUnit(
                    var->textureUnitHybridCoefficients);
        glVerticalInterpolationEffect->setUniformValue(
                    "surfacePressure1", var->textureUnitSurfacePressure);
        glVerticalInterpolationEffect->setUniformValue(
                    "hybridCoefficients1", var->textureUnitHybridCoefficients);
    }

    if (var->grid->getLevelType() == AUXILIARY_PRESSURE_3D)
    {
        // Texture binding for pressure field (3D texture).
        var->textureAuxiliaryPressure->bindToTextureUnit(
                    var->textureUnitAuxiliaryPressure); CHECK_GL_ERROR;
        glVerticalInterpolationEffect->setUniformValue(
                    "auxPressureField1_hPa", var->textureUnitAuxiliaryPressure); CHECK_GL_ERROR;
    }

    glVerticalInterpolationEffect->setUniformValue(
                "levelType2", int(varDiff->grid->getLevelType()));

    if (varDiff->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
    {
        varDiff->textureSurfacePressure->bindToTextureUnit(
                    varDiff->textureUnitSurfacePressure);
        varDiff->textureHybridCoefficients->bindToTextureUnit(
                    varDiff->textureUnitHybridCoefficients);
        glVerticalInterpolationEffect->setUniformValue(
                    "surfacePressure2", varDiff->textureUnitSurfacePressure);
        glVerticalInterpolationEffect->setUniformValue(
                    "hybridCoefficients2", varDiff->textureUnitHybridCoefficients);
    }

    if (varDiff->grid->getLevelType() == AUXILIARY_PRESSURE_3D)
    {
        varDiff->textureAuxiliaryPressure->bindToTextureUnit(
                    varDiff->textureUnitAuxiliaryPressure); CHECK_GL_ERROR;
        glVerticalInterpolationEffect->setUniformValue(
                    "auxPressureField2_hPa", varDiff->textureUnitAuxiliaryPressure); CHECK_GL_ERROR;
    }


    // Pressure value and world z coordinate of the slice.
    glVerticalInterpolationEffect->setUniformValue(
                "pressure_hPa", GLfloat(slicePosition_hPa));

    glVerticalInterpolationEffect->setUniformValue(
                "crossSectionGrid", var->imageUnitTargetGrid);
    glBindImageTexture(var->imageUnitTargetGrid,// image unit
                       var->textureTargetGrid->getTextureObject(),  // texture object
                       0,                        // level
                       GL_FALSE,                 // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       // GL_WRITE_ONLY,         // shader access
                       GL_R32F); CHECK_GL_ERROR; // format

    // Grid offsets to render only the requested subregion.
    glVerticalInterpolationEffect->setUniformValue("iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glVerticalInterpolationEffect->setUniformValue("jOffset", GLint(var->j0)); CHECK_GL_ERROR;

    glVerticalInterpolationEffect->setUniformValue("mode", differenceMode);

    // We don't want to compute entries twice thus we need to use at most the
    // width of the grid. (var->nlons can be larger than the grid size)
    glDrawArraysInstanced(GL_POINTS,
                          0, min(var->nlons, int(var->grid->nlons + 1)),
                          var->nlats); CHECK_GL_ERROR;
}


void MNWPHorizontalSectionActor::renderFilledContours(
        MSceneViewGLWidget *sceneView, MNWP2DHorizontalActorVariable *var)
{
    // Abort rendering if transfer function is not defined.
    if (var->transferFunction == nullptr)
    {
        return;
    }

    glFilledContoursShader->bind();

    // Model-view-projection matrix from the current scene view.
    glFilledContoursShader->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;

    // Texture bindings for Lat/Lon axes (1D textures).
    var->textureLonLatLevAxes->bindToTextureUnit(var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glFilledContoursShader->setUniformValue(
                "latLonAxesData", var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glFilledContoursShader->setUniformValue(
                "latOffset", var->grid->nlons); CHECK_GL_ERROR;

    // Texture bindings for transfer function for data field (1D texture from
    // transfer function class). Variables that are only rendered as
    // contour lines might not provide a valid transfer function.
    if (var->transferFunction != 0)
    {
        var->transferFunction->getTexture()->bindToTextureUnit(
                    var->textureUnitTransferFunction);
        glFilledContoursShader->setUniformValue(
                    "transferFunction", var->textureUnitTransferFunction); CHECK_GL_ERROR;
        glFilledContoursShader->setUniformValue(
                    "scalarMinimum", var->transferFunction->getMinimumValue()); CHECK_GL_ERROR;
        glFilledContoursShader->setUniformValue(
                    "scalarMaximum", var->transferFunction->getMaximumValue()); CHECK_GL_ERROR;
    }

    glFilledContoursShader->setUniformValue(
                "worldZ", GLfloat(sceneView->worldZfromPressure(slicePosition_hPa))); CHECK_GL_ERROR;

    glFilledContoursShader->setUniformValue(
                "crossSectionGrid", GLint(var->imageUnitTargetGrid)); CHECK_GL_ERROR;
    glBindImageTexture(var->imageUnitTargetGrid, // image unit
                       var->textureTargetGrid->getTextureObject(),
                                                 // texture object
                       0,                        // level
                       GL_FALSE,                 // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       // GL_WRITE_ONLY,         // shader access
                       GL_R32F); CHECK_GL_ERROR; // format

    // Grid offsets to render only the requested subregion.
    glFilledContoursShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glFilledContoursShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
    glFilledContoursShader->setUniformValue(
                "bboxLons", QVector2D(bBoxConnection->westLon(),
                                      bBoxConnection->eastLon())); CHECK_GL_ERROR;
    glFilledContoursShader->setUniformValue(
                "isCyclicGrid",
                GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
    glFilledContoursShader->setUniformValue(
                "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
    glFilledContoursShader->setUniformValue(
                "eastGridLon", GLfloat(var->grid->lons[var->grid->nlons - 1]));
    CHECK_GL_ERROR;
    glFilledContoursShader->setUniformValue(
                "shiftForWesternLon",
                GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

    // Use instanced rendering to avoid geometry upload (see notes 09Feb2012).
    glPolygonOffset(.8f, 1.0f); CHECK_GL_ERROR;
    glEnable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
    glPolygonMode(GL_FRONT_AND_BACK,
                  renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                          0, var->nlons * 2, var->nlats - 1); CHECK_GL_ERROR;

    glDisable(GL_POLYGON_OFFSET_FILL);
}


void MNWPHorizontalSectionActor::renderPseudoColour(
        MSceneViewGLWidget *sceneView, MNWP2DHorizontalActorVariable *var)
{
    glPseudoColourShader->bind();

    // Model-view-projection matrix from the current scene view.
    glPseudoColourShader->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;

    // Texture bindings for Lat/Lon axes (1D textures).
    var->textureLonLatLevAxes->bindToTextureUnit(
                var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glPseudoColourShader->setUniformValue(
                "latLonAxesData", var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glPseudoColourShader->setUniformValue(
                "latOffset", GLint(var->grid->nlons)); CHECK_GL_ERROR;
    glPseudoColourShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glPseudoColourShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
    glPseudoColourShader->setUniformValue(
                "isCyclicGrid",
                GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
    glPseudoColourShader->setUniformValue(
                "shiftForWesternLon",
                GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

    glPseudoColourShader->setUniformValue(
                "worldZ", GLfloat(sceneView->worldZfromPressure(slicePosition_hPa)));

    // The 2D data grid that the contouring algorithm processes.
    glBindImageTexture(var->imageUnitTargetGrid,      // image unit
                       var->textureTargetGrid->getTextureObject(),  // texture object
                       0,                        // level
                       GL_FALSE,                 // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       // GL_WRITE_ONLY,            // shader access
                       GL_R32F); CHECK_GL_ERROR; // format
    glPseudoColourShader->setUniformValue("sectionGrid", var->imageUnitTargetGrid);

    if (var->transferFunction != 0)
    {
        var->transferFunction->getTexture()->bindToTextureUnit(
                    var->textureUnitTransferFunction);
        glPseudoColourShader->setUniformValue(
                    "transferFunction", var->textureUnitTransferFunction); CHECK_GL_ERROR;
        glPseudoColourShader->setUniformValue(
                    "scalarMinimum", var->transferFunction->getMinimumValue()); CHECK_GL_ERROR;
        glPseudoColourShader->setUniformValue(
                    "scalarMaximum", var->transferFunction->getMaximumValue()); CHECK_GL_ERROR;
    }

    glPolygonOffset(.8f, 1.0f); CHECK_GL_ERROR;
    glEnable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
    glPolygonMode(GL_FRONT_AND_BACK,
                  renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
    glLineWidth(1); CHECK_GL_ERROR;
    glDrawArraysInstanced(GL_POINTS,
                          0, var->nlons, var->nlats); CHECK_GL_ERROR;
    glDisable(GL_POLYGON_OFFSET_FILL);
}


void MNWPHorizontalSectionActor::renderLineCountours(
        MSceneViewGLWidget *sceneView, MNWP2DHorizontalActorVariable *var)
{
    glMarchingSquaresShader->bind();

    // Model-view-projection matrix from the current scene view.
    glMarchingSquaresShader->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

    // Texture bindings for Lat/Lon axes (1D textures).
    var->textureLonLatLevAxes->bindToTextureUnit(
                var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "latLonAxesData", var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "latOffset", GLint(var->grid->nlons)); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "isCyclicGrid",
                GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "eastGridLon", GLfloat(var->grid->lons[var->grid->nlons - 1]));
    CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "shiftForWesternLon",
                GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

    glMarchingSquaresShader->setUniformValue(
                "worldZ", GLfloat(sceneView->worldZfromPressure(slicePosition_hPa)));

    // The 2D data grid that the contouring algorithm processes.
    glBindImageTexture(var->imageUnitTargetGrid,      // image unit
                       var->textureTargetGrid->getTextureObject(),  // texture object
                       0,                        // level
                       GL_FALSE,                 // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       // GL_WRITE_ONLY,            // shader access
                       GL_R32F); CHECK_GL_ERROR; // format
    glMarchingSquaresShader->setUniformValue("sectionGrid", var->imageUnitTargetGrid);

    // Draw individual line segments as output by the geometry shader (no
    // connected polygon is created).
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;

    // Loop over all contour sets.
    foreach (MNWP2DSectionActorVariable::ContourSettings contourSet,
             var->contourSetList)
    {
        if (contourSet.enabled)
        {
            glMarchingSquaresShader->setUniformValue(
                        "useTransferFunction",
                        GLboolean(var->renderSettings.contoursUseTF
                                  || contourSet.useTF)); CHECK_GL_ERROR;
            if (!(var->renderSettings.contoursUseTF || contourSet.useTF))
            {
                glMarchingSquaresShader->setUniformValue(
                            "colour", contourSet.colour); CHECK_GL_ERROR;
            }
            else
            {
                // Texture bindings for transfer function for data field
                // (1D texture from transfer function class).
                if (var->transferFunction != 0)
                {
                    var->transferFunction->getTexture()->bindToTextureUnit(
                                var->textureUnitTransferFunction);
                    glMarchingSquaresShader->setUniformValue(
                                "transferFunction",
                                var->textureUnitTransferFunction); CHECK_GL_ERROR;
                    glMarchingSquaresShader->setUniformValue(
                                "scalarMinimum",
                                GLfloat(var->transferFunction
                                        ->getMinimumValue())); CHECK_GL_ERROR;
                    glMarchingSquaresShader->setUniformValue(
                                "scalarMaximum",
                                GLfloat(var->transferFunction
                                        ->getMaximumValue())); CHECK_GL_ERROR;
                }
                // Don't draw contour set if transfer function is not present.
                else
                {
                    continue;
                }
            }
            glLineWidth(contourSet.thickness); CHECK_GL_ERROR;
            // Loop over all iso values for which contour lines should
            // be rendered -- one render pass per isovalue.
            for (int i = contourSet.startIndex; i < contourSet.stopIndex; i++)
            {
                glMarchingSquaresShader->setUniformValue(
                            "isoValue", GLfloat(contourSet.levels.at(i)));
                CHECK_GL_ERROR;
                glDrawArraysInstanced(GL_POINTS,
                                      0,
                                      var->nlons - 1,
                                      var->nlats - 1); CHECK_GL_ERROR;
            }
        }
    }
}


void MNWPHorizontalSectionActor::renderTexturedContours(
        MSceneViewGLWidget *sceneView, MNWP2DHorizontalActorVariable *var)
{
    // Abort rendering if transfer function is not defined.
    if (var->spatialTransferFunction == nullptr) return;
    if (var->spatialTransferFunction->getTexture() == nullptr) return;

//    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glTexturedContoursShader->bind();

    // Model-view-projection matrix from the current scene view.
    glTexturedContoursShader->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;

    // Texture bindings for Lat/Lon axes (1D textures).
    var->textureLonLatLevAxes->bindToTextureUnit(var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "latLonAxesData", var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "latOffset", var->grid->nlons); CHECK_GL_ERROR;

    glTexturedContoursShader->setUniformValue(
                "scalarMinimum",
                var->spatialTransferFunction->getMinimumValue()); CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "scalarMaximum",
                var->spatialTransferFunction->getMaximumValue()); CHECK_GL_ERROR;

    var->spatialTransferFunction->getTexture()->bindToTextureUnit(
                var->textureUnitSpatialTransferFunction);
    glTexturedContoursShader->setUniformValue(
                "transferFunction",
                var->textureUnitSpatialTransferFunction); CHECK_GL_ERROR;

    glTexturedContoursShader->setUniformValue(
                "distInterp",
                GLfloat(var->spatialTransferFunction->getInterpolationRange()));

    glTexturedContoursShader->setUniformValue(
                "clampMaximum",
                GLboolean(var->spatialTransferFunction->getClampMaximum()));

    glTexturedContoursShader->setUniformValue(
                "numLevels",
                GLint(var->spatialTransferFunction->getNumLevels())); CHECK_GL_ERROR;

    glTexturedContoursShader->setUniformValue(
                "scaleWidth",
                GLfloat(var->spatialTransferFunction->getTextureScale()));

    glTexturedContoursShader->setUniformValue(
                "aspectRatio",
                GLfloat(var->spatialTransferFunction->getTextureAspectRatio()));

    glTexturedContoursShader->setUniformValue(
                "gridAspectRatio",
                GLfloat(var->grid->getDeltaLon() / var->grid->getDeltaLat()));

    glTexturedContoursShader->setUniformValue(
                "worldZ", GLfloat(sceneView->worldZfromPressure(
                                      slicePosition_hPa))); CHECK_GL_ERROR;

    glTexturedContoursShader->setUniformValue("alphaBlendingMode",
                                     GLenum(var->spatialTransferFunction
                                           ->getAlphaBlendingMode()));
    glTexturedContoursShader->setUniformValue("invertAlpha",
                                     GLboolean(var->spatialTransferFunction
                                               ->getInvertAlpha()));
    glTexturedContoursShader->setUniformValue("useConstantColour",
                                     GLboolean(var->spatialTransferFunction
                                               ->getUseConstantColour()));
    glTexturedContoursShader->setUniformValue("constantColour",
                                     var->spatialTransferFunction
                                     ->getConstantColour());

    glTexturedContoursShader->setUniformValue(
                "height", GLfloat(bBoxConnection->northSouthExtent())); CHECK_GL_ERROR;

    glBindImageTexture(var->imageUnitTargetGrid, // image unit
                       var->textureTargetGrid->getTextureObject(),
                                                 // texture object
                       0,                        // level
                       GL_FALSE,                 // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       GL_R32F); CHECK_GL_ERROR; // format

    glTexturedContoursShader->setUniformValue(
                "crossSectionGrid", GLint(var->imageUnitTargetGrid)); CHECK_GL_ERROR;

    // Grid offsets to render only the requested subregion.
    glTexturedContoursShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "bboxLons", QVector2D(bBoxConnection->westLon(),
                                      bBoxConnection->eastLon())); CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "isCyclicGrid",
                GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "eastGridLon", GLfloat(var->grid->lons[var->grid->nlons - 1]));
    CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "shiftForWesternLon",
                GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

    // Use instanced rendering to avoid geometry upload (see notes 09Feb2012).
    glPolygonOffset(.8f, 1.0f); CHECK_GL_ERROR;

    glEnable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
    glPolygonMode(GL_FRONT_AND_BACK,
                  renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                          0, var->nlons * 2, var->nlats - 1); CHECK_GL_ERROR;

    glDisable(GL_POLYGON_OFFSET_FILL);

//    glEnable(GL_DEPTH_TEST);
}


void MNWPHorizontalSectionActor::renderWindBarbs(MSceneViewGLWidget *sceneView)
{
    // Selected variables valid?
    if (windBarbsSettings->vComponentVarIndex >= variables.size() ||
            windBarbsSettings->vComponentVarIndex < 0 ||
            windBarbsSettings->uComponentVarIndex >= variables.size() ||
            windBarbsSettings->uComponentVarIndex < 0)
    {
        return;
    }

    MNWP2DHorizontalActorVariable *varWindV =
            static_cast<MNWP2DHorizontalActorVariable*>(
                variables.at(windBarbsSettings->vComponentVarIndex)
                );
    MNWP2DHorizontalActorVariable *varWindU =
            static_cast<MNWP2DHorizontalActorVariable*>(
                variables.at(windBarbsSettings->uComponentVarIndex)
                );

    if (varWindV->grid->getLevelType() != varWindU->grid->getLevelType())
    {
        LOG4CPLUS_WARN(mlog, "WARNING: Wind barbs u and v variables must have "
                       "the same vertical level type. Disabling wind barbs.");
        return;
    }

    if (varWindV->grid->getLevelType() == SURFACE_2D)
    {
        LOG4CPLUS_WARN(mlog, "WARNING: Wind barbs have not been implemented for "
                       "2D surface fields. Disabling wind barbs.");
        return;
    }

    // Don't render wind barbs if horizontal slice position is
    // outside the data domain (assuming u/v are on the same grid).
    if (varWindV->grid->getBottomDataVolumePressure() < slicePosition_hPa
            || varWindV->grid->getTopDataVolumePressure() > slicePosition_hPa)
    {
        return;
    }

    glWindBarbsShader->bind();

    // Reset optional required textures (to avoid draw errors).
    // ========================================================

    varWindU->textureDummy1D->bindToTextureUnit(varWindU->textureUnitUnusedTextures);
    glWindBarbsShader->setUniformValue(
                "hybridCoefficientsU", varWindU->textureUnitUnusedTextures); CHECK_GL_ERROR;
    varWindV->textureDummy1D->bindToTextureUnit(varWindV->textureUnitUnusedTextures);
    glWindBarbsShader->setUniformValue(
                "hybridCoefficientsV", varWindV->textureUnitUnusedTextures); CHECK_GL_ERROR;

    varWindU->textureDummy2D->bindToTextureUnit(varWindU->textureUnitUnusedTextures);
    glWindBarbsShader->setUniformValue(
                "surfacePressureU", varWindU->textureUnitUnusedTextures); CHECK_GL_ERROR;
    varWindV->textureDummy2D->bindToTextureUnit(varWindV->textureUnitUnusedTextures);
    glWindBarbsShader->setUniformValue(
                "surfacePressureV", varWindV->textureUnitUnusedTextures); CHECK_GL_ERROR;

    // Set shader variables.
    // =====================

    glWindBarbsShader->setUniformValue(
                "mvpMatrix",
                *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;

//TODO (mr 17May2017) -- why is there an offset?
    const GLfloat worldZ = sceneView->worldZfromPressure(slicePosition_hPa) + 0.1;
    glWindBarbsShader->setUniformValue(
                "worldZ", worldZ); CHECK_GL_ERROR;

    varWindU->textureDataField->bindToTextureUnit(
                varWindU->textureUnitDataField);
    glWindBarbsShader->setUniformValue(
                "dataUComp",varWindU->textureUnitDataField); CHECK_GL_ERROR;
    varWindV->textureDataField->bindToTextureUnit(
                varWindV->textureUnitDataField);
    glWindBarbsShader->setUniformValue(
                "dataVComp",varWindV->textureUnitDataField); CHECK_GL_ERROR;

    if (varWindU->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
    {
        varWindU->textureSurfacePressure->bindToTextureUnit(
                    varWindU->textureUnitSurfacePressure);
        glWindBarbsShader->setUniformValue(
                    "surfacePressureU",varWindU->textureUnitSurfacePressure); CHECK_GL_ERROR;
        varWindV->textureSurfacePressure->bindToTextureUnit(
                    varWindV->textureUnitSurfacePressure);
        glWindBarbsShader->setUniformValue(
                    "surfacePressureV",varWindV->textureUnitSurfacePressure); CHECK_GL_ERROR;
        varWindU->textureHybridCoefficients->bindToTextureUnit(
                    varWindU->textureUnitHybridCoefficients);
        glWindBarbsShader->setUniformValue(
                    "hybridCoefficientsU",varWindU->textureUnitHybridCoefficients); CHECK_GL_ERROR;
        varWindV->textureHybridCoefficients->bindToTextureUnit(
                    varWindV->textureUnitHybridCoefficients);
        glWindBarbsShader->setUniformValue(
                    "hybridCoefficientsV",varWindV->textureUnitHybridCoefficients);CHECK_GL_ERROR;
    }

    if (varWindU->grid->getLevelType() == AUXILIARY_PRESSURE_3D)
    {
        varWindU->textureAuxiliaryPressure->bindToTextureUnit(
                    varWindU->textureUnitAuxiliaryPressure); CHECK_GL_ERROR;
        glWindBarbsShader->setUniformValue(
                    "auxPressureFieldU_hPa", varWindU->textureUnitAuxiliaryPressure); CHECK_GL_ERROR;

        varWindV->textureAuxiliaryPressure->bindToTextureUnit(
                    varWindV->textureUnitAuxiliaryPressure); CHECK_GL_ERROR;
        glWindBarbsShader->setUniformValue(
                    "auxPressureFieldV_hPa", varWindV->textureUnitAuxiliaryPressure); CHECK_GL_ERROR;
    }

    glWindBarbsShader->setUniformValue(
                "deltaLon", varWindU->grid->getDeltaLon()); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "deltaLat", varWindU->grid->getDeltaLat()); CHECK_GL_ERROR;

    glWindBarbsShader->setUniformValue(
                "pToWorldZParams",
                sceneView->pressureToWorldZParameters()); CHECK_GL_ERROR;

    QVector3D cameraPos = sceneView->getCamera()->getOrigin();
    glWindBarbsShader->setUniformValue(
                "cameraPosition", cameraPos); CHECK_GL_ERROR;

    QVector2D dataSECrnr(varWindU->grid->lons[varWindU->grid->nlons - 1],
                         varWindU->grid->lats[varWindU->grid->nlats - 1]);
    glWindBarbsShader->setUniformValue(
                        "dataSECrnr", dataSECrnr); CHECK_GL_ERROR;

    QVector2D dataNWCrnr(varWindU->grid->lons[0],
                         varWindU->grid->lats[0]);
    glWindBarbsShader->setUniformValue(
                        "dataNWCrnr", dataNWCrnr); CHECK_GL_ERROR;

    // Texture bindings for Lat/Lon axes (1D textures).
    varWindU->textureLonLatLevAxes->bindToTextureUnit(
                varWindU->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "latLonAxesData", varWindU->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "latOffset", GLint(varWindU->grid->nlons)); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "verticalOffset", GLint(varWindU->grid->nlons + varWindU->grid->nlats)); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "levelType", static_cast<GLint>(varWindU->levelType)); CHECK_GL_ERROR;

    glWindBarbsShader->setUniformValue(
                "pressure_hPa", GLfloat(slicePosition_hPa)); CHECK_GL_ERROR;

    glWindBarbsShader->setUniformValue(
                "lineWidth", windBarbsSettings->lineWidth); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "glyphColor", windBarbsSettings->color); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "showCalmGlyph", windBarbsSettings->showCalmGlyphs); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "numFlags", windBarbsSettings->pennantTilt); CHECK_GL_ERROR;

    float scale = 1.;
    float deltaBarbs = 1.;

    if (!windBarbsSettings->automaticScalingEnabled)
    {
        scale = windBarbsSettings->oldScale;
        deltaBarbs = windBarbsSettings->deltaBarbsLonLat;

        // Restrict wind barb distance to grid point spacing? If the barb
        // distance is smaller than the grid point spacing, nearest-neighbour
        // interpolation is used.
        if (windBarbsSettings->clampDeltaBarbsToGrid)
        {
            deltaBarbs = max(deltaBarbs, varWindU->grid->getDeltaLon());
        }
    }
    else
    {
        // Automatic barb scaling.

        // ray definition
        const QVector3D rayDir = sceneView->getCamera()->getZAxis();
        const QVector3D rayOrig = sceneView->getCamera()->getOrigin();

        // plane definition
        const QVector3D planeNormal(0,0,1);
        const float D = -worldZ;

        // compute intersection point between ray and plane
        const float s = -(planeNormal.z() * rayOrig.z() + D) /
                (planeNormal.z() * rayDir.z());
        const QVector3D P = rayOrig + s * rayDir;
        float t = (P - rayOrig).length();

        // quantize distance
        const float step = windBarbsSettings->sensitivity;
        t = step * std::floor(t / step);

        // try to handle camera distance to glyphs via logistical function
        const float c = windBarbsSettings->reduceFactor;
        const float b = windBarbsSettings->reduceSlope;
        const float a = c - 1;
        scale = c / (1.0f + a * std::exp(-b * (t)));

        // quantize scale to get only power-of-two scales
        scale = std::pow(2, std::floor(std::log2(scale) + 0.5));
        //scale = 2 * std::floor(scale / 2);
        scale = clamp(scale, 1.0f, 1024.0f);

        windBarbsSettings->oldScale = scale;
        deltaBarbs = varWindU->grid->getDeltaLon() * scale;
    }

    glWindBarbsShader->setUniformValue("deltaGridX", deltaBarbs); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue("deltaGridY", deltaBarbs); CHECK_GL_ERROR;

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    const QString requestKey = QString("vbo_windbarbs_actor#%1").arg(myID);

    GL::MVertexBuffer* vb = static_cast<GL::MVertexBuffer*>(
                glRM->getGPUItem(requestKey));

    int nBarbsLon = (bBoxConnection->eastWestExtent() / deltaBarbs) + 1;
    int nBarbsLat = (bBoxConnection->northSouthExtent() / deltaBarbs) + 1;
    const GLuint numBarbsTimes2 = nBarbsLon * nBarbsLat * 2;

    // If VBO doesn't exist, create a new one.
    if (!vb)
    {
        GL::MFloatVertexBuffer* newVB = nullptr;
        newVB = new GL::MFloatVertexBuffer(requestKey, numBarbsTimes2);
        if (glRM->tryStoreGPUItem(newVB))
        {
            newVB->upload(nullptr, numBarbsTimes2, sceneView);
        }
        else
        {
            delete newVB; return;
        }

        windBarbsVertexBuffer = static_cast<GL::MVertexBuffer*>(
                    glRM->getGPUItem(requestKey));
    }
    else
    {
        windBarbsVertexBuffer = vb;
    }

    // Generate vertex data (one vertex for each wind barb).
    QVector<float> vertexData(numBarbsTimes2);
    int iVertex = 0;
    for (int i = 0; i < nBarbsLon; i++)
    {
        for (int j = 0; j < nBarbsLat; j++)
        {
            vertexData[iVertex * 2    ] =
                    bBoxConnection->westLon() + i * deltaBarbs;
            vertexData[iVertex * 2 + 1] =
                    bBoxConnection->southLat() + j * deltaBarbs;
            iVertex++;
        }
    }

    // Upload vertex data to GPU and draw barbs.
    GL::MFloatVertexBuffer* buf =
            dynamic_cast<GL::MFloatVertexBuffer*>(windBarbsVertexBuffer);
    buf->reallocate(nullptr, numBarbsTimes2, 0, false, sceneView);
    buf->update(vertexData, 0, 0, sceneView);

    const int VERTEX_ATTRIBUTE = 0;
    windBarbsVertexBuffer->attachToVertexAttribute(VERTEX_ATTRIBUTE, 2);

    glPolygonOffset(.8f, 1.0f); CHECK_GL_ERROR;
    glEnable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
    glPolygonMode(GL_FRONT_AND_BACK,
                  renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;

    glDrawArrays(GL_POINTS, 0, nBarbsLon * nBarbsLat); CHECK_GL_ERROR;

    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    glDisable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
}


void MNWPHorizontalSectionActor::renderShadow(MSceneViewGLWidget* sceneView)
{
    glShadowQuad->bind();

    glShadowQuad->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

    QVector4D corners(bBoxConnection->westLon(), bBoxConnection->southLat(),
                      bBoxConnection->eastWestExtent(),
                      bBoxConnection->northSouthExtent());

    glShadowQuad->setUniformValue("cornersSection", corners);
    glShadowQuad->setUniformValue("colour", shadowColor);
    glShadowQuad->setUniformValue("height", shadowHeight);

    // Draw shadow quad.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); CHECK_GL_ERROR;

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


void MNWPHorizontalSectionActor::renderContourLabels(
        MSceneViewGLWidget *sceneView, MNWP2DHorizontalActorVariable *var)
{
    // Don't render contour labels if horizontal slice position is
    // outside the data domain. Special case: Don't restrict Surface fields
    // to there data volume pressure since this is zero and thus surface
    // fields won't be drawn anymore.
    if (var->grid->getLevelType() != MVerticalLevelType::SURFACE_2D
            && (var->grid->getBottomDataVolumePressure() < slicePosition_hPa
                || var->grid->getTopDataVolumePressure() > slicePosition_hPa))
    {
        return;
    }

//TODO (mr, Feb2015): Labels should not be rendered here but inserted into
//                    actor label pool -- however, that shouldn't be updated
//                    on each render cycle.
//                    This becomes important if global label collision detection
//                    is implemented.
    if (var->renderContourLabels && labelsAreEnabled)
    {
        MTextManager* tm = MGLResourcesManager::getInstance()->getTextManager();

        QList<MLabel*> renderLabels = var->getContourLabels(true, sceneView);
        tm->renderLabelList(sceneView, renderLabels);
    }
}


} // namespace Met3D
