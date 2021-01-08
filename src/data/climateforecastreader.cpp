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
#include "climateforecastreader.h"

// standard library imports
#include <iostream>
#include <limits>
#include <fstream>

// related third party imports
#include <netcdf>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/metroutines.h"
#include "util/mexception.h"
#include "util/mstopwatch.h"
#include "gxfw/mglresourcesmanager.h"
#include "data/nccfvar.h"

using namespace std;
using namespace netCDF;
using namespace netCDF::exceptions;

//#define MSTOPWATCH_ENABLED

namespace Met3D
{


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MClimateForecastReader::MClimateForecastReader(
        QString identifier, bool treatRotatedGridAsRegularGrid,
        bool treatProjectedGridAsRegularLonLatGrid,
        bool convertGeometricHeightToPressure_ICAOStandard,
        QString auxiliary3DPressureField, bool disableGridConsistencyCheck)
    : MWeatherPredictionReader(identifier, auxiliary3DPressureField),
      treatRotatedGridAsRegularGrid(treatRotatedGridAsRegularGrid),
      treatProjectedGridAsRegularLonLatGrid(treatProjectedGridAsRegularLonLatGrid),
      convertGeometricHeightToPressure_ICAOStandard(
          convertGeometricHeightToPressure_ICAOStandard),
      disableGridConsistencyCheck(disableGridConsistencyCheck),
      ensembleIDIsSpecifiedInFileName(false)

{
    // Read mapping "variable name to CF standard name", specific to ECMWF
    // forecasts converted to NetCDF with netcdf-java.
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    parseCfStandardNameFile(sysMC->getMet3DHomeDir().absoluteFilePath(
                                "config/cf_stdnames.dat"));
}


MClimateForecastReader::~MClimateForecastReader()
{
    QMutexLocker openFilesLocker(&openFilesMutex);

    // Close open NetCDF files.
    foreach (MFileInfo* finfo, openFiles) delete finfo;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QList<MVerticalLevelType> MClimateForecastReader::availableLevelTypes()
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    return availableDataFields.keys();
}


QStringList MClimateForecastReader::availableVariables(
        MVerticalLevelType levelType)
{
    // Reading values with []-operator results in non deterministic crashes
    // when using multi threading and QReadLocker. Thus it is necessary to use
    // QMap's value()-method or QWriteLocker. But since these operations should
    // only be reading operations, using the value()-method is prefered.

    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    return availableDataFields.value(levelType).keys();
}


QSet<unsigned int> MClimateForecastReader::availableEnsembleMembers(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);

    if (availableDataFields.value(levelType).keys().contains(variableName))
    {
        return availableDataFields.value(levelType).value(variableName)
                ->availableMembers;
    }
    else if (availableDataFieldsByStdName.value(levelType).keys().contains(
                 variableName))
    {
        return availableDataFieldsByStdName.value(levelType).value(variableName)
                ->availableMembers;
    }
    else
    {
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    }
}


QList<QDateTime> MClimateForecastReader::availableInitTimes(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);

    if (availableDataFields.value(levelType).keys().contains(variableName))
    {
        return availableDataFields.value(levelType).value(variableName)->timeMap.keys();
    }
    else if (availableDataFieldsByStdName.value(levelType).keys().contains(variableName))
    {
        return availableDataFieldsByStdName.value(levelType).value(variableName)->timeMap.keys();
    }
    else
    {
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    }
}


QList<QDateTime> MClimateForecastReader::availableValidTimes(
        MVerticalLevelType levelType,
        const QString&     variableName,
        const QDateTime&   initTime)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);

    if (availableDataFields.value(levelType).keys().contains(variableName))
    {
        if (!availableDataFields.value(levelType).value(variableName)
                ->timeMap.keys().contains(initTime))
            throw MBadDataFieldRequest(
                    "unkown init time requested: " +
                    initTime.toString(Qt::ISODate).toStdString(),
                    __FILE__, __LINE__);
        else
            return availableDataFields.value(levelType).value(variableName)
                    ->timeMap.value(initTime).keys();
    }
    else if(availableDataFieldsByStdName.value(levelType).keys().contains(
                variableName))
    {
        if (!availableDataFieldsByStdName.value(levelType).value(variableName)
                ->timeMap.keys().contains(initTime))
            throw MBadDataFieldRequest(
                    "unkown init time requested: " +
                    initTime.toString(Qt::ISODate).toStdString(),
                    __FILE__, __LINE__);
        else
            return availableDataFieldsByStdName.value(levelType)
                    .value(variableName)->timeMap.value(initTime).keys();
    }
    else
        throw MBadDataFieldRequest(
            "unkown variable requested: " + variableName.toStdString(),
            __FILE__, __LINE__);
}


QString MClimateForecastReader::variableLongName(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);

    if (availableDataFields.value(levelType).keys().contains(variableName))
    {
        return availableDataFields.value(levelType).value(variableName)
                ->longname;
    }
    else if (availableDataFieldsByStdName.value(levelType).keys().contains(
                 variableName))
    {
        return availableDataFieldsByStdName.value(levelType).value(variableName)
                ->longname;
    }
    else
    {
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    }
}


QString MClimateForecastReader::variableStandardName(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);

    if (availableDataFields.value(levelType).keys().contains(variableName))
    {
        return availableDataFields.value(levelType).value(variableName)
                ->standardname;
    }
    else if (availableDataFieldsByStdName.value(levelType).keys().contains(
                 variableName))
    {
        return availableDataFieldsByStdName.value(levelType).value(variableName)
                ->standardname;
    }
    else
    {
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    }
}


QString MClimateForecastReader::variableUnits(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (availableDataFields.value(levelType).keys().contains(variableName))
    {
        return availableDataFields.value(levelType).value(variableName)->units;
    }
    else if (availableDataFieldsByStdName.value(levelType).keys().contains(
                 variableName))
    {
        return availableDataFieldsByStdName.value(levelType).value(variableName)
                ->units;
    }
    else
    {
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    }
}


QString MClimateForecastReader::variableSurfacePressureName(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(
                    levelType).toStdString(),
                __FILE__, __LINE__);
    if (availableDataFields.value(levelType).keys().contains(variableName))
    {
        return availableDataFields.value(levelType).value(variableName)
                ->surfacePressureName;
    }
    else if (availableDataFieldsByStdName.value(levelType).keys().contains(
                 variableName))
    {
        return availableDataFieldsByStdName.value(levelType).value(variableName)
                ->surfacePressureName;
    }
    else
    {
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    }

}


QString MClimateForecastReader::variableAuxiliaryPressureName(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(
                    levelType).toStdString(),
                __FILE__, __LINE__);
    if (availableDataFields.value(levelType).keys().contains(variableName))
    {
        return availableDataFields.value(levelType).value(variableName)
                ->auxiliaryPressureName;
    }
    else if (availableDataFieldsByStdName.value(levelType).keys().contains(
                 variableName))
    {
        return availableDataFieldsByStdName.value(levelType).value(variableName)
                ->auxiliaryPressureName;
    }
    else
    {
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    }

}


MHorizontalGridType MClimateForecastReader::variableHorizontalGridType(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
    {
        throw MBadDataFieldRequest(
                    "unkown level type requested: " +
                    MStructuredGrid::verticalLevelTypeToString(
                        levelType).toStdString(),
                    __FILE__, __LINE__);
    }
    if (availableDataFields.value(levelType).keys().contains(variableName))
    {
        return availableDataFields.value(levelType).value(variableName)
                ->horizontalGridType;
    }
    else if (availableDataFieldsByStdName.value(levelType).keys().contains(
                 variableName))
    {
        return availableDataFieldsByStdName.value(levelType).value(
                    variableName)->horizontalGridType;
    }
    else
    {
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    }

}


