/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2016-2018 Bianca Tost [+]
**  Copyright 2017      Philipp Kaiser [+]
**  Copyright 2020      Marcel Meyer [*]
**  Copyright 2021      Christoph Neuhauser [+]
**
**  + Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
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
#include "trajectoryactor.h"

// standard library imports
#include <iostream>
#include <cmath>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/selectdatasourcedialog.h"
#include "mainwindow.h"
#include "gxfw/selectactordialog.h"
#include "data/trajectoryreader.h"
#include "data/trajectorycomputation.h"
#include "data/multivar/hidpi.h"
#include "actors/movablepoleactor.h"
#include "actors/nwphorizontalsectionactor.h"
#include "actors/nwpverticalsectionactor.h"
#include "actors/volumebboxactor.h"
#include "gxfw/mscenecontrol.h"
#include "system/qtproperties.h"
#include "system/qtproperties_templates.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryActor::MTrajectoryActor()
    : MActor(),
      MBoundingBoxInterface(this, MBoundingBoxConnectionType::HORIZONTAL),
      globalRequestIDCounter(0),
      trajectorySource(nullptr),
      normalsSource(nullptr),
      multiVarTrajectoriesSource(nullptr),
      useMultiVarTrajectories(false),
      trajectoryFilter(nullptr),
      dataSourceID(""),
      precomputedDataSource(false),
      initialDataRequest(true),
      renderMode(TRAJECTORY_TUBES),
      renderColorMode(PRESSURE),
      synchronizationControl(nullptr),
      synchronizeInitTime(true),
      synchronizeStartTime(true),
      synchronizeParticlePosTime(true),
      synchronizeEnsemble(true),
      transferFunction(nullptr),
      textureUnitTransferFunction(-1),
      tubeRadius(0.1),
      sphereRadius(0.2),
      shadowEnabled(true),
      shadowColoured(false)
{
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setActorType(staticActorType());
    setName(getActorType());

    // Remove labels property group since it is not used for trajectory actors
    // yet.
    actorPropertiesSupGroup->removeSubProperty(labelPropertiesSupGroup);

    // Data source selection.
    selectDataSourceProperty = addProperty(
                CLICK_PROPERTY, "select data source",
                actorPropertiesSupGroup);
    utilizedDataSourceProperty = addProperty(
                STRING_PROPERTY, "data source",
                actorPropertiesSupGroup);
    utilizedDataSourceProperty->setEnabled(false);

    // Synchronization properties.
    // ====================================
    synchronizationPropertyGroup = addProperty(
                GROUP_PROPERTY, "synchronization", actorPropertiesSupGroup);

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    synchronizationProperty = addProperty(
                ENUM_PROPERTY, "synchronize with", synchronizationPropertyGroup);
    properties->mEnum()->setEnumNames(
                synchronizationProperty, sysMC->getSyncControlIdentifiers());

    synchronizeInitTimeProperty = addProperty(
                BOOL_PROPERTY, "sync init time", synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeInitTimeProperty,
                                  synchronizeInitTime);
    synchronizeStartTimeProperty = addProperty(
                BOOL_PROPERTY, "sync valid with start time",
                synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeStartTimeProperty,
                                  synchronizeStartTime);
    synchronizeParticlePosTimeProperty = addProperty(
                BOOL_PROPERTY, "sync valid with particles",
                synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeParticlePosTimeProperty,
                                  synchronizeParticlePosTime);
    synchronizeEnsembleProperty = addProperty(
                BOOL_PROPERTY, "sync ensemble", synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeEnsembleProperty,
                                  synchronizeEnsemble);


    initTimeProperty = addProperty(ENUM_PROPERTY, "initialisation",
                                   actorPropertiesSupGroup);

    startTimeProperty = addProperty(ENUM_PROPERTY, "trajectory start",
                                    actorPropertiesSupGroup);

    particlePosTimeProperty = addProperty(ENUM_PROPERTY, "particle positions",
                                          actorPropertiesSupGroup);
    particlePosTimeProperty->setToolTip("Not selectable for 'tubes' and 'all"
                                        " positions' render mode");

    // Ensemble.
    QStringList ensembleModeNames;
    ensembleModeNames << "member" << "all";
    ensembleModeProperty = addProperty(
                ENUM_PROPERTY, "ensemble mode",
                actorPropertiesSupGroup);
    properties->mEnum()->setEnumNames(ensembleModeProperty, ensembleModeNames);
    ensembleModeProperty->setEnabled(false);

    ensembleSingleMemberProperty = addProperty(
                ENUM_PROPERTY, "ensemble member",
                actorPropertiesSupGroup);

    // Property group: Trajectory computation.
    // ====================================
    computationPropertyGroup = addProperty(GROUP_PROPERTY,
                                           "trajectory computation",
                                           actorPropertiesSupGroup);

    computationSeedPropertyGroup = addProperty(
                GROUP_PROPERTY, "seed points (start positions) from actors",
                computationPropertyGroup);
    computationSeedAddActorProperty = addProperty(
                CLICK_PROPERTY, "add seed actor",
                computationSeedPropertyGroup);

    computationLineTypeProperty = addProperty(
                ENUM_PROPERTY, "type",
                computationPropertyGroup);
    properties->mEnum()->setEnumNames(
                computationLineTypeProperty,
                QStringList()
                << "trajectories (path lines)" << "streamlines");

    computationIntegrationMethodProperty = addProperty(
                ENUM_PROPERTY, "integration method",
                computationPropertyGroup);
    properties->mEnum()->setEnumNames(
                computationIntegrationMethodProperty,
                QStringList() << "Euler (3 iter.)" << "Runge-Kutta");

    computationInterpolationMethodProperty = addProperty(
                ENUM_PROPERTY, "interpolation method",
                computationPropertyGroup);
    properties->mEnum()->setEnumNames(
                computationInterpolationMethodProperty,
                QStringList() << "as LAGRANTO v2" << "trilinear (in lon-lat-lnp)");

    computationNumSubTimeStepsProperty = addProperty(
                INT_PROPERTY, "timestep per data t.interv.",
                computationPropertyGroup);
    properties->setInt(computationNumSubTimeStepsProperty, 12, 1, 1000);
    computationNumSubTimeStepsProperty->setToolTip(
                "internal timestep for the trajectory integrations is taken "
                "to be 1/<this value> of the wind data time interval (default "
                "in LAGRANTO v2 is 1/12); wind data is interpolated in time "
                "to times between available wind data timesteps");

    computationIntegrationTimeProperty = addProperty(
                ENUM_PROPERTY, "integration time", computationPropertyGroup);
    computationIntegrationTimeProperty->setToolTip(
                "trajectory integration time (forward/backward) relative to "
                "\"trajectory start\" time specified above");

    computationStreamlineDeltaSProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "seg. length (delta s)",
                computationPropertyGroup);
    properties->setDDouble(
                computationStreamlineDeltaSProperty, 1000., 0.001,
                numeric_limits<double>::max(), 3, 0.5, "");
    computationStreamlineDeltaSProperty->setToolTip(
                "for streamline computations: segment length (delta s) for "
                "integration (dx(s)/ds = v(x); x = position, v = wind)");
    computationStreamlineDeltaSProperty->setEnabled(false);

    computationStreamlineLengthProperty = addProperty(
                INT_PROPERTY, "streamline length",
                computationPropertyGroup);
    properties->setInt(computationStreamlineLengthProperty, 500, 1, 1000000);
    computationStreamlineLengthProperty->setToolTip(
                "length of streamline in terms of number of segments");
    computationStreamlineLengthProperty->setEnabled(false);

    computationRecomputeProperty = addProperty(
                CLICK_PROPERTY, "recompute", computationPropertyGroup);


    // Trajectory filtering property group.
    // ====================================
    filtersGroupProperty =  addProperty(
                GROUP_PROPERTY, "trajectory filters",
                actorPropertiesSupGroup);

    // Bounding box of the actor.
    insertBoundingBoxProperty(actorPropertiesSupGroup);

    // Ascent (dp/dt > x).
    enableAscentFilterProperty = addProperty(
                BOOL_PROPERTY, "min. req. ascent",
                filtersGroupProperty);
    properties->mBool()->setValue(enableAscentFilterProperty, false);
    enableAscentFilterProperty->setToolTip(
                "require trajectories to ascend at least <pressure "
                "difference> in <time interval>.");

    deltaPressureFilterProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "pressure difference",
                enableAscentFilterProperty);
    properties->setDDouble(deltaPressureFilterProperty, 500., 1., 1050., 2, 5.,
                           " hPa");
    deltaTimeFilterProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                          "time interval",
                                          enableAscentFilterProperty);
    properties->setDDouble(deltaTimeFilterProperty, 48, 1, 48, 0, 1, " hrs");

    // Property group: Rendering.
    // ====================================
    renderingGroupProperty = addProperty(GROUP_PROPERTY, "rendering",
                                         actorPropertiesSupGroup);

    // Render mode.
    QStringList renderModeNames;
    renderModeNames << "entire trajectories as tubes"
                    << "all particle positions"
                    << "single time positions"
                    << "tubes and single time positions"
                    << "backward tubes and single time positions";
    renderModeProperty = addProperty(ENUM_PROPERTY, "render mode",
                                     renderingGroupProperty);
    properties->mEnum()->setEnumNames(renderModeProperty, renderModeNames);
    properties->mEnum()->setValue(renderModeProperty, renderMode);

    // Render colors.
    QStringList renderColorModeNames;
    renderColorModeNames << "pressure"
                         << "auxiliary variable";
    renderColorModeProperty = addProperty(ENUM_PROPERTY, "color according to",
                                          renderingGroupProperty);
    properties->mEnum()->setEnumNames(renderColorModeProperty,
                                      renderColorModeNames);
    properties->mEnum()->setValue(renderColorModeProperty, renderColorMode);

    // Choose auxiliary data variable for rendering.
    renderAuxDataVarProperty = addProperty(ENUM_PROPERTY, "auxiliary variable",
                                           renderingGroupProperty);
    renderAuxDataVarProperty->setEnabled(false);

    // Transfer function.
    // Scan currently available actors for transfer functions. Add TFs to
    // the list displayed in the combo box of the transferFunctionProperty.
    QStringList availableTFs;
    availableTFs << "None";
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    foreach (MActor *mactor, glRM->getActors())
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(mactor))
        {
            availableTFs << tf->transferFunctionName();
        }
    }
    transferFunctionProperty = addProperty(ENUM_PROPERTY, "transfer function",
                                           renderingGroupProperty);
    properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
    transferFunctionProperty->setToolTip("This transfer function is used "
                                         "for mapping either pressure or the "
                                         "selected auxiliary variable to "
                                         "the trajectory's colour.");

    // Render mode and parameters.
    tubeRadiusProperty = addProperty(DECORATEDDOUBLE_PROPERTY, "tube radius",
                                     renderingGroupProperty);
    properties->setDDouble(tubeRadiusProperty, tubeRadius,
                           0.01, 1., 2, 0.1, " (world space)");

    sphereRadiusProperty = addProperty(DECORATEDDOUBLE_PROPERTY, "sphere radius",
                                       renderingGroupProperty);
    properties->setDDouble(sphereRadiusProperty, sphereRadius,
                           0.01, 1., 2, 0.1, " (world space)");

    enableShadowProperty = addProperty(BOOL_PROPERTY, "display shadows",
                                       renderingGroupProperty);
    properties->mBool()->setValue(enableShadowProperty, shadowEnabled);

    colourShadowProperty = addProperty(BOOL_PROPERTY, "colour shadows",
                                       renderingGroupProperty);
    properties->mBool()->setValue(colourShadowProperty, shadowColoured);


    // Property group: Multi-variable rendering.
    // ====================================
    useMultiVarTrajectoriesProperty = addProperty(
            BOOL_PROPERTY, "use multi-var rendering", renderingGroupProperty);
    properties->mBool()->setValue(useMultiVarTrajectoriesProperty, useMultiVarTrajectories);

    multiVarGroupProperty = addProperty(
            GROUP_PROPERTY, "multi-var rendering", renderingGroupProperty);

    diagramTypeProperty = addProperty(
            ENUM_PROPERTY, "chart type", multiVarGroupProperty);
    properties->mEnum()->setEnumNames(diagramTypeProperty, diagramTypeNames);
    properties->mEnum()->setValue(diagramTypeProperty, static_cast<int>(diagramType));

    diagramTransferFunctionProperty = addProperty(
            ENUM_PROPERTY, "chart transfer function", multiVarGroupProperty);
    properties->mEnum()->setEnumNames(diagramTransferFunctionProperty, availableTFs);
    diagramTransferFunctionProperty->setToolTip(
            "This transfer function is used in the chart to map a trajectory attribute to a color.");

    similarityMetricGroup = addProperty(
            GROUP_PROPERTY, "similarity metric settings", multiVarGroupProperty);

    similarityMetricProperty = addProperty(
            ENUM_PROPERTY, "similarity metric", similarityMetricGroup);
    int numSimilarityMetrics = int(sizeof(SIMILARITY_METRIC_NAMES) / sizeof(*SIMILARITY_METRIC_NAMES));
    QStringList similarityMetricNames;
    for (int i = 0; i < numSimilarityMetrics; i++) {
        similarityMetricNames.push_back(SIMILARITY_METRIC_NAMES[i]);
    }
    properties->mEnum()->setEnumNames(similarityMetricProperty, similarityMetricNames);
    properties->mEnum()->setValue(similarityMetricProperty, int(similarityMetric));
    similarityMetricProperty->setToolTip(
            "The similarity metric to use when ordering variables after clicking on the diagram.");

    meanMetricInfluenceProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "mean metric influence", similarityMetricGroup);
    properties->setDDouble(
            meanMetricInfluenceProperty, meanMetricInfluence,
            0.0, 100.0, 2, 0.1, " (factor)");
    meanMetricInfluenceProperty->setToolTip(
            "Influence factor of the similarity of the variable means on the final sorting order.");

    stdDevMetricInfluenceProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "std. dev. metric influence", similarityMetricGroup);
    properties->setDDouble(
            stdDevMetricInfluenceProperty, stdDevMetricInfluence,
            0.0, 100.0, 2, 0.1, " (factor)");
    stdDevMetricInfluenceProperty->setToolTip(
            "Influence factor of the similarity of the variable standard deviations on the final sorting order.");

    numBinsProperty = addProperty(
            INT_PROPERTY, "#bins", similarityMetricGroup);
    properties->setInt(numBinsProperty, numBins, 1, 20);
    numBinsProperty->setToolTip(
            "Number of histogram bins used for the Mutual Information (MI) similarity metric.");

    sortByDescendingStdDevProperty = addProperty(
            CLICK_PROPERTY, "sort by desc. std. dev.", similarityMetricGroup);
    sortByDescendingStdDevProperty->setToolTip(
            "Sorts the variables in the diagram by their cross-trajectory "
            "standard deviation at the selected time step.");

    showMinMaxValueProperty = addProperty(
            BOOL_PROPERTY, "show min/max", similarityMetricGroup);
    properties->mBool()->setValue(showMinMaxValueProperty, showMinMaxValue);
    showMinMaxValueProperty->setToolTip(
            "Whether to show the minimum and maximum value in the diagram if the zoom factor permits it.");

    useMaxForSensitivityProperty = addProperty(
            BOOL_PROPERTY, "use max for sensitivity", similarityMetricGroup);
    properties->mBool()->setValue(useMaxForSensitivityProperty, useMaxForSensitivity);
    useMaxForSensitivityProperty->setToolTip(
            "Whether to show the minimum and maximum value in the diagram if the zoom factor permits it.");

    trimNanRegionsProperty = addProperty(
            BOOL_PROPERTY, "trim NaN regions", similarityMetricGroup);
    properties->mBool()->setValue(trimNanRegionsProperty, trimNanRegions);
    trimNanRegionsProperty->setToolTip(
            "Whether to remove regions with only NaN values at the beginning or end of the loaded data.");

    subsequenceMatchingTechniqueProperty = addProperty(
            ENUM_PROPERTY, "matching technique", similarityMetricGroup);
    int numSubsequenceMatchingTechniques =
            int(sizeof(SUBSEQUENCE_MATCHING_TECHNIQUE_NAMES) / sizeof(*SUBSEQUENCE_MATCHING_TECHNIQUE_NAMES));
    QStringList subsequenceMatchingTechniqueNames;
    for (int i = 0; i < numSubsequenceMatchingTechniques; i++) {
        subsequenceMatchingTechniqueNames.push_back(SUBSEQUENCE_MATCHING_TECHNIQUE_NAMES[i]);
    }
    properties->mEnum()->setEnumNames(
            subsequenceMatchingTechniqueProperty, subsequenceMatchingTechniqueNames);
    properties->mEnum()->setValue(
            subsequenceMatchingTechniqueProperty, int(subsequenceMatchingTechnique));
    similarityMetricProperty->setToolTip(
            "Which technique to use for sub-sequence matching in the curve-plot view.");

    springEpsilonProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "SPRING metric epsilon", similarityMetricGroup);
    properties->setDDouble(
            springEpsilonProperty, springEpsilon,
            0.1, 100.0, 2, 0.1, " (factor)");
    springEpsilonProperty->setToolTip(
            "SPRING subsequence matching algorithm distance metric epsilon.");

    backgroundOpacityProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "diagram opacity", similarityMetricGroup);
    properties->setDDouble(
            backgroundOpacityProperty, backgroundOpacity,
            0.0, 1.0, 2, 0.05, " (factor)");
    backgroundOpacityProperty->setToolTip("Diagram view background opacity.");

    diagramNormalizationModeProperty = addProperty(
            ENUM_PROPERTY, "diagram normalization mode", similarityMetricGroup);
    properties->mEnum()->setEnumNames(diagramNormalizationModeProperty, diagramNormalizationModeNames);
    properties->mEnum()->setValue(
            diagramNormalizationModeProperty, static_cast<int>(diagramNormalizationMode));
    diagramNormalizationModeProperty->setToolTip(
            "Whether to normalize the data by using the minimum/maximum also over not selected trajectories.");

    diagramTextSizeProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "diagram text size", similarityMetricGroup);
    properties->setDDouble(
            diagramTextSizeProperty, diagramTextSize,
            4.0, 64.0, 2, 1.0, "px");
    diagramTextSizeProperty->setToolTip("Diagram text size.");

    diagramUpscalingFactor = getHighDPIScaleFactor();
    useCustomDiagramUpscalingFactorProperty = addProperty(
            BOOL_PROPERTY, "use custom upscaling", similarityMetricGroup);
    properties->mBool()->setValue(useCustomDiagramUpscalingFactorProperty, useCustomDiagramUpscalingFactor);
    useCustomDiagramUpscalingFactorProperty->setToolTip(
            "Whether to normalize the data by using the minimum/maximum also over not selected trajectories.");
    diagramUpscalingFactorProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "diagram upscaling factor", similarityMetricGroup);
    properties->setDDouble(
            diagramUpscalingFactorProperty, diagramUpscalingFactor,
            0.25, 8.0, 3, 0.125, " (factor)");
    diagramUpscalingFactorProperty->setToolTip("Custom diagram upscaling factor.");
    diagramUpscalingFactorProperty->setEnabled(useCustomDiagramUpscalingFactor);


    updateSimilarityMetricGroupEnabled();


    trajectorySyncModeProperty = addProperty(
            ENUM_PROPERTY, "sync mode", multiVarGroupProperty);
    properties->mEnum()->setEnumNames(trajectorySyncModeProperty, trajectorySyncModeNames);
    properties->mEnum()->setValue(trajectorySyncModeProperty, static_cast<int>(trajectorySyncMode));
    trajectorySyncModeProperty->setToolTip(
            "Specifies whether to sync warm conveyor belt trajectories based on their ascension or height. "
            "The trajectories need to have the per-point attribute time_after_ascension for this to work.");

    // analysisGroupProperty for old paper videos.
    useVariableToolTipProperty = addProperty(
            BOOL_PROPERTY, "use variable tool tip", multiVarGroupProperty);
    properties->mBool()->setValue(useVariableToolTipProperty, useVariableToolTip);
    useVariableToolTipProperty->setToolTip("Use variable tool tip.");

    selectAllTrajectoriesProperty = addProperty(
            CLICK_PROPERTY, "select all trajectories", multiVarGroupProperty);
    selectAllTrajectoriesProperty->setToolTip("Selects (or unselects) all trajectories in the scene.");

    resetVariableSortingProperty = addProperty(
            CLICK_PROPERTY, "reset variable sorting", multiVarGroupProperty);
    resetVariableSortingProperty->setToolTip("Resets the variable sorting.");


    multiVarData.setProperties(this, properties, multiVarGroupProperty);
    actorHasSelectableData = true;


    // Property group: Analysis.
    // ====================================
    analysisGroupProperty = addProperty(GROUP_PROPERTY, "analysis",
                                        actorPropertiesSupGroup);

    outputAsLagrantoASCIIProperty = addProperty(
                CLICK_PROPERTY, "output as LAGRANTO ASCII",
                analysisGroupProperty);

    // ====================================

    // Observe the creation/deletion of other actors
    // -- if these are transfer functions add to the list displayed in the transfer function property.
    // -- if these are used as seed actors, change corresponding properties
    connect(glRM, SIGNAL(actorCreated(MActor*)), SLOT(onActorCreated(MActor*)));
    connect(glRM, SIGNAL(actorDeleted(MActor*)), SLOT(onActorDeleted(MActor*)));
    connect(glRM, SIGNAL(actorRenamed(MActor*, QString)),
            SLOT(onActorRenamed(MActor*, QString)));

    endInitialiseQtProperties();
}


MTrajectoryActor::~MTrajectoryActor()
{
    // Delete synchronization links (don't update the already deleted GUI
    // properties anymore...).
    synchronizeWith(nullptr, false);

    if (textureUnitTransferFunction >=0)
        releaseTextureUnit(textureUnitTransferFunction);

#ifdef USE_EMBREE
    for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
    {
        delete trajectoryPicker;
    }
#endif
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE 0
#define SHADER_NORMAL_ATTRIBUTE 1
#define SHADER_AUXDATA_ATTRIBUTE 2

void MTrajectoryActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);

    beginCompileShaders(4);

    compileShadersFromFileWithProgressDialog(
            tubeShader,
            "src/glsl/trajectory_tubes.fx.glsl");
    compileShadersFromFileWithProgressDialog(
            tubeShadowShader,
            "src/glsl/trajectory_tubes_shadow.fx.glsl");
    compileShadersFromFileWithProgressDialog(
            positionSphereShader,
            "src/glsl/trajectory_positions.fx.glsl");
    compileShadersFromFileWithProgressDialog(
            positionSphereShadowShader,
            "src/glsl/trajectory_positions_shadow.fx.glsl");

    endCompileShaders();
}


