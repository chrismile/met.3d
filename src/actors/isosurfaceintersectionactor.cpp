/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Michael Kern
**  Copyright 2017 Christoph Heidelmann
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
#include "isosurfaceintersectionactor.h"

// standard library imports

// related third party imports

// local application imports
#include "gxfw/memberselectiondialog.h"

using namespace std;
using namespace Met3D;

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MIsosurfaceIntersectionActor::MIsosurfaceIntersectionActor()
        : MNWPMultiVarIsolevelActor(),
          isosurfaceSource(nullptr),
          intersectionLines(nullptr),
          varTrajectoryFilter(nullptr),
          geomLengthTrajectoryFilter(nullptr),
          valueTrajectorySource(nullptr),
          linesVertexBuffer(nullptr),
          enableAutoComputation(true),
          supressActorUpdates(false),
          isCalculating(false),
          poleActor(nullptr),
          shadowMap(nullptr),
          shadowImageVBO(nullptr),
          shadowMapRes(8192),
          vboBoundingBox(nullptr),
          iboBoundingBox(0)
{
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Isosurface Intersection Actor");

    computeClickProperty = addProperty(CLICK_PROPERTY,
                                       "compute intersection",
                                       actorPropertiesSupGroup);
    enableAutoComputationProperty = addProperty(BOOL_PROPERTY,
                                                "enable auto-computation",
                                                actorPropertiesSupGroup);
    properties->mBool()->setValue(enableAutoComputationProperty,
                                  enableAutoComputation);

    computeClickProperty->setEnabled(!enableAutoComputation);

    variableSettings = std::make_shared<VariableSettings>(this);
    actorPropertiesSupGroup->addSubProperty(variableSettings->groupProp);

    lineFilterSettings = std::make_shared<LineFilterSettings>(this);
    actorPropertiesSupGroup->addSubProperty(lineFilterSettings->groupProp);

    appearanceSettings = std::make_shared<AppearanceSettings>(this);
    actorPropertiesSupGroup->addSubProperty(appearanceSettings->groupProp);

    tubeThicknessSettings = std::make_shared<TubeThicknessSettings>(this);
    appearanceSettings->groupProp->addSubProperty(
            tubeThicknessSettings->groupProp);
    tubeThicknessSettings->groupProp->setEnabled(
            appearanceSettings->colorMode == 2);

    ensembleSelectionSettings = std::make_shared<EnsembleSelectionSettings>(
            this);
    actorPropertiesSupGroup->addSubProperty(
            ensembleSelectionSettings->groupProp);

    // Bounding box.
    boundingBoxSettings = std::make_shared<BoundingBoxSettings>(this);
    actorPropertiesSupGroup->addSubProperty(boundingBoxSettings->groupProp);

    // Keep an instance of MovablePoleActor as a "subactor" to place poles
    // along jetstream core lines. This makes it easier for scientists to
    // infer the actual height of these lines in pressure.
    poleActor = std::make_shared<MMovablePoleActor>();
    poleActor->setName("Pole Actor");
    poleActor->setMovement(false);

    actorPropertiesSupGroup->addSubProperty(poleActor->getPropertyGroup());
    poleActor->setIndividualPoleHeightsEnabled(true);
    // Redraw the actor if its properties have been modified by the user.
    connect(poleActor.get(), SIGNAL(actorChanged()), this,
            SLOT(onPoleActorChanged()));

    // Observe the creation/deletion of other actors -- if these are transfer
    // functions, add to the list displayed in the transfer function property.
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    connect(glRM, SIGNAL(actorCreated(MActor * )),
            SLOT(onActorCreated(MActor * )));
    connect(glRM, SIGNAL(actorDeleted(MActor * )),
            SLOT(onActorDeleted(MActor * )));
    connect(glRM, SIGNAL(actorRenamed(MActor * , QString)),
            SLOT(onActorRenamed(MActor * , QString)));

    endInitialiseQtProperties();
}


MIsosurfaceIntersectionActor::~MIsosurfaceIntersectionActor()
{
    if (appearanceSettings->textureUnitTransferFunction >= 0)
        releaseTextureUnit(appearanceSettings->textureUnitTransferFunction);
}


/******************************************************************************
***                        SETTINGS CONSTRUCTORS                            ***
*******************************************************************************/

MIsosurfaceIntersectionActor::VariableSettings::VariableSettings(
        MIsosurfaceIntersectionActor *hostActor)
        : varsIndex({{-1, -1, -1}})
{
    MActor *a = hostActor;

    groupProp = a->addProperty(GROUP_PROPERTY, "variable settings");

    varsProperty[0] = a->addProperty(ENUM_PROPERTY, "1st source variable",
                                     groupProp);
    varsProperty[1] = a->addProperty(ENUM_PROPERTY, "2nd source variable",
                                     groupProp);
    varsProperty[2] = a->addProperty(ENUM_PROPERTY, "filter variable",
                                     groupProp);
}


MIsosurfaceIntersectionActor::LineFilterSettings::LineFilterSettings(
        MIsosurfaceIntersectionActor *hostActor)
        : valueFilter(40.0f),
          lineLengthFilter(500)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProp = a->addProperty(GROUP_PROPERTY, "line filters");

    valueFilterProperty = a->addProperty(DOUBLE_PROPERTY, "min. value",
                                         groupProp);
    properties->setDouble(valueFilterProperty, valueFilter,
                          0, 1000, 1, 1);

    lineLengthFilterProperty = a->addProperty(INT_PROPERTY,
                                              "min. line length (km)",
                                              groupProp);
    properties->setInt(lineLengthFilterProperty, lineLengthFilter, 1, 70000, 1);
}


MIsosurfaceIntersectionActor::AppearanceSettings::AppearanceSettings(
        MIsosurfaceIntersectionActor *hostActor) :
        colorMode(0),
        colorVariableIndex(-1),
        tubeRadius(0.2),
        tubeColor(QColor(255, 0, 0)),
        transferFunction(nullptr),
        textureUnitTransferFunction(-1),
        enableShadows(true),
        enableSelfShadowing(true),
        thicknessMode(0),
        polesEnabled(false),
        dropMode(3)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProp = a->addProperty(GROUP_PROPERTY, "line appearance");

    QStringList colorModes;
    colorModes << "Solid" << "Pressure" << "Mapped variable";
    colorModeProperty = a->addProperty(ENUM_PROPERTY, "colour mode",
                                       groupProp);
    properties->mEnum()->setEnumNames(colorModeProperty, colorModes);
    properties->mEnum()->setValue(colorModeProperty, 0);

    colorVariableProperty = a->addProperty(ENUM_PROPERTY, "mapped variable",
                                           groupProp);

    tubeColorProperty = a->addProperty(COLOR_PROPERTY, "solid colour",
                                       groupProp);
    properties->mColor()->setValue(tubeColorProperty, tubeColor);

    // Transfer function.
    // Scan currently available actors for transfer functions. Add TFs to
    // the list displayed in the combo box of the transferFunctionProperty.
    QStringList availableTFs;
    availableTFs << "None";
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    foreach (MActor *mactor, glRM->getActors())
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D *>(mactor))
        {
            availableTFs << tf->transferFunctionName();
        }
    }

    transferFunctionProperty = a->addProperty(ENUM_PROPERTY,
                                              "transfer function",
                                              groupProp);
    properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);

    tubeRadiusProperty = a->addProperty(DOUBLE_PROPERTY, "tube thickness",
                                        groupProp);
    properties->setDouble(tubeRadiusProperty, tubeRadius, 0.01, 10, 2, 0.01);

    QStringList thicknessModes;
    thicknessModes << "Constant" << "Mapped variable";
    thicknessModeProperty = a->addProperty(ENUM_PROPERTY, "thickness mode",
                                           groupProp);
    properties->mEnum()->setEnumNames(thicknessModeProperty, thicknessModes);
    properties->mEnum()->setValue(thicknessModeProperty, 0);

    enableShadowsProperty = a->addProperty(BOOL_PROPERTY, "render shadows",
                                           groupProp);
    properties->mBool()->setValue(enableShadowsProperty, enableShadows);

    enableSelfShadowingProperty = a->addProperty(BOOL_PROPERTY,
                                                 "enable self shadowing",
                                                 groupProp);
    properties->mBool()->setValue(enableSelfShadowingProperty,
                                  enableSelfShadowing);

    polesEnabledProperty = a->addProperty(BOOL_PROPERTY, "droplines",
                                          groupProp);
    properties->mBool()->setValue(polesEnabledProperty, polesEnabled);

    QStringList dropModes;
    dropModes << "Start" << "End" << "Start / End" << "Center" << "Maximum"
              << "Start / Center / End" << "Start / Max / End";
    dropModeProperty = a->addProperty(ENUM_PROPERTY, "drop mode",
                                      groupProp);
    properties->mEnum()->setEnumNames(dropModeProperty, dropModes);
    properties->mEnum()->setValue(dropModeProperty, dropMode);
}


