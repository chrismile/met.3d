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
#ifndef STRUCTUREDGRIDENSEMBLEFILTER_H
#define STRUCTUREDGRIDENSEMBLEFILTER_H

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
  @brief Computes per-gridpoint statistical quantities from the ensemble,
  e.g. mean, standard deviation, probabilities.
  */
class MStructuredGridEnsembleFilter
        : public MWeatherPredictionDataSource
{
public:
    MStructuredGridEnsembleFilter();

    MStructuredGrid* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

    void setInputSource(MWeatherPredictionDataSource* s);

    QList<MVerticalLevelType> availableLevelTypes();

    QStringList availableVariables(MVerticalLevelType levelType);

    QSet<unsigned int> availableEnsembleMembers(MVerticalLevelType levelType,
                                                const QString& variableName);

    QList<QDateTime> availableInitTimes(MVerticalLevelType levelType,
                                        const QString& variableName);

    QList<QDateTime> availableValidTimes(MVerticalLevelType levelType,
                                         const QString& variableName,
                                         const QDateTime& initTime);

    QString variableLongName(MVerticalLevelType levelType,
                             const QString&     variableName);

    QString variableStandardName(MVerticalLevelType levelType,
                             const QString&     variableName);

    QString variableUnits(MVerticalLevelType levelType,
                             const QString&     variableName);


protected:
    const QStringList locallyRequiredKeys();

    MWeatherPredictionDataSource* inputSource;

    /**
      Creates and initializes a new MStructuredGrid-subclass of the same type
      as @p templateGrid. Coordinate values etc. will be copied from
      @p templateGrid. If @p selectedMembers is not empty AND the new result
      grid is a hybrid sigma-pressure level grid or an auxiliary pressure field
      grid, the result grid will be based on the ensemble mean surface pressure
      or auxiliary pressure field computed from the members specified in
      @p selectedMembers. In this case, the ensemble processing algorithm in
      @p produceData() MUST, for each input member, call the incremental
      update method @p updateAuxDataInResultGrid(), as well as call
      @p finalizeAuxDataInResultGrid() after all members have been processed
      and the result grid is returned.
     */
    MStructuredGrid* createAndInitializeResultGrid(
            MStructuredGrid *templateGrid,
            const QSet<unsigned int> &selectedMembers=QSet<unsigned int>());

    /**
      See documentation of @ref createAndInitializeResultGrid().
     */
    void updateAuxDataInResultGrid(MStructuredGrid *resultGrid,
                                   MStructuredGrid *currentMemberGrid);

    /**
      See documentation of @ref createAndInitializeResultGrid().
     */
    void finalizeAuxDataInResultGrid(MStructuredGrid *resultGrid);

    QMap<MStructuredGrid*, MStructuredGrid*> resultAuxComputationValidMembersCounter;
};

} // namespace Met3D

#endif // STRUCTUREDGRIDENSEMBLEFILTER_H
