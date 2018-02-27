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
      MBoundingBoxInterface(this),
      updateNextRenderFrame("111"),
      renderMode(RenderMode::Original),
      variableIndex(0),
      var(nullptr),
      shadingVariableIndex(0),
      shadingVar(nullptr),
      gl(), // initialize gl objects
      bBoxEnabled(true),
      normalCurveNumVertices(0)
{
    bBoxConnection =
            new MBoundingBoxConnection(this, MBoundingBoxConnection::VOLUME);
    // Enable picking for the scene view's analysis mode. See
    // triggerAnalysisOfObjectAtPos().
    enablePicking(true);

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setActorType(staticActorType());
    setName(getActorType());

    QStringList modesLst;
    modesLst << "isosurface" << "bitfield" << "DVR";
    renderModeProp = addProperty(
                ENUM_PROPERTY, "render mode", actorPropertiesSupGroup);
    properties->mEnum()->setEnumNames(renderModeProp, modesLst);
    properties->mEnum()->setValue(renderModeProp, static_cast<int>(renderMode));

    variableIndexProp = addProperty(
                ENUM_PROPERTY, "observed variable", actorPropertiesSupGroup);

    shadingVariableIndexProp = addProperty(
                ENUM_PROPERTY, "shading variable", actorPropertiesSupGroup);

    // Raycaster.
    rayCasterSettings = new RayCasterSettings(this);
    actorPropertiesSupGroup->addSubProperty(rayCasterSettings->groupProp);

    // Bounding box.
    actorPropertiesSupGroup->addSubProperty(bBoxConnection->getProperty());
    bBoxEnabledProperty = addProperty(
                BOOL_PROPERTY, "draw bounding box", actorPropertiesSupGroup);
    properties->mBool()->setValue(bBoxEnabledProperty, bBoxEnabled);

    // Isosurface lighting.
    lightingSettings = new LightingSettings(this);
    actorPropertiesSupGroup->addSubProperty(lightingSettings->groupProp);

    // Normal curves.
    normalCurveSettings = new NormalCurveSettings(this);
    actorPropertiesSupGroup->addSubProperty(normalCurveSettings->groupProp);

    // Set isovalue IDs for normal curves... this works because normal
    // curve properties have been initialized before the constructor is called.
    // NOTE: At least one isovalue needs to be defined!
    QStringList names;
    for (int i = 0; i < rayCasterSettings->isoValueSetList.size(); i++)
        names << rayCasterSettings->isoValueSetList.at(i).groupProp->propertyName();
    properties->mEnum()->setEnumNames(
                normalCurveSettings->startIsoSurfaceProp, names);
    properties->mEnum()->setEnumNames(
                normalCurveSettings->stopIsoSurfaceProp, names);

    properties->mEnum()->setValue(normalCurveSettings->startIsoSurfaceProp, 0);
    properties->mEnum()->setValue(normalCurveSettings->stopIsoSurfaceProp, 0);

    normalCurveSettings->startIsoValue =
            rayCasterSettings->isoValueSetList.at(0).isoValue;
    normalCurveSettings->stopIsoValue =
            rayCasterSettings->isoValueSetList.at(0).isoValue;

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
      ssboInitPoints(nullptr),
      ssboNormalCurves(nullptr),

      tex2DShadowImage(nullptr),
      texUnitShadowImage(-1),
      tex2DDepthBuffer(nullptr),
      texUnitDepthBuffer(-1)
{
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

    shadowColorProp = a->addProperty(COLOR_PROPERTY, "shadow colour", groupProp);
    properties->mColor()->setValue(shadowColorProp, shadowColor);
}


MNWPVolumeRaycasterActor::IsoValueSettings::IsoValueSettings(
        MNWPVolumeRaycasterActor *hostActor,
        const uint8_t index,
        bool _enabled,
        float _isoValue,
        int significantDigits,
        double singleStep,
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

    QString propTitle = QString("isovalue #%1").arg(index  + 1);
    groupProp = a->addProperty(GROUP_PROPERTY, propTitle);

    enabledProp = a->addProperty(BOOL_PROPERTY, "enabled", groupProp);
    properties->mBool()->setValue(enabledProp, enabled);

    isoValueProp = a->addProperty(SCIENTIFICDOUBLE_PROPERTY, "isovalue",
                                  groupProp);
    properties->setSciDouble(isoValueProp, isoValue, significantDigits,
                           singleStep);

    isoValueSignificantDigitsProperty = a->addProperty(
                INT_PROPERTY, "isovalue significant digits", groupProp);
    properties->setInt(isoValueSignificantDigitsProperty,
                       significantDigits, 1, 9, 1);

    isoValueSingleStepProperty = a->addProperty(
                SCIENTIFICDOUBLE_PROPERTY, "isovalue step", groupProp);
    properties->setSciDouble(isoValueSingleStepProperty, singleStep,
                          significantDigits, singleStep);

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

    isoValueRemoveProp = a->addProperty(CLICK_PROPERTY, "remove", groupProp);

    a->endInitialiseQtProperties();
}


const int MAX_ISOSURFACES = 10;

MNWPVolumeRaycasterActor::RayCasterSettings::RayCasterSettings(
        MNWPVolumeRaycasterActor *hostActor)
    : hostActor(hostActor),
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

    groupProp = a->addProperty(
                GROUP_PROPERTY, "isosurface raycaster");

    isoValuesProp = a->addProperty(
                GROUP_PROPERTY, "isovalues", groupProp);

    addIsoValueProp = a->addProperty(
                CLICK_PROPERTY, "add isovalue", isoValuesProp);

    isoEnabled.reserve(MAX_ISOSURFACES);
    isoValues.reserve(MAX_ISOSURFACES);
    isoColors.reserve(MAX_ISOSURFACES);
    isoColorTypes.reserve(MAX_ISOSURFACES);

    // Default isosurface settings: A single isosurface at isovalue 0, colour
    // white.
    addIsoValue(true, false, 0, 1, 0.1, QColor(255,255,255,255));

    // Sort isovalues to ensure correct visualization via crossing levels.
    sortIsoValues();

    groupRaycasterSettings = a->addProperty(
                GROUP_PROPERTY, "sampling step size", groupProp);

    stepSizeProp = a->addProperty(
                SCIENTIFICDOUBLE_PROPERTY, "step size", groupRaycasterSettings);
    properties->setSciDouble(stepSizeProp, stepSize, 1e-9, 10.0, 3, 9, 0.01);

    interactionStepSizeProp = a->addProperty(
                SCIENTIFICDOUBLE_PROPERTY, "interaction step size",
                groupRaycasterSettings);
    properties->setSciDouble(interactionStepSizeProp, interactionStepSize,
                           1e-9, 10.0, 3, 9, 0.1);

    bisectionStepsProp = a->addProperty(
                INT_PROPERTY, "bisection steps", groupRaycasterSettings);
    properties->setInt(bisectionStepsProp, bisectionSteps, 0, 20);

    interactionBisectionStepsProp = a->addProperty(
                INT_PROPERTY, "interaction bisection steps", groupRaycasterSettings);
    properties->setInt(interactionBisectionStepsProp,
                       interactionBisectionSteps, 0, 20);

    groupShadowSettings = a->addProperty(
                GROUP_PROPERTY, "shadow", groupProp);

    QStringList shadowModesList;    
    shadowModesList << "off" << "shadow map" << "shadow ray";
    shadowModeProp = a->addProperty(ENUM_PROPERTY, "shadows", groupShadowSettings);
    properties->mEnum()->setEnumNames(shadowModeProp, shadowModesList);
    properties->mEnum()->setValue(shadowModeProp, static_cast<int>(shadowMode));

    QStringList modesLst;
    modesLst << "very low (0.5K)" << "low (1K)" << "normal (2K)" << "high (4K)"
             << "very high (8K)" << "maximum (16K)";
    shadowsResolutionProp = a->addProperty(
                ENUM_PROPERTY, "shadow map resolution", groupShadowSettings);
    properties->mEnum()->setEnumNames(shadowsResolutionProp, modesLst);
    properties->mEnum()->setValue(shadowsResolutionProp,
                                  static_cast<int>(shadowsResolution));
}


void MNWPVolumeRaycasterActor::RayCasterSettings::addIsoValue(
        const bool enabled, const bool hidden,
        const float isoValue, const int decimals,
        const double singleStep, const QColor color,
        const IsoValueSettings::ColorType colorType)
{
    IsoValueSettings isoSettings(hostActor, isoValueSetList.size(), enabled, isoValue,
                                 decimals, singleStep, color, colorType);
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
        isoValuesProp->addSubProperty(isoValueSetList.back().groupProp);
    }
}