MIsosurfaceIntersectionActor::TubeThicknessSettings::TubeThicknessSettings(
        MIsosurfaceIntersectionActor *hostActor)
        : mappedVariableIndex(-1),
          valueRange(50.0, 85.0),
          thicknessRange(0.01, 0.5)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProp = a->addProperty(GROUP_PROPERTY, "thickness mapping");

    mappedVariableProperty = a->addProperty(ENUM_PROPERTY, "mapped variable",
                                            groupProp);

    minValueProp = a->addProperty(DOUBLE_PROPERTY, "min value", groupProp);
    properties->setDouble(minValueProp, valueRange.x(), 0.0, 2000., 2, 10);
    maxValueProp = a->addProperty(DOUBLE_PROPERTY, "max value", groupProp);
    properties->setDouble(maxValueProp, valueRange.y(), 0.0, 2000., 2, 10);

    minProp = a->addProperty(DOUBLE_PROPERTY, "min thickness", groupProp);
    properties->setDouble(minProp, thicknessRange.x(), 0.0, 10., 2, 0.1);
    maxProp = a->addProperty(DOUBLE_PROPERTY, "max thickness", groupProp);
    properties->setDouble(maxProp, thicknessRange.y(), 0.0, 10., 2, 0.1);
}


MIsosurfaceIntersectionActor::EnsembleSelectionSettings::EnsembleSelectionSettings(
        MIsosurfaceIntersectionActor *hostActor)
        : syncModeEnabled(false)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProp = a->addProperty(GROUP_PROPERTY, "ensemble selection");

    ensembleMultiMemberSelectionProperty = a->addProperty(
            CLICK_PROPERTY, "select members", groupProp);
    ensembleMultiMemberSelectionProperty->setToolTip(
            "select which ensemble members this variable should utilize");
    ensembleMultiMemberSelectionProperty->setEnabled(true);
    ensembleMultiMemberProperty = a->addProperty(
            STRING_PROPERTY, "utilized members", groupProp);
    ensembleMultiMemberProperty->setEnabled(false);

    syncModeEnabledProperty = a->addProperty(BOOL_PROPERTY, "sync mode",
                                             groupProp);
    properties->mBool()->setValue(syncModeEnabledProperty, syncModeEnabled);

    selectedEnsembleMembers.clear();
    selectedEnsembleMembers << 0;

    properties->mString()->setValue(ensembleMultiMemberProperty,
                                    MDataRequestHelper::uintSetToString(
                                            selectedEnsembleMembers));
}


MIsosurfaceIntersectionActor::BoundingBoxSettings::BoundingBoxSettings(
        MIsosurfaceIntersectionActor *hostActor)
        : enabled(true),
          llcrnLon(-100.f), llcrnLat(20),
          urcrnLon(20), urcrnLat(80),
          pBot_hPa(1050.f),
          pTop_hPa(100.f)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    groupProp = a->addProperty(GROUP_PROPERTY, "bounding box");

    enabledProperty = a->addProperty(BOOL_PROPERTY, "enable rendering",
                                     groupProp);
    properties->mBool()->setValue(enabledProperty, enabled);

    boxCornersProp = a->addProperty(RECTF_LONLAT_PROPERTY, "corners",
                                    groupProp);
    properties->setRectF(boxCornersProp, QRectF(llcrnLon, llcrnLat,
                                                urcrnLon - llcrnLon,
                                                urcrnLat - llcrnLat), 2);

    pBotProp = a->addProperty(DOUBLE_PROPERTY, "bottom pressure", groupProp);
    properties->setDouble(pBotProp, pBot_hPa, 1050., 0.01, 2, 5.);

    pTopProp = a->addProperty(DOUBLE_PROPERTY, "top pressure", groupProp);
    properties->setDouble(pTopProp, pTop_hPa, 1050., 0.01, 2, 5.);
}

/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE 0
#define SHADER_NORMAL_ATTRIBUTE 1

void MIsosurfaceIntersectionActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);

    beginCompileShaders(4);

    compileShadersFromFileWithProgressDialog(intersectionLinesShader,
                                             "src/glsl/trajectory_tubes.fx.glsl");
    compileShadersFromFileWithProgressDialog(boundingBoxShader,
                                             "src/glsl/simple_coloured_geometry.fx.glsl");
    compileShadersFromFileWithProgressDialog(tubeShadowShader,
                                             "src/glsl/trajectory_tubes_shadow.fx.glsl");
    compileShadersFromFileWithProgressDialog(lineTubeShader,
                                             "src/glsl/simple_geometry_generation.fx.glsl");

    endCompileShaders();
}


void MIsosurfaceIntersectionActor::saveConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::saveConfiguration(settings);

    settings->beginGroup(getSettingsID());
    settings->setValue("var1stIndex", variableSettings->varsIndex[0]);
    settings->setValue("var2ndIndex", variableSettings->varsIndex[1]);
    settings->setValue("varSourceIndex", variableSettings->varsIndex[2]);

    settings->setValue("filterValue", lineFilterSettings->valueFilter);
    settings->setValue("fliterLineLength",
                       lineFilterSettings->lineLengthFilter);

    settings->setValue("colorMode", appearanceSettings->colorMode);
    settings->setValue("varColorIndex", appearanceSettings->colorVariableIndex);
    settings->setValue("transferFunction",
                       properties->getEnumItem(
                               appearanceSettings->transferFunctionProperty));

    settings->setValue("tubeRadius", appearanceSettings->tubeRadius);

    QString colorString =
            QString::number(appearanceSettings->tubeColor.red()) + "/" +
            QString::number(appearanceSettings->tubeColor.green()) + "/" +
            QString::number(appearanceSettings->tubeColor.blue());
    settings->setValue("tubeColor", colorString);

    settings->setValue("thicknessMode", appearanceSettings->thicknessMode);

    settings->setValue("enableShadows", appearanceSettings->enableShadows);
    settings->setValue("enableSelfShadowing",
                       appearanceSettings->enableSelfShadowing);
    settings->setValue("polesEnabled", appearanceSettings->polesEnabled);
    settings->setValue("dropMode", appearanceSettings->dropMode);

    settings->setValue("tubeThicknessVariableIndex",
                       tubeThicknessSettings->mappedVariableIndex);
    settings->setValue("tubeThicknessMinValue",
                       tubeThicknessSettings->valueRange.x());
    settings->setValue("tubeThicknessMaxValue",
                       tubeThicknessSettings->valueRange.y());
    settings->setValue("tubeThicknessMin",
                       tubeThicknessSettings->thicknessRange.x());
    settings->setValue("tubeThicknessMax",
                       tubeThicknessSettings->thicknessRange.y());

    settings->setValue("syncMode", ensembleSelectionSettings->syncModeEnabled);
    settings->setValue("ensembleMultiMemberProperty",
                       MDataRequestHelper::uintSetToString(
                               ensembleSelectionSettings->selectedEnsembleMembers));

    settings->setValue("enableAutoComputation", enableAutoComputation);

    settings->setValue("enabled", boundingBoxSettings->enabled);
    settings->setValue("llcrnLat", boundingBoxSettings->llcrnLat);
    settings->setValue("llcrnLon", boundingBoxSettings->llcrnLon);
    settings->setValue("urcrnLat", boundingBoxSettings->urcrnLat);
    settings->setValue("urcrnLon", boundingBoxSettings->urcrnLon);
    settings->setValue("p_bot_hPa", boundingBoxSettings->pBot_hPa);
    settings->setValue("p_top_hPa", boundingBoxSettings->pTop_hPa);

    poleActor->saveConfiguration(settings);
    settings->endGroup();
}


