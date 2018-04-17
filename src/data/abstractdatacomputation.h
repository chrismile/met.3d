/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Philipp Kaiser
**  Copyright 2017 Marc Rautenhaus
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
#ifndef MET_3D_ABSTRACTDATACOMPUTATION_H
#define MET_3D_ABSTRACTDATACOMPUTATION_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "data/weatherpredictiondatasource.h"


namespace Met3D
{

/**
  @brief MAbstractDataComputation is the base class for all data computations.
 */
class MAbstractDataComputation
{
public:
    MAbstractDataComputation(QString identifier);
    virtual ~MAbstractDataComputation();

    /**
      Returns the identifier string of this data computation.
     */
    QString getIdentifier() { return identifier; }

    /**
      Set input source for computations.
      @param reader is a pointer to a MWeatherPredictionDataSource instance.
     */
    void setInputSource(MWeatherPredictionDataSource* source);

protected:
    /**
      Will be called when input dataSource is set.

      Needs to implement initialisation from data drawn from dataSource (e.g.
      valid times, ensemble members, etc.).
     */
    virtual void initialiseFormDataSource() = 0;

    QString identifier;
    MWeatherPredictionDataSource* dataSource;

};


} // namespace Met3D

#endif // MET_3D_ABSTRACTDATACOMPUTATION_H