void MNWPVolumeRaycasterActor::RayCasterSettings::sortIsoValues()
{
    for (int i = 0; i < isoValueSetList.size(); ++i)
    {
        isoEnabled[i] = static_cast<int>(isoValueSetList[i].enabled);
        if (isoValueSetList[i].enabled)
        {
            isoValues[i] = isoValueSetList[i].isoValue;
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

    for (int i = 1; i < isoValueSetList.size(); ++i)
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
      threshold(Threshold::Steps),
      colour(CurveColor::ColorIsoValue),
      tubeRadius(0.05),
      stepSize(0.1),
      numLineSegments(20),
      integrationDir(IntegrationDir::Backwards),
      startIsoValue(0),
      stopIsoValue(0),
      initPointResX(1.75),
      initPointResY(1.75),
      initPointResZ(1.0),
      initPointVariance(0.3)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProp = a->addProperty(GROUP_PROPERTY, "normal curves");

    normalCurvesEnabledProp = a->addProperty(
                BOOL_PROPERTY, "enabled", groupProp);
    properties->mBool()->setValue(normalCurvesEnabledProp, normalCurvesEnabled);

    QStringList modesLst;

    modesLst.clear();
    modesLst << "number of line segments" << "stop at isosurface";
    thresholdProp = a->addProperty(
                ENUM_PROPERTY, "curve length limited by", groupProp);
    properties->mEnum()->setEnumNames(thresholdProp, modesLst);
    properties->mEnum()->setValue(thresholdProp, static_cast<int>(threshold));

    startIsoSurfaceProp = a->addProperty(
                ENUM_PROPERTY, "curves start at", groupProp);

    stopIsoSurfaceProp = a->addProperty(
                ENUM_PROPERTY, "curves stop at", groupProp);
    stopIsoSurfaceProp->setEnabled(false);

    modesLst.clear();
    modesLst << "high to low values" << "low to high values" << "both";
    integrationDirProp = a->addProperty(
                ENUM_PROPERTY, "integration direction", groupProp);
    properties->mEnum()->setEnumNames(integrationDirProp, modesLst);
    properties->mEnum()->setValue(integrationDirProp,
                                  static_cast<int>(integrationDir));

    numLineSegmentsProp = a->addProperty(
                INT_PROPERTY, "max number line segments", groupProp);
    properties->setInt(numLineSegmentsProp, numLineSegments, 1, 9999);

    stepSizeProp = a->addProperty(
                DOUBLE_PROPERTY, "length of single line segment", groupProp);
    properties->setDouble(stepSizeProp, stepSize, 0.001, 100, 3, 0.001);

    groupRenderingSettingsProp = a->addProperty(
                GROUP_PROPERTY, "rendering", groupProp);

    modesLst.clear();
    modesLst << "lines" << "boxes + slices" << "tubes";
    glyphProp = a->addProperty(ENUM_PROPERTY, "glyph type", groupRenderingSettingsProp);
    properties->mEnum()->setEnumNames(glyphProp, modesLst);
    properties->mEnum()->setValue(glyphProp, static_cast<int>(glyph));

    modesLst.clear();
//TODO (mr, 03Apr2016) -- first two colour modes are broken; I've disabled them
// for now (hence the "-2" below).
//    modesLst << "ratio steps/max steps" << "ratio curve length/max length"
//             << "transfer function (observed variable)";
    modesLst << "transfer function (observed variable)";
    colourProp = a->addProperty(
                ENUM_PROPERTY, "curve colour", groupRenderingSettingsProp);
    properties->mEnum()->setEnumNames(colourProp, modesLst);
    properties->mEnum()->setValue(colourProp, static_cast<int>(colour) - 2);

    tubeRadiusProp = a->addProperty(
                DOUBLE_PROPERTY, "curve radius", groupRenderingSettingsProp);
    properties->setDouble(tubeRadiusProp, tubeRadius, 0.01, 0.5, 2, 0.01);

    groupSeedSettingsProp = a->addProperty(
                GROUP_PROPERTY, "seed points", groupProp);

    seedPointResXProp = a->addProperty(
                DOUBLE_PROPERTY, "seed spacing lon", groupSeedSettingsProp);
    properties->setDouble(seedPointResXProp, initPointResX, 0.1, 10, 3, 0.1);

    seedPointResYProp = a->addProperty(
                DOUBLE_PROPERTY, "seed spacing lat", groupSeedSettingsProp);
    properties->setDouble(seedPointResYProp, initPointResY, 0.1, 10, 3, 0.1);

    seedPointResZProp = a->addProperty(
                DOUBLE_PROPERTY, "seed spacing Z", groupSeedSettingsProp);
    properties->setDouble(seedPointResZProp, initPointResZ, 0.1, 10, 3, 0.1);

    seedPointVarianceProp = a->addProperty(
                DOUBLE_PROPERTY, "seed points variance", groupSeedSettingsProp);
    properties->setDouble(seedPointVarianceProp, initPointVariance, 0, 2, 3, 0.01);
}


MNWPVolumeRaycasterActor::~MNWPVolumeRaycasterActor()
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    if (gl.tex2DShadowImage) glRM->releaseGPUItem(gl.tex2DShadowImage);
    if (gl.tex2DDepthBuffer) glRM->releaseGPUItem(gl.tex2DDepthBuffer);
    if (gl.vboShadowImageRender) glRM->releaseGPUItem(gl.vboShadowImageRender);
    if (gl.vboBoundingBox) glRM->releaseGPUItem(gl.vboBoundingBox);
    if (gl.ssboInitPoints != nullptr) glRM->releaseGPUItem(gl.ssboInitPoints);
    if (gl.ssboNormalCurves != nullptr) glRM->releaseGPUItem(gl.ssboNormalCurves);
    if (gl.texUnitShadowImage >=0) releaseTextureUnit(gl.texUnitShadowImage);
    if (gl.texUnitDepthBuffer >=0) releaseTextureUnit(gl.texUnitDepthBuffer);

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

    beginCompileShaders(7);

    compileShadersFromFileWithProgressDialog(
                gl.boundingBoxShader,
                "src/glsl/simple_coloured_geometry.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                gl.rayCasterEffect,
                "src/glsl/volume_raycaster.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                gl.shadowImageRenderShader,
                "src/glsl/volume_image.fx.glsl");

    compileShadersFromFileWithProgressDialog(
                gl.normalCurveGeometryEffect,
                "src/glsl/volume_normalcurves_geometry.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                gl.normalCurveInitPointsShader,
                "src/glsl/volume_normalcurves_initpoints.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                gl.normalCurveLineComputeShader,
                "src/glsl/volume_compute_normalcurves.fx.glsl");

    compileShadersFromFileWithProgressDialog(
                gl.bitfieldRayCasterEffect,
                "src/glsl/volume_bitfield_raycaster.fx.glsl");

    endCompileShaders();
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
    settings->setValue("bBoxEnabled", bBoxEnabled);

    // bounding box settings
    // =====================

    MBoundingBoxInterface::saveConfiguration(settings);

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

    settings->setValue("numIsoValues", uint(rayCasterSettings->isoValueSetList.size()));
    settings->setValue("stepSize", rayCasterSettings->stepSize);
    settings->setValue("interactionStepSize", rayCasterSettings->interactionStepSize);
    settings->setValue("bisectionSteps", rayCasterSettings->bisectionSteps);
    settings->setValue("interactionBisectionSteps", rayCasterSettings->interactionBisectionSteps);
    settings->setValue("shadowMode", properties->getEnumItem(
                           rayCasterSettings->shadowModeProp));
    settings->setValue("shadowMapRes", properties->getEnumItem(
                           rayCasterSettings->shadowsResolutionProp));

    settings->beginGroup("IsoValues");

    for (int i = 0; i < rayCasterSettings->isoValueSetList.size(); ++i)
    {
        IsoValueSettings& setting =  rayCasterSettings->isoValueSetList[i];

        settings->beginGroup(QString("isoValue%1").arg(i));

        settings->setValue("enabled", setting.enabled);
        settings->setValue("isoValue", setting.isoValue);
        settings->setValue(
                    "isoValueSignificantDigits",
                    properties->mInt()->value(setting.isoValueSignificantDigitsProperty));
        settings->setValue(
                    "isoValueSingleStep",
                    properties->mSciDouble()->value(setting.isoValueSingleStepProperty));
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
    settings->setValue("tubeRadius", normalCurveSettings->tubeRadius);
    settings->setValue("stepSize", normalCurveSettings->stepSize);
    settings->setValue("integrationDir", static_cast<int>(normalCurveSettings->integrationDir));
    settings->setValue("numLineSegments", normalCurveSettings->numLineSegments);
    settings->setValue("initPointResX", normalCurveSettings->initPointResX);
    settings->setValue("initPointResY", normalCurveSettings->initPointResY);
    settings->setValue("initPointResZ", normalCurveSettings->initPointResZ);
    settings->setValue("initPointVariance", normalCurveSettings->initPointVariance);
    settings->setValue("numSteps", normalCurveSettings->numSteps);
    settings->setValue("startIsoSurfaceEnum",
                       properties->mEnum()->value(normalCurveSettings->startIsoSurfaceProp));
    settings->setValue("stopIsoSurfaceEnum",
                       properties->mEnum()->value(normalCurveSettings->stopIsoSurfaceProp));

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
    properties->mBool()->setValue(bBoxEnabledProperty,
                                  settings->value("bBoxEnabled", true).toBool());

    // bounding box settings
    // =====================

    MBoundingBoxInterface::loadConfiguration(settings);

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
    uint numIsoValues = settings->value("numIsoValues").toUInt();

    settings->beginGroup("IsoValues");

    // Remove current isovalue properties.
    for (int i = 0; i < rayCasterSettings->isoValueSetList.size(); ++i)
    {
        IsoValueSettings& setting = rayCasterSettings->isoValueSetList[i];
        rayCasterSettings->isoValuesProp->removeSubProperty(setting.groupProp);
    }
    rayCasterSettings->isoValueSetList.clear();
    rayCasterSettings->isoEnabled.clear();
    rayCasterSettings->isoValues.clear();
    rayCasterSettings->isoColors.clear();
    rayCasterSettings->isoColorTypes.clear();

    // Load new isovalue properties from file.
    for (uint i = 0; i < numIsoValues; i++)
    {
        settings->beginGroup(QString("isoValue%1").arg(i));

        bool enabled  = settings->value("enabled").toBool();
        float isoValue = settings->value("isoValue").toFloat();
        IsoValueSettings::ColorType isoColorType = IsoValueSettings::ColorType(
                    settings->value("colourMode").toInt());
        QColor isoColor = settings->value("colour").value<QColor>();

        int significantDigits = 2;

        // Support old version of configuration.
        if (settings->contains("isoValueDecimals"))
        {
            significantDigits = settings->value("isoValueDecimals", 2).toInt();
        }
        else
        {
            significantDigits = settings->value("isoValueSignificantDigits",
                                                2).toInt();
        }
        double singleStep = settings->value("isoValueSingleStep", 0.01).toFloat();

        rayCasterSettings->addIsoValue(enabled, false,
                                       isoValue, significantDigits, singleStep,
                                       isoColor, isoColorType);

        settings->endGroup();
    }

    rayCasterSettings->sortIsoValues();

    // Update normal curves properties that link to isovalues.
    QStringList names;
    for (int i = 0; i < rayCasterSettings->isoValueSetList.size(); i++)
        names << rayCasterSettings->isoValueSetList.at(i).groupProp->propertyName();
    properties->mEnum()->setEnumNames(
                normalCurveSettings->startIsoSurfaceProp, names);
    properties->mEnum()->setEnumNames(
                normalCurveSettings->stopIsoSurfaceProp, names);

    settings->endGroup(); // isoValueSettings

    rayCasterSettings->stepSize = settings->value("stepSize").toFloat();
    properties->mSciDouble()->setValue(rayCasterSettings->stepSizeProp,
                                    rayCasterSettings->stepSize);
    rayCasterSettings->interactionStepSize =
            settings->value("interactionStepSize").toFloat();
    properties->mSciDouble()->setValue( rayCasterSettings->interactionStepSizeProp,
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

    normalCurveSettings->tubeRadius = settings->value("tubeRadius").toFloat();
    properties->mDouble()->setValue(normalCurveSettings->tubeRadiusProp,
                                  normalCurveSettings->tubeRadius);


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

    properties->mEnum()->setValue(normalCurveSettings->startIsoSurfaceProp,
                                  settings->value("startIsoSurfaceEnum").toInt());
    normalCurveSettings->startIsoValue =
            rayCasterSettings->isoValueSetList.at(
                settings->value("startIsoSurfaceEnum").toInt()).isoValue;
    properties->mEnum()->setValue(normalCurveSettings->stopIsoSurfaceProp,
                                  settings->value("stopIsoSurfaceEnum").toInt());
    normalCurveSettings->stopIsoValue =
            rayCasterSettings->isoValueSetList.at(
                settings->value("stopIsoSurfaceEnum").toInt()).isoValue;

    settings->endGroup();

    settings->endGroup();

    // Update normal curves and shadow map on next render cycle.
    updateNextRenderFrame.set(ComputeNCInitPoints);
    updateNextRenderFrame.set(RecomputeNCLines);
    updateNextRenderFrame.set(UpdateShadowImage);

    if (isInitialized()) generateVolumeBoxGeometry();

    if (!variables.empty())
    {
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
        case RenderMode::DVR:
            var->useFlags(false); break;
        }
    }
}


bool MNWPVolumeRaycasterActor::triggerAnalysisOfObjectAtPos(
        MSceneViewGLWidget *sceneView, float clipX, float clipY,
        float clipRadius)
{
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        LOG4CPLUS_DEBUG(mlog, "No bounding box is set.");
        return false;
    }

    Q_UNUSED(clipRadius);
    LOG4CPLUS_DEBUG(mlog, "triggering isosurface analysis.");

    QVector3D mousePosClipSpace = QVector3D(clipX, clipY, 0.);
    QVector3D mousePosWorldSpace = sceneView->clipSpaceToLonLatWorldZ(
                mousePosClipSpace);

    QVector3D rayOrigin = sceneView->getCamera()->getOrigin();
    QVector3D rayDirection = (mousePosWorldSpace - rayOrigin).normalized();

    // Compute the intersection points of the ray with the volume bounding
    // box. If the ray does not intersect with the box discard this fragment.
    QVector3D volumeTopNECrnr(
                bBoxConnection->eastLon(), bBoxConnection->northLat(),
                sceneView->worldZfromPressure(bBoxConnection->topPressure_hPa()));
    QVector3D volumeBottomSWCrnr(
                bBoxConnection->westLon(), bBoxConnection->southLat(),
                sceneView->worldZfromPressure(bBoxConnection->bottomPressure_hPa()));
    QVector2D lambdaNearFar;

    bool rayIntersectsRenderVolume = rayBoxIntersection(
                rayOrigin, rayDirection, volumeBottomSWCrnr, volumeTopNECrnr,
                &lambdaNearFar);
    if (!rayIntersectsRenderVolume)
    {
        LOG4CPLUS_DEBUG(mlog, "mouse position outside render volume.");
        return false;
    }

    // If the value for lambdaNear is < 0 the camera is located inside the
    // bounding box. It makes no sense to start the ray traversal behind the
    // camera, hence move lambdaNear to 0 to start in front of the camera.
    if (lambdaNearFar.x() < 0.01)
    {
        lambdaNearFar.setX(0.01);
    }

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


void MNWPVolumeRaycasterActor::onBoundingBoxChanged()
{
    labels.clear();
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
    generateVolumeBoxGeometry();

    updateNextRenderFrame.set(UpdateShadowImage);
    updateNextRenderFrame.set(ComputeNCInitPoints);
    updateNextRenderFrame.set(RecomputeNCLines);

    emitActorChangedSignal();
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MNWPVolumeRaycasterActor::updateShadow()
{
    updateNextRenderFrame.set(UpdateShadowImage);
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
            << "pressureLevelGradient";

    gl.rayCasterSubroutines[HYBRID_SIGMA_PRESSURE_3D]
            << "sampleHybridLevel"
            << "hybridLevelGradient";

    gl.bitfieldRayCasterSubroutines[PRESSURE_LEVELS_3D]
            << "samplePressureLevelVolumeBitfield"
            << "samplePressureVolumeAllBits"
            << "pressureLevelGradientBitfield";

    gl.bitfieldRayCasterSubroutines[HYBRID_SIGMA_PRESSURE_3D]
            << "sampleHybridSigmaVolumeBitfield"
            << "sampleHybridVolumeAllBits"
            << "hybridLevelGradientBitfield";

    gl.normalCompSubroutines[PRESSURE_LEVELS_3D]
            << "samplePressureLevel"
            << "pressureLevelGradient";

    gl.normalCompSubroutines[HYBRID_SIGMA_PRESSURE_3D]
            << "sampleHybridLevel"
            << "hybridLevelGradient";

    gl.normalInitSubroutines[PRESSURE_LEVELS_3D]
            << "samplePressureLevel";

    gl.normalInitSubroutines[HYBRID_SIGMA_PRESSURE_3D]
            << "sampleHybridLevel";

    // Re-compute normal curves and shadow image on next frame.
    updateNextRenderFrame.set(ComputeNCInitPoints);
    updateNextRenderFrame.set(RecomputeNCLines);
    updateNextRenderFrame.set(UpdateShadowImage);
}


void MNWPVolumeRaycasterActor::onQtPropertyChanged(QtProperty* property)
{
    // Parent signal processing.
    MNWPMultiVarActor::onQtPropertyChanged(property);

    if (property == lightingSettings->lightingModeProp ||
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

    else if (property == bBoxEnabledProperty)
    {
        bBoxEnabled = properties->mBool()->value(bBoxEnabledProperty);

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
        rayCasterSettings->stepSize = properties->mSciDouble()
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
        rayCasterSettings->interactionStepSize = properties->mSciDouble()
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

    else if (property == normalCurveSettings->startIsoSurfaceProp ||
             property == normalCurveSettings->stopIsoSurfaceProp)
    {
        int startIndex = properties->mEnum()->value(
                    normalCurveSettings->startIsoSurfaceProp);
        int stopIndex = properties->mEnum()->value(
                    normalCurveSettings->stopIsoSurfaceProp);
        if (startIndex < rayCasterSettings->isoValueSetList.size())
        {
            normalCurveSettings->startIsoValue =
                    rayCasterSettings->isoValueSetList.at(startIndex).isoValue;
        }
        else
        {
            normalCurveSettings->startIsoValue =
                    rayCasterSettings->isoValueSetList.at(0).isoValue;
        }

        if (stopIndex < rayCasterSettings->isoValueSetList.size())
        {
            normalCurveSettings->stopIsoValue =
                    rayCasterSettings->isoValueSetList.at(stopIndex).isoValue;
        }
        else
        {
            normalCurveSettings->stopIsoValue =
                    rayCasterSettings->isoValueSetList.at(0).isoValue;
        }

        if (normalCurveSettings->threshold == NormalCurveSettings::Threshold::Steps)
        {
            normalCurveSettings->integrationDir =
                            static_cast<NormalCurveSettings::IntegrationDir>(
                                properties->mEnum()->value(normalCurveSettings->integrationDirProp));
        }
        else
        {
            if (normalCurveSettings->startIsoValue > normalCurveSettings->stopIsoValue)
            {
                normalCurveSettings->integrationDir = NormalCurveSettings::Backwards;
            }
            else
            {
                normalCurveSettings->integrationDir = NormalCurveSettings::Forwards;
            }
        }

        if (normalCurveSettings->normalCurvesEnabled)
        {
            updateNextRenderFrame.set(ComputeNCInitPoints);
            updateNextRenderFrame.set(RecomputeNCLines);
        }

        updateNextRenderFrame.set(UpdateShadowImage);

        emitActorChangedSignal();
    }

    else if (property == normalCurveSettings->tubeRadiusProp ||
             property == normalCurveSettings->seedPointResXProp ||
             property == normalCurveSettings->seedPointResYProp ||
             property == normalCurveSettings->seedPointResZProp ||
             property == normalCurveSettings->seedPointVarianceProp ||
             property == normalCurveSettings->integrationDirProp)
    {
        normalCurveSettings->initPointResX = properties->mDouble()
                ->value(normalCurveSettings->seedPointResXProp);
        normalCurveSettings->initPointResY = properties->mDouble()
                ->value(normalCurveSettings->seedPointResYProp);
        normalCurveSettings->initPointResZ = properties->mDouble()
                ->value(normalCurveSettings->seedPointResZProp);
        normalCurveSettings->initPointVariance = properties->mDouble()
                ->value(normalCurveSettings->seedPointVarianceProp);
        normalCurveSettings->tubeRadius = properties->mDouble()
                ->value(normalCurveSettings->tubeRadiusProp);

        if (normalCurveSettings->threshold == NormalCurveSettings::Threshold::Steps)
        {
            normalCurveSettings->integrationDir =
                            static_cast<NormalCurveSettings::IntegrationDir>(
                                properties->mEnum()->value(normalCurveSettings->integrationDirProp));
        }
        else
        {
            if (normalCurveSettings->startIsoValue >= normalCurveSettings->stopIsoValue)
            {
                normalCurveSettings->integrationDir = NormalCurveSettings::Backwards;
            }
            else
            {
                normalCurveSettings->integrationDir = NormalCurveSettings::Forwards;
            }
        }

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
             property == normalCurveSettings->colourProp    ||
             property == normalCurveSettings->stepSizeProp)
    {
        normalCurveSettings->threshold =
                static_cast<NormalCurveSettings::Threshold>(
                    properties->mEnum()->value(normalCurveSettings->thresholdProp));

        if (normalCurveSettings->threshold == NormalCurveSettings::Threshold::Steps)
        {
            normalCurveSettings->integrationDirProp->setEnabled(true);
            normalCurveSettings->numLineSegmentsProp->setEnabled(true);
            normalCurveSettings->stopIsoSurfaceProp->setEnabled(false);
            normalCurveSettings->numLineSegments = properties->mInt()->value(
                        normalCurveSettings->numLineSegmentsProp);
        }
        else
        {
            normalCurveSettings->integrationDirProp->setEnabled(false);
            normalCurveSettings->numLineSegmentsProp->setEnabled(false);
            normalCurveSettings->stopIsoSurfaceProp->setEnabled(true);
//TODO (mr, 03Apr2016) -- this shouldn't be hard-coded!
            normalCurveSettings->numLineSegments = 500;
        }

//TODO (mr, 03Apr2016) -- disabled first two colour modes; hence the "+2". See
// normal curve properties constructor.
        normalCurveSettings->colour =
                static_cast<NormalCurveSettings::CurveColor>(
                    properties->mEnum()->value(normalCurveSettings->colourProp) + 2);
        normalCurveSettings->stepSize = properties->mDouble()->value(
                    normalCurveSettings->stepSizeProp);

        updateNextRenderFrame.set(ComputeNCInitPoints);
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
        case RenderMode::DVR:
            var->ensembleSingleMemberProperty->setEnabled(true);
            var->setEnsembleMember(properties->getEnumItem(
                                       var->ensembleSingleMemberProperty).toInt());

            updateNextRenderFrame.set(UpdateShadowImage);

            var->useFlags(false);

            break;

        case RenderMode::Bitfield:
            normalCurveSettings->groupProp->setEnabled(false);

            var->ensembleSingleMemberProperty->setEnabled(true);
            var->setEnsembleMember(properties->getEnumItem(
                                       var->ensembleSingleMemberProperty).toInt());

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

    else if (property == rayCasterSettings->addIsoValueProp)
    {
        if (rayCasterSettings->isoValueSetList.size() < MAX_ISOSURFACES)
        {
            enableEmissionOfActorChangedSignal(false);
            rayCasterSettings->addIsoValue();

            // Update normal curve properties that link to isovalues.
            int startIsoIndex =
                    properties->mEnum()->value(normalCurveSettings->startIsoSurfaceProp);
            int stopIsoIndex =
                    properties->mEnum()->value(normalCurveSettings->stopIsoSurfaceProp);

            QStringList names;
            for (int i = 0; i < rayCasterSettings->isoValueSetList.size(); i++)
            {
                names << rayCasterSettings->isoValueSetList.at(i).groupProp->propertyName();
            }

            properties->mEnum()->setEnumNames(normalCurveSettings->startIsoSurfaceProp, names);
            properties->mEnum()->setEnumNames(normalCurveSettings->stopIsoSurfaceProp, names);
            properties->mEnum()->setValue(normalCurveSettings->startIsoSurfaceProp, startIsoIndex);
            properties->mEnum()->setValue(normalCurveSettings->stopIsoSurfaceProp, stopIsoIndex);

            enableEmissionOfActorChangedSignal(true);
        }

        return;
    }

    else
    {
        int counter = 0;
        int removedIsoValueIndex = -1;
        for (auto it = rayCasterSettings->isoValueSetList.begin();
             it != rayCasterSettings->isoValueSetList.end(); ++it)
        {
            if (removedIsoValueIndex >= 0)
            {
                enableEmissionOfActorChangedSignal(false);
                QString oldDisplayText = it->groupProp->propertyName();
                int hashStringPosition = oldDisplayText.indexOf("#");
                QString oldTextFront   = oldDisplayText.mid(0, hashStringPosition + 1);
                it->groupProp->setPropertyName(
                            oldTextFront + QString::number(counter++));
                enableEmissionOfActorChangedSignal(true);
                continue;
            }

            if (property == it->isoValueRemoveProp)
            {

                if (rayCasterSettings->isoValueSetList.size() == 1) break;
                properties->mBool()->setValue(it->enabledProp, false);
                rayCasterSettings->isoValuesProp->removeSubProperty(
                          it->groupProp);
                QString oldDisplayText  = it->groupProp->propertyName();
                int hashStringPosition  = oldDisplayText.indexOf("#");
                int oldIndex            =
                      oldDisplayText.mid(hashStringPosition + 1).toInt();
                removedIsoValueIndex    = oldIndex;
                counter                 = oldIndex - 1;
                enableEmissionOfActorChangedSignal(false);
                if (it != rayCasterSettings->isoValueSetList.begin())
                {
                    --it;
                }
                else
                {
                    auto begin =
                          rayCasterSettings->isoValueSetList.begin();
                    QString oldDisplayText  = begin->groupProp->propertyName();
                    int hashStringPosition  = oldDisplayText.indexOf("#");
                    QString oldTextFront    =
                          oldDisplayText.mid(0, hashStringPosition + 1);
                    int oldIndex            =
                          oldDisplayText.mid(hashStringPosition + 1).toInt();
                    counter = oldIndex;
                    begin->groupProp->setPropertyName(
                              oldTextFront + QString::number(oldIndex - 1));
                }
                enableEmissionOfActorChangedSignal(true);
				// erase value only after iterator is not used anymore
				rayCasterSettings->isoValueSetList.erase(it);
				return;
            }

            else if ( property == it->enabledProp ||
                 property == it->isoValueProp )
            {
                it->enabled = properties->mBool()->value(it->enabledProp);
                it->isoValue = properties->mSciDouble()->value(it->isoValueProp);
                int startIsoIndex = properties->mEnum()->value(
                            normalCurveSettings->startIsoSurfaceProp);
                int stopIsoIndex = properties->mEnum()->value(
                            normalCurveSettings->stopIsoSurfaceProp);
                if (("isovalue #") + QString::number(startIsoIndex + 1) ==
                        it->groupProp->propertyName())
                {
                    normalCurveSettings->startIsoValue = it->isoValue;
                }

                if (("isovalue #") + QString::number(stopIsoIndex + 1) ==
                        it->groupProp->propertyName())
                {
                    normalCurveSettings->stopIsoValue = it->isoValue;
                }
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
            else if ( property == it->isoValueSignificantDigitsProperty )
            {
                int significantDigits = properties->mInt()->value(
                            it->isoValueSignificantDigitsProperty);
                properties->mSciDouble()->setSignificantDigits(
                            it->isoValueProp, significantDigits);
                properties->mSciDouble()->setSignificantDigits(
                            it->isoValueSingleStepProperty, significantDigits);
            }
            else if ( property == it->isoValueSingleStepProperty)
            {
                double singleStep = properties->mSciDouble()->value(
                            it->isoValueSingleStepProperty);
                properties->mSciDouble()->setSingleStep(
                            it->isoValueProp, singleStep);
            }
        } // isovalues

        if (removedIsoValueIndex >= 0)
        {
            int startIsoIndex =
                    properties->mEnum()->value(
                        normalCurveSettings->startIsoSurfaceProp);
            int stopIsoIndex =
                    properties->mEnum()->value(
                        normalCurveSettings->stopIsoSurfaceProp);

            QStringList names;
            for (int i = 0; i < rayCasterSettings->isoValueSetList.size(); i++)
            {
                names << rayCasterSettings->isoValueSetList.at(i).groupProp->propertyName();
            }
            properties->mEnum()->setEnumNames(
                        normalCurveSettings->startIsoSurfaceProp, names);
            properties->mEnum()->setEnumNames(
                        normalCurveSettings->stopIsoSurfaceProp, names);
            properties->mEnum()->setValue(
                        normalCurveSettings->startIsoSurfaceProp, startIsoIndex);
            properties->mEnum()->setValue(
                        normalCurveSettings->stopIsoSurfaceProp, stopIsoIndex);

            if (startIsoIndex == removedIsoValueIndex)
            {
                properties->mEnum()->setValue(
                            normalCurveSettings->startIsoSurfaceProp, 0);
            }
            else if (startIsoIndex > removedIsoValueIndex)
            {
                properties->mEnum()->setValue(
                            normalCurveSettings->startIsoSurfaceProp, startIsoIndex - 1);
            }
            else
            {
                properties->mEnum()->setValue(
                            normalCurveSettings->startIsoSurfaceProp, startIsoIndex);
            }

            if (stopIsoIndex == removedIsoValueIndex)
            {
                properties->mEnum()->setValue(
                            normalCurveSettings->stopIsoSurfaceProp, 0);
            }
            else if (stopIsoIndex > removedIsoValueIndex)
            {
                properties->mEnum()->setValue(
                            normalCurveSettings->stopIsoSurfaceProp, stopIsoIndex - 1);
            }
            else
            {
                properties->mEnum()->setValue(
                            normalCurveSettings->stopIsoSurfaceProp, stopIsoIndex);
            }
            emitActorChangedSignal();
        }
    }
}


void MNWPVolumeRaycasterActor::renderToCurrentContext(
        MSceneViewGLWidget* sceneView)
{
    // Check for available bounding box.
    // =================================
    if ( bBoxConnection->getBoundingBox() == nullptr )
    {
        return;
    }

    // Render volume bounding box
    // ==========================
    if (bBoxEnabled)
    {
        renderBoundingBox(sceneView);
    }

    // Check for valid actor variables.
    // ================================

    if ( variables.empty() )
    {
        return;
    }

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

    // Render depth of normal curve segments to depth buffer -- needs to be
    // called before createShadowImage() as the latter requires the depth
    // buffer in the shader.
    renderToDepthTexture(sceneView);

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

    // Raycaster.
    // ==========

    switch(renderMode)
    {
    case RenderMode::Original:
    case RenderMode::DVR:
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
    for (int i = 0; i < rayCasterSettings->isoValueSetList.size(); i++)
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
    if ( bBoxConnection->getBoundingBox() == nullptr )
    {
        return;
    }

    // Define geometry for bounding box
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

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
        vertexData[i * 3 + 0] = bBoxConnection->westLon() + vertexData[i * 3 + 0]
                * (bBoxConnection->eastLon() - bBoxConnection->westLon());
        vertexData[i * 3 + 1] = bBoxConnection->northLat() - vertexData[i * 3 + 1]
                * (bBoxConnection->northLat() - bBoxConnection->southLat());
        vertexData[i * 3 + 2] = (vertexData[i * 3 + 2] == 0)
                ? bBoxConnection->bottomPressure_hPa()
                : bBoxConnection->topPressure_hPa();
    }

    if (gl.vboBoundingBox)
    {
        GL::MFloat3VertexBuffer* buf = dynamic_cast<GL::MFloat3VertexBuffer*>(
                    gl.vboBoundingBox);
        buf->update(vertexData, numVertices);
    }
    else
    {
        const QString vboID = QString("vbo_bbox_actor#%1").arg(myID);

        GL::MFloat3VertexBuffer* buf =
                new GL::MFloat3VertexBuffer(vboID, numVertices);

        if (glRM->tryStoreGPUItem(buf))
        {
            buf->upload(vertexData, numVertices);
            gl.vboBoundingBox = static_cast<GL::MVertexBuffer*>(
                        glRM->getGPUItem(vboID));
        }
        else
        {
            LOG4CPLUS_WARN(mlog, "WARNING: cannot store buffer for volume"
                           " bbox in GPU memory.");
            delete buf;
            return;
        }

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
                    var->transferFunction->getMaximumValue()); CHECK_GL_ERROR;
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
    const GLfloat westernBoundary = dataNWCrnr.x() - var->grid->getDeltaLon() / 2.0f;
    const GLfloat eastWestExtent = dataSECrnr.x() - dataNWCrnr.x() + var->grid->getDeltaLon();
    const GLfloat northernBoundary = dataNWCrnr.y() + var->grid->getDeltaLat() / 2.0f;
    const GLfloat northSouthExtent = dataNWCrnr.y() - dataSECrnr.y() + var->grid->getDeltaLat();

    const GLint nLon = var->grid->nlons;
    const GLint nLat = var->grid->nlats;
    const GLint nLev = var->grid->nlevs;
    const GLfloat deltaLnP = std::abs(dataSECrnr.z() - dataNWCrnr.z()) / (nLev-1);
    const GLfloat upperBoundary = dataNWCrnr.z() + deltaLnP /2.0f;
    const GLfloat verticalExtent = abs(dataNWCrnr.z() - dataSECrnr.z()) + deltaLnP;

    // Assume that lat/lon spacing is the same.
    shader->setUniformValue(structName + ".deltaLat", var->grid->getDeltaLat()); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".deltaLon", var->grid->getDeltaLon()); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".dataSECrnr", dataSECrnr); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".dataNWCrnr", dataNWCrnr); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".westernBoundary", westernBoundary); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".eastWestExtent", eastWestExtent); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".northernBoundary", northernBoundary); CHECK_GL_ERROR;
    shader->setUniformValue(structName + ".northSouthExtent", northSouthExtent); CHECK_GL_ERROR;
    shader->setUniformValue(
                structName + ".gridIsCyclicInLongitude",
                var->grid->gridIsCyclicInLongitude()); CHECK_GL_ERROR;
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
                QVector3D(bBoxConnection->eastLon(), // upper-right == north-east
                          bBoxConnection->southLat(),
                          sceneView->worldZfromPressure(
                              bBoxConnection->bottomPressure_hPa())));
    CHECK_GL_ERROR;
    shader->setUniformValue(
                "volumeTopNWCrnr",
                QVector3D(bBoxConnection->westLon(), // lower-left == south-west
                          bBoxConnection->northLat(),
                          sceneView->worldZfromPressure(
                              bBoxConnection->topPressure_hPa())));
    CHECK_GL_ERROR;

    shader->setUniformValue("isOrthographic", sceneView->orthographicModeEnabled());
    CHECK_GL_ERROR;

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


    // 1) Bind the depth buffer texture to the current program.
    if (gl.tex2DDepthBuffer)
    {
        gl.tex2DDepthBuffer->bindToTextureUnit(gl.texUnitDepthBuffer);
        shader->setUniformValue("depthTex", gl.texUnitDepthBuffer);
    }

    // 2) Set lighting params variables.

    shader->setUniformValue("lightingMode", lightingSettings->lightingMode); CHECK_GL_ERROR;
    shader->setUniformValue("ambientCoeff", lightingSettings->ambient); CHECK_GL_ERROR;
    shader->setUniformValue("diffuseCoeff", lightingSettings->diffuse); CHECK_GL_ERROR;
    shader->setUniformValue("specularCoeff", lightingSettings->specular); CHECK_GL_ERROR;
    shader->setUniformValue("shininessCoeff", lightingSettings->shininess); CHECK_GL_ERROR;
    shader->setUniformValue("shadowColor", lightingSettings->shadowColor); CHECK_GL_ERROR;

    // 3) Set raycaster shader variables.

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
                "numIsoValues", GLint(rayCasterSettings->isoValueSetList.size())); CHECK_GL_ERROR;

    shader->setUniformValue(
                "renderingMode", GLint(renderMode)); CHECK_GL_ERROR;

    // 4) Set shadow setting variables.

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
                    "tfMaximum", var->transferFunction->getMaximumValue());
    }

    shader->setUniformValue(
                "normalized", GLboolean(
                    normalCurveSettings->colour != NormalCurveSettings::ColorIsoValue));

    shader->setUniformValue("tubeRadius", normalCurveSettings->tubeRadius); CHECK_GL_ERROR;

    gl.tex2DDepthBuffer->bindToTextureUnit(gl.texUnitDepthBuffer);
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
    if (rayCasterSettings->isoValueSetList.size() == 0) return;

    // Don't render DVR if observered variable doesn't have a transfer function
    // assigned.
    if ((renderMode == RenderMode::DVR) && var->transferFunction == nullptr)
    {
        return;
    }

    effect->bindProgram("Volume");

    setRayCasterShaderVars(effect, sceneView); CHECK_GL_ERROR;

    //pEffect->printSubroutineInformation(GL_FRAGMENT_SHADER);

    switch(renderMode)
    {
    case RenderMode::Original:
    case RenderMode::DVR:
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

    //float zMin = sceneView->worldZfromPressure(boundingBoxActor->bottomPressure());
    float zMax = sceneView->worldZfromPressure(bBoxConnection->topPressure_hPa());
    //float zDist = zMax - zMin;

    // create current vertex data (positions of bounding box lad)
    float vertexData[] =
    {
        -1,-1,
        float(bBoxConnection->westLon()), float(bBoxConnection->southLat()),
        zMax, -1 ,1,
        float(bBoxConnection->westLon()), float(bBoxConnection->northLat()),
        zMax, 1,-1,
        float(bBoxConnection->eastLon()), float(bBoxConnection->southLat()),
        zMax, 1, 1,
        float(bBoxConnection->eastLon()), float(bBoxConnection->northLat()),
        zMax,
    };


    const GLint numVertices = 20;

    if (gl.vboShadowImage)
    {
        GL::MFloatVertexBuffer* buf = dynamic_cast<GL::MFloatVertexBuffer*>(
                    gl.vboShadowImage);
        buf->update(vertexData, numVertices, 0, 0, sceneView);
    }
    else
    {
        const QString vboID = QString("vbo_shadowimage_actor_#%1").arg(myID);

        GL::MFloatVertexBuffer* newVB =
                new GL::MFloatVertexBuffer(vboID, numVertices);

        if (glRM->tryStoreGPUItem(newVB))
        {
            newVB->upload(vertexData, numVertices, sceneView);
            gl.vboShadowImage = static_cast<GL::MVertexBuffer*>(
                        glRM->getGPUItem(vboID));
        }
        else
        {
            LOG4CPLUS_WARN(mlog, "WARNING: cannot store buffer for shadow"
                           " image bbox in GPU memory.");
            delete newVB;
            return;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);  CHECK_GL_ERROR;

    float ratio = lonDist / latDist;

    int resX = 1 << (9 + static_cast<int>(rayCasterSettings->shadowsResolution));
    int resY = static_cast<int>(std::ceil(resX / ratio));

    GLuint tempFBO = 0;
    glGenFramebuffers(1, &tempFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, tempFBO);

    GLint oldResX = 0;
    GLint oldResY = 0;

    if (gl.tex2DShadowImage)
    {
        gl.tex2DShadowImage->bindToTextureUnit(gl.texUnitShadowImage);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &oldResX);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &oldResY);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Create new framebuffer texture if none exists, or update its size if
    // the resolution has changed.
    if (!gl.tex2DShadowImage || oldResX != resX || oldResY != resY)
    {
        if (!gl.tex2DShadowImage)
        {
            const QString shadowImageTextureID =
                    QString("shadow_image_2D_actor_#%1").arg(myID);

            gl.tex2DShadowImage = new GL::MTexture(shadowImageTextureID,
                                                   GL_TEXTURE_2D, GL_RGBA32F,
                                                   resX, resY);

            if (glRM->tryStoreGPUItem(gl.tex2DShadowImage))
            {
                gl.tex2DShadowImage = dynamic_cast<GL::MTexture*>(
                            glRM->getGPUItem(shadowImageTextureID));

            }
            else
            {
                LOG4CPLUS_WARN(mlog, "WARNING: cannot store texture for shadow"
                               " image in GPU memory.");
                delete gl.tex2DShadowImage;
                gl.tex2DShadowImage = nullptr;
                return;
            }
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
    glClearColor(0, 0, 0, 0);
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
    case RenderMode::DVR:
        pEffect = gl.rayCasterEffect;
        break;
    case RenderMode::Bitfield:
        pEffect = gl.bitfieldRayCasterEffect;
        break;
    }

    pEffect->bindProgram("Shadow");
    setRayCasterShaderVars(pEffect, sceneView);
    pEffect->setUniformValue("shadowMode", GLint(RenderMode::ShadowMap));
    pEffect->setUniformValue("stepSize", rayCasterSettings->stepSize);

    // set indices of subroutines
 //   pEffect->printSubroutineInformation(GL_FRAGMENT_SHADER);

    switch(renderMode)
    {
    case RenderMode::Original:
    case RenderMode::DVR:
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
    // Don't render DVR shadow if observered variable doesn't have a transfer
    // function assigned.
    if ((renderMode == RenderMode::DVR) && var->transferFunction == nullptr)
    {
        return;
    }

    float vertexData[] =
    {
        float(bBoxConnection->westLon()), float(bBoxConnection->southLat()),
        0.01f, 0.0f, 0.0f,

        float(bBoxConnection->westLon()), float(bBoxConnection->northLat()),
        0.01f, 0.0f, 1.0f,

        float(bBoxConnection->eastLon()), float(bBoxConnection->southLat()),
        0.01f, 1.0f, 0.0f,

        float(bBoxConnection->eastLon()), float(bBoxConnection->northLat()),
        0.01f, 1.0f, 1.0f,
    };

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    const GLint numVertices = 20;

    // Check buffer for shadow map bounding box.
    if (gl.vboShadowImageRender)
    {
        // Update buffer.
        GL::MFloatVertexBuffer* buf = dynamic_cast<GL::MFloatVertexBuffer*>(
                    gl.vboShadowImageRender);
        buf->update(vertexData, numVertices, 0,  0, sceneView);
    }
    else
    {
        // Create new buffer.
        const QString vboID = QString("vbo_shadowmap_bbox_actor#%1").arg(myID);

        GL::MFloatVertexBuffer* newVB =
                new GL::MFloatVertexBuffer(vboID, numVertices);

        if (glRM->tryStoreGPUItem(newVB))
        {
            newVB->upload(vertexData, numVertices, sceneView);
            gl.vboShadowImageRender = static_cast<GL::MVertexBuffer*>(
                        glRM->getGPUItem(vboID));
        }
        else
        {
            LOG4CPLUS_WARN(mlog, "WARNING: cannot store buffer for shadow"
                           " image bbox in GPU memory.");
            delete newVB;
            return;
        }
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

        glAlphaFunc(GL_GREATER, 0.01);
        glEnable(GL_ALPHA_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); CHECK_GL_ERROR;
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); CHECK_GL_ERROR;
//TODO (ch, 23Mar2016) -- is this correct and safe?
        glDisable(GL_ALPHA_TEST);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void MNWPVolumeRaycasterActor::computeNormalCurveInitialPoints(
        MSceneViewGLWidget* sceneView)
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    numNormalCurveInitPoints = 0;
    updateNextRenderFrame.reset(ComputeNCInitPoints);

    // Compute minimum and maximum z-values of the data.
    const float dataMinZ =
            sceneView->worldZfromPressure(bBoxConnection->bottomPressure_hPa());
    const float dataMaxZ =
            sceneView->worldZfromPressure(bBoxConnection->topPressure_hPa());

    // Determine seed points grid spacing.
    const float gridSpaceLon = normalCurveSettings->initPointResX;
    const float gridSpaceLat = normalCurveSettings->initPointResY;
    const float gridSpaceHeight = normalCurveSettings->initPointResZ;

    // Compute data extent in lon, lat and height domain.
    const float dataExtentLon =
            std::abs(bBoxConnection->eastLon() - bBoxConnection->westLon());
    const float dataExtentLat =
            std::abs(bBoxConnection->northLat() - bBoxConnection->southLat());
    const float dataExtentHeight = std::abs(dataMaxZ - dataMinZ);

    // Compute the number of rays to be shot through the scene in X/Y/Z parallel
    // direction. Used for number of threads started on GPU (see below).
    const uint16_t numRaysLon = dataExtentLon / gridSpaceLon + 1;
    const uint16_t numRaysLat = dataExtentLat / gridSpaceLat + 1;
    const uint16_t numRaysHeight = dataExtentHeight / gridSpaceHeight + 1;

    const uint32_t numRays = numRaysLon * numRaysLat
                             + numRaysLon * numRaysHeight
                             + numRaysLat * numRaysHeight;

    // Make resource manager to the current context.
    glRM->makeCurrent();

    // Create a 3D texture storing the ghost grid over the domain (to avoid
    // multiple curves seeded close to each other).
    QString ghostTexID = QString("normalcurves_ghost_grid_#%1").arg(myID);

    GL::MTexture* ghostGridTex3D = nullptr;
    ghostGridTex3D = static_cast<GL::MTexture*>(glRM->getGPUItem(ghostTexID));

    if (ghostGridTex3D == nullptr)
    {
        ghostGridTex3D = new GL::MTexture(ghostTexID, GL_TEXTURE_3D, GL_R32UI,
                                          numRaysLon, numRaysLat, numRaysHeight);

        if (!glRM->tryStoreGPUItem(ghostGridTex3D))
        {
            LOG4CPLUS_WARN(mlog, "WARNING: cannot store texture for normal curves"
                           " ghost grid in GPU memory, skipping normal curves"
                           " computation.");
            delete ghostGridTex3D;
            return;
        }
    }
    else
    {
        ghostGridTex3D->updateSize(numRaysLon, numRaysLat, numRaysHeight);
    }

    // Initialise ghost grid with zeros.
    ghostGridTex3D->bindToLastTextureUnit(); CHECK_GL_ERROR;
    QVector<GLint> nullData(numRaysLon * numRaysLat * numRaysHeight, 0);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32I, numRaysLon, numRaysLat, numRaysHeight, 0,
                 GL_RED_INTEGER, GL_INT, nullData.data()); CHECK_GL_ERROR;

    const GLint ghostGridImageUnit = assignImageUnit();

    glBindTexture(GL_TEXTURE_3D, 0); CHECK_GL_ERROR;

    const GLuint MAX_ESTIMATE_CROSSINGS = 2;
    const int MAX_INITPOINTS = numRays * MAX_ESTIMATE_CROSSINGS;

    // Create a shader storage buffer containing all possible init points.
    QVector<QVector4D> initData(MAX_INITPOINTS, QVector4D(-1,-1,-1,-1));

    if (gl.ssboInitPoints == nullptr)
    {
        const QString ssboInitPointsID =
                QString("normalcurves_ssbo_init_points_#%1").arg(myID);
        gl.ssboInitPoints = new GL::MShaderStorageBufferObject(
                    ssboInitPointsID, sizeof(QVector4D), MAX_INITPOINTS);

        if (glRM->tryStoreGPUItem(gl.ssboInitPoints))
        {
            // Obtain reference to SSBO item.
            gl.ssboInitPoints = static_cast<GL::MShaderStorageBufferObject*>(
                        glRM->getGPUItem(ssboInitPointsID));
        }
        else
        {
            LOG4CPLUS_WARN(mlog, "WARNING: cannot store buffer for normal curves"
                           " init points in GPU memory, skipping normal curves"
                           " computation.");
            delete gl.ssboInitPoints;
            gl.ssboInitPoints = nullptr;
            return;
        }
    }
    else
    {
        gl.ssboInitPoints->updateSize(MAX_INITPOINTS);
    }

    gl.ssboInitPoints->upload(initData.data(), GL_DYNAMIC_COPY);

    // Create an atomic counter to control the writes to the SSBO.
    GLuint atomicCounter = 0;
    GLuint atomicBuffer = 0;
    glGenBuffers(1, &atomicBuffer); CHECK_GL_ERROR;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicBuffer); CHECK_GL_ERROR;
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), &atomicCounter,
                 GL_DYNAMIC_DRAW); CHECK_GL_ERROR;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0); CHECK_GL_ERROR;

    // Bind the compute shader and set required shader variables.
    gl.normalCurveInitPointsShader->bind();
    setCommonShaderVars(gl.normalCurveInitPointsShader, sceneView);

    gl.normalCurveInitPointsShader->setUniformSubroutineByName(
                GL_COMPUTE_SHADER, gl.normalInitSubroutines[var->grid->getLevelType()]);

    // Bind the atomic counter to the binding index 0.
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicBuffer);
    // Bind the SSBO to the binding index 0.
    gl.ssboInitPoints->bindToIndex(0);

    gl.normalCurveInitPointsShader->setUniformValue(
                    "isoValue", normalCurveSettings->startIsoValue);

    gl.normalCurveInitPointsShader->setUniformValue(
                "stepSize", rayCasterSettings->stepSize);
    gl.normalCurveInitPointsShader->setUniformValue(
                "bisectionSteps", rayCasterSettings->bisectionSteps);

    const QVector3D initWorldPos(bBoxConnection->westLon(),
                                 bBoxConnection->southLat(), dataMinZ);
    gl.normalCurveInitPointsShader->setUniformValue(
                "initWorldPos", initWorldPos);
    gl.normalCurveInitPointsShader->setUniformValue(
                "bboxMin", QVector3D(bBoxConnection->westLon(),
                                     bBoxConnection->southLat(),
                                     dataMinZ));
    gl.normalCurveInitPointsShader->setUniformValue(
                "bboxMax", QVector3D(bBoxConnection->eastLon(),
                                     bBoxConnection->northLat(),
                                     dataMaxZ));

    // Set direction specific shader vars.

    // Different ray-casting directions.
    const QVector3D castDirection[] = {
        QVector3D(0, 0, 1), QVector3D(0, 1, 0), QVector3D(1, 0, 0) };

    // Maximum length of each ray.
    float maxRayLength[] = { dataExtentHeight, dataExtentLat, dataExtentLon };

    const QVector3D deltaGridX[] = { QVector3D(gridSpaceLon,0,0),
                                     QVector3D(gridSpaceLon,0,0),
                                     QVector3D(0,gridSpaceLat,0) };

    const QVector3D deltaGridY[] = { QVector3D(0,gridSpaceLat,0),
                                     QVector3D(0,0,gridSpaceHeight),
                                     QVector3D(0,0,gridSpaceHeight) };

    const GLuint dispatchXLonLat = numRaysLon / 64 + 1;
    const GLuint dispatchXLatHeight = numRaysLat / 64 + 1;
    const GLuint dispatchYLonLat = numRaysLat / 2 + 1;
    const GLuint dispatchYLonHeight = numRaysHeight / 2 + 1;

    const QPoint dispatches[] = { QPoint(dispatchXLonLat, dispatchYLonLat),
                                  QPoint(dispatchXLonLat, dispatchYLonHeight),
                                  QPoint(dispatchXLatHeight, dispatchYLonHeight) };

    const QPoint maxNumRays[] = { QPoint(numRaysLon, numRaysLat),
                                  QPoint(numRaysLon, numRaysHeight),
                                  QPoint(numRaysLat, numRaysHeight) };

    // Maximum extent of all 3D dimensions.
    const GLuint maxRes = std::max(numRaysLon, std::max(numRaysLat, numRaysHeight));

    // Create a texture to distort the start position of the rays.
    const QString distortTexID =
            QString("normalcurves_displacement_texture_#%1").arg(myID);
    GL::MTexture* distortTex2D = static_cast<GL::MTexture*>(
                glRM->getGPUItem(distortTexID));

    if (distortTex2D == nullptr)
    {
        distortTex2D = new GL::MTexture(distortTexID, GL_TEXTURE_2D, GL_RG32F,
                                        maxRes, maxRes);

        if (!glRM->tryStoreGPUItem(distortTex2D))
        {
            LOG4CPLUS_WARN(mlog, "WARNING: cannot store texture for normal curves"
                           " displacement grid in GPU memory, skipping normal curves"
                           " computation.");
            delete distortTex2D;
            return;
        }
    }
    else
    {
        distortTex2D->updateSize(maxRes, maxRes);
    }

    std::default_random_engine engine;
    std::uniform_real_distribution<float> distribution(
                    -normalCurveSettings->initPointVariance,
                     normalCurveSettings->initPointVariance);

    // Compute random distortion values.
    QVector<float> texels(maxRes * maxRes * 2);
    for (int i = 0; i < texels.size(); ++i) { texels[i] = distribution(engine); }

    // Set data of distort texture.
    const GLuint distortTexUnit = assignTextureUnit();

    distortTex2D->bindToTextureUnit(distortTexUnit);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, maxRes, maxRes, 0, GL_RG, GL_FLOAT,
                 texels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); CHECK_GL_ERROR;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); CHECK_GL_ERROR;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP); CHECK_GL_ERROR;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP); CHECK_GL_ERROR;
    glBindTexture(GL_TEXTURE_2D, 0);

    // Bind to texture unit.
    distortTex2D->bindToTextureUnit(distortTexUnit);
    gl.normalCurveInitPointsShader->setUniformValue("distortTex", distortTexUnit);
    gl.normalCurveInitPointsShader->setUniformValue("doubleIntegration",
                    (normalCurveSettings->integrationDir == NormalCurveSettings::Both));

    // Bind ghost grid as image3D to the shader.
    gl.normalCurveInitPointsShader->setUniformValue(
                "ghostGrid", ghostGridImageUnit); CHECK_GL_ERROR;

    glBindImageTexture(ghostGridImageUnit, // image unit
                       ghostGridTex3D->getTextureObject(), // texture object
                       0,                        // level
                       GL_TRUE,                  // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       GL_R32I); CHECK_GL_ERROR; // format

    // For each plane cast rays along a regular grid and search for intersection points
    // We compute the intersection points on GPU using Compute Shader (we do not need the rasterizer here).
    for (int i = 0; i < 3; ++i)
    {
        gl.normalCurveInitPointsShader->setUniformValue(
                    "castingDirection", castDirection[i]); CHECK_GL_ERROR;
        gl.normalCurveInitPointsShader->setUniformValue(
                    "maxRayLength", GLfloat(maxRayLength[i])); CHECK_GL_ERROR;

        gl.normalCurveInitPointsShader->setUniformValue(
                    "deltaGridX", deltaGridX[i]); CHECK_GL_ERROR;
        gl.normalCurveInitPointsShader->setUniformValue(
                    "deltaGridY", deltaGridY[i]); CHECK_GL_ERROR;

        gl.normalCurveInitPointsShader->setUniformValue(
                    "maxNumRays", maxNumRays[i]); CHECK_GL_ERROR;

        glDispatchCompute(GLint(dispatches[i].x()), GLint(dispatches[i].y()), 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // DEBUGGING
//    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl.ssboNormalCurves);
//    QVector<NormalCurveLineSegment> lines(200);//numRays * (normalCurveSettings->numLineSegments + 2));

//    GLint bufMask = GL_MAP_READ_BIT;
//    NormalCurveLineSegment* vertices = (NormalCurveLineSegment*) glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
//                numRays * (normalCurveSettings->numLineSegments + 2) * sizeof(NormalCurveLineSegment), bufMask); CHECK_GL_ERROR;

//    for(int i = 0; i < 200; ++i)
//    {
//        lines[i] = vertices[i];
//    }

//    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Obtain the number of detected init points from the atomic counter.
    GLuint counter = 0;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicBuffer);
    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &counter);
    //std::cout << "numInitPoints: " << counter << std::endl << std::flush;

    numNormalCurveInitPoints = counter;

    // Release temporary resources and texture/image units.
    glDeleteBuffers(1, &atomicBuffer);

    glRM->releaseGPUItem(distortTexID);
    releaseTextureUnit(distortTexUnit);

    glRM->releaseGPUItem(ghostTexID);
    releaseImageUnit(ghostGridImageUnit);

    // Set sceneView to the current context again.
    sceneView->makeCurrent();
}


