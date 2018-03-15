/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Michael Kern
**  Copyright 2017 Marc Rautenhaus
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
#include "jetcoredetectionactor.h"

// standard library imports

// related third party imports

// local application imports


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MJetcoreDetectionActor::MJetcoreDetectionActor()
    : MIsosurfaceIntersectionActor(),
      partialDerivFilters({nullptr, nullptr}),
      hessianFilter(nullptr),
      arrowsVertexBuffer(nullptr),
      arrowHeads(nullptr)
{
    beginInitialiseQtProperties();

    setActorType("Jetcore Detection Actor (experimental)");
    setName(getActorType());

    variableSettings->groupProp->setPropertyName("detection variables");

    variableSettings->varsProperty[0]->setPropertyName("u-component of wind");
    variableSettings->varsProperty[1]->setPropertyName("v-component of wind");

    variableSettings->groupProp->removeSubProperty(variableSettings->varsIsovalueProperty[0]);
    variableSettings->groupProp->removeSubProperty(variableSettings->varsIsovalueProperty[1]);

    variableSettingsCores =
            std::make_shared<VariableSettingsJetcores>(
                this, variableSettings->groupProp);

    lineFilterSettingsCores =
            std::make_shared<FilterSettingsJetcores>(
                this, lineFilterSettings->groupProp);

    appearanceSettingsCores =
            std::make_shared<AppearanceSettingsJetcores>(
                this, appearanceSettings->groupProp);

    endInitialiseQtProperties();
}


MJetcoreDetectionActor::~MJetcoreDetectionActor()
{
}


/******************************************************************************
***                        SETTINGS CONSTRUCTORS                            ***
*******************************************************************************/

MJetcoreDetectionActor::VariableSettingsJetcores::VariableSettingsJetcores(
        MJetcoreDetectionActor *hostActor, QtProperty *groupProp)
    : geoPotVarIndex(-1),
      geoPotOnly(false)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    geoPotVarProperty = a->addProperty(ENUM_PROPERTY, "geopotential height",
                                       groupProp);

    geoPotOnlyProperty = a->addProperty(BOOL_PROPERTY, "convert geopotential to geopot. height",
                                        groupProp);

    properties->mBool()->setValue(geoPotOnlyProperty, geoPotOnly);
}


MJetcoreDetectionActor::FilterSettingsJetcores::FilterSettingsJetcores(
        MJetcoreDetectionActor *hostActor, QtProperty *groupProp)
    : lambdaThreshold(0.f),
      angleThreshold(180.f),
      pressureDiffThreshold(10000.0f)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    lambdaThresholdProperty = a->addProperty(SCIENTIFICDOUBLE_PROPERTY,
                                             "max. lambda (* 10e-9)",
                                             groupProp);
    properties->setSciDouble(lambdaThresholdProperty, lambdaThreshold,
                             -std::numeric_limits<double>::min(),
                             std::numeric_limits<double>::max(), 6, 0.1);

    angleThresholdProperty = a->addProperty(DOUBLE_PROPERTY, "max. angle",
                                            groupProp);
    properties->setDouble(angleThresholdProperty, angleThreshold, 0, 180, 2,
                          1);

    pressureDiffThresholdProperty = a->addProperty(
                DECORATEDDOUBLE_PROPERTY, "max. pressure difference",
                groupProp);
    properties->setDDouble(pressureDiffThresholdProperty,
                           pressureDiffThreshold, 0., 10000., 2, 1., " hPa");
}