void MIsosurfaceIntersectionActor::loadConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::loadConfiguration(settings);

    supressActorUpdates = true;
    settings->beginGroup(getSettingsID());
    variableSettings->varsIndex[0] = settings->value("var1stIndex").toInt();
    properties->mEnum()->setValue(variableSettings->varsProperty[0],
                                  variableSettings->varsIndex[0]);
    variableSettings->varsIndex[1] = settings->value("var2ndIndex").toInt();
    properties->mEnum()->setValue(variableSettings->varsProperty[1],
                                  variableSettings->varsIndex[1]);
    variableSettings->varsIndex[2] = settings->value("varSourceIndex").toInt();
    properties->mEnum()->setValue(variableSettings->varsProperty[2],
                                  variableSettings->varsIndex[2]);

    lineFilterSettings->valueFilter = settings->value("filterValue").toFloat();
    properties->mDouble()->setValue(lineFilterSettings->valueFilterProperty,
                                    lineFilterSettings->valueFilter);
    lineFilterSettings->lineLengthFilter = settings->value(
            "fliterLineLength").toInt();
    properties->mInt()->setValue(lineFilterSettings->lineLengthFilterProperty,
                                 lineFilterSettings->lineLengthFilter);

    appearanceSettings->colorMode = settings->value("colorMode").toInt();
    properties->mEnum()->setValue(appearanceSettings->colorModeProperty,
                                  appearanceSettings->colorMode);

    appearanceSettings->colorVariableIndex = settings->value(
            "varColorIndex").toInt();
    properties->mEnum()->setValue(appearanceSettings->colorVariableProperty,
                                  appearanceSettings->colorVariableIndex);

    appearanceSettings->tubeRadius = settings->value("tubeRadius").toFloat();
    properties->mDouble()->setValue(appearanceSettings->tubeRadiusProperty,
                                    appearanceSettings->tubeRadius);

    QStringList colorString = settings->value("tubeColor").toString().split(
            "/");
    if (colorString.size() == 3)
    {
        appearanceSettings->tubeColor = QColor(colorString.at(0).toInt(),
                                               colorString.at(1).toInt(),
                                               colorString.at(2).toInt());
        properties->mColor()->setValue(appearanceSettings->tubeColorProperty,
                                       appearanceSettings->tubeColor);
    }

    QString tfName = settings->value("transferFunction").toString();
    if (!setTransferFunction(tfName))
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(QString("Trajectory actor '%1':\n"
                                       "Transfer function '%2' does not exist.\n"
                                       "Setting transfer function to 'None'.")
                               .arg(getName()).arg(tfName));
        msgBox.exec();
    }

    appearanceSettings->thicknessMode = settings->value(
            "thicknessMode").toInt();
    properties->mEnum()->setValue(appearanceSettings->thicknessModeProperty,
                                  appearanceSettings->thicknessMode);

    appearanceSettings->enableShadows = settings->value(
            "enableShadows").toBool();
    properties->mBool()->setValue(appearanceSettings->enableShadowsProperty,
                                  appearanceSettings->enableShadows);

    appearanceSettings->enableSelfShadowing = settings->value(
            "enableSelfShadowing").toBool();
    properties->mBool()->setValue(
            appearanceSettings->enableSelfShadowingProperty,
            appearanceSettings->enableSelfShadowing);

    appearanceSettings->polesEnabled = settings->value("polesEnabled").toBool();
    properties->mBool()->setValue(appearanceSettings->polesEnabledProperty,
                                  appearanceSettings->polesEnabled);
    appearanceSettings->dropMode = settings->value("dropMode").toInt();
    properties->mEnum()->setValue(appearanceSettings->dropModeProperty,
                                  appearanceSettings->dropMode);

    tubeThicknessSettings->mappedVariableIndex = settings->value(
            "tubeThicknessVariableIndex").toInt();
    properties->mEnum()->setValue(tubeThicknessSettings->mappedVariableProperty,
                                  tubeThicknessSettings->mappedVariableIndex);
    tubeThicknessSettings->valueRange.setX(
            settings->value("tubeThicknessMinValue").toDouble());
    tubeThicknessSettings->valueRange.setY(
            settings->value("tubeThicknessMaxValue").toDouble());
    tubeThicknessSettings->thicknessRange.setX(
            settings->value("tubeThicknessMin").toDouble());
    tubeThicknessSettings->thicknessRange.setY(
            settings->value("tubeThicknessMax").toDouble());
    properties->mDouble()->setValue(tubeThicknessSettings->minValueProp,
                                    tubeThicknessSettings->valueRange.x());
    properties->mDouble()->setValue(tubeThicknessSettings->maxValueProp,
                                    tubeThicknessSettings->valueRange.y());
    properties->mDouble()->setValue(tubeThicknessSettings->minProp,
                                    tubeThicknessSettings->thicknessRange.x());
    properties->mDouble()->setValue(tubeThicknessSettings->maxProp,
                                    tubeThicknessSettings->thicknessRange.y());

    tubeThicknessSettings->groupProp->setEnabled(
            appearanceSettings->thicknessMode == 1);

    ensembleSelectionSettings->syncModeEnabled = settings->value(
            "syncMode").toBool();
    properties->mBool()->setValue(
            ensembleSelectionSettings->syncModeEnabledProperty,
            ensembleSelectionSettings->syncModeEnabled);

    ensembleSelectionSettings->selectedEnsembleMembers = MDataRequestHelper::uintSetFromString(
            settings->value("ensembleMultiMemberProperty").toString());
    properties->mString()->setValue(
            ensembleSelectionSettings->ensembleMultiMemberProperty,
            settings->value("ensembleMultiMemberProperty").toString());

    enableAutoComputation = settings->value("enableAutoComputation").toBool();
    properties->mBool()->setValue(enableAutoComputationProperty,
                                  enableAutoComputation);

    computeClickProperty->setEnabled(!enableAutoComputation);

    properties->mBool()->setValue(boundingBoxSettings->enabledProperty,
                                  settings->value("enabled", true).toBool());

    boundingBoxSettings->enabled =
            properties->mBool()->value(boundingBoxSettings->enabledProperty);
    boundingBoxSettings->llcrnLat = settings->value("llcrnLat").toFloat();
    boundingBoxSettings->llcrnLon = settings->value("llcrnLon").toFloat();
    boundingBoxSettings->urcrnLat = settings->value("urcrnLat").toFloat();
    boundingBoxSettings->urcrnLon = settings->value("urcrnLon").toFloat();

    properties->mRectF()->setValue(
            boundingBoxSettings->boxCornersProp,
            QRectF(boundingBoxSettings->llcrnLon,
                   boundingBoxSettings->llcrnLat,
                   boundingBoxSettings->urcrnLon -
                   boundingBoxSettings->llcrnLon,
                   boundingBoxSettings->urcrnLat -
                   boundingBoxSettings->llcrnLat));

    boundingBoxSettings->pBot_hPa = settings->value("p_bot_hPa").toFloat();
    boundingBoxSettings->pTop_hPa = settings->value("p_top_hPa").toFloat();
    properties->mDouble()->setValue(boundingBoxSettings->pBotProp,
                                    boundingBoxSettings->pBot_hPa);
    properties->mDouble()->setValue(boundingBoxSettings->pTopProp,
                                    boundingBoxSettings->pTop_hPa);


    poleActor->loadConfiguration(settings);

    if (isInitialized())
    { generateVolumeBoxGeometry(); }

    supressActorUpdates = false;

    settings->endGroup();

    emit emitActorChangedSignal();
}


const QList<MVerticalLevelType>
MIsosurfaceIntersectionActor::supportedLevelTypes()
{
    return (QList<MVerticalLevelType>()
            << HYBRID_SIGMA_PRESSURE_3D << PRESSURE_LEVELS_3D);
}


MNWPActorVariable *MIsosurfaceIntersectionActor::createActorVariable(
        const MSelectableDataSource &dataSource)
{
    MNWPIsolevelActorVariable *newVar = new MNWPIsolevelActorVariable(this);

    newVar->dataSourceID = dataSource.dataSourceID;
    newVar->levelType = dataSource.levelType;
    newVar->variableName = dataSource.variableName;

    return newVar;
}


void
MIsosurfaceIntersectionActor::setDataSource(MIsosurfaceIntersectionSource *ds)
{
    if (isosurfaceSource != nullptr)
    {
        disconnect(isosurfaceSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousDataAvailable(MDataRequest)));
    }

    isosurfaceSource = ds;
    if (isosurfaceSource != nullptr)
    {
        connect(isosurfaceSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousDataAvailable(MDataRequest)));
    }
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

// Emit an actor changed signal if pole actor has been modified by the user.
// This causes both, the jetcore and pole actor to redraw on the current scene.
void MIsosurfaceIntersectionActor::onPoleActorChanged()
{
    emit actorChanged();
}

void MIsosurfaceIntersectionActor::onActorCreated(MActor *actor)
{
    // If the new actor is a transfer function, add it to the list of
    // available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D *>(actor))
    {
        // Don't render while the properties are being updated.
        enableEmissionOfActorChangedSignal(false);

        int index = properties->mEnum()->value(
                appearanceSettings->transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                appearanceSettings->transferFunctionProperty);
        availableTFs << tf->transferFunctionName();
        properties->mEnum()->setEnumNames(
                appearanceSettings->transferFunctionProperty,
                availableTFs);
        properties->mEnum()->setValue(
                appearanceSettings->transferFunctionProperty, index);

        enableEmissionOfActorChangedSignal(true);
    }
}


void MIsosurfaceIntersectionActor::onActorDeleted(MActor *actor)
{
    // If the deleted actor is a transfer function, remove it from the list of
    // available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D *>(actor))
    {
        enableEmissionOfActorChangedSignal(false);

        int index = properties->mEnum()->value(
                appearanceSettings->transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                appearanceSettings->transferFunctionProperty);

        // If the deleted transfer function is currently connected to this
        // variable, set current transfer function to "None" (index 0).
        if (availableTFs.at(index) == tf->getName()) index = 0;

        availableTFs.removeOne(tf->getName());
        properties->mEnum()->setEnumNames(
                appearanceSettings->transferFunctionProperty,
                availableTFs);
        properties->mEnum()->setValue(
                appearanceSettings->transferFunctionProperty, index);

        enableEmissionOfActorChangedSignal(true);
    }
}


void
MIsosurfaceIntersectionActor::onActorRenamed(MActor *actor, QString oldName)
{
    // If the renamed actor is a transfer function, change its name in the list
    // of available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D *>(actor))
    {
        // Don't render while the properties are being updated.
        enableEmissionOfActorChangedSignal(false);

        int index = properties->mEnum()->value(
                appearanceSettings->transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                appearanceSettings->transferFunctionProperty);

        // Replace affected entry.
        availableTFs[availableTFs.indexOf(oldName)] = tf->getName();

        properties->mEnum()->setEnumNames(
                appearanceSettings->transferFunctionProperty,
                availableTFs);
        properties->mEnum()->setValue(
                appearanceSettings->transferFunctionProperty, index);

        enableEmissionOfActorChangedSignal(true);
    }
}


