/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2017-2018 Bianca Tost [+]
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
#ifndef TRAJECTORYACTOR_GS_H
#define TRAJECTORYACTOR_GS_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <QtProperty>

// local application imports
#include "gxfw/mactor.h"
#include "gxfw/synccontrol.h"
#include "gxfw/gl/texture.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/boundingbox/boundingbox.h"
#include "data/datarequest.h"
#include "actors/transferfunction1d.h"

#include "data/trajectorydatasource.h"
#include "data/trajectorynormalssource.h"
#include "data/pressuretimetrajectoryfilter.h"
#include "data/trajectorycomputation.h"
#include "data/multivar/multivartrajectories.h"
#include "data/multivar/multivartrajectoriessource.h"
#include "data/multivar/multivardata.h"

#ifdef USE_EMBREE
#include "data/multivar/trajectorypicking.h"
#endif


class MGLResourcesManager;
class MSceneViewGLWidget;


namespace Met3D
{

/**
  @brief MTrajectoryActor renders trajectories of a single ensemble member at a
  single timestep. The trajectories can be rendered as tubes or as spheres
  marking the positions of the airparcels. Spheres can be restricted to a
  single time of the trajectory.
  */
class MTrajectoryActor : public MActor, public MBoundingBoxInterface,
        public MSynchronizedObject
{
    Q_OBJECT

public:
    MTrajectoryActor();

    ~MTrajectoryActor();

    static QString staticActorType() { return "Trajectories"; }

    void reloadShaderEffects();

    /**
      Set a transfer function to map vertical position (pressure) to colour.
      */
    void setTransferFunction(MTransferFunction1D *tf);
    /**
      Set a transfer function by its name. Set to 'none' if @param tfName does
      not exit.
      */
    bool setTransferFunction(QString tfName);

    /**
      Set a transfer function to map a diagram attribute (trajectory parameter
      or statistical information like standard deviation) to colour.
      */
    void setDiagramTransferFunction(MTransferFunction1D *tf);
    /**
      Set a diagram transfer function by its name. Set to 'none' if
      @param tfName does not exit.
      */
    bool setDiagramTransferFunction(QString tfName);

    /**
     Synchronize this actor (time, ensemble) with the synchronization control
     @p sync.
     */
    void synchronizeWith(MSyncControl *sync, bool updateGUIProperties=true);

    bool synchronizationEvent(MSynchronizationType syncType, QVector<QVariant> data);

    /**
      Updates colour hints for synchronization (green property background
      for matching sync, red for not matching sync). If @p scene is specified,
      the colour hints are only updated for the specified scene (e.g. used
      when the variable's actor is added to a new scene).
     */
    void updateSyncPropertyColourHints(MSceneControl *scene = nullptr);

    void setPropertyColour(QtProperty *property, const QColor& colour,
                           bool resetColour, MSceneControl *scene = nullptr);

    /**
     */
    void setDataSourceID(const QString& id)
    { properties->mString()->setValue(utilizedDataSourceProperty, id); }

    void setDataSource(MTrajectoryDataSource* ds);

    void setDataSource(const QString& id);

    void setNormalsSource(MTrajectoryNormalsSource* s);

    void setNormalsSource(const QString& id);

    void setMultiVarTrajectoriesSource(MMultiVarTrajectoriesSource* s);

    void setMultiVarTrajectoriesSource(const QString& id);

    void setTrajectoryFilter(MTrajectoryFilter* f);

    void setTrajectoryFilter(const QString& id);

    void setSynchronizationControl(MSyncControl *synchronizationControl)
    { this->synchronizationControl = synchronizationControl; }

    QString getSettingsID() override { return "TrajectoryActor"; }

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

    void onBoundingBoxChanged() override;

public slots:
    /**
      Programatically change the current ensemble member. If @p member is -1,
      the ensemble mode will be changed to "all".
     */
    bool setEnsembleMember(int member);

    /**
      Set the current trajectory start time and update the scene.
     */
    bool setStartDateTime(const QDateTime& datetime);

    /**
      Set the current particle position time and update the scene.
     */
    bool setParticleDateTime(const QDateTime& datetime);

    /**
      Set the current forecast init time and update the scene.
     */
    bool setInitDateTime(const QDateTime& datetime);


    /**
      Called by the trajectory source when data request by @ref
      asynchronousDataRequest() is ready.
     */
    void asynchronousDataAvailable(MDataRequest request);

    /**
      Called by the normals source when requested normals are ready.
     */
    void asynchronousNormalsAvailable(MDataRequest request);

    /**
      Called by the normals source when requested normals are ready.
     */
    void asynchronousMultiVarTrajectoriesAvailable(MDataRequest request);

    /**
      Called by the trajectory filter when a requested filter computation is
      done.
     */
    void asynchronousSelectionAvailable(MDataRequest request);

    void asynchronousSingleTimeSelectionAvailable(MDataRequest request);

    void prepareAvailableDataForRendering(uint slot);

    /**
      Connects to the MGLResourcesManager::actorCreated() signal. If the new
      actor is a transfer function, it is added to the list of transfer
      functions displayed by the transferFunctionProperty.
     */
    void onActorCreated(MActor *actor);

    /**
      Connects to the MGLResourcesManager::actorDeleted() signal. If the deleted
      actor is a transfer function, update the list of transfer functions
      displayed by the transferFunctionProperty, possibly disconnect from the
      transfer function.
     */
    void onActorDeleted(MActor *actor);

    /**
      Connects to the MGLResourcesManager::actorRenamed() signal. If the renamed
      actor is a transfer function, it is renamed in the list of transfer
      functions displayed by the transferFunctionProperty.
     */
    void onActorRenamed(MActor *actor, QString oldName);

    void registerScene(MSceneControl *scene) override;

    /**
      Since normals of trajectories depend on the scene view the trajectory
      actor is rendered in, it is necessary to trigger a data request if a scene
      view is added to a scene the trajectory actor is connected to.
     */
    void onSceneViewAdded();

    bool isConnectedTo(MActor *actor) override;

    /**
      Connects to the MActor::actorChanged() signal. Updates actor data,
      recomputes trajectories and emits actor change signal if actor updates
      are enabled.
     */
    void onSeedActorChanged();

#ifdef USE_EMBREE
    /**
      Checks whether selectable data of this actor is at
      @p mousePositionX, @p mousePositionY.
      It returns whether a redraw should be triggered.

      Called by a @ref MSceneViewGLWidget.
      */
    bool checkIntersectionWithSelectableData(
            MSceneViewGLWidget *sceneView, QMouseEvent *event) override;

    /**
      Checks whether a virtual (i.e., drawn using OpenGL) window is below
      the mouse cursor at @p mousePositionX, @p mousePositionY.

      Called by a @ref MSceneViewGLWidget.
      */
    bool checkVirtualWindowBelowMouse(
            MSceneViewGLWidget *sceneView, int mousePositionX, int mousePositionY) override;

    void mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;
    void mouseMoveEventParent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;
    void mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;
    void mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;
    void wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event) override;
#endif

protected:
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    void renderShadowsTubes(MSceneViewGLWidget *sceneView, int t);
    void renderShadowsSpheres(MSceneViewGLWidget *sceneView, int t);
    void bindShadowStencilBuffer();
    void unbindShadowStencilBuffer();

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);
    void renderOverlayToCurrentContext(MSceneViewGLWidget *sceneView);

    void updateTimeProperties();

    void updateEnsembleProperties();

    void printDebugOutputOnUserRequest();

    /**
      Outputs currently available trajectory data to an ASCII file in the
      format used by the LAGRANTO model:

      "
      Reference date 20090129_1200 / Time range 2880 min

        time      lon     lat     p
      -----------------------------
        0.00   -59.72   30.36   790
        6.00   -58.22   30.58   795
       12.00   -56.87   30.76   797
       ...
      "

      See https://www.geosci-model-dev.net/8/2569/2015/gmd-8-2569-2015-supplement.pdf
      for details on LAGRANTO output.
     */
    void outputAsLagrantoASCIIFile();

