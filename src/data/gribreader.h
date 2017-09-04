/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2017-2018 Bianca Tost
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
#ifndef GRIBREADER_H
#define GRIBREADER_H

// standard library imports

// related third party imports
#include <QtCore>
#include "eccodes.h"

// local application imports
#include "data/weatherpredictionreader.h"
#include "data/structuredgrid.h"


namespace Met3D
{

enum MECMWFForecastType
{
    INVALID_TYPE           = 0,
    ANALYSIS               = 1,
    DETERMINISTIC_FORECAST = 2,
    ENSEMBLE_FORECAST      = 3
};


/** Information specific to a variable and timestep. */
struct MGribDatafieldInfo
{
    QString filename;                // file in which the variable is stored
    QMap<long, long> offsetForLevel; // byte offset at which the grib message
                                     // for a given vertical level is stored
    bool applyExp;                   // indicates whether to apply exponatial
                                     // function to data field or not.
};

// Define a hierarchy of dictionaries that provide fast access to where a
// specific datafield is stored.
typedef QMap<int, MGribDatafieldInfo> MGribEnsembleMemberMap;
typedef QMap<QDateTime, MGribEnsembleMemberMap> MGribValidTimeMap;
typedef QMap<QDateTime, MGribValidTimeMap> MGribInitTimeMap;

struct MGribVariableInfo
{
    MGribVariableInfo() : nlons(0), nlats(0),
        lon0(0), lat0(0), lon1(0), lat1(0), dlon(0), dlat(0),
        availableMembers_bitfield(0) {}

    // This struct stores variable-specific information, the hierarchy of
    // maps is continued in the field "timeMap".
    MGribInitTimeMap timeMap;
    QString variablename;        // variable name
    QString longname;            // CF-conventions
    QString standardname;        // CF-conventions
    QString units;               // CF-conventions
    MECMWFForecastType fcType;   // forecast type of this variable
    QString surfacePressureName; // for variables on hybrid model levels
                                 // the name of the var containing the
                                 // corresponding sfc pressure field
    QString auxiliaryPressureName; // for variables with auxiliary pressure
                                   // levels; the name of the var containing the
                                   // corresponding 3D pressure field
    MHorizontalGridType horizontalGridType;  // Enum representing the type of the grid.

    long nlons, nlats;
    double lon0, lat0, lon1, lat1, dlon, dlat;

    QVector<double> levels, lats, lons, aki_hPa, bki, ak_hPa, bk;
    QSet<unsigned int> availableMembers;
    quint64 availableMembers_bitfield;
};

typedef QMap<QString, MGribVariableInfo*> MGribVariableNameMap;
typedef QMap<MVerticalLevelType, MGribVariableNameMap> MGribLevelTypeMap;

struct MGribFileInfo
{
    FILE *gribFile;

    // Mutex to lock access to the file.
    QMutex accessMutex;
};

typedef QHash<QString, MGribFileInfo*> MGribOpenFileMap;

/**
  Struct that stores a grib message's "important" data fields for the file
  index (the index consists of structs of this type).
 */
struct MGribMessageIndexInfo
{
    MVerticalLevelType levelType;

    QString variablename;        // variable name
    QString longname;            // CF-conventions
    QString standardname;        // CF-conventions
    QString units;               // CF-conventions
    MECMWFForecastType fcType;   // forecast type of this variable
    QString surfacePressureName; // for variables on hybrid model levels
                                 // the name of the var containing the
                                 // corresponding sfc pressure field

    quint64 nlons, nlats;
    double lon0, lat0, lon1, lat1, dlon, dlat;

    QVector<double> lats, lons, aki_hPa, bki, ak_hPa, bk;

    qint64 ensMember;
    QDateTime initTime;
    QDateTime validTime;

    quint64 level;
    quint64 filePosition;

    void writeToDataStream(QDataStream *dataStream);
    void readFromDataStream(QDataStream *dataStream);
};


/**
  @brief Reader for ECMWF Grib files that are retrieved from the ECMWF MARS
  system (or passed from Metview).

  For 3D fields, Met.3D checks if all required model levels are available for
  all time steps.
  If levels are missing, the corresponding variable is discarded.

  @todo Move availableXYZ methods up in class hierarchy.
  */
class MGribReader : public MWeatherPredictionReader
{
public:
    /**
      The GRIB reader takes an argument @p surfacePressureFieldType that
      specified which surface pressure field is used for reconstruction
      of pressure from hybrid coordinated. Can be "sp", "lnsp" or "auto".
      If set to "auto", the reader tries to detect the available field--this
      unfortunately currently requires scanning through all messages: SLOW...
     */
    MGribReader(QString identifier, QString surfacePressureFieldType,
                bool disableGridConsistencyCheck=false);
    ~MGribReader();