void MIsosurfaceIntersectionActor::onQtPropertyChanged(QtProperty *property)
{
    if (supressActorUpdates) return;
    // Parent signal processing.
    MNWPMultiVarActor::onQtPropertyChanged(property);

    // Variable settings.
    if (property == variableSettings->varsProperty[0])
    {
        variableSettings->varsIndex[0] = properties->mEnum()->value(
                variableSettings->varsProperty[0]);
    } else if (property == variableSettings->varsProperty[1])
    {
        variableSettings->varsIndex[1] = properties->mEnum()->value(
                variableSettings->varsProperty[1]);
    } else if (property == variableSettings->varsProperty[2])
    {
        variableSettings->varsIndex[2] = properties->mEnum()->value(
                variableSettings->varsProperty[2]);
    } else if (property == boundingBoxSettings->boxCornersProp ||
               property == boundingBoxSettings->pBotProp ||
               property == boundingBoxSettings->pTopProp)
    {
        generateVolumeBoxGeometry();
        updateShadowImage = true;

        QRectF cornerRect = properties->mRectF()->value(
                boundingBoxSettings->boxCornersProp);

        boundingBoxSettings->llcrnLat = static_cast<float>(cornerRect.y());
        boundingBoxSettings->llcrnLon = static_cast<float>(cornerRect.x());
        boundingBoxSettings->urcrnLat = static_cast<float>(cornerRect.y() +
                                                           cornerRect.height());
        boundingBoxSettings->urcrnLon = static_cast<float>(cornerRect.x() +
                                                           cornerRect.width());

        boundingBoxSettings->pBot_hPa = static_cast<float>(
                properties->mDouble()->value(boundingBoxSettings->pBotProp));
        boundingBoxSettings->pTop_hPa = static_cast<float>(
                properties->mDouble()->value(boundingBoxSettings->pTopProp));

        if (enableAutoComputation)
        {
            requestIsoSurfaceIntersectionLines();
        }

        emitActorChangedSignal();
    } else if (property == boundingBoxSettings->enabledProperty)
    {
        boundingBoxSettings->enabled =
                properties->mBool()->value(
                        boundingBoxSettings->enabledProperty);

        emitActorChangedSignal();
    }


        // Auto-computation check box.
    else if (property == enableAutoComputationProperty)
    {
        enableAutoComputation = properties->mBool()
                ->value(enableAutoComputationProperty);

        computeClickProperty->setEnabled(!enableAutoComputation);

        if (enableAutoComputation)
        {
            requestIsoSurfaceIntersectionLines();
        }

        emitActorChangedSignal();
    }

        // If enabled, click the compute button to compute the intersection lines.
    else if (property == computeClickProperty)
    {
        requestIsoSurfaceIntersectionLines();
        emitActorChangedSignal();
    }

        // Changed one of the tube appearance settings.
    else if (property == appearanceSettings->colorModeProperty
             || property == appearanceSettings->colorVariableProperty
             || property == appearanceSettings->thicknessModeProperty
             || property == tubeThicknessSettings->mappedVariableProperty)
    {
        appearanceSettings->colorMode = properties->mEnum()->value(
                appearanceSettings->colorModeProperty);

        appearanceSettings->colorVariableIndex = properties->mEnum()->value(
                appearanceSettings->colorVariableProperty);

        appearanceSettings->thicknessMode = properties->mEnum()->value(
                appearanceSettings->thicknessModeProperty);

        tubeThicknessSettings->mappedVariableIndex = properties->mEnum()->value(
                tubeThicknessSettings->mappedVariableProperty);

        tubeThicknessSettings->groupProp->setEnabled(
                appearanceSettings->thicknessMode == 1);


        if (enableAutoComputation)
        {
            requestIsoSurfaceIntersectionLines();
        }

        emitActorChangedSignal();
    } else if (property == appearanceSettings->transferFunctionProperty)
    {
        setTransferFunctionFromProperty();
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

        // Basic tube settings (tube radius and color).
    else if (property == appearanceSettings->tubeColorProperty
             || property == appearanceSettings->tubeRadiusProperty)
    {
        appearanceSettings->tubeColor = properties->mColor()->value(
                appearanceSettings->tubeColorProperty);
        appearanceSettings->tubeRadius = properties->mDouble()->value(
                appearanceSettings->tubeRadiusProperty);

        emitActorChangedSignal();
    }

        // Tube thickness settings.
    else if (tubeThicknessSettings &&
             (property == tubeThicknessSettings->minProp
              || property == tubeThicknessSettings->maxProp
              || property == tubeThicknessSettings->minValueProp
              || property == tubeThicknessSettings->maxValueProp))
    {
        tubeThicknessSettings->valueRange.setX(
                properties->mDouble()->value(
                        tubeThicknessSettings->minValueProp));
        tubeThicknessSettings->valueRange.setY(
                properties->mDouble()->value(
                        tubeThicknessSettings->maxValueProp));

        tubeThicknessSettings->thicknessRange.setX(
                properties->mDouble()->value(tubeThicknessSettings->minProp));
        tubeThicknessSettings->thicknessRange.setY(
                properties->mDouble()->value(tubeThicknessSettings->maxProp));

        emitActorChangedSignal();
    }

        // Ensemble member selection.
    else if (property ==
             ensembleSelectionSettings->ensembleMultiMemberSelectionProperty)
    {
        if (suppressActorUpdates()) return;

        MMemberSelectionDialog dlg;
        dlg.setAvailableEnsembleMembers(
                variables.at(0)->dataSource->availableEnsembleMembers(
                        variables.at(0)->levelType,
                        variables.at(0)->variableName));
        dlg.setSelectedMembers(
                ensembleSelectionSettings->selectedEnsembleMembers);

        if (dlg.exec() == QDialog::Accepted)
        {
            // Get set of selected members from dialog, update
            // ensembleMultiMemberProperty to display set to user and, if
            // necessary, request new data field.
            QSet<unsigned int> selMembers = dlg.getSelectedMembers();
            if (!selMembers.isEmpty())
            {
                ensembleSelectionSettings->selectedEnsembleMembers = selMembers;
                // Update ensembleMultiMemberProperty to display the currently selected
                // list of ensemble members.
                QString s = MDataRequestHelper::uintSetToString
                        (ensembleSelectionSettings->selectedEnsembleMembers);
                properties->mString()->setValue(
                        ensembleSelectionSettings->ensembleMultiMemberProperty,
                        s);
                ensembleSelectionSettings->ensembleMultiMemberProperty->setToolTip(
                        s);
            } else
            {
                // The user has selected an emtpy set of members. Display a
                // warning and do NOT accept the empty set.
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setText("You need to select at least one member.");
                msgBox.exec();
            }
        }
    }
        // Enable shadow map.
    else if (property == appearanceSettings->enableShadowsProperty
             || property == appearanceSettings->enableSelfShadowingProperty)
    {
        appearanceSettings->enableShadows =
                properties->mBool()->value(
                        appearanceSettings->enableShadowsProperty);
        appearanceSettings->enableSelfShadowing =
                properties->mBool()->value(
                        appearanceSettings->enableSelfShadowingProperty);
        emitActorChangedSignal();
    }

        // Enable pole placements along intersection lines.
    else if (property == appearanceSettings->polesEnabledProperty
             || property == appearanceSettings->dropModeProperty)
    {
        appearanceSettings->polesEnabled =
                properties->mBool()->value(
                        appearanceSettings->polesEnabledProperty);

        appearanceSettings->dropMode =
                properties->mEnum()->value(
                        appearanceSettings->dropModeProperty);

        if (intersectionLines != nullptr && appearanceSettings->polesEnabled)
        {
            placePoleActors(intersectionLines);
        }

        emitActorChangedSignal();
    }

    // If any of the variables was changed or the ensemble members, filter values
    // were altered, request new intersection lines.
    if (property == variableSettings->varsProperty[0] ||
        property == variableSettings->varsProperty[1] ||
        property == variableSettings->varsProperty[2] ||
        property ==
        ensembleSelectionSettings->ensembleMultiMemberSelectionProperty ||
        property == lineFilterSettings->valueFilterProperty ||
        property == lineFilterSettings->lineLengthFilterProperty)
    {
        lineFilterSettings->lineLengthFilter =
                properties->mInt()->value(
                        lineFilterSettings->lineLengthFilterProperty);
        lineFilterSettings->valueFilter =
                properties->mDouble()->value(
                        lineFilterSettings->valueFilterProperty);

        if (enableAutoComputation)
        {
            requestIsoSurfaceIntersectionLines();
        }

        emitActorChangedSignal();

    } else
    {
        // Automatically compute the intersection lines if two different variables
        // were selected.
        if (variables.size() >= 2)
        {
            MNWPIsolevelActorVariable *var1 =
                    dynamic_cast<MNWPIsolevelActorVariable *>(
                            variables.at(variableSettings->varsIndex[0]));
            MNWPIsolevelActorVariable *var2 =
                    dynamic_cast<MNWPIsolevelActorVariable *>(variables.at(
                            variableSettings->varsIndex[1]));
            if ((property == var1->validTimeProperty ||
                 property == var2->validTimeProperty)
                && var1->getPropertyTime(var1->validTimeProperty)
                   == var2->getPropertyTime(var2->validTimeProperty))
            {
                if (enableAutoComputation)
                {
                    requestIsoSurfaceIntersectionLines();
                }

                emitActorChangedSignal();
            }
        }
    }
}


void
MIsosurfaceIntersectionActor::asynchronousDataAvailable(MDataRequest request)
{
    if (intersectionLines)
    {
        intersectionLines->releaseVertexBuffer();
        intersectionLines->releaseStartPointsVertexBuffer();
    }

    intersectionLines = isosurfaceSource->getData(request);

    MDataRequestHelper rh(lineRequest);

    buildFilterChain(rh);
    requestFilters();
}


void
MIsosurfaceIntersectionActor::asynchronousFiltersAvailable(MDataRequest request)
{
    if (!intersectionLines)
    { return; }

    lineSelection = dynamic_cast<MTrajectoryEnsembleSelection *>(
            currentTrajectoryFilter->getData(request));
    requestFilters();

    emitActorChangedSignal();
}


void
MIsosurfaceIntersectionActor::asynchronousValuesAvailable(MDataRequest request)
{
    MTrajectoryValues *values = valueTrajectorySource->getData(request);
    const QVector<float> &trajValues = values->getValues();

    // Finally build the GPU resources.
    linesData.clear();

    int counter = 0;

    // Obtain the line vertices of each line and write it to the data array.
    for (int i = 0; i < lineSelection->getNumTrajectories(); ++i)
    {
        const int startIndex = lineSelection->getStartIndices()[i];
        const int indexCount = lineSelection->getIndexCount()[i];
        const int endIndex = startIndex + indexCount;

        linesData.push_back({-1, -1, -1, -1, -1});

        for (int j = startIndex; j < endIndex; ++j)
        {
            const QVector3D &point = intersectionLines->getVertices().at(j);
            float varValue = trajValues[counter++];
            float varThicknessValue = trajValues[counter++];

            // Create new line vertex (x/y/z/value/thickness) and push it to
            // the raw data vector.
            linesData.push_back({static_cast<float>(point.x()),
                                 static_cast<float>(point.y()),
                                 static_cast<float>(point.z()),
                                 varValue, varThicknessValue});
        }

        linesData.push_back({-1, -1, -1, -1, -1});
    }

    // Place some poles
    if (appearanceSettings->polesEnabled)
    { placePoleActors(intersectionLines); }

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    glRM->makeCurrent();
    // Create vertex buffer for the intersection lines
    const QString vbKey = QString("intersection_lines_VB-%1").arg(myID);

    GL::MVertexBuffer *vb = dynamic_cast<GL::MVertexBuffer *>(glRM->getGPUItem(
            vbKey));

    if (vb)
    {
        GL::MFloat5VertexBuffer *buf = dynamic_cast<GL::MFloat5VertexBuffer *>(vb);
        buf->reallocate(nullptr, static_cast<unsigned int>(linesData.size()), 0,
                        true);
        buf->update(reinterpret_cast<GLfloat *>(linesData.data()),
                    linesData.size());
        linesVertexBuffer = vb;
    } else
    {
        GL::MFloat5VertexBuffer *newVb = new GL::MFloat5VertexBuffer(vbKey,
                                                                     linesData.size());

        if (glRM->tryStoreGPUItem(newVb))
        {
            newVb->upload(
                    reinterpret_cast<GLfloat *>(linesData.data()),
                    linesData.size());
        } else
        { delete newVb; }
        linesVertexBuffer = dynamic_cast<GL::MVertexBuffer *>(glRM->getGPUItem(
                vbKey));
    }

    supressActorUpdates = true;
    variableSettings->groupProp->setEnabled(true);
    ensembleSelectionSettings->groupProp->setEnabled(true);
    supressActorUpdates = false;

    // Re-enable the sync control.
    isCalculating = false;
    MNWPIsolevelActorVariable *var2 = dynamic_cast<MNWPIsolevelActorVariable *>(
            variables.at(variableSettings->varsIndex[1]));
    if (var2->synchronizationControl != nullptr)
    {
        var2->synchronizationControl->setEnabled(true);
    }

    emitActorChangedSignal();
}


void MIsosurfaceIntersectionActor::isoValueOfVariableChanged()
{
    if (enableAutoComputation)
    {
        requestIsoSurfaceIntersectionLines();
    }
}


void MIsosurfaceIntersectionActor::onAddActorVariable(MNWPActorVariable *var)
{
    Q_UNUSED(var);
    refreshEnumsProperties(nullptr);
}


void MIsosurfaceIntersectionActor::onDeleteActorVariable(MNWPActorVariable *var)
{
    refreshEnumsProperties(var);
}


void MIsosurfaceIntersectionActor::onChangeActorVariable(MNWPActorVariable *var)
{
    Q_UNUSED(var);
    refreshEnumsProperties(nullptr);
}

/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MIsosurfaceIntersectionActor::requestIsoSurfaceIntersectionLines()
{
    if (getViews().empty())
    { return; }

    if (isCalculating
        || variableSettings->varsIndex[0] == variableSettings->varsIndex[1])
    { return; }

    isCalculating = true;

    // If the user has selected an ensemble member and at least one variable,
    // then obtain all selected ensemble members.
    if (ensembleSelectionSettings->selectedEnsembleMembers.size() == 0
        && variables.size() > 0
        && variables.at(0)->dataSource != nullptr)
    {
        ensembleSelectionSettings->selectedEnsembleMembers = variables.at(0)->
                dataSource->availableEnsembleMembers(variables.at(0)->levelType,
                                                     variables.at(
                                                             0)->variableName);
        QString s = MDataRequestHelper::uintSetToString(
                ensembleSelectionSettings->selectedEnsembleMembers);
        properties->mString()->setValue(
                ensembleSelectionSettings->ensembleMultiMemberProperty, s);
    }

    // Create a new instance of an iso-surface intersection source if not created.
    if (isosurfaceSource == nullptr)
    {
        MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
        MAbstractScheduler *scheduler = sysMC->getScheduler("MultiThread");
        MAbstractMemoryManager *memoryManager = sysMC->getMemoryManager("NWP");

        isosurfaceSource = new MIsosurfaceIntersectionSource();
        isosurfaceSource->setScheduler(scheduler);
        isosurfaceSource->setMemoryManager(memoryManager);

        setDataSource(isosurfaceSource);
        sysMC->registerDataSource("isosurfaceIntersectionlines",
                                  isosurfaceSource);
    }

    //  Release the current intersection lines.
    if (intersectionLines)
    {
        intersectionLines->releaseVertexBuffer();
        intersectionLines->releaseStartPointsVertexBuffer();
        //! TODO might raise an exception!
        isosurfaceSource->releaseData(intersectionLines);
        //lineGeometryFilter->releaseData(intersectionLines);
    }

    supressActorUpdates = true;
    variableSettings->groupProp->setEnabled(false);
    ensembleSelectionSettings->groupProp->setEnabled(false);
    supressActorUpdates = false;

    // Obtain the two variables that should be intersected.
    MNWPIsolevelActorVariable *var1st = dynamic_cast<MNWPIsolevelActorVariable *>(
            variables.at(variableSettings->varsIndex[0]));
    MNWPIsolevelActorVariable *var2nd = dynamic_cast<MNWPIsolevelActorVariable *>(
            variables.at(variableSettings->varsIndex[1]));

    isosurfaceSource->setInputSourceFirstVar(var1st->dataSource);
    isosurfaceSource->setInputSourceSecondVar(var2nd->dataSource);

    // Disable the sync control during computation.
    if (var2nd->synchronizationControl != nullptr)
    {
        var2nd->synchronizationControl->setEnabled(false);
    } else
    {
        if (var1st->synchronizationControl != nullptr)
        {
            var1st->synchronizationControl->setEnabled(false);
        }
    }

    // Set the line request.
    MDataRequestHelper rh;

    rh.insert("INIT_TIME", var1st->getPropertyTime(var1st->initTimeProperty));
    rh.insert("VALID_TIME", var1st->getPropertyTime(var1st->validTimeProperty));
    rh.insert("LEVELTYPE", var1st->levelType);

    // This variable is mandatory as the filter source requires to have it.
    // However, it is not used in the intersection source.
    rh.insert("MEMBER", 0);
    rh.insert("MEMBERS", MDataRequestHelper
    ::uintSetToString(ensembleSelectionSettings->selectedEnsembleMembers));

    // Set the variables and iso-values.
    rh.insert("ISOX_VARIABLES", var1st->variableName + "/"
                                + var2nd->variableName);
    rh.insert("ISOX_VALUES", QString::number(var1st->getIsoValue())
                             + "/" + QString::number(var2nd->getIsoValue()));
    rh.insert("VARIABLE", var1st->variableName);

    rh.insert("ISOX_BOUNDING_BOX",
              QString::number(boundingBoxSettings->llcrnLon) + "/"
              + QString::number(boundingBoxSettings->llcrnLat) + "/"
              + QString::number(boundingBoxSettings->pBot_hPa) + "/"
              + QString::number(boundingBoxSettings->urcrnLon) + "/"
              + QString::number(boundingBoxSettings->urcrnLat) + "/"
              + QString::number(boundingBoxSettings->pTop_hPa));

    lineRequest = rh.request();

    // Request the crossing lines.
    isosurfaceSource->requestData(lineRequest);
}


void MIsosurfaceIntersectionActor::buildFilterChain(MDataRequestHelper &rh)
{
    MTrajectorySelectionSource *inputSource = isosurfaceSource;

    MNWPIsolevelActorVariable *varSource = nullptr;

    if (variableSettings->varsIndex[2] > 0)
    {
        varSource = dynamic_cast<MNWPIsolevelActorVariable *>(
                variables.at(variableSettings->varsIndex[2] - 1));
    }

    MNWPIsolevelActorVariable *varMapped = nullptr;

    if (appearanceSettings->colorVariableIndex > 0)
    {
        varMapped = dynamic_cast<MNWPIsolevelActorVariable *>(
                variables.at(appearanceSettings->colorVariableIndex - 1));
    }

    MNWPIsolevelActorVariable *varThickness = nullptr;

    if (tubeThicknessSettings->mappedVariableIndex > 0)
    {
        varThickness = dynamic_cast<MNWPIsolevelActorVariable *>(
                variables.at(tubeThicknessSettings->mappedVariableIndex - 1));
    }

    // If the user has selected a variable to filter by, set the filter variable
    // and the corresponding filter value.
    if (varSource)
    {
        rh.insert("VARFILTER_MEMBERS", rh.value("MEMBERS"));
        rh.insert("VARFILTER_OP", "GREATER_OR_EQUAL");
        rh.insert("VARFILTER_VALUE",
                  QString::number(lineFilterSettings->valueFilter));
        rh.insert("VARFILTER_VARIABLE", varSource->variableName);

        varTrajectoryFilter->setIsosurfaceSource(isosurfaceSource);
        varTrajectoryFilter->setFilterVariableInputSource(
                varSource->dataSource);
        varTrajectoryFilter->setLineRequest(lineRequest);

        filterRequests.push_back(
                {varTrajectoryFilter, inputSource, rh.request()});
        inputSource = varTrajectoryFilter.get();
    }

    // Set the geometric length filter.
    geomLengthTrajectoryFilter->setLineRequest(lineRequest);
    geomLengthTrajectoryFilter->setIsosurfaceSource(isosurfaceSource);

    rh.insert("GEOLENFILTER_VALUE",
              QString::number(lineFilterSettings->lineLengthFilter));
    rh.insert("GEOLENFILTER_OP", "GREATER_OR_EQUAL");

    filterRequests.push_back(
            {geomLengthTrajectoryFilter, inputSource, rh.request()});

    inputSource = geomLengthTrajectoryFilter.get();

    // Set the value trajectory filter. The filter gathers the value information
    // at each intersection line vertex, especially for coloring and
    // thickness mapping.
    valueTrajectorySource->setIsosurfaceSource(isosurfaceSource);
    valueTrajectorySource->setLineRequest(lineRequest);
    valueTrajectorySource->setInputSelectionSource(inputSource);
    if (varMapped)
    { valueTrajectorySource->setInputSourceValueVar(varMapped->dataSource); }
    else
    { valueTrajectorySource->setInputSourceValueVar(nullptr); }
    if (varThickness)
    {
        valueTrajectorySource->setInputSourceThicknessVar(
                varThickness->dataSource);
    }
    else
    { valueTrajectorySource->setInputSourceThicknessVar(nullptr); }

    rh.insert("TRAJECTORYVALUES_MEMBERS", rh.value("MEMBERS"));
    rh.insert("TRAJECTORYVALUES_VARIABLE",
              (varMapped) ? varMapped->variableName : "");
    rh.insert("TRAJECTORYVALUES_THICKNESSVAR",
              (varThickness) ? varThickness->variableName : "");

    valueRequest = rh.request();
}


void MIsosurfaceIntersectionActor::requestFilters()
{
    if (!intersectionLines)
    { return; }

    if (!filterRequests.empty())
    {
        const Request filter = filterRequests.first();
        filterRequests.pop_front();

        currentTrajectoryFilter = filter.filter;
        filter.filter->setInputSelectionSource(filter.inputSelectionSource);
        filter.filter->requestData(filter.request);
    } else
    {
        onFilterChainEnd();
    }
}


void MIsosurfaceIntersectionActor::buildGPUResources()
{
    valueTrajectorySource->requestData(valueRequest);
}


void MIsosurfaceIntersectionActor::onFilterChainEnd()
{
    buildGPUResources();
}


void MIsosurfaceIntersectionActor::addFilter(
        std::shared_ptr<MScheduledDataSource> trajFilter)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MAbstractScheduler *scheduler = sysMC->getScheduler("MultiThread");
    MAbstractMemoryManager *memoryManager = sysMC->getMemoryManager("NWP");

    trajFilter->setMemoryManager(memoryManager);
    trajFilter->setScheduler(scheduler);

    connect(trajFilter.get(), SIGNAL(dataRequestCompleted(MDataRequest)),
            this, SLOT(asynchronousFiltersAvailable(MDataRequest)));
}


