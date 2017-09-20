/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015-2017 Bianca Tost
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
#include "grib_api.h"

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
  system (e.g. from Metview or for input into the DLR Mission Support System).

  The grib files currently (still) need to follow a specific formatting.

  @todo Move availableXYZ methods up in class hierarchy.
  */
class MGribReader : public MWeatherPredictionReader
{
public:
    MGribReader(QString identifier, QString surfacePressureFieldType);
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
     Detects surface pressure field type for this data source if not done
     already.

     We need the type to set the surfacePressureName of hybrid sigma pressure
     variables correctly thus we need to detect it before writing any index file for
     this data source but only once for all files belonging to the data source and
     only if we need to write index files (In the index files lnsp fields are stored
     as surface fields instead of hybrid fields.)
     */
    void detectSurfacePressureFieldType(QStringList *availableFiles);

    /**
     Sets @ref surfacePressureFieldType to @p surfacePressureFieldType and
     if @ref surfacePressureFieldType has not been set yet and prints which
     type was detected.
     */
    void setSurfacePressureFieldType(QString surfacePressureFieldType);

    MGribLevelTypeMap availableDataFields;
    MGribLevelTypeMap availableDataFieldsByStdName;
    QReadWriteLock availableItemsLock;

    MGribOpenFileMap openFiles;
    QMutex openFilesMutex;

    /**
      Stores whether the surface pressure fields of this data source are sp,
      lnsp or none.
      */
    QString surfacePressureFieldType;
};


} // namespace Met3D

#endif // GRIBREADER_H
