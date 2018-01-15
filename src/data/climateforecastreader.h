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
            bool convertGeometricHeightToPressure_ICAOStandard=false);
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

protected:
    QString variableSurfacePressureName(MVerticalLevelType levelType,
                                        const QString&     variableName);
    MHorizontalGridType variableHorizontalGridType(MVerticalLevelType levelType,
                                       const QString& variableName);
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

    bool treatRotatedGridAsRegularGrid;
    bool convertGeometricHeightToPressure_ICAOStandard;
};


} // namespace Met3D

#endif // CLIMATEFORECASTREADER_H