void MTrajectoryActor::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MTrajectoryActor::getSettingsID());

    settings->setValue("dataSourceID", dataSourceID);

    settings->setValue("renderMode", static_cast<int>(renderMode));
    settings->setValue("renderColorMode", static_cast<int>(renderColorMode));

    // Save synchronization properties.
    settings->setValue("synchronizationID",
                       (synchronizationControl != nullptr) ?
                           synchronizationControl->getID() : "");
    settings->setValue("synchronizeInitTime",
                       properties->mBool()->value(synchronizeInitTimeProperty));
    settings->setValue("synchronizeStartTime",
                       properties->mBool()->value(synchronizeStartTimeProperty));
    settings->setValue("synchronizeParticlePosTime",
                       properties->mBool()->value(
                           synchronizeParticlePosTimeProperty));
    settings->setValue("synchronizeEnsemble",
                       properties->mBool()->value(synchronizeEnsembleProperty));

    settings->setValue("enableFilter",
                       properties->mBool()->value(enableAscentFilterProperty));

    settings->setValue("deltaPressure",
                       properties->mDDouble()->value(deltaPressureFilterProperty));
    settings->setValue("deltaTime",
                       properties->mDDouble()->value(deltaTimeFilterProperty));

    MBoundingBoxInterface::saveConfiguration(settings);

    settings->setValue("transferFunction",
                       properties->getEnumItem(transferFunctionProperty));

    settings->setValue("useMultiVarTrajectories",
                       properties->mBool()->value(useMultiVarTrajectoriesProperty));
    settings->setValue("diagramType",
                       properties->mEnum()->value(diagramTypeProperty));
    settings->setValue("diagramTransferFunction",
                       properties->getEnumItem(diagramTransferFunctionProperty));
    settings->setValue(QString("useVariableToolTip"), useVariableToolTip);

    settings->setValue(QString("similarityMetric"), int(similarityMetric));
    settings->setValue(QString("meanMetricInfluence"), meanMetricInfluence);
    settings->setValue(QString("stdDevMetricInfluence"), stdDevMetricInfluence);
    settings->setValue(QString("numBins"), numBins);
    settings->setValue(QString("subsequenceMatchingTechnique"), int(subsequenceMatchingTechnique));
    settings->setValue(QString("springEpsilon"), springEpsilon);
    settings->setValue(QString("backgroundOpacity"), backgroundOpacity);
    settings->setValue(
            "diagramNormalizationMode", properties->mEnum()->value(diagramNormalizationModeProperty));
    settings->setValue(QString("diagramTextSize"), diagramTextSize);
    settings->setValue(QString("useCustomDiagramUpscalingFactor"), useCustomDiagramUpscalingFactor);
    settings->setValue(QString("diagramUpscalingFactor"), diagramUpscalingFactor);

    settings->setValue(QString("trajectorySyncMode"), int(trajectorySyncMode));
    settings->setValue(QString("syncModeTrajectoryIndex"), syncModeTrajectoryIndex);
    settings->setValue(QString("particlePosTimeStep"), int(particlePosTimeStep));

    multiVarData.saveConfiguration(settings);

    settings->setValue("tubeRadius", tubeRadius);
    settings->setValue("sphereRadius", sphereRadius);
    settings->setValue("shadowEnabled", shadowEnabled);
    settings->setValue("shadowColoured", shadowColoured);

    // Save computation properties.
    settings->setValue(
                "computationLineTypeProperty",
                properties->mEnum()->value(computationLineTypeProperty));
    settings->setValue(
                "computationIterationMethodProperty",
                properties->mEnum()->value(computationLineTypeProperty));
    settings->setValue("computationInterpolationMethodProperty",
                       properties->mEnum()->value(
                           computationInterpolationMethodProperty));
    settings->setValue(
                "computationNumSubTimeStepsProperty",
                properties->mInt()->value(computationNumSubTimeStepsProperty));
    settings->setValue(
                "computationIntegrationTimeProperty",
                properties->mEnum()->value(computationIntegrationTimeProperty));
    settings->setValue(
                "computationStreamlineDeltaSProperty",
                properties->mDDouble()->value(computationStreamlineDeltaSProperty));
    settings->setValue(
                "computationStreamlineLengthProperty",
                properties->mInt()->value(computationStreamlineLengthProperty));
    settings->setValue(
                "computationSeedActorSize",
                computationSeedActorProperties.size());
    for (int i = 0; i <  computationSeedActorProperties.size(); i++)
    {
        const SeedActorSettings& sas = computationSeedActorProperties.at(i);
        settings->setValue(QString("computationSeedActorName%1").arg(i),
                           sas.propertyGroup->propertyName());
        settings->setValue(QString("computationSeedActorStepSizeLon%1").arg(i),
                           properties->mDDouble()->value(sas.lonSpacing));
        settings->setValue(QString("computationSeedActorStepSizeLat%1").arg(i),
                           properties->mDDouble()->value(sas.latSpacing));
        settings->setValue(
                    QString("computationSeedActorPressureLevels%1").arg(i),
                    properties->mString()->value(sas.pressureLevels));
    }

    settings->setValue(
            "computationSeedActorSize",
            computationSeedActorProperties.size());

    settings->endGroup();
}


void MTrajectoryActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MTrajectoryActor::getSettingsID());

    QString dataSourceID = settings->value("dataSourceID", "").toString();

    releaseData();
    enableProperties(true);

    bool dataSourceAvailable = false;

    if ( !dataSourceID.isEmpty() )
    {
        dataSourceAvailable =
                MSelectDataSourceDialog::checkForTrajectoryDataSource(
                    dataSourceID);
        // The configuration file specifies a data source which does not exist.
        // Display warning and load rest of the configuration.
        if (!dataSourceAvailable)
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(QString("Trajectory actor '%1':\n"
                                   "Data source '%2' does not exist.\n"
                                   "Select a new one.")
                           .arg(getName()).arg(dataSourceID));
            msgBox.exec();
            if (isInitialized())
            {
                if (!selectDataSource())
                {
                    // User has selected no data source. Display a warning and
                    // disable all trajectory properties.
                    QMessageBox msgBox;
                    msgBox.setIcon(QMessageBox::Warning);
                    msgBox.setText("No data source selected. Disabling all"
                                   " properties.");
                    msgBox.exec();
                    enableActorUpdates(false);
                    properties->mString()->setValue(
                                utilizedDataSourceProperty, "");
                    enableActorUpdates(true);
                    enableProperties(false);
                }
                else
                {
                    dataSourceAvailable = true;
                }
            }
        }
        else
        {
            properties->mString()->setValue(utilizedDataSourceProperty,
                                            dataSourceID);

            this->setDataSource(dataSourceID + QString(" Reader"));
            this->setNormalsSource(dataSourceID + QString(" Normals"));
            this->setTrajectoryFilter(dataSourceID + QString(" timestepFilter"));
            this->setMultiVarTrajectoriesSource(dataSourceID + QString(" Multi-Var Trajectories"));

            updateInitTimeProperty();
            updateStartTimeProperty();
            updateEnsembleSingleMemberProperty();
            updateAuxDataVarNamesProperty();
        }
    }

    // set default values for rendering and coloring mode
    properties->mEnum()->setValue(
                renderModeProperty,
                settings->value("renderMode", 0).toInt());

    properties->mEnum()->setValue(
                renderColorModeProperty,
                settings->value("renderColorMode", 0).toInt());


    // Load synchronization properties (AFTER the ensemble mode properties
    // have been loaded; sync may overwrite some settings).
    // ===================================================================
    properties->mBool()->setValue(
                synchronizeInitTimeProperty,
                settings->value("synchronizeInitTime", true).toBool());
    properties->mBool()->setValue(
                synchronizeStartTimeProperty,
                settings->value("synchronizeStartTime", true).toBool());
    properties->mBool()->setValue(
                synchronizeParticlePosTimeProperty,
                settings->value("synchronizeParticlePosTime", true).toBool());
    properties->mBool()->setValue(
                synchronizeEnsembleProperty,
                settings->value("synchronizeEnsemble", true).toBool());

    QString syncID = settings->value("synchronizationID", "").toString();

    if ( !syncID.isEmpty() )
    {
        MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
        if (sysMC->getSyncControlIdentifiers().contains(syncID))
        {
            synchronizeWith(sysMC->getSyncControl(syncID));
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(QString("Trajectory actor '%1':\n"
                                   "Synchronization control '%2' does not exist.\n"
                                   "Setting synchronization control to 'None'.")
                           .arg(getName()).arg(syncID));
            msgBox.exec();

            synchronizeWith(nullptr);
        }
    }

    properties->mBool()->setValue(
                enableAscentFilterProperty,
                settings->value("enableFilter", true).toBool());

    properties->mDDouble()->setValue(
                deltaPressureFilterProperty,
                settings->value("deltaPressure", 500.).toDouble());

    // The range is adapted to the data source thus when changing sessions it
    // might appear the value can't be set correctly due to the property is
    // still restricted to the range of the data sourc of the previous session.
    // Therefore we set the range to a range which covers all possible value.
    properties->mDDouble()->setRange(
                deltaTimeFilterProperty, 0.,
                std::numeric_limits<double>::max());

    properties->mDDouble()->setValue(
                deltaTimeFilterProperty,
                settings->value("deltaTime", 48.).toDouble());

    MBoundingBoxInterface::loadConfiguration(settings);

    QString tfName = settings->value("transferFunction").toString();
    while (!setTransferFunction(tfName))
    {
        if (!MTransferFunction::loadMissingTransferFunction(
                    tfName, MTransferFunction1D::staticActorType(),
                    "Trajectories Actor ", getName(), settings))
        {
            break;
        }
    }

    useMultiVarTrajectories =
            settings->value("useMultiVarTrajectories", false).toBool()
            || settings->value("useBezierTrajectories", false).toBool();
    properties->mBool()->setValue(useMultiVarTrajectoriesProperty, useMultiVarTrajectories);

    properties->mEnum()->setValue(
            diagramTypeProperty,
            settings->value("diagramType", 0).toInt());
    diagramType = static_cast<DiagramDisplayType>(properties->mEnum()->value(diagramTypeProperty));
    properties->mEnum()->setValue(diagramTypeProperty, static_cast<int>(diagramType));

    similarityMetric = SimilarityMetric(settings->value(
            "similarityMetric", int(SimilarityMetric::ABSOLUTE_NCC)).toInt());
    properties->mEnum()->setValue(similarityMetricProperty, int(similarityMetric));
    meanMetricInfluence = settings->value("meanMetricInfluence", 0.5f).toFloat();
    properties->setDDouble(
            meanMetricInfluenceProperty, meanMetricInfluence,
            0.0, 100.0, 2, 0.1, " (factor)");
    stdDevMetricInfluence = settings->value("stdDevMetricInfluence", 0.25f).toFloat();
    properties->setDDouble(
            stdDevMetricInfluenceProperty, stdDevMetricInfluence,
            0.0, 100.0, 2, 0.1, " (factor)");
    numBins = settings->value("numBins", 10).toInt();
    properties->setInt(numBinsProperty, numBins, 1, 20);
    showMinMaxValue = settings->value("showMinMaxValue", true).toBool();
    properties->mBool()->setValue(showMinMaxValueProperty, showMinMaxValue);
    useMaxForSensitivity = settings->value("useMaxForSensitivity", true).toBool();
    properties->mBool()->setValue(useMaxForSensitivityProperty, useMaxForSensitivity);
    trimNanRegions = settings->value("trimNanRegions", true).toBool();
    properties->mBool()->setValue(trimNanRegionsProperty, trimNanRegions);
    subsequenceMatchingTechnique = SubsequenceMatchingTechnique(settings->value(
            "subsequenceMatchingTechnique", int(SubsequenceMatchingTechnique::SPRING)).toInt());
    properties->mEnum()->setValue(subsequenceMatchingTechniqueProperty, int(subsequenceMatchingTechnique));
    springEpsilon = settings->value("springEpsilon", 10.0f).toFloat();
    properties->setDDouble(
            springEpsilonProperty, springEpsilon,
            0.0, 100.0, 2, 0.1, " (factor)");
    backgroundOpacity = settings->value("springEpsilon", 10.0f).toFloat();
    properties->setDDouble(
            backgroundOpacityProperty, backgroundOpacity,
            0.0, 1.0, 2, 0.05, " (factor)");
    properties->mEnum()->setValue(
            diagramNormalizationModeProperty,
            settings->value("diagramNormalizationMode", 0).toInt());
    diagramNormalizationMode = static_cast<DiagramNormalizationMode>(properties->mEnum()->value(
            diagramNormalizationModeProperty));
    properties->mEnum()->setValue(
            diagramNormalizationModeProperty, static_cast<int>(diagramNormalizationMode));

    diagramTextSize = settings->value("diagramTextSize", 8.0f).toFloat();
    properties->setDDouble(
            diagramTextSizeProperty, diagramTextSize,
            4.0, 64.0, 2, 1.0, "px");
    useCustomDiagramUpscalingFactor = settings->value("useCustomDiagramUpscalingFactor", false).toBool();
    properties->mBool()->setValue(useCustomDiagramUpscalingFactorProperty, useCustomDiagramUpscalingFactor);
    diagramUpscalingFactor = settings->value(
            "diagramUpscalingFactor", getHighDPIScaleFactor()).toFloat();
    properties->setDDouble(
            diagramUpscalingFactorProperty, diagramUpscalingFactor,
            0.25, 8.0, 3, 0.125, " (factor)");
    diagramUpscalingFactorProperty->setEnabled(useCustomDiagramUpscalingFactor);
    updateSimilarityMetricGroupEnabled();

    trajectorySyncMode = TrajectorySyncMode(settings->value(
            "trajectorySyncMode", int(TrajectorySyncMode::TIMESTEP)).toInt());
    properties->mEnum()->setValue(trajectorySyncModeProperty, int(trajectorySyncMode));
#ifdef USE_EMBREE
    for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
    {
        trajectoryPicker->setSyncMode(trajectorySyncMode);
    }
#endif

    syncModeTrajectoryIndex = settings->value(
            "syncModeTrajectoryIndex", syncModeTrajectoryIndex).toUInt();
    if (settings->contains("particlePosTimeStep"))
    {
        particlePosTimeStep = settings->value("particlePosTimeStep", particlePosTimeStep).toInt();
        cachedParticlePosTimeStep = particlePosTimeStep;
        loadedParticlePosTimeStepFromSettings = true;
        if (!properties->mEnum()->enumNames(particlePosTimeProperty).empty())
        {
            properties->mEnum()->setValue(particlePosTimeProperty, particlePosTimeStep);
        }
        if (synchronizeParticlePosTime)
        {
            synchronizeParticlePosTime = false;
            properties->mBool()->setValue(
                    synchronizeParticlePosTimeProperty,
                    synchronizeParticlePosTime);
            updateTimeProperties();
        }
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setParticlePosTimeStep(particlePosTimeStep);
        }
#endif
    }

    useVariableToolTip = settings->value("showMinMaxValue", true).toBool();
    properties->mBool()->setValue(useVariableToolTipProperty, useVariableToolTip);
    useVariableToolTipChanged = true;

    multiVarData.setEnabled(useMultiVarTrajectories);
    multiVarData.loadConfiguration(settings);

    properties->mDDouble()->setValue(
                tubeRadiusProperty,
                settings->value("tubeRadius", 0.1).toDouble());

    properties->mDDouble()->setValue(
                sphereRadiusProperty,
                settings->value("sphereRadius", 0.2).toDouble());

    properties->mBool()->setValue(
                enableShadowProperty,
                settings->value("shadowEnabled", true).toBool());

    properties->mBool()->setValue(
                colourShadowProperty,
                settings->value("shadowColoured", false).toBool());


    // Load computation properties (The saved actors should already be initialized)
    properties->mEnum()->setValue(
                computationLineTypeProperty,
                settings->value("computationLineTypeProperty", 0).toInt());
    properties->mEnum()->setValue(
                computationIntegrationMethodProperty,
                settings->value("computationIterationMethodProperty",
                                0).toInt());
    properties->mEnum()->setValue(
                computationInterpolationMethodProperty,
                settings->value("computationInterpolationMethodProperty",
                                0).toInt());
    properties->mInt()->setValue(
                computationNumSubTimeStepsProperty,
                settings->value("computationNumSubTimeStepsProperty",
                                12).toInt());
    properties->mEnum()->setValue(
                computationIntegrationTimeProperty,
                settings->value("computationIntegrationTimeProperty",
                                0).toInt());
    properties->mDDouble()->setValue(
                computationStreamlineDeltaSProperty,
                settings->value("computationStreamlineDeltaSProperty",
                                1000.).toDouble());
    properties->mInt()->setValue(
                computationStreamlineLengthProperty,
                settings->value("computationStreamlineLengthProperty",
                                500.).toDouble());

    // Remove current seed actors.
    clearSeedActor();

    const int actorCount =
            settings->value("computationSeedActorSize", 0).toInt();
    for (int i = 0; i < actorCount; i++)
    {
        QString actorName = settings->value(
                    QString("computationSeedActorName%1").arg(i)).toString();
        double deltaLon =
                settings->value(QString("computationSeedActorStepSizeLon%1")
                                .arg(i)).toDouble();
        double deltaLat =
                settings->value(QString("computationSeedActorStepSizeLat%1")
                                .arg(i)).toDouble();
        QString presLvls =
                settings->value(QString("computationSeedActorPressureLevels%1")
                                .arg(i)).toString();
        addSeedActor(actorName, deltaLon, deltaLat,
                     parseFloatRangeString(presLvls));
    }

    settings->endGroup();

    if (isInitialized() && dataSourceAvailable)
    {
        updateActorData();
        asynchronousSelectionRequest();
        asynchronousDataRequest();
    }
    enableProperties(dataSourceAvailable);
}


void MTrajectoryActor::onBoundingBoxChanged()
{
    labels.clear();
    if (dataSourceID == "" || suppressActorUpdates())
    {
        return;
    }
    // Different from other actors the trajectory actor needs a recomputation
    // of its trajectories if swichting to no bounding box since this switch
    // disables the bounding box filter.
    asynchronousSelectionRequest();
}


void MTrajectoryActor::setTransferFunction(MTransferFunction1D *tf)
{
    transferFunction = tf;
}


bool MTrajectoryActor::setTransferFunction(QString tfName)
{
    QStringList tfNames = properties->mEnum()->enumNames(transferFunctionProperty);
    int tfIndex = tfNames.indexOf(tfName);

    if (tfIndex >= 0)
    {
        properties->mEnum()->setValue(transferFunctionProperty, tfIndex);
        return true;
    }

    // Set transfer function property to "None".
    properties->mEnum()->setValue(transferFunctionProperty, 0);

    return false; // the given tf name could not be found
}


void MTrajectoryActor::setDiagramTransferFunction(MTransferFunction1D *tf)
{
    transferFunction = tf;
}


bool MTrajectoryActor::setDiagramTransferFunction(QString tfName)
{
    QStringList tfNames = properties->mEnum()->enumNames(diagramTransferFunctionProperty);
    int tfIndex = tfNames.indexOf(tfName);

    if (tfIndex >= 0)
    {
        properties->mEnum()->setValue(diagramTransferFunctionProperty, tfIndex);
        return true;
    }

    // Set transfer function property to "None".
    properties->mEnum()->setValue(diagramTransferFunctionProperty, 0);

    return false; // the given tf name could not be found
}


void MTrajectoryActor::synchronizeWith(
        MSyncControl *sync, bool updateGUIProperties)
{
    if (synchronizationControl == sync)
    {
        return;
    }

    // Reset connection to current synchronization control.
    // ====================================================

    // If the variable is currently connected to a sync control, reset the
    // background colours of the start and init time properties (they have
    // been set to red/green from this class to indicate time sync status,
    // see setStartDateTime()) and disconnect the signals.
    if (synchronizationControl != nullptr)
    {
        foreach (MSceneControl* scene, getScenes())
        {
            scene->variableDeletesSynchronizationWith(synchronizationControl);
        }

#ifdef DIRECT_SYNCHRONIZATION
        synchronizationControl->deregisterSynchronizedClass(this);
#else
        disconnect(synchronizationControl, SIGNAL(initDateTimeChanged(QDateTime)),
                   this, SLOT(setInitDateTime(QDateTime)));
        disconnect(synchronizationControl, SIGNAL(validDateTimeChanged(QDateTime)),
                   this, SLOT(setStartDateTime(QDateTime)));
        disconnect(synchronizationControl, SIGNAL(ensembleMemberChanged(int)),
                   this, SLOT(setEnsembleMember(int)));
#endif
    }
    // Connect to new sync control and try to switch to its current times.
    synchronizationControl = sync;

    // Update "synchronizationProperty".
    // =================================
    if (updateGUIProperties)
    {
        QString displayedSyncID =
                properties->getEnumItem(synchronizationProperty);
        QString newSyncID =
                (sync == nullptr) ? "None" : synchronizationControl->getID();
        if (displayedSyncID != newSyncID)
        {
            enableActorUpdates(false);
            properties->setEnumItem(synchronizationProperty, newSyncID);
            enableActorUpdates(true);
        }
    }

    // Connect to new sync control and synchronize.
    // ============================================
    if (sync != nullptr)
    {
        // Tell the actor's scenes that this variable synchronized with this
        // sync control.
        foreach (MSceneControl* scene, getScenes())
        {
            scene->variableSynchronizesWith(sync);
        }

#ifdef DIRECT_SYNCHRONIZATION
        synchronizationControl->registerSynchronizedClass(this);
#else
//TODO (mr, 01Dec2015) -- add checks for synchronizeInitTime etc..
        connect(sync, SIGNAL(initDateTimeChanged(QDateTime)),
                this, SLOT(setInitDateTime(QDateTime)));
        connect(sync, SIGNAL(validDateTimeChanged(QDateTime)),
                this, SLOT(setStartDateTime(QDateTime)));
        connect(sync, SIGNAL(ensembleMemberChanged(int)),
                this, SLOT(setEnsembleMember(int)));
#endif
        if (updateGUIProperties)
        {
            enableActorUpdates(false);
            synchronizeInitTimeProperty->setEnabled(true);
            synchronizeInitTime = properties->mBool()->value(
                        synchronizeInitTimeProperty);
            synchronizeStartTimeProperty->setEnabled(true);
            synchronizeStartTime = properties->mBool()->value(
                        synchronizeStartTimeProperty);
            synchronizeParticlePosTimeProperty->setEnabled(true);
            synchronizeParticlePosTime = properties->mBool()->value(
                        synchronizeParticlePosTimeProperty);
            synchronizeEnsembleProperty->setEnabled(true);
            synchronizeEnsemble = properties->mBool()->value(
                        synchronizeEnsembleProperty);
            enableActorUpdates(true);
        }

        if (synchronizeInitTime)
        {
            setInitDateTime(sync->initDateTime());
        }
        if (synchronizeStartTime)
        {
            setStartDateTime(sync->validDateTime());
        }
        if (synchronizeParticlePosTime)
        {
            setParticleDateTime(sync->validDateTime());
        }
        if (synchronizeEnsemble)
        {
            setEnsembleMember(sync->ensembleMember());
        }
    }
    else
    {
        // No synchronization. Reset property colours and disable sync
        // checkboxes.
        foreach (MSceneControl* scene, getScenes())
        {
            scene->resetPropertyColour(initTimeProperty);
            scene->resetPropertyColour(startTimeProperty);
            scene->resetPropertyColour(particlePosTimeProperty);
            scene->resetPropertyColour(ensembleSingleMemberProperty);
        }

        if (updateGUIProperties)
        {
            enableActorUpdates(false);
            synchronizeInitTimeProperty->setEnabled(false);
            synchronizeInitTime = false;
            synchronizeStartTimeProperty->setEnabled(false);
            synchronizeStartTime = false;
            synchronizeParticlePosTimeProperty->setEnabled(false);
            synchronizeParticlePosTime = false;
            synchronizeEnsembleProperty->setEnabled(false);
            synchronizeEnsemble = false;
            enableActorUpdates(true);
        }
    }


    // Update "synchronize xyz" GUI properties.
    // ========================================
    if (updateGUIProperties && isInitialized())
    {
        updateTimeProperties();
        updateEnsembleProperties();
    }
}