void MClimateForecastReader::scanDataRoot()
{
    // Lock access to all availableXX data fields.
    QWriteLocker availableItemsWriteLocker(&availableItemsLock);

    LOG4CPLUS_DEBUG(mlog, "Scanning directory "
                    << dataRoot.absolutePath().toStdString() << " "
                    << "for files with NetCDF-CF forecast data.");
    LOG4CPLUS_DEBUG(mlog, "Using file filter: " << dirFileFilters.toStdString());
    LOG4CPLUS_DEBUG(mlog, "Parameters: "
                    << "data is specified on a horizontally regular grid in rotated lon-lat coordinates="
                    << (treatRotatedGridAsRegularGrid ? "enabled" : "disabled")
                    << "data is specified on a horizontally regular grid in projected x-y coordinates="
                    << (treatProjectedGridAsRegularLonLatGrid ? "enabled" : "disabled")
                    << "; convert geometric height to pressure (using standard ICAO)="
                    << (convertGeometricHeightToPressure_ICAOStandard ? "enabled" : "disabled")
                    << "; grid consistency check="
                    << (!disableGridConsistencyCheck ? "enabled" : "disabled")
                    << "; use auxiliary 3D pressure field="
                    << (auxiliary3DPressureField != "" ?
                "enabled (name= " + (auxiliary3DPressureField.toStdString())
                + ")" : "disabled")
                    );

    LOG4CPLUS_DEBUG(mlog, "Available files with forecast data:");

    // Get a list of all files in the directory that match the wildcard name
    // filter given in "dirFileFilters".

    QStringList availableFiles;
    getAvailableFilesFromFilters(availableFiles);

    ensembleIDIsSpecifiedInFileName = dirFileFilters.contains("%m");
    int  ensembleIDFromFile = -1;

    QMap<QString, MNcVarDimensionInfo> auxiliaryVarsDimensionsInfo;

    QString sharedLonLatReferenceVarName;
    QVector<double> sharedLons;
    QVector<double> sharedLats;

    // Contains shared data per level type and variable which must be the same
    // in all files (lons, lats, etc.).
    MSharedDataLevelTypeMap sharedVariableInfos;

    // Stores the variables which have been already checked for consistency in
    // the current file and connects them to the result of this check. Must be
    // cleared when starting to read a new file.
    QMap<MVerticalLevelType, QMap<QString, bool>> checkedVariables;

    // Create and initialise progress bar.
    initializeFileScanProgressDialog(availableFiles.size());

    // For each file, open the file and extract information about the contained
    // variables and forecast valid times.
    foreach (QString fileName, availableFiles)
    {
        updateFileScanProgressDialog();
        LOG4CPLUS_DEBUG(mlog, "\tParsing file "
                        << fileName.toStdString() << " .." << flush);

        checkedVariables.clear();

        // NetCDF library is not thread-safe (at least the regular C/C++
        // interface is not; hence all NetCDF calls need to be serialized
        // globally in Met.3D! (notes Feb2015).
        QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);

        // Open the file.
        NcFile *ncFile;
        try
        {
            ncFile = new NcFile(dataRoot.filePath(fileName).toStdString(),
                                NcFile::read);
        }
        catch (NcException& e)
        {
            LOG4CPLUS_ERROR(mlog, "ERROR: cannot open the file \""
                            << fileName.toStdString()
                            << "\".." << flush);
            continue;
        }

        multimap<string, NcVar> ncVariables = ncFile->getVars();

        if (ensembleIDIsSpecifiedInFileName)
        {
            ensembleIDFromFile = getEnsembleMemberIDFromFileName(fileName);

            if (ensembleIDFromFile == -1)
            {
                LOG4CPLUS_ERROR(mlog, "ERROR: ensemble tag found in file filter "
                                      " but filter did not match file \""
                                << fileName.toStdString()
                                << "\" (currently only integer values are allowed"
                                   " as ensemble member specifiers)." << flush);
                continue;
            }
        }

        QStringList gridMappingVarNames = QStringList();
        // Since we don't know the name of the grid mapping variable, loop over
        // all variables, check if variable with grid_mapping_name
        // 'rotated_latitude_longitude' exist and store their names in
        // gridMappingVarNames. Find the grid mapping variables first since
        // they are needed to check if a variable is defined on a rotated grid.
        for (multimap<string, NcVar>::iterator gridVar = ncVariables.begin();
             gridVar != ncVariables.end(); gridVar++)
        {
            QString varName = QString::fromStdString(gridVar->first);

            bool gridVariablePresent = NcCFVar::isCFGridMappingVariable(
                        ncFile->getVar(varName.toStdString()));
            if (gridVariablePresent)
            {
                gridMappingVarNames.append(varName);
            }
        }

        // Loop over all variables: Obtain available time values for each
        // variable and insert the fields into "availableDataFields".
        for (multimap<string, NcVar>::iterator var=ncVariables.begin();
             var != ncVariables.end(); var++)
        {
            QString varName = QString::fromStdString(var->first);

            if ( NcCFVar::isCFDataVariable(
                     ncFile->getVar(varName.toStdString()), NcCFVar::LAT_LON) )
            {
                // Get the NcVar object belonging to the variable and wrap
                // it in a NcCFVar object.
                NcCFVar currCFVar(ncFile->getVar(varName.toStdString()));

                LOG4CPLUS_DEBUG(mlog, "\tChecking variable <"
                                << varName.toStdString() << "> ...");

                // Read the variable's long_name, standard_name and units
                // attributes, if present. If they are not present, leave the
                // corresponding variables empty.
                string longname = "";
                string standardname = "";
                string units = "";
                try { currCFVar.getAtt("long_name").getValues(longname); }
                catch (NcException) {}
                try { currCFVar.getAtt("standard_name").getValues(standardname); }
                catch (NcException) {}
                try { currCFVar.getAtt("units").getValues(units); }
                catch (NcException) {}

                // If no standard name is provided in the file, check if we
                // can reconstruct the standard name from the
                // "variableToStandardNameMap" table.
                if (standardname == "")
                {
                    if (variableToStandardNameMap.contains(varName))
                    {
                        standardname = variableToStandardNameMap[
                                varName].toStdString();
                    }
                    else
                    {
                        LOG4CPLUS_WARN(mlog,
                                       "WARNING: no standard name and no mapping "
                                       "from variable name to standard name "
                                       "defined for <"
                                       << varName.toStdString() << ">.");
                    }
                }

                // Get time values of this variable.
                QList<QDateTime> currTimeCoordValues;
                try
                {
                    currTimeCoordValues = currCFVar.getTimeValues();
                }
                catch (NcException)
                {
                    LOG4CPLUS_WARN(mlog, "WARNING: unable to identify valid "
                                         "time values for variable <"
                                   << varName.toStdString()
                                   << "> -- skipping variable.");
                    continue;
                }

                // Determine init time from the CF variable.
                QDateTime initTime;
                try
                {
                    initTime = currCFVar.getBaseTime();
                }
                catch (NcException)
                {
                    LOG4CPLUS_WARN(mlog, "WARNING: unable to identify "
                                         "init/base time for variable <"
                                   << varName.toStdString()
                                   << ">  -- skipping variable.");
                    continue;
                }

                // Determin the type of the vertical level of the variable.
                MVerticalLevelType levelType;
                NcCFVar::NcVariableGridType gridType = currCFVar.getGridType(
                            auxiliary3DPressureField, disableGridConsistencyCheck);

                LOG4CPLUS_DEBUG(mlog, "\t--detected grid type: "
                                << NcCFVar::ncVariableGridTypeToString(
                                    gridType).toStdString());

                switch (gridType)
                {
                case NcCFVar::LAT_LON:
                    levelType = SURFACE_2D;
                    break;
                case NcCFVar::LAT_LON_P:
                    levelType = PRESSURE_LEVELS_3D;
                    if (convertGeometricHeightToPressure_ICAOStandard)
                    {
                        LOG4CPLUS_WARN(
                                    mlog,
                                    "WARNING: identified variable <"
                                    << varName.toStdString()
                                    << "> is defined on a grid using vertical"
                                       " pressure levels, and conversion of"
                                       " geometric height to pressure"
                                       " coordinates is enabled  -- skipping"
                                       " variable.");
                        continue;
                    }
                    else
                    {
                        levelType = PRESSURE_LEVELS_3D;
                    }
                    break;
                case NcCFVar::LAT_LON_HYBRID:
                    levelType = HYBRID_SIGMA_PRESSURE_3D;
                    break;
                case NcCFVar::LAT_LON_PVU:
                    levelType = POTENTIAL_VORTICITY_2D;
                    break;
                case NcCFVar::LAT_LON_AUXP3D:
                    levelType = AUXILIARY_PRESSURE_3D;
                    break;
                case NcCFVar::LAT_LON_Z:
                    if (convertGeometricHeightToPressure_ICAOStandard)
                    {
                        levelType = PRESSURE_LEVELS_3D;
                    }
                    else
                    {
                        LOG4CPLUS_WARN(
                                    mlog,
                                    "WARNING: identified variable <"
                                    << varName.toStdString()
                                    << "> is defined on a grid using vertical"
                                       " geometric height levels, and"
                                       " conversion to pressure coordinates is"
                                       " disabled -- Met.3D can currently only"
                                       " handle pressure coordinates internally;"
                                       " skipping variable.");
                        continue;
                    }
                    break;
                default:
                    // If neither of the above choices could be matched,
                    // discard this variable and continue.
                    LOG4CPLUS_WARN(
                                mlog,
                                "WARNING: identified variable <"
                                << varName.toStdString()
                                << "> is defined on a grid that Met.3D "
                                   "currently does not understand -- skipping "
                                   "variable.");
                    continue;
                    break;
                }
                // Create a new MVariableInfo struct and store available
                // variable information in this field.
                MVariableInfo* vinfo;
                if (availableDataFields[levelType].contains(varName))
                {
                    // We need to check a variable for consistency only once per
                    // file since the geographical region parameters are shared
                    // in one file. Thus use the result of a previous check if
                    // present.
                    if (checkedVariables[levelType].contains(varName))
                    {
                        if (!checkedVariables[levelType][varName])
                        {
                            continue;
                        }
                    }
                    else
                    {
                        bool consistent = checkSharedVariableDataConsistency(
                                    &sharedVariableInfos[levelType][varName],
                                    &currCFVar, levelType, false,
                                    !ensembleIDIsSpecifiedInFileName);
                        checkedVariables[levelType][varName] = consistent;
                        if (!consistent)
                        {
                            LOG4CPLUS_ERROR(
                                        mlog,
                                        "found different geographical region "
                                        "than previously used for this "
                                        "variable; skipping grids of variable '"
                                        + varName.toStdString()
                                        + "' found in file '"
                                        + fileName.toStdString() + "'.");
                            continue;
                        }
                    }
                    vinfo = availableDataFields[levelType].value(varName);
                    if (ensembleIDIsSpecifiedInFileName)
                    {
                        vinfo->availableMembers.insert(ensembleIDFromFile);

                        foreach (QDateTime validTime, currTimeCoordValues) // in UTC!
                        {
                            MDatafieldInfo info;
                            info.filename = fileName;
                            vinfo->timeMap[initTime][validTime][ensembleIDFromFile]
                                    = info;
                        } // for (valid times)
                    }
                }
                else
                {
                    vinfo = new MVariableInfo;
                    vinfo->longname        = QString::fromStdString(longname);
                    vinfo->standardname    = QString::fromStdString(standardname);
                    vinfo->units           = QString::fromStdString(units);
                    vinfo->variablename    = varName;

                    if (levelType == HYBRID_SIGMA_PRESSURE_3D)
                    {
                        NcVar vertVar, apVar, bVar;
                        QString psName;
                        vertVar = currCFVar.getVerticalCoordinateHybridSigmaPressure(
                                    &apVar, &bVar, &psName);
                        vinfo->surfacePressureName = psName;
                    }

                    if (levelType == AUXILIARY_PRESSURE_3D)
                    {
                        QString pressureName;
                        int lvlIndex;
                        NcVar pressureVar =
                                currCFVar.getVerticalCoordinateAuxiliaryPressure(
                                    auxiliary3DPressureField, &pressureName,
                                    &lvlIndex, disableGridConsistencyCheck);
                        vinfo->auxiliaryPressureName = pressureName;

                        // Auxiliary pressure field is not stored in the same
                        // file thus it is necessary to perform a check after
                        // reading all the files whether this variable is
                        // really an auxiliary pressure variable. (Dimensions
                        // must match the dimensions of the auxiliary 3D
                        // pressure field).
                        if (pressureVar.isNull())
                        {
                            // Store dimension information (names, sizes) for
                            // variables detected as auxiliary pressure variable
                            // but not checked yet since the auxiliary pressure
                            // field is not stored in the same file. The check
                            // is performed after all files have been read.
                            if (!auxiliaryVarsDimensionsInfo.contains(varName))
                            {
                                MNcVarDimensionInfo varDimensionsInfo =
                                        auxiliaryVarsDimensionsInfo[varName];
                                for (int j = 0; j < currCFVar.getDimCount(); j++)
                                {
                                    QString dimName = QString::fromStdString(
                                                currCFVar.getDim(j).getName());
                                    varDimensionsInfo.names << dimName;
                                    varDimensionsInfo.sizes
                                            << int(currCFVar.getDim(j).getSize());
                                }
                            }
                        }
                    }


                    if (ensembleIDIsSpecifiedInFileName)
                    {
                        vinfo->availableMembers.insert(ensembleIDFromFile);
                    }
                    else
                    {
                        // Check if the variable has an ensemble dimension.
                        if (currCFVar.hasEnsembleDimension())
                        {
                            // If yes, get the available ensemble members.
                            vinfo->availableMembers =
                                    currCFVar.getEnsembleMembers();
                        }
                        else
                        {
                            // No ensemble dimension could be found. List the
                            // available data field as the "0" member.
                            vinfo->availableMembers.insert(0);
                        }
                    }

                    // Check horizontal grid type. Default grid: regular lat-lon.
                    vinfo->horizontalGridType = MHorizontalGridType::REGULAR_LONLAT_GRID;

                    // Change grid type to ROTATED_REGULAR_LONLAT_GRID or
                    // PROJECTED_REGULAR_GRID if the data is defined on one
                    // of these grids.
                    if (!gridMappingVarNames.empty())
                    {
                        QString gridMappingVarName = "";
                        if (NcCFVar::isDefinedOnRotatedGrid(
                                    ncFile->getVar(varName.toStdString()),
                                    gridMappingVarNames, &gridMappingVarName))
                        {
                            vinfo->horizontalGridType =
                                    MHorizontalGridType::ROTATED_REGULAR_LONLAT_GRID;
                        }
                        // Check if data is defined on a projected grid.
                        if (NcCFVar::isDefinedOnProjectedGrid(
                                    ncFile->getVar(varName.toStdString()),
                                    gridMappingVarNames, &gridMappingVarName))
                        {
                            vinfo->horizontalGridType =
                                    MHorizontalGridType::PROJECTED_REGULAR_GRID;
                        }
                        // At the moment, only register rotated grids if the
                        // user wants to interpret rotated lon-latgrids as
                        // regular lon-lat grids.
                        if (!treatRotatedGridAsRegularGrid
                                && (vinfo->horizontalGridType
                                    == MHorizontalGridType::ROTATED_REGULAR_LONLAT_GRID))
                        {
                            continue;
                        }
                        // At the moment, only register projected grid if
                        // the user wants to interpret projected grids as
                        // regular lon-lat grids.
                        if (!treatProjectedGridAsRegularLonLatGrid
                                && (vinfo->horizontalGridType
                                    == MHorizontalGridType::PROJECTED_REGULAR_GRID))
                        {
                            continue;
                        }
                    }
                    // Initialise shared data of variable for consistency check.
                    checkSharedVariableDataConsistency(
                                &sharedVariableInfos[levelType][varName],
                                &currCFVar, levelType, true, false);

                    if (!disableGridConsistencyCheck)
                    {
                        QString refVarName = "";
                        bool check = true;
                        // Initialise shared lons and lats. This also means it
                        // is the first variable to be found.
                        if (sharedLons.isEmpty())
                        {
                            sharedLons =
                                    sharedVariableInfos[levelType][varName]
                                    .lons;
                            sharedLats =
                                    sharedVariableInfos[levelType][varName]
                                    .lats;
                            sharedLonLatReferenceVarName = varName;
                        }
                        else
                        {
                            // Only check consistency with level type
                            // specific values if there exists another
                            // variable of the same level type.
                            if (sharedVariableInfos[levelType].values()
                                    .size() > 1)
                            {
                                // Get the name of the reference variable.
                                QStringList varNames =
                                        sharedVariableInfos[levelType].keys();
                                refVarName = varNames.at(0);
                                // Don't use the variable itself as reference.
                                if (refVarName == varName)
                                {
                                    refVarName = varNames.at(1);
                                }
                                check = checkSharedVariableDataConsistency(
                                            &sharedVariableInfos[levelType]
                                            [refVarName], &currCFVar,
                                            levelType, false, false);
                            }
                            else
                            {
                                check = (sharedLons
                                         == sharedVariableInfos[levelType]
                                        [varName].lons)
                                        && (sharedLats
                                            == sharedVariableInfos[levelType]
                                        [varName].lats);

                                if (!check)
                                {
                                    LOG4CPLUS_ERROR(
                                                mlog,
                                                "detected inconsistency in"
                                                " 'longitudes' or 'latitudes'.");
                                    refVarName = sharedLonLatReferenceVarName;
                                    check = false;
                                }
                            }
                        }

                        if (!check)
                        {
                            LOG4CPLUS_ERROR(
                                        mlog,
                                        "found difference to reference variable"
                                        " '" + refVarName.toStdString()
                                        + "'; skipping grids of variable '"
                                        + varName.toStdString()
                                        + "' found in file '"
                                        + fileName.toStdString() + "'."
                                        + " [Dataset: "
                                        + getIdentifier().toStdString() + "]");
                            checkedVariables[levelType][varName] = false;
                            continue;
                        }
                    }

                    checkedVariables[levelType][varName] = true;
                }

                unsigned int ensMem;
                if (ensembleIDIsSpecifiedInFileName)
                {
                    ensMem = ensembleIDFromFile;
                }
                else
                {
                    // If ensemble members are all stored in one file, use 0 to
                    // represent them to avoid redundant informations stored.
                    ensMem = 0;
                }

                foreach (QDateTime validTime, currTimeCoordValues) // in UTC!
                {
                    MDatafieldInfo info;
                    info.filename = fileName;
                    vinfo->timeMap[initTime][validTime][ensMem] = info;

//                        cout << "\t"  << filedomain.toStdString()
//                             << "  "  << varName.toStdString()
//                             << "  (" << verticalLevelTypeToString(levelType).toStdString()
//                             << ")  " << initTime.toString(Qt::ISODate).toStdString()
//                             << "  "  << validTime.toString(Qt::ISODate).toStdString()
//                             << "  "  << ((vinfo->numEnsembleMembers > 0) ?
//                                              QString("-- ensemble members: %1")
//                                              .arg(vinfo->numEnsembleMembers)
//                                              .toStdString()
//                                            : "")
//                             << "\n";

                } // for (valid times)


                // Insert the new MVariableInfo struct into the variable name
                // map..
                availableDataFields[levelType].insert(
                            vinfo->variablename, vinfo);
                // ..and, if a CF standard name is available, into the std
                // name map.
                if (standardname != "")
                    availableDataFieldsByStdName[levelType].insert(
                                vinfo->standardname, vinfo);

            } // if (is CF variable)
        } // for (variables)

        delete ncFile;
    } // for (files)

    deleteFileScanProgressDialog();

    // Some auxiliary pressure variables might NOT have been checked yet if
    // their dimensions match the dimensions of auxiliary pressure field (i.e.
    // the auxiliary pressure field variable was not stored in the same file).
    // Thus, check these variables here and if the dimensions don't match,
    // discard them.
    if (auxiliary3DPressureField != "")
    {
        MVerticalLevelType levelType = AUXILIARY_PRESSURE_3D;
        MNcVarDimensionInfo auxiliaryPressureDimensions =
                auxiliaryVarsDimensionsInfo.take(auxiliary3DPressureField);
        foreach (QString varName, auxiliaryVarsDimensionsInfo.keys())
        {
            MNcVarDimensionInfo dimensionsInfo =
                    auxiliaryVarsDimensionsInfo[varName];
            if (!disableGridConsistencyCheck)
            {
                if (dimensionsInfo.names != auxiliaryPressureDimensions.names)
                {
                    LOG4CPLUS_ERROR(mlog,
                                    "variable '"
                                    + varName.toStdString()
                                    + "' was suggested as variable with"
                                      " auxiliary pressure but the names of its"
                                      " dimensions don't match the names of the"
                                      " dimensions of pressure variable '"
                                    + auxiliary3DPressureField.toStdString()
                                    + "'; discarding variable\n"
                                    << flush);
                    MVariableInfo *vInfo =
                            availableDataFields[levelType].take(varName);
                    delete availableDataFieldsByStdName[levelType].take(
                                vInfo->standardname);
                    delete vInfo;
                    continue;
                }
                if (dimensionsInfo.sizes
                        != auxiliaryPressureDimensions.sizes)
                {
                    LOG4CPLUS_ERROR(mlog,
                                    "variable '"
                                    + varName.toStdString()
                                    + "' was suggested as variable with"
                                      " auxiliary pressure but the sizes of its"
                                      " dimensions don't match the sizes of the"
                                      " dimensions of pressure variable '"
                                    + auxiliary3DPressureField.toStdString()
                                    + "'; discarding variable\n"
                                    << flush);
                    MVariableInfo *vInfo =
                            availableDataFields[levelType].take(varName);
                    delete availableDataFieldsByStdName[levelType].take(
                                vInfo->standardname);
                    delete vInfo;
                    continue;
                }
            }
            else
            {
                if (dimensionsInfo.names.size()
                        != auxiliaryPressureDimensions.names.size())
                {
                    LOG4CPLUS_ERROR(mlog,
                                    "variable '"
                                    + varName.toStdString()
                                    + "' was suggested as variable with"
                                      " auxiliary pressure but it has less"
                                      " dimensions than the pressure"
                                      " variable '"
                                    + auxiliary3DPressureField.toStdString()
                                    + "' in names; discarding variable\n"
                                    << flush);
                    MVariableInfo *vInfo =
                            availableDataFields[levelType].take(varName);
                    delete availableDataFieldsByStdName[levelType].take(
                                vInfo->standardname);
                    delete vInfo;
                    continue;
                }
            }
        }
    }
}


