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
#include "thinouttrajectoryfilter.h"

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

void MThinOutTrajectoryFilter::setTrajectorySource(MTrajectoryDataSource* s)
{
    // Enable pass-through to input trajectory source if required request
    // keys are not specified.
    MTrajectoryFilter::setTrajectorySource(s);
    enablePassThrough(s);
}


MTrajectorySelection* MThinOutTrajectoryFilter::produceData(
        MDataRequest request)
{
    // NOTE: The filter currently assumes a dimension index order "lat/lon/lev"
    // of the start grid positions of the trajectories. This is valid for the
    // Lagranto trajectories processed for the p(WCB) study, but cannot be
    // assumed in general! (mr, 10Feb2014).

    assert(trajectorySource != nullptr);

    MDataRequestHelper rh(request);

    QStringList args = rh.value("THINOUT_STRIDE").split("/");

    rh.remove("THINOUT_STRIDE");
    MTrajectories *input = trajectorySource->getData(rh.request());

    MWritableTrajectorySelection *filterResult =
            new MWritableTrajectorySelection(input->refersTo(),
                                             input->getNumTrajectories(),
                                             input->getTimes(),
                                             input->getStartGridStride());

    // Stride for lon/lat/lev.
    int thinoutLon = args[0].toInt();
    int thinoutLat = args[1].toInt();
    int thinoutLev = args[2].toInt();

    // Get start grid size from trajectory data item.
    shared_ptr<MStructuredGrid> startGrid = input->getStartGrid();
    int nlon = startGrid->getNumLons();
    int nlat = startGrid->getNumLats();
    int nlev = startGrid->getNumLevels();
    int nlevnlon = nlev * nlon;

    // Check for correct dimensions of the start grid.
    int startGridPoints = nlevnlon*nlat;
    if (startGridPoints == input->getNumTrajectories())
    {
        // Index for selected trajectories.
        int j = 0;

        // Dimension order: lat/lon/lev (cf. notes mr, 10Feb2014).
        for (int ilat = 0; ilat < nlat; ilat += thinoutLat)
        {
            int itrajlat = nlevnlon * ilat;
            for (int ilon = 0; ilon < nlon; ilon += thinoutLon)
            {
                int itrajlon = itrajlat + nlev * ilon;
                for (int ilev = 0; ilev < nlev; ilev += thinoutLev)
                {
                    int i = itrajlon + ilev;
                    filterResult->setStartIndex(j, input->getStartIndices()[i]);
                    filterResult->setIndexCount(j, input->getIndexCount()[i]);
                    j++;
                }
            }
        }

        filterResult->setStartGridStride(
                    QVector3D(thinoutLon, thinoutLat, thinoutLev));
        filterResult->decreaseNumSelectedTrajectories(j);
    }
    else
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: Trajectory start grid dimensions ("
                        << startGridPoints << ") don't match "
                        << "number of available trajectories ("
                        << input->getNumTrajectories() << "). Returning "
                        <<"empty filter result.");

        filterResult->decreaseNumSelectedTrajectories(0);
    }

    // Debug output.
    // input->dumpStartVerticesToLog(300, filterResult);

    trajectorySource->releaseData(input);
    return filterResult;
}


MTask* MThinOutTrajectoryFilter::createTaskGraph(MDataRequest request)
{
    assert(trajectorySource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    rh.removeAll(locallyRequiredKeys());
    task->addParent(trajectorySource->getTaskGraph(rh.request()));

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MThinOutTrajectoryFilter::locallyRequiredKeys()
{
    return (QStringList() << "THINOUT_STRIDE");
}



} // namespace Met3D
