/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016 Marc Rautenhaus
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
#include "bboxtrajectoryfilter.h"

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

void MBoundingBoxTrajectoryFilter::setTrajectorySource(MTrajectoryDataSource* s)
{
    // Enable pass-through to input trajectory source if required request
    // keys are not specified.
    MTrajectoryFilter::setTrajectorySource(s);
    enablePassThrough(s);
}


MTrajectorySelection* MBoundingBoxTrajectoryFilter::produceData(
        MDataRequest request)
{
    assert(inputSelectionSource != nullptr);

    MDataRequestHelper rh(request);

    QStringList args = rh.value("FILTER_BBOX").split("/");

    rh.remove("FILTER_BBOX");
    MTrajectories *trajectories = trajectorySource->getData(rh.request());
    MTrajectorySelection *inputSelection = inputSelectionSource->getData(rh.request());

    MWritableTrajectorySelection *filterResult =
            new MWritableTrajectorySelection(inputSelection->refersTo(),
                                             inputSelection->getNumTrajectories(),
                                             inputSelection->getTimes(),
                                             inputSelection->getStartGridStride());

    if (args[0] != "ALL")
    {
        // Compute the filter.

        float lonWest  = args[0].toFloat();
        float latSouth = args[1].toFloat();
        float lonEast  = args[2].toFloat();
        float latNorth = args[3].toFloat();

        // Filtering is implemented by simply looping over all trajectories.
        int numInputTrajectories = inputSelection->getNumTrajectories();
        int j = 0;

        for (int i = 0; i < numInputTrajectories; i++)
        {
            int startIndex = inputSelection->getStartIndices()[i];
            QVector3D p = trajectories->getVertices().at(startIndex);

            if (p.x() < lonWest) continue;
            if (p.x() > lonEast) continue;
            if (p.y() > latNorth) continue;
            if (p.y() < latSouth) continue;

            filterResult->setStartIndex(j, startIndex);
            filterResult->setIndexCount(j, inputSelection->getIndexCount()[i]);
            j++;
        }

        filterResult->decreaseNumSelectedTrajectories(j);
    }
    else
    {
        // Filter bypass: Copy all timesteps from input.

        for (int i = 0; i < inputSelection->getNumTrajectories(); i++)
        {
            filterResult->setStartIndex(i, inputSelection->getStartIndices()[i]);
            filterResult->setIndexCount(i, inputSelection->getIndexCount()[i]);
        }
    }

    trajectorySource->releaseData(trajectories);
    inputSelectionSource->releaseData(inputSelection);
    return filterResult;
}


MTask* MBoundingBoxTrajectoryFilter::createTaskGraph(MDataRequest request)
{
    assert(trajectorySource != nullptr);
    assert(inputSelectionSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    rh.removeAll(locallyRequiredKeys());

    task->addParent(trajectorySource->getTaskGraph(rh.request()));
    task->addParent(inputSelectionSource->getTaskGraph(rh.request()));

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MBoundingBoxTrajectoryFilter::locallyRequiredKeys()
{
    return (QStringList() << "FILTER_BBOX");
}



} // namespace Met3D