MStructuredGrid *MClimateForecastReader::readGrid(
        MVerticalLevelType levelType,
        const QString &variableName,
        const QDateTime &initTime,
        const QDateTime &validTime,
        unsigned int ensembleMember)
{
#ifdef MSTOPWATCH_ENABLED
    MStopwatch stopwatch;
#endif

    // Determine file name of data file that holds the requested field.
    QString filename;
    try
    {
        if (ensembleIDIsSpecifiedInFileName)
        {
            filename = dataFieldFile(levelType, variableName, initTime,
                                     validTime, ensembleMember);
        }
        else
        {
            // If ensemble members are all stored in one file, 0 is used to
            // represent them to avoid redundant informations stored.
            filename = dataFieldFile(levelType, variableName, initTime,
                                     validTime, 0);
        }
    }
    catch (MBadDataFieldRequest& e)
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: invalid data field requested. There "
                              "seems to be a problem with your dataset. Please "
                              "check that all variables are defined for all "
                              "timesteps and ensemble members. [Returning"
                              " a nullptr.]\n" << flush);
        return nullptr;
    }

    LOG4CPLUS_DEBUG(mlog, "Reading NetCDF data for variable <"
                    << "name=" << variableName.toStdString()
                    << "; leveltype="
                    << MStructuredGrid::verticalLevelTypeToString(levelType).toStdString()
                    << "; inittime=" << initTime.toString(Qt::ISODate).toStdString()
                    << "; validtime=" << validTime.toString(Qt::ISODate).toStdString()
                    << "> from file <"
                    << filename.toStdString() << "> ...");

    // Is this file opened for the first time? First access to a variable?
    openFilesMutex.lock();
    bool initialFileAccess = !openFiles.contains(filename);
    if (initialFileAccess)
        openFiles.insert(filename, new MFileInfo());
    MFileInfo* finfo = openFiles.value(filename);
    openFilesMutex.unlock();

    // Lock access to "filename" and associated data until the end of this
    // method.
    QMutexLocker accessMutexLocker(&(finfo->accessMutex));

    // Shortcut to this variable's shared file data.
    bool initialVariableAccess =
            !finfo->sharedData[levelType].contains(variableName);
    MVariableDataSharedPerFile *shared =
            &(finfo->sharedData[levelType][variableName]);

    if (initialFileAccess)
    {
        // The file is accessed for the first time -- open.
        try
        {
            // NetCDF library is not thread-safe (at least the regular C/C++
            // interface is not; hence all NetCDF calls need to be serialized
            // globally in Met.3D! (notes Feb2015).
            QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
            finfo->ncFile = new NcFile(filename.toStdString(), NcFile::read);
        }
        catch (NcException& e)
        {
            LOG4CPLUS_ERROR(mlog, "ERROR: cannot open file "
                            << filename.toStdString() << " -- " << e.what());
            throw;
        }
    }

    if (initialVariableAccess)
    {
        // The variable is for this file accessed for the first time --
        // initialise the shared metadata that can be re-used in future
        // access to this variable.
        LOG4CPLUS_DEBUG(mlog, "Initial access to " << variableName.toStdString()
                        << " in this file. Initialising shared metadata.");

        // Get a handle on the NetCDF variable and wrap it in a NcCFVar object.
        QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
        shared->cfVar = finfo->ncFile->getVar(variableName.toStdString());
        ncAccessMutexLocker.unlock();

        // If the specified variable name cannot be found, check whether the CF
        // standard name has been specified instead of the "real" variable
        // name. If the standard name cannot be identified as well, return.
        if (shared->cfVar.isNull())
        {
            availableItemsLock.lockForRead();
            MVariableInfo* info =
                    availableDataFieldsByStdName[levelType][variableName];
            availableItemsLock.unlock();

            ncAccessMutexLocker.relock();
            shared->cfVar = finfo->ncFile->getVar(
                        info->variablename.toStdString());
            ncAccessMutexLocker.unlock();

            if (shared->cfVar.isNull())
            {
                LOG4CPLUS_ERROR(mlog, "ERROR: cannot find variable "
                                << variableName.toStdString());
                // Cannot recover from this error; delete shared
                // data read so far and break.
                finfo->sharedData[levelType].remove(variableName);
                throw MBadDataFieldRequest(
                            "cannot find variable " + variableName.toStdString(),
                            __FILE__, __LINE__);
            }
        }

        // Query latitude, longitude and time coordinate system variables.
        ncAccessMutexLocker.relock();
        shared->latVar = shared->cfVar.getLatitudeVar();
        LOG4CPLUS_DEBUG(mlog, "\tLatitude variable is '" << shared->latVar.getName()
                        << "' (" << shared->latVar.getDim(0).getSize()
                        << " elements).");
        shared->lonVar = shared->cfVar.getLongitudeVar();
        LOG4CPLUS_DEBUG(mlog, "\tLongitude variable is '" << shared->lonVar.getName()
                        << "' (" << shared->lonVar.getDim(0).getSize()
                        << " elements).");
        shared->timeVar = shared->cfVar.getTimeVar();
        LOG4CPLUS_DEBUG(mlog, "\tTime variable is '" << shared->timeVar.getName()
                        << "' (" << shared->timeVar.getDim(0).getSize()
                        << " elements).");

        // Query scale and offset, if provided.
        try
        {
            shared->cfVar.getAtt("scale_factor").getValues(&(shared->scale_factor));
        }
        catch (NcException)
        {
            shared->scale_factor = 1.;
        }
        try
        {
            shared->cfVar.getAtt("add_offset").getValues(&(shared->add_offset));
        }
        catch (NcException)
        {
            shared->add_offset = 0.;
        }

        // Have scale and/or offset been provided? If not, they are not applied.
        shared->scaleAndOffsetProvided =
                (shared->scale_factor != 1.) || (shared->add_offset != 0.);

        if (shared->scaleAndOffsetProvided)
        {
            LOG4CPLUS_DEBUG(mlog, "\tScale and/or offset have been provided:"
                            << " scale = " << shared->scale_factor
                            << " offset = " << shared->add_offset
                            << ".");
        }

        // Query missing value, if provided.
        try
        {
            shared->cfVar.getAtt("missing_value").getValues(&(shared->missingValue));
            shared->missingValueProvided = true;
        }
        catch (NcException)
        {
            shared->missingValue = M_MISSING_VALUE;
            shared->missingValueProvided = false;
        }

        if (shared->missingValueProvided)
        {
            LOG4CPLUS_DEBUG(mlog, "\tMissing value has been provided:"
                            << " missing value = " << shared->missingValue
                            << ".");
        }

        // Determine grid-type-dependent metadata/settings.
        // ================================================
        if (levelType == PRESSURE_LEVELS_3D)
        {
            // Pressure levels: Get a link to the variable containing the
            // vertical coordinate data.

            if (convertGeometricHeightToPressure_ICAOStandard)
            {
                // Special case: If conversion of vertical geometric height
                // levels to pressure levels is enabled, the grid will appear
                // in Met.3D as a regular pressure level grid.
                LOG4CPLUS_DEBUG(mlog, "\tConversion of vertical geometric "
                                      "height to pressure levels is enabled.");
                shared->vertVar =
                        shared->cfVar.getVerticalCoordinateGeometricHeight();
            }
            else
            {
                shared->vertVar = shared->cfVar.getVerticalCoordinatePressure();
            }
        }

        else if (levelType == HYBRID_SIGMA_PRESSURE_3D)
        {
            // Hybrid pressure levels: Get a link to the variable containing the
            // vertical model level data and load the ak/bk coefficients.

            NcVar apVar, bVar;
            QString psName;
            shared->vertVar =
                    shared->cfVar.getVerticalCoordinateHybridSigmaPressure(
                        &apVar, &bVar, &psName);

            // Read hybrid coefficients.
            if ( !apVar.isNull() )
            {
                shared->ak.resize(apVar.getDim(0).getSize());
                apVar.getVar(shared->ak.data());

                // Check the units of the ak coefficients: Pa or hPa? We need hPa!
                string units = "";
                try { apVar.getAtt("units").getValues(units); }
                catch (NcException) {}

                if (units == "Pa")
                {
                    // .. hence, if the ak are given in Pa, convert to hPa.
                    for (int i = 0; i < shared->ak.size(); i++)
                    {
                        shared->ak[i] /= 100.;
                    }
                }
                else if (units != "hPa")
                {
                    // .. and if they're in completely different units, raise an
                    // exception.
                    // Cannot recover from this error; delete shared
                    // data read so far and break.
                    finfo->sharedData[levelType].remove(variableName);
                    QString err = QString(
                                "invalid units '%1' for ak coefficients "
                                "(must be Pa or hPa)").arg(units.c_str());
                    throw MNcException("NcException", err.toStdString(),
                                       __FILE__, __LINE__);
                }
            }

            if ( !bVar.isNull() )
            {
                shared->bk.resize(bVar.getDim(0).getSize());
                bVar.getVar(shared->bk.data());
            }
        } // HYBRID_SIGMA_PRESSURE_3D

        else if (levelType == POTENTIAL_VORTICITY_2D)
        {
            // Potential vorticity levels: Get a link to the vertical PV
            // coordinate variable.

            shared->vertVar = shared->cfVar.getVerticalCoordinatePotVort();
        }

        else if (levelType == AUXILIARY_PRESSURE_3D)
        {
            // Auxiliary pressure: Determine whether vertical pressure axis
            // in auxiliary field is correctly ordered.
            LOG4CPLUS_DEBUG(mlog, "\tChecking auxiliary pressure field.");

            // Obtain link to NcVar that contains the auxiliary pressure field.
            QString pressureName; int levelIndex;
            NcVar auxPressureVar =
                    shared->cfVar.getVerticalCoordinateAuxiliaryPressure(
                        auxiliary3DPressureField, &pressureName, &levelIndex,
                        disableGridConsistencyCheck);

            if ( !auxPressureVar.isNull() )
            {
                // If the auxiliary pressure field variable is stored in the
                // same NetCDF file, load a single vertical column (at lon/lat
                // indices 0/0 and timestep 0), then check its ordering.

                int nVerticalLevels = auxPressureVar.getDim(levelIndex).getSize();

                LOG4CPLUS_DEBUG(
                            mlog, "\tVertical dimension is of type "
                            << MStructuredGrid::verticalLevelTypeToString(
                                levelType).toStdString()
                            << ", vertical variable is '"
                            << auxPressureVar.getName()
                            << "' (" << nVerticalLevels << " levels).");

                // The values of the "real" vertical coordinate axis of the
                // data field don't matter for aux-p treatment: Instead, the
                // pressure values stored in the aux-p field will be used.
                shared->levels.fill(M_MISSING_VALUE, nVerticalLevels);

                // Load a single data column.
                QVector<double> singleDataColumn(nVerticalLevels);
                if (auxPressureVar.getDimCount() == 4)
                {
                    // No ensemble dimension, only time/lev/lat/lon.
                    vector<size_t> start(4);
                    start.assign(4, 0);
                    vector<size_t> count(4);
                    count.assign(4, 1);
                    count[1] = nVerticalLevels;
                    auxPressureVar.getVar(start, count, singleDataColumn.data());
                }
                else if (auxPressureVar.getDimCount() == 5)
                {
                    // Ensemble dimension present.
                    vector<size_t> start(5);
                    start.assign(5, 0);
                    vector<size_t> count(5);
                    count.assign(5, 1);
                    count[2] = nVerticalLevels;
                    auxPressureVar.getVar(start, count, singleDataColumn.data());
                }

                // Do the grid's vertical levels need to be reversed?
                // (Index 0 should be at the top of the atmosphere and have
                // the smaller pressure value).
                shared->reverseLevels =
                        singleDataColumn[0] > singleDataColumn[nVerticalLevels-1];
            }
            else
            {
                // The auxiliary pressure field variable is stored in a
                // different NetCDF file.
                // Current solution: Read the ENTIRE aux-p grid, just to
                // copy its "reverseLevels" flag...
//TODO (mr, 03Aug2018) -- this is a very inefficient way of determining the
// direction of the vertical aux-p axis -- this needs to be refactored!

                LOG4CPLUS_DEBUG(mlog, "\tNeed to access aux-p field in "
                                      "separate file.");

                ncAccessMutexLocker.unlock();
                MLonLatAuxiliaryPressureGrid *auxPGrid =
                        dynamic_cast<MLonLatAuxiliaryPressureGrid*>(
                            readGrid(levelType, auxiliary3DPressureField,
                                     initTime, validTime, ensembleMember));
                ncAccessMutexLocker.relock();

                shared->reverseLevels = auxPGrid->getReverseLevels();
                shared->levels.fill(M_MISSING_VALUE, auxPGrid->getNumLevels());

                delete auxPGrid;
            }

            if (shared->reverseLevels)
            {
                LOG4CPLUS_DEBUG(mlog, "\tReversing vertical levels.");
            }
        } // AUXILIARY_PRESSURE_3D
        // END (grid-type-dependent stuff).

        // Read lon/lat coordinate variable data.
        shared->lons.resize(shared->lonVar.getDim(0).getSize());
        shared->lonVar.getVar(shared->lons.data());
        shared->lats.resize(shared->latVar.getDim(0).getSize());
        shared->latVar.getVar(shared->lats.data());

        // Projected coordinates: Read units from the input file and scale
        // coordinate values to fit 0..360 range to be handled correctly by
        // Met.3D.
         if (treatProjectedGridAsRegularLonLatGrid)
         {
            string units = "";
            float scaleFactor = MetConstants::scaleFactorToFitProjectedCoordsTo360Range;
            shared->lonVar.getAtt("units").getValues(units);
            if (units == "km")
            {
                scaleFactor /= 1000.0f;
            }
            else if (units != "m")
            {
                LOG4CPLUS_ERROR(mlog, "ERROR: projected coordinates x,y have "
                                      "unknown units (must be 'm' or 'km').");
            }
            int size = shared->lats.size();
            for (int i = 0; i < size; i++)
            {
                shared->lats[i] /= scaleFactor;
            }
            size = shared->lons.size();
            for (int i = 0; i < size; i++)
            {
                shared->lons[i] /= scaleFactor;
            }
         }

        // Check if longitudes are cyclic (0..360 or -180..180 etc) and OVERLAP
        // (e.g. starts with -180 and ends with +180). In these cases we don't
        // need the redundant longitude.
        // NOTE: Using M_LONLAT_RESOLUTION workaround, see mutil.h
        double lon_west = MMOD(shared->lons[0], 360.);
        double lon_east = MMOD(shared->lons[shared->lons.size()-1], 360.);
        if ( fabs(lon_west - lon_east) < M_LONLAT_RESOLUTION )
        {
            LOG4CPLUS_DEBUG(mlog, "\tFirst longitude (" << lon_west
                            << ") is identical to last longitude ("
                            << lon_east << ") -- discarding last longitude.");
            shared->lons.pop_back();
        }

        // Check if latitudes go from north to south. If not, reverse array.
        double lat_north = shared->lats[0];
        double lat_south = shared->lats[shared->lats.size()-1];
        if (lat_north < lat_south)
        {
            LOG4CPLUS_DEBUG(mlog, "\tReversing latitudes.");
            shared->reverseLatitudes = true;
            QVector<double> tmpLats(shared->lats);
            int size = shared->lats.size();
            for (int i = 0; i < size; i++) shared->lats[i] = tmpLats[size-1-i];
        }
        else
        {
            shared->reverseLatitudes = false;
        }

        if ( !shared->vertVar.isNull() )
        {
            LOG4CPLUS_DEBUG(mlog, "\tVertical dimension is of type "
                            << MStructuredGrid::verticalLevelTypeToString(
                                levelType).toStdString()
                            << ", vertical variable is '"
                            << shared->vertVar.getName() << "' ("
                            << shared->vertVar.getDim(0).getSize()
                            << " elements).");
            shared->levels.resize(shared->vertVar.getDim(0).getSize());
            shared->vertVar.getVar(shared->levels.data());

            if (levelType == PRESSURE_LEVELS_3D)
            {
                // If vertical levels are specified in Pa, convert to hPa.
                string units = "";
                try { shared->vertVar.getAtt("units").getValues(units); }
                catch (NcException) {}

                if (convertGeometricHeightToPressure_ICAOStandard)
                {
                    // Conversion flag "geometric height -> pressure" has
                    // been set. The units of this variable are expected to be
                    // metres, i.e., geometric height.

                    if (NcCFVar::validUnitsVerticalCoordinate(
                                NcCFVar::LAT_LON_Z).contains(units.c_str()))
                    {
                        for (int i = 0; i < shared->levels.size(); i++)
                        {
                            shared->levels[i] = metre2pressure_standardICAO(
                                        shared->levels[i]) / 100.;
                        }
                    }
                    else
                    {
                        // Cannot recover from this error; delete shared
                        // data read so far and break.
                        finfo->sharedData[levelType].remove(variableName);
                        QString err = QString(
                                    "invalid units '%1' for geometric "
                                    "height levels in variable %2 "
                                    "(must be one of "
                                    "%3)").arg(units.c_str()).arg(
                                    shared->vertVar.getName().c_str()).arg(
                                    NcCFVar::validUnitsVerticalCoordinateAsString(
                                        NcCFVar::LAT_LON_Z));
                        throw MNcException("NcException",  err.toStdString(),
                                           __FILE__, __LINE__);
                    }
                }

                else if (units == "Pa")
                {
                    for (int i = 0; i < shared->levels.size(); i++)
                    {
                        shared->levels[i] /= 100.;
                    }
                }
                else if (units != "hPa")
                {
                    // Cannot recover from this error; delete shared
                    // data read so far and break.
                    finfo->sharedData[levelType].remove(variableName);
                    QString err = QString(
                                "invalid units '%1' for pressure "
                                "levels (must be "
                                "Pa or hPa)").arg(units.c_str());
                    throw MNcException("NcException",  err.toStdString(),
                                       __FILE__, __LINE__);
                }
            }

            // Determine whether the grid's vertical levels must be reversed.
            double lev_bot = shared->levels[0];
            double lev_top = shared->levels[shared->levels.size()-1];
            if (lev_bot > lev_top)
            {
                LOG4CPLUS_DEBUG(mlog, "\tReversing levels.");
                shared->reverseLevels = true;
                QVector<double> tmpLevs(shared->levels);
                int size = shared->levels.size();
                for (int i = 0; i < size; i++)
                {
                    shared->levels[i] = tmpLevs[size-1-i];
                }
            }
            else
            {
                shared->reverseLevels = false;
            }
        }
        else
        {
            LOG4CPLUS_DEBUG(mlog, "\tNo vertical dimension or auxiliary "
                                  "pressure field detected (if there should be "
                                  "one: are the 'units' "
                                  "and 'positive' attributes set correctly?).");
        }

        // Get time values of this variable.
        shared->timeCoordValues = shared->cfVar.getTimeValues();

        // Query ensemble dimension.
        try
        {
            shared->ensembleVar = shared->cfVar.getEnsembleVar();
            LOG4CPLUS_DEBUG(mlog, "\tEnsemble variable is '"
                            << shared->ensembleVar.getName()
                            << "'; ensemble forecast contains "
                            << shared->ensembleVar.getDim(0).getSize()
                            << " members.");
            shared->availableMembers = shared->cfVar.getEnsembleMembers(
                        &(shared->memberToFileIndexMap));
        }
        catch (NcException)
        {
            LOG4CPLUS_DEBUG(mlog, "\tNo ensemble dimension.");
        }
    } // initial access


    // Return value.
    MStructuredGrid *grid = nullptr;

    // Initialize the grid dependent on the vertical level type.
    if (levelType == SURFACE_2D)
    {
        grid = new MRegularLonLatGrid(shared->lats.size(),
                                      shared->lons.size());
    }

    else if (levelType == PRESSURE_LEVELS_3D)
    {
        grid = new MRegularLonLatStructuredPressureGrid(
                    shared->levels.size(),
                    shared->lats.size(),
                    shared->lons.size());
    }

    else if (levelType == HYBRID_SIGMA_PRESSURE_3D)
    {
        MLonLatHybridSigmaPressureGrid *sigpgrid =
                new MLonLatHybridSigmaPressureGrid(
                    shared->levels.size(),
                    shared->lats.size(),
                    shared->lons.size());

        for (unsigned int i = 0; i < sigpgrid->nlevs; i++)
        {
            sigpgrid->ak_hPa[i] = shared->ak[i];
            sigpgrid->bk[i] = shared->bk[i];
        }

        grid = sigpgrid;
    }

    else if (levelType == POTENTIAL_VORTICITY_2D)
    {
        grid = new MRegularLonLatGrid(shared->lats.size(),
                                      shared->lons.size());

    }

    else if (levelType == LOG_PRESSURE_LEVELS_3D)
    {
        grid = new MRegularLonLatLnPGrid(shared->levels.size(),
                                         shared->lats.size(),
                                         shared->lons.size());

    }

    else if (levelType == AUXILIARY_PRESSURE_3D)
    {
        MLonLatAuxiliaryPressureGrid *auxGrid =
                new MLonLatAuxiliaryPressureGrid(
                    shared->levels.size(),
                    shared->lats.size(),
                    shared->lons.size(),
                    shared->reverseLevels);
        // For the pressure field, add itself as pressure field directly since
        // it won't be done otherwise.
        if (variableName == auxiliary3DPressureField)
        {
            auxGrid->auxPressureField_hPa = auxGrid;
        }
        grid = auxGrid;
    }

    // Copy coordinate data.
    for (unsigned int i = 0; i < grid->nlons; i++)
        grid->lons[i] = shared->lons[i];

    for (unsigned int i = 0; i < grid->nlats; i++)
        grid->lats[i] = shared->lats[i];

    if ( !shared->vertVar.isNull() )
        for (unsigned int i = 0; i < grid->nlevs; i++)
            grid->levels[i] = shared->levels[i];

    // Determine the time index of this timestep.
    int timeIndex = shared->timeCoordValues.indexOf(validTime);

    // Store metadata in grid object.
    grid->setMetaData(initTime, validTime, variableName, ensembleMember);
    foreach (unsigned int m, shared->availableMembers)
    {
        grid->setAvailableMember(m);
    }

    // Load the data field.
    switch (levelType)
    {

    case PRESSURE_LEVELS_3D:
    case HYBRID_SIGMA_PRESSURE_3D:
    case AUXILIARY_PRESSURE_3D:
    case POTENTIAL_VORTICITY_2D:

        // No ensemble field.
        if ( shared->ensembleVar.isNull() )
        {
            if (shared->cfVar.getDimCount() == 4)
            {
                // Load from a 4D NetCDF variable (time, vertical, lat, lon).
                vector<size_t> start(4); start.assign(4,0);
                start[0] = timeIndex;
                vector<size_t> count(4); count.assign(4,1);
                count[1] = shared->levels.size();
                count[2] = shared->lats.size();
                count[3] = shared->lons.size();

                if (shared->reverseLatitudes || shared->reverseLevels)
                {
                    LOG4CPLUS_WARN(mlog, "WARNING: data field needs to be "
                                   "reversed w.r.t. latitude or levels. "
                                   "Performance may suffer.");
                    float *tmpData = new float[grid->nvalues];
                    QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
                    shared->cfVar.getVar(start, count, tmpData);
                    ncAccessMutexLocker.unlock();

                    if (shared->reverseLatitudes && shared->reverseLevels)
                    {
                        for (uint k = 0; k < grid->nlevs; k++)
                            for (uint j = 0; j < grid->nlats; j++)
                                for (uint i = 0; i < grid->nlons; i++)
                                {
                                    grid->setValue(k, j, i, tmpData[INDEX3zyx_2(
                                                grid->nlevs-1-k, grid->nlats-1-j, i,
                                                grid->nlatsnlons, grid->nlons)]);
                                }
                    }
                    else if (shared->reverseLatitudes)
                    {
                        for (uint k = 0; k < grid->nlevs; k++)
                            for (uint j = 0; j < grid->nlats; j++)
                                for (uint i = 0; i < grid->nlons; i++)
                                {
                                    grid->setValue(k, j, i, tmpData[INDEX3zyx_2(
                                                k, grid->nlats-1-j, i,
                                                grid->nlatsnlons, grid->nlons)]);
                                }
                    }
                    else if (shared->reverseLevels)
                    {
                        for (uint k = 0; k < grid->nlevs; k++)
                            for (uint j = 0; j < grid->nlats; j++)
                                for (uint i = 0; i < grid->nlons; i++)
                                {
                                    grid->setValue(k, j, i, tmpData[INDEX3zyx_2(
                                                grid->nlevs-1-k, j, i,
                                                grid->nlatsnlons, grid->nlons)]);
                                }
                    }

                    delete[] tmpData;
                }
                else
                {
                    QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
                    shared->cfVar.getVar(start, count, grid->data);
                }
            }
            else
            {
                // Error handling: The NetCDF variable has a different number
                // of dimensions than expected. Output error message and
                // return zero data field so that the application does not
                // crash.
                errorMsgDimensionMismatch(shared, levelType,
                                          "time, vertical, lat, lon");
                grid->setToZero();
            }
        }

        // Ensemble field.
        else
        {
            if (shared->cfVar.getDimCount() == 5)
            {
                // Load from a 5D NetCDF variable (time, ens, vertical, lat, lon).
                vector<size_t> start(5); start.assign(5,0);
                start[0] = timeIndex;
                start[1] = shared->memberToFileIndexMap.value(ensembleMember);
                vector<size_t> count(5); count.assign(5,1);
                count[1] = 1;
                count[2] = shared->levels.size();
                count[3] = shared->lats.size();
                count[4] = shared->lons.size();

                if (shared->reverseLatitudes || shared->reverseLevels)
                {
                    LOG4CPLUS_WARN(mlog, "WARNING: data field needs to be "
                                   "reversed w.r.t. latitude or levels. "
                                   "Performance may suffer.");
                    float *tmpData = new float[grid->nvalues];
                    QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
                    shared->cfVar.getVar(start, count, tmpData);
                    ncAccessMutexLocker.unlock();

                    if (shared->reverseLatitudes && shared->reverseLevels)
                    {
                        for (uint k = 0; k < grid->nlevs; k++)
                            for (uint j = 0; j < grid->nlats; j++)
                                for (uint i = 0; i < grid->nlons; i++)
                                {
                                    grid->setValue(k, j, i, tmpData[INDEX3zyx_2(
                                                grid->nlevs-1-k, grid->nlats-1-j, i,
                                                grid->nlatsnlons, grid->nlons)]);
                                }
                    }
                    else if (shared->reverseLatitudes)
                    {
                        for (uint k = 0; k < grid->nlevs; k++)
                            for (uint j = 0; j < grid->nlats; j++)
                                for (uint i = 0; i < grid->nlons; i++)
                                {
                                    grid->setValue(k, j, i, tmpData[INDEX3zyx_2(
                                                k, grid->nlats-1-j, i,
                                                grid->nlatsnlons, grid->nlons)]);
                                }
                    }
                    else if (shared->reverseLevels)
                    {
                        for (uint k = 0; k < grid->nlevs; k++)
                            for (uint j = 0; j < grid->nlats; j++)
                                for (uint i = 0; i < grid->nlons; i++)
                                {
                                    grid->setValue(k, j, i, tmpData[INDEX3zyx_2(
                                                grid->nlevs-1-k, j, i,
                                                grid->nlatsnlons, grid->nlons)]);
                                }
                    }

                    delete[] tmpData;
                }
                else
                {
                    QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
                    shared->cfVar.getVar(start, count, grid->data);
                }
            }
            else
            {
                errorMsgDimensionMismatch(shared, levelType,
                                          "time, ens, vertical, lat, lon");
                grid->setToZero();
            }
        }
        break;

    case SURFACE_2D:

        // No ensemble field.
        if ( shared->ensembleVar.isNull() )
        {
            if (shared->cfVar.getDimCount() == 3)
            {
                // Load from a 3D NetCDF variable (time, lat, lon).
                vector<size_t> start(3); start.assign(3,0);
                start[0] = timeIndex;
                vector<size_t> count(3); count.assign(3,1);
                count[1] = shared->lats.size();
                count[2] = shared->lons.size();

                if (shared->reverseLatitudes)
                {
                    LOG4CPLUS_WARN(mlog, "WARNING: data field needs to be "
                                   "reversed w.r.t. latitude. Performance may "
                                   "suffer.");
                    float *tmpData = new float[grid->nvalues];
                    QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
                    shared->cfVar.getVar(start, count, tmpData);
                    ncAccessMutexLocker.unlock();

                    MRegularLonLatGrid *grid2d =
                            static_cast<MRegularLonLatGrid*>(grid);
                    for (uint j = 0; j < grid2d->nlats; j++)
                        for (uint i = 0; i < grid2d->nlons; i++)
                        {
                            grid2d->setValue(j, i, tmpData[INDEX2yx(
                                        grid2d->nlats-1-j, i, grid2d->nlons)]);
                        }

                    delete[] tmpData;
                }
                else
                {
                    QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
                    shared->cfVar.getVar(start, count, grid->data);
                }
            }
            else
            {
                // Error handling: The NetCDF variable has a different number
                // of dimensions than expected. Output error message and
                // return zero data field so that the application does not
                // crash.
                errorMsgDimensionMismatch(shared, levelType,
                                          "time, lat, lon");
                grid->setToZero();
            }
        }

        // Ensemble field.
        else
        {
            if (shared->cfVar.getDimCount() == 4)
            {
                // Load from a 4D NetCDF variable (time, ens, lat, lon).
                vector<size_t> start(4); start.assign(4,0);
                start[0] = timeIndex;
                start[1] = shared->memberToFileIndexMap.value(ensembleMember);
                vector<size_t> count(4); count.assign(4,1);
                count[1] = 1;
                count[2] = shared->lats.size();
                count[3] = shared->lons.size();

                if (shared->reverseLatitudes)
                {
                    LOG4CPLUS_WARN(mlog, "WARNING: data field needs to be "
                                   "reversed w.r.t. latitude. Performance may "
                                   "suffer.");
                    float *tmpData = new float[grid->nvalues];
                    QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
                    shared->cfVar.getVar(start, count, tmpData);
                    ncAccessMutexLocker.unlock();

                    MRegularLonLatGrid *grid2d =
                            static_cast<MRegularLonLatGrid*>(grid);
                    for (uint j = 0; j < grid2d->nlats; j++)
                        for (uint i = 0; i < grid2d->nlons; i++)
                        {
                            grid2d->setValue(j, i, tmpData[INDEX2yx(
                                        grid2d->nlats-1-j, i, grid2d->nlons)]);
                        }

                    delete[] tmpData;
                }
                else
                {
                    QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
                    shared->cfVar.getVar(start, count, grid->data);
                }
            }
            else
            {
                errorMsgDimensionMismatch(shared, levelType,
                                          "time, ens, lat, lon");
                grid->setToZero();
            }
        }
        break;

    case LOG_PRESSURE_LEVELS_3D:
        break;

    default:
        break;

    } // switch

    // Check for missing value, if provided (apply BEFORE scale and offset!):
    // replace with M_MISSING_VALUE.
    if (shared->missingValueProvided)
    {
        for (unsigned int i = 0; i < grid->nvalues; i++)
        {
            if (floatIsAlmostEqualRelativeAndAbs(
                        grid->getValue(i), shared->missingValue, 1.E-6f))
            {
                grid->setValue(i, M_MISSING_VALUE);
            }
        }
    }

    // Apply offset and scale, if provided.
    if (shared->scaleAndOffsetProvided)
    {
        for (unsigned int i = 0; i < grid->nvalues; i++)
        {
            if (grid->getValue(i) == M_MISSING_VALUE) continue;
            grid->setValue(i, grid->getValue(i)
                           * shared->scale_factor + shared->add_offset);
        }
    }

    if (!auxiliary3DPressureField.isEmpty()
            && variableName == auxiliary3DPressureField)
    {
        // If auxiliary 3D pressure field is specified in Pa, convert to hPa.
        string units = "";
        try { shared->cfVar.getAtt("units").getValues(units); }
        catch (NcException) {}

        if (units == "Pa")
        {
            for (unsigned int i = 0; i < grid->nvalues; i++)
            {
                grid->setValue(i, grid->getValue(i) / 100.);
            }
        }
        else if (units != "hPa")
        {
            QString err = QString(
                        "invalid units '%1' for pressure levels of auxiliary "
                        "pressure field (must be Pa or hPa)").arg(units.c_str());
            throw MNcException("NcException", err.toStdString(),
                               __FILE__, __LINE__);
        }

        // Check for zero pressure values in grid. If these are present,
        // problems will likely arise when the data is used (e.g. interpolation
        // or shaders including the raycaster may crash).
        if (grid->min() == 0.)
        {
            LOG4CPLUS_WARN(mlog,
                           "SEVERE WARNING: auxiliary pressure field "
                           "contains '0' values. This may lead to rendering "
                           "problems up to program crashes. Please fix your "
                           "data and remove all entries with '0' pressure. "
                           "If the '0' entries are located along a domain "
                           "border, you can use the 'ncks' utility, e.g. "
                           "'ncks -d lat,<min>,<max> in.nc out.nc'.");
        }
    }