private:
    /**
      Loops over all seed actors and gather the type and region dimensions of
      all actors enabled which are used later to compute positions from which
      trajectories are started.
     */
    void updateActorData();

    /**
      Determine the current time value of the given enum property.
     */
    QDateTime getPropertyTime(QtProperty *enumProperty);

    /** Returns the current ensemble member setting. */
    int getEnsembleMember();

    void setTransferFunctionFromProperty();
    void setDiagramTransferFunctionFromProperty();

    /**
      Request trajectory data, normals and filter for current time and member
      from the data, normal, selection sources.
     */
    void asynchronousDataRequest(bool synchronizationRequest=false);

    /**
      Request a new filter selection.
     */
    void asynchronousSelectionRequest();

    /**
      Update the init time property (init time refers to the base time of the
      forecast on which the trajectory computations are based) from the current
      data source.
     */
    void updateInitTimeProperty();

    /**
      Update the start time property (listing the times at which trajectories
      have been started) from the current init time and the current data source.
     */
    void updateStartTimeProperty();

    /**
      Update the auxiliary data property (listing the auxiliary data variables
      that are available along trajectories) for the current data source.
     */
    void updateAuxDataVarNamesProperty();

    /**
      Update the trajectory time property (available time steps for the current
      trajectory) from the loaded trajectory data. Does not use data from the
      data source.
     */
    void updateParticlePosTimeProperty();

    bool updateEnsembleSingleMemberProperty();

    /**
      Updates the property displaying available integration times for
      trajectory computation from the current trajectories data source and
      "trajectory start time" setting.

      Note: Precomputed datasources do not support that property.
     */
    void updateTrajectoryIntegrationTimeProperty();

    /**
      Internal function containing common code for @ref setStartDateTime() and
      @ref setInitDateTime().
     */
    bool internalSetDateTime(const QList<QDateTime>& availableTimes,
                             const QDateTime& datetime,
                             QtProperty* timeProperty);


    /** Request information */
    struct MRequestQueueInfo { MDataRequest request; bool available; };
    struct MTrajectoryRequestQueueInfo
    {
        MRequestQueueInfo dataRequest;
        QHash<MSceneViewGLWidget*, MRequestQueueInfo> normalsRequests;
        QHash<MSceneViewGLWidget*, MRequestQueueInfo> multiVarTrajectoriesRequests;
        MRequestQueueInfo filterRequest;
        MRequestQueueInfo singleTimeFilterRequest;
        int numPendingRequests;
#ifdef DIRECT_SYNCHRONIZATION
        bool syncchronizationRequest;
        unsigned int requestID;
#endif
    };

    /** Data sources and pointers to current data objects. */
    struct MTrajectoryRequestBuffer
    {
        MTrajectoryRequestBuffer()
                : trajectories(nullptr),
                  trajectorySelection(nullptr),
                  trajectorySingleTimeSelection(nullptr),
                  trajectoriesVertexBuffer(nullptr),
                  trajectoriesAuxDataVertexBuffer(nullptr)
        { }

        MTrajectories *trajectories;
        MTrajectorySelection *trajectorySelection;
        MTrajectorySelection *trajectorySingleTimeSelection;
        GL::MVertexBuffer *trajectoriesVertexBuffer;
        GL::MVertexBuffer *trajectoriesAuxDataVertexBuffer;
        // Remember the name of the current aux var to correctly release
        // the "trajectoriesAuxDataVertexBuffer". Needs to be empty if
        // no aux VB is in use.
        QString currentAuxDataVarName;
        
        QHash<MSceneViewGLWidget*, MTrajectoryNormals*> normals;
        QHash<MSceneViewGLWidget*, GL::MVertexBuffer*> normalsVertexBuffer;

        // Multi-var trajectories.
        QHash<MSceneViewGLWidget*, MMultiVarTrajectories*> multiVarTrajectoriesMap;
        QHash<MSceneViewGLWidget*, MMultiVarTrajectoriesRenderData> multiVarTrajectoriesRenderDataMap;
        QHash<MSceneViewGLWidget*, MTimeStepSphereRenderData*> timeStepSphereRenderDataMap;
        QHash<MSceneViewGLWidget*, MTimeStepRollsRenderData*> timeStepRollsRenderDataMap;

        QQueue<MTrajectoryRequestQueueInfo> pendingRequestsQueue;

        void requestAuxVertexBuffer(QString auxVarName, QString outputParameterName = "")
        {
            // Only allow a new VB if the previous has been released.
            assert(currentAuxDataVarName.isEmpty());

            if (!auxVarName.isEmpty())
            {
                currentAuxDataVarName = auxVarName;
                trajectoriesAuxDataVertexBuffer = trajectories->
                        getAuxDataVertexBuffer(currentAuxDataVarName, outputParameterName);
            }
        }

        void releasePreviousAuxVertexBuffer()
        {
            if (!currentAuxDataVarName.isEmpty())
            {
                trajectories->releaseAuxDataVertexBuffer(currentAuxDataVarName);
                trajectoriesAuxDataVertexBuffer = nullptr;
                currentAuxDataVarName = QString(); // empty string = no VB in use
            }
        }
    };
    QVector<MTrajectoryRequestBuffer> trajectoryRequests;

    /** Counter to keep track of requests for correct handling of sync
        events with multiple seed actors present. See comment in
        asynchronousDataRequest(). */
    unsigned int globalRequestIDCounter;
    QMap<unsigned int, unsigned int> numSeedActorsForRequestEvent;

    MTrajectoryDataSource *trajectorySource;
    MTrajectories *trajectories;
    GL::MVertexBuffer *trajectoriesVertexBuffer;
    GL::MVertexBuffer *trajectoriesAuxDataVertexBuffer;

    MTrajectoryNormalsSource *normalsSource;
    QHash<MSceneViewGLWidget*, MTrajectoryNormals*> normals;
    QHash<MSceneViewGLWidget*, GL::MVertexBuffer*> normalsVertexBuffer;

    MMultiVarTrajectoriesSource* multiVarTrajectoriesSource;
    MMultiVarData multiVarData;
    bool useMultiVarTrajectories;
    bool multiVarDataDirty = false;
    QtProperty *useMultiVarTrajectoriesProperty;
    QtProperty *multiVarGroupProperty;
    bool useMultiVarSpheres = true;

    QStringList diagramTypeNames = {
            "None", "Radar Bar Chart (Time Dependent)", "Radar Bar Chart (Time Independent)",
            "Radar Chart", "Curve-Plot View"
    };
    DiagramDisplayType diagramType = DiagramDisplayType::CURVE_PLOT_VIEW;
    QtProperty *diagramTypeProperty;
    QtProperty *diagramTransferFunctionProperty;
    MTransferFunction1D *diagramTransferFunction = nullptr;

    QStringList diagramNormalizationModeNames = {
            "Global Min/Max", "Selection Min/Max", "Band Min/Max"
    };
    DiagramNormalizationMode diagramNormalizationMode = DiagramNormalizationMode::GLOBAL_MIN_MAX;

    void updateSimilarityMetricGroupEnabled();
    QtProperty *similarityMetricGroup;
    QtProperty *similarityMetricProperty;
    QtProperty *meanMetricInfluenceProperty;
    QtProperty *stdDevMetricInfluenceProperty;
    QtProperty *numBinsProperty;
    QtProperty *sortByDescendingStdDevProperty;
    QtProperty *showMinMaxValueProperty;
    QtProperty *useMaxForSensitivityProperty;
    QtProperty *trimNanRegionsProperty;
    QtProperty *subsequenceMatchingTechniqueProperty;
    QtProperty *springEpsilonProperty;
    QtProperty *backgroundOpacityProperty;
    QtProperty *diagramNormalizationModeProperty;
    QtProperty *diagramTextSizeProperty;
    QtProperty *useCustomDiagramUpscalingFactorProperty;
    QtProperty *diagramUpscalingFactorProperty;

    SimilarityMetric similarityMetric = SimilarityMetric::ABSOLUTE_NCC;
    float meanMetricInfluence = 0.5f;
    float stdDevMetricInfluence = 0.25f;
    int numBins = 10;
    bool showMinMaxValue = true;
    bool useMaxForSensitivity = true;
    bool trimNanRegions = true;
    SubsequenceMatchingTechnique subsequenceMatchingTechnique = SubsequenceMatchingTechnique::SPRING;
    float springEpsilon = 10.0f;
    float backgroundOpacity = 1.0f;
    float diagramTextSize = 8.0f;
    bool useCustomDiagramUpscalingFactor = false;
    float diagramUpscalingFactor = 1.0f;

    QStringList trajectorySyncModeNames = {
            "Time Step", "Time of Ascent", "Height"
    };
    QtProperty *trajectorySyncModeProperty;
    TrajectorySyncMode trajectorySyncMode = TrajectorySyncMode::TIMESTEP;
    uint32_t syncModeTrajectoryIndex = 0;

    bool useVariableToolTip = true;
    bool useVariableToolTipChanged = true;
    QtProperty *useVariableToolTipProperty;
    QtProperty *selectAllTrajectoriesProperty;
    QtProperty *resetVariableSortingProperty;