bool MTrajectoryActor::synchronizationEvent(
        MSynchronizationType syncType, QVector<QVariant> data)
{
    switch (syncType)
    {
    case SYNC_INIT_TIME:
    {
        if (!synchronizeInitTime)
        {
            return false;
        }
        enableActorUpdates(false);
        bool newInitTimeSet = setInitDateTime(data.at(0).toDateTime());
        enableActorUpdates(true);
        if (newInitTimeSet)
        {
            asynchronousDataRequest(true);
        }
        return newInitTimeSet;
    }
    case SYNC_VALID_TIME:
    {
        if (!synchronizeStartTime && !synchronizeParticlePosTime)
        {
            return false;
        }
        enableActorUpdates(false);
        bool newStartTimeSet = false;
        bool newParticleTimeSet = false;
        if (synchronizeStartTime)
        {
            newStartTimeSet = setStartDateTime(data.at(0).toDateTime());
        }
        if (synchronizeParticlePosTime)
        {
            newParticleTimeSet = setParticleDateTime(data.at(0).toDateTime());
        }
        enableActorUpdates(true);
        if (newStartTimeSet || newParticleTimeSet)
        {
            if (!precomputedDataSource)
            {
                asynchronousDataRequest(true);
            }
            else
            {
                return false;
            }
        }
        return (newStartTimeSet || newParticleTimeSet);
    }
    case SYNC_INIT_VALID_TIME:
    {
        enableActorUpdates(false);
        bool newInitTimeSet = false;
        bool newStartTimeSet = false;
        bool newParticleTimeSet = false;
        if (synchronizeInitTime)
        {
            newInitTimeSet = setInitDateTime(data.at(0).toDateTime());
        }
        if (synchronizeStartTime)
        {
            newStartTimeSet = setStartDateTime(data.at(0).toDateTime());
        }
        if (synchronizeParticlePosTime)
        {
            newParticleTimeSet = setParticleDateTime(data.at(0).toDateTime());
        }
        enableActorUpdates(true);
        if (newInitTimeSet || newStartTimeSet || newParticleTimeSet)
        {
            asynchronousDataRequest(true);
        }
        return (newInitTimeSet || newStartTimeSet || newParticleTimeSet);
    }
    case SYNC_ENSEMBLE_MEMBER:
    {
        if (!synchronizeEnsemble)
        {
            return false;
        }
        enableActorUpdates(false);
        bool newEnsembleMemberSet = setEnsembleMember(data.at(0).toInt());
        enableActorUpdates(true);
        if (newEnsembleMemberSet)
        {
            asynchronousDataRequest(true);
        }
        return newEnsembleMemberSet;
    }
    default:
        break;
    }

    return false;
}


void MTrajectoryActor::updateSyncPropertyColourHints(MSceneControl *scene)
{
    if (synchronizationControl == nullptr)
    {
        // No synchronization -- reset all property colours.
        setPropertyColour(initTimeProperty, QColor(), true, scene);
        setPropertyColour(startTimeProperty, QColor(), true, scene);
        setPropertyColour(particlePosTimeProperty, QColor(), true, scene);
        setPropertyColour(ensembleSingleMemberProperty, QColor(), true, scene);
    }
    else
    {
        // ( Also see internalSetDateTime() ).

        // Init time.
        // ==========
        bool match = (getPropertyTime(initTimeProperty)
                      == synchronizationControl->initDateTime());
        QColor colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);
        setPropertyColour(initTimeProperty, colour,
                          !synchronizeInitTime, scene);

        // Valid time.
        // ===========

        match = (getPropertyTime(startTimeProperty)
                 == synchronizationControl->validDateTime());
        colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);
        setPropertyColour(startTimeProperty, colour,
                          !synchronizeStartTime, scene);

        match = (getPropertyTime(particlePosTimeProperty)
                 == synchronizationControl->validDateTime());
        colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);
        setPropertyColour(particlePosTimeProperty, colour,
                          !synchronizeParticlePosTime, scene);

        // Ensemble.
        // =========
        match = (getEnsembleMember()
                 == synchronizationControl->ensembleMember());
        colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);
        setPropertyColour(ensembleSingleMemberProperty, colour,
                          !synchronizeEnsemble, scene);
    }
}


void MTrajectoryActor::setPropertyColour(
        QtProperty* property, const QColor &colour, bool resetColour,
        MSceneControl *scene)
{
    if (resetColour)
    {
        if (scene == nullptr)
        {
            // Reset colour for all scenes in which the actor appears.
            foreach (MSceneControl* sc, getScenes())
            {
                sc->resetPropertyColour(property);
            }
        }
        else
        {
            // Reset colour only for the specifed scene.
            scene->resetPropertyColour(property);
        }
    }
    else
    {
        if (scene == nullptr)
        {
            // Set colour for all scenes in which the actor appears.
            foreach (MSceneControl* sc, getScenes())
            {
                sc->setPropertyColour(property, colour);
            }
        }
        else
        {
            // Set colour only for the specifed scene.
            scene->setPropertyColour(property, colour);
        }
    }
}


void MTrajectoryActor::setDataSource(MTrajectoryDataSource *ds)
{
    if (trajectorySource != nullptr)
    {
        disconnect(trajectorySource, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousDataAvailable(MDataRequest)));
    }

    bool prevTrajectorySourceComputesInMet3D =
            (dynamic_cast<MTrajectoryComputationSource*>(
                 trajectorySource) != nullptr);
    bool newTrajectorySourceComputesInMet3D =
            (dynamic_cast<MTrajectoryComputationSource*>(ds) != nullptr);

    trajectorySource = ds;
    if (trajectorySource != nullptr)
    {
        connect(trajectorySource, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousDataAvailable(MDataRequest)));

        // Check whether this datasource is precomputed.
        precomputedDataSource =
                (dynamic_cast<MTrajectoryComputationSource*>(trajectorySource)
                 == nullptr);
        updateActorData();
    }

    // Connect seed actors change signal to trajectory actor only if the new
    // connected data source is a computation data source. If the data source
    // is NO computation data source, the seed actors change signal should not
    // trigger an update of the trajectory actor, hence disconnect signal.
    if (!prevTrajectorySourceComputesInMet3D && newTrajectorySourceComputesInMet3D)
    {
        for (SeedActorSettings& sas : computationSeedActorProperties)
        {
            connect(sas.actor, SIGNAL(actorChanged()),
                    this, SLOT(onSeedActorChanged()));
        }
    }
    else if (prevTrajectorySourceComputesInMet3D && !newTrajectorySourceComputesInMet3D)
    {
        for (SeedActorSettings& sas : computationSeedActorProperties)
        {
            disconnect(sas.actor, SIGNAL(actorChanged()),
                       this, SLOT(onSeedActorChanged()));
        }
    }
}


void MTrajectoryActor::setDataSource(const QString& id)
{
   MAbstractDataSource* ds =
           MSystemManagerAndControl::getInstance()->getDataSource(id);
   setDataSource(dynamic_cast<MTrajectoryDataSource*>(ds));
}


void MTrajectoryActor::setNormalsSource(MTrajectoryNormalsSource *s)
{
    if (normalsSource != nullptr)
    {
        disconnect(normalsSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousNormalsAvailable(MDataRequest)));
    }

    normalsSource = s;
    if (normalsSource != nullptr)
    {
        connect(normalsSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousNormalsAvailable(MDataRequest)));
    }
}


void MTrajectoryActor::setNormalsSource(const QString& id)
{
    MAbstractDataSource* ds =
            MSystemManagerAndControl::getInstance()->getDataSource(id);
    setNormalsSource(dynamic_cast<MTrajectoryNormalsSource*>(ds));
}


void MTrajectoryActor::setMultiVarTrajectoriesSource(MMultiVarTrajectoriesSource *s)
{
    if (multiVarTrajectoriesSource != nullptr)
    {
        disconnect(multiVarTrajectoriesSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousMultiVarTrajectoriesAvailable(MDataRequest)));
    }

    multiVarTrajectoriesSource = s;
    if (multiVarTrajectoriesSource != nullptr)
    {
        connect(multiVarTrajectoriesSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousMultiVarTrajectoriesAvailable(MDataRequest)));
    }
}


void MTrajectoryActor::setMultiVarTrajectoriesSource(const QString& id)
{
    MAbstractDataSource* ds =
            MSystemManagerAndControl::getInstance()->getDataSource(id);
    setMultiVarTrajectoriesSource(dynamic_cast<MMultiVarTrajectoriesSource *>(ds));
}


void MTrajectoryActor::setTrajectoryFilter(MTrajectoryFilter *f)
{
    if (trajectoryFilter != nullptr)
    {
        disconnect(trajectoryFilter, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousSelectionAvailable(MDataRequest)));
        disconnect(trajectoryFilter, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this,
                   SLOT(asynchronousSingleTimeSelectionAvailable(MDataRequest)));
    }

    trajectoryFilter = f;
    if (trajectoryFilter != nullptr)
    {
        connect(trajectoryFilter, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousSelectionAvailable(MDataRequest)));
        connect(trajectoryFilter, SIGNAL(dataRequestCompleted(MDataRequest)),
                this,
                SLOT(asynchronousSingleTimeSelectionAvailable(MDataRequest)));
    }
}


void MTrajectoryActor::setTrajectoryFilter(const QString& id)
{
    MAbstractDataSource* ds =
            MSystemManagerAndControl::getInstance()->getDataSource(id);
    setTrajectoryFilter(dynamic_cast<MTrajectoryFilter*>(ds));
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

bool MTrajectoryActor::setEnsembleMember(int member)
{
    // Don't change member if no data source is selected.
    if (dataSourceID == "")
    {
        return false;
    }

    int prevEnsembleMode = properties->mEnum()->value(ensembleModeProperty);

    if (member < 0)
    {
        // Ensemble mean: member == -1. As there are no "mean trajectories"
        // the ensemble mean is interpreted as "render all trajectories".

        // If the ensemble mode is already set to "ALL" return false; nothing
        // needs to be done.
        if (prevEnsembleMode == 1) return false;

        // Else set the property.
        properties->mEnum()->setValue(ensembleModeProperty, 1);
    }
    else
    {
#ifdef DIRECT_SYNCHRONIZATION
        int prevEnsembleMember = getEnsembleMember();
#endif
        // Change ensemble member.
        properties->setEnumPropertyClosest<unsigned int>(
                    availableEnsembleMembersAsSortedList, (unsigned int)member,
                    ensembleSingleMemberProperty,
                    synchronizeEnsemble, getScenes());
        properties->mEnum()->setValue(ensembleModeProperty, 0);

#ifdef DIRECT_SYNCHRONIZATION
        // Does a new data request need to be emitted?
        if (prevEnsembleMode == 1) return true;
        if (prevEnsembleMember != member) return true;
        return false;
#endif
    }

    return false;
}


bool MTrajectoryActor::setStartDateTime(const QDateTime& datetime)
{
    return internalSetDateTime(availableStartTimes, datetime,
                               startTimeProperty);
}


bool MTrajectoryActor::setParticleDateTime(const QDateTime& datetime)
{
    return internalSetDateTime(availableParticlePosTimes, datetime,
                               particlePosTimeProperty);
}


bool MTrajectoryActor::setInitDateTime(const QDateTime& datetime)
{
    return internalSetDateTime(availableInitTimes, datetime, initTimeProperty);
}


void MTrajectoryActor::asynchronousDataAvailable(MDataRequest request)
{
    // See NWPActorVariabe::asynchronousDataAvailable() for explanations
    // on request queue handling.
    // Iterate over all trajectory requests.
    for (int t = 0; t < trajectoryRequests.size(); t++)
    {
        bool queueContainsEntryWithNoPendingRequests = false;
        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size();
             i++)
        {
            if (trajectoryRequests[t].pendingRequestsQueue[i]
                    .dataRequest.request == request)
            {
                // If this is the first time we are informed about the availability
                // of the request (available still == false) decrease number of
                // pending requests.
                if ( !trajectoryRequests[t].pendingRequestsQueue[i]
                     .dataRequest.available )
                {
                    trajectoryRequests[t].pendingRequestsQueue[i]
                            .numPendingRequests--;
                }

                trajectoryRequests[t].pendingRequestsQueue[i]
                        .dataRequest.available = true;

                if (trajectoryRequests[t].pendingRequestsQueue[i]
                        .numPendingRequests == 0)
                {
                    queueContainsEntryWithNoPendingRequests = true;
                }

                // Do NOT break the loop here; "request" might be relevant to
                // multiple entries in the queue.
            }
        }

        if (queueContainsEntryWithNoPendingRequests)
        {
            prepareAvailableDataForRendering(t);
        }
    }
}


void MTrajectoryActor::asynchronousNormalsAvailable(MDataRequest request)
{
    for (int t = 0; t < trajectoryRequests.size(); t++)
    {
        bool queueContainsEntryWithNoPendingRequests = false;
        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size();
             i++)
        {
            foreach (MSceneViewGLWidget *view,
                     trajectoryRequests[t].pendingRequestsQueue[i]
                     .normalsRequests.keys())
            {
                if (trajectoryRequests[t].pendingRequestsQueue[i]
                        .normalsRequests[view].request == request)
                {
                    if ( !trajectoryRequests[t].pendingRequestsQueue[i]
                         .normalsRequests[view].available )
                    {
                        trajectoryRequests[t].pendingRequestsQueue[i]
                                .numPendingRequests--;
                    }

                    trajectoryRequests[t].pendingRequestsQueue[i]
                            .normalsRequests[view].available = true;

                    if (trajectoryRequests[t].pendingRequestsQueue[i]
                            .numPendingRequests == 0)
                    {
                        queueContainsEntryWithNoPendingRequests = true;
                    }

                    // Do NOT break the loop here; "request" might be relevant to
                    // multiple entries in the queue.
                }
            }
        }

        if (queueContainsEntryWithNoPendingRequests)
        {
            prepareAvailableDataForRendering(t);
        }
    }
}


void MTrajectoryActor::asynchronousMultiVarTrajectoriesAvailable(MDataRequest request)
{
    for (int t = 0; t < trajectoryRequests.size(); t++)
    {
        bool queueContainsEntryWithNoPendingRequests = false;
        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size();
             i++)
        {
                foreach (MSceneViewGLWidget *view,
                         trajectoryRequests[t].pendingRequestsQueue[i]
                                 .multiVarTrajectoriesRequests.keys())
                {
                    if (trajectoryRequests[t].pendingRequestsQueue[i]
                                .multiVarTrajectoriesRequests[view].request == request)
                    {
                        if ( !trajectoryRequests[t].pendingRequestsQueue[i]
                                .multiVarTrajectoriesRequests[view].available )
                        {
                            trajectoryRequests[t].pendingRequestsQueue[i]
                                    .numPendingRequests--;
                        }

                        trajectoryRequests[t].pendingRequestsQueue[i]
                                .multiVarTrajectoriesRequests[view].available = true;

                        if (trajectoryRequests[t].pendingRequestsQueue[i]
                                    .numPendingRequests == 0)
                        {
                            queueContainsEntryWithNoPendingRequests = true;
                        }

                        // Do NOT break the loop here; "request" might be relevant to
                        // multiple entries in the queue.
                    }
                }
        }

        if (queueContainsEntryWithNoPendingRequests)
        {
            prepareAvailableDataForRendering(t);
            if (loadedParticlePosTimeStepFromSettings)
            {
                particlePosTimeStep = cachedParticlePosTimeStep;
                properties->mEnum()->setValue(particlePosTimeProperty, particlePosTimeStep);
            }
            loadedParticlePosTimeStepFromSettings = false;
        }
    }
}


void MTrajectoryActor::asynchronousSelectionAvailable(MDataRequest request)
{
    for (int t = 0; t < trajectoryRequests.size(); t++)
    {
        bool queueContainsEntryWithNoPendingRequests = false;
        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size();
             i++)
        {
            if (trajectoryRequests[t].pendingRequestsQueue[i].filterRequest
                    .request == request)
            {
                if (!trajectoryRequests[t].pendingRequestsQueue[i]
                        .filterRequest.available)
                {
                    trajectoryRequests[t].pendingRequestsQueue[i]
                            .numPendingRequests--;
                }

                trajectoryRequests[t].pendingRequestsQueue[i]
                        .filterRequest.available = true;

                if (trajectoryRequests[t].pendingRequestsQueue[i]
                        .numPendingRequests == 0)
                {
                    queueContainsEntryWithNoPendingRequests = true;
                }

                // Do NOT break the loop here; "request" might be relevant to
                // multiple entries in the queue.
            }
        }

        if (queueContainsEntryWithNoPendingRequests)
        {
            prepareAvailableDataForRendering(t);
        }
    }
}


void MTrajectoryActor::asynchronousSingleTimeSelectionAvailable(MDataRequest request)
{
    for (int t = 0; t < trajectoryRequests.size(); t++)
    {
        bool queueContainsEntryWithNoPendingRequests = false;
        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size();
             i++)
        {
            if (trajectoryRequests[t].pendingRequestsQueue[i]
                    .singleTimeFilterRequest.request == request)
            {
                if (!trajectoryRequests[t].pendingRequestsQueue[i]
                        .singleTimeFilterRequest.available)
                {
                    trajectoryRequests[t].pendingRequestsQueue[i]
                            .numPendingRequests--;
                }

                trajectoryRequests[t].pendingRequestsQueue[i]
                        .singleTimeFilterRequest.available = true;

                if (trajectoryRequests[t].pendingRequestsQueue[i]
                        .numPendingRequests == 0)
                {
                    queueContainsEntryWithNoPendingRequests = true;
                }

                // Do NOT break the loop here; "request" might be relevant to
                // multiple entries in the queue.
            }
        }

        if (queueContainsEntryWithNoPendingRequests)
        {
            prepareAvailableDataForRendering(t);
        }
    }
}


void MTrajectoryActor::prepareAvailableDataForRendering(uint slot)
{
    // Prepare datafields for rendering as long as they are available in
    // the order in which they were requested.
    while ( ( !trajectoryRequests[slot].pendingRequestsQueue.isEmpty() )
            && ( trajectoryRequests[slot].pendingRequestsQueue.head()
                 .numPendingRequests == 0 ) )
    {
        MTrajectoryRequestQueueInfo trqi = trajectoryRequests[slot]
                .pendingRequestsQueue.dequeue();

        // 1. Trajectory data.
        // ===================

        if (trqi.dataRequest.available)
        {
            // Release current and get new trajectories.
            if (trajectoryRequests[slot].trajectories)
            {
                trajectoryRequests[slot].trajectories->releaseVertexBuffer();
                trajectoryRequests[slot].releasePreviousAuxVertexBuffer();

                trajectorySource->releaseData(trajectoryRequests[slot]
                                              .trajectories);
            }
            trajectoryRequests[slot].trajectories = trajectorySource->getData(
                        trqi.dataRequest.request);

            // Get vertex buffer for trajectory data.
            trajectoryRequests[slot].trajectoriesVertexBuffer =
                    trajectoryRequests[slot].trajectories->getVertexBuffer();

            // Get vertex buffer for auxiliary data along trajectories.
            trajectoryRequests[slot].requestAuxVertexBuffer(
                        properties->getEnumItem(renderAuxDataVarProperty), multiVarData.getOutputParameterName());

            // Update displayed information about timestep length.
            float timeStepLength_hours = trajectoryRequests[slot].trajectories
                    ->getTimeStepLength_sec() / 3600.;

            properties->mDDouble()->setSingleStep(
                        deltaTimeFilterProperty, timeStepLength_hours);
            properties->mDDouble()->setRange(
                        deltaTimeFilterProperty, timeStepLength_hours,
                        (trajectoryRequests[slot].trajectories
                         ->getNumTimeStepsPerTrajectory() - 1)
                        * timeStepLength_hours);

            updateParticlePosTimeProperty();
        }

        // 2. Normals.
        // ===========

        foreach (MSceneViewGLWidget *view, trqi.normalsRequests.keys())
        {
            if (trqi.normalsRequests[view].available)
            {
                if (trajectoryRequests[slot].normals.value(view, nullptr))
                {
                    trajectoryRequests[slot].normals[view]->releaseVertexBuffer();
                    normalsSource->releaseData(trajectoryRequests[slot].normals[view]);
                }
                trajectoryRequests[slot].normals[view] = normalsSource->getData(
                            trqi.normalsRequests[view].request);
                trajectoryRequests[slot].normalsVertexBuffer[view] =
                        trajectoryRequests[slot].normals[view]->getVertexBuffer();
            }
        }

        // 3. Selection.
        // =============

        if (trqi.filterRequest.available)
        {
            if (trajectoryRequests[slot].trajectorySelection)
            {
                trajectoryFilter->releaseData(trajectoryRequests[slot]
                                              .trajectorySelection);
            }
            trajectoryRequests[slot].trajectorySelection =
                    trajectoryFilter->getData(trqi.filterRequest.request);
        }

        // 4. Single time selection.
        // =========================

        if (trqi.singleTimeFilterRequest.available)
        {
            if (trajectoryRequests[slot].trajectorySingleTimeSelection)
            {
                trajectoryFilter->releaseData(trajectoryRequests[slot]
                                              .trajectorySingleTimeSelection);
            }
            trajectoryRequests[slot].trajectorySingleTimeSelection =
                    trajectoryFilter->getData(trqi.singleTimeFilterRequest.request);
        }

        // 5. Multi-var trajectories.
        // =======================

        if (useMultiVarTrajectories)
        {
            if (trqi.dataRequest.available)
            {
                multiVarData.onMultiVarTrajectoriesLoaded(trajectoryRequests[slot].trajectories);
                multiVarData.clearVariableRanges();
            }

            foreach (MSceneViewGLWidget *view, trqi.multiVarTrajectoriesRequests.keys())
            {
                if (trqi.multiVarTrajectoriesRequests[view].available)
                {
                    // TODO: Set output parameter id here?
                    if (trajectoryRequests[slot].multiVarTrajectoriesMap.value(view, nullptr))
                    {
                        trajectoryRequests[slot].multiVarTrajectoriesMap[view]->releaseRenderData();
                        multiVarTrajectoriesSource->releaseData(
                                trajectoryRequests[slot].multiVarTrajectoriesMap[view]);
                    }
                    trajectoryRequests[slot].multiVarTrajectoriesMap[view] = multiVarTrajectoriesSource->getData(
                            trqi.multiVarTrajectoriesRequests[view].request);
                    trajectoryRequests[slot].multiVarTrajectoriesRenderDataMap[view] =
                            trajectoryRequests[slot].multiVarTrajectoriesMap[view]->getRenderData();
                    trajectoryRequests[slot].timeStepSphereRenderDataMap[view] =
                            trajectoryRequests[slot].multiVarTrajectoriesMap[view]->getTimeStepSphereRenderData();
                    trajectoryRequests[slot].timeStepRollsRenderDataMap[view] =
                            trajectoryRequests[slot].multiVarTrajectoriesMap[view]->getTimeStepRollsRenderData();
                    trajectoryRequests[slot].multiVarTrajectoriesMap[view]->setDirty(true);

#ifdef USE_EMBREE
                    if (trajectoryPickerMap.find(view) == trajectoryPickerMap.end()) {
                        GLuint textureUnit = assignTextureUnit();
                        trajectoryPickerMap[view] = new MTrajectoryPicker(
                                textureUnit, view, multiVarData.getVarNames(),
                                multiVarData.getTransferFunctionsMultiVar(),
                                diagramType, diagramTransferFunction);
                        multiVarData.setDiagramType(diagramType);
                        trajectoryPickerMap[view]->setSelectedVariableIndices(multiVarData.getSelectedVariableIndices());
                        trajectoryPickerMap[view]->updateSectedOutputParameter(
                                multiVarData.getOutputParameterName(), multiVarData.getOutputParameterIdx());
                        trajectoryPickerMap[view]->updateTrajectoryRadius(tubeRadius);
                        trajectoryPickerMap[view]->setSyncMode(trajectorySyncMode);
                        trajectoryPickerMap[view]->setParticlePosTimeStep(particlePosTimeStep);
                        trajectoryPickerMap[view]->setDiagramType(diagramType);
                        trajectoryPickerMap[view]->setSimilarityMetric(similarityMetric);
                        trajectoryPickerMap[view]->setMeanMetricInfluence(meanMetricInfluence);
                        trajectoryPickerMap[view]->setStdDevMetricInfluence(stdDevMetricInfluence);
                        trajectoryPickerMap[view]->setNumBins(numBins);
                        trajectoryPickerMap[view]->setShowMinMaxValue(showMinMaxValue);
                        trajectoryPickerMap[view]->setUseMaxForSensitivity(useMaxForSensitivity);
                        trajectoryPickerMap[view]->setUseVariableToolTip(useVariableToolTip);
                        trajectoryPickerMap[view]->setTrimNanRegions(trimNanRegions);
                        trajectoryPickerMap[view]->setSubsequenceMatchingTechnique(subsequenceMatchingTechnique);
                        trajectoryPickerMap[view]->setSpringEpsilon(springEpsilon);
                        trajectoryPickerMap[view]->setBackgroundOpacity(backgroundOpacity);
                        trajectoryPickerMap[view]->setDiagramNormalizationMode(diagramNormalizationMode);
                        trajectoryPickerMap[view]->setTextSize(diagramTextSize);
                        if (useCustomDiagramUpscalingFactor) {
                            trajectoryPickerMap[view]->setDiagramUpscalingFactor(diagramUpscalingFactor);
                        }
                    }

                    trajectoryPickerMap[view]->setBaseTrajectories(
                            trajectoryRequests[slot].multiVarTrajectoriesMap[view]->getBaseTrajectories());
                    const QVector<QVector2D>& variableRanges = trajectoryPickerMap[view]->getVariableRanges();
                    multiVarData.updateVariableRanges(variableRanges);
#endif
                }
            }

            if (trqi.filterRequest.available || trqi.singleTimeFilterRequest.available)
            {
                multiVarDataDirty = true;
            }
        }


#ifdef DIRECT_SYNCHRONIZATION
        // If this was a synchronization request, check if the request was
        // the last request due to the sync sent by any seed actor -- if
        // yes, signal to the sync control that the sync has been completed.
        if (trqi.syncchronizationRequest)
        {
            numSeedActorsForRequestEvent[trqi.requestID] -= 1;
            if (numSeedActorsForRequestEvent[trqi.requestID] == 0)
            {
                synchronizationControl->synchronizationCompleted(this);
            }
        }
#endif
    }

    // Special case:
    // Since the particle position times depend on the data requested,
    // we need to call asynchronousSelectionRequest() after data request is
    // complete when requesting data from a data source for the first time.
    if (initialDataRequest)
    {
        initialDataRequest = false;
        asynchronousSelectionRequest();
    }
    else
    {
        emitActorChangedSignal();
        updateSyncPropertyColourHints();
    }
}


