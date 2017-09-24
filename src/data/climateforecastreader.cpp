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
        QString identifier, bool treatRotatedGridAsRegularGrid)
    : MWeatherPredictionReader(identifier),
      treatRotatedGridAsRegularGrid(treatRotatedGridAsRegularGrid)

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
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    return availableDataFields[levelType].keys();
}


QSet<unsigned int> MClimateForecastReader::availableEnsembleMembers(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);

    if (availableDataFields[levelType].keys().contains(variableName))
    {
        return availableDataFields[levelType][variableName]->availableMembers;
    }
    else if (availableDataFieldsByStdName[levelType].keys().contains(variableName))
    {
        return availableDataFieldsByStdName[levelType][variableName]->availableMembers;
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
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);

    if (availableDataFields[levelType].keys().contains(variableName))
    {
        return availableDataFields[levelType][variableName]->timeMap.keys();
    }
    else if (availableDataFieldsByStdName[levelType].keys().contains(variableName))
    {
        return availableDataFieldsByStdName[levelType][variableName]->timeMap.keys();
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
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);

    if (availableDataFields[levelType].keys().contains(variableName))
    {
        if (!availableDataFields[levelType][variableName]->timeMap.keys().contains(initTime))
            throw MBadDataFieldRequest(
                    "unkown init time requested: " +
                    initTime.toString(Qt::ISODate).toStdString(),
                    __FILE__, __LINE__);
        else
            return availableDataFields[levelType][variableName]->timeMap[initTime].keys();
    }
    else if(availableDataFieldsByStdName[levelType].keys().contains(variableName))
    {
        if (!availableDataFieldsByStdName[levelType][variableName]->timeMap.keys().contains(initTime))
            throw MBadDataFieldRequest(
                    "unkown init time requested: " +
                    initTime.toString(Qt::ISODate).toStdString(),
                    __FILE__, __LINE__);
        else
            return availableDataFieldsByStdName[levelType][variableName]->timeMap[initTime].keys();
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
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);

    if (availableDataFields[levelType].keys().contains(variableName))
    {
        return availableDataFields[levelType][variableName]->longname;
    }
    else if (availableDataFieldsByStdName[levelType].keys().contains(variableName))
    {
        return availableDataFieldsByStdName[levelType][variableName]->longname;
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
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);

    if (availableDataFields[levelType].keys().contains(variableName))
    {
        return availableDataFields[levelType][variableName]->standardname;
    }
    else if (availableDataFieldsByStdName[levelType].keys().contains(variableName))
    {
        return availableDataFieldsByStdName[levelType][variableName]->standardname;
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
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (availableDataFields[levelType].keys().contains(variableName))
    {
        return availableDataFields[levelType][variableName]->units;
    }
    else if (availableDataFieldsByStdName[levelType].keys().contains(variableName))
    {
        return availableDataFieldsByStdName[levelType][variableName]->units;
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
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (availableDataFields[levelType].keys().contains(variableName))
    {
        return availableDataFields[levelType][variableName]->surfacePressureName;
    }
    else if (availableDataFieldsByStdName[levelType].keys().contains(variableName))
    {
        return availableDataFieldsByStdName[levelType][variableName]->surfacePressureName;
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
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
    {
        throw MBadDataFieldRequest(
                    "unkown level type requested: " +
                    MStructuredGrid::verticalLevelTypeToString(
                        levelType).toStdString(),
                    __FILE__, __LINE__);
    }
    if (availableDataFields[levelType].keys().contains(variableName))
    {
        return availableDataFields[levelType][variableName]->horizontalGridType;
    }
    else if (availableDataFieldsByStdName[levelType].keys().contains(
                 variableName))
    {
        return availableDataFieldsByStdName[levelType][
                variableName]->horizontalGridType;
    }
    else
    {
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    }

}


QVector2D MClimateForecastReader::variableRotatedNorthPoleCoordinates(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
    {
        throw MBadDataFieldRequest(
                    "unkown level type requested: " +
                    MStructuredGrid::verticalLevelTypeToString(
                        levelType).toStdString(),
                    __FILE__, __LINE__);
    }
    if (availableDataFields[levelType].keys().contains(variableName))
    {
        if (availableDataFields[levelType][variableName]->horizontalGridType
                == ROTATED_LONLAT)
        {
            QVector2D coordinates;
            coordinates.setX(
                        availableDataFields[levelType][variableName]
                        ->rotatedNorthPoleLon);
            coordinates.setY(
                        availableDataFields[levelType][variableName]
                        ->rotatedNorthPoleLat);

            return coordinates;
        }
        else
        {
            throw MBadDataFieldRequest(
                        "Rotated north pole coordinates requested for grid not "
                        "rotated", __FILE__, __LINE__);
        }
    }
    else if (availableDataFieldsByStdName[levelType].keys().contains(
                 variableName))
    {
        if (availableDataFields[levelType][variableName]->horizontalGridType
                == ROTATED_LONLAT)
        {
            QVector2D coordinates;
            coordinates.setX(
                        availableDataFieldsByStdName[levelType][variableName]
                        ->rotatedNorthPoleLon);
            coordinates.setY(
                        availableDataFieldsByStdName[levelType][variableName]
                        ->rotatedNorthPoleLat);

            return coordinates;
        }
        else
        {
            throw MBadDataFieldRequest(
                        "Rotated north pole coordinates requested for grid not "
                        "rotated", __FILE__, __LINE__);
        }
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
    LOG4CPLUS_DEBUG(mlog, "Using file filter: " << fileFilter.toStdString());
    LOG4CPLUS_DEBUG(mlog, "Available files with forecast data:");

    // Get a list of all files in the directory that match the wildcard name
    // filter given in "fileFilter".
    QStringList availableFiles = dataRoot.entryList(
                QStringList(fileFilter), QDir::Files);

    // For each file, open the file and extract information about the contained
    // variables and forecast valid times.
    foreach (QString fileName, availableFiles)
    {
        LOG4CPLUS_DEBUG(mlog, "\tParsing file "
                        << fileName.toStdString() << " .." << flush);

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

        QStringList gridMappingVarNames = QStringList();
        // Since we don't know the name of the grid mapping variable, loop over
        // all variables, check if variable with grid_mapping_name
        // 'rotated_latitude_longitude' exist and store their names in
        // gridMappingVarNames. Find the grid mapping variables first since
        // they are needed to check if a variable if defined on a rotated grid.
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
                switch (currCFVar.getGridType())
                {
                case NcCFVar::LAT_LON:
                    levelType = SURFACE_2D;
                    break;
                case NcCFVar::LAT_LON_P:
                    levelType = PRESSURE_LEVELS_3D;
                    break;
                case NcCFVar::LAT_LON_HYBRID:
                    levelType = HYBRID_SIGMA_PRESSURE_3D;
                    break;
                case NcCFVar::LAT_LON_PVU:
                    levelType = POTENTIAL_VORTICITY_2D;
                    break;
                default:
                    // If neither of the above choices could be matched,
                    // discard this variable and continue.
                    continue;
                    break;
                }

                // Create a new MVariableInfo struct and store availabe
                // variable information in this field.
                MVariableInfo* vinfo;
                if (availableDataFields[levelType].contains(varName))
                {
                    vinfo = availableDataFields[levelType].value(varName);
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


                    vinfo->horizontalGridType = MHorizontalGridType::REGULAR_LONLAT;
                    // Change grid data type to ROTATED_LONLAT if a grid mapping
                    // variable exists and it is assigned to this variable.
                    if (!gridMappingVarNames.empty())
                    {
                        QString gridMappingVarName = "";
                        if (NcCFVar::isDefinedOnRotatedGrid(
                                    ncFile->getVar(varName.toStdString()),
                                    gridMappingVarNames, &gridMappingVarName))
                        {
                            if (NcCFVar::getRotatedNorthPoleCoordinates(
                                        ncFile->getVar(
                                            gridMappingVarName.toStdString()),
                                        &(vinfo->rotatedNorthPoleLon),
                                        &(vinfo->rotatedNorthPoleLat)))
                            {
                                vinfo->horizontalGridType = MHorizontalGridType::ROTATED_LONLAT;
                            }
                        }

                        // At the moment, only register rotated_lonlat variable
                        // if the user wants to treat rotated grids as regular
                        // grids. Later it should be possible to treat rotated
                        // grids as rotated grids.
                        if (!treatRotatedGridAsRegularGrid
                                && (vinfo->horizontalGridType
                                    == MHorizontalGridType::ROTATED_LONLAT))
                        {
                            continue;
                        }
                    }
                }

                foreach (QDateTime validTime, currTimeCoordValues) // in UTC!
                {
                    MDatafieldInfo info;
                    info.filename = fileName;
                    vinfo->timeMap[initTime][validTime] = info;

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
        filename = dataFieldFile(levelType, variableName, initTime, validTime);
    }
    catch (MBadDataFieldRequest& e)
    {
        LOG4CPLUS_ERROR(mlog, "invalid data field requested\n" << flush);
        throw;
    }

//    LOG4CPLUS_DEBUG(mlog, "reading NetCDF data from file "
//                    << filename.toStdString());

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
            LOG4CPLUS_ERROR(mlog, "cannot open file " << filename.toStdString());
            throw;
        }
    }

    if (initialVariableAccess)
    {
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
                LOG4CPLUS_ERROR(mlog, "cannot find variable "
                                << variableName.toStdString());
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
        try { shared->cfVar.getAtt("scale_factor").getValues(&(shared->scale_factor)); }
        catch (NcException) { shared->scale_factor = 1.; }
        try { shared->cfVar.getAtt("add_offset").getValues(&(shared->add_offset)); }
        catch (NcException) { shared->add_offset = 0.; }

        // Have scale and offset been provided? If not, they are not applied.
        shared->scaleAndOffsetProvided =
                (shared->scale_factor != 1.) && (shared->add_offset != 0.);

        if (shared->scaleAndOffsetProvided)
        {
            LOG4CPLUS_DEBUG(mlog, "\tScale and offset have been provided:"
                            << " scale = " << shared->scale_factor
                            << " offset = " << shared->add_offset
                            << ".");
        }

        // Read grid type dependent coordinate data.
        if (levelType == SURFACE_2D)
        {
        }

        else if (levelType == PRESSURE_LEVELS_3D)
        {
            shared->vertVar = shared->cfVar.getVerticalCoordinatePressure();
        }

        else if (levelType == HYBRID_SIGMA_PRESSURE_3D)
        {
            NcVar apVar, bVar;
            QString psName;
            shared->vertVar =
                    shared->cfVar.getVerticalCoordinateHybridSigmaPressure(
                        &apVar, &bVar, &psName);

            // Read hybrid coefficients, if available
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
                    throw MNcException(
                            "NcException",
                            "invalid units for ak coefficients (must be Pa or hPa)",
                            __FILE__, __LINE__);
                }
            }

            if ( !bVar.isNull() )
            {
                shared->bk.resize(bVar.getDim(0).getSize());
                bVar.getVar(shared->bk.data());
            }

        }

        else if (levelType == POTENTIAL_VORTICITY_2D)
        {
            shared->vertVar = shared->cfVar.getVerticalCoordinatePotVort();

        }

        else if (levelType == LOG_PRESSURE_LEVELS_3D)
        {
        }

        else if (levelType == AUXILIARY_PRESSURE_3D)
        {

        }

        // Read coordinate variables.
        shared->lons.resize(shared->lonVar.getDim(0).getSize());
        shared->lonVar.getVar(shared->lons.data());
        shared->lats.resize(shared->latVar.getDim(0).getSize());
        shared->latVar.getVar(shared->lats.data());

        // Check if longitudes are cyclic (0..360 or -180..180 etc) and OVERLAP
        // (e.g. starts with -180 and ends with +180). In these cases we don't
        // need the redundant lonitude.
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
                            << MStructuredGrid::verticalLevelTypeToString(levelType).toStdString()
                            << ", vertical variable is '" << shared->vertVar.getName()
                            << "' (" << shared->vertVar.getDim(0).getSize() << " elements).");
            shared->levels.resize(shared->vertVar.getDim(0).getSize());
            shared->vertVar.getVar(shared->levels.data());

            // Determine whether the grid's vertical levels must be reversed.
            double lev_bot = shared->levels[0];
            double lev_top = shared->levels[shared->levels.size()-1];
            if (lev_bot > lev_top)
            {
                LOG4CPLUS_DEBUG(mlog, "\tReversing levels.");
                shared->reverseLevels = true;
                QVector<double> tmpLevs(shared->levels);
                int size = shared->levels.size();
                for (int i = 0; i < size; i++) shared->levels[i] = tmpLevs[size-1-i];
            }
            else
            {
                shared->reverseLevels = false;
            }

            if (levelType == PRESSURE_LEVELS_3D)
            {
                // If vertical levels are specified in Pa, convert to hPa.
                string units = "";
                try { shared->vertVar.getAtt("units").getValues(units); }
                catch (NcException) {}

                if (units == "Pa")
                {
                    for (int i = 0; i < shared->levels.size(); i++)
                    {
                        shared->levels[i] /= 100.;
                    }
                }
                else if (units != "hPa")
                {
                    throw MNcException(
                            "NcException",
                            "invalid units for pressure levels (must be Pa or hPa)",
                            __FILE__, __LINE__);
                }
            }
        }
        else
        {
            LOG4CPLUS_DEBUG(mlog, "\tNo vertical dimension.");
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
        grid->setAvailableMember(m);

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
                // This shouldn't happen -- raise an exception.
                throw MBadDataFieldRequest("variable has unexpected number of "
                                           "dimensions (should be 4)",
                                           __FILE__, __LINE__);
            }
        }

        // Ensemble field.
        else
        {
            if (shared->cfVar.getDimCount() == 5)
            {
                // Load from a 5D NetCDF variable (ens, time, vertical, lat, lon).
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
                // This shouldn't happen -- raise an exception.
                throw MBadDataFieldRequest("variable has unexpected number of "
                                           "dimensions (should be 5)",
                                           __FILE__, __LINE__);
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
                // This shouldn't happen -- raise an exception.
                throw MBadDataFieldRequest("variable has unexpected number of "
                                           "dimensions (should be 3)",
                                           __FILE__, __LINE__);
            }
        }

        // Ensemble field.
        else
        {
            if (shared->cfVar.getDimCount() == 4)
            {
                // Load from a 3D NetCDF variable (time, lat, lon).
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
                // This shouldn't happen -- raise an exception.
                throw MBadDataFieldRequest("variable has unexpected number of "
                                           "dimensions (should be 4)",
                                           __FILE__, __LINE__);
            }
        }
        break;

    case LOG_PRESSURE_LEVELS_3D:
        break;

    default:
        break;

    } // switch

    // Apply offset and scale, if provided.
    if (shared->scaleAndOffsetProvided)
    {
        for (unsigned int i = 0; i < grid->nvalues; i++)
            grid->setValue(i, grid->getValue(i)
                           * shared->scale_factor + shared->add_offset);
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
        MVerticalLevelType levelType,
        const QString&     variableName,
        const QDateTime&   initTime,
        const QDateTime&   validTime)
{
    QString filename = "";

    QReadLocker availableItemsReadLocker(&availableItemsLock);

    if (availableDataFields.value(levelType).keys().contains(variableName))
        filename = availableDataFields.value(levelType)
                .value(variableName)->timeMap.value(initTime)
                .value(validTime).filename;
    else if (availableDataFieldsByStdName.value(levelType).keys().contains(variableName))
        filename = availableDataFieldsByStdName.value(levelType)
                .value(variableName)->timeMap.value(initTime)
                .value(validTime).filename;

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


} // namespace Met3D
