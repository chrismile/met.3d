/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2018 Bianca Tost
**  Copyright 2018 Marc Rautenhaus
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
#ifndef PROCESSINGWEATHERPREDICTIONDATASOURCE_H
#define PROCESSINGWEATHERPREDICTIONDATASOURCE_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "weatherpredictiondatasource.h"
#include "structuredgrid.h"
#include "datarequest.h"

namespace Met3D
{

/**
  @brief Super class for data sources processing data of one ore multiple
  @p MWeatherPredictionDataSource.
  */
class MProcessingWeatherPredictionDataSource
        : public MWeatherPredictionDataSource
{
public:
    MProcessingWeatherPredictionDataSource();

protected:
    /**
      Creates and initializes a new MStructuredGrid-subclass of the same type
      as @p templateGrid. Coordinate values etc. will be copied from @p
      templateGrid. If the new result grid is a hybrid sigma-pressure level
      grid or an auxiliary pressure field grid, the corresponding surface
      pressure or auxiliary pressure field will be copied as well.
     */
    virtual MStructuredGrid *createAndInitializeResultGrid(
            MStructuredGrid *templateGrid);

};


/**
  @brief Same as @ref MProcessingWeatherPredictionDataSource but for data
  sources that process only a single input. Methods including @p setInput
  and @p available* are predefined.
  */
class MSingleInputProcessingWeatherPredictionDataSource
        : public MProcessingWeatherPredictionDataSource
{
public:
    MSingleInputProcessingWeatherPredictionDataSource();

    /**
      Request pass-through is enabled by default. Override to disable.
     */
    virtual void setInputSource(MWeatherPredictionDataSource* s);

    QList<MVerticalLevelType> availableLevelTypes() override;

    QStringList availableVariables(MVerticalLevelType levelType) override;

    QSet<unsigned int> availableEnsembleMembers(MVerticalLevelType levelType,
                                                const QString& variableName) override;

    QList<QDateTime> availableInitTimes(MVerticalLevelType levelType,
                                        const QString& variableName) override;

    QList<QDateTime> availableValidTimes(MVerticalLevelType levelType,
                                         const QString& variableName,
                                         const QDateTime& initTime) override;

    QString variableLongName(MVerticalLevelType levelType,
                             const QString&     variableName) override;

    QString variableStandardName(MVerticalLevelType levelType,
                             const QString&     variableName) override;

    QString variableUnits(MVerticalLevelType levelType,
                             const QString&     variableName) override;


protected:
    MWeatherPredictionDataSource* inputSource;

};

} // namespace Met3D

#endif // PROCESSINGWEATHERPREDICTIONDATASOURCE_H
