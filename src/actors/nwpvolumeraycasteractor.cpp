/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Michael Kern
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
#include "nwpvolumeraycasteractor.h"

// standard library imports
#include <iostream>
#include <cmath>
#include <random>
#include <array>

// third party tools
#include <QObject>
#include <log4cplus/loggingmacros.h>

// local imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "data/structuredgrid.h"
#include "gxfw/synccontrol.h"
#include "gxfw/selectdatasourcedialog.h"
#include "gxfw/gl/typedvertexbuffer.h"


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNWPVolumeRaycasterActor::MNWPVolumeRaycasterActor()
    : MNWPMultiVarActor(),
      updateNextRenderFrame("111"),
      renderMode(RenderMode::Original),
      variableIndex(0),
      var(nullptr),
      shadingVariableIndex(0),
      shadingVar(nullptr),
      gl(), // initialize gl objects
      normalCurveNumVertices(0),
      normalCurveInitPoints()
{
    // Enable picking for the scene view's analysis mode. See
    // triggerAnalysisOfObjectAtPos().
    enablePicking(true);

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Volume raycaster");

    QStringList modesLst;
    modesLst << "standard" << "bitfield";
    renderModeProp = addProperty(ENUM_PROPERTY, "render mode",
                                 actorPropertiesSupGroup);
    properties->mEnum()->setEnumNames(renderModeProp, modesLst);
    properties->mEnum()->setValue(renderModeProp, static_cast<int>(renderMode));

    variableIndexProp = addProperty(ENUM_PROPERTY, "observed variable",
                                    actorPropertiesSupGroup);

    shadingVariableIndexProp = addProperty(ENUM_PROPERTY, "shading variable",
                                           actorPropertiesSupGroup);

    // Bounding box.
    bbSettings = new BoundingBoxSettings(this);
    actorPropertiesSupGroup->addSubProperty(bbSettings->groupProp);

    // Isosurface lighting.
    lightingSettings = new LightingSettings(this);
    actorPropertiesSupGroup->addSubProperty(lightingSettings->groupProp);

    // Raycaster.
    rayCasterSettings = new RayCasterSettings(this);
    actorPropertiesSupGroup->addSubProperty(rayCasterSettings->groupProp);

    // Normal curves.
    normalCurveSettings = new NormalCurveSettings(this);
    actorPropertiesSupGroup->addSubProperty(normalCurveSettings->groupProp);

    endInitialiseQtProperties();
}


MNWPVolumeRaycasterActor::OpenGL::OpenGL()
    : rayCasterEffect(nullptr),
      bitfieldRayCasterEffect(nullptr),
      boundingBoxShader(nullptr),
      shadowImageRenderShader(nullptr),
      normalCurveInitPointsShader(nullptr),
      normalCurveLineComputeShader(nullptr),
      normalCurveGeometryEffect(nullptr),

      vboBoundingBox(nullptr),
      iboBoundingBox(0),
      vboPositionCross(nullptr),
      vboShadowImageRender(nullptr),
      vboShadowImage(nullptr),
      vboInitPoints(nullptr),
      ssboNormalCurves(0),

      tex2DShadowImage(nullptr),
      texUnitShadowImage(-1),
      tex2DDepthBuffer(0),
      texUnitDepthBuffer(-1)
{
}


MNWPVolumeRaycasterActor::BoundingBoxSettings::BoundingBoxSettings(
        MNWPVolumeRaycasterActor *hostActor)
    : pBot_hPa(1050.),
      pTop_hPa(100.)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProp = a->addProperty(GROUP_PROPERTY, "bounding box");

    boxCornersProp = a->addProperty(RECTF_PROPERTY, "corners", groupProp);
    properties->setRectF(boxCornersProp, QRectF(-60., 30., 100., 40.), 2);

    pBotProp = a->addProperty(DOUBLE_PROPERTY, "bottom pressure", groupProp);
    properties->setDouble(pBotProp, pBot_hPa, 1050., 20., 1, 5.);

    pTopProp = a->addProperty(DOUBLE_PROPERTY, "top pressure", groupProp);
    properties->setDouble(pTopProp, pTop_hPa, 1050., 20., 1, 5.);
}


MNWPVolumeRaycasterActor::LightingSettings::LightingSettings(
        MNWPVolumeRaycasterActor *hostActor)
    : lightingMode(0),
      ambient(0.2),
      diffuse(0.6),
      specular(0.2),
      shininess(10),
      shadowColor(QColor(70, 70, 70))
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProp = a->addProperty(GROUP_PROPERTY, "lighting");

    QStringList modesLst;
    modesLst << "double-sided" << "single-sided" << "single-sided + headlight";
    lightingModeProp = a->addProperty(ENUM_PROPERTY, "lighting mode", groupProp);
    properties->mEnum()->setEnumNames(lightingModeProp, modesLst);
    properties->mEnum()->setValue(lightingModeProp, lightingMode);

    ambientProp = a->addProperty(DOUBLE_PROPERTY, "ambient", groupProp);
    properties->setDouble(ambientProp, ambient, 0.0, 1.0, 2, 0.01);

    diffuseProp = a->addProperty(DOUBLE_PROPERTY, "diffuse", groupProp);
    properties->setDouble(diffuseProp, diffuse, 0.0, 1.0, 2, 0.01);

    specularProp = a->addProperty(DOUBLE_PROPERTY, "specular", groupProp);
    properties->setDouble(specularProp, specular, 0.0, 1.0, 2, 0.01);

    shininessProp = a->addProperty(DOUBLE_PROPERTY, "shininess", groupProp);
    properties->setDouble(shininessProp, shininess, 0.0, 100.0, 3, 1.);

    shadowColorProp = a->addProperty(COLOR_PROPERTY, "shadow color", groupProp);
    properties->mColor()->setValue(shadowColorProp, shadowColor);
}


MNWPVolumeRaycasterActor::IsoValueSettings::IsoValueSettings(
        MNWPVolumeRaycasterActor *hostActor,
        const uint8_t index,
        bool _enabled,
        float _isoValue,
        QColor _isoColor,
        IsoValueSettings::ColorType _colorType)
    : enabled(_enabled),
      isoValue(_isoValue),
      isoColour(_isoColor),
      isoColourType(_colorType)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    a->beginInitialiseQtProperties();

    QString propTitle = QString("isovalue #%1").arg(index);
    groupProp = a->addProperty(GROUP_PROPERTY, propTitle);

    enabledProp = a->addProperty(BOOL_PROPERTY, "enabled", groupProp);
    properties->mBool()->setValue(enabledProp, enabled);

    isoValueProp = a->addProperty(DOUBLE_PROPERTY, "isovalue", groupProp);
    properties->setDouble(isoValueProp, isoValue, 6, 0.01);

    QStringList modesLst;
    modesLst.clear();
    modesLst << "constant colour"
             << "transfer function (observed variable)"
             << "transfer function (shading variable)"
             << "transfer function (max. neighbour shading variable)";
    isoColourTypeProp = a->addProperty(ENUM_PROPERTY, "colour mode", groupProp);
    properties->mEnum()->setEnumNames(isoColourTypeProp, modesLst);
    properties->mEnum()->setValue(isoColourTypeProp,
                                  static_cast<int>(isoColourType));

    isoColourProp = a->addProperty(COLOR_PROPERTY, "constant colour", groupProp);
    properties->mColor()->setValue(isoColourProp, isoColour);

    a->endInitialiseQtProperties();
}


const int MAX_ISOSURFACES = 10;

MNWPVolumeRaycasterActor::RayCasterSettings::RayCasterSettings(
        MNWPVolumeRaycasterActor *hostActor)
    : hostActor(hostActor),
      numIsoValues(2),
      numEnabledIsoValues(0),
      isoValueSetList(),
      isoEnabled(),
      isoValues(),
      isoColors(),
      isoColorTypes(),
      stepSize(0.1),
      interactionStepSize(1.0),
      bisectionSteps(4),
      interactionBisectionSteps(4),
      shadowMode(RenderMode::ShadowMap),
      shadowsResolution(RenderMode::LowRes)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProp = a->addProperty(GROUP_PROPERTY, "raycaster");

    numIsoValuesProp = a->addProperty(INT_PROPERTY, "num isovalues", groupProp);
    properties->setInt(numIsoValuesProp, numIsoValues, 1, MAX_ISOSURFACES, 1);

    isoValuesProp = a->addProperty(GROUP_PROPERTY, "isovalues", groupProp);

    isoEnabled.reserve(MAX_ISOSURFACES);
    isoValues.reserve(MAX_ISOSURFACES);
    isoColors.reserve(MAX_ISOSURFACES);
    isoColorTypes.reserve(MAX_ISOSURFACES);

    // Default isosurface settings.
    addIsoValue(1, true, false, 60);
    addIsoValue(2, true, false, 45, QColor(255,255,255,150));

    // Already create properties for remaining MAX_ISOSURFACES isovalue
    // settings (dynamic creation of new properties very expensive!).
    for (int i = numIsoValues; i < int(MAX_ISOSURFACES); ++i)
    {
        addIsoValue(i + 1, false, true);
    }

    // Sort isovalues to ensure correct visualization via crossing levels.
    sortIsoValues();

    stepSizeProp = a->addProperty(DOUBLE_PROPERTY, "step size", groupProp);
    properties->setDouble(stepSizeProp, stepSize, 0.001, 10.0, 3, 0.01);

    interactionStepSizeProp = a->addProperty(
                DOUBLE_PROPERTY, "interaction step size", groupProp);
    properties->setDouble(interactionStepSizeProp, interactionStepSize,
                          0.001, 10.0, 3, 0.1);

    bisectionStepsProp = a->addProperty
            (INT_PROPERTY, "bisection steps", groupProp);
    properties->setInt(bisectionStepsProp, bisectionSteps, 0, 20);

    interactionBisectionStepsProp = a->addProperty(
                INT_PROPERTY, "interaction bisection steps", groupProp);
    properties->setInt(interactionBisectionStepsProp,
                       interactionBisectionSteps, 0, 20);

    QStringList shadowModesList;
    shadowModesList << "off" << "shadow map" << "shadow ray";
    shadowModeProp = a->addProperty(ENUM_PROPERTY, "shadows", groupProp);
    properties->mEnum()->setEnumNames(shadowModeProp, shadowModesList);
    properties->mEnum()->setValue(shadowModeProp, static_cast<int>(shadowMode));

    QStringList modesLst;
    modesLst << "very low (0.5K)" << "low (1K)" << "normal (2K)" << "high (4K)"
             << "very high (8K)" << "maximum (16K)";
    shadowsResolutionProp = a->addProperty(
                ENUM_PROPERTY, "shadow map resolution", groupProp);
    properties->mEnum()->setEnumNames(shadowsResolutionProp, modesLst);
    properties->mEnum()->setValue(shadowsResolutionProp,
                                  static_cast<int>(shadowsResolution));
}


void MNWPVolumeRaycasterActor::RayCasterSettings::addIsoValue(
        const int index, const bool enabled, const bool hidden,
        const float isoValue, const QColor color,
        const IsoValueSettings::ColorType colorType)
{
    IsoValueSettings isoSettings(hostActor, index, enabled, isoValue,
                                 color, colorType);
    isoValueSetList.push_back(isoSettings);
    isoEnabled.push_back(isoSettings.enabled);
    isoValues.push_back(isoSettings.isoValue);
    QVector4D vecColor = QVector4D(isoSettings.isoColour.redF(),
                                   isoSettings.isoColour.greenF(),
                                   isoSettings.isoColour.blueF(),
                                   isoSettings.isoColour.alphaF());
    isoColors.push_back(vecColor);
    isoColorTypes.push_back(static_cast<GLint>(isoSettings.isoColourType));
    if (!hidden)
    {
        isoValuesProp->addSubProperty(isoValueSetList[index - 1].groupProp);
    }
}


void MNWPVolumeRaycasterActor::RayCasterSettings::sortIsoValues()
{
    numEnabledIsoValues = 0;

    for (int i = 0; i < int(MAX_ISOSURFACES); ++i)
    {
        isoEnabled[i] = static_cast<int>(isoValueSetList[i].enabled);
        if (isoValueSetList[i].enabled)
        {
            isoValues[i] = isoValueSetList[i].isoValue;
            numEnabledIsoValues++;
        }
        else
        {
            isoValues[i] = numeric_limits<float>::max();
        }
        isoColors[i].setX(isoValueSetList[i].isoColour.redF());
        isoColors[i].setY(isoValueSetList[i].isoColour.greenF());
        isoColors[i].setZ(isoValueSetList[i].isoColour.blueF());
        isoColors[i].setW(isoValueSetList[i].isoColour.alphaF());
        isoColorTypes[i] = static_cast<GLint>(isoValueSetList[i].isoColourType);
    }

    for (int i = 1; i < int(MAX_ISOSURFACES); ++i)
    {
        bool currEnabled = isoEnabled[i];
        GLfloat currIsoValue = isoValues[i];
        QVector4D currIsoColor = isoColors[i];
        GLint currIsoColorType = isoColorTypes[i];

        int c = i - 1;
        for (; c >= 0 && currIsoValue < isoValues[c]; --c)
        {
            isoEnabled[c + 1] = isoEnabled[c];
            isoValues[c + 1] = isoValues[c];
            isoColors[c + 1] = isoColors[c];
            isoColorTypes[c + 1] = isoColorTypes[c];
        }

        isoEnabled[c + 1] = currEnabled;
        isoValues[c + 1] = currIsoValue;
        isoColors[c + 1] = currIsoColor;
        isoColorTypes[c + 1] = currIsoColorType;
    }
}