void MIsosurfaceIntersectionActor::placePoleActors(
        MIsosurfaceIntersectionLines *intersectionLines)
{
    poleActor->removeAllPoles();

    MNWPIsolevelActorVariable *varSource = nullptr;

    if (appearanceSettings->colorVariableIndex > 0)
    {
        varSource = dynamic_cast<MNWPIsolevelActorVariable *>(
                variables.at(appearanceSettings->colorVariableIndex - 1));
    }

    for (int i = 0; i < lineSelection->getNumTrajectories(); ++i)
    {
        const int startIndex = lineSelection->getStartIndices()[i];
        const int indexCount = lineSelection->getIndexCount()[i];
        const int endIndex = startIndex + indexCount;

        // Obtain start / middle / and endpoint of each intersection line.
        const QVector3D &startPoint = intersectionLines->getVertices().at(
                startIndex);
        const QVector3D &endPoint = intersectionLines->getVertices().at(
                endIndex - 1);
        const int midIndex = (endIndex + startIndex) / 2;
        const QVector3D &midPoint = intersectionLines->getVertices().at(
                midIndex);

        QVector3D maxPoint = midPoint;

        // If variable input source is given, look for the maximum along the
        // intersection line.
        if (varSource)
        {
            QVector3D point = intersectionLines->getVertices().at(
                    startIndex + 1);
            float maxValue = varSource->grid->interpolateValue(point);
            int maxIndex = startIndex + 1;

            for (int k = startIndex + 2; k < endIndex - 1; ++k)
            {
                point = intersectionLines->getVertices().at(k);
                float value = varSource->grid->interpolateValue(point);

                if (value > maxValue)
                {
                    maxValue = value;
                    maxIndex = k;
                }
            }

            maxPoint = intersectionLines->getVertices().at(maxIndex);
        }

        switch (appearanceSettings->dropMode)
        {
            case 0:
                poleActor->addPole(startPoint);
                break;

            case 1:
                poleActor->addPole(endPoint);
                break;

            case 2:
                poleActor->addPole(startPoint);
                poleActor->addPole(endPoint);
                break;

            case 3:
                poleActor->addPole(midPoint);
                break;

            case 4:
                poleActor->addPole(maxPoint);
                break;

            case 5:
                poleActor->addPole(startPoint);
                poleActor->addPole(endPoint);
                poleActor->addPole(midPoint);
                break;

            case 6:
                poleActor->addPole(startPoint);
                poleActor->addPole(endPoint);
                poleActor->addPole(maxPoint);
                break;
        }


    }
}


