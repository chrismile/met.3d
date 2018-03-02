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
  @brief Super class for data sources processing data of input sources.
  */
class MProcessingWeatherPredictionDataSource
        : public MWeatherPredictionDataSource
{
public:
    MProcessingWeatherPredictionDataSource();

protected:
    virtual MStructuredGrid *createAndInitializeResultGrid(
            MStructuredGrid *templateGrid);

};

} // namespace Met3D

#endif // PROCESSINGWEATHERPREDICTIONDATASOURCE_H
