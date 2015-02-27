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
#ifndef SINGLETIMETRAJECTORYFILTER_H
#define SINGLETIMETRAJECTORYFILTER_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "trajectoryfilter.h"
#include "trajectories.h"
#include "datarequest.h"


namespace Met3D
{

/**
  @brief Trajectory filter that filters a single timestep of a trajectory
  dataset.

  Required keys:

  @p FILTER_TIMESTEP
  Can be either an integer [0..numTimeSteps-1] that specifies the index of a
  timestep, or a time in ISO format (e.g. 2012-10-19T18:00:00Z). If "ALL" is
  passed (or if an invalid value is passed), the filter is bypassed.
  */
class MSingleTimeTrajectoryFilter : public MTrajectoryFilter
{
public:
    MTrajectorySelection* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

protected:
    const QStringList locallyRequiredKeys();

};

} // namespace Met3D

#endif // SINGLETIMETRAJECTORYFILTER_H
