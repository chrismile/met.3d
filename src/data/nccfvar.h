/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2020 Kameswarro Modali [*]
**  Copyright 2017-2018 Bianca Tost [+]
**
**  + Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  * Regional Computing Center, Visual Data Analysis Group
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
  Inline function defined depending on the version of NetCDF-4 C++ library since
  the signatur of @ref NcException() changed from version 4.2 to 4.3.
*/
#if NetCDFCXX4_VERSION_MAJOR > 4 || (NetCDFCXX4_VERSION_MAJOR == 4 \
    && NetCDFCXX4_VERSION_MINOR >= 3)
// Newer version of NetCDF-4 C++
inline netCDF::exceptions::NcException MNcException(
        const std::string& exceptionName, const std::string& complaint,
        const char* fileName, int lineNumber)
{
    Q_UNUSED(exceptionName);
    return netCDF::exceptions::NcException(complaint.c_str(), fileName,
                                           lineNumber);
}
#else
// Older version of NetCDF-4 C++
inline netCDF::exceptions::NcException MNcException(
        const std::string& exceptionName, const std::string& complaint,
        const char* fileName, int lineNumber)
{
    return netCDF::exceptions::NcException(exceptionName, complaint, fileName,
                                           lineNumber);
}
#endif

/**
  @brief NcCFVar represents a NetCDF variable compliant with the CF-conventions
  (http://cfconventions.org/).

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
    enum NcVariableGridType
    {
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
        LAT_LON_PVU    = 5,
        // 2D regular lat/lon grid with geometric height as third dimension
        LAT_LON_Z      = 6,
        // 2D regular lat/lon grid with hybrid model level as third dimension
        // stored as 3D pressure field
        LAT_LON_AUXP3D  = 7
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
      Returns the rotated grid latitude variable corresponding to the CF
      variable represented by this object.

      (cf. http://www.cosmo-model.org/content/model/documentation/core/cosmoDyncsNumcs.pdf ,
       chapter 3.3)
      */
    NcVar getRotatedLatitudeVar();

    /**
      Returns the projected grid coordinate along the Y direction
      of the variable corresponding to the CF variable represented by this object.
      */
    NcVar getProjectionYCoordinateVar();

    /**
      Returns the longitude variable corresponding to the CF variable
      represented by this object.
      */
    NcVar getLongitudeVar();

    /**
      Returns the rotated grid longitude variable corresponding to the CF
      variable represented by this object.

      (cf. http://www.cosmo-model.org/content/model/documentation/core/cosmoDyncsNumcs.pdf ,
       chapter 3.3)
      */
    NcVar getRotatedLongitudeVar();

    /**
      Returns the projected grid coordinate along the X direction
      of the variable corresponding to the CF variable represented by this object.
      */
    NcVar getProjectionXCoordinateVar();

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
      If the CF variable represented by this object has a model level dimension
      stored as a 3D pressure field, getVerticalCoordinateAuxiliaryPressure()
      returns the corresponding vertical pressure field variable. If no such
      model level dimension exists or @p auxiliary3DPressureField is an empty
      string, an @ref NcException is thrown.

      If the auxiliary pressure variable is stored in the same file, it is
      checked whether the number of dimensions of the auxiliary pressure field
      variable matches the number of dimensions of this variable. If @p
      disableGridConsistencyCheck is false, it is also checked that the names
      of the dimensions match.

      If the auxiliary pressure variable is not stored in the same file, the
      variable is assumed to be an auxiliary pressure variable without the
      dimension checks previously mentioned and an empty NcVar is returned.
      */
    NcVar getVerticalCoordinateAuxiliaryPressure(
            QString auxiliary3DPressureField, QString *pressureName,
            int *levelIndex, bool disableGridConsistencyCheck);

    /**
      If the CF variable represented by this object has a potential vorticity
      dimension, getVerticalCoordinateModelLevel() returns the corresponding
      PVU variable. If no PVU dimension exists, an @ref NcException is thrown.
      */
    NcVar getVerticalCoordinatePotVort();

    /**
      If the CF variable represented by this object has a geometric height
      dimension, getVerticalCoordinateGeometricHeight() returns the
      corresponding geometric height variable. If no geometric height dimension
      exists, an @ref NcException is thrown.
      */
    NcVar getVerticalCoordinateGeometricHeight();

    static QSet<QString> validUnitsVerticalCoordinate(NcVariableGridType gridType);

    static QString validUnitsVerticalCoordinateAsString(NcVariableGridType gridType);

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
                             const vector<string> &standardname={""},
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

      @p auxiliary3DPressureField contains the name of the auxiliary pressure
      field variable if one should be used otherwise it is empty.

      @p disableGridConsistencyCheck specifies if consistency checks should be
      disabled or not. If the check is not disabled, it is checked whether the
      names of the dimensions of the auxiliary pressure field match the names
      of the dimensions of a variable which could be detected as auxiliary
      pressure variable. But the check is only performed here if the auxiliary
      pressure field variable is stored in the same file as the variable for
      which the grid type should be detected.
      */
    NcVariableGridType getGridType(QString auxiliary3DPressureField,
                                   bool disableGridConsistencyCheck);

    /**
      Static function that checks if the specified @p var is a CF variable of
      one of the types defined in @ref NcVariableGridType.
      */
    static bool isCFDataVariable(const NcVar& var, NcVariableGridType type,
                                 QString auxiliary3DPressureField="",
                                 bool disableGridConsistencyCheck=false);

    /**
      Static function that checks if the specified @p var is a CF variable of
      one of the types defined in @ref NcVariableGridType.

      ( cf. http://cfconventions.org/cf-conventions/v1.6.0/cf-conventions.html#grid-mappings-and-projections )
      */
    static bool isCFGridMappingVariable(const NcVar& var);

    /**
      Static function that checks if the specified @p var is a CF variable
      defined on a rotated grid with a mapping defined by one of the variables
      with their names stored in @p gridMappingVarNames. If so,
      @p gridMappingVarName holds the name of the corresponding grid mapping
      variable.
      */
    static bool isDefinedOnRotatedGrid(const NcVar& var,
                                       const QStringList gridMappingVarNames,
                                       QString *gridMappingVarName);

    /**
      Static function that checks if the specified @p var is a CF variable
      defined on a projected grid with a mapping defined by one of the variables
      with their names stored in @p gridMappingVarNames. If so,
      @p gridMappingVarName holds the name of the corresponding grid mapping
      variable.
      */
    static bool isDefinedOnProjectedGrid(const NcVar& var,
                                       const QStringList gridMappingVarNames,
                                       QString *gridMappingVarName);

    /**
      Static function that gets the rotated north pole coordinates for a given
      grid mapping variable @p gridMappingVar. The results are stored in
      @p rotatedNorthPoleLon and @p rotatedNorthPoleLat respectively.
      The function returns true if gridMappingVar is a rotated_lon_lat variable
      and it is able to get the coordinates.
      */
    static bool getRotatedNorthPoleCoordinates(const NcVar& gridMappingVar,
                                               float *rotatedNorthPoleLon,
                                               float *rotatedNorthPoleLat);

    /**
      Static function that converts an @ref NcVariableGridType enum to
      a string.
     */
    static QString ncVariableGridTypeToString(NcVariableGridType type);


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