MNWPVolumeRaycasterActor::NormalCurveSettings::NormalCurveSettings(
        MNWPVolumeRaycasterActor *hostActor)
    : normalCurvesEnabled(false),
      glyph(GlyphType::Tube),
      threshold(Threshold::IsoValueInner),
      colour(CurveColor::ColorIsoValue),
      surface(Surface::Outer),
      stepSize(0.1),
      integrationDir(IntegrationDir::Forwards),
      numLineSegments(100),
      initPointResX(0.5),
      initPointResY(0.5),
      initPointResZ(0.5),
      initPointVariance(0.45),
      numSteps(1),
      curveLength(1),
      isoValueBorder(75)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProp = a->addProperty(GROUP_PROPERTY, "normal curve settings");

    normalCurvesEnabledProp = a->addProperty(BOOL_PROPERTY, "enabled", groupProp);
    properties->mBool()->setValue(normalCurvesEnabledProp, normalCurvesEnabled);

    QStringList modesLst;
    modesLst << "lines" << "boxes + slices" << "tubes";
    glyphProp = a->addProperty(ENUM_PROPERTY, "glyph type", groupProp);
    properties->mEnum()->setEnumNames(glyphProp, modesLst);
    properties->mEnum()->setValue(glyphProp, static_cast<int>(glyph));

    modesLst.clear();
    modesLst << "number of steps" << "curve length"
             << "isovalue border" << "isovalue outer" << "isovalue inner";
    thresholdProp = a->addProperty(ENUM_PROPERTY, "stop criterion", groupProp);
    properties->mEnum()->setEnumNames(thresholdProp, modesLst);
    properties->mEnum()->setValue(thresholdProp, static_cast<int>(threshold));

    modesLst.clear();
    modesLst << "ratio steps/max steps" << "ratio curve length/max length"
             << "transfer function (observed variable)";
    colourProp = a->addProperty(ENUM_PROPERTY, "curve colour", groupProp);
    properties->mEnum()->setEnumNames(colourProp, modesLst);
    properties->mEnum()->setValue(colourProp, static_cast<int>(colour));

    modesLst.clear();
    modesLst << "inner" << "outer";
    surfaceProp = a->addProperty(ENUM_PROPERTY, "start isosurface", groupProp);
    properties->mEnum()->setEnumNames(surfaceProp, modesLst);
    properties->mEnum()->setValue(surfaceProp, static_cast<int>(surface));


    modesLst.clear();
    modesLst << "backwards" << "forwards" << "both";
    integrationDirProp = a->addProperty(
                ENUM_PROPERTY, "integration direction", groupProp);
    properties->mEnum()->setEnumNames(integrationDirProp, modesLst);
    properties->mEnum()->setValue(integrationDirProp,
                                  static_cast<int>(integrationDir));

    stepSizeProp = a->addProperty(DOUBLE_PROPERTY, "curve stepsize", groupProp);
    properties->setDouble(stepSizeProp, stepSize, 0.001, 100, 3, 0.001);

    numLineSegmentsProp = a->addProperty(
                INT_PROPERTY, "max number line segments", groupProp);
    properties->setInt(numLineSegmentsProp, numLineSegments, 1, 500);

    seedPointResXProp = a->addProperty(
                DOUBLE_PROPERTY, "seed points resolution X", groupProp);
    properties->setDouble(seedPointResXProp, initPointResX, 0.25, 4, 3, 0.25);

    seedPointResYProp = a->addProperty(
                DOUBLE_PROPERTY, "seed points resolution Y", groupProp);
    properties->setDouble(seedPointResYProp, initPointResY, 0.25, 4, 3, 0.25);

    seedPointResZProp = a->addProperty(
                DOUBLE_PROPERTY, "seed points resolution Z", groupProp);
    properties->setDouble(seedPointResZProp, initPointResZ, 0.25, 4, 3, 0.25);

    seedPointVarianceProp = a->addProperty(
                DOUBLE_PROPERTY, "seed points variance", groupProp);
    properties->setDouble(seedPointVarianceProp, initPointVariance, 0, 2, 3, 0.01);

    numStepsProp = a->addProperty(INT_PROPERTY, "max number steps", groupProp);
    properties->setInt(numStepsProp, numSteps, 1, 200);

    curveLengthProp = a->addProperty(
                DOUBLE_PROPERTY, "max curve length", groupProp);
    properties->setDouble(curveLengthProp, curveLength, 0.001, 100, 3, 0.001);

    isoValueBorderProp = a->addProperty(
                DOUBLE_PROPERTY, "isovalue border", groupProp);
    properties->setDouble(isoValueBorderProp, isoValueBorder, -400, 400, 2, 0.1);
}


MNWPVolumeRaycasterActor::~MNWPVolumeRaycasterActor()
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    if (gl.tex2DShadowImage) glRM->releaseGPUItem(gl.tex2DShadowImage);

    if (gl.texUnitShadowImage >=0) releaseTextureUnit(gl.texUnitShadowImage);
    if (gl.texUnitDepthBuffer >=0) releaseTextureUnit(gl.texUnitDepthBuffer);

    delete bbSettings;
    delete lightingSettings;
    delete rayCasterSettings;
    delete normalCurveSettings;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

const uint8_t SHADER_VERTEX_ATTRIBUTE = 0;
const uint8_t SHADER_BORDER_ATTRIBUTE = 1;
const uint8_t SHADER_TEXCOORD_ATTRIBUTE = 1;
const uint8_t SHADER_VALUE_ATTRIBUTE = 1;

void MNWPVolumeRaycasterActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);

    gl.boundingBoxShader->compileFromFile(
                "src/glsl/simple_coloured_geometry.fx.glsl");
    gl.rayCasterEffect->compileFromFile(
                "src/glsl/volume_raycaster.fx.glsl");
    gl.shadowImageRenderShader->compileFromFile(
                "src/glsl/volume_image.fx.glsl");

    gl.normalCurveGeometryEffect->compileFromFile(
                "src/glsl/volume_normalcurves_geometry.fx.glsl");
    gl.normalCurveInitPointsShader->compileFromFile(
                "src/glsl/volume_normalcurves_initpoints.fx.glsl");
    gl.normalCurveLineComputeShader->compileFromFile(
                "src/glsl/volume_compute_normalcurves.fx.glsl");

    gl.bitfieldRayCasterEffect->compileFromFile(
                "src/glsl/volume_bitfield_raycaster.fx.glsl");

    initializeRenderInformation();
}


void MNWPVolumeRaycasterActor::saveConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::saveConfiguration(settings);

    settings->beginGroup(MNWPVolumeRaycasterActor::getSettingsID());

    // actor settings
    // ==============
    settings->setValue("renderMode", properties->getEnumItem(renderModeProp));
    settings->setValue("varIndex", variableIndex);
    settings->setValue("shadingVarIndex", shadingVariableIndex);

    // bounding box settings
    // =====================
    settings->beginGroup("BoundingBox");

    settings->setValue("llcrnLat", bbSettings->llcrnLat);
    settings->setValue("llcrnLon", bbSettings->llcrnLon);
    settings->setValue("urcrnLat", bbSettings->urcrnLat);
    settings->setValue("urcrnLon", bbSettings->urcrnLon);
    settings->setValue("p_bot_hPa", bbSettings->pBot_hPa);
    settings->setValue("p_top_hPa", bbSettings->pTop_hPa);

    settings->endGroup();

    // lighting settings
    // =================
    settings->beginGroup("Lighting");

    settings->setValue("lightingMode", properties->getEnumItem(
                           lightingSettings->lightingModeProp));
    settings->setValue("ambient", lightingSettings->ambient);
    settings->setValue("diffuse", lightingSettings->diffuse);
    settings->setValue("specular", lightingSettings->specular);
    settings->setValue("shininess", lightingSettings->shininess);
    settings->setValue("shadowColor", lightingSettings->shadowColor);

    settings->endGroup();

    // raycaster settings
    // ==================
    settings->beginGroup("Raycaster");

    settings->setValue("numIsoValues", rayCasterSettings->numIsoValues);
    settings->setValue("stepSize", rayCasterSettings->stepSize);
    settings->setValue("interactionStepSize", rayCasterSettings->interactionStepSize);
    settings->setValue("bisectionSteps", rayCasterSettings->bisectionSteps);
    settings->setValue("interactionBisectionSteps", rayCasterSettings->interactionBisectionSteps);
    settings->setValue("shadowMode", properties->getEnumItem(
                           rayCasterSettings->shadowModeProp));
    settings->setValue("shadowMapRes", properties->getEnumItem(
                           rayCasterSettings->shadowsResolutionProp));

    settings->beginGroup("IsoValues");

    for (unsigned int i = 0; i < rayCasterSettings->numIsoValues; ++i)
    {
        IsoValueSettings& setting =  rayCasterSettings->isoValueSetList.at(i);

        settings->beginGroup(QString("isoValue%1").arg(i));

        settings->setValue("enabled", setting.enabled);
        settings->setValue("isoValue", setting.isoValue);
        settings->setValue("colourMode", setting.isoColourType);
        settings->setValue("colour", setting.isoColour);

        settings->endGroup();
    }

    settings->endGroup(); // isoValues

    settings->endGroup(); // raycaster

    // normal curve settings
    // =====================
    settings->beginGroup("NormalCurves");

    settings->setValue("enabled", normalCurveSettings->normalCurvesEnabled);
    settings->setValue("glyphType", static_cast<int>(normalCurveSettings->glyph));
    settings->setValue("threshold", static_cast<int>(normalCurveSettings->threshold));
    settings->setValue("colour", static_cast<int>(normalCurveSettings->colour));
    settings->setValue("surfaceStart", static_cast<int>(normalCurveSettings->surface));
    settings->setValue("stepSize", normalCurveSettings->stepSize);
    settings->setValue("integrationDir", static_cast<int>(normalCurveSettings->integrationDir));
    settings->setValue("numLineSegments", normalCurveSettings->numLineSegments);
    settings->setValue("initPointResX", normalCurveSettings->initPointResX);
    settings->setValue("initPointResY", normalCurveSettings->initPointResY);
    settings->setValue("initPointResZ", normalCurveSettings->initPointResZ);
    settings->setValue("initPointVariance", normalCurveSettings->initPointVariance);
    settings->setValue("numSteps", normalCurveSettings->numSteps);
    settings->setValue("curveLength", normalCurveSettings->curveLength);
    settings->setValue("isoValueBorder", normalCurveSettings->isoValueBorder);

    settings->endGroup(); // normal curves

    settings->endGroup(); // MNWPVolumeRaycasterActor
}


void MNWPVolumeRaycasterActor::loadConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::loadConfiguration(settings);

    settings->beginGroup(MNWPVolumeRaycasterActor::getSettingsID());

    properties->setEnumItem(renderModeProp,
                            settings->value("renderMode").toString());
    variableIndex = settings->value("varIndex").toInt();
    properties->mInt()->setValue(variableIndexProp, variableIndex);
    shadingVariableIndex = settings->value("shadingVarIndex").toInt();
    properties->mInt()->setValue(shadingVariableIndexProp,
                                 shadingVariableIndex);

    // bounding box settings
    // =====================
    settings->beginGroup("BoundingBox");

    bbSettings->llcrnLat = settings->value("llcrnLat").toFloat();
    bbSettings->llcrnLon = settings->value("llcrnLon").toFloat();
    bbSettings->urcrnLat = settings->value("urcrnLat").toFloat();
    bbSettings->urcrnLon = settings->value("urcrnLon").toFloat();

    properties->mRectF()->setValue(
                bbSettings->boxCornersProp,
                QRectF(  bbSettings->llcrnLon,
                         bbSettings->llcrnLat,
                         bbSettings->urcrnLon - bbSettings->llcrnLon,
                         bbSettings->urcrnLat - bbSettings->llcrnLat));

    bbSettings->pBot_hPa = settings->value("p_bot_hPa").toFloat();
    bbSettings->pTop_hPa = settings->value("p_top_hPa").toFloat();
    properties->mDouble()->setValue(bbSettings->pBotProp, bbSettings->pBot_hPa);
    properties->mDouble()->setValue(bbSettings->pTopProp, bbSettings->pTop_hPa);

    settings->endGroup();

    // lighting settings
    // =================
    settings->beginGroup("Lighting");

    properties->setEnumItem(lightingSettings->lightingModeProp,
                            settings->value("lightingMode").toString());
    lightingSettings->ambient = settings->value("ambient").toFloat();
    properties->mDouble()->setValue(lightingSettings->ambientProp,
                                    lightingSettings->ambient);
    lightingSettings->diffuse = settings->value("diffuse").toFloat();
    properties->mDouble()->setValue(lightingSettings->diffuseProp,
                                    lightingSettings->diffuse);
    lightingSettings->specular = settings->value("specular").toFloat();
    properties->mDouble()->setValue(lightingSettings->specularProp,
                                    lightingSettings->specular);
    lightingSettings->shininess = settings->value("shininess").toFloat();
    properties->mDouble()->setValue(lightingSettings->shininessProp,
                                    lightingSettings->shininess);
    lightingSettings->shadowColor = settings->value("shadowColor").value<QColor>();
    properties->mColor()->setValue(lightingSettings->shadowColorProp,
                                   lightingSettings->shadowColor);

    settings->endGroup();

    // raycaster settings
    // ==================
    settings->beginGroup("Raycaster");

    rayCasterSettings->numIsoValues = settings->value("numIsoValues").toUInt();
    properties->mInt()->setValue(rayCasterSettings->numIsoValuesProp,
                                            rayCasterSettings->numIsoValues);

    settings->beginGroup("IsoValues");

    // Remove current isovalue properties.
    for (int i = 0; i < MAX_ISOSURFACES; ++i)
    {
        IsoValueSettings& setting = rayCasterSettings->isoValueSetList.at(i);
        rayCasterSettings->isoValuesProp->removeSubProperty(setting.groupProp);
    }
    rayCasterSettings->isoValueSetList.clear();
    rayCasterSettings->isoEnabled.clear();
    rayCasterSettings->isoValues.clear();
    rayCasterSettings->isoColors.clear();
    rayCasterSettings->isoColorTypes.clear();

    // Load new isovalue properties from file.
    for (uint i = 0; i < rayCasterSettings->numIsoValues; ++i)
    {
        settings->beginGroup(QString("isoValue%1").arg(i));

        bool enabled  = settings->value("enabled").toBool();
        float isoValue = settings->value("isoValue").toFloat();
        IsoValueSettings::ColorType isoColorType = IsoValueSettings::ColorType(
                    settings->value("colourMode").toInt());
        QColor isoColor = settings->value("colour").value<QColor>();

        rayCasterSettings->addIsoValue(i + 1, enabled, false,
                                       isoValue, isoColor, isoColorType);

        settings->endGroup();
    }

    for (int i = rayCasterSettings->numIsoValues; i < MAX_ISOSURFACES; ++i)
    {
        rayCasterSettings->addIsoValue(i + 1, false, true);
    }

    rayCasterSettings->sortIsoValues();

    settings->endGroup(); // isoValueSettings

    rayCasterSettings->stepSize = settings->value("stepSize").toFloat();
    properties->mDouble()->setValue(rayCasterSettings->stepSizeProp,
                                    rayCasterSettings->stepSize);
    rayCasterSettings->interactionStepSize =
            settings->value("interactionStepSize").toFloat();
    properties->mDouble()->setValue( rayCasterSettings->interactionStepSizeProp,
                                     rayCasterSettings->interactionStepSize);
    rayCasterSettings->bisectionSteps =
            settings->value("bisectionSteps").toUInt();
    properties->mDouble()->setValue(rayCasterSettings->bisectionStepsProp,
                                    rayCasterSettings->bisectionSteps);
    rayCasterSettings->interactionBisectionSteps =
            settings->value("interactionBisectionSteps").toUInt();
    properties->mDouble()->setValue(rayCasterSettings->interactionBisectionStepsProp,
                                    rayCasterSettings->interactionBisectionSteps);

    properties->setEnumItem(rayCasterSettings->shadowModeProp,
                            settings->value("shadowMode").toString());
    properties->setEnumItem(rayCasterSettings->shadowsResolutionProp,
                            settings->value("shadowMapRes").toString());

    settings->endGroup();

    // normal curves
    // =============
    settings->beginGroup("NormalCurves");

    normalCurveSettings->normalCurvesEnabled =
            settings->value("enabled").toBool();
    properties->mBool()->setValue(normalCurveSettings->normalCurvesEnabledProp,
                                  normalCurveSettings->normalCurvesEnabled);
    normalCurveSettings->glyph =
            NormalCurveSettings::GlyphType(settings->value("glyphType").toInt());
    properties->mEnum()->setValue(normalCurveSettings->glyphProp,
                                  normalCurveSettings->glyph);
    normalCurveSettings->threshold =
            NormalCurveSettings::Threshold(settings->value("threshold").toInt());
    properties->mEnum()->setValue(normalCurveSettings->thresholdProp,
                                  normalCurveSettings->threshold);

    normalCurveSettings->colour =
            NormalCurveSettings::CurveColor(settings->value("colour").toInt());
    properties->mEnum()->setValue(normalCurveSettings->colourProp,
                                  normalCurveSettings->colour);
    normalCurveSettings->surface =
            NormalCurveSettings::Surface(settings->value("surfaceStart").toInt());
    properties->mEnum()->setValue(normalCurveSettings->surfaceProp,
                                  normalCurveSettings->surface);
    normalCurveSettings->integrationDir =
            NormalCurveSettings::IntegrationDir(settings->value("integrationDir").toInt());
    properties->mEnum()->setValue(normalCurveSettings->integrationDirProp,
                                  normalCurveSettings->integrationDir);

    normalCurveSettings->stepSize =
            settings->value("stepSize").toFloat();
    properties->mDouble()->setValue(normalCurveSettings->stepSizeProp,
                                    normalCurveSettings->stepSize);
    normalCurveSettings->numLineSegments =
            settings->value("numLineSegments").toUInt();
    properties->mInt()->setValue(normalCurveSettings->numLineSegmentsProp,
                                 normalCurveSettings->numLineSegments);
    normalCurveSettings->initPointResX =
            settings->value("initPointResX").toFloat();
    properties->mDouble()->setValue(normalCurveSettings->seedPointResXProp,
                                    normalCurveSettings->initPointResX);
    normalCurveSettings->initPointResY =
            settings->value("initPointResY").toFloat();
    properties->mDouble()->setValue(normalCurveSettings->seedPointResYProp,
                                    normalCurveSettings->initPointResY);
    normalCurveSettings->initPointResZ =
            settings->value("initPointResZ").toFloat();
    properties->mDouble()->setValue(normalCurveSettings->seedPointResZProp,
                                    normalCurveSettings->initPointResZ);
    normalCurveSettings->initPointVariance =
            settings->value("initPointVariance").toFloat();
    properties->mDouble()->setValue(normalCurveSettings->seedPointVarianceProp,
                                    normalCurveSettings->initPointVariance);
    normalCurveSettings->numSteps =
            settings->value("numSteps").toUInt();
    properties->mInt()->setValue(normalCurveSettings->numStepsProp,
                                 normalCurveSettings->numSteps);
    normalCurveSettings->curveLength =
            settings->value("curveLength").toFloat();
    properties->mDouble()->setValue(normalCurveSettings->curveLengthProp,
                                    normalCurveSettings->curveLength);
    normalCurveSettings->isoValueBorder =
            settings->value("isoValueBorder").toFloat();
    properties->mDouble()->setValue(normalCurveSettings->isoValueBorderProp,
                                    normalCurveSettings->isoValueBorder);

    settings->endGroup();

    settings->endGroup();

    // Update normal curves and shadow map on next render cycle.
    updateNextRenderFrame.set(ComputeNCInitPoints);
    updateNextRenderFrame.set(RecomputeNCLines);
    updateNextRenderFrame.set(UpdateShadowImage);

    if (isInitialized()) generateVolumeBoxGeometry();

    var = static_cast<MNWP3DVolumeActorVariable*>(
                variables.at(variableIndex));
    shadingVar = static_cast<MNWP3DVolumeActorVariable*>(
                variables.at(shadingVariableIndex));

    switch(renderMode)
    {
    case RenderMode::Original:
        var->useFlags(false); break;
    case RenderMode::Bitfield:
        var->useFlags(true); break;
    }
}


