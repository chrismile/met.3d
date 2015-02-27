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
#ifndef DELTAPRESSUREPERTRAJECTORY_H
#define DELTAPRESSUREPERTRAJECTORY_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "floatpertrajectorysource.h"
#include "trajectorydatasource.h"
#include "datarequest.h"


namespace Met3D
{

/**
  @brief For each trajectory, MDeltaPressurePerTrajectorySource computes the
  maximum pressure difference over a given time interval. The time interval is
  specified in hours in the request passed to @ref produceData().

  Example: A trajectory specified every six hours, with pressure values 1000,
  900, 800, 500, 200, 150 hPa. The maximum pressure difference for a 12 hour
  interval would be 600 hPa (800 to 200 hPa).
  */
class MDeltaPressurePerTrajectorySource : public MFloatPerTrajectorySource
{
public:
    MFloatPerTrajectorySupplement* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

protected:
    const QStringList locallyRequiredKeys();

};

} // namespace Met3D

#endif // DELTAPRESSUREPERTRAJECTORY_H
