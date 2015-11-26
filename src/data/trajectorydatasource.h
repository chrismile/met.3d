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
#ifndef TRAJECTORYDATASOURCE_H
#define TRAJECTORYDATASOURCE_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "trajectoryselectionsource.h"
#include "trajectories.h"
#include "datarequest.h"


namespace Met3D
{

/**
  @brief MTrajectoryDataSource is the abstract base class for all trajectory
  data sources (e.g. data reader, memory manager). The class defines the
  interface common to all classes that provide trajectory data.
  */
class MTrajectoryDataSource : public MTrajectorySelectionSource
{
public:
    /**
      Overloads @ref MMemoryManagedDataSource::getData() to cast the result
      to the type @ref MSingleMemberTrajectories.
     */
    MTrajectories* getData(MDataRequest request)
    { return static_cast<MTrajectories*>
                (MTrajectorySelectionSource::getData(request)); }

    /**
      Returns a @ref QList<QDateTime> containing the the available forecast
      initialisation times (base times).
      */
    virtual QList<QDateTime> availableInitTimes() = 0;

    /**
      Valid times correspond to the the trajectory start times available for
      the specified initialisation time @p initTime.
      */
    virtual QList<QDateTime> availableValidTimes(const QDateTime& initTime) = 0;

    /**
      For a given init and valid time, returns the valid (=start) times of
      those trajectories that overlap with the given valid time.
     */
    virtual QList<QDateTime> validTimeOverlap(const QDateTime& initTime,
                                              const QDateTime& validTime) = 0;

    /**
      Returns the available ensemble members.
      */
    virtual QSet<unsigned int> availableEnsembleMembers() = 0;

protected:

};


} // namespace Met3D

#endif // TRAJECTORYDATASOURCE_H
