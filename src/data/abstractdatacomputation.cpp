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
#include "abstractdatacomputation.h"

// standard library imports

// related third party imports

// local application imports

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MAbstractDataComputationSource::MAbstractDataComputationSource(QString identifier)
        : identifier(identifier),
          dataSource(nullptr)
{
}


MAbstractDataComputationSource::~MAbstractDataComputationSource()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MAbstractDataComputationSource::setInputSource(MWeatherPredictionDataSource* source)
{
    assert(source != nullptr);
    dataSource = source;
    initialiseFromDataSource();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

} // namespace Met3D
