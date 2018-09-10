/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2018 Marc Rautenhaus
**  Copyright 2018 Bianca Tost
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
#include "gridaggregation.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports


namespace Met3D
{

// MGridAggregation
// ================

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MGridAggregation::MGridAggregation()
{
}


MGridAggregation::~MGridAggregation()
{
    foreach (MStructuredGrid *grid, grids)
    {
        grid->getMemoryManager()->releaseData(grid);
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

unsigned int MGridAggregation::getMemorySize_kb()
{
    return sizeof(grids);
}


void MGridAggregation::addGrid(MStructuredGrid *grid)
{
    grids.append(grid);
    grid->increaseReferenceCounter();
}


// MGridAggregationDataSource
// ==========================

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MGridAggregationDataSource::MGridAggregationDataSource()
    : MScheduledDataSource(),
      inputSource(nullptr)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MGridAggregation *MGridAggregationDataSource::produceData(MDataRequest request)
{
    assert(inputSource != nullptr);

    // Parse request.
    MDataRequestHelper rh(request);
    QSet<unsigned int> selectedMembers = rh.uintSetValue("SELECTED_MEMBERS");
    rh.removeAll(locallyRequiredKeys());

    MGridAggregation* gridAggr = new MGridAggregation();


    foreach (unsigned int m, selectedMembers)
    {
        rh.insert("MEMBER", m);

        MStructuredGrid *memberGrid = inputSource->getData(rh.request());
        gridAggr->addGrid(memberGrid);
        // addGrid() increases the grid's reference count in its memory
        // manager. Hence, the grid can be released below.
        inputSource->releaseData(memberGrid);
    }

    return gridAggr;
}


MTask *MGridAggregationDataSource::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    QSet<unsigned int> selectedMembers = rh.uintSetValue("SELECTED_MEMBERS");
    QString ensembleOperation = rh.value("ENS_OPERATION");
    rh.removeAll(locallyRequiredKeys());

    if (ensembleOperation == "MULTIPLE_MEMBERS")
    {
        foreach (unsigned int m, selectedMembers)
        {
            rh.insert("MEMBER", m);
            task->addParent(inputSource->getTaskGraph(rh.request()));
        }
    }

    return task;
}


void MGridAggregationDataSource::setInputSource(MWeatherPredictionDataSource *s)
{
    inputSource = s;
    registerInputSource(inputSource);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MGridAggregationDataSource::locallyRequiredKeys()
{
    return (QStringList() << "ENS_OPERATION" << "SELECTED_MEMBERS");
}


} // namespace Met3D