#ifdef MSTOPWATCH_ENABLED
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "single member data field read in "
                    << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif

    // Return data field.
    return grid;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

QString MClimateForecastReader::dataFieldFile(
        MVerticalLevelType  levelType,
        const QString&      variableName,
        const QDateTime&    initTime,
        const QDateTime&    validTime,
        const unsigned int& ensembleMember)
{
    QString filename = "";

    QReadLocker availableItemsReadLocker(&availableItemsLock);

    if (availableDataFields.value(levelType).keys().contains(variableName))
        filename = availableDataFields.value(levelType)
                .value(variableName)->timeMap.value(initTime)
                .value(validTime).value(ensembleMember).filename;
    else if (availableDataFieldsByStdName.value(levelType).keys().contains(variableName))
        filename = availableDataFieldsByStdName.value(levelType)
                .value(variableName)->timeMap.value(initTime)
                .value(validTime).value(ensembleMember).filename;

    if (filename.length() == 0)
    {
        QString msg = QString("ERROR: Cannot find datafield %1/%2/%3/%4")
                .arg(MStructuredGrid::verticalLevelTypeToString(levelType))
                .arg(variableName)
                .arg(initTime.toString(Qt::ISODate))
                .arg(validTime.toString(Qt::ISODate));
        LOG4CPLUS_ERROR(mlog, msg.toStdString());

        throw MBadDataFieldRequest("no such datafield available",
                                   __FILE__, __LINE__);
    }

    return dataRoot.filePath(filename);
}