void MNWPVolumeRaycasterActor::setNormalCurveComputeShaderVars(
        std::shared_ptr<GL::MShaderEffect>& shader, MSceneViewGLWidget* sceneView)
{
    setCommonShaderVars(shader, sceneView);

    // Set subroutine indices.
    //pEffect->printSubroutineInformation(GL_COMPUTE_SHADER);

    shader->setUniformSubroutineByName(
                GL_COMPUTE_SHADER,
                gl.normalCompSubroutines[var->grid->getLevelType()]);


    shader->setUniformValue(
                "integrationStepSize", normalCurveSettings->stepSize); CHECK_GL_ERROR;
    shader->setUniformValue(
                "maxNumLines", GLint(numNormalCurveInitPoints)); CHECK_GL_ERROR;
    shader->setUniformValue(
                "maxNumLineSegments", GLint(normalCurveSettings->numLineSegments)); CHECK_GL_ERROR;
    shader->setUniformValue(
                "bisectionSteps", GLint(5)); CHECK_GL_ERROR;

    shader->setUniformValue(
                "isoValueStop", normalCurveSettings->stopIsoValue); CHECK_GL_ERROR;

    shader->setUniformValue(
                "colorMode", int(normalCurveSettings->colour)); CHECK_GL_ERROR;
    shader->setUniformValue(
                "abortCriterion", int(normalCurveSettings->threshold)); CHECK_GL_ERROR;
}


