/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2017      Bianca Tost
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
#include "data/datarequest.h"
#include "actors/transferfunction1d.h"

#include "data/trajectorydatasource.h"
#include "data/trajectorynormalssource.h"
#include "data/pressuretimetrajectoryfilter.h"
#include "gxfw/boundingbox/boundingbox.h"


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

    void reloadShaderEffects();

    /**
      Set a transfer function to map vertical position (pressure) to colour.
      */
    void setTransferFunction(MTransferFunction1D *tf);    
    /**
      Set a transfer function by its name. Set to 'none' if @oaram tfName does
      not exit.
      */
    bool setTransferFunction(QString tfName);

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
     Called by the trajectory filter when a requested filter computation is
     done.
     */
    void asynchronousSelectionAvailable(MDataRequest request);

    void asynchronousSingleTimeSelectionAvailable(MDataRequest request);

    void prepareAvailableDataForRendering();

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

protected:
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

    void updateTimeProperties();

    void updateEnsembleProperties();

private:
    /**
      Determine the current time value of the given enum property.
     */
    QDateTime getPropertyTime(QtProperty *enumProperty);

    /** Returns the current ensemble member setting. */
    int getEnsembleMember();

    void setTransferFunctionFromProperty();

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
     Update the trajectory time property (available time steps for the current
     trajectory) from the loaded trajectory data. Does not use data from the
     data source.
     */
    void updateParticlePosTimeProperty();

    /**
      Internal function containing common code for @ref setStartDateTime() and
      @ref setInitDateTime().
      */
    bool internalSetDateTime(const QList<QDateTime>& availableTimes,
                             const QDateTime& datetime,
                             QtProperty* timeProperty);



    /** Data sources and pointers to current data objects. */
    MTrajectoryDataSource *trajectorySource;
    MTrajectories *trajectories;
    GL::MVertexBuffer *trajectoriesVertexBuffer;

    MTrajectoryNormalsSource *normalsSource;
    QHash<MSceneViewGLWidget*, MTrajectoryNormals*> normals;
    QHash<MSceneViewGLWidget*, GL::MVertexBuffer*> normalsVertexBuffer;

    MTrajectoryFilter *trajectoryFilter;
    MTrajectorySelection *trajectorySelection;
    MTrajectorySelection *trajectorySingleTimeSelection;

    QtProperty *selectDataSourceProperty;
    QtProperty *utilizedDataSourceProperty;
    QString dataSourceID;

    bool initialDataRequest; // inidactor whether the actor loads for the
                                   // first time a data source.

    bool suppressUpdate;
    bool normalsToBeComputed; // true if the z-scaling of the scene view has
                              // changed: normals need to be recomputed

    /** Render mode (tubes, spheres, etc.). */
    enum TrajectoryRenderType {
        TRAJECTORY_TUBES     = 0,
        ALL_POSITION_SPHERES = 1,
        SINGLETIME_POSITIONS = 2,
        TUBES_AND_SINGLETIME = 3,
        BACKWARDTUBES_AND_SINGLETIME = 4
    };

    TrajectoryRenderType renderMode;
    QtProperty *renderModeProperty;

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


    /** Time management. */
    QList<QDateTime> availableStartTimes;
    QList<QDateTime> availableInitTimes;
    QList<QDateTime> availableParticlePosTimes;
    QtProperty *initTimeProperty;
    QtProperty *startTimeProperty;
    QtProperty *particlePosTimeProperty;
    int         particlePosTimeStep;

    /** Ensemble management. */
    QtProperty *ensembleModeProperty;
    QtProperty *ensembleMemberProperty;
    /** Trajectory filtering. */
    QtProperty *enableFilterProperty;
    QtProperty *deltaPressureProperty; // filter trajectories according to this
                                       // criterion
    QtProperty *deltaTimeProperty;

    /** GLSL shader objects. */
    std::shared_ptr<GL::MShaderEffect> tubeShader;
    std::shared_ptr<GL::MShaderEffect> tubeShadowShader;
    std::shared_ptr<GL::MShaderEffect> positionSphereShader;
    std::shared_ptr<GL::MShaderEffect> positionSphereShadowShader;

    QtProperty *transferFunctionProperty;
    /** Pointer to transfer function object and cooresponding texture unit. */
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

    struct MRequestQueueInfo { MDataRequest request; bool available; };
    struct MTrajectoryRequestQueueInfo
    {
        MRequestQueueInfo dataRequest;
        QHash<MSceneViewGLWidget*, MRequestQueueInfo> normalsRequests;
        MRequestQueueInfo filterRequest;
        MRequestQueueInfo singleTimeFilterRequest;
        int numPendingRequests;
#ifdef DIRECT_SYNCHRONIZATION
        bool syncchronizationRequest;
#endif
    };
    QQueue<MTrajectoryRequestQueueInfo> pendingRequestsQueue;

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
