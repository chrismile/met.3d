/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015-2017 Michael Kern
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
      slicePosition_hPa(250.),
      slicePositionGranularity_hPa(5.0),
      slicePosSynchronizationActor(nullptr),
      updateRenderRegion(false),
      vbMouseHandlePoints(nullptr),
      selectedMouseHandle(-1),
      horizontalBBox(QRectF(-60., 30., 100., 40.)),
      differenceMode(0),
      windBarbsVertexBuffer(nullptr),
      windBarbsSettings(),
      renderShadowQuad(true),
      shadowColor(QColor(60,60,60,70)),
      shadowHeight(0.01f)
{
    enablePicking(true);

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Horizontal cross-section");

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
    boundingBoxProperty = addProperty(RECTF_LONLAT_PROPERTY, "bounding box",
                                      actorPropertiesSupGroup);
    properties->setRectF(boundingBoxProperty, horizontalBBox, 2);

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
    graticuleActor = new MGraticuleActor();
    graticuleActor->setName("section graticule");
    graticuleActor->setBBox(horizontalBBox);
    graticuleActor->setVerticalPosition(slicePosition_hPa);
    actorPropertiesSupGroup->addSubProperty(graticuleActor->getPropertyGroup());

    endInitialiseQtProperties();
}


MNWPHorizontalSectionActor::WindBarbsSettings::WindBarbsSettings(
        MNWPHorizontalSectionActor *hostActor)
    : enabled(false),
      automaticEnabled(true),
      oldScale(1),
      lineWidth(0.04),
      numFlags(9),
      color(QColor(0,0,127)),
      showCalmGlyphs(false),
      reduceFactor(15.0f),
      reduceSlope(0.0175f),
      sensibility(1.0f),
      uComponentVarIndex(0),
      vComponentVarIndex(0)
{

    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProperty = a->addProperty(GROUP_PROPERTY, "wind barbs");

    enabledProperty = a->addProperty(BOOL_PROPERTY, "enabled", groupProperty);
    properties->mBool()->setValue(enabledProperty, enabled);

    automaticEnabledProperty = a->addProperty(BOOL_PROPERTY, "automatic scaling",
                                              groupProperty);
    properties->mBool()->setValue(automaticEnabledProperty, automaticEnabled);

    lineWidthProperty = a->addProperty(DOUBLE_PROPERTY, "line width",
                                       groupProperty);
    properties->setDouble(lineWidthProperty, lineWidth, 0.001, 0.30, 3, 0.001);

    numFlagsProperty = a->addProperty(INT_PROPERTY, "num flags", groupProperty);
    properties->setInt(numFlagsProperty, numFlags, 1, 20, 1);

    colorProperty = a->addProperty(COLOR_PROPERTY, "line color", groupProperty);
    properties->mColor()->setValue(colorProperty, color);

    showCalmGlyphsProperty = a->addProperty(BOOL_PROPERTY, "show calm glyphs",
                                            groupProperty);
    properties->mBool()->setValue(showCalmGlyphsProperty, showCalmGlyphs);

    reduceFactorProperty = a->addProperty(DOUBLE_PROPERTY, "reduction factor",
                                          groupProperty);
    properties->setDouble(reduceFactorProperty, reduceFactor, 1., 400., 1, 0.1);

    reduceSlopeProperty = a->addProperty(DOUBLE_PROPERTY, "reduction slope",
                                         groupProperty);
    properties->setDouble(reduceSlopeProperty, reduceSlope, 0.001, 1.0, 4, 0.0001);

    sensibilityProperty = a->addProperty(DOUBLE_PROPERTY, "margin for height",
                                         groupProperty);
    properties->setDouble(sensibilityProperty, sensibility, 1., 200., 1, 1.);

    uComponentVarProperty = a->addProperty(ENUM_PROPERTY, "u-component var",
                                           groupProperty);
    vComponentVarProperty = a->addProperty(ENUM_PROPERTY, "v-component var",
                                           groupProperty);
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


void MNWPHorizontalSectionActor::setBBox(QRectF bbox)
{
    properties->mRectF()->setValue(boundingBoxProperty, bbox);
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
    settings->setValue("boundingBox", horizontalBBox);
    settings->setValue("differenceMode", differenceMode);
    settings->setValue("shadowEnabled", renderShadowQuad);
    settings->setValue("shadowColor", shadowColor);
    settings->setValue("shadowHeight", shadowHeight);

    settings->beginGroup("Windbarbs");

    settings->setValue("enabled", windBarbsSettings->enabled);
    settings->setValue("automatic", windBarbsSettings->automaticEnabled);
    settings->setValue("lineWidth", windBarbsSettings->lineWidth);
    settings->setValue("numFlags", windBarbsSettings->numFlags);
    settings->setValue("color", windBarbsSettings->color);
    settings->setValue("showCalmGlyphs", windBarbsSettings->showCalmGlyphs);
    settings->setValue("reduceFactor", windBarbsSettings->reduceFactor);
    settings->setValue("reduceSlope", windBarbsSettings->reduceSlope);
    settings->setValue("sensibility", windBarbsSettings->sensibility);
    settings->setValue("uComponent", windBarbsSettings->uComponentVarIndex);
    settings->setValue("vComponent", windBarbsSettings->vComponentVarIndex);

    settings->endGroup(); // Windbarbs

    graticuleActor->saveConfiguration(settings);
    settings->endGroup(); // MNWPHorizontalSectionActor
}


void MNWPHorizontalSectionActor::loadConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::loadConfiguration(settings);

    settings->beginGroup(MNWPHorizontalSectionActor::getSettingsID());

    setSlicePosition(settings->value("slicePosition_hPa").toDouble());
    setBBox(settings->value("boundingBox").toRectF());

    properties->mInt()->setValue(differenceModeProperty,
                                 settings->value("differenceMode").toInt());

    properties->mBool()->setValue(shadowEnabledProp,
                                  settings->value("shadowEnabled").toBool());
    properties->mColor()->setValue(shadowColorProp,
                                   settings->value("shadowColor").value<QColor>());
    properties->mBool()->setValue(shadowHeightProp,
                                  settings->value("shadowHeight").toFloat());

    settings->beginGroup("Windbarbs");

    properties->mBool()->setValue(windBarbsSettings->enabledProperty,
                                  settings->value("enabled").toBool());
    properties->mBool()->setValue(windBarbsSettings->automaticEnabledProperty,
                                  settings->value("automatic").toBool());
    properties->mDouble()->setValue(windBarbsSettings->lineWidthProperty,
                                    settings->value("lineWidth").toFloat());
    properties->mInt()->setValue(windBarbsSettings->numFlagsProperty,
                                 settings->value("numFlags").toInt());
    properties->mColor()->setValue(windBarbsSettings->colorProperty,
                                   settings->value("color").value<QColor>());
    properties->mBool()->setValue(windBarbsSettings->showCalmGlyphsProperty,
                                  settings->value("showCalmGlyphs").toBool());
    properties->mDouble()->setValue(windBarbsSettings->reduceFactorProperty,
                                    settings->value("reduceFactor").toFloat());
    properties->mDouble()->setValue(windBarbsSettings->reduceSlopeProperty,
                                    settings->value("reduceSlope").toFloat());
    properties->mDouble()->setValue(windBarbsSettings->sensibilityProperty,
                                    settings->value("sensibility").toFloat());
    properties->mEnum()->setValue(windBarbsSettings->uComponentVarProperty,
                                  settings->value("uComponent").toInt());
    properties->mEnum()->setValue(windBarbsSettings->vComponentVarProperty,
                                  settings->value("vComponent").toInt());

    settings->endGroup(); // Windbarbs

    graticuleActor->loadConfiguration(settings);
    settings->endGroup(); // MNWPHorizontalSectionActor
}


