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
#ifndef WEATHERPREDICTIONDATASOURCE_H
#define WEATHERPREDICTIONDATASOURCE_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "scheduleddatasource.h"
#include "structuredgrid.h"


namespace Met3D
{

/**
  @brief MWeatherPredictionDataSource is the abstract base class for weather
  prediction data sources (e.g. data reader, ensemble statistics filter,
  derived variables). The class defines the interface common to all classes
  that provide gridded weather prediction data.
  */
class MWeatherPredictionDataSource : public MScheduledDataSource
{
public:
    MWeatherPredictionDataSource();

    MStructuredGrid* getData(MDataRequest request)
    { return static_cast<MStructuredGrid*>
                (MScheduledDataSource::getData(request)); }

    /**
      Returns a @ref QList containing the available level types.
      */
    virtual QList<MVerticalLevelType> availableLevelTypes() = 0;

    /**
      Returns a @ref QStringList containing the names of the variables
      available for level type @p levelType in dataset @p dataset.
      */
    virtual QStringList availableVariables(
            MVerticalLevelType levelType) = 0;

    /**
      If the variable is part of an ensemble forecast, returns the IDs of the
      available members in the ensemble. Otherwise returns a set with a single
      member "0".
      */
    virtual QSet<unsigned int> availableEnsembleMembers(
            MVerticalLevelType levelType,
            const QString&     variableName) = 0;

    /**
      Returns a @ref QList<QDateTime> containing the the forecast
      initialisation times (base times) available for the specified dataset,
      level type and variable.
      */
    virtual QList<QDateTime> availableInitTimes(
            MVerticalLevelType levelType,
            const QString&     variableName) = 0;

    /**
      Returns a @ref QList<QDateTime> containing the the forecast valid times
      available for the specified dataset, level type and variable at
      initialisation time @p initTime.
      */
    virtual QList<QDateTime> availableValidTimes(
            MVerticalLevelType levelType,
            const QString&     variableName,
            const QDateTime&   initTime) = 0;

    /**
      Returns the long (=full) name of a variable, if available (otherwise an
      empty string is returned).
      */
    virtual QString variableLongName(
            MVerticalLevelType levelType,
            const QString&     variableName)
    { Q_UNUSED(levelType); Q_UNUSED(variableName); return QString(); }

    /**
      Returns the standard name (according to the NetCDF CF conventions) of a
      variable, if available (otherwise an empty string is returned).
      */
    virtual QString variableStandardName(
            MVerticalLevelType levelType,
            const QString&     variableName)
    { Q_UNUSED(levelType); Q_UNUSED(variableName); return QString(); }

    /**
      Returns the units of a variable, if available (otherwise an empty string
      is returned).
      */
    virtual QString variableUnits(
            MVerticalLevelType levelType,
            const QString&     variableName)
    { Q_UNUSED(levelType); Q_UNUSED(variableName); return QString(); }

protected:

};


} // namespace Met3D

#endif // WEATHERPREDICTIONDATASOURCE_H
