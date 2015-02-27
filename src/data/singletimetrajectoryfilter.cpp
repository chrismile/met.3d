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
#include "singletimetrajectoryfilter.h"

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

MTrajectorySelection* MSingleTimeTrajectoryFilter::produceData(
        MDataRequest request)
{
    assert(inputSelectionSource != nullptr); // trajectorySource not required

    MDataRequestHelper rh(request);
    QDateTime t = rh.timeValue("FILTER_TIMESTEP");
    int timeStep = -1; // default: filter bypass if an invalid time is passed

    // If no valid time has been passed, try to interpret the string as integer
    // specifying the index of the time step.
    if (!t.isValid())
    {
        bool ok;
        timeStep = rh.value("FILTER_TIMESTEP").toInt(&ok);
        if (!ok) timeStep = -2;
    }

    rh.remove("FILTER_TIMESTEP");
    MTrajectorySelection *input = inputSelectionSource->getData(rh.request());

    // Determine index of time t in the list of trajectory times. indexOf()
    // returns -1 if no item is matched, hence in this case the filter will
    // be bypassed. (If timeStep == -2 t isn't valid, so we can skip this).
    if (timeStep == -1) timeStep = input->getTimes().indexOf(t);

    MWritableTrajectorySelection *filterResult =
            new MWritableTrajectorySelection(input->refersTo(),
                                             input->getNumTrajectories(),
                                             input->getTimes(),
                                             input->getStartGridStride());

    // Compute output indices.
    if (timeStep >= 0)
        for (int i = 0; i < input->getNumTrajectories(); i++)
        {
            // Extract single timestep.
            filterResult->setStartIndex(i, input->getStartIndices()[i] + timeStep);
            filterResult->setIndexCount(i, 1);
        }
    else
        for (int i = 0; i < input->getNumTrajectories(); i++)
        {
            // Filter bypass: Copy all timesteps from input.
            filterResult->setStartIndex(i, input->getStartIndices()[i]);
            filterResult->setIndexCount(i, input->getIndexCount()[i]);
        }

    inputSelectionSource->releaseData(input);
    return filterResult;
}


MTask* MSingleTimeTrajectoryFilter::createTaskGraph(MDataRequest request)
{
    assert(inputSelectionSource != nullptr);

    MTask *task = new MTask(request, this);

    // Add dependencies.
    MDataRequestHelper rh(request);
    rh.remove("FILTER_TIMESTEP");

    task->addParent(inputSelectionSource->getTaskGraph(rh.request()));

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MSingleTimeTrajectoryFilter::locallyRequiredKeys()
{
    return (QStringList() << "FILTER_TIMESTEP");
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


} // namespace Met3D
