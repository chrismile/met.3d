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
#include "ecmwfcfreader.h"

// standard library imports
#include <iostream>
#include <limits>
#include "assert.h"

// related third party imports
#include <netcdf>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "util/mstopwatch.h"
#include "gxfw/mglresourcesmanager.h"
#include "data/nccfvar.h"

using namespace std;
using namespace netCDF;
using namespace netCDF::exceptions;


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MECMWFClimateForecastReader::MECMWFClimateForecastReader(QString identifier)
    : MClimateForecastReader(identifier)
{
    // This regular expression is used to parse forecast file names of format
    // "20110808_00_ecmwf_forecast.T.EUR_LL015.036.ml.nc".
    reForecastFile.setPattern(
                "(\\d{8}_\\d{2})_ecmwf_forecast\\.(.*)\\.(.*)\\.(\\d{3})\\.(.*)\\.nc$");
    ecmwfFileFilter = "*_ecmwf_forecast.*.%1.*.nc";

    useFilenameAndDomainInfo = true;
}


MECMWFEnsembleClimateForecastReader::MECMWFEnsembleClimateForecastReader(
        QString identifier)
    : MECMWFClimateForecastReader(identifier)
{
    // This regular expression is used to parse forecast file names of format
    // "20120919_00_ecmwf_ensemble_forecast.EQPT_derived.EUR_LL10.096.ml.nc".
    reForecastFile.setPattern(
                "(\\d{8}_\\d{2})_ecmwf_ensemble_forecast\\.(.*)\\.(.*)\\.(\\d{3})\\.(.*)\\.nc$");
    ecmwfFileFilter = "*_ecmwf_ensemble_forecast.*.%1.*.nc";

    useFilenameAndDomainInfo = true;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MECMWFClimateForecastReader::setDataRoot(QString path, QString domain)
{
    this->domain = domain;
    fileFilter = ecmwfFileFilter.arg(domain);
    MAbstractDataReader::setDataRoot(path);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

bool MECMWFClimateForecastReader::parseForecastFileName(
        QString   filename,
        QDateTime *initTime,
        QString   *domain,
        QString   *levelType)
{
    // If the given filename cannot be matched to the regular expression,
    // return false..
    if (reForecastFile.indexIn(filename) < 0) return false;

    // ..otherwise the regular expression has been matched. The capturedTexts()
    // method returns the values of the regexp groups that have been matched.
    QStringList parsedValues = reForecastFile.capturedTexts();

    // Group 1 contains the initialisation time of the forecast.
    *initTime = QDateTime::fromString(parsedValues.at(1), "yyyyMMdd_hh");
    initTime->setTimeSpec(Qt::UTC);

    // Group 2 contains the variable identifier .. currently not needed.

    // Group 3 contains the domain identifier.
    *domain = parsedValues.at(3);

    // Group 4 contains the ECMWF forecast trigger (036, 072 etc.) .. currently
    // not needed.

    // Group 5 contains the level type of the data.
    *levelType = parsedValues.at(5);

    // The filename has been successfully parsed.
    return true;
}


} // namespace Met3D