void MIsosurfaceIntersectionActor::initializeActorResources()
{
    // Parent initialisation.
    MNWPMultiVarActor::initializeActorResources();

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    bool loadShaders = false;
    loadShaders |= glRM->generateEffectProgram("isosurfaceIntersectionlines",
                                               intersectionLinesShader);
    loadShaders |= glRM->generateEffectProgram("boundingbox_volume",
                                               boundingBoxShader);
    loadShaders |= glRM->generateEffectProgram("trajectory_tubeshadow",
                                               tubeShadowShader);
    loadShaders |= glRM->generateEffectProgram("trajectory_line_tubes",
                                               lineTubeShader);

    if (loadShaders) reloadShaderEffects();

    // create vertex shader of bounding box
    generateVolumeBoxGeometry();

    varTrajectoryFilter = std::make_shared<MVariableTrajectoryFilter>();
    addFilter(varTrajectoryFilter);

    geomLengthTrajectoryFilter = std::make_shared<MGeometricLengthTrajectoryFilter>();
    addFilter(geomLengthTrajectoryFilter);

    valueTrajectorySource = std::make_shared<MTrajectoryValueSource>();
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MAbstractScheduler *scheduler = sysMC->getScheduler("MultiThread");
    MAbstractMemoryManager *memoryManager = sysMC->getMemoryManager("NWP");

    valueTrajectorySource->setMemoryManager(memoryManager);
    valueTrajectorySource->setScheduler(scheduler);

    connect(valueTrajectorySource.get(),
            SIGNAL(dataRequestCompleted(MDataRequest)),
            this, SLOT(asynchronousValuesAvailable(MDataRequest)));

    if (appearanceSettings->textureUnitTransferFunction >= 0)
    {
        releaseTextureUnit(appearanceSettings->textureUnitTransferFunction);
    }

    appearanceSettings->textureUnitTransferFunction = assignImageUnit();

    // Explicitly initialize the pole actor.
    poleActor->initialize();
}