void MTrajectoryActor::onActorCreated(MActor *actor)
{
    // If the new actor is a transfer function, add it to the list of
    // available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        // Don't render while the properties are being updated.
        enableEmissionOfActorChangedSignal(false);

        int index = properties->mEnum()->value(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                transferFunctionProperty);
        availableTFs << tf->transferFunctionName();
        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);

        int indexDiagram = properties->mEnum()->value(diagramTransferFunctionProperty);
        QStringList availableTFsDiagram = properties->mEnum()->enumNames(
                diagramTransferFunctionProperty);
        availableTFs << tf->transferFunctionName();
        properties->mEnum()->setEnumNames(diagramTransferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(diagramTransferFunctionProperty, indexDiagram);

        enableEmissionOfActorChangedSignal(true);
    }

    if (useMultiVarTrajectories)
    {
        multiVarData.onActorCreated(actor);
    }
}


void MTrajectoryActor::onActorDeleted(MActor *actor)
{
    // If the deleted actor is a transfer function, remove it from the list of
    // available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        enableEmissionOfActorChangedSignal(false);

        QString tFName = properties->getEnumItem(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(transferFunctionProperty);

        availableTFs.removeOne(tf->getName());

        // Get the current index of the transfer function selected. If the
        // transfer function is the one to be deleted, the selection is set to
        // 'None'.
        int index = availableTFs.indexOf(tFName);

        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);


        QString tFNameDiagram = properties->getEnumItem(diagramTransferFunctionProperty);
        QStringList availableTFsDiagram = properties->mEnum()->enumNames(diagramTransferFunctionProperty);

        availableTFsDiagram.removeOne(tf->getName());

        int indexDiagram = availableTFsDiagram.indexOf(tFNameDiagram);

        properties->mEnum()->setEnumNames(diagramTransferFunctionProperty, availableTFsDiagram);
        properties->mEnum()->setValue(diagramTransferFunctionProperty, indexDiagram);

        enableEmissionOfActorChangedSignal(true);
    }

    // Check whether the actor is in our seeding actors list.
    else
    {
        for (SeedActorSettings& sas : computationSeedActorProperties)
        {
            if (sas.actor == actor)
            {
                removeSeedActor(sas.actor->getName());

                releaseData();
                updateActorData();

                asynchronousDataRequest();
                emitActorChangedSignal();
            }
        }
    }

    if (useMultiVarTrajectories)
    {
        multiVarData.onActorDeleted(actor);
    }
}


void MTrajectoryActor::onActorRenamed(MActor *actor, QString oldName)
{
    // If the renamed actor is a transfer function, change its name in the list
    // of available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        // Don't render while the properties are being updated.
        enableEmissionOfActorChangedSignal(false);

        int index = properties->mEnum()->value(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(transferFunctionProperty);

        // Replace affected entry.
        availableTFs[availableTFs.indexOf(oldName)] = tf->getName();

        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);


        int indexDiagram = properties->mEnum()->value(diagramTransferFunctionProperty);
        QStringList availableTFsDiagram = properties->mEnum()->enumNames(diagramTransferFunctionProperty);

        // Replace affected entry.
        availableTFsDiagram[availableTFsDiagram.indexOf(oldName)] = tf->getName();

        properties->mEnum()->setEnumNames(diagramTransferFunctionProperty, availableTFsDiagram);
        properties->mEnum()->setValue(diagramTransferFunctionProperty, indexDiagram);

        enableEmissionOfActorChangedSignal(true);
    }

    // Check whether the actor is in our seeding actors list.
    else
    {
        for (SeedActorSettings& sas : computationSeedActorProperties)
        {
            if (sas.actor == actor)
            {
                sas.propertyGroup->setPropertyName(sas.actor->getName());
            }
        }
    }

    if (useMultiVarTrajectories)
    {
        multiVarData.onActorRenamed(actor, oldName);
    }
}


void MTrajectoryActor::registerScene(MSceneControl *scene)
{
    MActor::registerScene(scene);
    // Only send data request if data source exists, but for each scene since
    // they have different scene views and thus different normals.
    if (dataSourceID != "")
    {
        asynchronousDataRequest();
    }
}


void MTrajectoryActor::onSceneViewAdded()
{
    // Only send data request if data source exists, but for each scene since
    // they have different scene views and thus different normals.
    if (dataSourceID != "")
    {
        asynchronousDataRequest();
    }
}


bool MTrajectoryActor::isConnectedTo(MActor *actor)
{
    if (MActor::isConnectedTo(actor))
    {
        return true;
    }

    // This actor is connected to the argument actor if the argument actor is
    // the transfer function of this actor.
    if (transferFunction == actor)
    {
        return true;
    }

    // This actor is connected to the argument actor if the argument actor is
    // a seeding actor of this actor.
    for (SeedActorSettings& sas : computationSeedActorProperties)
    {
        if (sas.actor == actor)
        {
            return true;
        }
    }

    return false;
}


void MTrajectoryActor::onSeedActorChanged()
{
    if (suppressActorUpdates()) return;

    releaseData();
    updateActorData();

    asynchronousDataRequest();
}


#ifdef USE_EMBREE
bool MTrajectoryActor::checkIntersectionWithSelectableData(
        MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (!useMultiVarTrajectories || trajectoryPickerMap.find(sceneView) == trajectoryPickerMap.end())
    {
        return false;
    }

    QVector3D firstHitPoint{};
    uint32_t trajectoryIndex = 0;
    float timeAtHit = 0.0f;
    if (trajectoryPickerMap[sceneView]->pickPointScreen(
            sceneView, event->x(), event->y(),
            firstHitPoint, trajectoryIndex, timeAtHit))
    {
        bool changed = false;
        if (event->button() == Qt::LeftButton && event->type() == QInputEvent::MouseButtonRelease)
        {
            trajectoryPickerMap[sceneView]->toggleTrajectoryHighlighted(trajectoryIndex);
            changed = true;
        }
        if (event->button() == Qt::RightButton || event->buttons() == Qt::MouseButton::RightButton)
        {
            if (synchronizeParticlePosTime)
            {
                synchronizeParticlePosTime = false;
                properties->mBool()->setValue(
                        synchronizeParticlePosTimeProperty,
                        synchronizeParticlePosTime);
                updateTimeProperties();
            }

            particlePosTimeStep = int(std::round(timeAtHit));
            syncModeTrajectoryIndex = trajectoryIndex;
            trajectoryPickerMap[sceneView]->setParticlePosTimeStep(particlePosTimeStep);
            changed = true;
        }
        return changed;
    }
    return false;
}

bool MTrajectoryActor::checkVirtualWindowBelowMouse(
        MSceneViewGLWidget *sceneView, int mousePositionX, int mousePositionY)
{
    if (!useMultiVarTrajectories || trajectoryPickerMap.find(sceneView) == trajectoryPickerMap.end())
    {
        return false;
    }
    return trajectoryPickerMap[sceneView]->checkVirtualWindowBelowMouse(sceneView, mousePositionX, mousePositionY);
}

void MTrajectoryActor::mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (!useMultiVarTrajectories || trajectoryPickerMap.find(sceneView) == trajectoryPickerMap.end())
    {
        return;
    }
    return trajectoryPickerMap[sceneView]->mouseMoveEvent(sceneView, event);
}

void MTrajectoryActor::mouseMoveEventParent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (!useMultiVarTrajectories || trajectoryPickerMap.find(sceneView) == trajectoryPickerMap.end())
    {
        return;
    }
    return trajectoryPickerMap[sceneView]->mouseMoveEventParent(sceneView, event);
}

void MTrajectoryActor::mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (!useMultiVarTrajectories || trajectoryPickerMap.find(sceneView) == trajectoryPickerMap.end())
    {
        return;
    }
    return trajectoryPickerMap[sceneView]->mousePressEvent(sceneView, event);
}

void MTrajectoryActor::mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (!useMultiVarTrajectories || trajectoryPickerMap.find(sceneView) == trajectoryPickerMap.end())
    {
        return;
    }
    return trajectoryPickerMap[sceneView]->mouseReleaseEvent(sceneView, event);
}

void MTrajectoryActor::wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event)
{
    if (!useMultiVarTrajectories || trajectoryPickerMap.find(sceneView) == trajectoryPickerMap.end())
    {
        return;
    }
    return trajectoryPickerMap[sceneView]->wheelEvent(sceneView, event);
}
#endif


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTrajectoryActor::initializeActorResources()
{
    // Initialise texture unit.
    if (textureUnitTransferFunction >= 0)
    {
        releaseTextureUnit(textureUnitTransferFunction);
    }
    textureUnitTransferFunction = assignImageUnit();

    // Since no data source was selected disable actor properties since they
    // have no use without a data source.
    if (dataSourceID == "" && !selectDataSource())
    {
        // TODO (bt, May2017): Why does the program crash when calling a message
        // boxes or dialogs during initialisation of GL?
        bool appInitialized =
                MSystemManagerAndControl::getInstance()->applicationIsInitialized();
        if (appInitialized)
        {
            // User has selected no data source. Display a warning and disable
            // all trajectory properties.
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setWindowTitle(getName());
            msgBox.setText("No data source selected. Disabling all properties.");
            msgBox.exec();
        }
        enableProperties(false);
    }
    else
    {
        enableProperties(true);

        updateInitTimeProperty();
        updateStartTimeProperty();
        updateEnsembleSingleMemberProperty();
        updateAuxDataVarNamesProperty();

        // Get values from sync control, if connected to one.
        if (synchronizationControl == nullptr)
        {
            synchronizeInitTimeProperty->setEnabled(false);
            synchronizeInitTime = false;
            synchronizeStartTimeProperty->setEnabled(false);
            synchronizeStartTime = false;
            synchronizeParticlePosTimeProperty->setEnabled(false);
            synchronizeParticlePosTime = false;
            synchronizeEnsembleProperty->setEnabled(false);
            synchronizeEnsemble = false;
        }
        else
        {
            // Set synchronizationControl to nullptr and sync with its
            // original value since otherwise the synchronisation won't
            // be executed.
            MSyncControl *temp = synchronizationControl;
            synchronizationControl = nullptr;
            synchronizeWith(temp);
        }

        updateActorData();

        asynchronousDataRequest();
    }

    // Load shader program if the returned program is new.
    bool loadShaders = false;
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    loadShaders |= glRM->generateEffectProgram("trajectory_tube",
                                               tubeShader);
    loadShaders |= glRM->generateEffectProgram("trajectory_tubeshadow",
                                               tubeShadowShader);
    loadShaders |= glRM->generateEffectProgram("trajectory_spheres",
                                               positionSphereShader);
    loadShaders |= glRM->generateEffectProgram("trajectory_spheresshadow",
                                               positionSphereShadowShader);

    if (loadShaders) reloadShaderEffects();
}


void MTrajectoryActor::updateSimilarityMetricGroupEnabled()
{
    similarityMetricGroup->setEnabled(diagramType == DiagramDisplayType::CURVE_PLOT_VIEW);
    numBinsProperty->setEnabled(similarityMetric == SimilarityMetric::MI);
    sortByDescendingStdDevProperty->setEnabled(diagramType == DiagramDisplayType::CURVE_PLOT_VIEW);
    showMinMaxValueProperty->setEnabled(diagramType == DiagramDisplayType::CURVE_PLOT_VIEW);
    useMaxForSensitivityProperty->setEnabled(diagramType == DiagramDisplayType::CURVE_PLOT_VIEW);
}