int MNWPHorizontalSectionActor::checkIntersectionWithHandle(
        MSceneViewGLWidget *sceneView,
        float clipX, float clipY,
        float clipRadius)
{
    // First call? Generate positions of corner points.
    if (mouseHandlePoints.size() == 0) updateMouseHandlePositions();

    float clipRadiusSq = clipRadius*clipRadius;

    selectedMouseHandle = -1;

    // Loop over all corner points and check whether the mouse cursor is inside
    // a circle with radius "clipRadius" around the corner point (in clip space).
    for (int i = 0; i < mouseHandlePoints.size(); i++)
    {
        // Transform the corner point coordinate to clip space.
        QVector3D pClip = sceneView->lonLatPToClipSpace(mouseHandlePoints.at(i));

        float dx = pClip.x() - clipX;
        float dy = pClip.y() - clipY;

        // Compute the distance between point and mouse in clip space. If it
        // is less than clipRadius return one.
        if ( (dx*dx + dy*dy) < clipRadiusSq )
        {
            selectedMouseHandle = i;
            break;
        }
    }

    return selectedMouseHandle;
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

    // Transform world space Z to pressure (hPa) and round off to match
    // granularity requested by used.
    double p_hPa = sceneView->pressureFromWorldZ(mousePosWorldSpace.z());
    p_hPa = p_hPa - fmod(p_hPa, slicePositionGranularity_hPa);

    // Set slice position to new pressure elevation.
    setSlicePosition(p_hPa);
}