void MNWPVolumeRaycasterActor::setBoundingBox(
        QRectF corners, float pbot, float ptop)
{
    properties->mRectF()->setValue(bbSettings->boxCornersProp, corners);
    properties->mDouble()->setValue(bbSettings->pBotProp, pbot);
    properties->mDouble()->setValue(bbSettings->pTopProp, ptop);
}


bool MNWPVolumeRaycasterActor::triggerAnalysisOfObjectAtPos(
        MSceneViewGLWidget *sceneView, float clipX, float clipY,
        float clipRadius)
{
    Q_UNUSED(clipRadius);
    LOG4CPLUS_DEBUG(mlog, "triggering isosurface analysis.");

    QVector3D mousePosClipSpace = QVector3D(clipX, clipY, 0.);
    QVector3D mousePosWorldSpace = sceneView->clipSpaceToLonLatWorldZ(
                mousePosClipSpace);

    QVector3D rayOrigin = sceneView->getCamera()->getOrigin();
    QVector3D rayDirection = (mousePosWorldSpace - rayOrigin).normalized();

    // Compute the intersection points of the ray with the volume bounding
    // box. If the ray does not intersect with the box discard this fragment.
    QVector3D volumeTopNWCrnr(bbSettings->urcrnLon, bbSettings->urcrnLat,
                              sceneView->worldZfromPressure(bbSettings->pTop_hPa));
    QVector3D volumeBottomSECrnr(bbSettings->llcrnLon, bbSettings->llcrnLat,
                                 sceneView->worldZfromPressure(bbSettings->pBot_hPa));
    QVector2D lambdaNearFar;

    bool rayIntersectsRenderVolume = rayBoxIntersection(
                rayOrigin, rayDirection, volumeBottomSECrnr, volumeTopNWCrnr,
                &lambdaNearFar);
    if (!rayIntersectsRenderVolume)
    {
        LOG4CPLUS_DEBUG(mlog, "mouse position outside render volume.");
        return false;
    }

    // If the value for lambdaNear is < 0 the camera is located inside the
    // bounding box. It makes no sense to start the ray traversal behind the
    // camera, hence move lambdaNear to 0 to start in front of the camera.
    lambdaNearFar.setX(max(lambdaNearFar.x(), 0.01));

    float stepSize = rayCasterSettings->stepSize;
    float lambda = lambdaNearFar.x();
    float prevLambda = lambda;
    QVector3D rayPosition = rayOrigin + lambdaNearFar.x() * rayDirection;
    QVector3D rayPosIncrement = stepSize * rayDirection;
    QVector3D prevRayPosition = rayPosition;

    float scalar = var->grid->interpolateValue(
                rayPosition.x(), rayPosition.y(),
                sceneView->pressureFromWorldZ(rayPosition.z()));

    int crossingLevelBack = computeCrossingLevel(scalar);
    int crossingLevelFront = crossingLevelBack;

    while (lambda < lambdaNearFar.y())
    {
        scalar = var->grid->interpolateValue(
                    rayPosition.x(), rayPosition.y(),
                    sceneView->pressureFromWorldZ(rayPosition.z()));

        crossingLevelFront = computeCrossingLevel(scalar);

        if (crossingLevelFront != crossingLevelBack)
        {
            bisectionCorrection(sceneView, &rayPosition, &lambda,
                                prevRayPosition, prevLambda,
                                &crossingLevelFront, &crossingLevelBack);

            // Stop after first isosurface crossing.
            QVector3D lonLatP = rayPosition;
            lonLatP.setZ(sceneView->pressureFromWorldZ(rayPosition.z()));

            LOG4CPLUS_DEBUG_FMT(mlog, "isosurface hit at position %.2f "
                                "deg/%.2f deg/%.2f hPa",
                                lonLatP.x(), lonLatP.y(), lonLatP.z());

            updatePositionCrossGeometry(lonLatP);

            if (analysisControl)
            {
                MDataRequestHelper rh;
                rh.insert("POS_LONLATP", lonLatP);
                analysisControl->run(rh.request());
            }

            return true;
        }

        prevLambda  = lambda;
        prevRayPosition = rayPosition;

        lambda += stepSize;
        rayPosition += rayPosIncrement;

        crossingLevelBack = crossingLevelFront;
    } // raycaster loop

    // If we arrive here no isosurface has been hit.
    LOG4CPLUS_DEBUG(mlog, "no isosurface could be identified at mouse position.");

    QVector3D lonLatP = rayPosition;
    lonLatP.setZ(sceneView->pressureFromWorldZ(rayPosition.z()));
    updatePositionCrossGeometry(lonLatP);

    return false;
}


const QList<MVerticalLevelType> MNWPVolumeRaycasterActor::supportedLevelTypes()
{
    return (QList<MVerticalLevelType>()
            << HYBRID_SIGMA_PRESSURE_3D
            << PRESSURE_LEVELS_3D
            << LOG_PRESSURE_LEVELS_3D);
}