void MTrajectoryActor::onQtPropertyChanged(QtProperty *property)
{
    if (property == selectDataSourceProperty)
    {
        if (suppressActorUpdates()) return;

        if (selectDataSource())
        {
            enableProperties(true);
            updateInitTimeProperty();
            updateStartTimeProperty();
            updateEnsembleSingleMemberProperty();
            updateAuxDataVarNamesProperty();
            // Synchronise with synchronisation control if available.
            if (synchronizationControl != nullptr)
            {
                // Set synchronizationControl to nullptr and sync with its
                // original value since otherwise the synchronisation won't
                // be executed.
                MSyncControl *temp = synchronizationControl;
                synchronizationControl = nullptr;
                synchronizeWith(temp);
            }
            asynchronousDataRequest();
        }
    }

    else if (property == utilizedDataSourceProperty)
    {
        dataSourceID = properties->mString()->value(utilizedDataSourceProperty);
        bool validDataSource =
                MSelectDataSourceDialog::checkForTrajectoryDataSource(
                    dataSourceID);
        if (!validDataSource)
        {
            dataSourceID = "";
        }
        else
        {
            initialDataRequest = true;
        }
        return;
    }

    // Connect to the time signals of the selected scene.
    else if (property == synchronizationProperty)
    {
        if (suppressActorUpdates()) return;

        MSystemManagerAndControl *sysMC =
                MSystemManagerAndControl::getInstance();
        QString syncID = properties->getEnumItem(synchronizationProperty);
        synchronizeWith(sysMC->getSyncControl(syncID));

        return;
    }

    else if (property == synchronizeInitTimeProperty)
    {
        synchronizeInitTime = properties->mBool()->value(
                    synchronizeInitTimeProperty);
        updateTimeProperties();

        if (suppressActorUpdates()) return;

        if (synchronizeInitTime && synchronizationControl != nullptr)
        {
            if (setInitDateTime(synchronizationControl->initDateTime()))
            {
                asynchronousDataRequest();
            }
        }

        return;
    }

    else if (property == synchronizeStartTimeProperty)
    {
        synchronizeStartTime = properties->mBool()->value(
                    synchronizeStartTimeProperty);
        updateTimeProperties();

        if (suppressActorUpdates()) return;

        if (synchronizeStartTime && synchronizationControl != nullptr)
        {
            if (setStartDateTime(synchronizationControl->validDateTime()))
            {
                asynchronousDataRequest();
            }
        }

        return;
    }

    else if (property == synchronizeParticlePosTimeProperty)
    {
        synchronizeParticlePosTime = properties->mBool()->value(
                    synchronizeParticlePosTimeProperty);
        updateTimeProperties();

        if (suppressActorUpdates()) return;

        if (synchronizeParticlePosTime && synchronizationControl != nullptr)
        {
            if (setParticleDateTime(synchronizationControl->validDateTime()))
            {
                if (!precomputedDataSource)
                {
                    asynchronousDataRequest();
                }
            }

            if (precomputedDataSource)
            {
                emitActorChangedSignal();
            }
        }

        return;
    }

    else if (property == synchronizeEnsembleProperty)
    {
        synchronizeEnsemble = properties->mBool()->value(
                    synchronizeEnsembleProperty);
        updateEnsembleProperties();
        updateSyncPropertyColourHints();

        if (suppressActorUpdates()) return;

        if (synchronizeEnsemble && synchronizationControl != nullptr)
        {
            if (setEnsembleMember(synchronizationControl->ensembleMember()))
            {
                asynchronousDataRequest();
            }
        }

        return;
    }

    else if (property == ensembleSingleMemberProperty)
    {
//        updateSyncPropertyColourHints();
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == enableAscentFilterProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == deltaPressureFilterProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == deltaTimeFilterProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == renderModeProperty)
    {
        renderMode = TrajectoryRenderType(
                    properties->mEnum()->value(renderModeProperty));

        // The trajectory time property is not needed when the entire
        // trajectories are rendered.
        switch (renderMode)
        {
        case TRAJECTORY_TUBES:
        case ALL_POSITION_SPHERES:
            particlePosTimeProperty->setEnabled(false);
            break;
        case SINGLETIME_POSITIONS:
        case TUBES_AND_SINGLETIME:
        case BACKWARDTUBES_AND_SINGLETIME:
            particlePosTimeProperty->setEnabled(!synchronizeParticlePosTime);
            break;
        }

        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == renderColorModeProperty)
    {
        // Get coloring mode.
        renderColorMode = TrajectoryRenderColorMode(
                    properties->mEnum()->value(renderColorModeProperty));

        if (renderColorMode == PRESSURE)
        {
            renderAuxDataVarProperty->setEnabled(false);
            // Allow colouring of shadows.
            colourShadowProperty->setEnabled(true);   
        }
        else if (renderColorMode == AUXDATA)
        {
            renderAuxDataVarProperty->setEnabled(true);
            // Disable the coloring of shadows because it
            // is not implemented for aux.-data vars.
            colourShadowProperty->setEnabled(false);
        }

        // Update aux data vertex buffers.
        if (suppressActorUpdates()) return;
        QString auxVariableName = properties->getEnumItem(
                    renderAuxDataVarProperty);
        QString auxOutputParameterName = multiVarData.getOutputParameterName();
        for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
        {
            // (for all seed actors or for single precomputed trajectories..)
            trajectoryRequests[t].releasePreviousAuxVertexBuffer();
            trajectoryRequests[t].requestAuxVertexBuffer(auxVariableName, auxOutputParameterName);
            // will not do anything for empty requestedAuxDataVarName
        }
        emitActorChangedSignal();
    }

    else if (property == renderAuxDataVarProperty)
    {
        // Update aux data vertex buffers.
        if (suppressActorUpdates()) return;
        QString auxVariableName = properties->getEnumItem(
                    renderAuxDataVarProperty);
        QString auxOutputParameterName = multiVarData.getOutputParameterName();
        for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
        {
            // (for all seed actors or for single precomputed trajectories..)
            trajectoryRequests[t].releasePreviousAuxVertexBuffer();
            trajectoryRequests[t].requestAuxVertexBuffer(auxVariableName, auxOutputParameterName);
            // will not do anything for empty requestedAuxDataVarName
        }
        emitActorChangedSignal();
    }

    else if (property == transferFunctionProperty)
    {
        setTransferFunctionFromProperty();
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else if (property == diagramTransferFunctionProperty)
    {
        setDiagramTransferFunctionFromProperty();
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else if (property == tubeRadiusProperty)
    {
        tubeRadius = properties->mDDouble()->value(tubeRadiusProperty);
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->updateTrajectoryRadius(tubeRadius);
        }
#endif
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else if (property == sphereRadiusProperty)
    {
        sphereRadius = properties->mDDouble()->value(sphereRadiusProperty);
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else if (property == enableShadowProperty)
    {
        shadowEnabled = properties->mBool()->value(enableShadowProperty);
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else if (property == colourShadowProperty)
    {
        shadowColoured = properties->mBool()->value(colourShadowProperty);
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    // The init time has been changed. Reload start times.
    else if (property == initTimeProperty)
    {
        updateStartTimeProperty();
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == startTimeProperty)
    {
        updateTrajectoryIntegrationTimeProperty();
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == particlePosTimeProperty)
    {
        particlePosTimeStep = properties->mEnum()->value(particlePosTimeProperty);

        if (suppressActorUpdates()) return;
        if (useMultiVarTrajectories) {
            emitActorChangedSignal();
        } else {
            asynchronousSelectionRequest();
        }
    }

    else if (property == computationLineTypeProperty)
    {
        TRAJECTORY_COMPUTATION_LINE_TYPE lineType =
                TRAJECTORY_COMPUTATION_LINE_TYPE(
                    properties->mEnum()->value(computationLineTypeProperty));

        // Enable/disable associated properties.
        enableActorUpdates(false);
        switch (lineType)
        {
            case PATH_LINE:
                computationNumSubTimeStepsProperty->setEnabled(true);
                computationIntegrationTimeProperty->setEnabled(true);
                computationStreamlineDeltaSProperty->setEnabled(false);
                computationStreamlineLengthProperty->setEnabled(false);
                break;
            case STREAM_LINE:
                computationNumSubTimeStepsProperty->setEnabled(false);
                computationIntegrationTimeProperty->setEnabled(false);
                computationStreamlineDeltaSProperty->setEnabled(true);
                computationStreamlineLengthProperty->setEnabled(true);
                break;
        }
        enableActorUpdates(true);

        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == computationIntegrationTimeProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if ( (property == computationStreamlineDeltaSProperty)    ||
              (property == computationStreamlineLengthProperty)    ||
              (property == computationInterpolationMethodProperty) ||
              (property == computationIntegrationMethodProperty)   ||
              (property == computationNumSubTimeStepsProperty)        )
    {
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == computationSeedAddActorProperty)
    {
        if (suppressActorUpdates()) return;
        openSeedActorDialog();
        onSeedActorChanged();
    }

    else if (property == computationRecomputeProperty)
    {
        onSeedActorChanged();
    }

    else if (property == outputAsLagrantoASCIIProperty)
    {
        outputAsLagrantoASCIIFile();
    }

    else if (property == useMultiVarTrajectoriesProperty)
    {
        useMultiVarTrajectories = properties->mBool()->value(useMultiVarTrajectoriesProperty);
        multiVarData.setEnabled(useMultiVarTrajectories);
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
        emitActorChangedSignal();
    }
    else if (property == diagramTypeProperty)
    {
        diagramType = static_cast<DiagramDisplayType>(properties->mEnum()->value(diagramTypeProperty));
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setDiagramType(diagramType);
        }
        multiVarData.setDiagramType(diagramType);
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == similarityMetricProperty)
    {
        similarityMetric = static_cast<SimilarityMetric>(properties->mEnum()->value(similarityMetricProperty));
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setSimilarityMetric(similarityMetric);
        }
        updateSimilarityMetricGroupEnabled();
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == meanMetricInfluenceProperty)
    {
        meanMetricInfluence = float(properties->mDDouble()->value(meanMetricInfluenceProperty));
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setMeanMetricInfluence(meanMetricInfluence);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == stdDevMetricInfluenceProperty)
    {
        stdDevMetricInfluence = float(properties->mDDouble()->value(stdDevMetricInfluenceProperty));
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setStdDevMetricInfluence(stdDevMetricInfluence);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == subsequenceMatchingTechniqueProperty)
    {
        subsequenceMatchingTechnique = static_cast<SubsequenceMatchingTechnique>(
                properties->mEnum()->value(subsequenceMatchingTechniqueProperty));
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setSubsequenceMatchingTechnique(subsequenceMatchingTechnique);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == springEpsilonProperty)
    {
        springEpsilon = float(properties->mDDouble()->value(springEpsilonProperty));
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setSpringEpsilon(springEpsilon);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == backgroundOpacityProperty)
    {
        backgroundOpacity = float(properties->mDDouble()->value(backgroundOpacityProperty));
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setBackgroundOpacity(backgroundOpacity);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == diagramNormalizationModeProperty)
    {
        diagramNormalizationMode = static_cast<DiagramNormalizationMode>(properties->mEnum()->value(
                diagramNormalizationModeProperty));
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setDiagramNormalizationMode(diagramNormalizationMode);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == diagramTextSizeProperty)
    {
        diagramTextSize = float(properties->mDDouble()->value(diagramTextSizeProperty));
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setTextSize(diagramTextSize);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == useCustomDiagramUpscalingFactorProperty)
    {
        useCustomDiagramUpscalingFactor = properties->mBool()->value(useCustomDiagramUpscalingFactorProperty);
#ifdef USE_EMBREE
        if (useCustomDiagramUpscalingFactorProperty)
        {
            for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
            {
                trajectoryPicker->setDiagramUpscalingFactor(diagramUpscalingFactor);
            }
            if (suppressActorUpdates()) return;
            emitActorChangedSignal();
        }
        diagramUpscalingFactorProperty->setEnabled(useCustomDiagramUpscalingFactor);
#endif
    }

    else if (property == diagramUpscalingFactorProperty)
    {
        diagramUpscalingFactor = float(properties->mDDouble()->value(diagramUpscalingFactorProperty));
#ifdef USE_EMBREE
        if (useCustomDiagramUpscalingFactor)
        {
            for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
            {
                trajectoryPicker->setDiagramUpscalingFactor(diagramUpscalingFactor);
            }
            if (suppressActorUpdates()) return;
            emitActorChangedSignal();
        }
#endif
    }

    else if (property == numBinsProperty)
    {
        numBins = properties->mInt()->value(numBinsProperty);
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setNumBins(numBins);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == sortByDescendingStdDevProperty)
    {
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->sortByDescendingStdDev();
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == showMinMaxValueProperty)
    {
        showMinMaxValue = properties->mBool()->value(showMinMaxValueProperty);
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setShowMinMaxValue(showMinMaxValue);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == useMaxForSensitivityProperty)
    {
        useMaxForSensitivity = properties->mBool()->value(useMaxForSensitivityProperty);
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setUseMaxForSensitivity(useMaxForSensitivity);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == trimNanRegionsProperty)
    {
        trimNanRegions = properties->mBool()->value(trimNanRegionsProperty);
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setTrimNanRegions(trimNanRegions);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == trajectorySyncModeProperty)
    {
        trajectorySyncMode = static_cast<TrajectorySyncMode>(
                properties->mEnum()->value(trajectorySyncModeProperty));
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setSyncMode(trajectorySyncMode);
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == useVariableToolTipProperty)
    {
        useVariableToolTip = properties->mBool()->value(useVariableToolTipProperty);
        useVariableToolTipChanged = true;
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->setUseVariableToolTip(useVariableToolTip);
        }
#endif
    }

    else if (property == selectAllTrajectoriesProperty)
    {
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->triggerSelectAllLines();
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (property == resetVariableSortingProperty)
    {
#ifdef USE_EMBREE
        for (MTrajectoryPicker* trajectoryPicker : trajectoryPickerMap)
        {
            trajectoryPicker->resetVariableSorting();
        }
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
#endif
    }

    else if (multiVarData.hasProperty(property))
    {
        multiVarData.onQtPropertyChanged(property);
#ifdef USE_EMBREE
        if (multiVarData.getSelectedOutputParameterChanged())
        {
            for (MTrajectoryPicker *trajectoryPicker: trajectoryPickerMap)
            {
                trajectoryPicker->updateSectedOutputParameter(multiVarData.getOutputParameterName(), multiVarData.getOutputParameterIdx());
            }
        }
#endif
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else
    {
        for (SeedActorSettings& sas : computationSeedActorProperties)
        {
            if (property == sas.removeProperty)
            {
                if (suppressActorUpdates())
                {
                    return;
                }

                QMessageBox::StandardButton reply = QMessageBox::question(
                            nullptr,
                            "Trajectory Actor",
                            "Are you sure you want to remove '"
                            + sas.actor->getName() + "'?",
                            QMessageBox::Yes|QMessageBox::No,
                            QMessageBox::No);

                if (reply == QMessageBox::Yes)
                {
                    removeSeedActor(sas.actor->getName());

                    releaseData();
                    updateActorData();

                    asynchronousDataRequest();
                    emitActorChangedSignal();
                }
            }
            else if (property == sas.lonSpacing || property == sas.latSpacing
                     || property == sas.pressureLevels)
            {
                if (suppressActorUpdates()) return;

                updateActorData();

                asynchronousDataRequest();
            }
        }
    }
}


void MTrajectoryActor::renderShadowsTubes(MSceneViewGLWidget *sceneView, int t)
{
    tubeShadowShader->bind();

    tubeShadowShader->setUniformValue(
            "mvpMatrix",
            *(sceneView->getModelViewProjectionMatrix()));
    tubeShadowShader->setUniformValue(
            "pToWorldZParams",
            sceneView->pressureToWorldZParameters());
    tubeShadowShader->setUniformValue(
            "lightDirection",
            sceneView->getLightDirection());
    tubeShadowShader->setUniformValue(
            "cameraPosition",
            sceneView->getCamera()->getOrigin());
    tubeShadowShader->setUniformValue(
            "radius",
            GLfloat(tubeRadius));
    tubeShadowShader->setUniformValue(
            "numObsPerTrajectory",
            trajectoryRequests[t].trajectories
                    ->getNumTimeStepsPerTrajectory());

    if (renderMode == BACKWARDTUBES_AND_SINGLETIME)
    {
        tubeShadowShader->setUniformValue(
                "renderTubesUpToIndex",
                particlePosTimeStep);
    }
    else
    {
        tubeShadowShader->setUniformValue(
                "renderTubesUpToIndex",
                trajectoryRequests[t].trajectories
                        ->getNumTimeStepsPerTrajectory());
    }

    tubeShadowShader->setUniformValue(
            "useTransferFunction", GLboolean(shadowColoured));

    if (shadowColoured)
    {
        tubeShadowShader->setUniformValue(
                "transferFunction", textureUnitTransferFunction);
        tubeShadowShader->setUniformValue(
                "scalarMinimum",
                transferFunction->getMinimumValue());
        tubeShadowShader->setUniformValue(
                "scalarMaximum",
                transferFunction->getMaximumValue());
    }
    else
        tubeShadowShader->setUniformValue(
                "constColour", QColor(20, 20, 20, 155));

    bindShadowStencilBuffer();
    glMultiDrawArrays(GL_LINE_STRIP_ADJACENCY,
                      trajectoryRequests[t].trajectorySelection
                              ->getStartIndices(),
                      trajectoryRequests[t].trajectorySelection
                              ->getIndexCount(),
                      trajectoryRequests[t].trajectorySelection
                              ->getNumTrajectories());
    unbindShadowStencilBuffer();
}

void MTrajectoryActor::renderShadowsSpheres(MSceneViewGLWidget *sceneView, int t)
{
    positionSphereShadowShader->bind();

    positionSphereShadowShader->setUniformValue(
            "mvpMatrix",
            *(sceneView->getModelViewProjectionMatrix()));
    CHECK_GL_ERROR;
    positionSphereShadowShader->setUniformValue(
            "pToWorldZParams",
            sceneView->pressureToWorldZParameters()); CHECK_GL_ERROR;
    positionSphereShadowShader->setUniformValue(
            "lightDirection",
            sceneView->getLightDirection());
    positionSphereShadowShader->setUniformValue(
            "cameraPosition",
            sceneView->getCamera()->getOrigin()); CHECK_GL_ERROR;
    positionSphereShadowShader->setUniformValue(
            "radius",
            GLfloat(sphereRadius)); CHECK_GL_ERROR;
    positionSphereShadowShader->setUniformValue(
            "scaleRadius",
            GLboolean(false)); CHECK_GL_ERROR;

    positionSphereShadowShader->setUniformValue(
            "useTransferFunction",
            GLboolean(shadowColoured)); CHECK_GL_ERROR;

    if (shadowColoured)
    {
        // Transfer function texture is still bound from the sphere
        // shader.
        positionSphereShadowShader->setUniformValue(
                "transferFunction",
                textureUnitTransferFunction); CHECK_GL_ERROR;
        positionSphereShadowShader->setUniformValue(
                "scalarMinimum",
                transferFunction->getMinimumValue()); CHECK_GL_ERROR;
        positionSphereShadowShader->setUniformValue(
                "scalarMaximum",
                transferFunction->getMaximumValue()); CHECK_GL_ERROR;

    }
    else
        positionSphereShadowShader->setUniformValue(
                "constColour", QColor(20, 20, 20, 155)); CHECK_GL_ERROR;

    bindShadowStencilBuffer();
    if (useMultiVarTrajectories)
    {
        positionSphereShadowShader->setUniformValue(
                "isInputWorldSpace", 1); CHECK_GL_ERROR;

        MTimeStepSphereRenderData* timeStepSphereRenderData =
                trajectoryRequests[t].timeStepSphereRenderDataMap[sceneView];
        GLuint positionBufferId = timeStepSphereRenderData->spherePositionsBuffer->getBufferObject();
        glBindBuffer(GL_ARRAY_BUFFER, positionBufferId); CHECK_GL_ERROR;
        glVertexAttribPointer(
                SHADER_VERTEX_ATTRIBUTE,
                3, // size
                GL_FLOAT, // type
                GL_FALSE, // normalized
                sizeof(QVector4D), // stride
                nullptr); CHECK_GL_ERROR; // offset
        glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE); CHECK_GL_ERROR;
        glDrawArrays(GL_POINTS, 0, timeStepSphereRenderData->numSpheres);
    }
    else
    {
        positionSphereShadowShader->setUniformValue(
                "isInputWorldSpace", 0); CHECK_GL_ERROR;

        if (renderMode == ALL_POSITION_SPHERES)
            glMultiDrawArrays(GL_POINTS,
                              trajectoryRequests[t]
                                      .trajectorySelection->getStartIndices(),
                              trajectoryRequests[t]
                                      .trajectorySelection->getIndexCount(),
                              trajectoryRequests[t]
                                      .trajectorySelection->getNumTrajectories());
        else
            glMultiDrawArrays(GL_POINTS,
                              trajectoryRequests[t]
                                      .trajectorySingleTimeSelection
                                      ->getStartIndices(),
                              trajectoryRequests[t]
                                      .trajectorySingleTimeSelection
                                      ->getIndexCount(),
                              trajectoryRequests[t]
                                      .trajectorySingleTimeSelection
                                      ->getNumTrajectories());
    }
    unbindShadowStencilBuffer();
    CHECK_GL_ERROR;
}

void MTrajectoryActor::bindShadowStencilBuffer()
{
    glDepthMask(GL_FALSE);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    if (!stencilBufferCleared) {
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        stencilBufferCleared = true;
    }
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    // Darker shadows
    //glStencilFunc(GL_NOTEQUAL, 2, 0xFF);
    //glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
}

void MTrajectoryActor::unbindShadowStencilBuffer()
{
    glStencilMask(0x00);
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
    CHECK_GL_ERROR;
}


void MTrajectoryActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    // Only render if transfer function is available.
    if (transferFunction == nullptr && !useMultiVarTrajectories)
    {
        return;
    }
    stencilBufferCleared = false;

    if (useMultiVarTrajectories)
    {
        for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
        {
            // If any required data item is missing we cannot render.
            if ( (trajectoryRequests[t].trajectories == nullptr)
                 || (trajectoryRequests[t].multiVarTrajectoriesMap[sceneView] == nullptr)
                 || (trajectoryRequests[t].trajectorySelection == nullptr) )
            {
                continue;
            }

            // If the vertical scaling of the view has changed, a recomputation
            // of the normals is necessary, as they are based on worldZ
            // coordinates.
            if (sceneView->visualisationParametersHaveChanged())
            {
                // Discard old normals.
                if (trajectoryRequests[t].normals.value(sceneView, nullptr))
                {
                    normalsSource->releaseData(trajectoryRequests[t].normals[sceneView]);
                }
                trajectoryRequests[t].normals[sceneView] = nullptr;

                // Discard old multi-var trajectories.
                if (trajectoryRequests[t].multiVarTrajectoriesMap.value(sceneView, nullptr))
                {
                    trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->releaseRenderData();
                    multiVarTrajectoriesSource->releaseData(
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]);
                }
                trajectoryRequests[t].multiVarTrajectoriesMap[sceneView] = nullptr;

                if (suppressActorUpdates()) return;
                asynchronousDataRequest();
                emitActorChangedSignal();

                continue;
            }

            if (multiVarData.getInternalRepresentationChanged())
            {
                // Discard old multi-var trajectories.
                if (trajectoryRequests[t].multiVarTrajectoriesMap.value(sceneView, nullptr))
                {
                    trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->releaseRenderData();
                    multiVarTrajectoriesSource->releaseData(
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]);
                }
                trajectoryRequests[t].multiVarTrajectoriesMap[sceneView] = nullptr;
                multiVarData.resetInternalRepresentationChanged();

                if (suppressActorUpdates()) return;
                asynchronousDataRequest();
                emitActorChangedSignal();

                continue;
            }

#ifdef USE_EMBREE
            if (trajectoryPickerMap[sceneView]->getSelectedTimeStepChanged())
            {
                if (synchronizeParticlePosTime)
                {
                    synchronizeParticlePosTime = false;
                    properties->mBool()->setValue(
                            synchronizeParticlePosTimeProperty,
                            synchronizeParticlePosTime);
                    updateTimeProperties();
                }

                particlePosTimeStep = int(std::round(trajectoryPickerMap[sceneView]->getSelectedTimeStep()));
                trajectoryPickerMap[sceneView]->setParticlePosTimeStep(particlePosTimeStep);
                trajectoryPickerMap[sceneView]->resetSelectedTimeStepChanged();
            }
            if (trajectoryPickerMap[sceneView]->getSelectedVariablesChanged())
            {
                multiVarData.setSelectedVariables(
                        trajectoryPickerMap[sceneView]->getSelectedVariableIndices());
                trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->updateSelectedVariableIndices(
                        multiVarData.getSelectedVariableIndices());
                trajectoryPickerMap[sceneView]->resetSelectedVariablesChanged();
            }

            if (trajectoryPickerMap[sceneView]->getSelectedTrajectoriesChanged())
            {
                trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->updateSelectedLines(
                        trajectoryPickerMap[sceneView]->getSelectedTrajectories());
                if (trajectoryPickerMap[sceneView]->getHasAscentData())
                {
                    trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->updateLineAscentTimeStepArrayBuffer(
                            trajectoryPickerMap[sceneView]->getAscentTimeStepIndices(),
                            trajectoryPickerMap[sceneView]->getMaxAscentTimeStepIndex());
                }
                trajectoryPickerMap[sceneView]->resetSelectedTrajectoriesChanged();
            }
#endif

            if (multiVarData.getSelectedVariablesChanged())
            {
                trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->updateSelectedVariableIndices(
                        multiVarData.getSelectedVariableIndices());
#ifdef USE_EMBREE
                trajectoryPickerMap[sceneView]->setSelectedVariableIndices(multiVarData.getSelectedVariableIndices());
#endif
                multiVarData.resetSelectedVariablesChanged();
            }

            if (multiVarData.getVarDivergingChanged())
            {
                trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->updateDivergingVariables(
                        multiVarData.getVarDiverging());
                multiVarData.resetVarDivergingChanged();
            }
            if (multiVarData.getSelectedOutputParameterChanged())
            {
                // TODO with Embree?
                trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->updateOutputParameterIdx(
                        multiVarData.getOutputParameterIdx());
                multiVarData.resetSelectedOutputParameterChanged();
            }
            if (multiVarData.getUseROI() && multiVarData.getSelectedROIChanged())
            {
                // TODO with Embree?
                trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->updateROI(
                        multiVarData.getROI());
                multiVarData.resetSelectedROIChanged();
            }

            std::shared_ptr<GL::MShaderEffect> tubeShader = multiVarData.getShaderEffect();
            tubeShader->bind();
            multiVarData.setUniformData(textureUnitTransferFunction);

            tubeShader->setUniformValue("mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
            tubeShader->setUniformValue("vMatrix", sceneView->getCamera()->getViewMatrix());
            tubeShader->setUniformValue("lightDirection", sceneView->getLightDirection());
            if (sceneView->getLightDirectionEnum() == MSceneViewGLWidget::VIEWDIRECTION) {
                tubeShader->setUniformValue(
                        "useHeadLight",
                        sceneView->getLightDirectionEnum() == MSceneViewGLWidget::VIEWDIRECTION ? 1 : 0);
            }
            tubeShader->setUniformValue("cameraPosition", sceneView->getCamera()->getOrigin());
            tubeShader->setUniformValue("orthographicModeEnabled", sceneView->orthographicModeEnabled() ? 1 : 0);
            //tubeShader->setUniformValue("cameraLookDirection", sceneView->getCamera()->getZAxis());
            tubeShader->setUniformValue("lineWidth", GLfloat(2.0f * tubeRadius));

            //if (renderMode == BACKWARDTUBES_AND_SINGLETIME)
            //{
            //    tubeShader->setUniformValue(
            //            "renderTubesUpToIndex",
            //            particlePosTimeStep);
            //}
            //else
            //{
            //    tubeShader->setUniformValue(
            //            "renderTubesUpToIndex",
            //            trajectoryRequests[t]
            //                    .trajectories->getNumTimeStepsPerTrajectory());
            //}
            if (multiVarData.getUseTimestepLens())
            {
                if (!useMultiVarSpheres)
                {
                    tubeShader->setUniformValue("timestepLensePosition", particlePosTimeStep);
                }
                else
                {
                    tubeShader->setUniformValue("timestepLensePosition", std::numeric_limits<int>::lowest());
                }
            }

            MMultiVarTrajectoriesRenderData& multiVarTrajectoriesRenderData =
                    trajectoryRequests[t].multiVarTrajectoriesRenderDataMap[sceneView];

            // Bind vertex buffer objects.
            if (multiVarTrajectoriesRenderData.useGeometryShader) {
                multiVarTrajectoriesRenderData.vertexPositionBuffer->attachToVertexAttribute(0);
                multiVarTrajectoriesRenderData.vertexNormalBuffer->attachToVertexAttribute(1);
                multiVarTrajectoriesRenderData.vertexTangentBuffer->attachToVertexAttribute(2);
                multiVarTrajectoriesRenderData.vertexLineIDBuffer->attachToVertexAttribute(3);
                multiVarTrajectoriesRenderData.vertexElementIDBuffer->attachToVertexAttribute(4);
            } else {
                multiVarTrajectoriesRenderData.linePointDataBuffer->bindToIndex(0);
            }

            // Bind shader storage buffer objects.
            multiVarTrajectoriesRenderData.variableArrayBuffer->bindToIndex(2);
            multiVarTrajectoriesRenderData.lineDescArrayBuffer->bindToIndex(4);
            multiVarTrajectoriesRenderData.varDescArrayBuffer->bindToIndex(5);
            multiVarTrajectoriesRenderData.lineVarDescArrayBuffer->bindToIndex(6);
            if (multiVarData.getShowTargetVariableAndSensitivity())
            {
                multiVarTrajectoriesRenderData.varSelectedTargetVariableAndSensitivityArrayBuffer->bindToIndex(7);
            }
            else
            {
                multiVarTrajectoriesRenderData.varSelectedArrayBuffer->bindToIndex(7);
            }
            multiVarTrajectoriesRenderData.varDivergingArrayBuffer->bindToIndex(8);
            if (multiVarData.getUseROI())
            {
                multiVarTrajectoriesRenderData.roiSelectionBuffer->bindToIndex(17);
            }
            if (diagramType == DiagramDisplayType::NONE || diagramType == DiagramDisplayType::CURVE_PLOT_VIEW)
            {
                multiVarTrajectoriesRenderData.lineSelectedArrayBuffer->bindToIndex(14);
            }
            multiVarTrajectoriesRenderData.varOutputParameterIdxBuffer->bindToIndex(16);

            glPolygonMode(GL_FRONT_AND_BACK, renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;

            multiVarTrajectoriesRenderData.indexBuffer->bindToElementArrayBuffer();
            if (multiVarDataDirty) {
                multiVarDataDirty = false;
                trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->setDirty(true);
            }
            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->updateTrajectorySelection(
                    trajectoryRequests[t].trajectorySelection->getStartIndices(),
                    trajectoryRequests[t].trajectorySelection->getIndexCount(),
                    trajectoryRequests[t].trajectorySelection->getNumTimeStepsPerTrajectory(),
                    trajectoryRequests[t].trajectorySelection->getNumTrajectories());
#ifdef USE_EMBREE
            QVector<QVector<QVector3D>> trajectoriesData;
            QVector<QVector<float>> trajectoryPointTimeSteps;
            QVector<uint32_t> selectedTrajectoryIndices;
            if (trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getFilteredTrajectories(
                    trajectoryRequests[t].trajectorySelection->getStartIndices(),
                    trajectoryRequests[t].trajectorySelection->getIndexCount(),
                    trajectoryRequests[t].trajectorySelection->getNumTimeStepsPerTrajectory(),
                    trajectoryRequests[t].trajectorySelection->getNumTrajectories(),
                    trajectoriesData, trajectoryPointTimeSteps, selectedTrajectoryIndices)) {
                trajectoryPickerMap[sceneView]->setTrajectoryData(
                        trajectoriesData, trajectoryPointTimeSteps, selectedTrajectoryIndices,
                        trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getNumTrajectoriesTotal());
            }
#endif
            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->setDirty(false);
            bool useFiltering = trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getUseFiltering();
            if (multiVarTrajectoriesRenderData.useGeometryShader)
            {
                if (useFiltering)
                {
                    glMultiDrawElements(
                            GL_LINES,
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getTrajectorySelectionCount(),
                            GL_UNSIGNED_INT,
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getTrajectorySelectionIndices(),
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getNumFilteredTrajectories());
                }
                else
                {
                    glDrawElements(
                            GL_LINES, multiVarTrajectoriesRenderData.indexBuffer->getCount(),
                            multiVarTrajectoriesRenderData.indexBuffer->getType(), nullptr);
                }
            }
            else
            {
                if (useFiltering)
                {
                    glMultiDrawElements(
                            GL_TRIANGLES,
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getTrajectorySelectionCount(),
                            GL_UNSIGNED_INT,
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getTrajectorySelectionIndices(),
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getNumFilteredTrajectories());
                }
                else
                {
                    glDrawElements(
                            GL_TRIANGLES, multiVarTrajectoriesRenderData.indexBuffer->getCount(),
                            multiVarTrajectoriesRenderData.indexBuffer->getType(), nullptr);
                }
            }

            if (multiVarData.getShowTargetVariableAndSensitivity())
            {
                multiVarTrajectoriesRenderData.varSelectedArrayBuffer->bindToIndex(7);
            }

#ifdef USE_EMBREE
            // Render selected/highlighted trajectories.
            trajectoryPickerMap[sceneView]->setParticlePosTimeStep(particlePosTimeStep);
            if (diagramType != DiagramDisplayType::NONE && diagramType != DiagramDisplayType::CURVE_PLOT_VIEW)
            {
                MHighlightedTrajectoriesRenderData highlightedTrajectoriesRenderData =
                        trajectoryPickerMap[sceneView]->getHighlightedTrajectoriesRenderData();
                if (highlightedTrajectoriesRenderData.indexBufferHighlighted)
                {
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT);
                    std::shared_ptr<GL::MShaderEffect> highlightShader =
                            trajectoryPickerMap[sceneView]->getHighlightShaderEffect();
                    highlightShader->bind();
                    highlightShader->setUniformValue(
                            "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
                    highlightedTrajectoriesRenderData.indexBufferHighlighted->bindToElementArrayBuffer();
                    highlightedTrajectoriesRenderData.vertexPositionBufferHighlighted->attachToVertexAttribute(0);
                    highlightedTrajectoriesRenderData.vertexColorBufferHighlighted->attachToVertexAttribute(1);
                    glDrawElements(
                            GL_TRIANGLES, highlightedTrajectoriesRenderData.indexBufferHighlighted->getCount(),
                            highlightedTrajectoriesRenderData.indexBufferHighlighted->getType(), nullptr);
                    glDisable(GL_CULL_FACE);
                }
            }
#endif

#ifdef USE_EMBREE
            trajectoryPickerMap[sceneView]->updateRenderSpheresIfNecessary(multiVarData.getRenderSpheres());
#endif
            // Render spheres at the selected time step positions.
            if (multiVarData.getUseTimestepLens() && useMultiVarSpheres && multiVarData.getRenderSpheres())
            {
                bool sphereDataChanged = trajectoryRequests[t].multiVarTrajectoriesMap[
                        sceneView]->updateTimeStepSphereRenderDataIfNecessary(
                        trajectorySyncMode, particlePosTimeStep, syncModeTrajectoryIndex, sphereRadius);
                MTimeStepSphereRenderData* timeStepSphereRenderData =
                        trajectoryRequests[t].timeStepSphereRenderDataMap[sceneView];

#ifdef USE_EMBREE
                trajectoryPickerMap[sceneView]->setMultiVarFocusRenderMode(multiVarData.getFocusRenderMode());
                trajectoryPickerMap[sceneView]->setShowTargetVariableAndSensitivity(
                        multiVarData.getShowTargetVariableAndSensitivity());
                if (useVariableToolTip && (useVariableToolTipChanged || sphereDataChanged))
                {
                    trajectoryPickerMap[sceneView]->setTimeStepSphereData(
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getSpherePositions(),
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getSphereEntrancePoints(),
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getSphereExitPoints(),
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getSphereLineElementIds(),
                            sphereRadius);
                    useVariableToolTipChanged = false;
                }
#endif

                std::shared_ptr<GL::MShaderEffect> timeStepSphereShader = multiVarData.getTimeStepSphereShader();
                timeStepSphereShader->bind();
                multiVarData.setUniformDataSpheres(textureUnitTransferFunction);

                timeStepSphereShader->setUniformValue("mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
                timeStepSphereShader->setUniformValue("vMatrix", sceneView->getCamera()->getViewMatrix());
                timeStepSphereShader->setUniformValue("lightDirection", sceneView->getLightDirection());
                if (sceneView->getLightDirectionEnum() == MSceneViewGLWidget::VIEWDIRECTION) {
                    timeStepSphereShader->setUniformValue(
                            "useHeadLight",
                            sceneView->getLightDirectionEnum() == MSceneViewGLWidget::VIEWDIRECTION ? 1 : 0);
                }
                timeStepSphereShader->setUniformValue("cameraPosition", sceneView->getCamera()->getOrigin());
                timeStepSphereShader->setUniformValue("orthographicModeEnabled", sceneView->orthographicModeEnabled() ? 1 : 0);
                timeStepSphereShader->setUniformValue("sphereRadius", sphereRadius);
                timeStepSphereShader->setUniformValue("lineRadius", tubeRadius);
                timeStepSphereShader->setUniformValue("timestepLensePosition", particlePosTimeStep);
                timeStepSphereShader->setUniformValue("cameraUp", sceneView->getCamera()->getYAxis());

                timeStepSphereRenderData->indexBuffer->bindToElementArrayBuffer();
                timeStepSphereRenderData->vertexPositionBuffer->attachToVertexAttribute(0);
                timeStepSphereRenderData->vertexNormalBuffer->attachToVertexAttribute(1);

                timeStepSphereRenderData->spherePositionsBuffer->bindToIndex(10);
                timeStepSphereRenderData->entrancePointsBuffer->bindToIndex(11);
                timeStepSphereRenderData->exitPointsBuffer->bindToIndex(12);
                timeStepSphereRenderData->lineElementIdsBuffer->bindToIndex(13);

                if (useFiltering)
                {
                    glMultiDrawElements(
                            GL_TRIANGLES,
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getTrajectorySelectionCount(),
                            GL_UNSIGNED_INT,
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getTrajectorySelectionIndices(),
                            trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->getNumFilteredTrajectories());
                }
                else
                {
                    glDrawElements(
                            GL_TRIANGLES, multiVarTrajectoriesRenderData.indexBuffer->getCount(),
                            multiVarTrajectoriesRenderData.indexBuffer->getType(), nullptr);
                }

                glDrawElementsInstanced(
                        GL_TRIANGLES, timeStepSphereRenderData->indexBuffer->getCount(),
                        timeStepSphereRenderData->indexBuffer->getType(), nullptr,
                        timeStepSphereRenderData->numSpheres);
            }

            // Render rolls at time step positions.
            if (multiVarData.getUseTimestepLens() && useMultiVarSpheres && multiVarData.getRenderRolls())
            {
                trajectoryRequests[t].multiVarTrajectoriesMap[sceneView]->updateTimeStepRollsRenderDataIfNecessary(
                        trajectorySyncMode, particlePosTimeStep, syncModeTrajectoryIndex, tubeRadius, sphereRadius,
                        multiVarData.getRollsWidth(), multiVarData.getMapRollsThickness(),
                        multiVarData.getNumLineSegments());
                MTimeStepRollsRenderData* timeStepRollsRenderData =
                        trajectoryRequests[t].timeStepRollsRenderDataMap[sceneView];
                if (timeStepRollsRenderData->indexBuffer)
                {
                    std::shared_ptr<GL::MShaderEffect> timeStepSphereShader = multiVarData.getTimeStepRollsShader();
                    timeStepSphereShader->bind();
                    multiVarData.setUniformDataRolls(textureUnitTransferFunction);

                    timeStepSphereShader->setUniformValue(
                            "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
                    timeStepSphereShader->setUniformValue(
                            "vMatrix", sceneView->getCamera()->getViewMatrix());
                    timeStepSphereShader->setUniformValue(
                            "lightDirection", sceneView->getLightDirection());
                    timeStepSphereShader->setUniformValue(
                            "cameraPosition", sceneView->getCamera()->getOrigin());
                    timeStepSphereShader->setUniformValue(
                            "orthographicModeEnabled", sceneView->orthographicModeEnabled() ? 1 : 0);
                    timeStepSphereShader->setUniformValue(
                            "lineRadius", tubeRadius);
                    timeStepSphereShader->setUniformValue(
                            "rollsRadius", sphereRadius);
                    timeStepSphereShader->setUniformValue(
                            "timestepLensePosition", particlePosTimeStep);

                    timeStepRollsRenderData->indexBuffer->bindToElementArrayBuffer();
                    timeStepRollsRenderData->vertexPositionBuffer->attachToVertexAttribute(0);
                    timeStepRollsRenderData->vertexNormalBuffer->attachToVertexAttribute(1);
                    timeStepRollsRenderData->vertexTangentBuffer->attachToVertexAttribute(2);
                    timeStepRollsRenderData->vertexRollPositionBuffer->attachToVertexAttribute(3);
                    timeStepRollsRenderData->vertexLineIdBuffer->attachToVertexAttribute(4);
                    timeStepRollsRenderData->vertexLinePointIdxBuffer->attachToVertexAttribute(5);
                    timeStepRollsRenderData->vertexVariableIdAndIsCapBuffer->attachToVertexAttribute(6);

                    glDrawElements(
                            GL_TRIANGLES, timeStepRollsRenderData->indexBuffer->getCount(),
                            timeStepRollsRenderData->indexBuffer->getType(), nullptr);
                }
            }

            // Unbind IBO.
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

            if (shadowEnabled)
            {
                // Bind trajectories and normals vertex buffer objects.
                trajectoryRequests[t].trajectoriesVertexBuffer
                        ->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

                trajectoryRequests[t].normalsVertexBuffer[sceneView]
                        ->attachToVertexAttribute(SHADER_NORMAL_ATTRIBUTE);

                // Bind auxiliary data vertex buffer object.
                if (trajectoryRequests[t].trajectoriesAuxDataVertexBuffer != nullptr)
                {
                    trajectoryRequests[t].trajectoriesAuxDataVertexBuffer
                            ->attachToVertexAttribute(SHADER_AUXDATA_ATTRIBUTE);
                }

                renderShadowsTubes(sceneView, t);

                if (multiVarData.getRenderSpheres())
                {
                    renderShadowsSpheres(sceneView, t);
                }
            }

            // Unbind VBO.
            glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
        }

        return;
    }


    if ( (renderMode == TRAJECTORY_TUBES)
         || (renderMode == TUBES_AND_SINGLETIME)
         || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
    {
        for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size());
             t++)
        {
            // If any required data item is missing we cannot render.
            if ( (trajectoryRequests[t].trajectories == nullptr)
                 || (trajectoryRequests[t].normals[sceneView] == nullptr)
                 || (trajectoryRequests[t].trajectorySelection == nullptr) )
            {
                continue;
            }

            // If the vertical scaling of the view has changed, a recomputation
            // of the normals is necessary, as they are based on worldZ
            // coordinates.
            if (sceneView->visualisationParametersHaveChanged())
            {
                // Discard old normals.
                if (trajectoryRequests[t].normals.value(sceneView, nullptr))
                {
                    normalsSource->releaseData(trajectoryRequests[t]
                                               .normals[sceneView]);
                }

                trajectoryRequests[t].normals[sceneView] = nullptr;
                continue;
            }

            tubeShader->bind();

            tubeShader->setUniformValue(
                    "mvpMatrix",
                    *(sceneView->getModelViewProjectionMatrix()));
            tubeShader->setUniformValue(
                    "pToWorldZParams",
                    sceneView->pressureToWorldZParameters());
            tubeShader->setUniformValue(
                    "lightDirection",
                    sceneView->getLightDirection());
            tubeShader->setUniformValue(
                    "cameraPosition",
                    sceneView->getCamera()->getOrigin());
            tubeShader->setUniformValue(
                    "radius",
                    GLfloat(tubeRadius));
            tubeShader->setUniformValue(
                    "numObsPerTrajectory",
                    trajectoryRequests[t]
                        .trajectories->getNumTimeStepsPerTrajectory());

            // Set shader uniform for defining the type of color rendering.
            tubeShader->setUniformValue(
                        "renderAuxData",
                        GLboolean(renderColorMode == AUXDATA));

            if (renderMode == BACKWARDTUBES_AND_SINGLETIME)
            {
                tubeShader->setUniformValue(
                            "renderTubesUpToIndex",
                            particlePosTimeStep);
            }
            else
            {
                tubeShader->setUniformValue(
                            "renderTubesUpToIndex",
                            trajectoryRequests[t]
                            .trajectories->getNumTimeStepsPerTrajectory());
            }

            // Texture bindings for transfer function for data scalar (1D texture
            // from transfer function class). The data scalar is stored in the
            // vertex.w component passed to the vertex shader.
            transferFunction->getTexture()->bindToTextureUnit(
                    textureUnitTransferFunction);
            tubeShader->setUniformValue(
                    "transferFunction", textureUnitTransferFunction);
            tubeShader->setUniformValue(
                    "scalarMinimum", transferFunction->getMinimumValue());
            tubeShader->setUniformValue(
                    "scalarMaximum", transferFunction->getMaximumValue());

            // Bind trajectories and normals vertex buffer objects.
            trajectoryRequests[t].trajectoriesVertexBuffer
                    ->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

            trajectoryRequests[t].normalsVertexBuffer[sceneView]
                    ->attachToVertexAttribute(SHADER_NORMAL_ATTRIBUTE);

            // Bind auxiliary data vertex buffer object.
            if (trajectoryRequests[t].trajectoriesAuxDataVertexBuffer != nullptr)
            {
                trajectoryRequests[t].trajectoriesAuxDataVertexBuffer
                        ->attachToVertexAttribute(SHADER_AUXDATA_ATTRIBUTE);
            }

            glPolygonMode(GL_FRONT_AND_BACK,
                          renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
            glLineWidth(1); CHECK_GL_ERROR;

            glMultiDrawArrays(GL_LINE_STRIP_ADJACENCY,
                              trajectoryRequests[t].trajectorySelection
                              ->getStartIndices(),
                              trajectoryRequests[t].trajectorySelection
                              ->getIndexCount(),
                              trajectoryRequests[t].trajectorySelection
                              ->getNumTrajectories());
            CHECK_GL_ERROR;


            if (shadowEnabled)
            {
                renderShadowsTubes(sceneView, t);
            }

            // Unbind VBO.
            glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
        }
    }


    if ( (renderMode == ALL_POSITION_SPHERES)
         || (renderMode == SINGLETIME_POSITIONS)
         || (renderMode == TUBES_AND_SINGLETIME)
         || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
    {
        for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size());
             t++)
        {
            if (trajectoryRequests[t].trajectories == nullptr)
            {
                continue;
            }

            if (renderMode == ALL_POSITION_SPHERES)
            {
                if (trajectoryRequests[t].trajectorySelection == nullptr)
                {
                    continue;
                }
            }
            else
            {
                if (trajectoryRequests[t]
                        .trajectorySingleTimeSelection == nullptr)
                {
                    continue;
                }
            }

            positionSphereShader->bindProgram("Normal");

            // Set MVP-matrix and parameters to map pressure to world space in
            // the vertex shader.
            positionSphereShader->setUniformValue(
                    "mvpMatrix",
                    *(sceneView->getModelViewProjectionMatrix()));
            positionSphereShader->setUniformValue(
                    "pToWorldZParams",
                    sceneView->pressureToWorldZParameters());
            positionSphereShader->setUniformValue(
                    "lightDirection",
                    sceneView->getLightDirection());
            positionSphereShader->setUniformValue(
                    "cameraPosition",
                    sceneView->getCamera()->getOrigin());
            positionSphereShader->setUniformValue(
                    "cameraUpDir",
                    sceneView->getCamera()->getYAxis());
            positionSphereShader->setUniformValue(
                    "radius",
                    GLfloat(sphereRadius));
            positionSphereShader->setUniformValue(
                    "scaleRadius",
                    GLboolean(false));
            positionSphereShader->setUniformValue(
                        "renderAuxData",
                        GLboolean(renderColorMode == AUXDATA));

            // Texture bindings for transfer function for data scalar (1D
            // texture from transfer function class). The data scalar is stored
            // in the vertex.w component passed to the vertex shader.
            transferFunction->getTexture()->bindToTextureUnit(
                    textureUnitTransferFunction);
            positionSphereShader->setUniformValue(
                    "useTransferFunction", GLboolean(true));
            positionSphereShader->setUniformValue(
                    "transferFunction", textureUnitTransferFunction);
            positionSphereShader->setUniformValue(
                    "scalarMinimum", transferFunction->getMinimumValue());
            positionSphereShader->setUniformValue(
                    "scalarMaximum", transferFunction->getMaximumValue());

            // Bind vertex buffer object.
            trajectoryRequests[t].trajectoriesVertexBuffer
                    ->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

            // Bind auxiliary data vertex buffer object.
            if (trajectoryRequests[t].trajectoriesAuxDataVertexBuffer != nullptr)
            {
                trajectoryRequests[t].trajectoriesAuxDataVertexBuffer
                        ->attachToVertexAttribute(SHADER_AUXDATA_ATTRIBUTE);
            }

            glPolygonMode(GL_FRONT_AND_BACK,
                          renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
            glLineWidth(1); CHECK_GL_ERROR;

            if (renderMode == ALL_POSITION_SPHERES)
            {
                glMultiDrawArrays(GL_POINTS,
                                  trajectoryRequests[t]
                                  .trajectorySelection->getStartIndices(),
                                  trajectoryRequests[t]
                                  .trajectorySelection->getIndexCount(),
                                  trajectoryRequests[t]
                                  .trajectorySelection->getNumTrajectories());
            }
            else
            {
                glMultiDrawArrays(GL_POINTS,
                                  trajectoryRequests[t]
                                  .trajectorySingleTimeSelection
                                  ->getStartIndices(),
                                  trajectoryRequests[t]
                                  .trajectorySingleTimeSelection
                                  ->getIndexCount(),
                                  trajectoryRequests[t]
                                  .trajectorySingleTimeSelection
                                  ->getNumTrajectories());
            }
            CHECK_GL_ERROR;


            if (shadowEnabled)
            {
                renderShadowsSpheres(sceneView, t);
            }

            // Unbind VBO.
            glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
        }
    }
}


void MTrajectoryActor::renderOverlayToCurrentContext(MSceneViewGLWidget *sceneView)
{
    // Only render if transfer function is available.
    if (transferFunction == nullptr && !useMultiVarTrajectories)
    {
        return;
    }

    if (useMultiVarTrajectories)
    {
        for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
        {
            // If any required data item is missing we cannot render.
            if ( (trajectoryRequests[t].trajectories == nullptr)
                 || (trajectoryRequests[t].multiVarTrajectoriesMap[sceneView] == nullptr)
                 || (trajectoryRequests[t].trajectorySelection == nullptr) )
            {
                continue;
            }

#ifdef USE_EMBREE
            trajectoryPickerMap[sceneView]->render();
#endif
        }
    }
}


void MTrajectoryActor::updateTimeProperties()
{
    enableActorUpdates(false);

    bool enableSync = synchronizationControl != nullptr;

    initTimeProperty->setEnabled(!(enableSync && synchronizeInitTime));
    startTimeProperty->setEnabled(!(enableSync && synchronizeStartTime));
    particlePosTimeProperty->setEnabled(
                !(enableSync && synchronizeParticlePosTime));

    updateSyncPropertyColourHints();

    enableActorUpdates(true);
}


void MTrajectoryActor::updateEnsembleProperties()
{
    enableActorUpdates(false);

    // If the ensemble is synchronized, disable all properties (they are set
    // via the synchronization control).
    ensembleSingleMemberProperty->setEnabled(!synchronizeEnsemble);

    enableActorUpdates(true);
}


void MTrajectoryActor::printDebugOutputOnUserRequest()
{
    debugPrintPendingRequestsQueue();
}


void MTrajectoryActor::outputAsLagrantoASCIIFile()
{
    LOG4CPLUS_DEBUG(mlog, "Writing trajectory data to an ASCII file in "
                          "'LAGANRTO.2' format.");

    if (trajectoryRequests.isEmpty() ||
            trajectoryRequests[0].trajectories == nullptr)
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: No trajectory data available.");
        return;
    }

    // Let user specify filename.
    MSystemManagerAndControl* sysMC = MSystemManagerAndControl::getInstance();
    QString directory = sysMC->getMet3DWorkingDirectory().absolutePath();
    QString fileName = QFileDialog::getSaveFileName(
                sysMC->getMainWindow(), tr("Save trajectory file"), directory);

    if (fileName.isEmpty())
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: No filename specified.");
        return;
    }

    QFile asciiFile(fileName);
    if (asciiFile.open(QFile::WriteOnly | QFile::Truncate))
    {
        QTextStream out(&asciiFile);

        // Shortcut to trajectories.
        MTrajectories* t = trajectoryRequests.at(0).trajectories;

        // Write header line in the format
        // Reference date 20090129_0600 / Time range    2880 min
        out << "Reference date " << t->getValidTime().toString("yyyyMMdd_hhmm")
            << " / Time range " << QString("%1").arg(
                   t->getTimes().at(0).secsTo(
                       t->getTimes().at(t->getTimes().size()-1)) / 60.,
                   7, 'f', 0)
            << " min\n\n";

        // Trajectory data are written in the format:
        // (see https://www.geosci-model-dev.net/8/2569/2015/gmd-8-2569-2015-supplement.pdf)
        //
        //   time      lon     lat     p        TH       IWC     LABEL
        // -----------------------------------------------------------
        //    0.00   -65.42   42.61   859   293.010     0.000     0.000
        //    6.00   -58.64   47.48   713   296.745     0.208     1.000

        // Print header with vertex dimensions (time, lon, lat, pres).
        out << QString(" %1 ").arg("time [h]",-10);
        out << QString(" %1 ").arg("lon",-10);
        out << QString(" %1 ").arg("lat",-10);
        out << QString(" %1 ").arg("p",-20);
        QString underLineHeader = QString(" %1").arg("----------------------------------------------------");

        // Add the auxiliary data variable names to the header line.
        QStringList allAuxDataVarNames = t->getAuxDataVarNames();
        for (int i = 0; i < allAuxDataVarNames.size(); i++)
        {
           out << QString(" %1 ").arg(allAuxDataVarNames.value(i),-20) ;
           underLineHeader = underLineHeader + "----------------------";
        }
        out << "\n";
        out << underLineHeader + "\n\n";

        // Loop over all available trajectory data objects and write their
        // contents to file.
        int numTrajObjects = precomputedDataSource ? 1 : seedActorData.size();
        for (int tobj = 0; tobj < numTrajObjects; tobj++)
        {
            t = trajectoryRequests.at(tobj).trajectories;
            if (t == nullptr)
            {
                continue;
            }

            // Loop over all trajectories.
            for (int iTraj = 0; iTraj < t->getNumTrajectories(); iTraj++)
            {
                // Loop over all times steps per trajectory.
                for (int iTime = 0; iTime < t->getTimes().size(); iTime++)
                {
                    // Get vertex coordinates of current trajectory and time
                    // step.
                    QVector3D v = t->getVertices().at(
                                t->getStartIndices()[iTraj] + iTime);

                    // Compute time difference to start time in hours.
                    double tDiff = t->getTimes().at(0).secsTo(
                                t->getTimes().at(iTime)) / 3600.;

                    // Write traj. vertices to file.
                    out << QString(" %1 ").arg(tDiff,-10, 'f', 2);
                    out << QString(" %1 ").arg(v.x(),-10, 'g', 6);
                    out << QString(" %1 ").arg(v.y(),-10, 'g', 6);
                    out << QString(" %1 ").arg(v.z(),-20, 'g', 6);

                    // Write auxiliary data values at this vertex to file.
                    if (t->getSizeOfAuxDataAtVertices() > 0)
                    {
                        for (int iAuxDataVar=0;
                             iAuxDataVar<allAuxDataVarNames.size();
                             iAuxDataVar++)
                        {
                            float iAuxDataValue =
                                    t->getAuxDataAtVertex(
                                        t->getStartIndices()[iTraj] + iTime).at(
                                        iAuxDataVar);
                            out << QString(" %1 ").arg(iAuxDataValue, -20, 'g', 6);
                        }
                    }
                    out << endl;
                }

                out << endl;
            }
        }
    }

    LOG4CPLUS_DEBUG(mlog, "Trajectory data write completed.");
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MTrajectoryActor::updateActorData()
{
    seedActorData.clear();
    for (SeedActorSettings& sas : computationSeedActorProperties)
    {
        if (!sas.actor->isEnabled())
        {
            continue;
        }

        double stepSizeLon = properties->mDDouble()->value(sas.lonSpacing);
        double stepSizeLat = properties->mDDouble()->value(sas.latSpacing);
        QVector<float> pressureLevels = parseFloatRangeString(
                    properties->mString()->value(sas.pressureLevels));

        switch (sas.type)
        {
        case POLE_ACTOR:
        {
            MMovablePoleActor *actor =
                    dynamic_cast<MMovablePoleActor *>(sas.actor);

            // Every pole has 2 vertices for bottom and top.
            for (int i = 0; i < actor->getPoleVertices().size(); i += 2)
            {
                QVector3D bot = actor->getPoleVertices().at(i);
                QVector3D top = actor->getPoleVertices().at(i + 1);

                SeedActorData data;
                data.type = VERTICAL_POLE;
                data.minPosition = QVector3D(top.x(), top.y(), top.z());
                data.maxPosition = QVector3D(bot.x(), bot.y(), bot.z());
                data.stepSize = QVector2D(stepSizeLon, stepSizeLat);
                data.pressureLevels.clear();
                for (float f : pressureLevels)
                {
                    if (bot.z() >= f && f >= top.z())
                        data.pressureLevels << f;
                }
                seedActorData.push_back(data);
            }
            break;
        }
        case HORIZONTAL_ACTOR:
        {
            MNWPHorizontalSectionActor *actor =
                    dynamic_cast<MNWPHorizontalSectionActor *>(sas.actor);

            if (actor->getBoundingBoxName() == "None")
            {
                continue;
            }

            QRectF hor = actor->getHorizontalBBox();
            double p = actor->getSectionElevation_hPa();

            SeedActorData data;
            data.type = HORIZONTAL_SECTION;
            data.minPosition = QVector3D(hor.x(), hor.y(), p);
            data.maxPosition = QVector3D(hor.x() + hor.width(),
                                         hor.y() + hor.height(), p);
            data.stepSize = QVector2D(stepSizeLon, stepSizeLat);
            data.pressureLevels.clear();
            data.pressureLevels << p;
            seedActorData.push_back(data);
            break;
        }
        case VERTICAL_ACTOR:
        {
            MNWPVerticalSectionActor *actor =
                    dynamic_cast<MNWPVerticalSectionActor *>(sas.actor);

            if (actor->getBoundingBoxName() == "None")
            {
                continue;
            }

            if (!actor->getWaypointsModel())
                continue;

            for (int i = 0; i < actor->getWaypointsModel()->size() - 1; i++)
            {
                QVector2D point1 = actor->getWaypointsModel()->positionLonLat(i);
                QVector2D point2 =
                        actor->getWaypointsModel()->positionLonLat(i + 1);
                double pbot = actor->getBottomPressure_hPa();
                double ptop = actor->getTopPressure_hPa();

                SeedActorData data;
                data.type = VERTICAL_SECTION;
                data.minPosition = QVector3D(point1.x(), point1.y(), ptop);
                data.maxPosition = QVector3D(point2.x(), point2.y(), pbot);
                data.stepSize = QVector2D(stepSizeLon, stepSizeLat);
                data.pressureLevels.clear();
                for (float f : pressureLevels)
                {
                    if (pbot >= f && f >= ptop)
                        data.pressureLevels << f;
                }

                seedActorData.push_back(data);
            }
            break;
        }
        case BOX_ACTOR:
        {
            MVolumeBoundingBoxActor *actor =
                    dynamic_cast<MVolumeBoundingBoxActor *>(sas.actor);

            if (actor->getBoundingBoxName() == "None")
            {
                continue;
            }

            QRectF hor = actor->getHorizontalBBox();
            double pbot = actor->getBottomPressure_hPa();
            double ptop = actor->getTopPressure_hPa();

            SeedActorData data;
            data.type = VOLUME_BOX;
            data.minPosition = QVector3D(hor.x(), hor.y(), ptop);
            data.maxPosition = QVector3D(hor.x() + hor.width(),
                                         hor.y() + hor.height(), pbot);
            data.stepSize = QVector2D(stepSizeLon, stepSizeLat);
            data.pressureLevels.clear();
            for (float f : pressureLevels)
            {
                if (pbot >= f && f >= ptop)
                    data.pressureLevels << f;
            }
            seedActorData.push_back(data);
            break;
        }
        }
    }

    int size = precomputedDataSource ? 1 : seedActorData.size();

    // Resize trajectory request vector containing all buffers.
    trajectoryRequests.resize(size);
}


QDateTime MTrajectoryActor::getPropertyTime(QtProperty *enumProperty)
{
    QStringList dateStrings = properties->mEnum()->enumNames(enumProperty);

    // If the list of date strings is empty return an invalid null time.
    if (dateStrings.empty())
    {
        return QDateTime();
    }

    int index = properties->mEnum()->value(enumProperty);
    return QDateTime::fromString(dateStrings.at(index), Qt::ISODate);
}


int MTrajectoryActor::getEnsembleMember()
{
    QString memberString = properties->getEnumItem(ensembleSingleMemberProperty);

    bool ok = true;
    int member = memberString.toInt(&ok);

    if (ok) return member; else return -99999;
}


void MTrajectoryActor::setTransferFunctionFromProperty()
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    QString tfName = properties->getEnumItem(transferFunctionProperty);

    if (tfName == "None")
    {
        transferFunction = nullptr;
        return;
    }

    // Find the selected transfer function in the list of actors from the
    // resources manager. Not very efficient, but works well enough for the
    // small number of actors at the moment.
    foreach (MActor *actor, glRM->getActors())
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
        {
            if (tf->transferFunctionName() == tfName)
            {
                transferFunction = tf;
                return;
            }
        }
    }
}


void MTrajectoryActor::setDiagramTransferFunctionFromProperty()
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    QString tfName = properties->getEnumItem(diagramTransferFunctionProperty);

    if (tfName == "None")
    {
        diagramTransferFunction = nullptr;
        return;
    }

    // Find the selected transfer function in the list of actors from the
    // resources manager. Not very efficient, but works well enough for the
    // small number of actors at the moment.
    foreach (MActor *actor, glRM->getActors())
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
        {
            if (tf->transferFunctionName() == tfName)
            {
                diagramTransferFunction = tf;
                return;
            }
        }
    }
}


