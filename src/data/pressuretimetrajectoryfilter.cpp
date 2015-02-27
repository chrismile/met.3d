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
#include "pressuretimetrajectoryfilter.h"

// standard library imports
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
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

void MPressureTimeTrajectoryFilter::setDeltaPressureSource(
        MFloatPerTrajectorySource* s)
{
    deltaPressureSource = s;
    registerInputSource(deltaPressureSource);
}


MTrajectorySelection* MPressureTimeTrajectoryFilter::produceData(
        MDataRequest request)
{
    assert(inputSelectionSource != nullptr);
    assert(deltaPressureSource != nullptr);

    MDataRequestHelper rh(request);

    QStringList args = rh.value("FILTER_PRESSURE_TIME").split("/");

    rh.remove("FILTER_PRESSURE_TIME");
    MTrajectorySelection *input = inputSelectionSource->getData(rh.request());

    MWritableTrajectorySelection *filterResult =
            new MWritableTrajectorySelection(input->refersTo(),
                                             input->getNumTrajectories(),
                                             input->getTimes(),
                                             input->getStartGridStride());

    if (args[0] != "ALL")
    {
        // Compute the filter.

        float deltaPressure_hPa = args[0].toFloat();
        int   deltaTime_hrs     = args[1].toInt();

        rh.insert("MAX_DELTA_PRESSURE_HOURS", deltaTime_hrs);
        MFloatPerTrajectorySupplement *deltaP =
                deltaPressureSource->getData(rh.request());

        // Filtering is implemented by simply looping over all trajectories.
        QVector<float> deltaPressure = deltaP->getValues();
        int numTrajectories = input->getNumTrajectories();
        int numTimes = input->getNumTimeStepsPerTrajectory();
        int j = 0;

        for (int i = 0; i < numTrajectories; i++)
        {
            // i = index of trajectory in input selection
            // trajectoryIndex = index of trajectory in "full" dataset
            int startIndex = input->getStartIndices()[i];
            int trajectoryIndex = ceil(float(startIndex)/numTimes);
            if (deltaPressure.at(trajectoryIndex) >= deltaPressure_hPa)
            {
                filterResult->setStartIndex(j, startIndex);
                filterResult->setIndexCount(j, input->getIndexCount()[i]);
                j++;
            }
        }

        filterResult->decreaseNumSelectedTrajectories(j);
        deltaPressureSource->releaseData(deltaP);
    }
    else
    {
        // Filter bypass: Copy all timesteps from input.

        for (int i = 0; i < input->getNumTrajectories(); i++)
        {
            filterResult->setStartIndex(i, input->getStartIndices()[i]);
            filterResult->setIndexCount(i, input->getIndexCount()[i]);
        }
    }

    inputSelectionSource->releaseData(input);
    return filterResult;
}


MTask* MPressureTimeTrajectoryFilter::createTaskGraph(MDataRequest request)
{
    assert(inputSelectionSource != nullptr);
    assert(deltaPressureSource != nullptr);

    MTask *task = new MTask(request, this);

    // Add dependencies.
    MDataRequestHelper rh(request);
    QStringList args = rh.value("FILTER_PRESSURE_TIME").split("/");
    rh.removeAll(locallyRequiredKeys());

    if (args[0] == "ALL")
    {
        // Filter bypass: If the passed value is "ALL", no filtering is
        // performed. We don't need the max. pressure in time interval
        // information in this case.
    }
    else
    {
        int deltaTime_hrs = args[1].toInt();
        rh.insert("MAX_DELTA_PRESSURE_HOURS", deltaTime_hrs);
        task->addParent(deltaPressureSource->getTaskGraph(rh.request()));
        rh.remove("MAX_DELTA_PRESSURE_HOURS");
    }

//TODO: This data source shouldn't contain knowledge about potential other
//      sources "upstream". Evaluate possible alternative solutions (e.g.
//      MTask refactoring) -- notes 12Feb2015.
    rh.remove("TRY_PRECOMPUTED");
    task->addParent(inputSelectionSource->getTaskGraph(rh.request()));

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MPressureTimeTrajectoryFilter::locallyRequiredKeys()
{
    return (QStringList() << "FILTER_PRESSURE_TIME");
}



} // namespace Met3D