MNWPActorVariable* MNWPVolumeRaycasterActor::createActorVariable(
        const MSelectableDataSource& dataSource)
{
    MNWP3DVolumeActorVariable* newVar = new MNWP3DVolumeActorVariable(this);

    newVar->dataSourceID = dataSource.dataSourceID;
    newVar->levelType = dataSource.levelType;
    newVar->variableName = dataSource.variableName;

    return newVar;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MNWPVolumeRaycasterActor::initializeActorResources()
{
    // Parent initialisation (triggers loading of initial data fields).
    MNWPMultiVarActor::initializeActorResources();

    if (!variables.empty())
    {
        var = static_cast<MNWP3DVolumeActorVariable*>(
                    variables.at(variableIndex));
        shadingVar = static_cast<MNWP3DVolumeActorVariable*>(
                    variables.at(shadingVariableIndex));
    }

    // Set variable names and indices in properties.
    varNameList.clear();
    foreach (MNWPActorVariable* v, variables) varNameList << v->variableName;
    properties->mEnum()->setEnumNames(variableIndexProp, varNameList);
    properties->mEnum()->setValue(variableIndexProp, variableIndex);
    properties->mEnum()->setEnumNames(shadingVariableIndexProp, varNameList);
    properties->mEnum()->setValue(shadingVariableIndexProp, shadingVariableIndex);

    // generate bounding box
    generateVolumeBoxGeometry();
    updatePositionCrossGeometry((QVector3D(0., 0., 1050.)));

    // generate and load shaders
    bool loadShaders = false;

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    loadShaders |= glRM->generateEffectProgram("multiactor_bbox",
                                               gl.boundingBoxShader);
    loadShaders |= glRM->generateEffectProgram("multiactor_raycaster",
                                               gl.rayCasterEffect);
    loadShaders |= glRM->generateEffectProgram("multiactor_shadowimage",
                                               gl.shadowImageRenderShader);
    loadShaders |= glRM->generateEffectProgram("multiactor_normalcurve_geom",
                                               gl.normalCurveGeometryEffect);
    loadShaders |= glRM->generateEffectProgram("multiactor_normalcurve_init",
                                               gl.normalCurveInitPointsShader);
    loadShaders |= glRM->generateEffectProgram("multiactor_normalcurve_comp",
                                               gl.normalCurveLineComputeShader);
    loadShaders |= glRM->generateEffectProgram("multiactor_racaster_bitfield",
                                               gl.bitfieldRayCasterEffect);

    if (loadShaders)
    {
        reloadShaderEffects();
    }
    else
    {
        initializeRenderInformation();
    }

    if (gl.texUnitShadowImage >=0) releaseTextureUnit(gl.texUnitShadowImage);
    gl.texUnitShadowImage = assignTextureUnit();

    if (gl.texUnitDepthBuffer >=0) releaseTextureUnit(gl.texUnitDepthBuffer);
    gl.texUnitDepthBuffer = assignTextureUnit();
}


void MNWPVolumeRaycasterActor::initializeRenderInformation()
{
    gl.rayCasterSubroutines.resize(MVerticalLevelType::SIZE_LEVELTYPES);
    gl.bitfieldRayCasterSubroutines.resize(MVerticalLevelType::SIZE_LEVELTYPES);
    gl.normalCompSubroutines.resize(MVerticalLevelType::SIZE_LEVELTYPES);
    gl.normalInitSubroutines.resize(MVerticalLevelType::SIZE_LEVELTYPES);

    gl.rayCasterSubroutines[PRESSURE_LEVELS_3D]
            << "samplePressureLevel"
            << "samplePressureLevelAccel"
            << "pressureLevelGradient";

    gl.rayCasterSubroutines[HYBRID_SIGMA_PRESSURE_3D]
            << "sampleHybridLevel"
            << "sampleHybridLevelAccel"
            << "hybridLevelGradient";

//    gl.rayCasterSubroutines[LOG_PRESSURE_LEVELS_3D]
//            << "sampleLogPressureLevel"
//            << "sampleLogPressureLevelAccel"
//            << "logPressureLevelGradient";

    gl.bitfieldRayCasterSubroutines[PRESSURE_LEVELS_3D]
            << "samplePressureLevelVolumeBitfield"
            << "samplePressureVolumeAllBits"
            << "pressureLevelGradientBitfield";

    gl.bitfieldRayCasterSubroutines[HYBRID_SIGMA_PRESSURE_3D]
            << "sampleHybridSigmaVolumeBitfield"
            << "sampleHybridVolumeAllBits"
            << "hybridLevelGradientBitfield";

//    gl.bitfieldRayCasterSubroutines[LOG_PRESSURE_LEVELS_3D]
//            << "sampleLogPressureLevel"
//            << "sampleLogPressureLevelAccel";

    gl.normalCompSubroutines[PRESSURE_LEVELS_3D]
            << "samplePressureLevel"
            << "pressureLevelGradient";

    gl.normalCompSubroutines[HYBRID_SIGMA_PRESSURE_3D]
            << "sampleHybridLevel"
            << "hybridLevelGradient";

//    gl.normalCompSubroutines[LOG_PRESSURE_LEVELS_3D]
//            << "sampleLogPressureLevel"
//            << "logPressureLevelGradient";

    gl.normalInitSubroutines[PRESSURE_LEVELS_3D]
            << "samplePressureLevel";

    gl.normalInitSubroutines[HYBRID_SIGMA_PRESSURE_3D]
            << "sampleHybridLevel";

//    gl.normalInitSubroutines[LOG_PRESSURE_LEVELS_3D]
//            << "sampleLogPressureLevel";

    // Re-compute normal curves and shadow image on next frame.
    updateNextRenderFrame.set(ComputeNCInitPoints);
    updateNextRenderFrame.set(RecomputeNCLines);
    updateNextRenderFrame.set(UpdateShadowImage);
}


void MNWPVolumeRaycasterActor::onQtPropertyChanged(QtProperty* property)
{
    // Parent signal processing.
    MNWPMultiVarActor::onQtPropertyChanged(property);

    if (property == bbSettings->boxCornersProp ||
        property == bbSettings->pBotProp ||
        property == bbSettings->pTopProp)
    {
        if (suppressActorUpdates()) return;

        generateVolumeBoxGeometry();

        updateNextRenderFrame.set(UpdateShadowImage);
        updateNextRenderFrame.set(ComputeNCInitPoints);
        updateNextRenderFrame.set(RecomputeNCLines);

        emitActorChangedSignal();
    }

    else if (property == lightingSettings->lightingModeProp ||
             property == lightingSettings->ambientProp ||
             property == lightingSettings->diffuseProp ||
             property == lightingSettings->specularProp ||
             property == lightingSettings->shininessProp)
    {
        lightingSettings->lightingMode = properties->mEnum()
                ->value(lightingSettings->lightingModeProp);
        lightingSettings->ambient = properties->mDouble()
                ->value(lightingSettings->ambientProp);
        lightingSettings->diffuse = properties->mDouble()
                ->value(lightingSettings->diffuseProp);
        lightingSettings->specular = properties->mDouble()
                ->value(lightingSettings->specularProp);
        lightingSettings->shininess = properties->mDouble()
                ->value(lightingSettings->shininessProp);

        emitActorChangedSignal();
    }

    else if (property == lightingSettings->shadowColorProp ||
             property == rayCasterSettings->shadowsResolutionProp)
    {
        lightingSettings->shadowColor = properties->mColor()
                ->value(lightingSettings->shadowColorProp);

        rayCasterSettings->shadowsResolution =
                static_cast<RenderMode::Resolution>(
                    properties->mEnum()
                    ->value(rayCasterSettings->shadowsResolutionProp));

        updateNextRenderFrame.set(UpdateShadowImage);

        emitActorChangedSignal();
    }

    else if (property == rayCasterSettings->stepSizeProp ||
             property == rayCasterSettings->bisectionStepsProp ||
             property == rayCasterSettings->shadowModeProp)
    {
        rayCasterSettings->stepSize = properties->mDouble()
                ->value(rayCasterSettings->stepSizeProp);
        rayCasterSettings->bisectionSteps = properties->mInt()
                ->value(rayCasterSettings->bisectionStepsProp);
        rayCasterSettings->shadowMode = static_cast<RenderMode::ShadowMode>(
                    properties->mEnum()
                    ->value(rayCasterSettings->shadowModeProp));

        emitActorChangedSignal();
    }

    else if (property == rayCasterSettings->interactionStepSizeProp ||
             property == rayCasterSettings->interactionBisectionStepsProp)
    {
        rayCasterSettings->interactionStepSize = properties->mDouble()
                ->value(rayCasterSettings->interactionStepSizeProp);
        rayCasterSettings->interactionBisectionSteps = properties->mInt()
                ->value(rayCasterSettings->interactionBisectionStepsProp);
        // no redraw necessary
    }

    else if (property == normalCurveSettings->normalCurvesEnabledProp)
    {
        normalCurveSettings->normalCurvesEnabled = properties->mBool()
                ->value(normalCurveSettings->normalCurvesEnabledProp);

        if (normalCurveSettings->normalCurvesEnabled)
        {
            updateNextRenderFrame.set(ComputeNCInitPoints);
            updateNextRenderFrame.set(RecomputeNCLines);
        }

        emitActorChangedSignal();
    }

    else if (property == normalCurveSettings->numLineSegmentsProp)
    {
        normalCurveSettings->numLineSegments = properties->mInt()
                ->value(normalCurveSettings->numLineSegmentsProp);

        if (normalCurveSettings->normalCurvesEnabled)
        {
            updateNextRenderFrame.set(RecomputeNCLines);
        }

        emitActorChangedSignal();
    }

    else if (property == normalCurveSettings->surfaceProp ||
             property == normalCurveSettings->seedPointResXProp ||
             property == normalCurveSettings->seedPointResYProp ||
             property == normalCurveSettings->seedPointResZProp ||
             property == normalCurveSettings->seedPointVarianceProp)
    {
        normalCurveSettings->surface =
                static_cast<NormalCurveSettings::Surface>(
                    properties->mEnum()->value(normalCurveSettings->surfaceProp));
        normalCurveSettings->initPointResX = properties->mDouble()
                ->value(normalCurveSettings->seedPointResXProp);
        normalCurveSettings->initPointResY = properties->mDouble()
                ->value(normalCurveSettings->seedPointResYProp);
        normalCurveSettings->initPointResZ = properties->mDouble()
                ->value(normalCurveSettings->seedPointResZProp);
        normalCurveSettings->initPointVariance = properties->mDouble()
                ->value(normalCurveSettings->seedPointVarianceProp);

        if (normalCurveSettings->normalCurvesEnabled)
        {
            updateNextRenderFrame.set(ComputeNCInitPoints);
            updateNextRenderFrame.set(RecomputeNCLines);
        }

        updateNextRenderFrame.set(UpdateShadowImage);

        emitActorChangedSignal();
    }

    else if (property == normalCurveSettings->glyphProp)
    {
        normalCurveSettings->glyph =
                static_cast<NormalCurveSettings::GlyphType>(
                    properties->mEnum()->value(normalCurveSettings->glyphProp));

        emitActorChangedSignal();
    }

    else if (property == normalCurveSettings->thresholdProp ||
             property == normalCurveSettings->colourProp ||
             property == normalCurveSettings->stepSizeProp ||
             property == normalCurveSettings->integrationDirProp ||
             property == normalCurveSettings->numStepsProp ||
             property == normalCurveSettings->curveLengthProp ||
             property == normalCurveSettings->isoValueBorderProp)
    {
        normalCurveSettings->threshold =
                static_cast<NormalCurveSettings::Threshold>(
                    properties->mEnum()->value(normalCurveSettings->thresholdProp));
        normalCurveSettings->colour =
                static_cast<NormalCurveSettings::CurveColor>(
                    properties->mEnum()->value(normalCurveSettings->colourProp));
        normalCurveSettings->stepSize = properties->mDouble()->value(
                    normalCurveSettings->stepSizeProp);
        normalCurveSettings->integrationDir =
                static_cast<NormalCurveSettings::IntegrationDir>(
                    properties->mEnum()->value(normalCurveSettings->integrationDirProp));
        normalCurveSettings->numSteps = properties->mInt()->value(
                    normalCurveSettings->numStepsProp);
        normalCurveSettings->curveLength = properties->mDouble()->value(
                    normalCurveSettings->curveLengthProp);
        normalCurveSettings->isoValueBorder = properties->mDouble()->value(
                    normalCurveSettings->isoValueBorderProp);

        updateNextRenderFrame.set(RecomputeNCLines);

        emitActorChangedSignal();
    }

    else if (property == renderModeProp)
    {
        renderMode = static_cast<RenderMode::Type>(
                    properties->mEnum()->value(renderModeProp));

        if (suppressActorUpdates()) return;

        var = dynamic_cast<MNWP3DVolumeActorVariable*>(variables[variableIndex]);
        shadingVar = dynamic_cast<MNWP3DVolumeActorVariable*>(variables[shadingVariableIndex]);

        switch (renderMode)
        {
        case RenderMode::Original:
            var->ensembleMemberProperty->setEnabled(true);
            var->setEnsembleMember(properties->mInt()->value(
                                       var->ensembleMemberProperty));

            updateNextRenderFrame.set(UpdateShadowImage);

            var->useFlags(false);

            break;

        case RenderMode::Bitfield:

            normalCurveSettings->groupProp->setEnabled(false);

            var->ensembleMemberProperty->setEnabled(true);
            var->setEnsembleMember(properties->mInt()->value(
                                       var->ensembleMemberProperty));

            updateNextRenderFrame.set(UpdateShadowImage);

            var->useFlags(true);

            break;
        }

        emitActorChangedSignal();
    }

    else if (property == variableIndexProp)
    {
        variableIndex = properties->mEnum()->value(variableIndexProp);
        if (variableIndex < 0) return;

        if (variableIndex >= variables.size())
        {
            variableIndex = variables.size() - 1;
            properties->mEnum()->setValue(variableIndexProp, variableIndex);
        }
        else
        {
            var = static_cast<MNWP3DVolumeActorVariable*>(
                        variables.at(variableIndex));
        }

        updateNextRenderFrame.set(ComputeNCInitPoints);
        updateNextRenderFrame.set(RecomputeNCLines);
        updateNextRenderFrame.set(UpdateShadowImage);

        emitActorChangedSignal();
    }

    else if (property == shadingVariableIndexProp)
    {
        shadingVariableIndex = properties->mEnum()->value(
                    shadingVariableIndexProp);
        if (shadingVariableIndex < 0) return;

        if (shadingVariableIndex >= variables.size())
        {
            shadingVariableIndex = variables.size() - 1;
            properties->mEnum()->setValue(shadingVariableIndexProp,
                                          shadingVariableIndex);
        }
        else
        {
            shadingVar = static_cast<MNWP3DVolumeActorVariable*>(
                        variables.at(shadingVariableIndex));
        }

        updateNextRenderFrame.set(ComputeNCInitPoints);
        updateNextRenderFrame.set(RecomputeNCLines);
        updateNextRenderFrame.set(UpdateShadowImage);

        emitActorChangedSignal();
    }

    else if (property == rayCasterSettings->numIsoValuesProp)
    {
        GLuint oldNumIsoValues = rayCasterSettings->numIsoValues;
        rayCasterSettings->numIsoValues = properties->mInt()
                ->value(rayCasterSettings->numIsoValuesProp);

        // remove all not required isovalues
        for (int i = 0; i < int(oldNumIsoValues - rayCasterSettings->numIsoValues); ++i)
        {
            IsoValueSettings& currIsoValueSettings =
                    rayCasterSettings->isoValueSetList[oldNumIsoValues - 1 - i];

            currIsoValueSettings.enabled = false;
            properties->mBool()->setValue(
                    currIsoValueSettings.enabledProp, false);

            rayCasterSettings->isoValuesProp->removeSubProperty(
                        rayCasterSettings->isoValueSetList[oldNumIsoValues - 1 - i].groupProp);
        }

        // add new isovalues
        for(int i = 0; i < int(rayCasterSettings->numIsoValues - oldNumIsoValues); ++i)
        {
            rayCasterSettings->isoValuesProp->addSubProperty(
                        rayCasterSettings->isoValueSetList[oldNumIsoValues + i].groupProp);
        }

        return;
    }

    else
    {
        for (auto it = rayCasterSettings->isoValueSetList.begin();
             it != rayCasterSettings->isoValueSetList.end(); ++it)
        {
            if ( property == it->enabledProp ||
                 property == it->isoValueProp )
            {
                it->enabled = properties->mBool()->value(it->enabledProp);
                it->isoValue = properties->mDouble()->value(it->isoValueProp);

                if (normalCurveSettings->normalCurvesEnabled)
                {
                    updateNextRenderFrame.set(ComputeNCInitPoints);
                    updateNextRenderFrame.set(RecomputeNCLines);
                }

                updateNextRenderFrame.set(UpdateShadowImage);

                // sort list of isoValues
                rayCasterSettings->sortIsoValues();

                if ( it->isoColourType == IsoValueSettings::TransferFuncShadingVar ||
                     it->isoColourType == IsoValueSettings::TransferFuncShadingVarMaxNeighbour )
                {
                    shadingVar->actorPropertyChangeEvent(
                                MPropertyType::IsoValue, &(it->isoValue));
                }

                emitActorChangedSignal();

                return;
            }
            else if (property == it->isoColourProp
                     || property == it->isoColourTypeProp)
            {
                it->isoColour = properties->mColor()->value(it->isoColourProp);
                it->isoColourType = static_cast<IsoValueSettings::ColorType>(
                            properties->mEnum()->value(it->isoColourTypeProp));

                updateNextRenderFrame.set(UpdateShadowImage);

                rayCasterSettings->sortIsoValues();

                emitActorChangedSignal();

                return;
            }
        } // isovalues
    }
}


void MNWPVolumeRaycasterActor::renderToCurrentContext(
        MSceneViewGLWidget* sceneView)
{     
    // Render volume bounding box
    // ==========================
    renderBoundingBox(sceneView);

    // Check for valid actor variables.
    // ================================

    if ( variables.empty() ) return;

    // Update variables
    var = static_cast<MNWP3DVolumeActorVariable*>(
                variables.at(variableIndex));
    shadingVar = static_cast<MNWP3DVolumeActorVariable*>(
                variables.at(shadingVariableIndex));

    // Are the variable grids valid objects?
    if ( !var->hasData() || !shadingVar->hasData() ) return;

    // If the variable's bitfield shall be rendered, does the grid contain
    // valid flags?
    if (renderMode == RenderMode::Bitfield)
        if ( !var->grid->flagsEnabled() ) return;

    // In analysis mode, render a cross at the position where the user
    // has clicked.
    if (sceneView->analysisModeEnabled()) renderPositionCross(sceneView);

    // Compute (if requested) and render normal curves and shadow map.
    // ===============================================================
    if (normalCurveSettings->normalCurvesEnabled)
    {
        if(updateNextRenderFrame[RecomputeNCLines])
        {
            computeNormalCurves(sceneView);
        }
    }

    if (rayCasterSettings->shadowMode == RenderMode::ShadowMap)
    {
        if (updateNextRenderFrame[UpdateShadowImage])
        {
            createShadowImage(sceneView);
        }

        renderShadows(sceneView);

        if (normalCurveSettings->normalCurvesEnabled)
        {
            renderNormalCurves(sceneView, false, true);
        }
    }

    if (normalCurveSettings->normalCurvesEnabled)
    {
        renderNormalCurves(sceneView, false);
    }

    // render depth of curve segments to depth buffer
    renderToDepthTexture(sceneView);

    // Raycaster.
    // ==========

    switch(renderMode)
    {
    case RenderMode::Original:
        renderRayCaster(gl.rayCasterEffect, sceneView);
        break;

    case RenderMode::Bitfield:
        renderRayCaster(gl.bitfieldRayCasterEffect, sceneView);
        break;
    }

    // OpenGL "cleanup".
    // =================

    // Disable polygon offset and face culling
    glDisable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
    glDisable(GL_CULL_FACE); CHECK_GL_ERROR;

    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


void MNWPVolumeRaycasterActor::dataFieldChangedEvent()
{
    if (!isInitialized()) return;

    // Re-compute normal curves and shadow image on next render frame if the
    // data field has changed.
    updateNextRenderFrame.set(ComputeNCInitPoints);
    updateNextRenderFrame.set(RecomputeNCLines);
    updateNextRenderFrame.set(UpdateShadowImage);

    emitActorChangedSignal();
}


bool MNWPVolumeRaycasterActor::rayBoxIntersection(
        QVector3D rayOrigin, QVector3D rayDirection,
        QVector3D boxCrnr1, QVector3D boxCrnr2, QVector2D *tNearFar)
{
    float tnear = 0.;
    float tfar  = 0.;

    QVector3D rayDirInv = QVector3D(1./rayDirection.x(), 1./rayDirection.y(),
                                    1./rayDirection.z());
    if(rayDirInv.x() >= 0.0)
    {
        tnear = (boxCrnr1.x() - rayOrigin.x()) * rayDirInv.x();
        tfar  = (boxCrnr2.x() - rayOrigin.x()) * rayDirInv.x();
    }
    else
    {
        tnear = (boxCrnr2.x() - rayOrigin.x()) * rayDirInv.x();
        tfar  = (boxCrnr1.x() - rayOrigin.x()) * rayDirInv.x();
    }

    if(rayDirInv.y() >= 0.0)
    {
        tnear = max(tnear, float((boxCrnr1.y() - rayOrigin.y()) * rayDirInv.y()));
        tfar  = min(tfar,  float((boxCrnr2.y() - rayOrigin.y()) * rayDirInv.y()));
    }
    else
    {
        tnear = max(tnear, float((boxCrnr2.y() - rayOrigin.y()) * rayDirInv.y()));
        tfar  = min(tfar,  float((boxCrnr1.y() - rayOrigin.y()) * rayDirInv.y()));
    }

    if(rayDirInv.z() >= 0.0)
    {
        tnear = max(tnear, float((boxCrnr1.z() - rayOrigin.z()) * rayDirInv.z()));
        tfar  = min(tfar,  float((boxCrnr2.z() - rayOrigin.z()) * rayDirInv.z()));
    }
    else
    {
        tnear = max(tnear, float((boxCrnr2.z() - rayOrigin.z()) * rayDirInv.z()));
        tfar  = min(tfar,  float((boxCrnr1.z() - rayOrigin.z()) * rayDirInv.z()));
    }

    tNearFar->setX(tnear);
    tNearFar->setY(tfar);
    return (tnear < tfar);
}


int MNWPVolumeRaycasterActor::computeCrossingLevel(float scalar)
{
    int level = 0;

//TODO (mr, 17Nov2014) -- replace by numEnabledIsoValues?
    for (unsigned int i = 0; i < rayCasterSettings->numIsoValues; i++)
    {
        if (rayCasterSettings->isoEnabled[i])
            level += int(scalar >= rayCasterSettings->isoValues[i]);
    }

    return level;
}


void MNWPVolumeRaycasterActor::bisectionCorrection(
        MSceneViewGLWidget *sceneView,
        QVector3D *rayPosition, float *lambda, QVector3D prevRayPosition,
        float prevLambda, int *crossingLevelFront, int *crossingLevelBack)
{
    QVector3D rayCenterPosition;
    float centerLambda;
    int crossingLevelCenter;

    for (unsigned int i = 0; i < rayCasterSettings->bisectionSteps; i++)
    {
        rayCenterPosition = (*rayPosition + prevRayPosition) / 2.0;
        centerLambda = (*lambda + prevLambda) / 2.0;

        float scalar = var->grid->interpolateValue(
                    rayCenterPosition.x(), rayCenterPosition.y(),
                    sceneView->pressureFromWorldZ(rayCenterPosition.z()));

        crossingLevelCenter = computeCrossingLevel(scalar);

        if (crossingLevelCenter != *crossingLevelBack)
        {
            *rayPosition = rayCenterPosition;
            *lambda = centerLambda;
            *crossingLevelFront = crossingLevelCenter;
        }
        else
        {
            prevRayPosition = rayCenterPosition;
            prevLambda = centerLambda;
            *crossingLevelBack = crossingLevelCenter;
        }
    }
}


void MNWPVolumeRaycasterActor::onDeleteActorVariable(MNWPActorVariable *var)
{
    // Correct variable indices.

    // Get index of variable that is about to be removed.
    int i = variables.indexOf(var);

    // Update variableIndex and shadingVariableIndex if these point to
    // the removed variable or to one with a lower index.
    if (i <= variableIndex)
    {
        variableIndex = std::max(-1, variableIndex - 1);
    }
    if (i <= shadingVariableIndex)
    {
        shadingVariableIndex = std::max(-1, shadingVariableIndex - 1);
    }

    // Temporarily save variable indices.
    int tmpVarIndex = variableIndex;
    int tmpShadingVarIndex = shadingVariableIndex;

    // Remove the variable name from the enum lists.
    varNameList.removeAt(i);

    // Update enum lists.
    properties->mEnum()->setEnumNames(variableIndexProp, varNameList);
    properties->mEnum()->setEnumNames(shadingVariableIndexProp, varNameList);
    properties->mEnum()->setValue(variableIndexProp, tmpVarIndex);
    properties->mEnum()->setValue(shadingVariableIndexProp, tmpShadingVarIndex);

    updateNextRenderFrame.set(ComputeNCInitPoints);
    updateNextRenderFrame.set(RecomputeNCLines);
    updateNextRenderFrame.set(UpdateShadowImage);
}


void MNWPVolumeRaycasterActor::onAddActorVariable(MNWPActorVariable *var)
{
    varNameList << var->variableName;

    // Temporarily save variable indices.
    int tmpVarIndex = variableIndex;
    int tmpShadingVarIndex = shadingVariableIndex;

    properties->mEnum()->setEnumNames(variableIndexProp, varNameList);
    properties->mEnum()->setEnumNames(shadingVariableIndexProp, varNameList);
    properties->mEnum()->setValue(variableIndexProp, tmpVarIndex);
    properties->mEnum()->setValue(shadingVariableIndexProp, tmpShadingVarIndex);
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MNWPVolumeRaycasterActor::generateVolumeBoxGeometry()
{
    // Define geometry for bounding box
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    QRectF cornerRect = properties->mRectF()->value(bbSettings->boxCornersProp);

    bbSettings->llcrnLat = cornerRect.y();
    bbSettings->llcrnLon = cornerRect.x();
    bbSettings->urcrnLat = cornerRect.y() + cornerRect.height();
    bbSettings->urcrnLon = cornerRect.x() + cornerRect.width();

    bbSettings->pBot_hPa = properties->mDouble()->value(bbSettings->pBotProp);
    bbSettings->pTop_hPa = properties->mDouble()->value(bbSettings->pTopProp);

    const int numVertices = 8;
    float vertexData[] =
    {
        0, 0, 0, // node 0
        0, 1, 0, // node 1
        1, 1, 0, // node 2
        1, 0, 0, // node 3
        0, 0, 1, // node 4
        0, 1, 1, // node 5
        1, 1, 1, // node 6
        1, 0, 1  // node 7
    };

    const int numIndices = 16 + 36;
    GLushort indexData[] =
    {
        // volume box lines
        0, 1, 2, 3, 0,
        4, 7, 3,
        7, 6, 2,
        6, 5, 1,
        5, 4,

        // bottom
        0, 3, 1,
        3, 2, 1,
        // front
        0, 4, 7,
        0, 7, 3,
        // left
        0, 1, 4,
        1, 5, 4,
        // right
        3, 7, 2,
        7, 6, 2,
        // back
        1, 2, 6,
        1, 6, 5,
        // top
        5, 6, 7,
        5, 7, 4
    };

    // convert vertices to lat/lon/p space
    for (int i = 0; i < numVertices; i++) {
        vertexData[i * 3 + 0] = bbSettings->llcrnLon + vertexData[i * 3 + 0]
                * (bbSettings->urcrnLon - bbSettings->llcrnLon);
        vertexData[i * 3 + 1] = bbSettings->urcrnLat - vertexData[i * 3 + 1]
                * (bbSettings->urcrnLat - bbSettings->llcrnLat);
        vertexData[i * 3 + 2] = (vertexData[i * 3 + 2] == 0)
                ? bbSettings->pBot_hPa : bbSettings->pTop_hPa;
    }

    const QString vboID = QString("vbo_bbox_actor#%1").arg(myID);
    GL::MVertexBuffer* vbo = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(vboID));

    if (vbo)
    {
        gl.vboBoundingBox = vbo;
        GL::MFloat3VertexBuffer* buf = dynamic_cast<GL::MFloat3VertexBuffer*>(vbo);
        buf->update(vertexData, numVertices);
    }
    else
    {
        GL::MFloat3VertexBuffer* buf = nullptr;
        buf = new GL::MFloat3VertexBuffer(vboID, numVertices);
        if (glRM->tryStoreGPUItem(buf))
        {
            buf->upload(vertexData, numVertices);
        }
        else
        {
            delete buf;
        }
        gl.vboBoundingBox = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(vboID));
    }

    glGenBuffers(1, &gl.iboBoundingBox); CHECK_GL_ERROR;
    //cout << "uploading raycaster elements indices to IBO" << gl.iboBoundingBox << "\n" << flush;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.iboBoundingBox); CHECK_GL_ERROR;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 numIndices * sizeof(GLushort),
                 indexData,
                 GL_STATIC_DRAW); CHECK_GL_ERROR;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


void MNWPVolumeRaycasterActor::updatePositionCrossGeometry(
        QVector3D worldSpacePosition)
{
    float size = 2.;
    QVector<QVector3D> vertices;
    QVector3D &p = worldSpacePosition;
    vertices.append(QVector3D(p.x()-size, p.y(), p.z()));
    vertices.append(QVector3D(p.x()+size, p.y(), p.z()));
    vertices.append(QVector3D(p.x(), p.y()-size, p.z()));
    vertices.append(QVector3D(p.x(), p.y()+size, p.z()));
    vertices.append(QVector3D(p.x(), p.y(), p.z()-40.));
    vertices.append(QVector3D(p.x(), p.y(), p.z()+40.));

    const QString vboID = QString("vbo_positioncross_actor#%1").arg(myID);
    uploadVec3ToVertexBuffer(vertices, vboID, &gl.vboPositionCross);
}


void MNWPVolumeRaycasterActor::setBoundingBoxShaderVars(
        MSceneViewGLWidget* sceneView)
{
    gl.boundingBoxShader->bindProgram("Pressure");
    gl.boundingBoxShader->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
    gl.boundingBoxShader->setUniformValue(
                "pToWorldZParams", sceneView->pressureToWorldZParameters());
    gl.boundingBoxShader->setUniformValue(
                "colour", QColor(Qt::black));
}


void MNWPVolumeRaycasterActor::setVarSpecificShaderVars(
        std::shared_ptr<GL::MShaderEffect>& shader,
        MSceneViewGLWidget* sceneView,
        MNWP3DVolumeActorVariable* var,
        const QString& structName,
        const QString& volumeName,
        const QString& transferFuncName,
        const QString& pressureTableName,
        const QString& surfacePressureName,
        const QString& hybridCoeffName,
        const QString& lonLatLevAxesName,
        const QString& pressureTexCoordTable2DName,
        const QString& minMaxAccelStructure3DName,
        const QString& dataFlagsVolumeName)
{
    // Reset optional textures to avoid draw errors.
    // =============================================

    // 1D textures...
    var->textureDummy1D->bindToTextureUnit(var->textureUnitUnusedTextures);
    shader->setUniformValue(pressureTableName, var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    shader->setUniformValue(hybridCoeffName, var->textureUnitUnusedTextures); CHECK_GL_ERROR;
    shader->setUniformValue(transferFuncName, var->textureUnitTransferFunction); CHECK_GL_ERROR;

    // 2D textures...
    var->textureDummy2D->bindToTextureUnit(var->textureUnitUnusedTextures);
    shader->setUniformValue(surfacePressureName, var->textureUnitUnusedTextures); CHECK_GL_ERROR;
#ifdef ENABLE_HYBRID_PRESSURETEXCOORDTABLE
    shader->setUniformValue(pressureTexCoordTable2DName, var->textureUnitUnusedTextures); CHECK_GL_ERROR;
#endif

    // 3D textures...
    var->textureDummy3D->bindToTextureUnit(var->textureUnitUnusedTextures);
    shader->setUniformValue(dataFlagsVolumeName, var->textureUnitUnusedTextures); CHECK_GL_ERROR;

    // Bind textures and set uniforms.
    // ===============================

    // Bind volume data
    var->textureDataField->bindToTextureUnit(
                var->textureUnitDataField);  CHECK_GL_ERROR;
    shader->setUniformValue(
                volumeName,
                var->textureUnitDataField); CHECK_GL_ERROR;

    // Texture bindings for transfer function for data field (1D texture from
    // transfer function class).
    if (var->transferFunction)
    {
        var->transferFunction->getTexture()->bindToTextureUnit(
                    var->textureUnitTransferFunction);
        shader->setUniformValue(
                    transferFuncName,
                    var->textureUnitTransferFunction); CHECK_GL_ERROR;

        shader->setUniformValue(
                    structName + ".tfMinimum",
                    var->transferFunction->getMinimumValue()); CHECK_GL_ERROR;
        shader->setUniformValue(
                    structName + ".tfMaximum",
                    var->transferFunction->getMaximimValue()); CHECK_GL_ERROR;
    }
    else
    {
        shader->setUniformValue(structName + ".tfMinimum", 0.); CHECK_GL_ERROR;
        shader->setUniformValue(structName + ".tfMaximum", 0.); CHECK_GL_ERROR;
    }

    var->textureLonLatLevAxes->bindToTextureUnit(var->textureUnitLonLatLevAxes);
    shader->setUniformValue(lonLatLevAxesName, var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;

#ifdef ENABLE_RAYCASTER_ACCELERATION
    // Bind acceleration grid.
    var->textureMinMaxAccelStructure->bindToTextureUnit(
                var->textureUnitMinMaxAccelStructure);
    shader->setUniformValue(
                minMaxAccelStructure3DName,
                var->textureUnitMinMaxAccelStructure); CHECK_GL_ERROR;
#endif

    if (var->textureDataFlags != nullptr)
    {
        // The data flags texture will only be valid if the grid contains
        // a flags field and this actor's render mode requests the flags
        // bitfield.
        var->textureDataFlags->bindToTextureUnit(var->textureUnitDataFlags); CHECK_GL_ERROR;
        shader->setUniformValue("flagsVolume", var->textureUnitDataFlags);
    }

    // Set uniforms specific to data var level type.
    // =============================================

    QVector3D dataNWCrnr = var->grid->getNorthWestTopDataVolumeCorner_lonlatp();
    dataNWCrnr.setZ(sceneView->worldZfromPressure(dataNWCrnr.z()));
    QVector3D dataSECrnr = var->grid->getSouthEastBottomDataVolumeCorner_lonlatp();
    dataSECrnr.setZ(sceneView->worldZfromPressure(dataSECrnr.z()));

    if (var->grid->getLevelType() == PRESSURE_LEVELS_3D)
    {
        shader->setUniformValue(structName + ".levelType", GLint(0)); CHECK_GL_ERROR;

        // Bind pressure to texture coordinate LUT.
        var->texturePressureTexCoordTable->bindToTextureUnit(
                    var->textureUnitPressureTexCoordTable);
        shader->setUniformValue(
                    pressureTableName,
                    var->textureUnitPressureTexCoordTable); CHECK_GL_ERROR;

        // Helper variables for texture coordinate LUT.
        const GLint nPTable = var->texturePressureTexCoordTable->getWidth();
        const GLfloat deltaZ_PTable = abs(dataSECrnr.z() - dataNWCrnr.z()) / (nPTable - 1);
        const GLfloat upperPTableBoundary = dataNWCrnr.z() + deltaZ_PTable / 2.0f;
        const GLfloat vertPTableExtent = abs(dataNWCrnr.z() - dataSECrnr.z()) + deltaZ_PTable;
        // shader->setUniformValue(structName + ".nPTable", nPTable); CHECK_GL_ERROR;
        // shader->setUniformValue(structName + ".deltaZ_PTable", deltaZ_PTable); CHECK_GL_ERROR;
        shader->setUniformValue(structName + ".upperPTableBoundary", upperPTableBoundary); CHECK_GL_ERROR;
        shader->setUniformValue(structName + ".vertPTableExtent", vertPTableExtent); CHECK_GL_ERROR;
    }

    else if (var->grid->getLevelType() == LOG_PRESSURE_LEVELS_3D)
    {
        shader->setUniformValue(structName + ".levelType", GLint(2)); CHECK_GL_ERROR;
    }

    else if (var->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
    {
        shader->setUniformValue(structName + ".levelType", GLint(1)); CHECK_GL_ERROR;

        // Bind hybrid coefficients
        var->textureHybridCoefficients->bindToTextureUnit(var->textureUnitHybridCoefficients);
        shader->setUniformValue(
                    hybridCoeffName,
                    var->textureUnitHybridCoefficients); CHECK_GL_ERROR;

        // Bind surface pressure
        var->textureSurfacePressure->bindToTextureUnit(var->textureUnitSurfacePressure);
        shader->setUniformValue(
                    surfacePressureName,
                    var->textureUnitSurfacePressure); CHECK_GL_ERROR;

#ifdef ENABLE_HYBRID_PRESSURETEXCOORDTABLE
        // Bind pressure to texture coordinate LUT.
        var->texturePressureTexCoordTable->bindToTextureUnit(
                    var->textureUnitPressureTexCoordTable);
        shader->setUniformValue(
                    pressureTexCoordTable2DName,
                    var->textureUnitPressureTexCoordTable); CHECK_GL_ERROR;
#endif
    }

    // Precompute data extent variables and store in uniform struct.
    // =============================================================
    const GLfloat deltaLatLon = GLfloat(fabs(var->grid->getLons()[1] - var->grid->getLons()[0]));
    const GLfloat westernBoundary = dataNWCrnr.x() - deltaLatLon / 2.0f;
    const GLfloat eastWestExtent = dataSECrnr.x() - dataNWCrnr.x() + deltaLatLon;
    const GLfloat northernBoundary = dataNWCrnr.y() + deltaLatLon / 2.0f;
    const GLfloat northSouthExtent = dataNWCrnr.y() - dataSECrnr.y() + deltaLatLon;

    const GLint nLon = var->grid->nlons;
    const GLint nLat = var->grid->nlats;
    const GLint nLev = var->grid->nlevs;
    const GLfloat deltaLnP = std::abs(dataSECrnr.z() - dataNWCrnr.z()) / (nLev-1);
    const GLfloat upperBoundary = dataNWCrnr.z() + deltaLnP /2.0f;
    const GLfloat verticalExtent = abs(dataNWCrnr.z() - dataSECrnr.z()) + deltaLnP;

    // Assume that lat/lon spacing is the same.
    shader->setUniformValue(structName + ".deltaLatLon", deltaLatLon); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".dataSECrnr", dataSECrnr); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".dataNWCrnr", dataNWCrnr); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".westernBoundary", westernBoundary); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".eastWestExtent", eastWestExtent); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".northernBoundary", northernBoundary); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".northSouthExtent", northSouthExtent); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".nLon", nLon); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".nLat", nLat); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".nLev", nLev); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".deltaLnP", deltaLnP); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".upperBoundary", upperBoundary); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".verticalExtent", verticalExtent); CHECK_GL_ERROR;
}


