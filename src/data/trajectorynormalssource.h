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
#ifndef TRAJECTORYNORMALSSOURCE_H
#define TRAJECTORYNORMALSSOURCE_H

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
  @brief Computes normals (required for rendering trajectory tubes) associated
  with a trajectory dataset.
  */
class MTrajectoryNormalsSource : public MScheduledDataSource
{
public:
    MTrajectoryNormalsSource();

    /**
      Overloads @ref MMemoryManagedDataSource::getData() to cast the result
      to the type @ref MFloatPerTrajectorySupplement.
     */
    MTrajectoryNormals* getData(MDataRequest request)
    { return static_cast<MTrajectoryNormals*>
                (MScheduledDataSource::getData(request)); }

    MTrajectoryNormals* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

    void setTrajectorySource(MTrajectoryDataSource* s);

protected:
    const QStringList locallyRequiredKeys();

private:
    MTrajectoryDataSource* trajectorySource;

};


} // namespace Met3D

#endif // TRAJECTORYNORMALSSOURCE_H