void MTrajectoryActor::asynchronousDataRequest(bool synchronizationRequest)
{
    // No computations necessary if trajectories are not displayed. (Besides
    // data requests not needed lead to predefined trajectory actor not being
    // displayed and system crash due to waiting for a unfinished thread at
    // program end.) Also no computations needed if actor is not connected to a
    // data source.
    if (getViews().size() == 0 || trajectorySource == nullptr)
    {
        return;
    }

#ifndef DIRECT_SYNCHRONIZATION
    Q_UNUSED(synchronizationRequest);
#else
    unsigned int requestID = ++globalRequestIDCounter;
    if (synchronizationRequest)
    {
        // Due to the current implementation of having multiple request queues
        // for multiple seed actors, we need to keep track of how many
        // requests have been sent for a given sync event. For this, each
        // request is assigned a unique "request ID"; the map
        // "numSeedActorsForRequestEvent" stores how many "seed actors" have
        // caused requests. In prepareAvailableDataForRendering(), this
        // counter is checked before the "sync completed" signal is sent
        // to the sync control.
//NOTE (mr, 28Aug2018) -- a better way to implement this would probably be to
// signal back to the sync control //which// sync signal has been completed...
        numSeedActorsForRequestEvent.insert(requestID, 0);
    }
#endif

    for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
    {
        // Initialize empty MTrajectoryRequestQueueInfo.
        MTrajectoryRequestQueueInfo trqi;
        trqi.dataRequest.request = "NULL";
        trqi.dataRequest.available = false;
        trqi.singleTimeFilterRequest.request = "NULL";
        trqi.singleTimeFilterRequest.available = false;
        trqi.filterRequest.request = "NULL";
        trqi.filterRequest.available = false;
        trqi.numPendingRequests = 0;
#ifdef DIRECT_SYNCHRONIZATION
        trqi.syncchronizationRequest = synchronizationRequest;
        trqi.requestID = requestID;
        if (synchronizationRequest)
        {
            numSeedActorsForRequestEvent[requestID] += 1;
        }
#endif

        // Request 1: Trajectories for the current time and ensemble settings.
        // ===================================================================
        QDateTime initTime  = getPropertyTime(initTimeProperty);
        QDateTime validTime = getPropertyTime(startTimeProperty);
        unsigned int member = getEnsembleMember();

        MDataRequestHelper rh;
        rh.insert("INIT_TIME", initTime);
        rh.insert("VALID_TIME", validTime);
        rh.insert("MEMBER", member);
        rh.insert("TIME_SPAN", "ALL");

        // If computed dataSource is used, additional information is needed.
        if (!precomputedDataSource)
        {
            unsigned int lineType = properties->mEnum()->value(
                        computationLineTypeProperty);
            QDateTime endTime;
            if (TRAJECTORY_COMPUTATION_LINE_TYPE(lineType) == PATH_LINE)
            {
                endTime = availableStartTimes.at(
                            properties->mEnum()->value(
                                computationIntegrationTimeProperty));
            }
            else
            {
                // For streamlines the "end time" is not really required.
                // The trajectory computation source, however, requests all
                // time steps between validTime and endTime, hence set it
                // to the same time. (mr, 17Jul2018 -- could be changed..)
                endTime = validTime;
            }
            unsigned int integrationMethod = properties->mEnum()->value(
                        computationIntegrationMethodProperty);
            unsigned int interpolationMethod = properties->mEnum()->value(
                        computationInterpolationMethodProperty);
            unsigned int numSubTimeSteps = properties->mInt()->value(
                        computationNumSubTimeStepsProperty);
            unsigned int seedType = seedActorData[t].type;
            double streamlineDeltaS = properties->mDDouble()->value(
                        computationStreamlineDeltaSProperty);
            int streamlineLength = properties->mInt()->value(
                        computationStreamlineLengthProperty);
            QString seedMinPosition = QString("%1/%2/%3")
                    .arg(seedActorData[t].minPosition.x())
                    .arg(seedActorData[t].minPosition.y())
                    .arg(seedActorData[t].minPosition.z());
            QString seedMaxPosition = QString("%1/%2/%3")
                    .arg(seedActorData[t].maxPosition.x())
                    .arg(seedActorData[t].maxPosition.y())
                    .arg(seedActorData[t].maxPosition.z());
            QString seedStepSize = QString("%1/%2")
                    .arg(seedActorData[t].stepSize.x())
                    .arg(seedActorData[t].stepSize.y());
            QString seedPressureLevels = listOfPressureLevelsAsString(
                        seedActorData[t].pressureLevels, QString("/"));

            // Insert additional informations into data request.
            rh.insert("END_TIME", endTime);
            rh.insert("LINE_TYPE", lineType);
            rh.insert("SUBTIMESTEPS_PER_DATATIMESTEP", numSubTimeSteps);
            rh.insert("STREAMLINE_DELTA_S", streamlineDeltaS);
            rh.insert("STREAMLINE_LENGTH", streamlineLength);
            rh.insert("INTEGRATION_METHOD", integrationMethod);
            rh.insert("INTERPOLATION_METHOD", interpolationMethod);
            rh.insert("SEED_TYPE", seedType);
            rh.insert("SEED_MIN_POSITION", seedMinPosition);
            rh.insert("SEED_MAX_POSITION", seedMaxPosition);
            rh.insert("SEED_STEP_SIZE_LON_LAT", seedStepSize);
            rh.insert("SEED_PRESSURE_LEVELS", seedPressureLevels);
        }

        trqi.dataRequest.request = rh.request();
        trqi.numPendingRequests++;

        // Request 2: Normals for all scene views that display the trajectories.
        // =====================================================================
        foreach (MSceneViewGLWidget* view, getViews())
        {
            QVector2D params = view->pressureToWorldZParameters();
            QString query = QString("%1/%2").arg(params.x()).arg(params.y());
            LOG4CPLUS_DEBUG(mlog, "NORMALS: " << query.toStdString());

            rh.insert("NORMALS_LOGP_SCALED", query);
            MRequestQueueInfo rqi;
            rqi.request = rh.request();
            rqi.available = false;
            trqi.normalsRequests.insert(view, rqi);
            trqi.numPendingRequests++;
        }
        rh.remove("NORMALS_LOGP_SCALED");

        // Request 3: Multi-var trajectories for all scene views that display the trajectories.
        // =====================================================================
        if (useMultiVarTrajectories)
        {
            foreach (MSceneViewGLWidget* view, getViews())
            {
                QVector2D params = view->pressureToWorldZParameters();
                QString query;
                if (multiVarData.getGeometryMode() == MultiVarGeometryMode::GEOMETRY_SHADER) {
                    query = QString("%1/%2/1").arg(params.x()).arg(params.y());
                } else {
                    query = QString("%1/%2/0/%3").arg(params.x()).arg(params.y()).arg(multiVarData.getNumLineSegments());
                }
                LOG4CPLUS_DEBUG(mlog, "MULTIVARTRAJECTORIES: " << query.toStdString());

                rh.insert("MULTIVARTRAJECTORIES_LOGP_SCALED", query);
                MRequestQueueInfo rqi;
                rqi.request = rh.request();
                rqi.available = false;
                trqi.multiVarTrajectoriesRequests.insert(view, rqi);
                trqi.numPendingRequests++;
            }
            rh.remove("MULTIVARTRAJECTORIES_LOGP_SCALED");
        }

        // Request 4: Pressure/Time selection filter.
        // ==========================================

        //TODO: add property
        rh.insert("TRY_PRECOMPUTED", 1);

        bool filteringEnabled = properties->mBool()->value(
                    enableAscentFilterProperty);
        if (filteringEnabled)
        {
            float deltaPressure_hPa = properties->mDDouble()->value(
                        deltaPressureFilterProperty);
            int deltaTime_hrs =
                    properties->mDDouble()->value(deltaTimeFilterProperty);
            // Request is e.g. 500/48 for 500 hPa in 48 hours.
            rh.insert("FILTER_PRESSURE_TIME",
                      QString("%1/%2").arg(deltaPressure_hPa).arg(deltaTime_hrs));
        }
        else
        {
            rh.insert("FILTER_PRESSURE_TIME", "ALL");
        }
        // Request bounding box filtering.
        if (bBoxConnection->getBoundingBox() != nullptr)
        {
            rh.insert("FILTER_BBOX", QString("%1/%2/%3/%4")
                      .arg(bBoxConnection->westLon()).arg(
                          bBoxConnection->southLat())
                      .arg(bBoxConnection->eastLon()).arg(
                          bBoxConnection->northLat()));
        }

        if ((renderMode == SINGLETIME_POSITIONS)
            || (renderMode == TUBES_AND_SINGLETIME)
            || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
        {
            rh.insert("FILTER_TIMESTEP", QString("%1").arg(particlePosTimeStep));
            trqi.singleTimeFilterRequest.request = rh.request();
            trqi.numPendingRequests++;
        }

        if (renderMode != SINGLETIME_POSITIONS)
        {
            rh.insert("FILTER_TIMESTEP", "ALL");
            trqi.filterRequest.request = rh.request();
            trqi.numPendingRequests++;
        }

        LOG4CPLUS_DEBUG(mlog, "Trajectories asynchronous data request: "
                              "Enqueuing [" << trqi.numPendingRequests
                        << "] new pending requests.");
        trajectoryRequests[t].pendingRequestsQueue.enqueue(trqi);
//        debugPrintPendingRequestsQueue();

        // Emit requests AFTER its information has been posted to the queue.
        // (Otherwise requestData() may trigger a call to
        // asynchronous...Available() before the request information has been
        // posted; then the incoming request is not accepted).

        trajectorySource->requestData(trqi.dataRequest.request);

        foreach (MSceneViewGLWidget* view, getViews())
        {
            normalsSource->requestData(trqi.normalsRequests[view].request);
        }

        if (useMultiVarTrajectories)
        {
            foreach (MSceneViewGLWidget* view, getViews())
            {
                view->setLightDirection(MSceneViewGLWidget::VIEWDIRECTION);
                multiVarTrajectoriesSource->requestData(trqi.multiVarTrajectoriesRequests[view].request);
            }
        }

        if ((renderMode == SINGLETIME_POSITIONS)
            || (renderMode == TUBES_AND_SINGLETIME)
            || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
        {
            trajectoryFilter->requestData(trqi.singleTimeFilterRequest.request);
        }

        if (renderMode != SINGLETIME_POSITIONS)
        {
            trajectoryFilter->requestData(trqi.filterRequest.request);
        }
    }
}


void MTrajectoryActor::asynchronousSelectionRequest()
{
    for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
    {
        MTrajectoryRequestQueueInfo trqi;
        trqi.dataRequest.request = "NULL";
        trqi.dataRequest.available = false;
        trqi.singleTimeFilterRequest.request = "NULL";
        trqi.singleTimeFilterRequest.available = false;
        trqi.filterRequest.request = "NULL";
        trqi.filterRequest.available = false;
        trqi.numPendingRequests = 0;
#ifdef DIRECT_SYNCHRONIZATION
        // Selection requests currently are not synchronized.
        trqi.syncchronizationRequest = false;
#endif

        // Get the current init and valid (= trajectory start) time.
        QDateTime initTime  = getPropertyTime(initTimeProperty);
        QDateTime validTime = getPropertyTime(startTimeProperty);
        unsigned int member = getEnsembleMember();

        MDataRequestHelper rh;
        rh.insert("INIT_TIME", initTime);
        rh.insert("VALID_TIME", validTime);
        rh.insert("MEMBER", member);
        rh.insert("TIME_SPAN", "ALL");
        //TODO: add property
        rh.insert("TRY_PRECOMPUTED", 1);

        // If computed dataSource is used, additional information is needed.
        if (!precomputedDataSource)
        {
            unsigned int lineType = properties->mEnum()->value(
                        computationLineTypeProperty);
            QDateTime endTime;
            if (TRAJECTORY_COMPUTATION_LINE_TYPE(lineType) == PATH_LINE)
            {
                int endTimeIndex = properties->mEnum()->value(
                            computationIntegrationTimeProperty);
                if (endTimeIndex < 0)
                {
                    continue;
                }
                endTime = availableStartTimes.at(max(0, endTimeIndex));
            }
            else
            {
                // For streamlines the "end time" is not really required.
                // The trajectory computation source, however, requests all
                // time steps between validTime and endTime, hence set it
                // to the same time. (mr, 17Jul2018 -- could be changed..)
                endTime = validTime;
            }
            unsigned int integrationMethod = properties->mEnum()->value(
                        computationIntegrationMethodProperty);
            unsigned int interpolatonMethod = properties->mEnum()->value(
                        computationInterpolationMethodProperty);
            unsigned int subTimeSteps = properties->mInt()->value(
                        computationNumSubTimeStepsProperty);
            unsigned int seedType = seedActorData[t].type;
            double streamlineDeltaS = properties->mDDouble()->value(
                        computationStreamlineDeltaSProperty);
            int streamlineLength = properties->mInt()->value(
                        computationStreamlineLengthProperty);
            QString seedMinPosition = QString("%1/%2/%3")
                    .arg(seedActorData[t].minPosition.x())
                    .arg(seedActorData[t].minPosition.y())
                    .arg(seedActorData[t].minPosition.z());
            QString seedMaxPosition = QString("%1/%2/%3")
                    .arg(seedActorData[t].maxPosition.x())
                    .arg(seedActorData[t].maxPosition.y())
                    .arg(seedActorData[t].maxPosition.z());
            QString seedStepSize = QString("%1/%2")
                    .arg(seedActorData[t].stepSize.x())
                    .arg(seedActorData[t].stepSize.y());
            QString seedPressureLevels = listOfPressureLevelsAsString(
                        seedActorData[t].pressureLevels, QString("/"));

            // Insert additional informations into data request.
            rh.insert("END_TIME", endTime);
            rh.insert("LINE_TYPE", lineType);
            rh.insert("SUBTIMESTEPS_PER_DATATIMESTEP", subTimeSteps);
            rh.insert("STREAMLINE_DELTA_S", streamlineDeltaS);
            rh.insert("STREAMLINE_LENGTH", streamlineLength);
            rh.insert("INTEGRATION_METHOD", integrationMethod);
            rh.insert("INTERPOLATION_METHOD", interpolatonMethod);
            rh.insert("SEED_TYPE", seedType);
            rh.insert("SEED_MIN_POSITION", seedMinPosition);
            rh.insert("SEED_MAX_POSITION", seedMaxPosition);
            rh.insert("SEED_STEP_SIZE_LON_LAT", seedStepSize);
            rh.insert("SEED_PRESSURE_LEVELS", seedPressureLevels);
        }

        // Filter the trajectories of this member according to the specified
        // pressure interval (xx hPa over the "lifetime" of the trajectories;
        // e.g. for T-NAWDEX over 48 hours).

        bool filteringEnabled = properties->mBool()->value(
                    enableAscentFilterProperty);
        if (filteringEnabled)
        {
            float deltaPressure_hPa = properties->mDDouble()->value(
                    deltaPressureFilterProperty);
            int deltaTime_hrs = properties->mDDouble()->value(
                        deltaTimeFilterProperty);
            // Request is e.g. 500/48 for 500 hPa in 48 hours.
            rh.insert("FILTER_PRESSURE_TIME",
                      QString("%1/%2").arg(deltaPressure_hPa).arg(deltaTime_hrs));
        }
        else
        {
            rh.insert("FILTER_PRESSURE_TIME", "ALL");
        }
        // Request bounding box filtering.
        if (bBoxConnection->getBoundingBox() != nullptr)
        {
            rh.insert("FILTER_BBOX", QString("%1/%2/%3/%4")
                  .arg(bBoxConnection->westLon()).arg(
                          bBoxConnection->southLat())
                  .arg(bBoxConnection->eastLon()).arg(
                          bBoxConnection->northLat()));
        }

        if ((renderMode == SINGLETIME_POSITIONS)
            || (renderMode == TUBES_AND_SINGLETIME)
            || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
        {
            rh.insert("FILTER_TIMESTEP", QString("%1").arg(particlePosTimeStep));
            trqi.singleTimeFilterRequest.request = rh.request();
            trqi.numPendingRequests++;
        }

        if (renderMode != SINGLETIME_POSITIONS)
        {
            rh.insert("FILTER_TIMESTEP", "ALL");
            trqi.filterRequest.request = rh.request();
            trqi.numPendingRequests++;
        }

        trajectoryRequests[t].pendingRequestsQueue.enqueue(trqi);

        if ((renderMode == SINGLETIME_POSITIONS)
            || (renderMode == TUBES_AND_SINGLETIME)
            || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
        {
            trajectoryFilter->requestData(trqi.singleTimeFilterRequest.request);
        }

        if (renderMode != SINGLETIME_POSITIONS)
        {
            trajectoryFilter->requestData(trqi.filterRequest.request);
        }
    }
}


void MTrajectoryActor::updateInitTimeProperty()
{
    enableActorUpdates(false);

    if (trajectorySource == nullptr)
    {
        properties->mEnum()->setEnumNames(initTimeProperty, QStringList());
    }
    else
    {
        // Get the current init time value.
        QDateTime initTime  = getPropertyTime(initTimeProperty);

        // Get available init times from the trajectory source. Convert the
        // QDateTime objects to strings for the enum manager.
        availableInitTimes = trajectorySource->availableInitTimes();
        QStringList timeStrings;
        for (int i = 0; i < availableInitTimes.size(); i++)
        {
            timeStrings << availableInitTimes.at(i).toString(Qt::ISODate);
        }

        properties->mEnum()->setEnumNames(initTimeProperty, timeStrings);

        int newIndex = max(0, availableInitTimes.indexOf(initTime));
        properties->mEnum()->setValue(initTimeProperty, newIndex);
        if (synchronizeInitTime && synchronizationControl != nullptr)
        {
            setInitDateTime(synchronizationControl->initDateTime());
        }
    }

    enableActorUpdates(true);
}


void MTrajectoryActor::updateStartTimeProperty()
{
    enableActorUpdates(false);

    if (trajectorySource == nullptr)
    {
        properties->mEnum()->setEnumNames(startTimeProperty, QStringList());
    }
    else
    {
        // Get the current time values.
        QDateTime initTime  = getPropertyTime(initTimeProperty);
        QDateTime startTime = getPropertyTime(startTimeProperty);

        // Get a list of the available start times for the new init time,
        // convert the QDateTime objects to strings for the enum manager.
        availableStartTimes = trajectorySource->availableStartTimes(initTime);
        QStringList startTimeStrings;
        for (int i = 0; i < availableStartTimes.size(); i++)
            startTimeStrings << availableStartTimes.at(i).toString(Qt::ISODate);

        properties->mEnum()->setEnumNames(startTimeProperty, startTimeStrings);

        int newIndex = max(0, availableStartTimes.indexOf(startTime));
        properties->mEnum()->setValue(startTimeProperty, newIndex);
        if (synchronizeStartTime && synchronizationControl != nullptr)
        {
            setStartDateTime(synchronizationControl->validDateTime());
        }
    }

    updateTrajectoryIntegrationTimeProperty();

    enableActorUpdates(true);
}


void MTrajectoryActor::updateAuxDataVarNamesProperty()
{
    enableActorUpdates(false);

    if (trajectorySource == nullptr)
    {
        properties->mEnum()->setEnumNames(renderAuxDataVarProperty,
                                          QStringList());
    }
    else
    {
        properties->mEnum()->setEnumNames(
                    renderAuxDataVarProperty,
                    trajectorySource->availableAuxiliaryVariables());
        trajectorySyncModeProperty->setEnabled(
                trajectorySource->availableAuxiliaryVariables().indexOf("time_after_ascent") >= 0);
    }

    enableActorUpdates(true);
}



void MTrajectoryActor::updateParticlePosTimeProperty()
{
    enableActorUpdates(false);

    // All trajectories have the exact same type, find any initialized source.
    int index = -1;
    for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
    {
        if (trajectoryRequests[t].trajectories != nullptr)
        {
            index = t;
            break;
        }
    }

    // No initialized trajectory found.
    if (index < 0)
    {
        properties->mEnum()->setEnumNames(particlePosTimeProperty, QStringList());
    }
    else
    {
        QDateTime currentValue = getPropertyTime(particlePosTimeProperty);

        availableParticlePosTimes = trajectoryRequests[index].trajectories
                ->getTimes().toList();
        QStringList particlePosTimeStrings;
        for (int i = 0;
             i < trajectoryRequests[index].trajectories->getTimes().size(); i++)
        {
            particlePosTimeStrings
                    << trajectoryRequests[index].trajectories
                       ->getTimes().at(i).toString(Qt::ISODate);
        }

        properties->mEnum()->setEnumNames(particlePosTimeProperty,
                                          particlePosTimeStrings);

        // Try to restore previous time value. If the previous value is not
        // available for the new trajectories, indexOf() returns -1. This is
        // changed to 0, i.e. the first available time value is selected.
        int newIndex = max(0, trajectoryRequests[index].trajectories
                           ->getTimes().indexOf(currentValue));
        properties->mEnum()->setValue(particlePosTimeProperty, newIndex);

        // The trajectory time property is not needed when the entire
        // trajectories are rendered.
        switch (renderMode)
        {
        case TRAJECTORY_TUBES:
        case ALL_POSITION_SPHERES:
            particlePosTimeProperty->setEnabled(false);
            break;
        case SINGLETIME_POSITIONS:
        case TUBES_AND_SINGLETIME:
        case BACKWARDTUBES_AND_SINGLETIME:
            particlePosTimeProperty->setEnabled(!synchronizeParticlePosTime);
            break;
        }
        if (synchronizeParticlePosTime && synchronizationControl != nullptr)
        {
            setParticleDateTime(synchronizationControl->validDateTime());
        }
    }

    enableActorUpdates(true);
}


bool MTrajectoryActor::updateEnsembleSingleMemberProperty()
{
    // Remember currently set ensemble member in order to restore it below
    // (if getEnsembleMember() returns a value < 0 the list is currently
    // empty; however, since the ensemble members are represented by
    // unsigned ints below we cast this case to 0).
    int prevEnsembleMember = max(0, getEnsembleMember());

    // Update ensembleSingleMemberProperty so that the user can choose
    // from the list of available members. (Requires first sorting the set of
    // members as a list, which can then be converted  to a string list).
    availableEnsembleMembersAsSortedList =
            trajectorySource->availableEnsembleMembers().toList();
    qSort(availableEnsembleMembersAsSortedList);

    QStringList availableMembersAsStringList;
    foreach (unsigned int member, availableEnsembleMembersAsSortedList)
        availableMembersAsStringList << QString("%1").arg(member);

    enableActorUpdates(false);
    properties->mEnum()->setEnumNames(
                ensembleSingleMemberProperty, availableMembersAsStringList);
    properties->setEnumPropertyClosest<unsigned int>(
                availableEnsembleMembersAsSortedList,
                (unsigned int)prevEnsembleMember,
                ensembleSingleMemberProperty, synchronizeEnsemble, getScenes());
    enableActorUpdates(true);

    bool displayedMemberHasChanged = (getEnsembleMember() != prevEnsembleMember);
    return displayedMemberHasChanged;
}


void MTrajectoryActor::updateTrajectoryIntegrationTimeProperty()
{
    enableActorUpdates(false);

    if (trajectorySource == nullptr || precomputedDataSource)
    {
        properties->mEnum()->setEnumNames(
                    computationIntegrationTimeProperty, QStringList());
    }
    else
    {
        // Get current start time.
        QDateTime startTime = getPropertyTime(startTimeProperty);

        // Determine possible integration times from available time steps
        // in the NWP data source.
        QStringList timeIntervals;
        for (QDateTime& time : availableStartTimes)
        {
            timeIntervals << QString("%1 hrs").arg(
                                 startTime.secsTo(time) / 3600.0);
        }

        // Update items in GUI list.
        properties->updateEnumItems(
                    computationIntegrationTimeProperty, timeIntervals);
    }

    enableActorUpdates(true);
}


bool MTrajectoryActor::internalSetDateTime(
        const QList<QDateTime>& availableTimes,
        const QDateTime&        datetime,
        QtProperty*             timeProperty)
{
    // Find the time closest to "datetime" in the list of available valid
    // times.
    int i = -1; // use of "++i" below
    bool exactMatch = false;
    while (i < availableTimes.size() - 1)
    {
        // Loop as long as datetime is larger that the currently inspected
        // element (use "++i" to have the same i available for the remaining
        // statements in this block).
        if (datetime > availableTimes.at(++i)) continue;

        // We'll only get here if datetime <= availableTimes.at(i). If we
        // have an exact match, break the loop. This is our time.
        if (availableTimes.at(i) == datetime)
        {
            exactMatch = true;
            break;
        }

        // If datetime cannot be exactly matched it lies between indices i-1
        // and i in availableTimes. Determine which is closer.
        if (i == 0) break; // if there's no i-1 we're done
        if ( abs(datetime.secsTo(availableTimes.at(i-1)))
             <= abs(datetime.secsTo(availableTimes.at(i))) ) i--;
        // "i" now contains the index of the closest available valid time.
        break;
    }

    if (i > -1)
    {
        // Update background colour of the valid time property in the connected
        // scene's property browser: green if the scene's valid time is an
        // exact match with one of the available valid time, red otherwise.
        if (synchronizationControl != nullptr)
        {
            QColor colour = exactMatch ? QColor(0, 255, 0) : QColor(255, 0, 0);
            for (int i = 0; i < getScenes().size(); i++)
            {
                getScenes().at(i)->setPropertyColour(timeProperty, colour);
            }
        }

        // Get the currently selected index.
        int currentIndex = static_cast<QtEnumPropertyManager*> (
                    timeProperty->propertyManager())->value(timeProperty);

        if (i == currentIndex)
        {
            // Index i is already the current one. Nothing needs to be done.
            return false;
        }
        else
        {
            // Set the new valid time.
            static_cast<QtEnumPropertyManager*> (timeProperty->propertyManager())
                    ->setValue(timeProperty, i);
            // A new index was set. Return true.
            return true;
        }
    }

    return false;
}


void MTrajectoryActor::debugPrintPendingRequestsQueue()
{
    // Debug: Output content of request queue.

    QString str = QString("\n==================\nTrajectory Actor :: "
                          "Pending requests queue:\n");

    for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
    {
        if (!precomputedDataSource)
        {
            str += QString("\n++++++++++++++\nRequests triggered for seed "
                           "actor #%1:\n").arg(t);
        }

        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size(); i++)
        {
            str += QString("\nRequest queue entry #%1: pending requests=[%2]\n")
                    .arg(i).arg(trajectoryRequests[t].pendingRequestsQueue[i]
                                .numPendingRequests);

            str += QString("\nData: available=[%1]\n[%2]\n")
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i]
                         .dataRequest.available)
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i]
                         .dataRequest.request);

            str += QString("\nFiler: available=[%1]\n[%2]\n")
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i]
                         .filterRequest.available)
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i]
                         .filterRequest.request);

            str += QString("\nSingle time filer: available=[%1]\n[%2]\n")
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i]
                         .singleTimeFilterRequest.available)
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i]
                         .singleTimeFilterRequest.request);

            int sv = 0;
            foreach (MSceneViewGLWidget *view,
                     trajectoryRequests[t].pendingRequestsQueue[i]
                     .normalsRequests.keys())
            {
                str += QString("\nScene view [%1] normals: available=[%2]\n[%3]\n")
                        .arg(sv++)
                        .arg(trajectoryRequests[t].pendingRequestsQueue[i]
                             .normalsRequests[view].available)
                        .arg(trajectoryRequests[t].pendingRequestsQueue[i]
                             .normalsRequests[view].request);
            }

            if (useMultiVarTrajectories)
            {
                int sv = 0;
                foreach (MSceneViewGLWidget *view,
                         trajectoryRequests[t].pendingRequestsQueue[i]
                                 .multiVarTrajectoriesRequests.keys())
                {
                    str += QString("\nScene view [%1] multi-var trajectories: available=[%2]\n[%3]\n")
                            .arg(sv++)
                            .arg(trajectoryRequests[t].pendingRequestsQueue[i]
                                         .multiVarTrajectoriesRequests[view].available)
                            .arg(trajectoryRequests[t].pendingRequestsQueue[i]
                                         .multiVarTrajectoriesRequests[view].request);
                }
            }
        }
    }

    str += QString("\n==================\n");

    LOG4CPLUS_DEBUG(mlog, str.toStdString());
}