void MNWPVolumeRaycasterActor::setCommonShaderVars(
        std::shared_ptr<GL::MShaderEffect>& shader, MSceneViewGLWidget* sceneView)
{
    // Set common shader variables.
    // ============================

    shader->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));  CHECK_GL_ERROR;
    shader->setUniformValue(
                "cameraPosition", sceneView->getCamera()->getOrigin());  CHECK_GL_ERROR;
    shader->setUniformValue(
                "lightDirection", sceneView->getLightDirection());  CHECK_GL_ERROR;

    // In single member mode, current ensemble member (used to access single
    // bits from the bitfield in the shader).
    shader->setUniformValue("ensembleMember", var->getEnsembleMember()); CHECK_GL_ERROR;

    shader->setUniformValue(
                "pToWorldZParams",
                sceneView->pressureToWorldZParameters()); CHECK_GL_ERROR;

    shader->setUniformValue(
                "volumeBottomSECrnr",
                QVector3D(bbSettings->llcrnLon,
                          bbSettings->llcrnLat,
                          sceneView->worldZfromPressure(bbSettings->pBot_hPa))); CHECK_GL_ERROR;
    shader->setUniformValue(
                "volumeTopNWCrnr",
                QVector3D(bbSettings->urcrnLon,
                          bbSettings->urcrnLat,
                          sceneView->worldZfromPressure(bbSettings->pTop_hPa))); CHECK_GL_ERROR;

    setVarSpecificShaderVars(shader, sceneView, var, "dataExtent",
                             "dataVolume","transferFunction",
                             "pressureTable", "surfacePressure",
                             "hybridCoefficients", "lonLatLevAxes",
                             "pressureTexCoordTable2D", "minMaxAccel3D",
                             "flagsVolume");

    setVarSpecificShaderVars(shader, sceneView, shadingVar, "dataExtentShV",
                             "dataVolumeShV","transferFunctionShV",
                             "pressureTableShV", "surfacePressureShV",
                             "hybridCoefficientsShV", "lonLatLevAxesShV",
                             "pressureTexCoordTable2DShV", "minMaxAccel3DShV",
                             "flagsVolumeShV");
}