bool MClimateForecastReader::parseCfStandardNameFile(const QString &filename)
{
    std::ifstream file(filename.toStdString());

    if (!file.is_open())
    {
        LOG4CPLUS_WARN(mlog, "WARNING: Cannot open CF standard name file <"
                       << filename.toStdString() << ">." << flush);
        return false;
    }

    std::string keyword = "";
    std::string currStdName = "";
    char line[1000];

    while (file.getline(line, 1000))
    {
        std::stringstream lineStream;
        lineStream << line;

        lineStream >> keyword;

        if (keyword[0] == '#')
        {
            continue;
        }
        else if (keyword == "standard_name")
        {
            lineStream >> currStdName;
        }
        else if (keyword == "variable_name")
        {
            std::string name;
            lineStream >> name;

            variableToStandardNameMap[QString(name.c_str())] =
                    QString(currStdName.c_str());
        }
    }

    file.close();

    return true;
}


bool MClimateForecastReader::checkSharedVariableDataConsistency(
        MVariableDataSharedPerFile *shared,
        netCDF::NcCFVar *cfVar,
        MVerticalLevelType levelType,
        bool initialiseConsistencyData,
        bool testEnsembleMemberConsistency)
{
    MVariableDataSharedPerFile current;

    // Query latitude, longitude and time coordinate system variables.
    current.latVar = cfVar->getLatitudeVar();
    current.lonVar = cfVar->getLongitudeVar();
    current.timeVar = cfVar->getTimeVar();

    // Query scale and offset, if provided.
    try { cfVar->getAtt("scale_factor").getValues(&(current.scale_factor)); }
    catch (NcException) { current.scale_factor = 1.; }
    try { cfVar->getAtt("add_offset").getValues(&(current.add_offset)); }
    catch (NcException) { current.add_offset = 0.; }

    // Have scale and offset been provided? If not, they are not applied.
    current.scaleAndOffsetProvided =
            (current.scale_factor != 1.) || (current.add_offset != 0.);

    if (!initialiseConsistencyData
            && (current.scale_factor != shared->scale_factor)
            && (current.add_offset != shared->add_offset))
    {
        LOG4CPLUS_ERROR(
                    mlog,
                    "detected inconsistency in 'scale factor' or 'add offset'.");
        return false;
    }

    switch (levelType)
    {
    case PRESSURE_LEVELS_3D:
    {
        if (convertGeometricHeightToPressure_ICAOStandard)
        {
            // Special case: If conversion of vertical geometric height
            // levels to pressure levels is enabled, the grid will appear
            // in Met.3D as a regular pressure level grid.
            current.vertVar = cfVar->getVerticalCoordinateGeometricHeight();
        }
        else
        {
            current.vertVar = cfVar->getVerticalCoordinatePressure();
        }
        break;
    }
    case HYBRID_SIGMA_PRESSURE_3D:
    {
        NcVar apVar, bVar;
        QString psName;
        current.vertVar =
                cfVar->getVerticalCoordinateHybridSigmaPressure(
                    &apVar, &bVar, &psName);

        // Read hybrid coefficients, if available
        if ( !apVar.isNull() )
        {
            current.ak.resize(apVar.getDim(0).getSize());
            apVar.getVar(current.ak.data());

            // Check the units of the ak coefficients: Pa or hPa? We need hPa!
            string units = "";
            try { apVar.getAtt("units").getValues(units); }
            catch (NcException) {}

            if (units == "Pa")
            {
                // .. hence, if the ak are given in Pa, convert to hPa.
                for (int i = 0; i < current.ak.size(); i++)
                {
                    current.ak[i] /= 100.;
                }
            }

            // QVector operator != compares QVectors componentwise:
            // https://doc.qt.io/qt-5/qvector.html#operator-not-eq (Qt 5.10)
            if (!initialiseConsistencyData && (current.ak != shared->ak))
            {
                LOG4CPLUS_ERROR(
                            mlog,
                            "detected inconsistency in 'ak coefficients'.");
                return false;
            }
        }

        if ( !bVar.isNull() )
        {
            current.bk.resize(bVar.getDim(0).getSize());
            bVar.getVar(current.bk.data());

            if (!initialiseConsistencyData && (current.bk != shared->bk))
            {
                LOG4CPLUS_ERROR(
                            mlog,
                            "detected inconsistency in 'bk coefficients'.");
                return false;
            }
        }
        break;
    }
    case POTENTIAL_VORTICITY_2D:
    {
        current.vertVar = cfVar->getVerticalCoordinatePotVort();
        break;
    }
    case SURFACE_2D:
    case LOG_PRESSURE_LEVELS_3D:
    case AUXILIARY_PRESSURE_3D:
    default:
        break;
    }

    // Read coordinate variables.
    current.lons.resize(current.lonVar.getDim(0).getSize());
    current.lonVar.getVar(current.lons.data());
    current.lats.resize(current.latVar.getDim(0).getSize());
    current.latVar.getVar(current.lats.data());

    // Check if longitudes are cyclic (0..360 or -180..180 etc) and OVERLAP
    // (e.g. starts with -180 and ends with +180). In these cases we don't
    // need the redundant lognitude.
    // NOTE: Using M_LONLAT_RESOLUTION workaround, see mutil.h
    double lon_west = MMOD(current.lons[0], 360.);
    double lon_east = MMOD(current.lons[current.lons.size()-1], 360.);
    if ( fabs(lon_west - lon_east) < M_LONLAT_RESOLUTION )
    {
        current.lons.pop_back();
    }


    if (!initialiseConsistencyData && (current.lons != shared->lons))
    {
        LOG4CPLUS_ERROR(
                    mlog,
                    "detected inconsistency in 'longitudes'.");
        return false;
    }

    if (!initialiseConsistencyData && (current.lats != shared->lats))
    {
        LOG4CPLUS_ERROR(
                    mlog,
                    "detected inconsistency in 'latitudes'.");
        return false;
    }


    if ( !current.vertVar.isNull() )
    {
        current.levels.resize(current.vertVar.getDim(0).getSize());
        current.vertVar.getVar(current.levels.data());

        if (levelType == PRESSURE_LEVELS_3D)
        {
            // If vertical levels are specified in Pa, convert to hPa.
            string units = "";
            try { current.vertVar.getAtt("units").getValues(units); }
            catch (NcException) {}

            if (units == "Pa")
            {
                for (int i = 0; i < current.levels.size(); i++)
                {
                    current.levels[i] /= 100.;
                }
            }
            else if (units != "hPa")
            {
                // If vertical levels are specified in metres and convertion
                // flag is set, convert metres to pressure with standard ICAO
                // field.
                if (NcCFVar::validUnitsVerticalCoordinate(
                            NcCFVar::LAT_LON_Z).contains(units.c_str())
                        && convertGeometricHeightToPressure_ICAOStandard)
                {
                    for (int i = 0; i < current.levels.size(); i++)
                    {
                        current.levels[i] = metre2pressure_standardICAO(
                                    current.levels[i]) / 100.;
                    }
                }
            }
        }
    }

    if (!initialiseConsistencyData && (current.levels != shared->levels))
    {
        LOG4CPLUS_ERROR(
                    mlog,
                    "detected inconsistency in 'vertical levels'.");
        return false;
    }

    // Query ensemble dimension.
    try
    {
        current.ensembleVar = cfVar->getEnsembleVar();
        current.availableMembers = cfVar->getEnsembleMembers(
                    &(current.memberToFileIndexMap));
    }
    catch (NcException)
    {
    }

    if (testEnsembleMemberConsistency)
    {
        if (!initialiseConsistencyData
                && (current.availableMembers != shared->availableMembers))
        {
            LOG4CPLUS_ERROR(
                        mlog,
                        "detected inconsistency in 'available ensemble members'.");
            return false;
        }
    }

    if (initialiseConsistencyData)
    {
        shared->levels = current.levels;
        shared->lats = current.lats;
        shared->lons = current.lons;
        shared->ak = current.ak;
        shared->bk = current.bk;
        shared->availableMembers = current.availableMembers;
        shared->scale_factor = current.scale_factor;
        shared->add_offset = current.add_offset;
    }

    return true;
}


QString MClimateForecastReader::errorMsgDimensionMismatch(
        MVariableDataSharedPerFile *shared, MVerticalLevelType levelType,
        QString expectedDims)
{
    QString msg = QString("ERROR: cannot load data field - NetCDF "
                          "variable %1 (type %2) has unexpected number of "
                          "dimensions (found %3, expected are: %4). "
                          "RETURNING DATA FIELD WITH ZERO VALUES.")
            .arg(shared->cfVar.getName().c_str())
            .arg(MStructuredGrid::verticalLevelTypeToString(levelType))
            .arg(shared->cfVar.getDimCount())
            .arg(expectedDims);

    LOG4CPLUS_ERROR(mlog, msg.toStdString());
    return msg;
}


} // namespace Met3D
