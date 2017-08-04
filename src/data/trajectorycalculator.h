/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2017 Philipp Kaiser
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

#ifndef MET_3D_TRAJECTORYCALCULATOR_H
#define MET_3D_TRAJECTORYCALCULATOR_H

// standard library imports

// related third party imports
#include <QtCore>
#include <netcdf>

// local application imports
#include "data/abstractdatacalculator.h"
#include "data/trajectoryreader.h"


namespace Met3D
{

    enum TRAJ_CALC_ITERATION_METHOD
    {
        EULER,
        RUNGE_KUTTA
    };

    enum TRAJ_CALC_INTERPOLATION_METHOD
    {
        LAGRANTO_INTERPOLATION,
        MET3D_INTERPOLATION
    };

    enum TRAJ_CALC_LINE_TYPE
    {
        PATH_LINE,
        STREAM_LINE
    };

    enum TRAJ_CALC_SEED_TYPE
    {
        POLE,
        HORIZONTAL,
        BOX,
        VERTICAL
    };

/**
  Data that for each trajectory calculation have to be calculated only once and that can
  be cached as long as the calculation is valid.
*/
struct MTrajectoryCalculationInfo
{
    MTrajectoryCalculationInfo()
            : numTimeSteps(0), numTrajectories(0), numEnsembleMembers(0)
    { }

    unsigned int numTimeSteps, numTrajectories, numEnsembleMembers;

    // QVector3D (lon, lat, pres) * numTimeSteps * numTrajectories
    QVector<QVector<QVector3D>> vertices;

    // all available times
    QVector<QDateTime> times;

    // Start grid geometry stored in the file.
    std::shared_ptr<MStructuredGrid> startGrid;

    // Mutex to lock access to the struct.
    QMutex accessMutex;
};

/**
@brief MTrajectoryCalculator calculates particle trajectories similar to LAGRANTO
NetCDF based on based on weather prediction data.
*/
class MTrajectoryCalculator : public MTrajectoryDataSource, public MAbstractDataCalculator
{
public:
    MTrajectoryCalculator(QString identifier);

    ~MTrajectoryCalculator();

    void setUVPVariables(QString u, QString v, QString p);
    void setVericalLevelType(QString levelTypeString);

    /**
     * Returns a @ref QList<QDateTime> containing the the available forecast
     * initialisation times (base times).
    */
    QList<QDateTime> availableInitTimes();

    /**
     * Returns a @ref QList<QDateTime> containing the the trajectory start times
     * available for the specified initialisation time @p initTime.
    */
    QList<QDateTime> availableValidTimes(const QDateTime& initTime);

    /**
     * For a given init and valid time, returns the valid (=start) times of
     * those trajectories that overlap with the given valid time.
    */
    QList<QDateTime> validTimeOverlap(const QDateTime& initTime, const QDateTime& validTime);

    QSet<unsigned int> availableEnsembleMembers();

    MTrajectories* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

protected:
    // helper construct for calculation
    struct CalculationHelper
    {
        CalculationHelper()
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
        TRAJ_CALC_ITERATION_METHOD iterationMethod;
        TRAJ_CALC_INTERPOLATION_METHOD interpolationMethod;
        TRAJ_CALC_LINE_TYPE lineType;
        TRAJ_CALC_SEED_TYPE seedType;
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
     * Define the request keys required by this reader.
     */
    const QStringList locallyRequiredKeys();

    /**
     * Check data source and fill data source specific fields like init and valid time
     */
    void checkDataSource();

    /**
     * Calculate trajectory for given parameter
     */
    void calculateTrajectory(MDataRequest request, MTrajectoryCalculationInfo& cInfo);
    void calculateStreamLine(CalculationHelper& ch, MTrajectoryCalculationInfo& cInfo);
    void calculatePathLine(CalculationHelper& ch, MTrajectoryCalculationInfo& cInfo);


    /**
     * Convert velocity from m/s to (lon,lat,p)/s
     */
    QVector3D convertVelocity(QVector3D v, QVector3D pos);

    /**
     * Calculation Methods
     */
    QVector3D euler(QVector3D pos, float deltat, float factor0, float factor1, TRAJ_CALC_INTERPOLATION_METHOD method, QVector<QVector<MStructuredGrid*>> &grids, bool& valid);
    QVector3D rungeKutta(QVector3D pos, float deltat, float factor0, float factor1, TRAJ_CALC_INTERPOLATION_METHOD method, QVector<QVector<MStructuredGrid*>> &grids, bool& valid);

    /**
     * Sample methods
     */
    QVector3D sampleVelocity(QVector3D pos, float factor, TRAJ_CALC_INTERPOLATION_METHOD method, QVector<QVector<MStructuredGrid *>> &grids, bool& valid);
    float pressure(QVector3D index, MStructuredGrid* grid);
    float value(QVector3D index, MStructuredGrid* grid);
    QVector3D findGridIndex(QVector3D pos, MStructuredGrid* grid);
    QVector3D interpolatedIndex(QVector3D pos, MStructuredGrid* grid0, MStructuredGrid* grid1, float factor);
    float interpolatedValue(QVector3D index, MStructuredGrid* grid0, MStructuredGrid* grid1, float factor);

    /**
     * Calculate Seeding position for given trajectory
     */
     QVector3D calculateSeedPosition(int trajectory, CalculationHelper& ch);

    // Dictionaries of available trajectory data. Access needs to be proteced
    // by the provided read/write lock.
    MTrajectoryInitTimeMap availableTrajectories;
    QSet<unsigned int>     availableMembers;
    QReadWriteLock         availableItemsLock;

    QString uVariableName;
    QString vVariableName;
    QString pVariableName;
    MVerticalLevelType levelType;

};


} // namespace Met3D

#endif //MET_3D_TRAJECTORYCALCULATOR_H