void MNWPVolumeRaycasterActor::setRayCasterShaderVars(
        std::shared_ptr<GL::MShaderEffect>& shader, MSceneViewGLWidget* sceneView)
{
    setCommonShaderVars(shader, sceneView);

    // set depth texture for opacity mapping
    // =====================================

    glActiveTexture(GL_TEXTURE0 + gl.texUnitDepthBuffer);
    glBindTexture(GL_TEXTURE_2D, gl.tex2DDepthBuffer);

    shader->setUniformValue("depthTex", gl.texUnitDepthBuffer);

    // set lighting params variables
    // =============================

    shader->setUniformValue("lightingMode", lightingSettings->lightingMode); CHECK_GL_ERROR;
    shader->setUniformValue("ambientCoeff", lightingSettings->ambient); CHECK_GL_ERROR;
    shader->setUniformValue("diffuseCoeff", lightingSettings->diffuse); CHECK_GL_ERROR;
    shader->setUniformValue("specularCoeff", lightingSettings->specular); CHECK_GL_ERROR;
    shader->setUniformValue("shininessCoeff", lightingSettings->shininess); CHECK_GL_ERROR;
    shader->setUniformValue("shadowColor", lightingSettings->shadowColor); CHECK_GL_ERROR;

    // set raycaster shader variables
    // ==============================

    // enhance performance when user is interacting with scene
    if (sceneView->userIsInteractingWithScene() || sceneView->userIsScrollingWithMouse())
    {
        shader->setUniformValue(
                    "stepSize", rayCasterSettings->interactionStepSize); CHECK_GL_ERROR;
        shader->setUniformValue(
                    "bisectionSteps", rayCasterSettings->interactionBisectionSteps); CHECK_GL_ERROR;
    }
    else
    {
        shader->setUniformValue(
                    "stepSize", rayCasterSettings->stepSize); CHECK_GL_ERROR;
        shader->setUniformValue(
                    "bisectionSteps", rayCasterSettings->bisectionSteps); CHECK_GL_ERROR;
    }

    shader->setUniformValueArray(
                "isoEnables", &rayCasterSettings->isoEnabled[0], MAX_ISOSURFACES); CHECK_GL_ERROR;
    shader->setUniformValueArray(
                "isoValues", &rayCasterSettings->isoValues[0], MAX_ISOSURFACES, 1); CHECK_GL_ERROR;
    shader->setUniformValueArray(
                "isoColors", &rayCasterSettings->isoColors[0], MAX_ISOSURFACES); CHECK_GL_ERROR;
    shader->setUniformValueArray(
                "isoColorModes", &rayCasterSettings->isoColorTypes[0], MAX_ISOSURFACES); CHECK_GL_ERROR;
    shader->setUniformValue(
                "numIsoValues", rayCasterSettings->numEnabledIsoValues); CHECK_GL_ERROR;

    // set shadow variables
    // ====================

    if (rayCasterSettings->shadowMode == RenderMode::ShadowMap)
    {
        shader->setUniformValue("shadowMode", GLint(RenderMode::ShadowOff));
    }
    else
    {
        shader->setUniformValue("shadowMode", GLint(rayCasterSettings->shadowMode));
    }
    CHECK_GL_ERROR;
}


void MNWPVolumeRaycasterActor::setNormalCurveShaderVars(
        std::shared_ptr<GL::MShaderEffect>& shader, MSceneViewGLWidget* sceneView)
{
    shader->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;
    shader->setUniformValue(
                "cameraPosition", sceneView->getCamera()->getOrigin()); CHECK_GL_ERROR;

    // Lighting direction from scene view.
    shader->setUniformValue(
                "lightDirection", sceneView->getLightDirection()); CHECK_GL_ERROR;

    if (var->transferFunction)
    {
        var->transferFunction->getTexture()->bindToTextureUnit(
                    var->textureUnitTransferFunction);
        shader->setUniformValue(
                    "transferFunction", var->textureUnitTransferFunction);

        shader->setUniformValue(
                    "tfMinimum", var->transferFunction->getMinimumValue());
        shader->setUniformValue(
                    "tfMaximum", var->transferFunction->getMaximimValue());
    }

    shader->setUniformValue(
                "normalized", GLboolean(
                    normalCurveSettings->colour != NormalCurveSettings::ColorIsoValue));

    glActiveTexture(GL_TEXTURE0 + gl.texUnitDepthBuffer);
    glBindTexture(GL_TEXTURE_2D, gl.tex2DDepthBuffer);

    shader->setUniformValue("depthTex", gl.texUnitDepthBuffer);
}


void MNWPVolumeRaycasterActor::renderBoundingBox(
        MSceneViewGLWidget* sceneView)
{
    setBoundingBoxShaderVars(sceneView);

    //glBindBuffer(GL_ARRAY_BUFFER, gl.vboBoundingBox); CHECK_GL_ERROR;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.iboBoundingBox); CHECK_GL_ERROR;

    gl.vboBoundingBox->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
    glLineWidth(1); CHECK_GL_ERROR;

    glDrawElements(GL_LINE_STRIP, 16, GL_UNSIGNED_SHORT, 0);
}


void MNWPVolumeRaycasterActor::renderPositionCross(
        MSceneViewGLWidget *sceneView)
{
    setBoundingBoxShaderVars(sceneView);

    gl.vboPositionCross->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
    glLineWidth(2); CHECK_GL_ERROR;

    glDrawArrays(GL_LINES, 0, 6); CHECK_GL_ERROR;
}


void MNWPVolumeRaycasterActor::renderRayCaster(
        std::shared_ptr<GL::MShaderEffect>& effect, MSceneViewGLWidget* sceneView)
{
    effect->bindProgram("Volume");

    setRayCasterShaderVars(effect, sceneView); CHECK_GL_ERROR;

    //pEffect->printSubroutineInformation(GL_FRAGMENT_SHADER);

    switch(renderMode)
    {
        case RenderMode::Original:
            // set subroutine indices
            effect->setUniformSubroutineByName(
                        GL_FRAGMENT_SHADER,
                        gl.rayCasterSubroutines[var->grid->getLevelType()]);
            break;
        case RenderMode::Bitfield:
            effect->setUniformSubroutineByName(
                        GL_FRAGMENT_SHADER,
                        gl.bitfieldRayCasterSubroutines[var->grid->getLevelType()]);
            break;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.iboBoundingBox); CHECK_GL_ERROR;

    gl.vboBoundingBox->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glPolygonMode(GL_BACK, GL_FILL); CHECK_GL_ERROR; // draw back faces..
    glCullFace(GL_FRONT); CHECK_GL_ERROR;            // ..and cull front faces
    glEnable(GL_CULL_FACE); CHECK_GL_ERROR;

    glPolygonOffset(.8f, 1.0f); CHECK_GL_ERROR;
    glEnable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;

    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, (void*)(16 * sizeof(GLushort)));
    CHECK_GL_ERROR;
}


void MNWPVolumeRaycasterActor::createShadowImage(
        MSceneViewGLWidget* sceneView)
{
    updateNextRenderFrame.reset(UpdateShadowImage);

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    float lonDist = var->grid->lons[var->grid->nlons - 1] - var->grid->lons[0];
    float latDist = var->grid->lats[0] - var->grid->lats[var->grid->nlats - 1];

    //float zMin = sceneView->worldZfromPressure(bbSettings->pBot);
    float zMax = sceneView->worldZfromPressure(bbSettings->pTop_hPa);
    //float zDist = zMax - zMin;

    // create current vertex data (positions of bounding box lad)
    float vertexData[] =
    {
        -1,-1,
        bbSettings->llcrnLon, bbSettings->llcrnLat, zMax,
        -1 ,1,
        bbSettings->llcrnLon, bbSettings->urcrnLat, zMax,
         1,-1,
        bbSettings->urcrnLon, bbSettings->llcrnLat, zMax,
         1, 1,
        bbSettings->urcrnLon, bbSettings->urcrnLat, zMax,
    };

    const QString vboID = QString("vbo_shadowimage_actor_#%1").arg(myID);
    GL::MVertexBuffer* vbo = static_cast<GL::MVertexBuffer*>(
                                        glRM->getGPUItem(vboID));

    const GLint numVertices = 20;

    if (vbo)
    {
        gl.vboShadowImage = vbo;
        GL::MFloatVertexBuffer* buf = dynamic_cast<GL::MFloatVertexBuffer*>(vbo);
        buf->update(vertexData, numVertices, 0, 0, sceneView);
    }
    else
    {
        // update buffer instead of deleting and creating a new one
        GL::MFloatVertexBuffer* newVB = nullptr;
        newVB = new GL::MFloatVertexBuffer(vboID, numVertices);
        if (glRM->tryStoreGPUItem(newVB))
        {
            newVB->upload(vertexData, numVertices, sceneView);
        }
        else
        {
            delete newVB;
        }
        gl.vboShadowImage = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(vboID));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);  CHECK_GL_ERROR;


    float ratio = lonDist / latDist;

    int resX = 1 << (9 + static_cast<int>(rayCasterSettings->shadowsResolution));
    int resY = static_cast<int>(std::ceil(resX / ratio));

    GLuint tempFBO = 0;
    glGenFramebuffers(1, &tempFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, tempFBO);

    QString shadowImageTextureID = QString("shadow_image_2D_actor_#%1").arg(myID);
//    gl.tex2DShadowImage = dynamic_cast<GL::Texture*>(glRM->getGPUItem(getID()));

    GLint oldResX = 0;
    GLint oldResY = 0;

    if (gl.tex2DShadowImage)
    {
        gl.tex2DShadowImage->bindToTextureUnit(gl.texUnitShadowImage);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &oldResX);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &oldResY);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Create new framebuffer texture if either none had been created so far,
    // or if the resolution has changed.
    if (!gl.tex2DShadowImage || oldResX != resX || oldResY != resY)
    {
        if (!gl.tex2DShadowImage)
        {
            gl.tex2DShadowImage = new GL::MTexture(shadowImageTextureID,
                                                  GL_TEXTURE_2D, GL_RGBA32F,
                                                  resX, resY);
            glRM->tryStoreGPUItem(gl.tex2DShadowImage);
        }
        else
        {
            gl.tex2DShadowImage->updateSize(resX, resY);
        }

        gl.tex2DShadowImage->bindToTextureUnit(gl.texUnitShadowImage);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, resX, resY, 0, GL_RGBA,
                     GL_FLOAT, nullptr); CHECK_GL_ERROR;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); CHECK_GL_ERROR;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); CHECK_GL_ERROR;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP); CHECK_GL_ERROR;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP); CHECK_GL_ERROR;
        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                gl.tex2DShadowImage->getTextureObject(), 0);
    }
    else
    {
        gl.tex2DShadowImage->bindToTextureUnit(gl.texUnitShadowImage);
        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                gl.tex2DShadowImage->getTextureObject(), 0);
    }

    glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERROR;

    // activate render to target 0
    GLenum DrawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, DrawBuffers);

    // set viewport resolution
    glViewport(0,0,resX,resY);

    // clear framebuffer
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    // bind current buffers
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    //glBindBuffer(GL_ARRAY_BUFFER, gl.vboShadowImage); CHECK_GL_ERROR;

    // bind vertex attributes
    gl.vboShadowImage->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE,
                                                     2, false,
                                                     5 * sizeof(float),
                                                     (const GLvoid*)(0 * sizeof(float)));

    gl.vboShadowImage->attachToVertexAttribute(SHADER_BORDER_ATTRIBUTE,
                                                     3, false,
                                                     5 * sizeof(float),
                                                     (const GLvoid*)(2 * sizeof(float)));

    // select the mode, polygons have to be drawn. Here back faces and their surfaces are filled
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); CHECK_GL_ERROR;

    // draw results to shadow image

    // set shader variables
    std::shared_ptr<GL::MShaderEffect> pEffect = nullptr;
    switch(renderMode)
    {
    case RenderMode::Original:
        pEffect = gl.rayCasterEffect;
        break;
    case RenderMode::Bitfield:
        pEffect = gl.bitfieldRayCasterEffect;
        break;
    }

    pEffect->bindProgram("Shadow");
    setRayCasterShaderVars(pEffect, sceneView);
    pEffect->setUniformValue("shadowMode", GLint(RenderMode::ShadowMap));
    pEffect->setUniformValue("stepSize", GLfloat(0.5));

    // set indices of subroutines
 //   pEffect->printSubroutineInformation(GL_FRAGMENT_SHADER);

    switch(renderMode)
    {
        case RenderMode::Original:
            // set subroutine indices
            pEffect->setUniformSubroutineByName(GL_FRAGMENT_SHADER,
                                                gl.rayCasterSubroutines[var->grid->getLevelType()]);
            break;
        case RenderMode::Bitfield:
            pEffect->setUniformSubroutineByName(GL_FRAGMENT_SHADER,
                                        gl.bitfieldRayCasterSubroutines[var->grid->getLevelType()]);
            break;
    }

    glDrawArrays(GL_TRIANGLE_STRIP,0,4); CHECK_GL_ERROR;

    /*std::vector<float> pixelData(resX * resY * 4);

    glBindTexture(GL_TEXTURE_2D, gl.tex2DShadowImage->getTextureObject()); CHECK_GL_ERROR;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &pixelData[0]); CHECK_GL_ERROR;
    glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERROR;*/

    /*QImage test_img((int)resX, (int)resY, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < resY; ++y)
    {
        for (int x = 0; x < resX; ++x)
        {
            test_img.setPixel(x,y, QColor(pixelData[(x + y * resX) * 4] * 255,
                                          pixelData[(x + y * resX) * 4 + 1] * 255,
                                          pixelData[(x + y * resX) * 4 + 2] * 255,
                                          pixelData[(x + y * resX) * 4 + 3] * 255).rgba());
        }
    }
    test_img.save("raycaster_topdown_screen.png");*/

    // delete temporary fbo
    glDeleteFramebuffers(1, &tempFBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0,0,sceneView->getViewPortWidth(),sceneView->getViewPortHeight());
    glBindBuffer(GL_ARRAY_BUFFER, 0);  CHECK_GL_ERROR;

    pEffect->setUniformValue("shadowMode", GLint(RenderMode::ShadowOff));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);  CHECK_GL_ERROR;
}


