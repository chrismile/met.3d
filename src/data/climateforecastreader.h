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
#ifndef CLIMATEFORECASTREADER_H
#define CLIMATEFORECASTREADER_H

// standard library imports

// related third party imports
#include <QtCore>
#include <netcdf>

// local application imports
#include "data/weatherpredictionreader.h"
#include "data/structuredgrid.h"
#include "data/nccfvar.h"

namespace Met3D
{

/** Information specific to a variable and file. */
struct MVariableDataSharedPerFile
{
    netCDF::NcCFVar cfVar;
    netCDF::NcVar   latVar;
    netCDF::NcVar   lonVar;
    netCDF::NcVar   timeVar;
    netCDF::NcVar   vertVar;
    netCDF::NcVar   ensembleVar;

    QVector<double> levels, lats, lons, ak, bk;
    QList<QDateTime> timeCoordValues;
    QSet<unsigned int> availableMembers;
    QHash<unsigned int, unsigned int> memberToFileIndexMap;

    bool scaleAndOffsetProvided;
    double scale_factor, add_offset;

    bool reverseLatitudes;
    bool reverseLevels;
};

struct MNcVarDimensionInfo
{
    QList<QString> names;
    QList<int>     sizes;
};

typedef QHash<QString, MVariableDataSharedPerFile> MSharedDataVariableNameMap;
typedef QHash<MVerticalLevelType, MSharedDataVariableNameMap> MSharedDataLevelTypeMap;

struct MFileInfo
{
    MFileInfo()
        : ncFile(nullptr)
    { }

    ~MFileInfo()
    { if (ncFile) delete ncFile; }

    netCDF::NcFile *ncFile;
    MSharedDataLevelTypeMap sharedData;

    // Mutex to lock access to the file.
    QMutex accessMutex;
};

typedef QHash<QString, MFileInfo*> MOpenFileMap;


/**
  @brief Data reader for NetCDF files that follow (a subset of the) the CF
  (climate & forecast) conventions.

  @ref convertGeometricHeightToPressure_ICAOStandard specifies whether Met.3D
  should handle data fields defined on geometric height by converting the height
  to pressure levels using the ICAO standard atmosphere.

  @ref auxiliary3DPressureField contains the name of the auxiliary pressure
  field variable if one should be used for the data set. If it is empty, no auxiliary
  pressure field is used.

  @ref disableGridConsistencyCheck specifies whether Met.3D should disable the
  consistency check which is performed to make sure that all variables in one
  data set are defined on the same grid (exception: different vertical level
  types are allowed in one data set).

  @note While a number of CF keywords are understood this module does not
  implement the entire conventions. If something is missing that you require,
  please consider contributing to the software by implementing it!

  @see http://cfconventions.org/
  */
class MClimateForecastReader : public MWeatherPredictionReader
{
public:
    MClimateForecastReader(
            QString identifier,
            bool treatRotatedGridAsRegularGrid=false,
            bool convertGeometricHeightToPressure_ICAOStandard=false,
            QString auxiliary3DPressureField="",
            bool disableGridConsistencyCheck=false);
    ~MClimateForecastReader();

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

    MHorizontalGridType variableHorizontalGridType(MVerticalLevelType levelType,
                                                   const QString& variableName);

protected:
    QString variableSurfacePressureName(MVerticalLevelType levelType,
                                        const QString&     variableName);
    QString variableAuxiliaryPressureName(MVerticalLevelType levelType,
                                          const QString&     variableName);
    QVector2D variableRotatedNorthPoleCoordinates( MVerticalLevelType levelType,
                                                   const QString& variableName);

    void scanDataRoot();

    MStructuredGrid* readGrid(MVerticalLevelType levelType,
                              const QString&     variableName,
                              const QDateTime&   initTime,
                              const QDateTime&   validTime,
                              unsigned int       ensembleMember);

    // Dictionaries of available data. Access needs to be proteced
    // by the provided read/write lock.
    MLevelTypeMap availableDataFields;
    MLevelTypeMap availableDataFieldsByStdName;
    QReadWriteLock availableItemsLock;

    // Dictionary of open file information. Read/write access to this
    // dictionary must be protected by the provided mutex.
    MOpenFileMap  openFiles;
    QMutex openFilesMutex;

    /**
      Determine the name of the file that contains the specified data field.
      */
    QString dataFieldFile(MVerticalLevelType levelType,
                          const QString&     variableName,
                          const QDateTime&   initTime,
                          const QDateTime&   validTime);

    // Maps NetCDF variable names to standard names.
    QMap<QString, QString> variableToStandardNameMap;

    /**
      Loads the table with variable name -> CF standard name mappings. The
      table is applied if the standard name of a variable is not stored in
      an attribute in the NetCDF file.
     */
    bool parseCfStandardNameFile(const QString& filename);

    /**
      Checks consistency of shared data associated with the NetCDF variable @p
      cfVar of the vertical level type @p levelType. Shared data includes
      longitudes, latitudes, levels, etc., that are read only once and re-used
      for all time steps of a variable. This function ensures that all time
      steps are defined on, e.g., the same grid.

      If @p initialiseConsistencyData is true @p shared is initialised with the
      shared data data obtained from @p cfVar. This is the data that subsequent
      calls to this method will be checked against for consistency.

      If @p initialiseConsistencyData is false the shared data of @p cfVar are
      compared to the corresponding data stored in @p shared.

      Set @p testEnsembleMemberConsistency to true to also test consistency in
      ensemble members (consistency of one variable) and to false to disable
      the ensemble members check (consistency of dataset).

      @return false if @p initialiseConsistencyData is false and the shared
      data of @p cfVar does not match the corresponding data in @p shared; true
      otherwise.
     */
    bool checkSharedVariableDataConsistency(
            MVariableDataSharedPerFile *shared,
            netCDF::NcCFVar *cfVar,
            MVerticalLevelType levelType,
            bool initialiseConsistencyData,
            bool testEnsembleMemberConsistency);

    bool treatRotatedGridAsRegularGrid;
    bool convertGeometricHeightToPressure_ICAOStandard;
    bool disableGridConsistencyCheck;
};


} // namespace Met3D

#endif // CLIMATEFORECASTREADER_H
