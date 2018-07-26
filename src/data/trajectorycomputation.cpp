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
#include "trajectorycomputation.h"

// standard library imports

// related third party imports

// local application imports

using namespace std;

#define EULER_ITERATION 3

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryComputationSource::MTrajectoryComputationSource(QString identifier)
        : MTrajectoryDataSource(),
          MAbstractDataComputationSource(identifier)
{
    levelType = HYBRID_SIGMA_PRESSURE_3D;
}

MTrajectoryComputationSource::~MTrajectoryComputationSource()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MTrajectoryComputationSource::setVerticalLevelType(
        MVerticalLevelType levelType)
{
    this->levelType = levelType;
}


void MTrajectoryComputationSource::setInputWindVariables(
        QString eastwardWind_ms, QString northwardWind_ms,
        QString verticalWind_Pas)
{
    windEastwardVariableName = eastwardWind_ms;
    windNorthwardVariableName = northwardWind_ms;
    windVerticalVariableName = verticalWind_Pas;
}


QList<QDateTime> MTrajectoryComputationSource::availableInitTimes()
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    return availableTrajectories.keys();
}


QList<QDateTime> MTrajectoryComputationSource::availableValidTimes(
        const QDateTime& initTime)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableTrajectories.keys().contains(initTime))
        throw MBadDataFieldRequest(
            "unkown init time requested: " +
            initTime.toString(Qt::ISODate).toStdString(),
            __FILE__, __LINE__);
    return availableTrajectories.value(initTime).keys();
}


QList<QDateTime> MTrajectoryComputationSource::validTimeOverlap(
        const QDateTime& initTime, const QDateTime& validTime)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableTrajectories.keys().contains(initTime))
        throw MBadDataFieldRequest(
            "unkown init time requested: " +
            initTime.toString(Qt::ISODate).toStdString(),
            __FILE__, __LINE__);

    if (!availableTrajectories.value(initTime).keys().contains(validTime))
        throw MBadDataFieldRequest(
            "unkown valid time requested: " +
            validTime.toString(Qt::ISODate).toStdString(),
            __FILE__, __LINE__);
    return availableTrajectories.value(initTime).value(validTime)
            .validTimeOverlap;
}


QSet<uint> MTrajectoryComputationSource::availableEnsembleMembers()
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    return availableMembers;
}


MTrajectories* MTrajectoryComputationSource::produceData(MDataRequest request)
{
#define MSTOPWATCH_ENABLED
#ifdef MSTOPWATCH_ENABLED
    MStopwatch stopwatch;
#endif

    // Read request parameters.
    MDataRequestHelper rh(request);
    QDateTime initTime  = rh.timeValue("INIT_TIME");
    QDateTime validTime = rh.timeValue("VALID_TIME");
    QDateTime endTime = rh.timeValue("END_TIME");
    uint member = rh.intValue("MEMBER");
    QString timeSpan    = rh.value("TIME_SPAN");

    // If only a specified time interval of the trajectories should be read
    // (timeSpan != ALL), timeSpan contains either a single time or a time
    // interval separated by a "/".
    QDateTime startTime, stopTime;
    if (timeSpan != "ALL")
    {
        QStringList args = timeSpan.split("/");
        startTime = QDateTime::fromString(args[0], Qt::ISODate);
        if (args.size() > 1)
        {
            stopTime = QDateTime::fromString(args[1], Qt::ISODate);
        }
        else
        {
            stopTime = startTime;
        }
    }

    LOG4CPLUS_DEBUG(
                mlog, "Starting computation of trajectories/streamlines. "
                      "Forecast IT="
                << initTime.toString(Qt::ISODate).toStdString()
                << ", trajectory start="
                << validTime.toString(Qt::ISODate).toStdString()
                << ", trajectory end="
                << endTime.toString(Qt::ISODate).toStdString()
                << " (i.e., "
                << validTime.secsTo(endTime) / 3600.
                << " hours), ensemble member=" << member
                << ", using time interval=("
                << startTime.toString(Qt::ISODate).toStdString() << "/"
                << stopTime.toString(Qt::ISODate).toStdString() << ")");

    // Check validity of initTime, startTime and member.
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableTrajectories.keys().contains(initTime))
    {
        throw MBadDataFieldRequest("unkown init time requested: "
                                   + initTime.toString(Qt::ISODate).toStdString(),
                                   __FILE__, __LINE__);
    }
    else if (!availableTrajectories.value(initTime).keys().contains(validTime))
    {
        throw MBadDataFieldRequest("unkown start time requested: "
                                   + validTime.toString(Qt::ISODate).toStdString(),
                                   __FILE__, __LINE__);
    }
    else if (!availableTrajectories.value(initTime).keys().contains(endTime))
    {
        throw MBadDataFieldRequest("unkown end time requested: "
                                   + endTime.toString(Qt::ISODate).toStdString(),
                                   __FILE__, __LINE__);
    }
    else if (!availableMembers.contains(member))
    {
        throw MBadDataFieldRequest("invalid ensemble member requested",__FILE__,
                                   __LINE__);
    }
    availableItemsReadLocker.unlock();

    // Compute.
    MTrajectoryComputationInfo cInfo;
    computeTrajectories(request, cInfo);

    // Create the trajectory data struct and fill content. This value is
    // returned by this function.
    Met3D::MTrajectories* trajectories = new Met3D::MTrajectories(
                cInfo.numTrajectories, cInfo.times);
    trajectories->setMetaData(initTime, validTime, "MET3DCOMPUTED_trajectories",
                              member);
    trajectories->copyVertexDataFrom(cInfo.vertices);
    trajectories->setStartGrid(cInfo.startGrid);

    LOG4CPLUS_DEBUG(mlog, "Computation of trajectories/streamlines finished.");