bool MTrajectoryActor::selectDataSource()
{
    // TODO (bt, May2017): Why does the program crash when calling a message
    // boxes or dialogs during initialisation of GL?
    bool appInitialized =
            MSystemManagerAndControl::getInstance()->applicationIsInitialized();
    if (!appInitialized)
    {
        return false;
    }
    // Ask the user for data sources to which times and ensemble members the
    // sync control should be restricted to.
    MSelectDataSourceDialog dialog(MSelectDataSourceDialogType::TRAJECTORIES, 0);
    if (dialog.exec() == QDialog::Rejected)
    {
        return false;
    }

    QString dataSourceID = dialog.getSelectedDataSourceID();

    if (dataSourceID == "")
    {
        return false;
    }

    // Only change data sources if necessary.
    if (this->dataSourceID != dataSourceID)
    {
        properties->mString()->setValue(utilizedDataSourceProperty,
                                        dataSourceID);

        // Release data from old data sources before switching to new data
        // sources.
        releaseData();

        this->setDataSource(dataSourceID + QString(" Reader"));
        this->setNormalsSource(dataSourceID + QString(" Normals"));
        this->setTrajectoryFilter(dataSourceID + QString(" timestepFilter"));
        this->setMultiVarTrajectoriesSource(dataSourceID + QString(" Multi-Var Trajectories"));

        updateInitTimeProperty();
        updateStartTimeProperty();
        updateEnsembleSingleMemberProperty();
        updateAuxDataVarNamesProperty();

        return true;
    }

    return true;
}