void MNWPVolumeRaycasterActor::renderShadows(
        MSceneViewGLWidget* sceneView)
{
    float vertexData[] =
    {
        bbSettings->llcrnLon, bbSettings->llcrnLat, 0.01f,
        0.0f,0.0f,

        bbSettings->llcrnLon, bbSettings->urcrnLat, 0.01f,
        0.0f,1.0f,

        bbSettings->urcrnLon, bbSettings->llcrnLat, 0.01f,
        1.0f,0.0f,

        bbSettings->urcrnLon, bbSettings->urcrnLat, 0.01f,
        1.0f,1.0f,
    };

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    const QString vboID = QString("vbo_shadowrender_actor#%1").arg(myID);
    GL::MVertexBuffer* vbo = static_cast<GL::MVertexBuffer*>(
                                        glRM->getGPUItem(vboID));

    const GLint numVertices = 20;

    if (vbo)
    {
        gl.vboShadowImageRender = vbo;
        GL::MFloatVertexBuffer* buf = dynamic_cast<GL::MFloatVertexBuffer*>(vbo);
        buf->update(vertexData, numVertices, 0,  0, sceneView);
    }
    else
    {
        // update buffer instead of deleting and creating a new one
        GL::MFloatVertexBuffer* newVB = nullptr;
        newVB = new GL::MFloatVertexBuffer(vboID, numVertices);
        if (glRM->tryStoreGPUItem(newVB))
        {
            newVB->upload(vertexData, numVertices, sceneView);
        }
        else
        {
            delete newVB;
        }
        gl.vboShadowImageRender = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(vboID));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

    gl.shadowImageRenderShader->bind();

    gl.shadowImageRenderShader->setUniformValue("mvpMatrix",*(sceneView->getModelViewProjectionMatrix()));

    gl.tex2DShadowImage->bindToTextureUnit(gl.texUnitShadowImage);
    gl.shadowImageRenderShader->setUniformValue("texImage", GLint(gl.texUnitShadowImage));

    {
        gl.vboShadowImageRender->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE,
                                                         3, false,
                                                         5 * sizeof(float),
                                                         (const GLvoid*)(0 * sizeof(float)));

        gl.vboShadowImageRender->attachToVertexAttribute(SHADER_TEXCOORD_ATTRIBUTE,
                                                         2, false,
                                                         5 * sizeof(float),
                                                         (const GLvoid*)(3 * sizeof(float)));

        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); CHECK_GL_ERROR;
        glDrawArrays(GL_TRIANGLE_STRIP,0,4); CHECK_GL_ERROR;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void MNWPVolumeRaycasterActor::computeNormalCurveInitialPoints(
        MSceneViewGLWidget* sceneView)
{
    updateNextRenderFrame.reset(ComputeNCInitPoints);

    float boxMinZ = sceneView->worldZfromPressure(bbSettings->pBot_hPa);
    float boxMaxZ = sceneView->worldZfromPressure(bbSettings->pTop_hPa);

    float gridSpace = (var->grid->lons[1] - var->grid->lons[0]);

    float boxSpaceLon = std::abs(bbSettings->urcrnLon - bbSettings->llcrnLon);
    float boxSpaceLat = std::abs(bbSettings->urcrnLat - bbSettings->llcrnLat);
    float boxSpaceP = std::abs(boxMaxZ - boxMinZ);

    QVector3D castDirection[] =
    {
        QVector3D(0, 0,-1),
        QVector3D(0, 1, 0),
        QVector3D(1, 0, 0)
    };

    float castRayLength[] =
    {
        boxSpaceP, boxSpaceLat, boxSpaceLon
    };

    struct ImageRes
    {
        int width, height;
    };

    // adjust grid in every direction (voxels are equidistant in every direction)
    uint16_t numLons = boxSpaceLon / gridSpace;
    uint16_t numLats = boxSpaceLat / gridSpace;
    uint16_t numPLs = boxSpaceP / gridSpace;

    int resLons = 1 << static_cast<int>(std::ceil(
                    log2(static_cast<int>((numLons * normalCurveSettings->initPointResX )))));
    int resLats = 1 << static_cast<int>(std::ceil(
                    log2(static_cast<int>((numLats * normalCurveSettings->initPointResY )))));
    int resPLs =  1 << static_cast<int>(std::ceil(
                    log2(static_cast<int>((numPLs  * normalCurveSettings->initPointResZ )))));

    // actual space between two grids in a certain dimension (according to viewport resolution)
    QVector3D gridSpacing(boxSpaceLon / resLons ,
                          boxSpaceLat / resLats ,
                          boxSpaceP / resPLs );

    ImageRes castImageRes[] =
    {
        { resLons, resLats },
        { resLons, resPLs },
        { resLats, resPLs  }
    };

    // temporary FBO to render
    GLuint tempFBO = 0;
    glGenFramebuffers(1, &tempFBO);

    float vertexData[] =
    {
        // top view
        -1,-1,
        bbSettings->llcrnLon, bbSettings->llcrnLat, boxMaxZ,
        -1, 1,
        bbSettings->llcrnLon, bbSettings->urcrnLat, boxMaxZ,
         1,-1,
        bbSettings->urcrnLon, bbSettings->llcrnLat, boxMaxZ,
         1, 1,
        bbSettings->urcrnLon, bbSettings->urcrnLat, boxMaxZ,

        // front view
        -1,-1,
        bbSettings->llcrnLon, bbSettings->llcrnLat, boxMinZ,
        -1, 1,
        bbSettings->llcrnLon, bbSettings->llcrnLat, boxMaxZ,
         1,-1,
        bbSettings->urcrnLon, bbSettings->llcrnLat, boxMinZ,
         1, 1,
        bbSettings->urcrnLon, bbSettings->llcrnLat, boxMaxZ,

        // left view
        -1,-1,
        bbSettings->llcrnLon, bbSettings->urcrnLat, boxMinZ,
        -1, 1,
        bbSettings->llcrnLon, bbSettings->urcrnLat, boxMaxZ,
         1,-1,
        bbSettings->llcrnLon, bbSettings->llcrnLat, boxMinZ,
         1, 1,
        bbSettings->llcrnLon, bbSettings->llcrnLat, boxMaxZ
    };

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    const QString vboID = QString("vbo_ncinitpoints_actor#%1").arg(myID);
    GL::MVertexBuffer* vbo = static_cast<GL::MVertexBuffer*>(
                                            glRM->getGPUItem(vboID));

    const GLint numVertices = 20 * 3;

    if (vbo)
    {
        gl.vboInitPoints = vbo;
        GL::MFloatVertexBuffer* buf = dynamic_cast<GL::MFloatVertexBuffer*>(vbo);
        buf->update(vertexData, numVertices, 0, 0, sceneView);
    }
    else
    {
        // update buffer instead of deleting and creating a new one
        GL::MFloatVertexBuffer* newVB = nullptr;
        newVB = new GL::MFloatVertexBuffer(vboID, numVertices);
        if (glRM->tryStoreGPUItem(newVB))
        {
            newVB->upload(vertexData, numVertices, sceneView);
        }
        else
        {
            delete newVB;
        }
        gl.vboInitPoints = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(vboID));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);  CHECK_GL_ERROR;

    normalCurveInitPoints.clear();
    normalCurveInitPoints.reserve(  resLons * resLats
                                  + resPLs  * resLons
                                  + resLats * resPLs);

    GLuint disturbSeedTex[3] = { 0 };
    glGenTextures(3, disturbSeedTex);

    std::default_random_engine engine;
    std::uniform_real_distribution<float> distribution(
                -normalCurveSettings->initPointVariance,
                normalCurveSettings->initPointVariance);

    GLuint textureUnit = assignTextureUnit();
    for (int i = 0; i < 3; ++i)
    {
        std::vector<float> pixels;
        pixels.reserve(castImageRes[i].width * castImageRes[i].height * 2);

        for(GLuint i = 0; i < pixels.capacity(); ++i)
        {
            pixels.push_back(distribution(engine));
        }

        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, disturbSeedTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, castImageRes[i].width, castImageRes[i].height, 0, GL_RG, GL_FLOAT, pixels.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); CHECK_GL_ERROR;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); CHECK_GL_ERROR;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP); CHECK_GL_ERROR;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP); CHECK_GL_ERROR;
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    int ghostGridRes[3] = {resLons ,resLats, resPLs};

    // start point of the ghost grid
    QVector3D ghostGridPoint(bbSettings->llcrnLon,
                             bbSettings->urcrnLat,
                             boxMaxZ);

    // ghost grid containing information, if a certain grid box contains any init point
    std::vector<bool> ghostGrid(ghostGridRes[0] * ghostGridRes[1] * ghostGridRes[2]);
    std::fill(ghostGrid.begin(), ghostGrid.end(), false);

    // DEBUG
    /*uint32_t numInitPoints = 0;
    uint32_t numAcceptedPoints = 0;
    uint32_t numRejectedPoints = 0;
    uint32_t numUpdatedPoints = 0;*/
    // DEBUG

    for (int i = 0; i < 3; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, tempFBO);

        const int NUM_FBO_TEXS = 4;

        GLuint tempFBOTex[NUM_FBO_TEXS];

        glGenTextures(NUM_FBO_TEXS, tempFBOTex);

        for (int j = 0; j < NUM_FBO_TEXS; ++j)
        {
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            glBindTexture(GL_TEXTURE_2D, tempFBOTex[j]); CHECK_GL_ERROR;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
                         castImageRes[i].width, castImageRes[i].height,
                         0, GL_RGBA, GL_FLOAT, NULL); CHECK_GL_ERROR;
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); CHECK_GL_ERROR;
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); CHECK_GL_ERROR;

            glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + j, GL_TEXTURE_2D, tempFBOTex[j], 0);
            glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERROR;
        }

        GLenum DrawBuffers[NUM_FBO_TEXS];
        for (int k = 0; k < NUM_FBO_TEXS; ++k)
        {
            DrawBuffers[k] = GL_COLOR_ATTACHMENT0 + k;
        }

        glDrawBuffers(NUM_FBO_TEXS, DrawBuffers); CHECK_GL_ERROR;

        glViewport(0, 0, castImageRes[i].width, castImageRes[i].height);

        if (renderMode == RenderMode::Original)
        {
            gl.normalCurveInitPointsShader->bind();
            setRayCasterShaderVars(gl.normalCurveInitPointsShader, sceneView);
        }

        //gl.normalCurveInitPointsShader->printSubroutineInformation(GL_FRAGMENT_SHADER);
        gl.normalCurveInitPointsShader->setUniformSubroutineByName(GL_FRAGMENT_SHADER,
                                                                   gl.normalInitSubroutines[var->grid->getLevelType()]);

        gl.normalCurveInitPointsShader->setUniformValue("lightDirection", castDirection[i]); CHECK_GL_ERROR;
        gl.normalCurveInitPointsShader->setUniformValue("boxDistance", GLfloat(castRayLength[i])); CHECK_GL_ERROR;
        gl.normalCurveInitPointsShader->setUniformValue("stepSize", rayCasterSettings->stepSize); CHECK_GL_ERROR;
        gl.normalCurveInitPointsShader->setUniformValue("bisectionSteps", rayCasterSettings->bisectionSteps); CHECK_GL_ERROR;
        gl.normalCurveInitPointsShader->setUniformValue("gridSpacing", gridSpacing); CHECK_GL_ERROR;

        if (normalCurveSettings->surface == NormalCurveSettings::Outer)
        {
            gl.normalCurveInitPointsShader->setUniformValue("isoValue", rayCasterSettings->isoValueSetList[1].isoValue);
        }
        else
        {
            gl.normalCurveInitPointsShader->setUniformValue("isoValue", rayCasterSettings->isoValueSetList[0].isoValue);
        }

        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, disturbSeedTex[i]);
        gl.normalCurveInitPointsShader->setUniformValue("disturbTex", textureUnit);

        // render scene from pixel grid through box

        // clear current backbuffer
        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

        gl.vboInitPoints->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE,
                                                         2, false,
                                                         5 * sizeof(float),
                                                         (const GLvoid*)(0 * sizeof(float)));

        gl.vboInitPoints->attachToVertexAttribute(SHADER_BORDER_ATTRIBUTE,
                                                         3, false,
                                                         5 * sizeof(float),
                                                         (const GLvoid*)(2 * sizeof(float)));

        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); CHECK_GL_ERROR;
        glDrawArrays(GL_TRIANGLE_STRIP, i * 4, 4); CHECK_GL_ERROR;

        // obtain texture data and write it back to memory
        std::vector<std::vector<float> > texDatas(NUM_FBO_TEXS);

        for (int k = 0; k < NUM_FBO_TEXS; ++k)
        {
            texDatas[k].resize(4 * castImageRes[i].width *  castImageRes[i].height);
            glBindTexture(GL_TEXTURE_2D, tempFBOTex[k]);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &texDatas[k][0]); CHECK_GL_ERROR;
            glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERROR;
        }

        /*// print images
        for (int j = 0; j < NUM_FBO_TEXS; ++j)
        {
            QImage img(castImageRes[i].width, castImageRes[i].height, QImage::Format_ARGB32_Premultiplied);

            for (int y = 0; y < img.height(); ++y)
            {
                for (int x = 0; x < img.width(); ++x)
                {
                    int r = static_cast<unsigned char>(texDatas[j][(x + y * castImageRes[i].width) * 4 + 0] * 255);
                    int g = static_cast<unsigned char>(texDatas[j][(x + y * castImageRes[i].width) * 4 + 1] * 255);
                    int b = static_cast<unsigned char>(texDatas[j][(x + y * castImageRes[i].width) * 4 + 2] * 255);
                    int a = static_cast<unsigned char>(texDatas[j][(x + y * castImageRes[i].width) * 4 + 3] * 255);

                    img.setPixel(x,y,QColor(r,g,b,a).rgba());
                }
            }

            img.save(QString("raycaster_image_") + QString("%1_%2.png").arg(i).arg(j));

        }*/

        for (int y = 0; y < castImageRes[i].height; y++)
        {
            for (int x = 0; x < castImageRes[i].width; ++x)
            {
                for (int k = 0; k < NUM_FBO_TEXS; ++k)
                {
                    float alpha = texDatas[k][(x + y * castImageRes[i].width) * 4 + 3];

                    if (alpha)
                    {
                        //numInitPoints++;

                        float red = texDatas[k][(x + y * castImageRes[i].width) * 4 + 0];
                        float green = texDatas[k][(x + y * castImageRes[i].width) * 4 + 1];
                        float blue = texDatas[k][(x + y * castImageRes[i].width) * 4 + 2];

                        QVector3D pos(red,green,blue);

                        GLuint stride = ghostGridRes[0];
                        GLuint slice = ghostGridRes[1] * stride;

                        uint32_t idX = static_cast<uint32_t>(std::abs(pos.x() - ghostGridPoint.x()) / gridSpacing.x());
                        uint32_t idY = static_cast<uint32_t>(std::abs(pos.y() - ghostGridPoint.y()) / gridSpacing.y());
                        uint32_t idZ = static_cast<uint32_t>(std::abs(pos.z() - ghostGridPoint.z()) / gridSpacing.z());
                        uint32_t id = idX + idY * stride + idZ * slice;

                        bool entry = ghostGrid[id];

                        if (!entry)
                        {
                            normalCurveInitPoints.emplace(id, pos);
                            //numAcceptedPoints++;
                            ghostGrid[id] = true;
                        }
                        else
                        {
                            const auto it = normalCurveInitPoints.find(id);

                            if (it == normalCurveInitPoints.end())
                                continue;

                            QVector3D prevPos = it->second;

                            const QVector3D gridCenter(ghostGridPoint.x() + gridSpacing.x() * idX,
                                                       ghostGridPoint.y() - gridSpacing.y() * idY,
                                                       ghostGridPoint.z() - gridSpacing.z() * idZ);

                            QVector3D prevDist = prevPos - gridCenter;
                            QVector3D dist = pos - gridCenter;

                            it->second = prevDist.length() > dist.length() ? pos : prevPos;

                            //numUpdatedPoints += prevDist.length() > dist.length() ? 1 : 0;
                            //numRejectedPoints++;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        glDeleteTextures(NUM_FBO_TEXS, tempFBOTex);

    }

    // DEBUG:
    /*std::cout << "NumInitPoints: " << numInitPoints << "\n" << std::flush;
    std::cout << "NumAcceptedPoints: " << numAcceptedPoints << "\n" << std::flush;
    std::cout << "NumRejectedPoints: " << numRejectedPoints << "\n" << std::flush;
    std::cout << "NumUpdatedPoints: " << numUpdatedPoints << "\n" << std::flush;*/

    glBindBuffer(GL_ARRAY_BUFFER, 0);  CHECK_GL_ERROR;

    glDeleteTextures(3, disturbSeedTex); CHECK_GL_ERROR;

    glDeleteFramebuffers(1, &tempFBO); CHECK_GL_ERROR;
    glBindFramebuffer(GL_FRAMEBUFFER, 0); CHECK_GL_ERROR;

    glViewport(0,0,sceneView->getViewPortWidth(),sceneView->getViewPortHeight());

    glBindBuffer(GL_ARRAY_BUFFER, 0);  CHECK_GL_ERROR;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_OFFSET_FILL);

    releaseTextureUnit(textureUnit);
}


void MNWPVolumeRaycasterActor::setNormalCurveComputeShaderVars(
        std::shared_ptr<GL::MShaderEffect>& shader, MSceneViewGLWidget* sceneView)
{

    setCommonShaderVars(shader, sceneView);

    // set subroutine indices
    //pEffect->printSubroutineInformation(GL_COMPUTE_SHADER);

    shader->setUniformSubroutineByName(
                GL_COMPUTE_SHADER,
                gl.normalCompSubroutines[var->grid->getLevelType()]);


    shader->setUniformValue(
                "integrationStepSize", normalCurveSettings->stepSize); CHECK_GL_ERROR;
    shader->setUniformValue(
                "maxNumLines", GLint(normalCurveInitPoints.size())); CHECK_GL_ERROR;
    shader->setUniformValue(
                "maxNumLineSegments", GLint(normalCurveSettings->numLineSegments)); CHECK_GL_ERROR;
    shader->setUniformValue(
                "bisectionSteps", GLint(5)); CHECK_GL_ERROR;


    shader->setUniformValue(
                "colorMode", int(normalCurveSettings->colour)); CHECK_GL_ERROR;
    shader->setUniformValue(
                "abortCriterion", int(normalCurveSettings->threshold)); CHECK_GL_ERROR;

    shader->setUniformValue(
                "maxNumSteps", GLint(normalCurveSettings->numSteps)); CHECK_GL_ERROR;
    shader->setUniformValue(
                "maxCurveLength", GLfloat(normalCurveSettings->curveLength)); CHECK_GL_ERROR;
    shader->setUniformValue(
                "isoValueBorderInner", rayCasterSettings->isoValueSetList[0].isoValue); CHECK_GL_ERROR;
    shader->setUniformValue(
                "isoValueBorderOuter", rayCasterSettings->isoValueSetList[1].isoValue); CHECK_GL_ERROR;
    shader->setUniformValue(
                "isoValueBorder", normalCurveSettings->isoValueBorder); CHECK_GL_ERROR;
}


void MNWPVolumeRaycasterActor::computeNormalCurves(
        MSceneViewGLWidget* sceneView)
{
    updateNextRenderFrame.reset(RecomputeNCLines);

    if (updateNextRenderFrame[ComputeNCInitPoints])
    {
        computeNormalCurveInitialPoints(sceneView);
    }

    normalCurveNumVertices = (normalCurveSettings->numLineSegments + 2) * normalCurveInitPoints.size();

    normalCurveNumVertices = (normalCurveSettings->integrationDir == NormalCurveSettings::Both)
                                ? 2 * normalCurveNumVertices : normalCurveNumVertices;

    if (normalCurveNumVertices == 0)
    {
        LOG4CPLUS_ERROR(mlog, "Warning: could not find any normal curve init "
                        "points");
        return;
    }

    if (!gl.ssboNormalCurves)
    {
        glDeleteBuffers(1, &gl.ssboNormalCurves); CHECK_GL_ERROR;
    }

    glGenBuffers(1, &gl.ssboNormalCurves); CHECK_GL_ERROR;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl.ssboNormalCurves); CHECK_GL_ERROR;
    // STATIC = data is modified once | COPY = read data from GL and use as source for rendering
    glBufferData(GL_SHADER_STORAGE_BUFFER, normalCurveNumVertices * sizeof(NormalCurveLineSegment), NULL, GL_STATIC_COPY); CHECK_GL_ERROR;

    GLint bufMask = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT; CHECK_GL_ERROR;
    NormalCurveLineSegment* vertices = (NormalCurveLineSegment*)
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, normalCurveNumVertices * sizeof(NormalCurveLineSegment), bufMask); CHECK_GL_ERROR;

    auto it = normalCurveInitPoints.cbegin();
    QVector3D currStartPos;

    for (GLuint i = 0; i < normalCurveNumVertices; ++i)
    {
        //GLuint index = i % normalCurveInitPoints.size();
        // fill start vertices of lines
        if(i % (normalCurveSettings->numLineSegments + 2) == 0)
        {
            if (it == normalCurveInitPoints.end())
            {
                it = normalCurveInitPoints.cbegin();
            }

            const QVector3D& pos = it->second;

            vertices[i].x = pos.x();
            vertices[i].y = pos.y();
            vertices[i].z = pos.z();
            vertices[i].value = 0;
            currStartPos = it->second;
            it++;
        }
        else
        {
            // reset other vertices to the dummy element
            vertices[i].x = -1;
            vertices[i].y = -1;
            vertices[i].z = -1;
            vertices[i].value = -1;
        }
    }

    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); CHECK_GL_ERROR;

    // bind compute shader and shader storage buffer object and compute lines
    gl.normalCurveLineComputeShader->bind();
    setNormalCurveComputeShaderVars(gl.normalCurveLineComputeShader, sceneView);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gl.ssboNormalCurves);

    int dispatchX = normalCurveInitPoints.size() / 128 + 1;

    switch(normalCurveSettings->integrationDir)
    {
    case NormalCurveSettings::Backwards:
        gl.normalCurveLineComputeShader->setUniformValue("integrationMode", int(-1)); CHECK_GL_ERROR;
        gl.normalCurveLineComputeShader->setUniformValue("indexOffset", int(0)); CHECK_GL_ERROR;
        glDispatchCompute(dispatchX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        break;

    case NormalCurveSettings::Forwards:
        gl.normalCurveLineComputeShader->setUniformValue("integrationMode", int(1)); CHECK_GL_ERROR;
        gl.normalCurveLineComputeShader->setUniformValue("indexOffset", int(0)); CHECK_GL_ERROR;
        glDispatchCompute(dispatchX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        break;

    case NormalCurveSettings::Both:
    default:
        gl.normalCurveLineComputeShader->setUniformValue("integrationMode", int(-1)); CHECK_GL_ERROR;
        gl.normalCurveLineComputeShader->setUniformValue("indexOffset", int(0)); CHECK_GL_ERROR;
        glDispatchCompute(dispatchX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        gl.normalCurveLineComputeShader->setUniformValue("integrationMode", int(1)); CHECK_GL_ERROR;
        gl.normalCurveLineComputeShader->setUniformValue("indexOffset", int(normalCurveNumVertices / 2)); CHECK_GL_ERROR;
        glDispatchCompute(dispatchX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        break;

    }

    // DEBUG
    /*std::vector<NormalCurveLineSegment> lines(numVertices);

    bufMask = GL_MAP_READ_BIT;
    vertices = (NormalCurveLineSegment*) glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                  numVertices * sizeof(NormalCurveLineSegment), bufMask); CHECK_GL_ERROR;

    for (GLuint i = 0; i < numVertices; ++i)
    {
        lines[i] = vertices[i];
    }*/

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}


void MNWPVolumeRaycasterActor::renderNormalCurves(
        MSceneViewGLWidget* sceneView, bool toDepth, bool shadow)
{
    if (normalCurveNumVertices == 0)
    {
        return;
    }

    switch(normalCurveSettings->glyph)
    {
    case NormalCurveSettings::Line:
        gl.normalCurveGeometryEffect->bindProgram("Line");
        break;
    case NormalCurveSettings::Box:
        gl.normalCurveGeometryEffect->bindProgram("Box");
        break;
    case NormalCurveSettings::Tube:
        gl.normalCurveGeometryEffect->bindProgram(shadow ? "TubeShadow" : "Tube");
        break;
    }

    setNormalCurveShaderVars(gl.normalCurveGeometryEffect, sceneView);
    gl.normalCurveGeometryEffect->setUniformValue("toDepth", GLboolean(toDepth));
    gl.normalCurveGeometryEffect->setUniformValue("shadowColor", lightingSettings->shadowColor);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, gl.ssboNormalCurves); CHECK_GL_ERROR;

    glVertexAttribPointer(
                SHADER_VERTEX_ATTRIBUTE,
                3, GL_FLOAT,
                GL_FALSE,
                4 * sizeof(float),
                (const GLvoid *)0);

    glVertexAttribPointer(
                SHADER_VALUE_ATTRIBUTE,
                1, GL_FLOAT,
                GL_FALSE,
                4 * sizeof(float),
                (const GLvoid *)(3 * sizeof(float)));

    glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);
    glEnableVertexAttribArray(SHADER_VALUE_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); CHECK_GL_ERROR;
    glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, normalCurveNumVertices);

    glBindBuffer(GL_ARRAY_BUFFER, 0);  CHECK_GL_ERROR;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_OFFSET_FILL);
}