#ifdef USE_EMBREE
    QHash<MSceneViewGLWidget*, MTrajectoryPicker*> trajectoryPickerMap;
#endif

    MTrajectoryFilter *trajectoryFilter;
    MTrajectorySelection *trajectorySelection;
    MTrajectorySelection *trajectorySingleTimeSelection;

    QtProperty *selectDataSourceProperty;
    QtProperty *utilizedDataSourceProperty;
    QString dataSourceID;

    bool precomputedDataSource; // indicate whether a precomputed dataSource is used

    bool initialDataRequest; // indicator whether the actor loads for the
                             // first time a data source.

    /** Render mode (tubes, spheres, etc.). */
    enum TrajectoryRenderType {
        TRAJECTORY_TUBES     = 0,
        ALL_POSITION_SPHERES = 1,
        SINGLETIME_POSITIONS = 2,
        TUBES_AND_SINGLETIME = 3,
        BACKWARDTUBES_AND_SINGLETIME = 4
    };

    /** Render color mode (pressure, aux-data). */
    enum TrajectoryRenderColorMode {
        PRESSURE = 0,
        AUXDATA  = 1
    };

    TrajectoryRenderType renderMode;
    TrajectoryRenderColorMode renderColorMode;
    QtProperty *renderModeProperty;
    QtProperty *renderColorModeProperty;
    QtProperty *renderAuxDataVarProperty;

    /** Synchronisation with MSyncControl. */
    MSyncControl *synchronizationControl;

    /** Synchronization properties */
    QtProperty *synchronizationPropertyGroup;
    QtProperty *synchronizationProperty;
    bool        synchronizeInitTime;
    QtProperty *synchronizeInitTimeProperty;
    bool        synchronizeStartTime;
    QtProperty *synchronizeStartTimeProperty;
    bool        synchronizeParticlePosTime;
    QtProperty *synchronizeParticlePosTimeProperty;
    bool        synchronizeEnsemble;
    QtProperty *synchronizeEnsembleProperty;

    /** Trajectory Computation properties */
    QtProperty *computationPropertyGroup;
    QtProperty *computationIntegrationTimeProperty;
    QtProperty *computationInterpolationMethodProperty;
    QtProperty *computationLineTypeProperty;
    QtProperty *computationIntegrationMethodProperty;
    QtProperty *computationNumSubTimeStepsProperty;
    QtProperty *computationStreamlineDeltaSProperty;
    QtProperty *computationStreamlineLengthProperty;
    QtProperty *computationSeedPropertyGroup;
    QtProperty *computationSeedAddActorProperty;
    QtProperty *computationRecomputeProperty;

    enum SeedActorType
    {
        POLE_ACTOR,
        HORIZONTAL_ACTOR,
        VERTICAL_ACTOR,
        BOX_ACTOR
    };

    struct SeedActorSettings
    {
        MActor* actor;
        SeedActorType type;
        QtProperty* propertyGroup;
        QtProperty *lonSpacing;
        QtProperty *latSpacing;
        QtProperty *pressureLevels;
        QtProperty *removeProperty;

    };
    QList<SeedActorSettings> computationSeedActorProperties;

    struct SeedActorData
    {
        QVector3D minPosition;
        QVector3D maxPosition;
        QVector2D stepSize;
        QVector<float> pressureLevels;
        TRAJECTORY_COMPUTATION_SEED_TYPE type;
    };
    QVector<SeedActorData> seedActorData;

    /** Time management. */
    QList<QDateTime> availableStartTimes;
    QList<QDateTime> availableInitTimes;
    QList<QDateTime> availableParticlePosTimes;
    QList<unsigned int> availableEnsembleMembersAsSortedList;
    QtProperty *initTimeProperty;
    QtProperty *startTimeProperty;
    QtProperty *particlePosTimeProperty;
    int         particlePosTimeStep;
    int         cachedParticlePosTimeStep = 0;
    bool        loadedParticlePosTimeStepFromSettings = false;

    /** Ensemble management. */
    QtProperty *ensembleModeProperty;
    QtProperty *ensembleSingleMemberProperty;
    /** Trajectory filtering. */
    QtProperty *filtersGroupProperty;
    QtProperty *enableAscentFilterProperty;
    QtProperty *deltaPressureFilterProperty; // filter trajectories according
                                             // to this criterion
    QtProperty *deltaTimeFilterProperty;

    /** Rendering. */
    QtProperty *renderingGroupProperty;

    /** Analysis. */
    QtProperty *analysisGroupProperty;
    QtProperty *outputAsLagrantoASCIIProperty;

    /** GLSL shader objects. */
    std::shared_ptr<GL::MShaderEffect> tubeShader;
    std::shared_ptr<GL::MShaderEffect> tubeShadowShader;
    std::shared_ptr<GL::MShaderEffect> positionSphereShader;
    std::shared_ptr<GL::MShaderEffect> positionSphereShadowShader;
    bool stencilBufferCleared = false;

    QtProperty *transferFunctionProperty;
    /** Pointer to transfer function object and corresponding texture unit. */
    MTransferFunction1D *transferFunction;
    int                 textureUnitTransferFunction;

    /** Render properties: tube/sphere radius in world space */
    QtProperty *tubeRadiusProperty;
    float       tubeRadius;
    QtProperty *sphereRadiusProperty;
    float       sphereRadius;

    QtProperty *enableShadowProperty;
    bool        shadowEnabled;
    QtProperty *colourShadowProperty;
    bool        shadowColoured;

    void debugPrintPendingRequestsQueue();

    /**
      @brief selectDataSource calls a selection dialog to select the data source
      for this trajectory actor.

      Additionally, selectDataSource sets data source, normals source and
      trajectory filter to the selected one if one was selected.

      @return false if the user cancled the selection dialog, no data sources
      are available to select or the user selected no data source or the same
      as already selected, @return true otherwise.
     */
    bool selectDataSource();

    /**
      Opens dialog to select a seed actor from.

      At the moment @ref MMovablePoleActor, @ref MNWPHorizontalSectionActor,
      @ref MNWPVerticalSectionActor and @ref MVolumeBoundingBoxActor can be used
      as seed actor.
     */
    void openSeedActorDialog();

    /**
      Adds an actor (specified by @p name) used to determine trajectories'
      starting points (seed actor) to the trajectory actor.

      @p deltaLon and @p deltaLat set the initial spacing between seeding
      points in horizontal dimensions while @p presLvls holds the initial
      pressure levels used to determine seeding points.

      Note: Returns if actor does not exist or is already registered as
      seed actor for this trajectory actor.
     */
    void addSeedActor(QString name, double deltaLon, double deltaLat,
                      QVector<float> pressureLevels);

    /**
      Removes all seed actors from the trajectory actor.
      */
    void clearSeedActor();

    /**
      Removes the seed actor with the specified @p name from the trajectory
      actor.
      */
    void removeSeedActor(QString name);

    /**
      @brief enableProperties changes the enabled status of all properties to
      @p enable exept for @ref selectDataSourceProperty and
      @ref utilizedDataSourceProperty.

      enableProperties is used to disable all properties if the user selects no
      data source for the trajectory actor to avoid changes in properties to
      cause a program crash.
     */
    void enableProperties(bool enable);
    /**
      @brief releaseData releases all trajectory, normals, selection and single
      time selection data if present.

      releaseData is used to release all data before switching data sources.
     */
    void releaseData();

    /**
      @brief releaseData releases trajectories, normals, selection and single
      time selection data for given @p slot.

     @param slot Index in @ref trajectoryRequests vector of the trajectories
     data to be released.
     */
    void releaseData(int slot);
};


class MTrajectoryActorFactory : public MAbstractActorFactory
{
public:
    MTrajectoryActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override { return new MTrajectoryActor(); }
};


} // namespace Met3D

#endif // TRAJECTORYACTOR_GS_H