void MTrajectoryActor::openSeedActorDialog()
{
    const QList<MSelectActorType> types =
    {MSelectActorType::POLE_ACTOR, MSelectActorType::HORIZONTALSECTION_ACTOR,
     MSelectActorType::VERTICALSECTION_ACTOR, MSelectActorType::BOX_ACTOR};

    MSelectActorDialog dialog(types, 0);
    if (dialog.exec() == QDialog::Rejected)
    {
        return;
    }

    QString actorName = dialog.getSelectedActor().actorName;
    QVector<float> defaultPressureLvls = {850, 700, 500, 300, 200};
    addSeedActor(actorName, 10.0, 10.0, defaultPressureLvls);
}


void MTrajectoryActor::addSeedActor(
        QString actorName, double deltaLon, double deltaLat,
        QVector<float> pressureLevels)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    MActor* actor = glRM->getActorByName(actorName);

    // Actor does not exist.
    if (!actor)
    {
        return;
    }

    // Check if entry already exists.
    for (SeedActorSettings& sas : computationSeedActorProperties)
    {
        if (sas.actor->getName().compare(actorName) == 0)
        {
            return;
        }
    }

    // Connect to actor changed signal but only if the trajectory data source
    // is a computation data source. (Otherwise this connection would lead to
    // unwanted recomputation requests).
    if (dynamic_cast<MTrajectoryComputationSource*>(trajectorySource))
    {
        connect(actor, SIGNAL(actorChanged()), this, SLOT(onSeedActorChanged()));
    }

    // Create property group for this seed actor.
    SeedActorSettings actorSettings;
    actorSettings.actor = actor;
    actorSettings.propertyGroup = addProperty(
                GROUP_PROPERTY, actorName, computationSeedPropertyGroup);
    actorSettings.removeProperty = addProperty(
                CLICK_PROPERTY, "remove", actorSettings.propertyGroup);

    actorSettings.lonSpacing = addProperty(
                DECORATEDDOUBLE_PROPERTY, "seed lon spacing",
                actorSettings.propertyGroup);
    actorSettings.latSpacing = addProperty(
                DECORATEDDOUBLE_PROPERTY, "seed lat spacing",
                actorSettings.propertyGroup);
    actorSettings.pressureLevels = addProperty(
                STRING_PROPERTY, "seed pressure levels",
                actorSettings.propertyGroup);

    bool seedInLon = false, seedInLat = false, seedInP = false;
    if (dynamic_cast<MMovablePoleActor*>(actor))
    {
        actorSettings.type = POLE_ACTOR;
        seedInLon = false;
        seedInLat = false;
        seedInP = true;
    }
    else if (dynamic_cast<MNWPHorizontalSectionActor*>(actor))
    {
        actorSettings.type = HORIZONTAL_ACTOR;
        seedInLon = true;
        seedInLat = true;
        seedInP = false;
    }
    else if (dynamic_cast<MNWPVerticalSectionActor*>(actor))
    {
        actorSettings.type = VERTICAL_ACTOR;
        seedInLon = true;
        seedInLat = false;
        seedInP = true;
    }
    else if (dynamic_cast<MVolumeBoundingBoxActor*>(actor))
    {
        actorSettings.type = BOX_ACTOR;
        seedInLon = true;
        seedInLat = true;
        seedInP = true;
    }

    properties->setDDouble(actorSettings.lonSpacing,
                           seedInLon ? deltaLon : NC_MAX_DOUBLE,
                           0.001, 999999.999, 3, 1.0, " deg");
    properties->setDDouble(actorSettings.latSpacing,
                           seedInLat ? deltaLat : NC_MAX_DOUBLE,
                           0.001, 999999.999, 3, 1.0, " deg");
    properties->mString()->setValue(
                actorSettings.pressureLevels,
                listOfPressureLevelsAsString(pressureLevels, QString(",")));

    actorSettings.lonSpacing->setEnabled(seedInLon);
    actorSettings.latSpacing->setEnabled(seedInLat);
    actorSettings.pressureLevels->setEnabled(seedInP);

    // Store new seed actor settings to list of seed actors.
    computationSeedActorProperties.push_back(actorSettings);
}


void MTrajectoryActor::clearSeedActor()
{
    for (SeedActorSettings& sas : computationSeedActorProperties)
    {
        // Disconnect signal before deletion.
        disconnect(sas.actor, SIGNAL(actorChanged()), this,
                   SLOT(onSeedActorChanged()));

        // Delete property.
        computationSeedPropertyGroup->removeSubProperty(sas.propertyGroup);
    }

    // Clear list.
    computationSeedActorProperties.clear();
}


void MTrajectoryActor::removeSeedActor(QString name)
{
    // Delete actor with given name.
    for (int i = 0; i < computationSeedActorProperties.size(); i++)
    {
        if (computationSeedActorProperties.at(i).actor->getName()
                .compare(name) == 0)
        {
            // Disconnect signal before deletion.
            disconnect(computationSeedActorProperties.at(i).actor,
                       SIGNAL(actorChanged()), this, SLOT(onSeedActorChanged()));

            // Remove actor from properties and list.
            computationSeedPropertyGroup->removeSubProperty(
                        computationSeedActorProperties.at(i).propertyGroup);
            computationSeedActorProperties.removeAt(i);
            break;
        }
    }
}


void MTrajectoryActor::enableProperties(bool enable)
{
    enableActorUpdates(false);
//    ensembleModeProperty->setEnabled(enable);
    enableAscentFilterProperty->setEnabled(enable);
    deltaPressureFilterProperty->setEnabled(enable);
    deltaTimeFilterProperty->setEnabled(enable);
    renderModeProperty->setEnabled(enable);
    renderColorModeProperty->setEnabled(enable);
    renderAuxDataVarProperty->setEnabled(enable);

    // Synchronisation properties should only be enabled if actor is connected
    // to a sync control.
    bool enableSync = (enable && (synchronizationControl != nullptr));
    synchronizationProperty->setEnabled(enable);
    synchronizeInitTimeProperty->setEnabled(enableSync);
    synchronizeStartTimeProperty->setEnabled(enableSync);
    synchronizeParticlePosTimeProperty->setEnabled(enableSync);
    synchronizeEnsembleProperty->setEnabled(enableSync);

    transferFunctionProperty->setEnabled(enable);
    tubeRadiusProperty->setEnabled(enable);
    sphereRadiusProperty->setEnabled(enable);
    enableShadowProperty->setEnabled(enable);
    colourShadowProperty->setEnabled(enable);

    useMultiVarTrajectoriesProperty->setEnabled(enable);
    diagramTypeProperty->setEnabled(enable);

    initTimeProperty->setEnabled(
                enable && !(enableSync && synchronizeInitTime));
    startTimeProperty->setEnabled(
                enable && !(enableSync && synchronizeStartTime));
    particlePosTimeProperty->setEnabled(
                enable && !(enableSync && synchronizeParticlePosTime));

    bBoxConnection->getProperty()->setEnabled(enable);
    ensembleSingleMemberProperty->setEnabled(enable && !synchronizeEnsemble);

    // Computation Properties.
    bool enableComputation = enable && !precomputedDataSource;
    computationPropertyGroup->setEnabled(enableComputation);

    enableActorUpdates(true);
}


void MTrajectoryActor::releaseData()
{
    // Release for all seed points or for single precomputed trajectories
    for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
    {
        releaseData(t);
    }
}


void MTrajectoryActor::releaseData(int slot)
{
    // 1. Trajectory data.
    // ===================
    if (trajectoryRequests[slot].trajectories)
    {
        trajectoryRequests[slot].trajectories->releaseVertexBuffer();
        trajectoryRequests[slot].releasePreviousAuxVertexBuffer();
        trajectorySource->releaseData(trajectoryRequests[slot].trajectories);
        trajectoryRequests[slot].trajectories = nullptr;
    }

    // 2. Normals.
    // ===========
    foreach (MSceneViewGLWidget *view,
             MSystemManagerAndControl::getInstance()->getRegisteredViews())
    {
        if (trajectoryRequests[slot].normals.value(view, nullptr))
        {
            trajectoryRequests[slot].normals[view]->releaseVertexBuffer();
            normalsSource->releaseData(trajectoryRequests[slot].normals[view]);
            trajectoryRequests[slot].normals[view] = nullptr;
        }
    }

    // 3. Selection.
    // =============
    if (trajectoryRequests[slot].trajectorySelection)
    {
        trajectoryFilter->releaseData(trajectoryRequests[slot].trajectorySelection);
        trajectoryRequests[slot].trajectorySelection = nullptr;
    }

    // 4. Single time selection.
    // =========================
    if (trajectoryRequests[slot].trajectorySingleTimeSelection)
    {
        trajectoryFilter->releaseData(trajectoryRequests[slot]
                                      .trajectorySingleTimeSelection);
        trajectoryRequests[slot].trajectorySingleTimeSelection = nullptr;
    }

    // 5. Multi-var trajectories.
    // ===========
    if (useMultiVarTrajectories)
    {
        foreach (MSceneViewGLWidget *view,
                 MSystemManagerAndControl::getInstance()->getRegisteredViews())
        {
            if (trajectoryRequests[slot].multiVarTrajectoriesMap.value(view, nullptr))
            {
                trajectoryRequests[slot].multiVarTrajectoriesMap[view]->releaseRenderData();
                multiVarTrajectoriesSource->releaseData(trajectoryRequests[slot].multiVarTrajectoriesMap[view]);
                trajectoryRequests[slot].multiVarTrajectoriesMap[view] = nullptr;
            }
        }
    }
}


} // namespace Met3D