#ifdef MSTOPWATCH_ENABLED
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "Single ensemble member trajectories computed in "
                    << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif

    return trajectories;
}


MTask* MTrajectoryComputationSource::createTaskGraph(MDataRequest request)
{
    assert(dataSource != nullptr);

    // Create a new task.
    MTask* task = new MTask(request, this);
    MDataRequestHelper rh(request);

    // Remove variables used by this instance.
    rh.remove("TIME_SPAN");
    rh.remove("LINE_TYPE");
    rh.remove("INTEGRATION_METHOD");
    rh.remove("INTERPOLATION_METHOD");
    rh.remove("SEED_TYPE");
    rh.remove("SUBTIMESTEPS_PER_DATATIMESTEP");
    rh.remove("STREAMLINE_DELTA_S");
    rh.remove("STREAMLINE_LENGTH");
    rh.remove("SEED_MIN_POSITION");
    rh.remove("SEED_MAX_POSITION");
    rh.remove("SEED_STEP_SIZE_LON_LAT");
    rh.remove("SEED_PRESSURE_LEVELS");

    // Get parameters from request.
    QDateTime initTime = rh.timeValue("INIT_TIME");
    QDateTime validTime = rh.timeValue("VALID_TIME");
    QDateTime endTime = rh.timeValue("END_TIME");
    QStringList variables = QStringList()
            << windEastwardVariableName << windNorthwardVariableName
            << windVerticalVariableName;

    // Get valid times for given init time.
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    QList<QDateTime> validTimes = availableTrajectories.value(initTime).keys();
    availableItemsReadLocker.unlock();

    // Get indices.
    int startIndex = validTimes.indexOf(validTime);
    int endIndex = validTimes.indexOf(endTime);

    // Request resources required for computation.
    for (QString variable : variables)
    {
        for (int i = min(startIndex, endIndex);
             i <= max(startIndex, endIndex); i++)
        {
            rh.insert("VALID_TIME", validTimes.at(i));
            rh.insert("VARIABLE", variable);
            rh.insert("LEVELTYPE", levelType);
            task->addParent(dataSource->getTaskGraph(rh.request()));
        }
    }

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTrajectoryComputationSource::initialiseFromDataSource()
{
    QWriteLocker availableItemsWriteLocker(&availableItemsLock);

    // Store init and valid times.
    QList<QDateTime> initTimes = dataSource->availableInitTimes(
                levelType, windEastwardVariableName);
    for (QDateTime& initTime : initTimes)
    {
        QList<QDateTime> validTimes = dataSource->availableValidTimes(
                    levelType, windEastwardVariableName, initTime);
        for (QDateTime validTime : validTimes)
        {
            availableTrajectories[initTime][validTime].filename = QString();
            availableTrajectories[initTime][validTime].isStartTime = true;
        }
    }

    // Store available ensemble members.
    availableMembers = dataSource->availableEnsembleMembers(
                levelType, windEastwardVariableName);
}


const QStringList MTrajectoryComputationSource::locallyRequiredKeys()
{
    return (QStringList() << "INIT_TIME" << "VALID_TIME" << "END_TIME" <<
            "MEMBER" << "TIME_SPAN" << "SUBTIMESTEPS_PER_DATATIMESTEP" <<
            "STREAMLINE_DELTA_S" << "STREAMLINE_LENGTH" << "LINE_TYPE" <<
            "INTEGRATION_METHOD" << "INTERPOLATION_METHOD" << "SEED_TYPE" <<
            "SEED_MIN_POSITION" << "SEED_MAX_POSITION" <<
            "SEED_STEP_SIZE_LON_LAT" << "SEED_PRESSURE_LEVELS");
}


void MTrajectoryComputationSource::computeTrajectories(
        MDataRequest request, MTrajectoryComputationInfo& cInfo)
{
    // A) Process request
    // ===============================
    // Read parameters from incoming request string.

    // Request data of all variables.
    MDataRequestHelper rh(request);
    QDateTime initTime = rh.timeValue("INIT_TIME");
    QDateTime validTime = rh.timeValue("VALID_TIME");
    QDateTime endTime = rh.timeValue("END_TIME");
    TRAJECTORY_COMPUTATION_INTEGRATION_METHOD integrationMethod =
            TRAJECTORY_COMPUTATION_INTEGRATION_METHOD(
                rh.intValue("INTEGRATION_METHOD"));
    TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD  interpolationMethod =
            TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD(
                rh.intValue("INTERPOLATION_METHOD"));
    TRAJECTORY_COMPUTATION_LINE_TYPE lineType =
            TRAJECTORY_COMPUTATION_LINE_TYPE(rh.intValue("LINE_TYPE"));
    TRAJECTORY_COMPUTATION_SEED_TYPE seedType =
            TRAJECTORY_COMPUTATION_SEED_TYPE(rh.intValue("SEED_TYPE"));
    uint iterationPerTimeStep = rh.intValue("SUBTIMESTEPS_PER_DATATIMESTEP");
    double streamlineDeltaT = rh.doubleValue("STREAMLINE_DELTA_S");
    int streamlineLength = rh.intValue("STREAMLINE_LENGTH");
    QStringList seedMinPositionList = rh.value("SEED_MIN_POSITION").split("/");
    QStringList seedMaxPositionList = rh.value("SEED_MAX_POSITION").split("/");
    QStringList seedStepSizeList = rh.value("SEED_STEP_SIZE_LON_LAT").split("/");
    QStringList seedPressureLevelsList =
            rh.value("SEED_PRESSURE_LEVELS").split("/");

    QVector3D seedMinPosition = QVector3D(seedMinPositionList.at(0).toDouble(),
                                          seedMinPositionList.at(1).toDouble(),
                                          seedMinPositionList.at(2).toDouble());
    QVector3D seedMaxPosition = QVector3D(seedMaxPositionList.at(0).toDouble(),
                                          seedMaxPositionList.at(1).toDouble(),
                                          seedMaxPositionList.at(2).toDouble());
    QVector2D seedStepSize = QVector2D(seedStepSizeList.at(0).toDouble(),
                                       seedStepSizeList.at(1).toDouble());
    QVector<double> seedPressureLevels(seedPressureLevelsList.size());
    for (int i = 0; i < seedPressureLevels.size(); i++)
    {
        seedPressureLevels[i] = seedPressureLevelsList.at(i).toDouble();
    }

    // Change request to obtain base request for NWP data access.
    rh.remove("END_TIME");
    rh.remove("TIME_SPAN");
    rh.remove("LINE_TYPE");
    rh.remove("INTEGRATION_METHOD");
    rh.remove("INTERPOLATION_METHOD");
    rh.remove("SEED_TYPE");
    rh.remove("SUBTIMESTEPS_PER_DATATIMESTEP");
    rh.remove("STREAMLINE_DELTA_S");
    rh.remove("STREAMLINE_LENGTH");
    rh.remove("SEED_MIN_POSITION");
    rh.remove("SEED_MAX_POSITION");
    rh.remove("SEED_STEP_SIZE_LON_LAT");
    rh.remove("SEED_PRESSURE_LEVELS");
    rh.insert("LEVELTYPE", levelType);

    // Determine available valid times.
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    QList<QDateTime> validTimes = availableTrajectories.value(initTime).keys();
    availableItemsReadLocker.unlock();

    // B) Initialization
    // ===============================
    // Initialize a data struct of type TrajectoryComputationHelper that
    // contains all variables required for further processing.

    TrajectoryComputationHelper helper;
    helper.baseRequest = rh.request();
    helper.varNames[0] = windEastwardVariableName;
    helper.varNames[1] = windNorthwardVariableName;
    helper.varNames[2] = windVerticalVariableName;
    helper.validTimes = validTimes;
    helper.iterationMethod = integrationMethod;
    helper.interpolationMethod = interpolationMethod;
    helper.startTimeStep = validTimes.indexOf(validTime);
    helper.endTimeStep = validTimes.indexOf(endTime);
    helper.subTimeStepsPerDataTimeStep = iterationPerTimeStep;
    helper.streamlineDeltaS = streamlineDeltaT;
    helper.streamlineLength = streamlineLength;
    helper.seedType = seedType;
    helper.seedMinPosition = seedMinPosition;
    helper.seedMaxPosition = seedMaxPosition;
    helper.seedStepSizeHorizontalLonLat = seedStepSize;
    helper.seedPressureLevels = seedPressureLevels;
    switch (helper.seedType)
    {
        case VERTICAL_POLE:
        case HORIZONTAL_SECTION:
        case VOLUME_BOX:
            helper.seedCount = QVector3D(
                static_cast<uint>(abs(seedMaxPosition.x() - seedMinPosition.x())
                                  / seedStepSize.x() + 1.0),
                static_cast<uint>(abs(seedMaxPosition.y() - seedMinPosition.y())
                                  / seedStepSize.y() + 1.0),
                seedPressureLevels.size());

            // Initialize size and values of the start grid if type is pole,
            // horizontal or box.
            cInfo.startGrid = std::shared_ptr<MStructuredGrid>(
                        new MRegularLonLatStructuredPressureGrid(
                            helper.seedCount.z(), helper.seedCount.y(),
                            helper.seedCount.x()));
            for (int lon = 0; lon < helper.seedCount.x(); lon++)
            {
                cInfo.startGrid->lons[lon] = seedMinPosition.x()
                        + (seedStepSize.x() * lon);
            }
            for (int lat = 0; lat < helper.seedCount.y(); lat++)
            {
                cInfo.startGrid->lats[lat] = seedMinPosition.y()
                        + (seedStepSize.y() * lat);
            }
            for (int p = 0; p < helper.seedCount.z(); p++)
            {
                cInfo.startGrid->levels[p] = seedPressureLevels.at(p);
            }

            break;
        case VERTICAL_SECTION:
            helper.seedCount = QVector3D(
                static_cast<uint>((seedMaxPosition.toVector2D()
                                   - seedMinPosition.toVector2D()).length()
                                  / seedStepSize.x() + 1.0),
                1,
                seedPressureLevels.size());
            break;
    }
    helper.trajectoryCount = static_cast<uint>(helper.seedCount.x()
                                               * helper.seedCount.y()
                                               * helper.seedCount.z());


    // C) Trajectory or streamline computation
    // =======================================

    switch (lineType)
    {
        case PATH_LINE:
            computePathLines(helper, cInfo);
            break;
        case STREAM_LINE:
            computeStreamLines(helper, cInfo);
            break;
    }
}


void MTrajectoryComputationSource::computeStreamLines(
        TrajectoryComputationHelper& ch, MTrajectoryComputationInfo& cInfo)
{
    // Array to store grids with wind data that are passed to the integration
    // methods. trajectoryIntegrationEuler() and trajectoryIntegrationRungeKutta()
    // both require two timesteps to be passed. As for streamlines we only
    // consider a single timestep, the current implementation simply stores
    // pointers to the same data for both time steps (see below).
    QVector<QVector<MStructuredGrid*>> grids(
                2, QVector<MStructuredGrid *>(3, nullptr));

    MDataRequestHelper rh(ch.baseRequest);

    // Initialize computation information.
    cInfo.numTrajectories = ch.trajectoryCount;
    cInfo.numStoredVerticesPerTrajectory = ch.streamlineLength;
    cInfo.times.reserve(cInfo.numStoredVerticesPerTrajectory);
    cInfo.vertices = QVector<QVector<QVector3D>>(cInfo.numTrajectories);
    for (uint t = 0; t < cInfo.numTrajectories; ++t)
    {
        cInfo.vertices[t].reserve(cInfo.numStoredVerticesPerTrajectory);
    }

    // Add start time step and seed points.
    QVector<bool> validPosition(cInfo.numTrajectories);
    cInfo.times.push_back(ch.validTimes.at(ch.startTimeStep));
    for (uint trajectory = 0; trajectory < ch.trajectoryCount; ++trajectory)
    {
        cInfo.vertices[trajectory].push_back(
                    determineTrajectorySeedPosition(trajectory, ch));
        validPosition[trajectory] = true;
    }

    // Load wind data (see comment above; two timesteps are required for the
    // integration methods, here both grids are the same as only a single time
    // step is required for streamline computation).
    QDateTime timeStep = ch.validTimes.at(ch.startTimeStep);
    rh.insert("VALID_TIME", timeStep);
    bool allDataFieldsValid = true;
    for (int v = 0; v < ch.varNames.size(); ++v) // v = 0,1,2 => wind u,v,w
    {
        rh.insert("VARIABLE", ch.varNames[v]);
        grids[0][v] = dataSource->getData(rh.request()); // timestep 0
        grids[1][v] = grids[0][v];                       // timestep 1 (link)

        if (grids[0][v] == nullptr)
        {
            allDataFieldsValid = false;
            QString emsg = QString("ERROR: Not all wind components required "
                                   "for the streamline computation could be "
                                   "loaded. Please check the console output "
                                   "and your datasets. Aborting streamline "
                                   "computation.");
            LOG4CPLUS_ERROR(mlog, emsg.toStdString());
//TODO (mr, 20Jul2018) -- see corresponding part in computePathLines().
//            QMessageBox::critical(nullptr, "Error", emsg, QMessageBox::Ok);
        }
    }

    // cInfo.times contains the list of timesteps that correspond to the
    // trajectory vertices. For streamlines, there is only one timestep.
    // How should this be handled? Currently, all times are set to the same
    // value.
//TODO (mr, 13Jul2018) -- how should this be handled?
    for (uint i = 1; i <= cInfo.numStoredVerticesPerTrajectory; ++i)
    {
        cInfo.times.push_back(ch.validTimes.at(ch.startTimeStep));
    }

    // Compute the streamlines.
    if (allDataFieldsValid)
    {
#pragma omp parallel for
        for (uint iStreamline = 0; iStreamline < ch.trajectoryCount; ++iStreamline)
        {
            // Integrate streamline in wind field.
            for (uint iVertex = 1; iVertex <= cInfo.numStoredVerticesPerTrajectory;
                 ++iVertex)
            {
                // Get current position from vertex buffer.
                QVector3D currentPosition = cInfo.vertices[iStreamline].back();

                // Compute next vertex with Euler or Runge-Kutta integration.
                QVector3D nextPos;
                switch (ch.iterationMethod)
                {
                case EULER:
                    nextPos = trajectoryIntegrationEuler(
                                currentPosition, ch.streamlineDeltaS, 0, 0,
                                ch.interpolationMethod, grids,
                                validPosition[iStreamline]);
                    break;
                case RUNGE_KUTTA:
                    nextPos = trajectoryIntegrationRungeKutta(
                                currentPosition, ch.streamlineDeltaS, 0, 0,
                                ch.interpolationMethod, grids,
                                validPosition[iStreamline]);
                    break;
                }

                // Check validity of computed vertex position.
                if (!validPosition[iStreamline])
                {
                    nextPos = QVector3D(M_INVALID_TRAJECTORY_POS,
                                        M_INVALID_TRAJECTORY_POS,
                                        M_INVALID_TRAJECTORY_POS);
                }

                // Store position in vertex buffer.
                cInfo.vertices[iStreamline].push_back(nextPos);
            }
        }
    }

    // Release wind data grids.
    for (int v = 0; v < ch.varNames.size(); ++v)
    {
        if (grids[0][v])
        {
            dataSource->releaseData(grids[0][v]);
        }
    }
}


void MTrajectoryComputationSource::computePathLines(
        TrajectoryComputationHelper& ch, MTrajectoryComputationInfo& cInfo)
{
    // Array to store grids with wind data that are passed to the integration
    // methods. Two timesteps are stored.
    QVector<QVector<MStructuredGrid*>> grids(
                2, QVector<MStructuredGrid *>(3, nullptr));
    QVector<QDateTime> timeSteps(2);
    MDataRequestHelper rh(ch.baseRequest);

    // Initialize computation information.
    cInfo.numStoredVerticesPerTrajectory =
            abs(int(ch.endTimeStep) - int(ch.startTimeStep)) + 1;
    cInfo.numTrajectories = ch.trajectoryCount;
    cInfo.times.reserve(cInfo.numStoredVerticesPerTrajectory);
    cInfo.vertices = QVector<QVector<QVector3D>>(cInfo.numTrajectories);
    for (uint t = 0; t < cInfo.numTrajectories; ++t)
    {
        cInfo.vertices[t].reserve(cInfo.numStoredVerticesPerTrajectory);
    }

    // Create temporary vector to cache trajectory vertex positions.
    QVector<QVector3D> positions(cInfo.numTrajectories);
    QVector<bool> validPosition(cInfo.numTrajectories);

    // Add start time step and seed points.
    cInfo.times.push_back(ch.validTimes.at(ch.startTimeStep));
    for (uint trajectory = 0; trajectory < ch.trajectoryCount; ++trajectory)
    {
        positions[trajectory] = determineTrajectorySeedPosition(trajectory, ch);
        validPosition[trajectory] = true;

        cInfo.vertices[trajectory].push_back(positions[trajectory]);
    }

    // Compute path lines for all time steps involved
    // (check if forward or backward computation is requested).
    bool forward = ch.startTimeStep <= ch.endTimeStep;
    for (uint step = ch.startTimeStep; step != ch.endTimeStep;
         forward ? step++ : step--)
    {
        // Set previous and next data times and time between data time
        // steps.
        timeSteps[0] = ch.validTimes.at(step);
        timeSteps[1] = ch.validTimes.at(step + (forward ? 1 : -1));
        const float timeStepSeconds = timeSteps[0].secsTo(timeSteps[1])
                / static_cast<float>(ch.subTimeStepsPerDataTimeStep);

        // Load wind data (u,v,w components for both data time steps).
        bool allDataFieldsValid = true;
        for (int t = 0; t < timeSteps.size(); ++t)
        {
            rh.insert("VALID_TIME", timeSteps[t]);
            for (int v = 0; v < ch.varNames.size(); ++v)
            {
                rh.insert("VARIABLE", ch.varNames[v]);
                if (!grids[t][v])
                {
                    // The "next" timestep of the previous time iteration
                    // is now at the "previous" storage index [0] (see "swap"
                    // command below) and set -- hence the check for a valid
                    // pointer above.
                    grids[t][v] = dataSource->getData(rh.request());

                    if (grids[t][v] == nullptr)
                    {
                        allDataFieldsValid = false;
                    }
                }
            }
        }
        if (!allDataFieldsValid)
        {
            QString emsg = QString("ERROR: Not all wind components required "
                                   "for the trajectory computation could be "
                                   "loaded. Please check the console output "
                                   "and your datasets. Aborting trajectory "
                                   "computation.");
            LOG4CPLUS_ERROR(mlog, emsg.toStdString());
//TODO (mr, 20Jul2018) -- show QMessageBox at this place to inform the user
// of the error. Needs a signal/slot construct, see
// https://stackoverflow.com/questions/3268073/qobject-cannot-create-children-for-a-parent-that-is-in-a-different-thread
//            QMessageBox::critical(nullptr, "Error", emsg, QMessageBox::Ok);
            break;
        }

        // The "current" timestep for which the new vertex position needs to
        // be computed is the "next" timestep (index [1]).
        cInfo.times.push_back(timeSteps[1]);

        // Compute the trajectories.
        #pragma omp parallel for
        for (uint iTrajectory = 0; iTrajectory < ch.trajectoryCount; ++iTrajectory)
        {
            // Iterate over "sub-timesteps" (i.e. internally time-interpolated
            // interpolation nodes; cf. trajectory integration implemented in
            // LAGRANTO and described in Sprenger and Wernli (GMD, 2015)).
            for (uint iSubTimeStep = 1;
                 iSubTimeStep <= ch.subTimeStepsPerDataTimeStep; ++iSubTimeStep)
            {
                // Compute time interpolation weights.
                const float timeIntWeight0 = (iSubTimeStep - 1)
                        / static_cast<float>(ch.subTimeStepsPerDataTimeStep);
                const float timeIntWeight1 = iSubTimeStep
                        / static_cast<float>(ch.subTimeStepsPerDataTimeStep);

                // Compute next vertex with Euler or Runge-Kutta integration.
                switch (ch.iterationMethod)
                {
                    case EULER:
                        positions[iTrajectory] =
                                trajectoryIntegrationEuler(
                                    positions[iTrajectory], timeStepSeconds,
                                    timeIntWeight0, timeIntWeight1,
                                    ch.interpolationMethod,
                                    grids, validPosition[iTrajectory]);
                        break;
                    case RUNGE_KUTTA:
                        positions[iTrajectory] =
                                trajectoryIntegrationRungeKutta(
                                    positions[iTrajectory], timeStepSeconds,
                                    timeIntWeight0, timeIntWeight1,
                                    ch.interpolationMethod,
                                    grids, validPosition[iTrajectory]);
                        break;
                }

                // Check validity of computed vertex position.
                if (!validPosition[iTrajectory])
                {
                    positions[iTrajectory] = QVector3D(M_INVALID_TRAJECTORY_POS,
                                                       M_INVALID_TRAJECTORY_POS,
                                                       M_INVALID_TRAJECTORY_POS);
                }
            }

            // Save computed vertex position for the "current" time step.
            cInfo.vertices[iTrajectory].push_back(positions[iTrajectory]);
        }

        // Release data grid of previous time step (i.e. index [0]).
        for (int v = 0; v < ch.varNames.size(); ++v)
        {
            dataSource->releaseData(grids[0][v]);
            grids[0][v] = nullptr;
        }

        // Swap data grids (the current data field at index [1] will be
        // needed at index [0] in the next time iteration).
        swap(grids[0], grids[1]);
    }

    // Release all remaining not-yet released data fields.
    for (int t = 0; t < grids.size(); ++t)
    {
        for (int v = 0; v < grids[t].size(); ++v)
        {
            if (grids[t][v])
            {
                dataSource->releaseData(grids[t][v]);
                grids[t][v] = nullptr;
            }
        }
    }
}


QVector3D MTrajectoryComputationSource::trajectoryIntegrationEuler(
        QVector3D pos, float deltaT, float timePos0, float timePos1,
        TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD method,
        QVector<QVector<MStructuredGrid*>> &grids, bool& valid)
{
    QVector3D p0 = pos;
    QVector3D v0 = sampleVelocity3DSpaceTime(p0, timePos0, method, grids, valid);
    QVector3D p1 = p0;

    for (int i = 0; i < EULER_ITERATION; i++)
    {
        QVector3D v1 = sampleVelocity3DSpaceTime(p1, timePos1, method, grids, valid);
        QVector3D v = (v0 + v1) / 2.0f;
        p1 = p0 + (convertWindVelocityFromMetricToSpherical(v, pos) * deltaT);
    }

    return p1;
}


QVector3D MTrajectoryComputationSource::trajectoryIntegrationRungeKutta(
        QVector3D pos, float deltaT, float timePos0, float timePos1,
        TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD method,
        QVector<QVector<MStructuredGrid*>> &grids, bool& valid)
{
    QVector3D s;
    QVector<QVector3D> k(4);
    float timeInterpolationWeight;

    for (int i = 0; i < 4; i++)
    {
        if (i == 0)
        {
            s = QVector3D(0.0f, 0.0f, 0.0f);
            timeInterpolationWeight = timePos0;
        }
        else if (i == 3)
        {
            s = k[i - 1];
            timeInterpolationWeight = timePos1;
        }
        else
        {
            s = k[i - 1] / 2.0f;
            timeInterpolationWeight = (timePos0 + timePos1) / 2.0f;
        }

        QVector3D v = sampleVelocity3DSpaceTime(
                    pos + s, timeInterpolationWeight, method, grids, valid);
        k[i] = convertWindVelocityFromMetricToSpherical(v, pos);
    }

    return pos + ((k[0] + 2.0f * k[1] + 2.0f * k[2] + k[3]) * deltaT / 6.0f);
}


QVector3D MTrajectoryComputationSource::sampleVelocity3DSpaceTime(
        QVector3D pos, float timeInterpolationValue,
        TRAJECTORY_COMPUTATION_INTERPOLATION_METHOD method,
        QVector<QVector<MStructuredGrid*>> &grids, bool& valid)
{
    // Initialise velocity.
    QVector3D velocity(0, 0, 0);

    // Sample velocity with given interpolation method.
    // NOTE: See Philipp Kaiser's master's thesis (TUM 2017) for details on the
    // different interpolation approaches. The LAGRANTO_INTERPOLATION method
    // follows the implementation of LAGRANTO v2
    // (http://dx.doi.org/10.5194/gmd-8-2569-2015).
    switch (method)
    {
        case LAGRANTO_INTERPOLATION:
        {
//TODO (mr, 11Jul2018) -- I assume this is still the UNCORRECTED version
//  of LAGRANTO. Has this been corrected in a new LAGRANTO version? Check
//  consistency!!
            QVector3D uIndex = floatIndexAtPosInterpolatedInTime(
                        pos, grids[0][0], grids[1][0], timeInterpolationValue);
            QVector3D vIndex = floatIndexAtPosInterpolatedInTime(
                        pos, grids[0][1], grids[1][1], timeInterpolationValue);
            QVector3D pIndex = floatIndexAtPosInterpolatedInTime(
                        pos, grids[0][2], grids[1][2], timeInterpolationValue);

            // Check if index is valid.
            if ( uIndex.x() < 0 || uIndex.y() < 0 || uIndex.z() < 0
                 || vIndex.x() < 0 || vIndex.y() < 0 || vIndex.z() < 0
                 || pIndex.x() < 0 || pIndex.y() < 0 || pIndex.z() < 0 )
            {
                valid = false;
                break;
            }

            float u = sampleDataValueAtFloatIndexAndInterpolateInTime(
                        uIndex, grids[0][0], grids[1][0], timeInterpolationValue);
            float v = sampleDataValueAtFloatIndexAndInterpolateInTime(
                        vIndex, grids[0][1], grids[1][1], timeInterpolationValue);
            float p = sampleDataValueAtFloatIndexAndInterpolateInTime(
                        pIndex, grids[0][2], grids[1][2], timeInterpolationValue);

            velocity = QVector3D(u, v, p);
            break;
        }

        case MET3D_INTERPOLATION:
        {
            // Sample velocity.
            float uT0 = grids[0][0]->interpolateValue(pos.x(), pos.y(), pos.z());
            float vT0 = grids[0][1]->interpolateValue(pos.x(), pos.y(), pos.z());
            float pT0 = grids[0][2]->interpolateValue(pos.x(), pos.y(), pos.z());
            float uT1 = grids[1][0]->interpolateValue(pos.x(), pos.y(), pos.z());
            float vT1 = grids[1][1]->interpolateValue(pos.x(), pos.y(), pos.z());
            float pT1 = grids[1][2]->interpolateValue(pos.x(), pos.y(), pos.z());

            // Check velocity for missing values; interpolate if ok.
            if (IS_MISSING(uT0) || IS_MISSING(vT0) || IS_MISSING(pT0)
                    || IS_MISSING(uT1) || IS_MISSING(vT1)
                    || IS_MISSING(pT1))
            {
                valid = false;
                break;
            }

            velocity = QVector3D(MMIX(uT0, uT1, timeInterpolationValue),
                                 MMIX(vT0, vT1, timeInterpolationValue),
                                 MMIX(pT0, pT1, timeInterpolationValue));
            break;
        }
    }

    return velocity;
}


QVector3D MTrajectoryComputationSource::convertWindVelocityFromMetricToSpherical(
        QVector3D velocity_ms_ms_Pas, QVector3D position_lon_lat_p)
{
    // Convert velocity from m/s to (lat, lon, p)/s .
    QVector3D vel;
    vel.setX(velocity_ms_ms_Pas.x()
             / (LAT_TO_METER * cos(position_lon_lat_p.y() * M_PI / 180.0)));
    vel.setY(velocity_ms_ms_Pas.y() / LAT_TO_METER);
    vel.setZ(velocity_ms_ms_Pas.z() / 100.0);
    return vel;
}


float MTrajectoryComputationSource::samplePressureAtFloatIndex(
        QVector3D index, MStructuredGrid *grid)
{
    QVector3D p0(floor(index.x()), floor(index.y()), floor(index.z()));
    QVector3D p1(ceil(index.x()), ceil(index.y()), ceil(index.z()));

    float fraci = index.x() - p0.x();
    float fracj = index.y() - p0.y();
    float frack = index.z() - p0.z();

    float val000 = grid->getPressure(p0.z(), p0.y(), p0.x());
    float val001 = grid->getPressure(p0.z(), p0.y(), p1.x());
    float val010 = grid->getPressure(p0.z(), p1.y(), p0.x());
    float val011 = grid->getPressure(p0.z(), p1.y(), p1.x());
    float val100 = grid->getPressure(p1.z(), p0.y(), p0.x());
    float val101 = grid->getPressure(p1.z(), p0.y(), p1.x());
    float val110 = grid->getPressure(p1.z(), p1.y(), p0.x());
    float val111 = grid->getPressure(p1.z(), p1.y(), p1.x());

    return MMIX(MMIX(MMIX(val000, val001, fraci), MMIX(val010, val011, fraci),
                     fracj),
                MMIX(MMIX(val100, val101, fraci), MMIX(val110, val111, fraci),
                     fracj),
                frack);
}


float MTrajectoryComputationSource::sampleDataValueAtFloatIndex(
        QVector3D index, MStructuredGrid *grid)
{
    QVector3D p0(floor(index.x()), floor(index.y()), floor(index.z()));
    QVector3D p1(ceil(index.x()), ceil(index.y()), ceil(index.z()));

    float fraci = index.x() - p0.x();
    float fracj = index.y() - p0.y();
    float frack = index.z() - p0.z();

    float val000 = grid->getValue(p0.z(), p0.y(), p0.x());
    float val001 = grid->getValue(p0.z(), p0.y(), p1.x());
    float val010 = grid->getValue(p0.z(), p1.y(), p0.x());
    float val011 = grid->getValue(p0.z(), p1.y(), p1.x());
    float val100 = grid->getValue(p1.z(), p0.y(), p0.x());
    float val101 = grid->getValue(p1.z(), p0.y(), p1.x());
    float val110 = grid->getValue(p1.z(), p1.y(), p0.x());
    float val111 = grid->getValue(p1.z(), p1.y(), p1.x());

    return MMIX(MMIX(MMIX(val000, val001, fraci), MMIX(val010, val011, fraci),
                     fracj),
                MMIX(MMIX(val100, val101, fraci), MMIX(val110, val111, fraci),
                     fracj),
                frack);
}


QVector3D MTrajectoryComputationSource::floatIndexAtPos(
        QVector3D pos, MStructuredGrid *grid)
{
    float i = MMOD(pos.x() - grid->getLons()[0], 360.)
            / abs(grid->getLons()[1] - grid->getLons()[0]);
    float j = (grid->getLats()[0] - pos.y())
            / abs(grid->getLats()[1] - grid->getLats()[0]);

    if (i < 0 || i > (grid->getNumLons() - 1) || j < 0
            || j > (grid->getNumLats() - 1))
    {
        return QVector3D(-1, -1, -1);
    }

    int kl = 0;
    int ku = grid->getNumLevels() - 1;

    float pp0 = samplePressureAtFloatIndex(QVector3D(i, j, kl), grid);
    float pp1 = samplePressureAtFloatIndex(QVector3D(i, j, ku), grid);

    // Perform the binary search.
    while ((ku - kl) > 1)
    {
        // Element midway between klower and kupper.
        int kmid = (ku+kl) / 2;
        float ppm = samplePressureAtFloatIndex(QVector3D(i, j, kmid), grid);

        // Cut interval in half.
        if (ppm >= pos.z())
        {
            ku = kmid;
            pp1 = ppm;
        }
        else
        {
            kl = kmid;
            pp0 = ppm;
        }
    }

    float k = kl + (pp0 - pos.z()) / ( pp0 - pp1);

    if (k < 0 || k > (grid->getNumLevels() - 1))
    {
        return QVector3D(i, j, -1);
    }

    return QVector3D(i, j, k);
}


QVector3D MTrajectoryComputationSource::floatIndexAtPosInterpolatedInTime(
        QVector3D pos, MStructuredGrid *grid0, MStructuredGrid *grid1,
        float timeInterpolationValue)
{
    // Get indices for both grids.
    QVector3D index0 = floatIndexAtPos(pos, grid0);
    QVector3D index1 = floatIndexAtPos(pos, grid1);

    // Check if any of the indices has a missing value (-1) and return
    // interpolated index.
    QVector3D mixIndex;
    mixIndex.setX((index0.x() < 0 || index1.x() < 0)
                  ? - 1 : MMIX(index0.x(), index1.x(), timeInterpolationValue));
    mixIndex.setY((index0.y() < 0 || index1.y() < 0)
                  ? - 1 : MMIX(index0.y(), index1.y(), timeInterpolationValue));
    mixIndex.setZ((index0.z() < 0 || index1.z() < 0)
                  ? - 1 : MMIX(index0.z(), index1.z(), timeInterpolationValue));
    return mixIndex;
}


float MTrajectoryComputationSource::sampleDataValueAtFloatIndexAndInterpolateInTime(
        QVector3D index, MStructuredGrid *grid0, MStructuredGrid *grid1,
        float timeInterpolationValue)
{
    float v0 = sampleDataValueAtFloatIndex(index, grid0);
    float v1 = sampleDataValueAtFloatIndex(index, grid1);
    return MMIX(v0, v1, timeInterpolationValue);
}


QVector3D MTrajectoryComputationSource::determineTrajectorySeedPosition(
        int trajectory, TrajectoryComputationHelper& ch)
{
    switch (ch.seedType)
    {
        // POLE and HORIZONTAL seed types are axis aligned
        case VERTICAL_POLE:
        case HORIZONTAL_SECTION:
        case VOLUME_BOX:
        {
            // Compute grid positions for x, y and z.
            int x = trajectory % static_cast<int>(ch.seedCount.x());
            int y = trajectory / static_cast<int>(ch.seedCount.x())
                    % static_cast<int>(ch.seedCount.y());
            int z = trajectory / static_cast<int>(ch.seedCount.x())
                    / static_cast<int>(ch.seedCount.y());

            // Compute seed position given by delta and grid position.
            return QVector3D(ch.seedMinPosition.x()
                             + x * ch.seedStepSizeHorizontalLonLat.x(),
                             ch.seedMinPosition.y()
                             + y * ch.seedStepSizeHorizontalLonLat.y(),
                             ch.seedPressureLevels.at(z));
        }
         // VERTICAL seed type is not axis aligned in x and y, interpolate along x;
        case VERTICAL_SECTION:
        {
            // Compute grid position for x and z (ignore y).
            int x = trajectory % static_cast<int>(ch.seedCount.x());
            int z = trajectory / static_cast<int>(ch.seedCount.x());

            // Interpolate horizontal position along with factor given by grid
            // slot x.
            QVector2D dir = (ch.seedMaxPosition.toVector2D()
                             - ch.seedMinPosition.toVector2D()).normalized();
            QVector2D xyPos = ch.seedMinPosition.toVector2D()
                    + dir * ch.seedStepSizeHorizontalLonLat.x() * x;

            // Compute seed position given by interpolation value for xy and
            // by z delta and z grid position.
            return QVector3D(xyPos.x(), xyPos.y(), ch.seedPressureLevels.at(z));
        }
    }

    return ch.seedMinPosition;
}


} // namespace Met3D
