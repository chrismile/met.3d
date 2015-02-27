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
#ifndef ECMWFCFREADER_H
#define ECMWFCFREADER_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "data/climateforecastreader.h"


namespace Met3D
{

/**
  @brief MECMWFClimateForecastReader implements some functions specific to
  NetCDF-CF files that are output by the MSS (DLR Mission Support System)
  data system (for instance, filename parsing).

  Works for deterministic forecast data. For ensemble data, see
  @ref MECMWFEnsembleClimateForecastReader
  */
class MECMWFClimateForecastReader : public MClimateForecastReader
{
public:
    MECMWFClimateForecastReader(QString identifier);

    /**
      Overloads @ref MAbstractDataReader::setDataRoot() to add the additional
      @p domain specific to ECMWF datasets.
      */
    void setDataRoot(QString path, QString domain);

protected:
    QRegExp reForecastFile; // regular expression used to parse the filenames
    QString ecmwfFileFilter;

    /**
      Parses the filename of an ECMWF file of format
      "20120206_00_ecmwf_forecast.T.EUR_LL015.036.ml.nc". The initialisation
      time, domain identifier and level type of the data stored in the file are
      extracted. The returned @p initTime is in UTC.

      @note Shouldn't the metadata stored in the filename go into the NetCDF
      attributes? (mr 04Nov2011)
      */
    bool parseForecastFileName(QString   filename,
                               QDateTime *initTime,
                               QString   *domain,
                               QString   *levelType) override;

};


/**
  @brief Same as @ref MECMWFClimateForecastReader, but for ensemble datasets.
  */
class MECMWFEnsembleClimateForecastReader : public MECMWFClimateForecastReader
{
public:
    MECMWFEnsembleClimateForecastReader(QString identifier);

};


} // namespace Met3D

#endif // ECMWFCFREADER_H
