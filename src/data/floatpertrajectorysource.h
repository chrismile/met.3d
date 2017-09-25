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
#ifndef FLOATPERTRAJECTORYSOURCE_H
#define FLOATPERTRAJECTORYSOURCE_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "scheduleddatasource.h"
#include "trajectorydatasource.h"
#include "trajectories.h"
#include "datarequest.h"


namespace Met3D
{

/**
  @brief Base class for modules that supplement a trajectory data item with one
  float value per trajectory (for example, the maximum pressure or temperature
  encountered along a trajectory).
  */
class MFloatPerTrajectorySource : public MScheduledDataSource
{
public:
    MFloatPerTrajectorySource();

    /**
      Overloads @ref MMemoryManagedDataSource::getData() to cast the result
      to the type @ref MFloatPerTrajectorySupplement.
     */
    MFloatPerTrajectorySupplement* getData(MDataRequest request)
    { return static_cast<MFloatPerTrajectorySupplement*>
                (MScheduledDataSource::getData(request)); }

    void setTrajectorySource(MTrajectoryDataSource* s);

protected:
    MTrajectoryDataSource* trajectorySource;

};


} // namespace Met3D

#endif // FLOATPERTRAJECTORYSOURCE_H