void MIsosurfaceIntersectionActor::generateVolumeBoxGeometry()
{
    // Define geometry for bounding box
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

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
    for (int i = 0; i < numVertices; i++)
    {
        vertexData[i * 3 + 0] =
                boundingBoxSettings->llcrnLon + vertexData[i * 3 + 0]
                                                *
                                                (boundingBoxSettings->urcrnLon -
                                                 boundingBoxSettings->llcrnLon);
        vertexData[i * 3 + 1] =
                boundingBoxSettings->urcrnLat - vertexData[i * 3 + 1]
                                                *
                                                (boundingBoxSettings->urcrnLat -
                                                 boundingBoxSettings->llcrnLat);
        vertexData[i * 3 + 2] = (vertexData[i * 3 + 2] == 0)
                                ? boundingBoxSettings->pBot_hPa
                                : boundingBoxSettings->pTop_hPa;
    }

    if (vboBoundingBox)
    {
        GL::MFloat3VertexBuffer *buf = dynamic_cast<GL::MFloat3VertexBuffer *>(
                vboBoundingBox);
        buf->update(vertexData, numVertices);
    } else
    {
        const QString vboID = QString("vbo_bbox_actor#%1").arg(myID);

        GL::MFloat3VertexBuffer *buf =
                new GL::MFloat3VertexBuffer(vboID, numVertices);

        if (glRM->tryStoreGPUItem(buf))
        {
            buf->upload(vertexData, numVertices);
            vboBoundingBox = static_cast<GL::MVertexBuffer *>(
                    glRM->getGPUItem(vboID));
        } else
        {
            LOG4CPLUS_WARN(mlog, "WARNING: cannot store buffer for volume"
                    " bbox in GPU memory.");
            delete buf;
            return;
        }

    }

    glGenBuffers(1, &iboBoundingBox);
    CHECK_GL_ERROR;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboBoundingBox);
    CHECK_GL_ERROR;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 numIndices * sizeof(GLushort),
                 indexData,
                 GL_STATIC_DRAW);
    CHECK_GL_ERROR;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    CHECK_GL_ERROR;
}


