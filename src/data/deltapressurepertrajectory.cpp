/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
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
#include "deltapressurepertrajectory.h"

// standard library imports
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "trajectories.h"
#include "trajectoryreader.h"
#include "util/mutil.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MFloatPerTrajectorySupplement* MDeltaPressurePerTrajectorySource::produceData(
        MDataRequest request)
{
    assert(trajectorySource != nullptr);

    MDataRequestHelper rh(request);
    int  timeInterval_hrs = rh.intValue("MAX_DELTA_PRESSURE_HOURS");
    bool tryPrecomputed   = bool(rh.intValue("TRY_PRECOMPUTED"));

    // Request required input trajectory data.
    rh.remove("MAX_DELTA_PRESSURE_HOURS");
    rh.remove("TRY_PRECOMPUTED");

    if (tryPrecomputed)
    {
        // Try to get precomputed results from trajectoy reader.
        // =====================================================

        // This only works if the input trajectory source is a reader
        // (NOTE -- mr, 15Jan2014 -- an alternative would be to add a field
        // "creator" to MTrajectories to access the reader. Or, in general, add
        // a history of objects that have touched the data to MAbstract data
        // item).
        if (MTrajectoryReader *reader =
                dynamic_cast<MTrajectoryReader*>(trajectorySource))
        {
            MDataRequestHelper srh;
            srh.insert("MAX_DELTA_PRESSURE_HOURS", timeInterval_hrs);

            MFloatPerTrajectorySupplement *deltaPressure =
                    reader->readFloatPerTrajectorySupplement(rh.request(),
                                                             srh.request());

            if (deltaPressure != nullptr)
            {
                // Release the requested but not required trajectory data.
                trajectorySource->releaseData(rh.request());

                return deltaPressure;
            }
            else
            {
                LOG4CPLUS_DEBUG(mlog, "cannot find precomputed delta pressure "
                                "data -- computing requested data from "
                                "trajectory data.");
            }
        }
        else
        {
            LOG4CPLUS_DEBUG(mlog, "no access to precomputed delta pressure "
                            "data -- computing requested data from trajectory "
                            "data.");
        }
    }

    // Compute result from trajectory data.
    // ====================================

    MTrajectories *traj = trajectorySource->getData(rh.request());

    // Number of time steps covering the given time interval.
    int numTimeIntervals = timeInterval_hrs /
            (traj->getTimeStepLength_sec() / 3600.);

    // Range checks: Increment the given number of time intervals by one to get
    // the number of values (i.e. time steps) to compare (e.g. one time step
    // required two values to determine the pressure difference, two time steps
    // correspond to three values etc.). The method requires a minimum of two
    // timesteps, but a maximum of _numTimeStepsPerTrajectory.
    int numTimeStepsPerTrajectory = traj->getNumTimeStepsPerTrajectory();
    int numTimeSteps = numTimeIntervals + 1;
    numTimeSteps = max(2, numTimeSteps);
    numTimeSteps = min(numTimeSteps, numTimeStepsPerTrajectory);

    LOG4CPLUS_DEBUG(mlog, "computing max. delta pressure over "
                    << numTimeIntervals << " time intervals ("
                    << numTimeSteps << " time steps)");

    MFloatPerTrajectorySupplement *deltaPressure =
            new MFloatPerTrajectorySupplement(traj->getGeneratingRequest(),
                                              traj->getNumTrajectories());

    QVector<QVector3D> vertices = traj->getVertices();

    int numTrajectories = traj->getNumTrajectories();
    for (int i = 0; i < numTrajectories; i++)
    {
        int trajStartIndex = i * numTimeStepsPerTrajectory;
        deltaPressure->setValue(i, 0.);

        // For every time window [t, t+numTimeSteps] find delta p and advance
        // window in time.
        int lastPossibleStartIndex = numTimeStepsPerTrajectory - numTimeSteps;
        for (int t = 0; t <= lastPossibleStartIndex; t++)
        {
            float pmin = 1200.;
            float pmax = 0;

            int maxT = t + numTimeSteps;
            for (int it = t; it < maxT; it++)
            {
                float pres = vertices.at(trajStartIndex+it).z();
                // Check for (pres > 0) to skip missing values.
                if ((pres > 0) && (pres < pmin)) pmin = pres;
                if ((pres > 0) && (pres > pmax)) pmax = pres;
            }

             // Remember the largest dp for this trajectory.
            float dp = pmax - pmin;
            if (dp > deltaPressure->getValues()[i])
                deltaPressure->setValue(i, dp);
        }

    }

    trajectorySource->releaseData(traj);
    return deltaPressure;
}


MTask* MDeltaPressurePerTrajectorySource::createTaskGraph(MDataRequest request)
{
    assert(trajectorySource != nullptr);

    MTask *task = new MTask(request, this);

    // Add dependency: the trajectories.
    MDataRequestHelper rh(request);
    rh.removeAll(locallyRequiredKeys());
    task->addParent(trajectorySource->getTaskGraph(rh.request()));

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MDeltaPressurePerTrajectorySource::locallyRequiredKeys()
{
    return (QStringList() << "MAX_DELTA_PRESSURE_HOURS" << "TRY_PRECOMPUTED");
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

} // namespace Met3D