const QList<MVerticalLevelType> MNWPHorizontalSectionActor::supportedLevelTypes()
{
    return (QList<MVerticalLevelType>()
            << HYBRID_SIGMA_PRESSURE_3D
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
    if (MNWPMultiVarActor::isConnectedTo(actor)) return true;
    if (slicePosSynchronizationActor == actor) return true;

    return false;
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MNWPHorizontalSectionActor::setSlicePosition(double pressure_hPa)
{
    properties->mDDouble()->setValue(slicePosProperty, pressure_hPa);

    emit slicePositionChanged(pressure_hPa);
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

        if (suppressActorUpdates()) return;

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
            disconnect(slicePosSynchronizationActor,
                       SIGNAL(slicePositionChanged(double)),
                       this, SLOT(setSlicePosition(double)));

        // Get pointer to new synchronization actor and connect to signal.
        MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
        slicePosSynchronizationActor = dynamic_cast<MNWPHorizontalSectionActor*>(
                    glRM->getActorByName(hsecName));

        if (slicePosSynchronizationActor != nullptr)
            connect(slicePosSynchronizationActor,
                    SIGNAL(slicePositionChanged(double)),
                    this, SLOT(setSlicePosition(double)));
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

    else if (property == boundingBoxProperty)
    {
        horizontalBBox = properties->mRectF()->value(boundingBoxProperty);
        if (suppressActorUpdates()) return;

        // The bbox position has changed. In the next render cycle, update the
        // render region, download target grid from GPU and update contours.
        computeRenderRegionParameters();
        updateMouseHandlePositions();
        crossSectionGridsNeedUpdate = true;
        emitActorChangedSignal();
    }

    else if (property == differenceModeProperty)
    {
        differenceMode = properties->mEnum()->value(differenceModeProperty);
        crossSectionGridsNeedUpdate = true;
        emitActorChangedSignal();
    }

    else if (property == windBarbsSettings->enabledProperty ||
             property == windBarbsSettings->automaticEnabledProperty ||
             property == windBarbsSettings->lineWidthProperty ||
             property == windBarbsSettings->numFlagsProperty ||
             property == windBarbsSettings->colorProperty ||
             property == windBarbsSettings->showCalmGlyphsProperty ||
             property == windBarbsSettings->reduceFactorProperty ||
             property == windBarbsSettings->reduceSlopeProperty ||
             property == windBarbsSettings->sensibilityProperty ||
             property == windBarbsSettings->uComponentVarProperty ||
             property == windBarbsSettings->vComponentVarProperty)
    {
        windBarbsSettings->enabled = properties->mBool()
                ->value(windBarbsSettings->enabledProperty);
        windBarbsSettings->automaticEnabled = properties->mBool()
                ->value(windBarbsSettings->automaticEnabledProperty);
        windBarbsSettings->lineWidth = properties->mDouble()
                ->value(windBarbsSettings->lineWidthProperty);
        windBarbsSettings->numFlags = properties->mInt()
                ->value(windBarbsSettings->numFlagsProperty);
        windBarbsSettings->color = properties->mColor()
                ->value(windBarbsSettings->colorProperty);
        windBarbsSettings->showCalmGlyphs = properties->mBool()
                ->value(windBarbsSettings->showCalmGlyphsProperty);
        windBarbsSettings->reduceFactor = properties->mDouble()
                ->value(windBarbsSettings->reduceFactorProperty);
        windBarbsSettings->reduceSlope = properties->mDouble()
                ->value(windBarbsSettings->reduceSlopeProperty);
        windBarbsSettings->sensibility = properties->mDouble()
                ->value(windBarbsSettings->sensibilityProperty);
        windBarbsSettings->uComponentVarIndex = properties->mEnum()
                ->value(windBarbsSettings->uComponentVarProperty);
        windBarbsSettings->vComponentVarIndex = properties->mEnum()
                ->value(windBarbsSettings->vComponentVarProperty);

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

        // If the bounding box is outside the model grid domain, there is
        // nothing to render (see computeRenderRegionParameters()).
        if (var->nlons == 0 || var->nlats == 0) continue;

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
                renderVerticalInterpolationDifference(var, varDiff);
            }
            else
                renderVerticalInterpolation(var);

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
                    "radius",
                    GLfloat(0.5));
        positionSpheresShader->setUniformValue(
                    "scaleRadius",
                    GLboolean(true));

        positionSpheresShader->setUniformValue(
                    "useTransferFunction", GLboolean(false));
        positionSpheresShader->setUniformValue(
                    "constColour", QColor(Qt::white));

        vbMouseHandlePoints->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

        if (selectedMouseHandle >= 0)
        {
            positionSpheresShader->setUniformValue(
                        "constColour", QColor(Qt::red));
        }

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
    llcrnrlat = horizontalBBox.y();
    llcrnrlon = horizontalBBox.x();
    urcrnrlat = horizontalBBox.y() + horizontalBBox.height();
    urcrnrlon = horizontalBBox.x() + horizontalBBox.width();

    // Compute render region parameters for each variable.
    for (int vi = 0; vi < variables.size(); vi++)
    {
        MNWP2DHorizontalActorVariable* var =
                static_cast<MNWP2DHorizontalActorVariable*> (variables.at(vi));

        var->computeRenderRegionParameters(llcrnrlon, llcrnrlat,
                                           urcrnrlon, urcrnrlat);
    }

    // Pass the new bbox on this hsec's graticule actor. Disable redrawing, as
    // the scene will be redrawn after this function is completed.
    graticuleActor->enableEmissionOfActorChangedSignal(false);
    graticuleActor->setBBox(horizontalBBox);
    graticuleActor->enableEmissionOfActorChangedSignal(true);

    // The label displaying the current pressure elevation needs to be put
    // at a new place.
    updateDescriptionLabel();
}


void MNWPHorizontalSectionActor::updateDescriptionLabel(bool deleteOldLabel)
{
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
                      llcrnrlon, urcrnrlat, slicePosition_hPa,
                      labelsize, labelColour, MTextManager::BASELINELEFT,
                      labelbbox, labelBBoxColour, 0.3)
                  );
}