    QList<MVerticalLevelType> availableLevelTypes();

    QStringList availableVariables(MVerticalLevelType levelType);

    QSet<unsigned int> availableEnsembleMembers(MVerticalLevelType levelType,
                                                const QString&     variableName);

    QList<QDateTime> availableInitTimes(MVerticalLevelType levelType,
                                        const QString&     variableName);

    QList<QDateTime> availableValidTimes(MVerticalLevelType levelType,
                                         const QString&     variableName,
                                         const QDateTime&   initTime);

    QString variableLongName(MVerticalLevelType levelType,
                             const QString&     variableName);

    QString variableStandardName(MVerticalLevelType levelType,
                                 const QString&     variableName);

    QString variableUnits(MVerticalLevelType levelType,
                          const QString&     variableName);

protected:
    QString variableSurfacePressureName(MVerticalLevelType levelType,
                                        const QString&     variableName);
    QString variableAuxiliaryPressureName(MVerticalLevelType levelType,
                                          const QString&     variableName);

    MHorizontalGridType variableHorizontalGridType(MVerticalLevelType levelType,
                                       const QString&     variableName);

    QVector2D variableRotatedNorthPoleCoordinates(MVerticalLevelType levelType,
                                                  const QString& variableName);

    MStructuredGrid* readGrid(MVerticalLevelType levelType,
                              const QString&     variableName,
                              const QDateTime&   initTime,
                              const QDateTime&   validTime,
                              unsigned int       ensembleMember);

    void scanDataRoot();

    bool isValidGribFile(QString path);

    QStringList getGribIndexStringKeyList(grib_index* gribIndex, QString key);

    QList<long> getGribIndexLongKeyList(grib_index* gribIndex, QString key);

    QList<double> getGribIndexDoubleKeyList(grib_index* gribIndex, QString key);

    template<typename T> QString keyListToString(QList<T> keyList);

    QString getGribStringKey(grib_handle* gh, QString key);

    long getGribLongKey(grib_handle* gh, QString key);

    double getGribDoubleKey(grib_handle* gh, QString key);

    void debugPrintLevelTypeMap(MGribLevelTypeMap& m);

    bool checkIndexForVariable(MGribVariableInfo* vinfo);

    QString forecastTypeToString(MECMWFForecastType type);

    /**
     Detect type of surface pressure field used for reconstruction of pressure
     for hybrid sigma-pressure levels. Possible options: surface pressure is
     stored in Pa in a field "sp", or as the logarithm of sp in a field "lnsp".
     To make things complicated, the latter is stored at ECMWF as a single
     model level, sp is stored as a surface field.

     Detection is currently implemented by searching all grib messages for
     either "sp" or "lnsp" fields.

     We need the sp/lnsp information to correctly set the "surfacePressureName"
     variable of hybrid sigma pressure grids. The information is stored in
     the internal variable->grib message mapping as well as in the index files.
     Hence, sp/lnsp currently needs to be detected BEFORE any hybrid grids
     are scanned in @ref scanDataRoot().
     */
    void detectSurfacePressureFieldType(QStringList *availableFiles);

    /**
     Sets @ref surfacePressureFieldType to @p surfacePressureFieldType.
     If @ref surfacePressureFieldType has not been set before, print which
     type was detected.
     */
    void setSurfacePressureFieldType(QString surfacePressureFieldType);

    /**
     Checks consistency of horizontal geographical region data stored in
     @p referenceVInfo and @p currentVInfo.

     Horizontal geographical region data includes start and end lons and lats,
     grid spacing in lon and lat direction, number of lons and lats and one
     vector containing all longitudes and one containing all latitudes.
     */
    bool checkConsistencyOfVariable(MGribVariableInfo *referenceVInfo,
                                    MGribVariableInfo *currentVInfo);

    MGribLevelTypeMap availableDataFields;
    MGribLevelTypeMap availableDataFieldsByStdName;
    QReadWriteLock availableItemsLock;

    MGribOpenFileMap openFiles;
    QMutex openFilesMutex;

    QString surfacePressureFieldType; // for hybrid grids: sp, lnsp, <empty>
    bool disableGridConsistencyCheck;
};


} // namespace Met3D

#endif // GRIBREADER_H
