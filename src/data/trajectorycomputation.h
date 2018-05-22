/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Philipp Kaiser
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

    enum TRAJECTORY_COMPUTATION_ITERATION_METHOD
    {
        EULER,
        RUNGE_KUTTA
    };

    enum TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD
    {
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
        POLE,
        HORIZONTAL,
        BOX,
        VERTICAL
    };

/**
  Stores data that for each trajectory computation has to be computed only once
  and that can be cached as long as the computation is valid.
 */
struct MTrajectoryComputationInfo
{
    MTrajectoryComputationInfo()
            : numTimeSteps(0), numTrajectories(0), numEnsembleMembers(0)
    { }

    unsigned int numTimeSteps, numTrajectories, numEnsembleMembers;

    // QVector3D (lon, lat, pres) * numTimeSteps * numTrajectories
    QVector<QVector<QVector3D>> vertices;

    // All available times.
    QVector<QDateTime> times;

    // Start grid geometry stored in the file.
    std::shared_ptr<MStructuredGrid> startGrid;

    // Mutex to lock access to the struct.
    QMutex accessMutex;
};

/**
  @brief MTrajectoryComputation computes particle trajectories similar to
  LAGRANTO NetCDF based on weather prediction data.

  Vertical level type needs to be the same for all input wind variables.
 */
class MTrajectoryComputation : public MTrajectoryDataSource,
        public MAbstractDataComputation
{
public:
    MTrajectoryComputation(QString identifier);

    ~MTrajectoryComputation();

    void setInputWindVariables(QString eastwardWind_ms, QString northwardWind_ms,
                               QString verticalWind_Pas);

    /**
      Sets vertical level type of computed trajectories.

      Needs to be 3D and the same as the vertical level type of the wind
      component variables (eastwardWind_ms, northwardWind_ms and
      verticalWind_Pas).
     */
    void setVericalLevelType(MVerticalLevelType levelType);

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
                  seedType(POLE),
                  startTimeStep(0),
                  endTimeStep(0),
                  trajectoryCount(0),
                  iterationPerTimeStep(0)
        { }

        QVector<QString> varNames;
        QList<QDateTime> validTimes;
        MDataRequest baseRequest;
        TRAJECTORY_COMPUTATION_ITERATION_METHOD iterationMethod;
        TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD interpolationMethod;
        TRAJECTORY_COMPUTATION_LINE_TYPE lineType;
        TRAJECTORY_COMPUTATION_SEED_TYPE seedType;
        uint startTimeStep;
        uint endTimeStep;
        uint trajectoryCount;
        uint iterationPerTimeStep;
        QVector3D seedMinPosition;
        QVector3D seedMaxPosition;
        QVector3D seedCount;
        QVector2D seedStepSizeLonLat;
        QVector<double> seedPressureLevels;
    };


    /**
      Defines the request keys required by this reader.
     */
    const QStringList locallyRequiredKeys();

    /**
      Fills data source specific fields (init and valid time, ensemble members).
     */
    void initialiseFormDataSource();

    /**
      @brief Computes trajectory for given parameter.

      Performs tasks needed for both computing stream and path lines and
      calls either @ref computeStreamLine() or @ref computePathLine()
      depending on which line type is chosen.
     */
    void computeTrajectory(MDataRequest request,
                           MTrajectoryComputationInfo& cInfo);

    /**
      @brief Computes stream lines for given parameter.

      Is called by @ref computeTrajectory() which performs the tasks needed for
      both stream and path lines.
     */
    void computeStreamLine(TrajectoryComputationHelper& ch,
                           MTrajectoryComputationInfo& cInfo);

    /**
      @brief Computes path lines for given parameter.

      Is called by @ref computeTrajectory() which performs the tasks needed for
      both stream and path lines.
     */
    void computePathLine(TrajectoryComputationHelper& ch,
                         MTrajectoryComputationInfo& cInfo);

    /**
      Converts 3D wind velocity in (m/s, m/s, Pa/s) to units (deg lon, deg lat,
      hPa/s), taking current position on sphere into account.
     */
    QVector3D convertWindVelocityFromMetricToSpherical(
            QVector3D velocity_ms_ms_Pas, QVector3D position_lon_lat_p);

    /* Computation Methods. */
    /**
      Performs Euler integration between @p timePos0 and @p timePos1 on
      @p grids with step size @p deltaT using the interpolation method given by
      @p method to get wind velocity at @p pos.

      @p grids needs to contain eastward wind grid, northward wind grid and
      vertical wind grid in this order.

      If the integration was successfull @p valid will be set to true, false
      otherwise.

     Note: The method performs @ref EULER_ITERATION times the integration using
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

    /* Sample methods. */
    /**
      Samples wind velocity from @p grid using the interpolation method
      @p method and the interpolation factor @p factor.

      @p grids needs to contain eastward wind grid, northward wind grid and
      vertical wind grid in this order.

      If sampling was successfull @p valid will be set to true, false otherwise.
     */
    QVector3D sampleVelocity(QVector3D pos, float factor,
                             TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD method,
                             QVector<QVector<MStructuredGrid*>> &grids,
                             bool& valid);

    /**
      Performs tri-linear intperpolation to get pressure value in @p grid at the
      relative position @p index.
     */
    float pressure(QVector3D index, MStructuredGrid *grid);

    /**
      Performs tri-linear intperpolation to get scalar value in @p grid at the
      relative position @p index.
     */
    float value(QVector3D index, MStructuredGrid *grid);

    /**
      Get indices (lon, lat, pressure) as floating point number representing @p
      pos with respect to position and dimensions of @p grid.

      If an index is out of bounds it is replaced by -1.
     */
    QVector3D findGridIndex(QVector3D pos, MStructuredGrid *grid);

    /**
      Computes linear interpolation between indices of @p grid0 and @p grid1 at
      position @p pos using @p factor as interpolation factor.
     */
    QVector3D interpolatedIndex(QVector3D pos, MStructuredGrid* grid0,
                                MStructuredGrid* grid1, float factor);

    /**
      Computes linear interpolation between values of @p grid0 and @p grid1 at
      index @p index using @p factor as interpolation factor.
     */
    float interpolatedValue(QVector3D index, MStructuredGrid *grid0,
                            MStructuredGrid *grid1, float factor);

    /**
      Computes seeding position for given trajectory.
     */
     QVector3D computeSeedPosition(int trajectory, TrajectoryComputationHelper& ch);

    // Dictionaries of available trajectory data. Access needs to be proteced
    // by the provided read/write lock.
    MTrajectoryInitTimeMap availableTrajectories;
    QSet<unsigned int>     availableMembers;
    QReadWriteLock         availableItemsLock;

    QString windEastwardVariableName;
    QString windNorthwardVariableName;
    QString windVerticalVariableName;
    MVerticalLevelType levelType;

};


} // namespace Met3D

#endif // MET_3D_TRAJECTORYCOMPUTATION_H
