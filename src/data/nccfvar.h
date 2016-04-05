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
#ifndef NCCFVAR_H
#define NCCFVAR_H

// standard library imports

// related third party imports
#include <netcdf>
#include <QtCore>

// local application imports

using namespace std;


// C++ API for netCDF4.
namespace netCDF
{

/**
  @brief NcCFVar represents a NetCDF variable compliant with the CF-conventions
  (http://cf-pcmdi.llnl.gov/).

  NcCFVar inherits the NcVar class to add methods that provide CF-functionality
  (e.g. to determine the coordinate variables and to get the time values of the
  variable).
  */
class NcCFVar : public NcVar
{
public:
    /**
      Type codes for @ref isCFDataVariable() and @ref getGridType().
      */
    enum NcVariableGridType {
        UNDEFINED      = 0,
        // all CF variables
        ALL            = 1,
        // 2D regular lat/lon grid
        LAT_LON        = 2,
        // 2D regular lat/lon grid with pressure as third dimension
        LAT_LON_P      = 3,
        // 2D regular lat/lon grid with hybrid model level as third dimension
        LAT_LON_HYBRID = 4,
        // 2D regular lat/lon grid with potential vorticity as third dimension
        LAT_LON_PVU    = 5
    };

    /**
      Default constructur, generates a "null object".
      */
    NcCFVar();

    /**
      Constructs a new NcCFVar object from an existing NcVar object. Determines
      the @ref NcVariableGridType of the variable.
      */
    NcCFVar(const NcVar& rhs);

    /**
      Returns the latitude variable corresponding to the CF variable
      represented by this object.
      */
    NcVar getLatitudeVar();

    /**
      Returns the longitude variable corresponding to the CF variable
      represented by this object.
      */
    NcVar getLongitudeVar();

    /**
      If the CF variable represented by this object has a pressure dimension,
      getVerticalCoordinatePressure() returns the corresponding pressure
      variable. If no pressure dimension exists, an @ref NcException is thrown.
      */
    NcVar getVerticalCoordinatePressure();

    /**
      If the CF variable represented by this object has a hybrid sigma pressure
      model level dimension, getVerticalCoordinateHybridSigmaPressure() returns
      the corresponding vertical hybrid variable. If no such model level
      dimension exists, an @ref NcException is thrown.
      */
    NcVar getVerticalCoordinateHybridSigmaPressure(NcVar* ap,
                                                   NcVar* b,
                                                   QString* psfcName);

    /**
      If the CF variable represented by this object has a potential vorticity
      dimension, getVerticalCoordinateModelLevel() returns the corresponding
      PVU variable. If no PVU dimension exists, an @ref NcException is thrown.
      */
    NcVar getVerticalCoordinatePotVort();

    /**
      Returns TRUE if this CF variable has an ensemble dimension.
     */
    bool hasEnsembleDimension();

    /**
      If the CF variable represented by this object has an ensemble dimension,
      getEnsembleVar() returns the corresponding coordinate variable. If no
      ensemble dimension exists, an @ref NcException is thrown.
      */
    NcVar getEnsembleVar();

    /**
      Returns a set of available ensemble members, if an ensemble dimension
      is available for this variable.
     */
    QSet<unsigned int> getEnsembleMembers(
            QHash<unsigned int, unsigned int> *memberToFileIndexMap = nullptr);

    /**
      Returns TRUE if this CF variable has a time dimension.
     */
    bool hasTimeDimension();

    /**
      Returns the time variable corresponding to the CF variable represented by
      this object.
      */
    NcVar getTimeVar();

    /**
      Tries to match one of the coordinate (=dimension) variables of this CF
      variable with the given arguments. The matched variable is identified by
      either the specifed CF standard name (@p standardname), or by the
      presence of one of the specifed @p units attributes. If @p
      requirepositive is set to true, the presence of the "positive" attribute
      is checked. Its value has to be either "up" or "down". (See the CF specs
      for further information).
      */
    NcVar getCFCoordinateVar(const vector<string>& units,
                             const string& standardname="",
                             bool requirepositive=false) const;

    /**
      Returns a list of the time values for which this variable contains data.
      */
    QList<QDateTime> getTimeValues();

    /**
      Returns the base time of the time variable. This is often identical with
      the forecast base time (= initialisation time).
      */
    QDateTime getBaseTime();

    /**
      Interprets a time string stored in an attribute of the current variable.
      The name of the attribute needs to be given.
     */
    QDateTime getTimeFromAttribute(QString attributeName);

    /**
      Returns the grid type of this variable.
      */
    NcVariableGridType getGridType();

    /**
      Static function that checks if the specified @p var is a CF variable of
      one of the types defined in @ref NcVariableGridType.
      */
    static bool isCFDataVariable(const NcVar& var, NcVariableGridType type);

private:
    /**
      Parses the CF time units string @p units and extracts the encoded base
      time, offset from UTC and the time units (second, minute, hour, day or
      year).
      */
    bool parseTimeUnits(QString units, QDateTime *baseTime, double* utcOffset,
                        QString *timeUnit);

    // Regular expression describing valid CF time unit strings.
    QRegExp reTimeUnits;
    // The grid type of this variable, determined in the constructor.
    NcVariableGridType varGridType;

    // Stores the time variable (if present) to avoid multiple detection runs
    // in getTimeVar().
    NcVar timeVar;

};

} // namespace netCDF

#endif // NCCFVAR_H
