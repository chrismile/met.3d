/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2020 Marcel Meyer
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
**
**  + Computer Graphics and Visualization Group
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
#ifndef DERIVEDDATAFIELDPROCESSOR_H
#define DERIVEDDATAFIELDPROCESSOR_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "data/processingwpdatasource.h"
#include "data/structuredgrid.h"
#include "data/datarequest.h"



namespace Met3D
{

/**
 @brief The MDerivedDataFieldProcessor class is the abstract base class for all
 classes that derive a data field, e.g., wind speed or potential temperature.
 */
class MDerivedDataFieldProcessor
{
public:
    MDerivedDataFieldProcessor(QString standardName,
                               QStringList requiredInputVariables);

    virtual ~MDerivedDataFieldProcessor() {}

    QString getStandardName() { return standardName; }

    QStringList getRequiredInputVariables() { return requiredInputVariables; }

    /**
      This method computes the derived data field and needs to be implemented
      in any derived class.

      It is called from @ref MDerivedMetVarsDataSource::produceData() if the
      corresponding variable is requested.

      @p inputGrids contains the required input data fields in the order
      specified in @ref requiredInputVariables. @p derivedGrid contains
      a pre-initialized result grid that already contains lon/lat/lev etc.
      information copied from the first grid in @p inputGrids.
     */
    virtual void compute(QList<MStructuredGrid*>& inputGrids,
                         MStructuredGrid *derivedGrid) = 0;

private:
    QString standardName;
    QStringList requiredInputVariables;
};

}

#endif // DERIVEDDATAFIELDPROCESSOR_H