void MNWPHorizontalSectionActor::updateMouseHandlePositions()
{
    mouseHandlePoints.clear();

    mouseHandlePoints.append(
            QVector3D(horizontalBBox.x(),
                      horizontalBBox.y(),
                      slicePosition_hPa));
    mouseHandlePoints.append(
            QVector3D(horizontalBBox.right(),
                      horizontalBBox.y(),
                      slicePosition_hPa));
    mouseHandlePoints.append(
            QVector3D(horizontalBBox.right(),
                      horizontalBBox.y() + horizontalBBox.height(),
                      slicePosition_hPa));
    mouseHandlePoints.append(
            QVector3D(horizontalBBox.x(),
                      horizontalBBox.y() + horizontalBBox.height(),
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

    glDrawArraysInstanced(GL_POINTS,
                          0, var->nlons, var->nlats); CHECK_GL_ERROR;
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

    glDrawArraysInstanced(GL_POINTS,
                          0, var->nlons, var->nlats); CHECK_GL_ERROR;
}


void MNWPHorizontalSectionActor::renderFilledContours(
        MSceneViewGLWidget *sceneView, MNWP2DHorizontalActorVariable *var)
{
    // Abort rendering if transfer function is not defined.
    if (var->transferFunction == nullptr) return;

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
                "bboxLons", QVector2D(llcrnrlon, urcrnrlon)); CHECK_GL_ERROR;

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
    if (var->renderSettings.contoursUseTF)
    {
        glMarchingSquaresShader->bindProgram("TransferFunction");

        // Texture bindings for transfer function for data field (1D texture from
        // transfer function class).
        if (var->transferFunction != 0)
        {
            var->transferFunction->getTexture()->bindToTextureUnit(
                        var->textureUnitTransferFunction);
            glMarchingSquaresShader->setUniformValue(
                        "transferFunction",
                        var->textureUnitTransferFunction); CHECK_GL_ERROR;
            glMarchingSquaresShader->setUniformValue(
                        "scalarMinimum",
                        var->transferFunction->getMinimumValue()); CHECK_GL_ERROR;
            glMarchingSquaresShader->setUniformValue(
                        "scalarMaximum",
                        var->transferFunction->getMaximumValue()); CHECK_GL_ERROR;
        }
        // Don't draw anything if no transfer function is present.
        else
        {
            return;
        }
    }
    else
    {
        glMarchingSquaresShader->bindProgram("Standard");
    }

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
                "bboxLons", QVector2D(llcrnrlon, urcrnrlon)); CHECK_GL_ERROR;

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
            glLineWidth(contourSet.thickness); CHECK_GL_ERROR;
            if (!var->renderSettings.contoursUseTF)
            {
                glMarchingSquaresShader->setUniformValue(
                            "colour", contourSet.colour); CHECK_GL_ERROR;
            }
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
                "height", GLfloat(horizontalBBox.height())); CHECK_GL_ERROR;

    glBindImageTexture(var->imageUnitTargetGrid, // image unit
                       var->textureTargetGrid->getTextureObject(),
                                                 // texture object
                       0,                        // level
                       GL_FALSE,                 // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       GL_R32F); CHECK_GL_ERROR; // format
    // TODO:
//    glBindImageTexture(var->imageUnitTargetGrid, // image unit
//                       var->textureTargetGrid->getTextureObject(),
//                                                 // texture object
//                       0,                        // level
//                       GL_FALSE,                 // layered
//                       0,                        // layer
//                       GL_READ_WRITE,            // shader access
//                       // GL_WRITE_ONLY,         // shader access
//                       GL_RG32F); CHECK_GL_ERROR; // format

    glTexturedContoursShader->setUniformValue(
                "crossSectionGrid", GLint(var->imageUnitTargetGrid)); CHECK_GL_ERROR;

    // Grid offsets to render only the requested subregion.
    glTexturedContoursShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
    glTexturedContoursShader->setUniformValue(
                "bboxLons", QVector2D(llcrnrlon, urcrnrlon)); CHECK_GL_ERROR;

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
    glWindBarbsShader->bind();

    if (windBarbsSettings->vComponentVarIndex >= variables.size() ||
        windBarbsSettings->vComponentVarIndex < 0 ||
        windBarbsSettings->uComponentVarIndex >= variables.size() ||
        windBarbsSettings->uComponentVarIndex < 0) { return; }

    // assume that the last two variables are the wind components
    MNWPActorVariable *windV = variables.at(windBarbsSettings->vComponentVarIndex);
    MNWPActorVariable *windU = variables.at(windBarbsSettings->uComponentVarIndex);

    if (windV->grid->getLevelType() != windU->grid->getLevelType())
    {
        LOG4CPLUS_WARN(mlog, "WARNING: Wind barbs u and v variables must have "
                       "the same vertical level type. Disabling wind barbs.");
        return;
    }

    if (windV->grid->getLevelType() == SURFACE_2D)
    {
        LOG4CPLUS_WARN(mlog, "WARNING: Wind barbs have not been implemented for "
                       "2D surface fields. Disabling wind barbs.");
        return;
    }

    // collect infos of data
    const int widthX = std::floor(std::abs(urcrnrlon - llcrnrlon) / windU->grid->getDeltaLon());
    const int widthY = std::floor(std::abs(urcrnrlat - llcrnrlat) / windU->grid->getDeltaLat());

    const int resLon = windU->grid->nlons;
    const int resLat = windU->grid->nlats;

    // compute current boundary indices in grid
    int minX = static_cast<int>((llcrnrlon - windU->grid->lons[0]) / windU->grid->getDeltaLon()) % 360;
    int maxX = minX + widthX;

    int minY = static_cast<int>((windU->grid->lats[0] - urcrnrlat) / windU->grid->getDeltaLat());
    int maxY = minY + widthY;

    minX = min(max(0, minX), resLon - 1);
    minY = min(max(0, minY), resLat - 1);
    maxX = max(min(maxX, resLon - 1), minX);
    maxY = max(min(maxY, resLat - 1), minY);

    const GLfloat worldZ = sceneView->worldZfromPressure(slicePosition_hPa) + 0.1;

    const GLfloat lowerX = windU->grid->lons[minX];
    const GLfloat lowerY = windU->grid->lats[maxY];


    // Reset optional required textures (to avoid draw errors).
    // ========================================================

    windU->textureDummy1D->bindToTextureUnit(windU->textureUnitUnusedTextures);
    glWindBarbsShader->setUniformValue(
                "hybridCoefficientsU", windU->textureUnitUnusedTextures); CHECK_GL_ERROR;
    windV->textureDummy1D->bindToTextureUnit(windV->textureUnitUnusedTextures);
    glWindBarbsShader->setUniformValue(
                "hybridCoefficientsV", windV->textureUnitUnusedTextures); CHECK_GL_ERROR;

    windU->textureDummy2D->bindToTextureUnit(windU->textureUnitUnusedTextures);
    glWindBarbsShader->setUniformValue(
                "surfacePressureU", windU->textureUnitUnusedTextures); CHECK_GL_ERROR;
    windV->textureDummy2D->bindToTextureUnit(windV->textureUnitUnusedTextures);
    glWindBarbsShader->setUniformValue(
                "surfacePressureV", windV->textureUnitUnusedTextures); CHECK_GL_ERROR;

    // Set shader variables.
    // =====================

    glWindBarbsShader->setUniformValue(
                "mvpMatrix",
                *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;

    glWindBarbsShader->setUniformValue(
                "worldZ", worldZ); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "bboxll", QVector2D(lowerX, lowerY)); CHECK_GL_ERROR;

    windU->textureDataField->bindToTextureUnit(
                windU->textureUnitDataField);
    glWindBarbsShader->setUniformValue(
                "dataUComp",windU->textureUnitDataField); CHECK_GL_ERROR;
    windV->textureDataField->bindToTextureUnit(
                windV->textureUnitDataField);
    glWindBarbsShader->setUniformValue(
                "dataVComp",windV->textureUnitDataField); CHECK_GL_ERROR;

    if (windU->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
    {
        windU->textureSurfacePressure->bindToTextureUnit(
                    windU->textureUnitSurfacePressure);
        glWindBarbsShader->setUniformValue(
                    "surfacePressureU",windU->textureUnitSurfacePressure); CHECK_GL_ERROR;
        windV->textureSurfacePressure->bindToTextureUnit(
                    windV->textureUnitSurfacePressure);
        glWindBarbsShader->setUniformValue(
                    "surfacePressureV",windV->textureUnitSurfacePressure); CHECK_GL_ERROR;
        windU->textureHybridCoefficients->bindToTextureUnit(
                    windU->textureUnitHybridCoefficients);
        glWindBarbsShader->setUniformValue(
                    "hybridCoefficientsU",windU->textureUnitHybridCoefficients); CHECK_GL_ERROR;
        windV->textureHybridCoefficients->bindToTextureUnit(
                    windV->textureUnitHybridCoefficients);
        glWindBarbsShader->setUniformValue(
                    "hybridCoefficientsV",windV->textureUnitHybridCoefficients);CHECK_GL_ERROR;
    }

    glWindBarbsShader->setUniformValue(
                "deltaLon", windU->grid->getDeltaLon()); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "deltaLat", windU->grid->getDeltaLat()); CHECK_GL_ERROR;

    glWindBarbsShader->setUniformValue(
                "pToWorldZParams",
                sceneView->pressureToWorldZParameters()); CHECK_GL_ERROR;

    QVector3D cameraPos = sceneView->getCamera()->getOrigin();

    glWindBarbsShader->setUniformValue(
                "cameraPosition", cameraPos); CHECK_GL_ERROR;

    QVector2D dataSECrnr(windU->grid->lons[windU->grid->nlons - 1],
                         windU->grid->lats[windU->grid->nlats - 1]);

    glWindBarbsShader->setUniformValue(
                        "dataSECrnr", dataSECrnr); CHECK_GL_ERROR;

    QVector2D dataNWCrnr(windU->grid->lons[0],
                         windU->grid->lats[0]);

    glWindBarbsShader->setUniformValue(
                        "dataNWCrnr", dataNWCrnr); CHECK_GL_ERROR;

    // Texture bindings for Lat/Lon axes (1D textures).
    windU->textureLonLatLevAxes->bindToTextureUnit(
                windU->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "latLonAxesData", windU->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "latOffset", GLint(windU->grid->nlons)); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "verticalOffset", GLint(windU->grid->nlons + windU->grid->nlats)); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "levelType", static_cast<GLint>(windU->levelType)); CHECK_GL_ERROR;

    glWindBarbsShader->setUniformValue(
                "pressure_hPa", GLfloat(slicePosition_hPa)); CHECK_GL_ERROR;

    glWindBarbsShader->setUniformValue(
                "lineWidth", windBarbsSettings->lineWidth); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "glyphColor", windBarbsSettings->color); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "showCalmGlyph", windBarbsSettings->showCalmGlyphs); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue(
                "numFlags", windBarbsSettings->numFlags); CHECK_GL_ERROR;

    float scale = 1;

    // handle automatic resolution adaption
    if (!windBarbsSettings->automaticEnabled && windBarbsSettings->oldScale > 0)
    {
        scale = windBarbsSettings->oldScale;
    }
    else
    {
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
        const float step = windBarbsSettings->sensibility;
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
    }

    const float deltaGlyph = windU->grid->getDeltaLon() * scale;

    const int width = maxX - minX + 1;
    const int height = maxY - minY + 1;

    const int resLons = static_cast<int>(std::ceil(width / scale));
    const int resLats = static_cast<int>(std::ceil(height / scale));

    glWindBarbsShader->setUniformValue("deltaGridX", deltaGlyph); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue("deltaGridY", deltaGlyph); CHECK_GL_ERROR;
    glWindBarbsShader->setUniformValue("width", resLons); CHECK_GL_ERROR;

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    const QString requestKey = QString("vbo_windbarbs_actor#%1").arg(myID);

    GL::MVertexBuffer* vb = static_cast<GL::MVertexBuffer*>(
                                            glRM->getGPUItem(requestKey));

    const GLuint NUM_VERTICES = resLons * resLats * 2;

    // create VBO if not existed
    if (!vb)
    {
        GL::MFloatVertexBuffer* newVB = nullptr;
        newVB = new GL::MFloatVertexBuffer(requestKey, NUM_VERTICES);
        if (glRM->tryStoreGPUItem(newVB))
        {
            newVB->upload(nullptr, NUM_VERTICES, sceneView);
        }
        else
        {
            delete newVB; return;
        }

        windBarbsVertexBuffer = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(requestKey));

    }
    else
    {
        windBarbsVertexBuffer = vb;
    }

    QVector<float> vertexData(NUM_VERTICES);

    // compute positions on CPU
    // as unfortunately on GPU some errors occured
    for(int i = 0; i < resLons * resLats; ++i)
    {
        const int idX = i % resLons;
        const int idY = i / resLons;

        vertexData[i * 2] = lowerX + idX * deltaGlyph;
        vertexData[i * 2 + 1] = lowerY + idY * deltaGlyph;
    }

    GL::MFloatVertexBuffer* buf =
            dynamic_cast<GL::MFloatVertexBuffer*>(windBarbsVertexBuffer);
    buf->reallocate(nullptr, NUM_VERTICES, 0, false, sceneView);
    buf->update(vertexData, 0, 0, sceneView);

    const int VERTEX_ATTRIBUTE = 0;
    windBarbsVertexBuffer->attachToVertexAttribute(VERTEX_ATTRIBUTE, 2);


    glPolygonOffset(.8f, 1.0f); CHECK_GL_ERROR;
    glEnable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
    glPolygonMode(GL_FRONT_AND_BACK,
                  renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
    //glLineWidth(1); CHECK_GL_ERROR;
    //glBindBuffer(GL_ARRAY_BUFFER, windBarbsVBO);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDrawArrays(GL_POINTS, 0, resLons * resLats); CHECK_GL_ERROR;

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisable(GL_POLYGON_OFFSET_FILL);
}


void MNWPHorizontalSectionActor::renderShadow(MSceneViewGLWidget* sceneView)
{
    glShadowQuad->bind();

    glShadowQuad->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

    QVector4D corners(horizontalBBox.x(), horizontalBBox.y(),
                      horizontalBBox.width(), horizontalBBox.height());

    glShadowQuad->setUniformValue("cornersSection", corners);
    glShadowQuad->setUniformValue("colour", shadowColor);
    glShadowQuad->setUniformValue("height", shadowHeight);

    // Draw shadow quad.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); CHECK_GL_ERROR;

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


void MNWPHorizontalSectionActor::renderContourLabels(MSceneViewGLWidget *sceneView, MNWP2DHorizontalActorVariable *var)
{
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
