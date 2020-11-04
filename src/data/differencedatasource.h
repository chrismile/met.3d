/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2018 Marc Rautenhaus [*, previously +]
**  Copyright 2018 Bianca Tost [+]
**
**  + Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
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
#ifndef DIFFERENCEDATASOURCE_H
#define DIFFERENCEDATASOURCE_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "processingwpdatasource.h"
#include "structuredgrid.h"
#include "datarequest.h"

namespace Met3D
{

/**
  @brief MDifferenceDataSource derives meteorological variables from basic
  forecast parameters.

  This class is "work in progress" and not yet documented.
  If created through a pipeline configuration file, the difference module
  can be created in the [ConfigurablePipeline] section. Important is that
  two "base requests" are defined that map the request that this
  class receives in the pipeline to two input requests that can be passed on
  to the input sources. See @ref constructInputSourceRequestFromRequest().

  Example:

      [NWPPipeline]
      size=2

      1\name=ECMWF ANALYSIS
      ...

      2\name=ECMWF ENSEMBLE
      ...

      [ConfigurablePipeline]
      size=2

      1\type=DIFFERENCE
      1\name=Difference to z(an)
      1\input1=ECMWF ENSEMBLE ENSFilter
      1\input2=ECMWF ANALYSIS ENSFilter
      1\baseRequest1="LEVELTYPE=REQUESTED_LEVELTYPE;VARIABLE=REQUESTED_VARIABLE;INIT_TIME=REQUESTED_INIT_TIME;VALID_TIME=REQUESTED_VALID_TIME;MEMBER=REQUESTED_MEMBER"
      1\baseRequest2="LEVELTYPE=1;VARIABLE=z (an);INIT_TIME=REQUESTED_VALID_TIME;VALID_TIME=REQUESTED_VALID_TIME;MEMBER=0"
      1\schedulerID=MultiThread
      1\memoryManagerID=NWP
      1\enableRegridding=false

      2\type=DIFFERENCE
      2\name=ECMWF NAWDEX PL ENSEMBLE 6h tendency
      2\input1=ECMWF NAWDEX ANALYSIS ENSFilter
      2\input2=ECMWF NAWDEX ANALYSIS ENSFilter
      2\baseRequest1="LEVELTYPE=REQUESTED_LEVELTYPE;VARIABLE=REQUESTED_VARIABLE;INIT_TIME=REQUESTED_INIT_TIME;VALID_TIME=REQUESTED_VALID_TIME;MEMBER=REQUESTED_MEMBER"
      2\baseRequest2="LEVELTYPE=REQUESTED_LEVELTYPE;VARIABLE=REQUESTED_VARIABLE;INIT_TIME=TIMEDIFF_-6_HOURS_REQUESTED_INIT_TIME;VALID_TIME=TIMEDIFF_-6_HOURS_REQUESTED_VALID_TIME;MEMBER=REQUESTED_MEMBER"
      2\schedulerID=MultiThread
      2\memoryManagerID=NWP
      2\enableRegridding=false

  @todo Correct determination of available init/valid times is still missing.
  This can lead to missing input grids and nullptr fields to be returned --
  potential segmentation faults! See @ref availableValidTimes().
  */
class MDifferenceDataSource
        : public MProcessingWeatherPredictionDataSource
{
public:
    MDifferenceDataSource();

    void setInputSource(int id, MWeatherPredictionDataSource* s);
    void setBaseRequest(int id, MDataRequest request);

    MStructuredGrid* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

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

    MDataRequest constructInputSourceRequestFromRequest(int id,
                                                        MDataRequest request);

    QVector<MWeatherPredictionDataSource*> inputSource;
    QVector<MDataRequest> baseRequest;
};

} // namespace Met3D

#endif // DIFFERENCEDATASOURCE_H
