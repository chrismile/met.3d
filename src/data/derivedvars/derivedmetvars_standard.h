/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2020 Marcel Meyer [*]
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
#ifndef DERIVEDMETVARS_STANDARD_H
#define DERIVEDMETVARS_STANDARD_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "data/processingwpdatasource.h"
#include "data/structuredgrid.h"
#include "data/datarequest.h"
#include "data/derivedvars/derivedmetvarsdatasource.h"


namespace Met3D
{

/******************************************************************************
***                            DATA PROCESSORS                              ***
*******************************************************************************/

class MHorizontalWindSpeedProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MHorizontalWindSpeedProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MMagnitudeOfAirVelocityProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MMagnitudeOfAirVelocityProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MPotentialTemperatureProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MPotentialTemperatureProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MEquivalentPotentialTemperatureProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MEquivalentPotentialTemperatureProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MRelativeHumdityProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MRelativeHumdityProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MPotentialVorticityProcessor_LAGRANTOcalvar
        : public MDerivedDataFieldProcessor
{
public:
    MPotentialVorticityProcessor_LAGRANTOcalvar();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MGeopotentialHeightProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MGeopotentialHeightProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MGeopotentialHeightFromGeopotentialProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MGeopotentialHeightFromGeopotentialProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MDewPointTemperatureProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MDewPointTemperatureProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MTHourlyTotalPrecipitationProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MTHourlyTotalPrecipitationProcessor(int hours);

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MMagnitudeOfVerticallyIntegratedMoistureFluxProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MMagnitudeOfVerticallyIntegratedMoistureFluxProcessor(
            QString levelTypeString);

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MPressureProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MPressureProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};

}

#endif // DERIVEDMETVARS_STANDARD_H