void
MIsosurfaceIntersectionActor::renderToDepthMap(MSceneViewGLWidget *sceneView)
{
    const QVector3D llbottomCrnr(boundingBoxSettings->llcrnLon,
                                 boundingBoxSettings->llcrnLat,
                                 sceneView->worldZfromPressure(
                                         boundingBoxSettings->pBot_hPa));

    const QVector3D urtopCrnr(boundingBoxSettings->urcrnLon,
                              boundingBoxSettings->urcrnLat,
                              sceneView->worldZfromPressure(
                                      boundingBoxSettings->pTop_hPa));

    // Set up light space transformation.
    QMatrix4x4 lightView;
    QMatrix4x4 lightProjection;

    QVector3D center = (llbottomCrnr + urtopCrnr) / 2;
    center.setZ(urtopCrnr.z());

    lightView.lookAt(center, QVector3D(center.x(), center.y(), 0),
                     QVector3D(0, 1, 0));
    lightProjection.ortho(llbottomCrnr.x() - center.x(),
                          urtopCrnr.x() - center.x(),
                          llbottomCrnr.y() - center.y(),
                          urtopCrnr.y() - center.y(), 0, 100.0);

    lightMVP = lightProjection * lightView;

    lineTubeShader->bindProgram("TrajectoryShadowMap");
    CHECK_GL_ERROR;

    lineTubeShader->setUniformValue("mvpMatrix", lightMVP);
    CHECK_GL_ERROR;

    lineTubeShader->setUniformValue("tubeRadius",
                                    appearanceSettings->tubeRadius);
    lineTubeShader->setUniformValue("geometryColor",
                                    appearanceSettings->tubeColor);
    lineTubeShader->setUniformValue("colorMode", appearanceSettings->colorMode);

    if (appearanceSettings->colorVariableIndex > 0 &&
        appearanceSettings->transferFunction != 0)
    {
        appearanceSettings->transferFunction->getTexture()->bindToTextureUnit(
                static_cast<GLuint>(appearanceSettings->textureUnitTransferFunction));
        lineTubeShader->setUniformValue("transferFunction",
                                        appearanceSettings->textureUnitTransferFunction);
        lineTubeShader->setUniformValue("tfMinimum",
                                        appearanceSettings->transferFunction->getMinimumValue());
        lineTubeShader->setUniformValue("tfMaximum",
                                        appearanceSettings->transferFunction->getMaximumValue());
        lineTubeShader->setUniformValue("normalized", false);
    }

    lineTubeShader->setUniformValue("thicknessMapping",
                                    appearanceSettings->thicknessMode == 1);
    lineTubeShader->setUniformValue("thicknessRange",
                                    tubeThicknessSettings->thicknessRange);
    lineTubeShader->setUniformValue("thicknessValueRange",
                                    tubeThicknessSettings->valueRange);

    lineTubeShader->setUniformValue("pToWorldZParams",
                                    sceneView->pressureToWorldZParameters());

    lineTubeShader->setUniformValue(
            "lightDirection", sceneView->getLightDirection());
    lineTubeShader->setUniformValue(
            "cameraPosition", sceneView->getCamera()->getOrigin());
    lineTubeShader->setUniformValue("shadowColor", QColor(100, 100, 100, 155));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    CHECK_GL_ERROR;
    glBindBuffer(GL_ARRAY_BUFFER, linesVertexBuffer->getVertexBufferObject());
    CHECK_GL_ERROR;

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (const GLvoid *) 0);

    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (const GLvoid *) (3 * sizeof(float)));

    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (const GLvoid *) (4 * sizeof(float)));

    glEnableVertexAttribArray(0);
    CHECK_GL_ERROR;
    glEnableVertexAttribArray(1);
    CHECK_GL_ERROR;
    glEnableVertexAttribArray(2);
    CHECK_GL_ERROR;

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    CHECK_GL_ERROR;
    glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, linesData.size());
    CHECK_GL_ERROR;

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void MIsosurfaceIntersectionActor::renderShadows(MSceneViewGLWidget *sceneView)
{
    if (updateShadowImage || !shadowImageVBO)
    {
        MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
        //glRM->makeCurrent();

        float quadData[] =
                {
                        boundingBoxSettings->llcrnLon,
                        boundingBoxSettings->llcrnLat, 0.1, 0, 0,
                        boundingBoxSettings->llcrnLon,
                        boundingBoxSettings->urcrnLat, 0.1, 0, 1,
                        boundingBoxSettings->urcrnLon,
                        boundingBoxSettings->llcrnLat, 0.1, 1, 0,
                        boundingBoxSettings->urcrnLon,
                        boundingBoxSettings->urcrnLat, 0.1, 1, 1
                };

        if (!shadowImageVBO)
        {
            const QString vboID = QString(
                    "trajectory_shadowmap_image_actor_#%1").arg(myID);

            GL::MFloatVertexBuffer *newVB =
                    new GL::MFloatVertexBuffer(vboID, 20);

            if (glRM->tryStoreGPUItem(newVB))
            {
                newVB->upload(quadData, 20, sceneView);
                shadowImageVBO = static_cast<GL::MVertexBuffer *>(glRM->getGPUItem(
                        vboID));
            } else
            {
                delete newVB;
            }
        } else
        {
            GL::MFloatVertexBuffer *buf = dynamic_cast<GL::MFloatVertexBuffer *>(shadowImageVBO);
            buf->update(quadData, 20, 0, 0, sceneView);
        }
    }

    lineTubeShader->bindProgram("ShadowGroundMap");
    CHECK_GL_ERROR;

    lineTubeShader->setUniformValue("mvpMatrix",
                                    *(sceneView->getModelViewProjectionMatrix()));

    shadowMap->bindToTextureUnit(static_cast<GLuint>(shadowMapTexUnit));
    lineTubeShader->setUniformValue("shadowMap", shadowMapTexUnit);
    CHECK_GL_ERROR;

    lineTubeShader->setUniformValue("shadowColor", QColor(100, 100, 100, 155));

    glBindBuffer(GL_ARRAY_BUFFER, shadowImageVBO->getVertexBufferObject());
    shadowImageVBO->attachToVertexAttribute(0, 3, false, 5 * sizeof(float),
                                            (void *) 0);
    shadowImageVBO->attachToVertexAttribute(1, 2, false, 5 * sizeof(float),
                                            (void *) (3 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    CHECK_GL_ERROR;

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void
MIsosurfaceIntersectionActor::renderBoundingBox(MSceneViewGLWidget *sceneView)
{
    boundingBoxShader->bindProgram("Pressure");
    boundingBoxShader->setUniformValue(
            "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
    boundingBoxShader->setUniformValue(
            "pToWorldZParams", sceneView->pressureToWorldZParameters());
    boundingBoxShader->setUniformValue(
            "colour", QColor(Qt::black));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboBoundingBox);
    CHECK_GL_ERROR;
    vboBoundingBox->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    CHECK_GL_ERROR;
    glLineWidth(1);
    CHECK_GL_ERROR;

    glDrawElements(GL_LINE_STRIP, 16, GL_UNSIGNED_SHORT, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void MIsosurfaceIntersectionActor::renderToCurrentContext(
        MSceneViewGLWidget *sceneView)
{
    if (boundingBoxSettings->enabled)
    {
        renderBoundingBox(sceneView);
    }

    if (intersectionLines != nullptr &&
        linesVertexBuffer != nullptr &&
        variableSettings->varsIndex[0] != variableSettings->varsIndex[1])
    {
        if (!shadowMap)
        {
            MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

            glGenFramebuffers(1, &shadowMapFBO);
            CHECK_GL_ERROR;

            shadowMapTexUnit = assignTextureUnit();
            const QString shadowMapID = QString("shadow_map_#%1").arg(myID);

            shadowMap = new GL::MTexture(shadowMapID, GL_TEXTURE_2D,
                                         GL_DEPTH_COMPONENT32,
                                         shadowMapRes, shadowMapRes);

            if (glRM->tryStoreGPUItem(shadowMap))
            {
                shadowMap = dynamic_cast<GL::MTexture *>(glRM->getGPUItem(
                        shadowMapID));
            } else
            {
                delete shadowMap;
                shadowMap = nullptr;
            }

            if (shadowMap)
            {
                shadowMap->bindToLastTextureUnit();
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_NEAREST);
                CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_NEAREST);
                CHECK_GL_ERROR;
                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32,
                             shadowMapRes, shadowMapRes, 0,
                             GL_DEPTH_COMPONENT, GL_FLOAT, 0);
                CHECK_GL_ERROR;
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
        CHECK_GL_ERROR;
        // Attach the shadow map texture to the depth buffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D,
                               shadowMap->getTextureObject(), 0);
        CHECK_GL_ERROR;
        // We're not going to render any color data --> disable draw buffer and
        // read buffer.
        glDrawBuffer(GL_NONE);
        CHECK_GL_ERROR;
        glReadBuffer(GL_NONE);
        CHECK_GL_ERROR;
        // Unbind depth buffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        CHECK_GL_ERROR;

        // Set the viewport size to the size of our shadow map texture.
        glViewport(0, 0, shadowMapRes, shadowMapRes);
        CHECK_GL_ERROR;
        glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
        CHECK_GL_ERROR;
        // Clear all set depth values in the framebuffer.
        glClear(GL_DEPTH_BUFFER_BIT);
        CHECK_GL_ERROR;

        renderToDepthMap(sceneView);

        // Restore the old viewport.
        glViewport(0, 0, sceneView->getViewPortWidth(),
                   sceneView->getViewPortHeight());
        CHECK_GL_ERROR;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        CHECK_GL_ERROR;

        if (appearanceSettings->enableShadows)
        { renderShadows(sceneView); }

        lineTubeShader->bindProgram("Trajectory");
        CHECK_GL_ERROR;

        lineTubeShader->setUniformValue("mvpMatrix",
                                        *(sceneView->getModelViewProjectionMatrix()));

        lineTubeShader->setUniformValue("lightMVPMatrix", lightMVP);
        CHECK_GL_ERROR;

        lineTubeShader->setUniformValue("tubeRadius",
                                        appearanceSettings->tubeRadius);
        lineTubeShader->setUniformValue("geometryColor",
                                        appearanceSettings->tubeColor);
        lineTubeShader->setUniformValue("colorMode",
                                        appearanceSettings->colorMode);

        if (appearanceSettings->colorVariableIndex > 0 &&
            appearanceSettings->transferFunction != 0)
        {
            appearanceSettings->transferFunction->getTexture()->bindToTextureUnit(
                    static_cast<GLuint>(appearanceSettings->textureUnitTransferFunction));
            lineTubeShader->setUniformValue("transferFunction",
                                            appearanceSettings->textureUnitTransferFunction);
            lineTubeShader->setUniformValue("tfMinimum",
                                            appearanceSettings->transferFunction->getMinimumValue());
            lineTubeShader->setUniformValue("tfMaximum",
                                            appearanceSettings->transferFunction->getMaximumValue());
            lineTubeShader->setUniformValue("normalized", false);
        }

        lineTubeShader->setUniformValue("thicknessMapping",
                                        appearanceSettings->thicknessMode == 1);
        lineTubeShader->setUniformValue("thicknessRange",
                                        tubeThicknessSettings->thicknessRange);
        lineTubeShader->setUniformValue("thicknessValueRange",
                                        tubeThicknessSettings->valueRange);

        lineTubeShader->setUniformValue("pToWorldZParams",
                                        sceneView->pressureToWorldZParameters());

        lineTubeShader->setUniformValue(
                "lightDirection", sceneView->getLightDirection());
        lineTubeShader->setUniformValue(
                "cameraPosition", sceneView->getCamera()->getOrigin());
        lineTubeShader->setUniformValue("shadowColor",
                                        QColor(100, 100, 100, 155));

        shadowMap->bindToTextureUnit(static_cast<GLuint>(shadowMapTexUnit));
        lineTubeShader->setUniformValue("shadowMap", shadowMapTexUnit);
        CHECK_GL_ERROR;
        lineTubeShader->setUniformValue("enableSelfShadowing",
                                        appearanceSettings->enableSelfShadowing);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        CHECK_GL_ERROR;
        glBindBuffer(GL_ARRAY_BUFFER,
                     linesVertexBuffer->getVertexBufferObject());
        CHECK_GL_ERROR;

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (const GLvoid *) 0);

        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (const GLvoid *) (3 * sizeof(float)));

        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (const GLvoid *) (4 * sizeof(float)));

        glEnableVertexAttribArray(0);
        CHECK_GL_ERROR;
        glEnableVertexAttribArray(1);
        CHECK_GL_ERROR;
        glEnableVertexAttribArray(2);
        CHECK_GL_ERROR;

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        CHECK_GL_ERROR;
        glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, linesData.size());
        CHECK_GL_ERROR;

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    } else
    {
        if (enableAutoComputation)
        {
            requestIsoSurfaceIntersectionLines();
        }
    }

    if (appearanceSettings->polesEnabled)
    {
        // Render all placed poles
        poleActor->render(sceneView);

        // And render the labels of all poles
        MTextManager *tm = MGLResourcesManager::getInstance()->getTextManager();
        tm->renderLabelList(sceneView, poleActor->getLabelsToRender());
    }
}


void
MIsosurfaceIntersectionActor::refreshEnumsProperties(MNWPActorVariable *var)
{
    QStringList names;

    foreach (MNWPActorVariable *act, variables)
    {
        if (var == nullptr || var != act)
        {
            names.append(act->variableName);
        }
    }

    variableSettings->varsIndex[0] = 0;
    variableSettings->varsIndex[1] = 0;
    variableSettings->varsIndex[2] = 0;
    appearanceSettings->colorVariableIndex = 0;
    tubeThicknessSettings->mappedVariableIndex = 0;

    enableActorUpdates(false);
    properties->mEnum()->setEnumNames(variableSettings->varsProperty[0], names);
    properties->mEnum()->setEnumNames(variableSettings->varsProperty[1], names);

    QStringList varNamesWithNone = names;
    varNamesWithNone.prepend("None");

    properties->mEnum()->setEnumNames(variableSettings->varsProperty[2],
                                      varNamesWithNone);
    properties->mEnum()->setEnumNames(appearanceSettings->colorVariableProperty,
                                      varNamesWithNone);
    properties->mEnum()->setEnumNames(
            tubeThicknessSettings->mappedVariableProperty,
            varNamesWithNone);

    enableActorUpdates(true);
}


void MIsosurfaceIntersectionActor::setTransferFunctionFromProperty()
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    QString tfName = properties->getEnumItem(
            appearanceSettings->transferFunctionProperty);

    if (tfName == "None")
    {
        appearanceSettings->transferFunction = nullptr;
        return;
    }

    // Find the selected transfer function in the list of actors from the
    // resources manager. Not very efficient, but works well enough for the
    // small number of actors at the moment..
    foreach (MActor *actor, glRM->getActors())
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D *>(actor))
        {
            if (tf->transferFunctionName() == tfName)
            {
                appearanceSettings->transferFunction = tf;
                return;
            }

        }
    }
}


bool MIsosurfaceIntersectionActor::setTransferFunction(const QString &tfName)
{
    QStringList tfNames = properties->mEnum()->enumNames(
            appearanceSettings->transferFunctionProperty);
    int tfIndex = tfNames.indexOf(tfName);

    if (tfIndex >= 0)
    {
        properties->mEnum()->setValue(
                appearanceSettings->transferFunctionProperty, tfIndex);
        return true;
    }

    // Set transfer function property to "None".
    properties->mEnum()->setValue(appearanceSettings->transferFunctionProperty,
                                  0);

    return false; // the given tf name could not be found
}

/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/