void MNWPVolumeRaycasterActor::computeNormalCurves(MSceneViewGLWidget* sceneView)
{
    if (rayCasterSettings->isoValueSetList.size() == 0) return;

    updateNextRenderFrame.reset(RecomputeNCLines);

    if (updateNextRenderFrame[ComputeNCInitPoints])
    {
        computeNormalCurveInitialPoints(sceneView);
    }

    if (numNormalCurveInitPoints == 0)
    {
        LOG4CPLUS_ERROR(mlog, "Warning: could not find any normal curve init "
                        "points");
        return;
    }

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    glRM->makeCurrent();

    normalCurveNumVertices = (normalCurveSettings->numLineSegments + 2)
                             * numNormalCurveInitPoints;

    // Create the normal curve line buffer for every init point.
    if (gl.ssboNormalCurves == nullptr)
    {
        const QString ssboNCCurvesID =
                QString("normalcurves_ssbo_lines_#%1").arg(myID);

        gl.ssboNormalCurves = new GL::MShaderStorageBufferObject(
                    ssboNCCurvesID, sizeof(NormalCurveLineSegment),
                    normalCurveNumVertices);

        if (glRM->tryStoreGPUItem(gl.ssboNormalCurves))
        {
            gl.ssboNormalCurves = static_cast<GL::MShaderStorageBufferObject*>(
                        glRM->getGPUItem(ssboNCCurvesID));
        }
        else
        {
            LOG4CPLUS_WARN(mlog, "WARNING: cannot store buffer for normal curves"
                           " in GPU memory, skipping normal curves computation.");
            delete gl.ssboNormalCurves;
            gl.ssboNormalCurves = nullptr;
            return;
        }
    }
    else
    {
        gl.ssboNormalCurves->updateSize(normalCurveNumVertices);
    }

    QVector<QVector4D> initData(normalCurveNumVertices, QVector4D(-1,-1,-1,-1));

    gl.ssboNormalCurves->upload(initData.data(), GL_DYNAMIC_COPY);

    // Bind compute shader and shader storage buffer object and compute lines.
    if (normalCurveSettings->integrationDir == NormalCurveSettings::Both)
    {
        gl.normalCurveLineComputeShader->bindProgram("DoubleIntegration");
    } else {
        gl.normalCurveLineComputeShader->bindProgram("SingleIntegration");
    }

    setNormalCurveComputeShaderVars(gl.normalCurveLineComputeShader, sceneView);

    // Bind the ssbo's to the corresponding binding indices.
    gl.ssboInitPoints->bindToIndex(0);
    gl.ssboNormalCurves->bindToIndex(1);

    int dispatchX = numNormalCurveInitPoints / 128 + 1;

    switch(normalCurveSettings->integrationDir)
    {
    case NormalCurveSettings::IntegrationDir::Backwards:
        gl.normalCurveLineComputeShader->setUniformValue("integrationMode", int(-1)); CHECK_GL_ERROR;
        glDispatchCompute(dispatchX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        break;

    case NormalCurveSettings::IntegrationDir::Forwards:
        gl.normalCurveLineComputeShader->setUniformValue("integrationMode", int(1)); CHECK_GL_ERROR;
        glDispatchCompute(dispatchX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        break;

    case NormalCurveSettings::Both:
    default:
        glDispatchCompute(dispatchX / 2 + 1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        break;

    }

    // DEBUG
//    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl.ssboNormalCurves); CHECK_GL_ERROR;

//    QVector<NormalCurveLineSegment> lines(200);

//    GLint bufMask = GL_MAP_READ_BIT;
//    NormalCurveLineSegment* vertices = (NormalCurveLineSegment*) glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
//                  normalCurveNumVertices * sizeof(NormalCurveLineSegment), bufMask); CHECK_GL_ERROR;

//    for (GLuint i = 0; i < 200; ++i)
//    {
//        lines[i] = vertices[i];
//    }

//    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Set sceneView to the current OpenGL context, again.
    sceneView->makeCurrent();

}


void MNWPVolumeRaycasterActor::renderNormalCurves(
        MSceneViewGLWidget* sceneView, bool toDepth, bool shadow)
{
    if (normalCurveNumVertices                    == 0 ||
        rayCasterSettings->isoValueSetList.size() == 0)
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

    setNormalCurveShaderVars(gl.normalCurveGeometryEffect, sceneView); CHECK_GL_ERROR;
    gl.normalCurveGeometryEffect->setUniformValue("toDepth", GLboolean(toDepth)); CHECK_GL_ERROR;
    gl.normalCurveGeometryEffect->setUniformValue("shadowColor", lightingSettings->shadowColor); CHECK_GL_ERROR;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    glBindBuffer(GL_ARRAY_BUFFER, gl.ssboNormalCurves->getBufferObject()); CHECK_GL_ERROR;

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
    glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, normalCurveNumVertices); CHECK_GL_ERROR;

    glBindBuffer(GL_ARRAY_BUFFER, 0);  CHECK_GL_ERROR;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    glDisable(GL_CULL_FACE); CHECK_GL_ERROR;
    glDisable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
}


void MNWPVolumeRaycasterActor::renderToDepthTexture(
        MSceneViewGLWidget* sceneView)
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    //glRM->makeCurrent();

    // Create temporary depth and frame buffer. The depth buffer is rendered
    // to the depth texture represented as FramebufferTexture2D
    // and not to the default OpenGL depth buffer.
    // This is guaranteed by the GL_DEPTH_COMPONENT and GL_DEPTH_ATTACHMENT flag.
    GLuint tempFBO = 0;
    GLuint tempDBO = 0;

    glGenFramebuffers(1, &tempFBO); CHECK_GL_ERROR;
    glBindFramebuffer(GL_FRAMEBUFFER, tempFBO);CHECK_GL_ERROR;

    const GLint width = sceneView->getViewPortWidth();
    const GLint height = sceneView->getViewPortHeight();

    glGenRenderbuffers(1, &tempDBO); CHECK_GL_ERROR;
    glBindRenderbuffer(GL_RENDERBUFFER, tempDBO); CHECK_GL_ERROR;
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height); CHECK_GL_ERROR;
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, tempDBO); CHECK_GL_ERROR;

    GLint oldWidth = 0;
    GLint oldHeight = 0;

    if (gl.tex2DDepthBuffer == nullptr)
    {
        const QString depthTexID = QString("depth_buffer_tex_#%1").arg(myID);

        gl.tex2DDepthBuffer = new GL::MTexture(depthTexID, GL_TEXTURE_2D,
                                               GL_DEPTH_COMPONENT32, width, height);

        if (glRM->tryStoreGPUItem(gl.tex2DDepthBuffer))
        {
            gl.tex2DDepthBuffer = static_cast<GL::MTexture*>(
                        glRM->getGPUItem(depthTexID));
        }
        else
        {
            LOG4CPLUS_WARN(mlog, "WARNING: cannot store texture for depth map"
                           " in GPU memory.");
            delete gl.tex2DDepthBuffer;
            gl.tex2DDepthBuffer = nullptr;
            return;
        }
    }

    gl.tex2DDepthBuffer->bindToTextureUnit(gl.texUnitDepthBuffer);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &oldWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &oldHeight);

    if (width != oldWidth || height != oldHeight)
    {
        gl.tex2DDepthBuffer->updateSize(width, height);

        glTexImage2D(GL_TEXTURE_2D, 0,
                     GL_DEPTH_COMPONENT32, width, height,
                     0, GL_DEPTH_COMPONENT,
                     GL_FLOAT, nullptr); CHECK_GL_ERROR;
    }
    else
    {
        QVector<float> texData(width * height, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        0, 0, width, height,
                        GL_DEPTH_COMPONENT,
                        GL_FLOAT, texData.data() );
    }

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); CHECK_GL_ERROR;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); CHECK_GL_ERROR;

    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                            gl.tex2DDepthBuffer->getTextureObject(), 0); CHECK_GL_ERROR;

    glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERROR;

    glClear(GL_DEPTH_BUFFER_BIT); CHECK_GL_ERROR;
    glClear(GL_COLOR_BUFFER_BIT); CHECK_GL_ERROR;

    glDisable(GL_LIGHTING); CHECK_GL_ERROR;
    glDisable(GL_CULL_FACE); CHECK_GL_ERROR;
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); CHECK_GL_ERROR;

    glBindTexture(GL_TEXTURE_2D, 0);
    //sceneView->makeCurrent();

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

    //sceneView->makeCurrent();
}

} // namespace Met3D