void MNWPVolumeRaycasterActor::renderToDepthTexture(
        MSceneViewGLWidget* sceneView)
{
    GLuint tempFBO = 0;
    GLuint tempDBO = 0;

    glGenFramebuffers(1, &tempFBO); CHECK_GL_ERROR;
    glBindFramebuffer(GL_FRAMEBUFFER, tempFBO);CHECK_GL_ERROR;

    GLint width = sceneView->getViewPortWidth();
    GLint height = sceneView->getViewPortHeight();

    glGenRenderbuffers(1, &tempDBO); CHECK_GL_ERROR;
    glBindRenderbuffer(GL_RENDERBUFFER, tempDBO); CHECK_GL_ERROR;
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height); CHECK_GL_ERROR;
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, tempDBO); CHECK_GL_ERROR;

    GLint oldWidth = 0;
    GLint oldHeight = 0;

    if (gl.tex2DDepthBuffer)
    {
        glActiveTexture(GL_TEXTURE0 + gl.texUnitDepthBuffer);
        glBindTexture(GL_TEXTURE_2D, gl.tex2DDepthBuffer);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &oldWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &oldHeight);
        glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERROR;
    }

    if (!gl.tex2DDepthBuffer || width != oldWidth || height != oldHeight)
    {
        if (gl.tex2DDepthBuffer)
        {
            glDeleteTextures(1, &gl.tex2DDepthBuffer);
        }
        glGenTextures(1, &gl.tex2DDepthBuffer);
        glActiveTexture(GL_TEXTURE0 + gl.texUnitDepthBuffer);
        glBindTexture(GL_TEXTURE_2D, gl.tex2DDepthBuffer);

        glTexImage2D(GL_TEXTURE_2D, 0,
                     GL_DEPTH_COMPONENT32, width, height,
                     0, GL_DEPTH_COMPONENT,
                     GL_FLOAT, nullptr); CHECK_GL_ERROR;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); CHECK_GL_ERROR;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); CHECK_GL_ERROR;

        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                gl.tex2DDepthBuffer, 0); CHECK_GL_ERROR;
    }
    else
    {
        glActiveTexture(GL_TEXTURE0 + gl.texUnitDepthBuffer);
        glBindTexture(GL_TEXTURE_2D, gl.tex2DDepthBuffer);

        std::vector<float> texData(width * height);
        std::fill(texData.begin(), texData.end(), 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        0, 0, width, height,
                        GL_DEPTH_COMPONENT,
                        GL_FLOAT, texData.data() );

        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                gl.tex2DDepthBuffer, 0); CHECK_GL_ERROR;
    }

    glClear(GL_DEPTH_BUFFER_BIT); CHECK_GL_ERROR;
    glClear(GL_COLOR_BUFFER_BIT); CHECK_GL_ERROR;

    glDisable(GL_LIGHTING); CHECK_GL_ERROR;
    glDisable(GL_CULL_FACE); CHECK_GL_ERROR;
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); CHECK_GL_ERROR;

    if (normalCurveSettings->normalCurvesEnabled)
    {
        renderNormalCurves(sceneView, true);
    }

    /*
    std::vector<float> debugImg(width * height);
        glBindTexture(GL_TEXTURE_2D, gl.tex2DDepthBuffer);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &debugImg[0]); CHECK_GL_ERROR;
        glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERROR;



        QImage img(width, height, QImage::Format_ARGB32_Premultiplied);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                float t = debugImg[(x + (height - 1 - y) * width)];
                int r = static_cast<unsigned char>(t * 255);

                img.setPixel(x,y,QColor(r,r,r,255).rgba());
            }
        }

        img.save(QString("depth_image.png"));
    */

    glEnable(GL_LIGHTING);
    glDisable(GL_POLYGON_OFFSET_FILL);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);


    glDeleteRenderbuffers(1, &tempDBO);
    glDeleteFramebuffers(1, &tempFBO);
}

} // namespace Met3D
