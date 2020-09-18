/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017      Philipp Kaiser [+]
**  Copyright 2017-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2020      Marcel Meyer [*]
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
#ifndef MET_3D_TRAJECTORYCOMPUTATION_H
#define MET_3D_TRAJECTORYCOMPUTATION_H

// standard library imports

// related third party imports
#include <QtCore>
#include <netcdf>

// local application imports
#include "data/abstractdatacomputation.h"
#include "data/trajectoryreader.h"


namespace Met3D
{

enum TRAJECTORY_COMPUTATION_INTEGRATION_METHOD
{
    EULER,
    RUNGE_KUTTA
};

enum TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD
{
    // NOTE: See Philipp Kaiser's master's thesis (TUM 2017) for details on the
    // different interpolation approaches. The LAGRANTO_INTERPOLATION method
    // follows the implementation of LAGRANTO v2
    // (http://dx.doi.org/10.5194/gmd-8-2569-2015).
    LAGRANTO_INTERPOLATION,
    MET3D_INTERPOLATION
};

enum TRAJECTORY_COMPUTATION_LINE_TYPE
{
    PATH_LINE,
    STREAM_LINE
};

enum TRAJECTORY_COMPUTATION_SEED_TYPE
{
    VERTICAL_POLE,
    HORIZONTAL_SECTION,
    VOLUME_BOX,
    VERTICAL_SECTION
};


/**
  Stores data that for each trajectory computation has to be computed only once
  and that can be cached as long as the computation is valid.
 */
struct MTrajectoryComputationInfo
{
    MTrajectoryComputationInfo()
        : numStoredVerticesPerTrajectory(0),
          numTrajectories(0),
          numEnsembleMembers(0)
    { }

    unsigned int numStoredVerticesPerTrajectory, numTrajectories, numEnsembleMembers;

    // QVector3D (lon, lat, pres) * numStoredVerticesPerTrajectory * numTrajectories
    QVector<QVector<QVector3D>> vertices;

    // Times corresponding to the trajectory vertices.
    QVector<QDateTime> times;

    // Start grid geometry (i.e., seed points).
    std::shared_ptr<MStructuredGrid> startGrid;

    // Array for storing auxiliary data along trajectories:
    // - 1-Dim: number of trajectories,
    // - 2-Dim: vertices/time-steps per trajectory,
    // - 3-Dim: auxiliary data.
    QVector<QVector<QVector<float>>> auxDataAtVertices;

    // List with auxiliary data variable names.
    QStringList auxDataVarNames;    