MJetcoreDetectionActor::AppearanceSettingsJetcores::AppearanceSettingsJetcores(
        MJetcoreDetectionActor *hostActor, QtProperty *groupProp)
    : arrowsEnabled(false)
{
    MActor *a = hostActor;
    MQtProperties *properties = a->getQtProperties();

    arrowsEnabledProperty = a->addProperty(BOOL_PROPERTY, "render arrow heads",
                                           groupProp);
    properties->mBool()->setValue(arrowsEnabledProperty, arrowsEnabled);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MJetcoreDetectionActor::saveConfiguration(QSettings *settings)
{
    MIsosurfaceIntersectionActor::saveConfiguration(settings);

    settings->beginGroup(MJetcoreDetectionActor::getSettingsID());

    settings->setValue("geoPotVarIndex", variableSettingsCores->geoPotVarIndex);
    settings->setValue("geoPotVarOnly", variableSettingsCores->geoPotOnly);
    settings->setValue("lambdaThreshold",
                       lineFilterSettingsCores->lambdaThreshold);
    settings->setValue("angleThreshold",
                       lineFilterSettingsCores->angleThreshold);
    settings->setValue("pressureDiffThreshold",
                       lineFilterSettingsCores->pressureDiffThreshold);
    settings->setValue("arrowsEnabled", appearanceSettingsCores->arrowsEnabled);

    settings->endGroup();
}


void MJetcoreDetectionActor::loadConfiguration(QSettings *settings)
{
    MIsosurfaceIntersectionActor::loadConfiguration(settings);

    enableActorUpdates(false);
    settings->beginGroup(getSettingsID());

    variableSettingsCores->geoPotVarIndex = settings->value(
                "geoPotVarIndex", -1).toInt();
    properties->mEnum()->setValue(variableSettingsCores->geoPotVarProperty,
                                  variableSettingsCores->geoPotVarIndex);
    variableSettingsCores->geoPotOnly = settings->value(
                "geoPotVarOnly", false).toBool();
    properties->mBool()->setValue(variableSettingsCores->geoPotOnlyProperty,
                                  variableSettingsCores->geoPotOnly);

    lineFilterSettingsCores->lambdaThreshold = settings->value(
                "lambdaThreshold", 0.f).toFloat();
    properties->mSciDouble()->setValue(
                lineFilterSettingsCores->lambdaThresholdProperty,
                lineFilterSettingsCores->lambdaThreshold);
    lineFilterSettingsCores->angleThreshold = settings->value(
                "angleThreshold", 50.f).toFloat();
    properties->mDouble()->setValue(
                lineFilterSettingsCores->angleThresholdProperty,
                lineFilterSettingsCores->angleThreshold);
    lineFilterSettingsCores->pressureDiffThreshold = settings->value(
                "pressureDiffThreshold", 10.f).toFloat();
    properties->mDecoratedDouble()->setValue(
                lineFilterSettingsCores->pressureDiffThresholdProperty,
                lineFilterSettingsCores->pressureDiffThreshold);

    appearanceSettingsCores->arrowsEnabled = settings->value(
                "arrowsEnabled", false).toBool();
    properties->mBool()->setValue(
                appearanceSettingsCores->arrowsEnabledProperty,
                appearanceSettingsCores->arrowsEnabled);

    settings->endGroup();
    enableActorUpdates(true);
}


/******************************************************************************
***                               PUBLIC SLOTS                              ***
*******************************************************************************/

void MJetcoreDetectionActor::onQtPropertyChanged(QtProperty *property)
{
    MIsosurfaceIntersectionActor::onQtPropertyChanged(property);

    if (suppressActorUpdates()) { return; }

    if (property == variableSettingsCores->geoPotVarProperty
            || property == variableSettingsCores->geoPotOnlyProperty
            || property == lineFilterSettingsCores->lambdaThresholdProperty
            || property == lineFilterSettingsCores->angleThresholdProperty
            || property == lineFilterSettingsCores->pressureDiffThresholdProperty)
    {
        variableSettingsCores->geoPotVarIndex = properties->mEnum()
                ->value(variableSettingsCores->geoPotVarProperty);

        variableSettingsCores->geoPotOnly = properties->mBool()
                ->value(variableSettingsCores->geoPotOnlyProperty);

        lineFilterSettingsCores->lambdaThreshold = properties->mSciDouble()
                ->value(lineFilterSettingsCores->lambdaThresholdProperty);

        lineFilterSettingsCores->angleThreshold = properties->mDouble()
                ->value(lineFilterSettingsCores->angleThresholdProperty);

        lineFilterSettingsCores->pressureDiffThreshold =
                properties->mDecoratedDouble()->value(
                    lineFilterSettingsCores->pressureDiffThresholdProperty);

        if (enableAutoComputation)
        {
            requestIsoSurfaceIntersectionLines();
        }

        emitActorChangedSignal();
    }
    else if (property == appearanceSettingsCores->arrowsEnabledProperty)
    {
        appearanceSettingsCores->arrowsEnabled = properties->mBool()
                ->value(appearanceSettingsCores->arrowsEnabledProperty);

        emitActorChangedSignal();
    }
}


/******************************************************************************
***                            PROTECTED METHODS                            ***
*******************************************************************************/

void MJetcoreDetectionActor::initializeActorResources()
{
    MIsosurfaceIntersectionActor::initializeActorResources();

    hessianFilter = std::make_shared<MHessianTrajectoryFilter>();
    addFilter(hessianFilter);

    angleFilter = std::make_shared<MAngleTrajectoryFilter>();
    addFilter(angleFilter);

    pressureDiffFilter =
            std::make_shared<MEndPressureDifferenceTrajectoryFilter>();
    addFilter(pressureDiffFilter);

    arrowHeadsSource = std::make_shared<MTrajectoryArrowHeadsSource>();
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MAbstractScheduler *scheduler = sysMC->getScheduler("MultiThread");
    MAbstractMemoryManager *memoryManager = sysMC->getMemoryManager("NWP");

    arrowHeadsSource->setMemoryManager(memoryManager);
    arrowHeadsSource->setScheduler(scheduler);

    connect(arrowHeadsSource.get(), SIGNAL(dataRequestCompleted(MDataRequest)),
            this, SLOT(asynchronousArrowsAvailable(MDataRequest)));
}


void MJetcoreDetectionActor::requestIsoSurfaceIntersectionLines()
{
    // Only send request if actor is connected to a bounding box and at least
    // one scene view.
    if (getViews().empty() || bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }

    if (isCalculating
            || variableSettings->varsIndex[0] == variableSettings->varsIndex[1])
    {
        return;
    }

    isCalculating = true;

    // If the user has selected an ensemble member and at least one variable,
    // then obtain all selected ensemble members.
    if (ensembleSelectionSettings->selectedEnsembleMembers.size() == 0 &&
            variables.size() > 0 &&
            variables.at(0)->dataSource != nullptr)
    {
        ensembleSelectionSettings->selectedEnsembleMembers = variables.at(0)->
                dataSource->availableEnsembleMembers(
                    variables.at(0)->levelType,
                    variables.at(0)->variableName);
        QString s = MDataRequestHelper::uintSetToString(
                    ensembleSelectionSettings->selectedEnsembleMembers);
        properties->mString()->setValue(
                    ensembleSelectionSettings->ensembleMultiMemberProperty, s);
    }

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MAbstractScheduler *scheduler = sysMC->getScheduler("MultiThread");
    MAbstractMemoryManager *memoryManager = sysMC->getMemoryManager("NWP");

    // Create a new instance of an iso-surface intersection source if not created.
    if (isosurfaceSource == nullptr)
    {
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
        //isosurfaceSource->releaseData(intersectionLines);
        //lineGeometryFilter->releaseData(intersectionLines);
    }

    // Two filters, one for each variable.
    for (int i = 0; i < 2; ++i)
    {
        if (partialDerivFilters[i] != nullptr)
        {
            continue;
        }

        partialDerivFilters[i] = new MMultiVarPartialDerivativeFilter();

        partialDerivFilters[i]->setScheduler(scheduler);
        partialDerivFilters[i]->setMemoryManager(memoryManager);
        sysMC->registerDataSource("partialderivfilter" + QString::number(i),
                                  partialDerivFilters[i]);
    }

    enableActorUpdates(false);
    variableSettings->groupProp->setEnabled(false);
    ensembleSelectionSettings->groupProp->setEnabled(false);
    enableActorUpdates(true);

    // Obtain the two variables that should be intersected.
    auto var1st = dynamic_cast<MNWPActorVariable *>(
                variables.at(variableSettings->varsIndex[0]));
    auto var2nd = dynamic_cast<MNWPActorVariable *>(
                variables.at(variableSettings->varsIndex[1]));

    // Obtain the variable for geopotential height.
    auto varGeoPot = dynamic_cast<MNWPActorVariable *>(
                variables.at(variableSettingsCores->geoPotVarIndex));

    partialDerivFilters[0]->setInputSource(var1st->dataSource);
    partialDerivFilters[1]->setInputSource(var2nd->dataSource);

    isosurfaceSource->setInputSourceFirstVar(partialDerivFilters[0]);
    isosurfaceSource->setInputSourceSecondVar(partialDerivFilters[1]);

    // Disable the sync control during computation.
    if (var2nd->synchronizationControl != nullptr)
    {
        var2nd->synchronizationControl->setEnabled(false);
    }
    else
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
    rh.insert("MEMBER", 0);

    QString memberList = "";
    if (ensembleSelectionSettings->spaghettiPlotEnabled)
    {
        memberList = MDataRequestHelper
        ::uintSetToString(ensembleSelectionSettings->selectedEnsembleMembers);
    }
    else
    {
        memberList = QString::number(var1st->getEnsembleMember());
    }

    rh.insert("MEMBERS", memberList);

    rh.insert("ISOX_VARIABLES", var1st->variableName + "/"
              + var2nd->variableName);
    rh.insert("ISOX_VALUES", QString::number(0)
              + "/" + QString::number(0));
    rh.insert("VARIABLE", var1st->variableName);

    rh.insert("MULTI_DERIVATIVE_SETTINGS", "ddn/ddz");
    rh.insert("MULTI_GEOPOTENTIAL", varGeoPot->variableName);
    rh.insert("MULTI_GEOPOTENTIAL_TYPE",
              static_cast<int>(variableSettingsCores->geoPotOnly));

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


void MJetcoreDetectionActor::buildFilterChain(MDataRequestHelper &rh)
{
    MTrajectorySelectionSource *inputSource = isosurfaceSource;

    MNWPActorVariable *varSource = nullptr;

    if (lineFilterSettings->filterVarIndex > 0)
    {
        varSource =
                dynamic_cast<MNWPActorVariable *>(
                    variables.at(lineFilterSettings->filterVarIndex - 1));
    }

    MNWPActorVariable *varThickness = nullptr;

    if (tubeThicknessSettings->mappedVariableIndex > 0)
    {
        varThickness = dynamic_cast<MNWPActorVariable *>(
                    variables.at(tubeThicknessSettings->mappedVariableIndex - 1)
                    );
    }

    MNWPActorVariable *varMapped = nullptr;

    if (appearanceSettings->colorVariableIndex > 0)
    {
        varMapped = dynamic_cast<MNWPActorVariable *>(
                    variables.at(appearanceSettings->colorVariableIndex - 1));
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

    auto var1st = dynamic_cast<MNWPActorVariable *>(
                variables.at(variableSettings->varsIndex[0]));
    auto var2nd = dynamic_cast<MNWPActorVariable *>(
                variables.at(variableSettings->varsIndex[1]));

    auto varGeoPot = dynamic_cast<MNWPActorVariable *>(
                variables.at(variableSettingsCores->geoPotVarIndex));

    // Set the Hessian eigenvalue filter.
    rh.insert("HESSIANFILTER_MEMBERS", rh.value("MEMBERS"));
    rh.insert("HESSIANFILTER_VALUE",
              QString::number(lineFilterSettingsCores->lambdaThreshold * 10E-9));
    rh.insert("HESSIANFILTER_GEOPOTENTIAL", varGeoPot->variableName);
    rh.insert("HESSIANFILTER_GEOPOTENTIAL_TYPE",
              static_cast<int>(variableSettingsCores->geoPotOnly));
    rh.insert("HESSIANFILTER_VARIABLES", var1st->variableName + "/"
              + var2nd->variableName);
    rh.insert("HESSIANFILTER_DERIVOPS", "d2dn2/d2dz2/d2dndz");

    hessianFilter->setIsosurfaceSource(isosurfaceSource);
    hessianFilter->setMultiVarParialDerivSource(partialDerivFilters[0]);
    hessianFilter->setLineRequest(lineRequest);

    filterRequests.push_back({hessianFilter, inputSource, rh.request()});

    inputSource = hessianFilter.get();

    // Set the line segment angle filter.
    rh.insert("ANGLEFILTER_MEMBERS", rh.value("MEMBERS"));
    rh.insert("ANGLEFILTER_VALUE",
              QString::number(lineFilterSettingsCores->angleThreshold));

    angleFilter->setIsosurfaceSource(isosurfaceSource);
    angleFilter->setLineRequest(lineRequest);

    filterRequests.push_back({angleFilter, inputSource, rh.request()});

    inputSource = angleFilter.get();

    // Set the end pressure difference filter.
    rh.insert("ENDPRESSUREDIFFFILTER_MEMBERS", rh.value("MEMBERS"));
    rh.insert("ENDPRESSUREDIFFFILTER_ANGLE",
              QString::number(lineFilterSettingsCores->angleThreshold));
    rh.insert("ENDPRESSUREDIFFFILTER_VALUE",
              QString::number(lineFilterSettingsCores->pressureDiffThreshold));

    pressureDiffFilter->setIsosurfaceSource(isosurfaceSource);
    pressureDiffFilter->setLineRequest(lineRequest);

    filterRequests.push_back({pressureDiffFilter, inputSource, rh.request()});

    inputSource = pressureDiffFilter.get();

    // Set the geometric length filter.
    geomLengthTrajectoryFilter->setLineRequest(lineRequest);
    geomLengthTrajectoryFilter->setIsosurfaceSource(isosurfaceSource);

    rh.insert("GEOLENFILTER_VALUE",
              QString::number(lineFilterSettings->lineLengthFilter));
    rh.insert("GEOLENFILTER_OP", "GREATER_OR_EQUAL");

    filterRequests.push_back(
    {geomLengthTrajectoryFilter, inputSource, rh.request()});

    inputSource = geomLengthTrajectoryFilter.get();

    // Set the arrow head filter.
    arrowHeadsSource->setIsosurfaceSource(isosurfaceSource);
    arrowHeadsSource->setLineRequest(lineRequest);
    arrowHeadsSource->setInputSelectionSource(inputSource);
    arrowHeadsSource->setInputSourceUVar(var1st->dataSource);
    arrowHeadsSource->setInputSourceVVar(var2nd->dataSource);
    if (varMapped)
    {
        arrowHeadsSource->setInputSourceVar(varMapped->dataSource);
    }
    else
    {
        arrowHeadsSource->setInputSourceVar(nullptr);
    }

    rh.insert("ARROWHEADS_MEMBERS", rh.value("MEMBERS"));
    rh.insert("ARROWHEADS_UV_VARIABLES", var1st->variableName + "/"
              + var2nd->variableName);
    rh.insert("ARROWHEADS_SOURCEVAR",
              (varMapped) ? varMapped->variableName : "");

    arrowRequest = rh.request();

    rh.remove("ARROWHEADS_MEMBERS");
    rh.remove("ARROWHEADS_UV_VARIABLES");
    rh.remove("ARROWHEADS_SOURCEVAR");

    // Set the value trajectory filter. The filter gathers the value information
    // at each intersection line vertex, especially for coloring and
    // thickness mapping.
    valueTrajectorySource->setIsosurfaceSource(isosurfaceSource);
    valueTrajectorySource->setLineRequest(lineRequest);
    valueTrajectorySource->setInputSelectionSource(inputSource);
    if (varMapped)
    {
        valueTrajectorySource->setInputSourceValueVar(varMapped->dataSource);
    }
    else
    {
        valueTrajectorySource->setInputSourceValueVar(nullptr);
    }
    if (varThickness)
    {
        valueTrajectorySource->setInputSourceThicknessVar(
                    varThickness->dataSource);
    }
    else
    {
        valueTrajectorySource->setInputSourceThicknessVar(nullptr);
    }

    rh.insert("TRAJECTORYVALUES_MEMBERS", rh.value("MEMBERS"));
    rh.insert("TRAJECTORYVALUES_VARIABLE",
              (varMapped) ? varMapped->variableName : "");
    rh.insert("TRAJECTORYVALUES_THICKNESSVAR",
              (varThickness) ? varThickness->variableName : "");

    valueRequest = rh.request();
}


void MJetcoreDetectionActor::onFilterChainEnd()
{
    arrowHeadsSource->requestData(arrowRequest);
}


void MJetcoreDetectionActor::asynchronousArrowsAvailable(MDataRequest request)
{
    arrowHeads = arrowHeadsSource->getData(request);

    arrowsVertexBuffer = arrowHeads->getVertexBuffer();

    buildGPUResources();
}


void MJetcoreDetectionActor::dataFieldChangedEvent()
{
    if (enableAutoComputation && variables.size() >= 3)
    {
        requestIsoSurfaceIntersectionLines();
    }
}


void MJetcoreDetectionActor::renderToDepthMap(MSceneViewGLWidget *sceneView)
{
    MIsosurfaceIntersectionActor::renderToDepthMap(sceneView);

    if (arrowsVertexBuffer && appearanceSettingsCores->arrowsEnabled)
    {
        // Draw arrow heads
        lineTubeShader->bindProgram("ArrowHeadsShadowMap");
        CHECK_GL_ERROR;

        lineTubeShader->setUniformValue("mvpMatrix", lightMVP);
        CHECK_GL_ERROR;

        lineTubeShader->setUniformValue("tubeRadius",
                                        appearanceSettings->tubeRadius);
        lineTubeShader->setUniformValue("geometryColor",
                                        appearanceSettings->tubeColor);
        CHECK_GL_ERROR;
        lineTubeShader->setUniformValue("colorMode",
                                        appearanceSettings->colorMode);
        CHECK_GL_ERROR;

        if (appearanceSettings->colorVariableIndex > 0 &&
                appearanceSettings->transferFunction != 0)
        {
            appearanceSettings->transferFunction->getTexture()->bindToTextureUnit(
                        static_cast<GLuint>(
                            appearanceSettings->textureUnitTransferFunction));
            lineTubeShader->setUniformValue(
                        "transferFunction",
                        appearanceSettings->textureUnitTransferFunction);
            lineTubeShader->setUniformValue(
                        "tfMinimum",
                        appearanceSettings->transferFunction->getMinimumValue());
            lineTubeShader->setUniformValue(
                        "tfMaximum",
                        appearanceSettings->transferFunction->getMaximumValue());
            lineTubeShader->setUniformValue("normalized", false);
        }

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

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        CHECK_GL_ERROR;
        glBindBuffer(GL_ARRAY_BUFFER,
                     arrowsVertexBuffer->getVertexBufferObject());
        CHECK_GL_ERROR;

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                              (const GLvoid *) 0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                              (const GLvoid *) (3 * sizeof(float)));

        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                              (const GLvoid *) (6 * sizeof(float)));

        glEnableVertexAttribArray(0);
        CHECK_GL_ERROR;
        glEnableVertexAttribArray(1);
        CHECK_GL_ERROR;
        glEnableVertexAttribArray(2);
        CHECK_GL_ERROR;

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        CHECK_GL_ERROR;
        glDrawArrays(GL_POINTS, 0, arrowHeads->getVertices().size());
        CHECK_GL_ERROR;

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_GL_ERROR;
    }
}


void MJetcoreDetectionActor::renderToCurrentContext(
        MSceneViewGLWidget *sceneView)
{
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }

    MIsosurfaceIntersectionActor::renderToCurrentContext(sceneView);

    // Draw the arrow heads at the end of each jet core line.
    if (arrowsVertexBuffer && appearanceSettingsCores->arrowsEnabled)
    {
        // Draw arrow heads
        lineTubeShader->bindProgram("ArrowHeads");
        CHECK_GL_ERROR;

        lineTubeShader->setUniformValue(
                    "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
        CHECK_GL_ERROR;

        lineTubeShader->setUniformValue("lightMVPMatrix", lightMVP);
        CHECK_GL_ERROR;

        lineTubeShader->setUniformValue("tubeRadius",
                                        appearanceSettings->tubeRadius);
        //appearanceSettings->arrowRadius);
        lineTubeShader->setUniformValue("geometryColor",
                                        appearanceSettings->tubeColor);
        lineTubeShader->setUniformValue("colorMode",
                                        appearanceSettings->colorMode);


        if (appearanceSettings->colorVariableIndex > 0 &&
                appearanceSettings->transferFunction != 0)
        {
            appearanceSettings->transferFunction->getTexture()->bindToTextureUnit(
                        static_cast<GLuint>(
                            appearanceSettings->textureUnitTransferFunction));
            lineTubeShader->setUniformValue(
                        "transferFunction",
                        appearanceSettings->textureUnitTransferFunction);
            lineTubeShader->setUniformValue(
                        "tfMinimum",
                        appearanceSettings->transferFunction->getMinimumValue());
            lineTubeShader->setUniformValue(
                        "tfMaximum",
                        appearanceSettings->transferFunction->getMaximumValue());
            lineTubeShader->setUniformValue("normalized", false);
        }

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
                     arrowsVertexBuffer->getVertexBufferObject());
        CHECK_GL_ERROR;

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                              (const GLvoid *) 0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                              (const GLvoid *) (3 * sizeof(float)));

        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                              (const GLvoid *) (6 * sizeof(float)));

        glEnableVertexAttribArray(0);
        CHECK_GL_ERROR;
        glEnableVertexAttribArray(1);
        CHECK_GL_ERROR;
        glEnableVertexAttribArray(2);
        CHECK_GL_ERROR;

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        CHECK_GL_ERROR;
        glDrawArrays(GL_POINTS, 0, arrowHeads->getVertices().size());
        CHECK_GL_ERROR;

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_GL_ERROR;
    }
}


void MJetcoreDetectionActor::refreshEnumsProperties(MNWPActorVariable *var)
{
    enableActorUpdates(false);

    MIsosurfaceIntersectionActor::refreshEnumsProperties(var);

    QStringList names;
    foreach (MNWPActorVariable *act, variables)
    {
        if (var == nullptr || var != act)
        {
            names.append(act->variableName);
        }
    }

    const QString varNameGeoPot = properties->getEnumItem(variableSettingsCores->geoPotVarProperty);

    properties->mEnum()->setEnumNames(variableSettingsCores->geoPotVarProperty,
                                      names);

    setVariableIndexFromEnumProperty(varNameGeoPot, variableSettingsCores->geoPotVarProperty, variableSettingsCores->geoPotVarIndex);

    enableActorUpdates(true);

    if (enableAutoComputation)
    {
        requestIsoSurfaceIntersectionLines();
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


} // namespace Met3D