    // Mutex to lock access to the struct.
    QMutex accessMutex;
};


/**
  @brief MTrajectoryComputationSource computes particle trajectories (path lines)
  and stream lines. Computation is implemented following the implementation of
  the LAGRANTO model (http://dx.doi.org/10.5194/gmd-8-2569-2015).

  @note Vertical level type needs to be the same for all input wind variables.
 */
class MTrajectoryComputationSource : public MTrajectoryDataSource,
        public MAbstractDataComputationSource
{
public:
    MTrajectoryComputationSource(QString identifier);

    ~MTrajectoryComputationSource();

    /**
      Specify the vertical level type of the used input wind fields (cf.
      @ref setInputWindVariables()).

      @note Needs to be a 3D level type.
     */
    void setVerticalLevelType(MVerticalLevelType levelType);

    /**
      Specify variable names of the three wind components used to compute the
      field lines. Names refer to variable names in the data source set by @ref
      setInputSource() and the level type set by @ref setVerticalLevelType().
     */
    void setInputWindVariables(
            QString eastwardWind_ms, QString northwardWind_ms,
            QString verticalWind_Pas);

    /**
      Specify variable names of the auxiliary data fields sampled
      along trajectories.
     */
    void setAuxDataVariables(QString);

    /**
      Specify the vertical level types of auxiliary data variables sampled
      along trajectories.
     */
    void setVerticalLevelsOfAuxDataVariables(
            QMap<QString, MVerticalLevelType> verticalLevelsOfAuxDataVars);

    /**
      Returns a @ref QList<QDateTime> containing the available forecast
      initialisation times (base times).
     */
    QList<QDateTime> availableInitTimes();

    /**
      Returns a @ref QList<QDateTime> containing the trajectory start times
      available for the specified initialisation time @p initTime.
     */
    QList<QDateTime> availableValidTimes(const QDateTime& initTime);

    /**
      For a given init and valid time, returns the valid (=start) times of
      those trajectories that overlap with the given valid time.
     */
    QList<QDateTime> validTimeOverlap(const QDateTime& initTime,
                                      const QDateTime& validTime);

    QSet<unsigned int> availableEnsembleMembers();

    QStringList availableAuxiliaryVariables();

    MTrajectories* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

protected:
    // Helper construct for computation.
    struct TrajectoryComputationHelper
    {
        TrajectoryComputationHelper()
                : varNames(3, QString()),
                  iterationMethod(EULER),
                  lineType(PATH_LINE),
                  seedType(VERTICAL_POLE),
                  startTimeStep(0),
                  endTimeStep(0),
                  trajectoryCount(0),
                  subTimeStepsPerDataTimeStep(0),
                  streamlineDeltaS(1.),
                  streamlineLength(1),
                  auxVarNames(QStringList())
        { }

        QVector<QString> varNames;
        QStringList auxVarNames;
        QList<QDateTime> validTimes;
        MDataRequest baseRequest;
        TRAJECTORY_COMPUTATION_INTEGRATION_METHOD iterationMethod;
        TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD interpolationMethod;
        TRAJECTORY_COMPUTATION_LINE_TYPE lineType;
        TRAJECTORY_COMPUTATION_SEED_TYPE seedType;
        uint startTimeStep;
        uint endTimeStep;
        uint trajectoryCount;
        uint subTimeStepsPerDataTimeStep;
        double streamlineDeltaS; // delta of parameter "s" that parameterises
                                 // the streamline: dx(s)/ds = v(x)
        int streamlineLength;
        QVector3D seedMinPosition;
        QVector3D seedMaxPosition;
        QVector3D seedCount;
        QVector2D seedStepSizeHorizontalLonLat;
        QVector<double> seedPressureLevels;
    };

    const QStringList locallyRequiredKeys();

    /**
      Fills data source specific fields (init and valid time, ensemble members).
     */
    void initialiseFromDataSource();

    /**
      Computes the field lines for the specified parameters.

      Performs tasks needed for both computing stream and path lines and
      calls either @ref computeStreamLine() or @ref computePathLine()
      depending on which line type is chosen.
     */
    void computeTrajectories(MDataRequest request,
                             MTrajectoryComputationInfo& cInfo);

    /**
      Computes streamlines.

      Is called by @ref computeTrajectories() if streamlines are requested.
     */
    void computeStreamLines(TrajectoryComputationHelper& ch,
                            MTrajectoryComputationInfo& cInfo);

    /**
      Computes path lines (i.e., trajectories).

      Is called by @ref computeTrajectories() if path lines are requested.
     */
    void computePathLines(TrajectoryComputationHelper& ch,
                          MTrajectoryComputationInfo& cInfo);

    /**
      Converts 3D wind velocity in (m/s, m/s, Pa/s) to units (deg lon, deg lat,
      hPa/s), taking current position on sphere into account.
     */
    QVector3D convertWindVelocityFromMetricToSpherical(
            QVector3D velocity_ms_ms_Pas, QVector3D position_lon_lat_p);

    /* Methods for Euler and Runge-Kutta integration. */
    /**
      Performs Euler integration between @p timePos0 and @p timePos1 on
      @p grids with step size @p deltaT using the interpolation method given by
      @p method to get wind velocity at @p pos.

      @p grids needs to contain eastward wind grid, northward wind grid and
      vertical wind grid in this order.

      If the integration was successfull @p valid will be set to true, false
      otherwise.

      @note The method performs @ref EULER_ITERATION times the integration using
      the results of the previous integration.
     */
    QVector3D trajectoryIntegrationEuler(
            QVector3D pos, float deltaT, float timePos0, float timePos1,
            TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD method,
            QVector<QVector<MStructuredGrid*>> &grids, bool& valid);

    /**
      Performs Runge-Kutta integration between @p timePos0 and @p timePos1 on
      @p grids with step size @p deltaT using the interpolation method given by
      @p method to get wind velocity at @p pos.

      @p grids needs to contain eastward wind grid, northward wind grid and
      vertical wind grid in this order.

      If the integration was successfull @p valid will be set to true, false
      otherwise.
     */
    QVector3D trajectoryIntegrationRungeKutta(
            QVector3D pos, float deltaT, float timePos0, float timePos1,
            TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD method,
            QVector<QVector<MStructuredGrid*>> &grids, bool& valid);

    /* Sampling methods. */
    /**
      Samples wind velocity at @p pos in 3D space and time from @p grids using
      the interpolation method @p method and the time interpolation
      factor @p timeInterpolationValue (0..1).

      @p grids is a two-dimensional list of grids that needs to contain two
      timesteps of eastward wind grid, northward wind grid and vertical wind
      grid (in this order).

      If sampling was successfull @p valid will be set to true, false otherwise.

      @note The different approaches to 3D space/time interpolation are
      documented in Philipp Kaiser's master's thesis (TUM 2017).
     */
    QVector3D sampleVelocity3DSpaceTime(
            QVector3D pos, float timeInterpolationValue,
            TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD method,
            QVector<QVector<MStructuredGrid*>> &grids, bool& valid);

    /**
      Samples auxiliary data (e.g. humidity) at @p pos in 3D space and
      time from @p grids using the interpolation method @p and the time
      interpolation factor @p timeInterpolationValue (0..1).

      @p grids is a 2-D list of data grids that needs to contain two
      timesteps of the data field that is sampled at pos. The indexAuxVar
      contains the index required to pick the correct data grid from the
      list of grids.

      If sampling was successfull @p valid will be set to true, else false.

      @note The different approaches to 3D space/time interpolation are
      documented in Philipp Kaiser's master's thesis (TUM 2017).
     */
     float sampleAuxDataAtTrajectoryVertex(
            QVector3D pos, float timeInterpolationValue,
            TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD method,
            QVector<QVector<MStructuredGrid*>> &grids, uint indexAuxVar,
            bool& valid);

    /**
      Performs tri-linear intperpolation to obtain the pressure value of
      @p grid at the float index position @p index.
     */
    float samplePressureAtFloatIndex(QVector3D index, MStructuredGrid *grid);

    /**
      Performs tri-linear intperpolation to obtain the data value of
      @p grid at the float index position @p index.
     */
    float sampleDataValueAtFloatIndex(QVector3D index, MStructuredGrid *grid);

    /**
      Determine indices (of lon, lat, pressure) as floating point numbers
      representing @p pos with respect to position and dimensions of @p grid.

      If an index is out of bounds it is replaced by -1.
     */
    QVector3D floatIndexAtPos(QVector3D pos, MStructuredGrid *grid);

    /**
      Determines the float indices at position @p pos in @p grid0 and @p grid1,
      then interpolates these indices linearly in time using the time using
      the time interpolation factor @p timeInterpolationValue.

      @note The different approaches to 3D space/time interpolation are
      documented in Philipp Kaiser's master's thesis (TUM 2017).
     */
    QVector3D floatIndexAtPosInterpolatedInTime(
            QVector3D pos, MStructuredGrid* grid0,
            MStructuredGrid* grid1, float timeInterpolationValue);

    /**
      For a pre-computed float index, first samples the data values in both
      grids @p grid0 and @p grid1 using @ref sampleDataValueAtFloatIndex(),
      then linearly interpolates the two values in time using the time
      interpolation factor @p timeInterpolationValue.

      @note The different approaches to 3D space/time interpolation are
      documented in Philipp Kaiser's master's thesis (TUM 2017).
     */
    float sampleDataValueAtFloatIndexAndInterpolateInTime(
            QVector3D index, MStructuredGrid *grid0,
            MStructuredGrid *grid1, float timeInterpolationValue);

    /**
      Determines the seed position for a given trajectory.
     */
    QVector3D determineTrajectorySeedPosition(
            int trajectory, TrajectoryComputationHelper& ch);

    // Dictionaries of available trajectory data. Access needs to be proteced
    // by the provided read/write lock.
    MTrajectoryInitTimeMap availableTrajectories;
    QSet<unsigned int>     availableMembers;
    QReadWriteLock         availableItemsLock;

    QString windEastwardVariableName;
    QString windNorthwardVariableName;
    QString windVerticalVariableName;
    MVerticalLevelType levelType;
    QStringList auxDataVarNames;
    QMap<QString, MVerticalLevelType> verticalLevelsOfAuxDataVariables;
};


} // namespace Met3D

#endif // MET_3D_TRAJECTORYCOMPUTATION_H
