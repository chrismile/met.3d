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
#include "gribreader.h"

// standard library imports
#include <iostream>
#include <limits>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "util/mstopwatch.h"
#include "gxfw/mglresourcesmanager.h"

using namespace std;

namespace Met3D
{


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MGribReader::MGribReader(QString identifier, QString surfacePressureFieldType,
                         bool disableGridConsistencyCheck)
    : MWeatherPredictionReader(identifier),
      surfacePressureFieldType(surfacePressureFieldType),
      disableGridConsistencyCheck(disableGridConsistencyCheck)
{
    // Name of surface pressure field to reconstruct pressure for hybrid
    // coordinates. If set to "auto" set the string to empty here (will trigger
    // search of GRIB messages for sp/lnsp field in
    // detectSurfacePressureFieldType().
    if (surfacePressureFieldType == "auto")
    {
        this->surfacePressureFieldType = "";
    }
}


MGribReader::~MGribReader()
{
    QMutexLocker openFilesLocker(&openFilesMutex);

    // Close open Grib files & their indices.
    foreach (MGribFileInfo *finfo, openFiles) fclose(finfo->gribFile);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QList<MVerticalLevelType> MGribReader::availableLevelTypes()
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    return availableDataFields.keys();
}


QStringList MGribReader::availableVariables(
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


QSet<unsigned int> MGribReader::availableEnsembleMembers(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (!availableDataFields[levelType].keys().contains(variableName))
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    return availableDataFields[levelType][variableName]->availableMembers;
}


QList<QDateTime> MGribReader::availableInitTimes(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (!availableDataFields[levelType].keys().contains(variableName))
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    return availableDataFields[levelType][variableName]->timeMap.keys();
}


QList<QDateTime> MGribReader::availableValidTimes(
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
    if (!availableDataFields[levelType].keys().contains(variableName))
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    if (!availableDataFields[levelType][variableName]->timeMap.keys().contains(initTime))
        throw MBadDataFieldRequest(
                "unkown init time requested: " +
                initTime.toString(Qt::ISODate).toStdString(),
                __FILE__, __LINE__);
    return availableDataFields[levelType][variableName]->timeMap[initTime].keys();
}


QString MGribReader::variableLongName(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (!availableDataFields[levelType].keys().contains(variableName))
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    return availableDataFields[levelType][variableName]->longname;
}


QString MGribReader::variableStandardName(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (!availableDataFields[levelType].keys().contains(variableName))
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    return availableDataFields[levelType][variableName]->standardname;
}


QString MGribReader::variableUnits(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (!availableDataFields[levelType].keys().contains(variableName))
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    return availableDataFields[levelType][variableName]->units;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

QString MGribReader::variableSurfacePressureName(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (!availableDataFields[levelType].keys().contains(variableName))
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    return availableDataFields[levelType][variableName]->surfacePressureName;
}


QString MGribReader::variableAuxiliaryPressureName(
        MVerticalLevelType levelType,
        const QString&     variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (!availableDataFields[levelType].keys().contains(variableName))
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    return availableDataFields[levelType][variableName]->auxiliaryPressureName;
}


MHorizontalGridType MGribReader::variableHorizontalGridType(MVerticalLevelType levelType,
                                   const QString&     variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
        throw MBadDataFieldRequest(
                "unkown level type requested: " +
                MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                __FILE__, __LINE__);
    if (!availableDataFields[levelType].keys().contains(variableName))
        throw MBadDataFieldRequest(
                "unkown variable requested: " + variableName.toStdString(),
                __FILE__, __LINE__);
    return availableDataFields[levelType][variableName]->horizontalGridType;
}


QVector2D MGribReader::variableRotatedNorthPoleCoordinates(
        MVerticalLevelType levelType, const QString& variableName)
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableDataFields.keys().contains(levelType))
    {
        throw MBadDataFieldRequest(
                    "unkown level type requested: " +
                    MStructuredGrid::verticalLevelTypeToString(levelType).toStdString(),
                    __FILE__, __LINE__);
    }
    if (!availableDataFields[levelType].keys().contains(variableName))
    {
        throw MBadDataFieldRequest(
                    "unkown variable requested: " + variableName.toStdString(),
                    __FILE__, __LINE__);
    }
    if (availableDataFields[levelType][variableName]->horizontalGridType
            != ROTATED_LONLAT)
    {
        throw MBadDataFieldRequest(
                    "Rotated north pole coordinates requested for grid not "
                    "rotated", __FILE__, __LINE__);
    }
    // TODO (bt, 09Feb2017): Adapt to get rotated north pole coordinates
    // read from grib file. Reading rotated north pole coordinates from grib
    // file isn't implemented yet. If done, coordinates could be read like this:

    //    QVector2D coordinates;
    //    coordinates.setX(
    //                availableDataFields[levelType][variableName]
    //                ->rotatedNorthPoleLon);
    //    coordinates.setY(
    //                availableDataFields[levelType][variableName]
    //                ->rotatedNorthPoleLat);
    QVector2D coordinates(0.f, 0.f);
    return coordinates;
}


MStructuredGrid *MGribReader::readGrid(
        MVerticalLevelType levelType,
        const QString &variableName,
        const QDateTime &initTime,
        const QDateTime &validTime,
        unsigned int ensembleMember)
{
#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
#endif

    // Read-lock the entire method; read calls to items of availableDataFields
    // are sprinkled throughout the method.
    QReadLocker availableItemsReadLocker(&availableItemsLock);

    // Determine file name of data file that holds the requested field.
    MGribVariableInfo* vinfo = availableDataFields
            .value(levelType).value(variableName);
    MGribDatafieldInfo dinfo = vinfo->timeMap.value(initTime)
            .value(validTime).value(ensembleMember);

    if (dinfo.filename.length() == 0)
    {
        LOG4CPLUS_ERROR(mlog, "invalid data field requested\n" << flush);
        throw;
    }

    QString filePath = dataRoot.filePath(dinfo.filename);

    LOG4CPLUS_DEBUG(mlog, "reading Grib data from file "
                    << filePath.toStdString());

    // Is this file opened for the first time?
    QMutexLocker openFilesLocker(&openFilesMutex);
    MGribFileInfo* finfo = nullptr;
    if ( openFiles.contains(dinfo.filename) )
    {
        finfo = openFiles[dinfo.filename];
    }
    else
    {
        // The file is accessed for the first time -- open.
        finfo = new MGribFileInfo();
        QByteArray ba = filePath.toLocal8Bit();
        finfo->gribFile = fopen(ba.data(), "r");

        if (!finfo->gribFile)
        {
            LOG4CPLUS_ERROR(mlog, "cannot open file "
                            << filePath.toStdString());
            throw;
        }

        openFiles.insert(dinfo.filename, finfo);
    }

    // Shortcut to the file handle.
    FILE *gribFile = finfo->gribFile;

    // Lock access to "filename" and associated data until the end of this
    // method.
    QMutexLocker accessMutexLocker(&(finfo->accessMutex));

    openFilesLocker.unlock();


    // Return value.
    MStructuredGrid *grid = nullptr;

    if (levelType == SURFACE_2D)
    {
        grid = new MRegularLonLatGrid(vinfo->nlats, vinfo->nlons);

        // Copy coordinate data.
        for (unsigned int i = 0; i < grid->nlons; i++)
            grid->lons[i] = vinfo->lons[i];
        for (unsigned int i = 0; i < grid->nlats; i++)
            grid->lats[i] = vinfo->lats[i];

        // Store metadata in grid object.
        grid->setMetaData(initTime, validTime, variableName, ensembleMember);
        grid->setAvailableMembers(vinfo->availableMembers_bitfield);

        // Seek current message in file and get grib handle to message.
        fseek(gribFile, dinfo.offsetForLevel[0], SEEK_SET);
        int err = 0;
        grib_handle* gribHandle = grib_handle_new_from_file(0, gribFile, &err);
        GRIB_CHECK(err, 0);

        if (gribHandle != NULL)
        {
            // Read data values into grid object.

            // Set the missing value number.
            GRIB_CHECK(grib_set_double(gribHandle, "missingValue",
                                       M_MISSING_VALUE), 0);

            // Get the size of the values array
            size_t nGribValues = 0;
            GRIB_CHECK(grib_get_size(gribHandle, "values", &nGribValues), 0);
            if (nGribValues != grid->nvalues)
            {
                LOG4CPLUS_ERROR(mlog, "Number of data values in grib message "
                                "does not correspond to expected data size. "
                                "Cannot read data values.");
            }

            // Get data.
            double *values = new double[nGribValues];
            GRIB_CHECK(grib_get_double_array(gribHandle, "values", values,
                                             &nGribValues), 0);

            // Copy double data to MStructuredGrid float array.
            if (dinfo.applyExp)
            {
                // If surface pressure is specified as lnsp, apply exponential
                // function to reconstruct surface pressure "sp".
                for (uint n = 0; n < nGribValues; n++)
                {
                    grid->data[n] = exp(values[n]);
                }
            }
            else
            {
                for (uint n = 0; n < nGribValues; n++)
                {
                    grid->data[n] = values[n];
                }
            }

            delete[] values;

            grib_handle_delete(gribHandle);
        }
        else
        {
            LOG4CPLUS_ERROR(mlog, "Could not read grib message.");
        }
    }

    else if (levelType == PRESSURE_LEVELS_3D)
    {
        grid = new MRegularLonLatStructuredPressureGrid(
                    vinfo->levels.size(), vinfo->nlats, vinfo->nlons);

        // Copy coordinate data.
        for (unsigned int i = 0; i < grid->nlons; i++)
            grid->lons[i] = vinfo->lons[i];
        for (unsigned int i = 0; i < grid->nlats; i++)
            grid->lats[i] = vinfo->lats[i];
        for (unsigned int i = 0; i < grid->nlevs; i++)
            grid->levels[i] = vinfo->levels[i];

        // Store metadata in grid object.
        grid->setMetaData(initTime, validTime, variableName, ensembleMember);
        grid->setAvailableMembers(vinfo->availableMembers_bitfield);

        // Loop over levels, read each level and store in grid.
        for (int il = 0; il < vinfo->levels.size(); il++)
        {
            long level = vinfo->levels[il];

            // Seek current message in file and get grib handle to message.
            fseek(gribFile, dinfo.offsetForLevel[level], SEEK_SET);
            int err = 0;
            grib_handle* gribHandle = grib_handle_new_from_file(0, gribFile, &err);
            GRIB_CHECK(err, 0);

            if (gribHandle != NULL)
            {
                // Read data values into grid object.

                // Set the missing value number.
                GRIB_CHECK(grib_set_double(gribHandle, "missingValue",
                                           M_MISSING_VALUE), 0);

                // Get the size of the values array
                size_t nGribValues = 0;
                GRIB_CHECK(grib_get_size(gribHandle, "values", &nGribValues), 0);
                if (nGribValues != grid->nlatsnlons)
                {
                    LOG4CPLUS_ERROR(mlog, "Number of data values in grib message "
                                    "does not correspond to expected data size. "
                                    "Cannot read data values.");
                }

                // Get data.
                double *values = new double[nGribValues];
                GRIB_CHECK(grib_get_double_array(gribHandle, "values", values,
                                                 &nGribValues), 0);

                // Copy double data to MStructuredGrid float array.
                for (uint n = 0; n < nGribValues; n++)
                    grid->data[il*nGribValues + n] = values[n];

                delete[] values;

                grib_handle_delete(gribHandle);
            }
            else
            {
                LOG4CPLUS_ERROR(mlog, "Could not read grib message.");
            }
        }
    }

    else if (levelType == HYBRID_SIGMA_PRESSURE_3D)
    {
        MLonLatHybridSigmaPressureGrid *sigpgrid =
                new MLonLatHybridSigmaPressureGrid(
                    vinfo->levels.size(), vinfo->nlats, vinfo->nlons);

        grid = sigpgrid;

        // Copy coordinate data.
        for (uint i = 0; i < grid->nlons; i++) grid->lons[i] = vinfo->lons[i];
        for (uint i = 0; i < grid->nlats; i++) grid->lats[i] = vinfo->lats[i];
        for (uint i = 0; i < grid->nlevs; i++) grid->levels[i] = vinfo->levels[i];

        // Copy ak/bk coefficients. If not all levels are stored in the dataset,
        // make sure that the correct ak/bk are copied (-> levelOffset).
        // NOTE: Met.3D only supports continous levels, no support for missing
        // levels is currently implemented.
        uint levelOffset = grid->levels[0]-1;
        sigpgrid->allocateInterfaceCoefficients();
        for (uint i = 0; i <= sigpgrid->nlevs; i++)
        {
            sigpgrid->aki_hPa[i] = vinfo->aki_hPa[i+levelOffset];
            sigpgrid->bki[i] = vinfo->bki[i+levelOffset];

            if (i < sigpgrid->nlevs)
            {
                sigpgrid->ak_hPa[i] = vinfo->ak_hPa[i+levelOffset];
                sigpgrid->bk[i] = vinfo->bk[i+levelOffset];
            }
        }

        // Store metadata in grid object.
        grid->setMetaData(initTime, validTime, variableName, ensembleMember);
        grid->setAvailableMembers(vinfo->availableMembers_bitfield);

        // Loop over levels, read each level and store in grid.
        for (int il = 0; il < vinfo->levels.size(); il++)
        {
            long level = vinfo->levels[il];

            // Seek current message in file and get grib handle to message.
            fseek(gribFile, dinfo.offsetForLevel[level], SEEK_SET);
            int err = 0;
            grib_handle* gribHandle = grib_handle_new_from_file(0, gribFile, &err);
            GRIB_CHECK(err, 0);

            if (gribHandle != NULL)
            {
                // Read data values into grid object.

                // Set the missing value number.
                GRIB_CHECK(grib_set_double(gribHandle, "missingValue",
                                           M_MISSING_VALUE), 0);

                // Get the size of the values array
                size_t nGribValues = 0;
                GRIB_CHECK(grib_get_size(gribHandle, "values", &nGribValues), 0);
                if (nGribValues != grid->nlatsnlons)
                {
                    LOG4CPLUS_ERROR(mlog, "Number of data values in grib message "
                                    "does not correspond to expected data size. "
                                    "Cannot read data values.");
                }

                // Get data.
                double *values = new double[nGribValues];
                GRIB_CHECK(grib_get_double_array(gribHandle, "values", values,
                                                 &nGribValues), 0);

                // Copy double data to MStructuredGrid float array.
                for (uint n = 0; n < nGribValues; n++)
                    grid->data[il*nGribValues + n] = values[n];

                delete[] values;

                grib_handle_delete(gribHandle);
            }
            else
            {
                LOG4CPLUS_ERROR(mlog, "Could not read grib message.");
            }
        }
    }

    else if (levelType == POTENTIAL_VORTICITY_2D)
    {
        LOG4CPLUS_ERROR(mlog, "Reading GRIB potential vorticity fields has not "
                        "been implemented yet.");
    }

    else if (levelType == LOG_PRESSURE_LEVELS_3D)
    {
        LOG4CPLUS_ERROR(mlog, "Reading GRIB log(pressure) fields has not "
                        "been implemented yet.");
    }

    else if (levelType == AUXILIARY_PRESSURE_3D)
    {
        LOG4CPLUS_ERROR(mlog, "Reading GRIB auxiliary pressure fields has not "
                        "been implemented yet.");
    }

#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "single member GRIB data field read in "
                    << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif

    // Return data field.
    return grid;
}


void MGribReader::scanDataRoot()
{
#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
#endif

    // Lock access to all availableXX data fields.
    QWriteLocker availableItemsWriteLocker(&availableItemsLock);

    LOG4CPLUS_DEBUG(mlog, "Scanning directory "
                    << dataRoot.absolutePath().toStdString() << " "
                    << "for grib files with forecast data.");
    LOG4CPLUS_DEBUG(mlog, "Using file filter: " << fileFilter.toStdString());
    LOG4CPLUS_DEBUG(mlog, "Available files:" << flush);

    // Get a list of all files in the directory that match the wildcard name
    // filter given in "fileFilter".
    QStringList availableFiles = dataRoot.entryList(
                QStringList(fileFilter), QDir::Files);

    // Scan all grib files contained in the directory.
    foreach (QString gribFileName, availableFiles)
    {
        // (Skip index files.)
        if (gribFileName.endsWith("met3d_grib_index")) continue;

        LOG4CPLUS_DEBUG(mlog, "Scanning file "
                        << gribFileName.toStdString() << " .." << flush);

        // First, read or create the grib index for fast access to messages.
        // =================================================================

        QString filePath = dataRoot.filePath(gribFileName);
        QString fileIndexPath = QString("%1.met3d_grib_index").arg(filePath);

        // If the grib index exists, read it. Otherwise, create it and store it.
        if (QFile::exists(fileIndexPath))
        {
            LOG4CPLUS_DEBUG(mlog, "Reading grib index for file "
                            << gribFileName.toStdString() << " ..");

            // Open index file.
            QFile indexFile(fileIndexPath);
            indexFile.open(QIODevice::ReadOnly);
            QDataStream indexDataStream(&indexFile);

            // Read index file header (only version number "2" supported for
            // now; this may be changed in the future when the index format
            // changes).
            qint32 indexVersion;
            indexDataStream >> indexVersion;
            if (indexVersion != 2)
            {
                LOG4CPLUS_ERROR(mlog, "ERROR: grib index has version "
                                << indexVersion << "; this version of Met.3D "
                                "only supports version 2. Skipping file -- "
                                "remove the index file if you want the grib "
                                "file to be considered.");
                continue;
            }

            indexDataStream.setVersion(QDataStream::Qt_4_8);

            MGribMessageIndexInfo gmiInfo;
            while ( !indexDataStream.atEnd() )
            {
                gmiInfo.readFromDataStream(&indexDataStream);

                MVerticalLevelType levelType = gmiInfo.levelType;

                // Create a new MVariableInfo struct and store available
                // variable information in this field.
                MGribVariableInfo* vinfo;
                if (availableDataFields[levelType].contains(gmiInfo.variablename))
                {
                    vinfo = availableDataFields[levelType].value(gmiInfo.variablename);

                    // Domain checks have already been run on index creation;
                    // skip them here (see below).
                }
                else
                {
                    vinfo = new MGribVariableInfo();
                    vinfo->variablename = gmiInfo.variablename;
                    vinfo->longname = gmiInfo.longname;
                    vinfo->standardname = gmiInfo.standardname;
                    vinfo->units = gmiInfo.units;
                    vinfo->fcType = gmiInfo.fcType;

                    vinfo->nlons = gmiInfo.nlons;
                    vinfo->nlats = gmiInfo.nlats;
                    vinfo->lon0 = gmiInfo.lon0;
                    vinfo->lat0 = gmiInfo.lat0;
                    vinfo->lon1 = gmiInfo.lon1;
                    vinfo->lat1 = gmiInfo.lat1;
                    vinfo->dlon = gmiInfo.dlon;
                    vinfo->dlat = gmiInfo.dlat;

                    // Fill lat/lon arrays.
                    vinfo->lons = gmiInfo.lons;
                    vinfo->lats = gmiInfo.lats;
                    // TODO (bt, 08FEB2017) Adapt to set type according to the data read.
                    vinfo->horizontalGridType = MHorizontalGridType::REGULAR_LONLAT;

                    if (levelType == HYBRID_SIGMA_PRESSURE_3D)
                    {
                        vinfo->surfacePressureName = gmiInfo.surfacePressureName;
                        vinfo->aki_hPa = gmiInfo.aki_hPa;
                        vinfo->bki = gmiInfo.bki;
                        vinfo->ak_hPa = gmiInfo.ak_hPa;
                        vinfo->bk = gmiInfo.bk;
                    }

                    // Insert the new MVariableInfo struct into the variable
                    // name map..
                    availableDataFields[levelType].insert(
                                vinfo->variablename, vinfo);
                    // ..and, if a CF standard name is available, into the std
                    // name map.
                    if (vinfo->standardname != "")
                        availableDataFieldsByStdName[levelType].insert(
                                    vinfo->standardname, vinfo);
                }

                long ensMember = gmiInfo.ensMember;
                vinfo->availableMembers.insert(ensMember);
                vinfo->availableMembers_bitfield |= (Q_UINT64_C(1) << ensMember);

                // Get time values of this message.
                QDateTime initTime = gmiInfo.initTime;
                QDateTime validTime = gmiInfo.validTime;

                // Store filename and offset of grib message in index.
                MGribDatafieldInfo *info = nullptr;
                if (vinfo->timeMap[initTime][validTime].contains(ensMember))
                {
                    info = &(vinfo->timeMap[initTime][validTime][ensMember]);
                    if (info->filename != gribFileName)
                    {
                        LOG4CPLUS_ERROR(mlog, "found levels of the same "
                                        "3D data field in different files"
                                        "; skipping grib message");
                        continue;
                    }
                }
                else
                {
                    info = &(vinfo->timeMap[initTime][validTime][ensMember]);
                    info->filename = gribFileName;
                }

                // Get vertical level.
                long level = gmiInfo.level;

                // Distinguish between ln surface pressure fields and surface
                // pressure fields (in index files both are stored as
                // surface_2D).
                if (levelType == SURFACE_2D)
                {
                    if (vinfo->variablename.startsWith("lnsp"))
                    {
                        setSurfacePressureFieldType("lnsp");
                        level = 0;
                        info->applyExp = true;
                    }
                    else if (vinfo->variablename.startsWith("sp"))
                    {
                        setSurfacePressureFieldType("sp");
                        info->applyExp = false;
                    }
                }
                else
                {
                    info->applyExp = false;
                }

                info->offsetForLevel[level] = gmiInfo.filePosition;

                // Insert level into list of vertical levels for this
                // variable.
                if (!vinfo->levels.contains(level)) vinfo->levels << level;

                // That's it!
            }
        } // read index
        else
        { // read from grib file and create new index
            LOG4CPLUS_DEBUG(mlog, "Creating new index... please wait." << flush);

            // Open the file.
            FILE* gribfile = NULL;
            QByteArray ba = filePath.toLocal8Bit();
            gribfile = fopen(ba.data(), "r");
            if (!gribfile)
            {
                LOG4CPLUS_WARN(mlog, "Cannot open file "
                               << filePath.toStdString() << ", skipping.");
                continue;
            }

            // Open a new index file.
            QFile indexFile(fileIndexPath);
            indexFile.open(QIODevice::WriteOnly);
            QDataStream indexDataStream(&indexFile);
            // Write index file header (version number "2" for now; this may
            // be changed in the future when the index format changes).
            indexDataStream << (qint32)2;
            indexDataStream.setVersion(QDataStream::Qt_4_8);

            // lnsp model level fix (ECMWF inconsistency: lnsp is stored on
            // a single model level, not as a surface field...).
            bool fixLNSPmodelLevel = false;

            // Loop over grib messages contained in the file.
            grib_handle* gribHandle = NULL;
            int err = 0;
            int messageCount = 0;

            // Get current file position to store in the index for this message
            // (see end of loop).
            long filePosition = ftell(gribfile);
            // LOG4CPLUS_DEBUG(mlog, "file position: " << filePosition);

            while ((gribHandle =
                    grib_handle_new_from_file(0, gribfile, &err)) != NULL)
            {
                fixLNSPmodelLevel = false;
                // LOG4CPLUS_DEBUG(mlog, "=========> Indexing message "
                //                 << messageCount);

                MGribMessageIndexInfo gmiInfo;

                // Determine type of data fields (analysis, deterministic,
                // ensemble). Append type to variable name so variables with
                // the same name but different types can be distinguished).
                QString dataType = getGribStringKey(gribHandle, "ls.dataType");
                // LOG4CPLUS_DEBUG(mlog, "ls.dataType: " << dataType.toStdString());
                // Perturbed (pf) and control (cf) forecasts are combined into
                // "ensemble" (ens) forecasts.
                if ( (dataType == "pf") || (dataType == "cf") ) dataType = "ens";

                // Currently Met.3D can only handle data fields on a regular
                // lat/lon grid in the horizontal.
                QString gridType = getGribStringKey(gribHandle, "geography.gridType");
                // LOG4CPLUS_DEBUG(mlog, "geography.gridType: " << gridType.toStdString());
                if (gridType != "regular_ll")
                {
                    LOG4CPLUS_WARN(mlog, "Met.3D can only handle 'regular_ll' "
                                   "grid, skipping this field.");
                    grib_handle_delete(gribHandle);
                    continue;
                }

                // Determine the type of the vertical level of the variable.
                QString typeOfLevel = getGribStringKey(gribHandle, "vertical.typeOfLevel");
                // LOG4CPLUS_DEBUG(mlog, "vertical.typeOfLevel: " << typeOfLevel.toStdString());

                MVerticalLevelType levelType;
                if (typeOfLevel == "surface")
                    levelType = SURFACE_2D;
                else if (typeOfLevel == "isobaricInhPa")
                    levelType = PRESSURE_LEVELS_3D;
                else if (typeOfLevel == "hybrid")
                    levelType = HYBRID_SIGMA_PRESSURE_3D;
                else if (typeOfLevel == "potentialVorticity")
                    levelType = POTENTIAL_VORTICITY_2D;
                else
                {
                    // If neither of the above choices could be matched,
                    // discard this variable and continue.
                    LOG4CPLUS_WARN(mlog, "cannot recognize level type '"
                                   << typeOfLevel.toStdString()
                                   << "', skipping this field.");
                    grib_handle_delete(gribHandle);
                    continue;
                }
                gmiInfo.levelType = levelType;

                // Determine the variable name.
                QString shortName = getGribStringKey(gribHandle, "parameter.shortName");
                // LOG4CPLUS_DEBUG(mlog, "parameter.shortName: " << shortName.toStdString());
                QString varName = QString("%1 (%2)").arg(shortName).arg(dataType);

                // Handle special case "lnsp". "lnsp" fields at ECMWF are
                // stored on model level 1 (not as a surface field). In Met.3D,
                // we re-cast as a surface field (which it is...).
                if (shortName == "lnsp" && levelType == HYBRID_SIGMA_PRESSURE_3D)
                {
                    levelType = SURFACE_2D;
                    gmiInfo.levelType = levelType;
                    fixLNSPmodelLevel = true;
                    setSurfacePressureFieldType("lnsp");

                }
                else if (shortName == "sp")
                {
                    setSurfacePressureFieldType("sp");
                }

                // Create a new MVariableInfo struct and store availabe
                // variable information in this field.
                MGribVariableInfo* vinfo;
                if (availableDataFields[levelType].contains(varName))
                {
                    vinfo = availableDataFields[levelType].value(varName);

                    MGribVariableInfo currentVInfo;
                    currentVInfo.nlons = getGribLongKey(gribHandle, "Ni");
                    currentVInfo.nlats = getGribLongKey(gribHandle, "Nj");
                    currentVInfo.lon0 = getGribDoubleKey(
                                gribHandle, "longitudeOfFirstGridPointInDegrees");
                    currentVInfo.lat0 = getGribDoubleKey(
                                gribHandle, "latitudeOfFirstGridPointInDegrees");
                    currentVInfo.lon1 = getGribDoubleKey(
                                gribHandle, "longitudeOfLastGridPointInDegrees");
                    currentVInfo.lat1 = getGribDoubleKey(
                                gribHandle, "latitudeOfLastGridPointInDegrees");
                    currentVInfo.dlon = getGribDoubleKey(
                                gribHandle, "iDirectionIncrementInDegrees");
                    currentVInfo.dlat = getGribDoubleKey(
                                gribHandle, "jDirectionIncrementInDegrees");

                    if (!checkConsistencyOfVariable(vinfo, &currentVInfo))
                    {
                        LOG4CPLUS_ERROR(mlog, "found different geographical "
                                        "region than previously used for "
                                        "variable '" + varName.toStdString()
                                        + "'; skipping grib message");
                        grib_handle_delete(gribHandle);
                        continue;
                    }

                    // Copy data to gmiInfo.
                    // NOTE: Only "variablename" is required as map key
                    // for vinfo when the index is read; all other variables
                    // can be ommitted -- they are only read from the first
                    // message of the current variable (when a new vinfo is
                    // created). This saves a huge amount of data in the index!
                    gmiInfo.variablename = vinfo->variablename;
                    // end copy
                }
                else
                {
                    vinfo = new MGribVariableInfo();
                    vinfo->variablename = gmiInfo.variablename = varName;
                    vinfo->longname = gmiInfo.longname =
                            getGribStringKey(gribHandle, "parameter.name");
//TODO (mr, 02Oct2014): fill standard name field.
                    vinfo->standardname = gmiInfo.standardname = "";
                    vinfo->units = gmiInfo.units =
                            getGribStringKey(gribHandle, "parameter.units");

                    if (dataType == "an") vinfo->fcType = ANALYSIS;
                    else if (dataType == "fc") vinfo->fcType = DETERMINISTIC_FORECAST;
                    else if (dataType == "ens") vinfo->fcType = ENSEMBLE_FORECAST;
                    else vinfo->fcType = INVALID_TYPE;

                    vinfo->nlons = gmiInfo.nlons = getGribLongKey(
                                gribHandle, "Ni");
                    vinfo->nlats = gmiInfo.nlats = getGribLongKey(
                                gribHandle, "Nj");
                    vinfo->lon0 = gmiInfo.lon0 = getGribDoubleKey(
                                gribHandle, "longitudeOfFirstGridPointInDegrees");
                    vinfo->lat0 = gmiInfo.lat0 = getGribDoubleKey(
                                gribHandle, "latitudeOfFirstGridPointInDegrees");
                    vinfo->lon1 = gmiInfo.lon1 = getGribDoubleKey(
                                gribHandle, "longitudeOfLastGridPointInDegrees");
                    vinfo->lat1 = gmiInfo.lat1 = getGribDoubleKey(
                                gribHandle, "latitudeOfLastGridPointInDegrees");
                    vinfo->dlon = gmiInfo.dlon = getGribDoubleKey(
                                gribHandle, "iDirectionIncrementInDegrees");
                    vinfo->dlat = gmiInfo.dlat = getGribDoubleKey(
                                gribHandle, "jDirectionIncrementInDegrees");

                    // Fill lat/lon arrays.
                    vinfo->lons.resize(vinfo->nlons);
                    double lon0 = (vinfo->lon0 > vinfo->lon1) ?
                                vinfo->lon0 - 360. : vinfo->lon0;
                    for (int ilon = 0; ilon < vinfo->nlons; ilon ++)
                        vinfo->lons[ilon] = lon0 + ilon*vinfo->dlon;
                    gmiInfo.lons = vinfo->lons;

                    vinfo->lats.resize(vinfo->nlats);
                    for (int ilat = 0; ilat < vinfo->nlats; ilat ++)
                        vinfo->lats[ilat] = vinfo->lat0 - ilat*vinfo->dlat;
                    gmiInfo.lats = vinfo->lats;

                    if (levelType == HYBRID_SIGMA_PRESSURE_3D)
                    {
                        detectSurfacePressureFieldType(&availableFiles);

                        vinfo->surfacePressureName =
                                gmiInfo.surfacePressureName =
                                QString("%1 (%2)").arg(surfacePressureFieldType)
                                .arg(dataType);

                        // Read hybrid level coefficients.
                        double *akbk = new double[300];
                        size_t akbk_len = 300;

                        // NOTE: Grib stores half level interface coefficients.
                        GRIB_CHECK(grib_get_double_array(gribHandle, "pv",
                                                         akbk, &akbk_len), 0);

                        int numLevels = (akbk_len / 2) - 1; // -1 to exclude surface
                        vinfo->aki_hPa.resize(numLevels+1);
                        vinfo->bki.resize(numLevels+1);
                        vinfo->ak_hPa.resize(numLevels);
                        vinfo->bk.resize(numLevels);

                        for (int il = 0; il <= numLevels; il++)
                        {
                            vinfo->aki_hPa[il] = akbk[il] / 100.;
                            vinfo->bki[il] = akbk[il + numLevels+1];

                            if (il < numLevels)
                            {
                                // Compute full level coefficients.
                                vinfo->ak_hPa[il] =
                                        akbk[il] + (akbk[il+1]-akbk[il]) / 2.;
                                vinfo->ak_hPa[il] /= 100.; // convert to hPa
                                vinfo->bk[il] =
                                        akbk[il + numLevels+1]
                                        + ( akbk[il+1 + numLevels+1]
                                        - akbk[il + numLevels+1] ) / 2.;
                            }
                        }

                        delete[] akbk;

                        gmiInfo.aki_hPa = vinfo->aki_hPa;
                        gmiInfo.bki = vinfo->bki;
                        gmiInfo.ak_hPa = vinfo->ak_hPa;
                        gmiInfo.bk = vinfo->bk;
                    }

                    // Insert the new MVariableInfo struct into the variable
                    // name map..
                    availableDataFields[levelType].insert(
                                vinfo->variablename, vinfo);
                    // ..and, if a CF standard name is available, into the std
                    // name map.
                    if (vinfo->standardname != "")
                        availableDataFieldsByStdName[levelType].insert(
                                    vinfo->standardname, vinfo);
                }

                // Determine ensemble member of this data field. Deterministic
                // and analysis datafields are stored as member "0".
                long ensMember = 0;
                if (vinfo->fcType == ENSEMBLE_FORECAST)
                {
                    ensMember = getGribLongKey(gribHandle, "perturbationNumber");
                    // LOG4CPLUS_DEBUG(mlog, "perturbationNumber: " << ensMember);
                }
                vinfo->availableMembers.insert(ensMember);
                vinfo->availableMembers_bitfield |= (Q_UINT64_C(1) << ensMember);
                gmiInfo.ensMember = ensMember;

                // Get time values of this variable.
                long dataDate = getGribLongKey(gribHandle, "time.dataDate");
                // LOG4CPLUS_DEBUG(mlog, "time.dataDate: " << dataDate);
                long dataTime = getGribLongKey(gribHandle, "time.dataTime");
                // LOG4CPLUS_DEBUG(mlog, "time.dataTime: " << dataTime);

                QString initTimeStr = QString("%1_%2").arg(dataDate)
                        .arg(dataTime, 4, 10, QChar('0'));
                QDateTime initTime = QDateTime::fromString(initTimeStr,
                                                           "yyyyMMdd_hhmm");
                initTime.setTimeSpec(Qt::UTC);
                gmiInfo.initTime = initTime;

                long validityDate = getGribLongKey(gribHandle, "time.validityDate");
                // LOG4CPLUS_DEBUG(mlog, "time.validityDate: " << validityDate);

                long validityTime = getGribLongKey(gribHandle, "time.validityTime");
                // LOG4CPLUS_DEBUG(mlog, "time.validityTime: " << validityTime);

                QString validTimeStr = QString("%1_%2").arg(validityDate)
                        .arg(validityTime, 4, 10, QChar('0'));
                QDateTime validTime = QDateTime::fromString(validTimeStr,
                                                           "yyyyMMdd_hhmm");
                validTime.setTimeSpec(Qt::UTC);
                gmiInfo.validTime = validTime;

                // Store filename and offset of grib message in index.
                MGribDatafieldInfo *info = nullptr;
                if (vinfo->timeMap[initTime][validTime].contains(ensMember))
                {
                    info = &(vinfo->timeMap[initTime][validTime][ensMember]);
                    if (info->filename != gribFileName)
                    {
                        LOG4CPLUS_ERROR(mlog, "found levels of the same "
                                        "3D data field in different files"
                                        "; skipping grib message");
                        grib_handle_delete(gribHandle);
                        continue;
                    }
                }
                else
                {
                    info = &(vinfo->timeMap[initTime][validTime][ensMember]);
                    info->filename = gribFileName;
                }

                // Get vertical level.
                long level = gmiInfo.level = getGribLongKey(
                            gribHandle, "vertical.level");
                if (fixLNSPmodelLevel)
                {
                    level = 0;
                }
                info->applyExp = fixLNSPmodelLevel;

                // LOG4CPLUS_DEBUG(mlog, "vertical.level: " << level);

                if (info->offsetForLevel.keys().contains(level))
                {
                    LOG4CPLUS_ERROR(mlog, "level " << level <<
                                    " of data field " << varName.toStdString()
                                    << "already exists; skipping grib message");
                    grib_handle_delete(gribHandle);
                    continue;
                }
                info->offsetForLevel[level] = gmiInfo.filePosition =
                        filePosition;

                // Insert level into list of vertical levels for this
                // variable.
                if (!vinfo->levels.contains(level)) vinfo->levels << level;

                // That's it!
                grib_handle_delete(gribHandle);
                messageCount++;

                // Write index struct to index file.
                gmiInfo.writeToDataStream(&indexDataStream);

                // Update current file position.
                filePosition = ftell(gribfile);
            } // for (grib messages)

            if (messageCount == 0)
            {
                LOG4CPLUS_DEBUG(mlog, "No grib messages found.");
            }
            else
            {
                LOG4CPLUS_DEBUG(mlog, "Indexed " << messageCount
                                << " grib messages.");
            }

            fclose(gribfile);
        } // if (create new index)

#ifdef ENABLE_MET3D_STOPWATCH
        stopwatch.split();
        LOG4CPLUS_DEBUG(mlog, "File scanned in "
                        << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                        << " seconds.\n" << flush);
#endif
    } // for (files)


    QString horizontalRefVarName = "";
    MVerticalLevelType referenceLevelType;
    // Perform checks, e.g. to make sure that for each data field all levels
    // are present.
    LOG4CPLUS_DEBUG(mlog, "Checking consistency of indexed data fields...");
    foreach (MVerticalLevelType levelType, availableDataFields.keys())
    {
        QStringList varNames = availableDataFields[levelType].keys();
        QString referenceVarName = "";
        QVector<double> referenceLevels;
        foreach (QString varName, varNames)
        {
            if (!checkIndexForVariable(availableDataFields[levelType][varName]))
            {
                delete availableDataFields[levelType].take(varName);
                continue;
            }
            else if ( !disableGridConsistencyCheck )
            {
                if (horizontalRefVarName.isEmpty())
                {
                    horizontalRefVarName = varName;
                    referenceLevelType = levelType;
                }
                if (referenceLevels.isEmpty())
                {
                    referenceVarName = varName;
                    referenceLevels =
                            availableDataFields[levelType][referenceVarName]
                            ->levels;
                }
                // Check consistency of vertical levels.
                else if (availableDataFields[levelType][varName]->levels
                         != referenceLevels)
                {
                    LOG4CPLUS_ERROR(mlog,
                                    "found difference in vertical levels to"
                                    " reference variable '"
                                    + referenceVarName.toStdString()
                                    + "'; discarding variable: '"
                                    + varName.toStdString() + "' [Dataset: "
                                    + getIdentifier().toStdString() + "]");
                    delete availableDataFields[levelType].take(varName);
                    continue;
                }
                // Check consistency of horizontal coordinates.
                else if (!checkConsistencyOfVariable(
                            availableDataFields[referenceLevelType]
                            [horizontalRefVarName],
                             availableDataFields[levelType][varName]))
                {
                    LOG4CPLUS_ERROR(mlog,
                                    "found difference to reference"
                                    " variable '"
                                    + referenceVarName.toStdString()
                                    + "'; discarding variable: '"
                                    + varName.toStdString() + "' [Dataset: "
                                    + getIdentifier().toStdString() + "]");
                    delete availableDataFields[levelType].take(varName);
                    continue;
                }
            }
        }
    }

    // debugPrintLevelTypeMap(availableDataFields);

#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "Directory scanned in "
                    << stopwatch.getElapsedTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif

    //    gribReader->readGrid(
    //                SURFACE_2D, "sp",
    //                QDateTime::fromString("2012-10-17T00:00:00Z", Qt::ISODate),
    //                QDateTime::fromString("2012-10-19T18:00:00Z", Qt::ISODate), 2
    //                )->dumpGridData();
    //    gribReader->readGrid(
    //                PRESSURE_LEVELS_3D, "t",
    //                QDateTime::fromString("2012-10-17T00:00:00Z", Qt::ISODate),
    //                QDateTime::fromString("2012-10-19T18:00:00Z", Qt::ISODate), 2
    //                )->dumpGridData();
    //    gribReader->readGrid(
    //                HYBRID_SIGMA_PRESSURE_3D, "t",
    //                QDateTime::fromString("2012-10-15T00:00:00Z", Qt::ISODate),
    //                QDateTime::fromString("2012-10-19T18:00:00Z", Qt::ISODate), 2
    //                )->dumpGridData();
}


bool MGribReader::isValidGribFile(QString path)
{
    // Try to open the file.
    FILE* gribfile = NULL;
    QByteArray ba = path.toLocal8Bit();
    gribfile = fopen(ba.data(), "r");
    if (!gribfile) return false;

    // Try to get grib handle to first message.
    grib_handle* gribHandle = NULL;
    int err = 0;
    gribHandle = grib_handle_new_from_file(0, gribfile, &err);
    if (gribHandle == NULL) return false;

    // Grib handle could be created; this is indeed a grib file.
    grib_handle_delete(gribHandle);
    fclose(gribfile);
    return true;
}


QStringList MGribReader::getGribIndexStringKeyList(
        grib_index* gribIndex, QString key)
{
    // Convert QString to c string for grip_api.
    QByteArray ba = key.toLocal8Bit();
    char* keyAsCString = ba.data();

    // Get number of index entries for this key.
    size_t gribKeySize;
    GRIB_CHECK(grib_index_get_size(gribIndex, keyAsCString, &gribKeySize), 0);

    // Get keys.
    char** keys = (char**) malloc(gribKeySize * 128 * sizeof(char*));
    GRIB_CHECK(grib_index_get_string(gribIndex, keyAsCString, keys, &gribKeySize), 0);
    QStringList keyList;
    for (size_t n = 0; n < gribKeySize; n++) keyList << keys[n];
    free(keys);

    return keyList;
}


QList<long> MGribReader::getGribIndexLongKeyList(
        grib_index* gribIndex, QString key)
{
    // Convert QString to c string for grip_api.
    QByteArray ba = key.toLocal8Bit();
    char* keyAsCString = ba.data();

    // Get number of index entries for this key.
    size_t gribKeySize;
    GRIB_CHECK(grib_index_get_size(gribIndex, keyAsCString, &gribKeySize), 0);

    // Get keys.
    long* keys = (long*) malloc(gribKeySize * sizeof(long));
    GRIB_CHECK(grib_index_get_long(gribIndex, keyAsCString, keys, &gribKeySize), 0);
    QList<long> keyList;
    for (size_t n = 0; n < gribKeySize; n++) keyList << keys[n];
    free(keys);

    return keyList;
}


QList<double> MGribReader::getGribIndexDoubleKeyList(
        grib_index* gribIndex, QString key)
{
    // Convert QString to c string for grip_api.
    QByteArray ba = key.toLocal8Bit();
    char* keyAsCString = ba.data();

    // Get number of index entries for this key.
    size_t gribKeySize;
    GRIB_CHECK(grib_index_get_size(gribIndex, keyAsCString, &gribKeySize), 0);

    // Get keys.
    double* keys = (double*) malloc(gribKeySize * sizeof(double));
    GRIB_CHECK(grib_index_get_double(gribIndex, keyAsCString, keys, &gribKeySize), 0);
    QList<double> keyList;
    for (size_t n = 0; n < gribKeySize; n++) keyList << keys[n];
    free(keys);

    return keyList;
}


QString MGribReader::getGribStringKey(grib_handle *gh, QString key)
{
    // Convert QString to c string for grip_api.
    QByteArray ba = key.toLocal8Bit();
    char* keyAsCString = ba.data();

    const int MAX_CHAR_LEN = 64;
    char cval[MAX_CHAR_LEN];
    size_t vlen = MAX_CHAR_LEN;
    GRIB_CHECK(grib_get_string(gh, keyAsCString, cval, &vlen), 0);

    return QString(cval);
}


long MGribReader::getGribLongKey(grib_handle *gh, QString key)
{
    // Convert QString to c string for grip_api.
    QByteArray ba = key.toLocal8Bit();
    char* keyAsCString = ba.data();

    long value;
    GRIB_CHECK(grib_get_long(gh, keyAsCString, &value), 0);

    return value;
}


double MGribReader::getGribDoubleKey(grib_handle *gh, QString key)
{
    // Convert QString to c string for grip_api.
    QByteArray ba = key.toLocal8Bit();
    char* keyAsCString = ba.data();

    double value;
    GRIB_CHECK(grib_get_double(gh, keyAsCString, &value), 0);

    return value;
}


void MGribReader::debugPrintLevelTypeMap(MGribLevelTypeMap &m)
{
    QString str;

    for (int ilevt = 0; ilevt < m.keys().size(); ilevt++)
    {
        MVerticalLevelType levt = m.keys()[ilevt];
        str += "\n>" + MStructuredGrid::verticalLevelTypeToString(levt) + "\n";

        for (int ivar = 0; ivar < m[levt].keys().size(); ivar++)
        {
            QString var = m[levt].keys()[ivar];
            str += "\t>" + var + "\n";

            MGribVariableInfo *vi = m[levt][var];
            str += "\t\tvariablename: " + vi->variablename + "\n";
            str += "\t\tlongname: " + vi->longname + "\n";
            str += "\t\tstandardname: " + vi->standardname + "\n";
            str += "\t\tunits: " + vi->units + "\n";
            str += "\t\tsurfacePressureName: " + vi->surfacePressureName + "\n";

            str += QString("\t\tnlons/nlats: %1/%2; ")
                    .arg(vi->nlons).arg(vi->nlats);
            str += QString("lon0/lat0: %1/%2; lon1/lat1: %3/%4; ")
                    .arg(vi->lon0).arg(vi->lat0).arg(vi->lon1).arg(vi->lat1);
            str += QString("dlon/dlat: %1/%2\n")
                    .arg(vi->dlon).arg(vi->dlat);

            str += QString("\t\tlevels -- %1\n")
                    .arg(keyListToString<double>(vi->levels.toList()));
            str += QString("\t\tlons -- %1\n")
                    .arg(keyListToString<double>(vi->lons.toList()));
            str += QString("\t\tlats -- %1\n")
                    .arg(keyListToString<double>(vi->lats.toList()));
            str += QString("\t\taki -- %1\n")
                    .arg(keyListToString<double>(vi->aki_hPa.toList()));
            str += QString("\t\tbki -- %1\n")
                    .arg(keyListToString<double>(vi->bki.toList()));
            str += QString("\t\tavailableMembers -- %1\n")
                    .arg(keyListToString<unsigned int>(vi->availableMembers.toList()));

            for (int iit = 0; iit < vi->timeMap.keys().size(); iit++)
            {
                QDateTime it = vi->timeMap.keys()[iit];
                str += "\t\t>" + it.toString(Qt::ISODate) + "\n";
                for (int ivt = 0; ivt < vi->timeMap[it].keys().size(); ivt++)
                {
                    QDateTime vt = vi->timeMap[it].keys()[ivt];
                    str += "\t\t\t>" + vt.toString(Qt::ISODate) + "\n";

                    MGribEnsembleMemberMap &em = vi->timeMap[it][vt];
                    for (int im = 0; im < em.keys().size(); im++)
                    {
                        int member = em.keys()[im];
                        str += QString("\t\t\t\t>%1\n").arg(member);
                        str += QString("\t\t\t\t\tfilename: %1\n")
                                .arg(em[member].filename);
                        str += QString("\t\t\t\t\tlevel offsets -- ");

                        for (int il = 0; il < em[member].offsetForLevel
                             .keys().size(); il++)
                        {
                            int level = em[member].offsetForLevel.keys()[il];
                            str += QString("%1:%2/")
                                    .arg(level)
                                    .arg(em[member].offsetForLevel[level]);
                        }
                        str += "\n";
                    }
                }
            }
        }
    }

    LOG4CPLUS_DEBUG(mlog, "==================> MGribLevelTypeMap:"
                    << str.toStdString());
}


bool MGribReader::checkIndexForVariable(MGribVariableInfo *vinfo)
{
    LOG4CPLUS_DEBUG(mlog, "Checking variable "
                    << vinfo->longname.toStdString() << "...");

    // Sort discovered levels.
    qSort(vinfo->levels);

    // If this is a hybrid sigmal pressure model levels field (i.e. if
    // ak/bk coefficients are available): Check if levels are continous
    // -- Met.3D cannot handle missing levels. We don't need all model
    // levels, though, missing levels at top and/or bottom of the
    // domain are ok.
    if ( !(vinfo->ak_hPa.empty()) )
    {
        bool levelsAreContinous = true;
        for (long ilev = 0; ilev < vinfo->levels.size()-1; ilev++)
            if (vinfo->levels[ilev] != vinfo->levels[ilev+1] - 1)
            {
                levelsAreContinous = false;
                break;
            }

        if (!levelsAreContinous)
        {
            LOG4CPLUS_ERROR(mlog, "Variable '"
                            << vinfo->longname.toStdString()
                            << "' has missing levels ... discarding variable.");
            return false;
        }

        // Get variable info of corresponding surface pressure.
        MGribVariableInfo *sPVInfo = availableDataFields
                .value(SURFACE_2D).value(vinfo->surfacePressureName);
        // Discard variable if surface pressure variable is missing.
        if ((sPVInfo) == nullptr)
        {
            LOG4CPLUS_ERROR(mlog, "Variable '"
                            << vinfo->longname.toStdString()
                            << "' has missing surface pressure field "
                            "... discarding variable.");
            return false;
        }

        // Defining error message to reduce code length.
        QString errorMessage0 = QString("Variable '" + vinfo->longname
                + "' has an inconsistency with its surface pressure variable '"
                + vinfo->surfacePressureName) + "'. Surface pressure field is "
                                                "missing ";
        std::string errorMessage1 =  " ... discarding variable.";

        // Check consistency of surface pressure variable and variable for each
        // member at each valid time step of the given variabel.
        foreach (QDateTime initTime, vinfo->timeMap.keys())
        {
            MGribValidTimeMap validTimeMap = vinfo->timeMap.value(initTime);
            MGribValidTimeMap spValidTimeMap =
                    sPVInfo->timeMap.value(initTime, MGribValidTimeMap());
            if (spValidTimeMap.empty())
            {
                LOG4CPLUS_ERROR(mlog, errorMessage0.toStdString()
                                << "at initTime "
                                << initTime.toString().toStdString()
                                << errorMessage1);
                return false;
            }
            foreach (QDateTime validTime, validTimeMap.keys())
            {
                MGribEnsembleMemberMap ensembleMemberMap =
                        validTimeMap.value(validTime);
                MGribEnsembleMemberMap spEnsembleMemberMap =
                        spValidTimeMap.value(validTime,
                                             MGribEnsembleMemberMap());
                if (spEnsembleMemberMap.empty())
                {
                    LOG4CPLUS_ERROR(mlog, errorMessage0.toStdString()
                                    << "at validTime "
                                    << validTime.toString().toStdString()
                                    << " of initTime "
                                    << initTime.toString().toStdString()
                                    << errorMessage1);
                    return false;
                }
                foreach (int member, ensembleMemberMap.keys())
                {
                    MGribDatafieldInfo datafieldInfo =
                            ensembleMemberMap.value(member);
                    MGribDatafieldInfo spDatafieldInfo =
                            spEnsembleMemberMap.value(member,
                                                      MGribDatafieldInfo());
                    if (spDatafieldInfo.filename == "")
                    {
                        LOG4CPLUS_ERROR(mlog, errorMessage0.toStdString()
                                        << "for member " << member
                                        << " at validTime "
                                        << validTime.toString().toStdString()
                                        << " of initTime "
                                        << initTime.toString().toStdString()
                                        << errorMessage1);
                        return false;
                    }
                    if ( !spDatafieldInfo.offsetForLevel.keys().contains(0))
                    {
                        LOG4CPLUS_ERROR(mlog, errorMessage0.toStdString()
                                        << "at level " << 0
                                        << " for member " << member
                                        << " at validTime "
                                        << validTime.toString().toStdString()
                                        << " of initTime "
                                        << initTime.toString().toStdString()
                                        << errorMessage1);
                        return false;
                    }

                    for (long level = 0; level < vinfo->levels.size() - 1;
                         level++)
                    {
                        if ( !datafieldInfo.offsetForLevel.keys().contains(
                                 vinfo->levels[level]))
                        {
                            LOG4CPLUS_ERROR(mlog, "Variable '"
                                            << vinfo->longname.toStdString()
                                            << "' has an inconsistency for level "
                                            << vinfo->levels[level]
                                            << " ... discarding variable.");
                            return false;
                        }
                    }

                }
            }
        }
    }

    else
    {
        foreach (MGribValidTimeMap validTimeMap, vinfo->timeMap)
        {
            foreach (MGribEnsembleMemberMap ensembleMemberMap, validTimeMap)
            {
                foreach (MGribDatafieldInfo datafieldInfo, ensembleMemberMap)
                {
                    for (long level = 0; level < vinfo->levels.size() - 1;
                         level++)
                    {
                        if ( !datafieldInfo.offsetForLevel.keys().contains(
                                 vinfo->levels[level]) )
                        {
                            LOG4CPLUS_ERROR(mlog, "Variable '"
                                            << vinfo->longname.toStdString()
                                            << "' has an inconsistency for level "
                                            << vinfo->levels[level]
                                            << " ... discarding variable.");
                            return false;
                        }
                    }

                }
            }
        }
    }


    // Everything is ok.
    LOG4CPLUS_DEBUG(mlog, "... variable '"
                    << vinfo->longname.toStdString() << "' is ok.");
    return true;
}


QString MGribReader::forecastTypeToString(MECMWFForecastType type)
{
    switch (type)
    {
    case ANALYSIS:
        return QString("ANA");
    case DETERMINISTIC_FORECAST:
        return QString("DET");
    case ENSEMBLE_FORECAST:
        return QString("ENS");
    default:
        return QString("---");
    }
}


template<typename T> QString MGribReader::keyListToString(QList<T> keyList)
{
    QString str = QString("%1 item(s): ").arg(keyList.size());
    for (int n = 0; n < keyList.size(); n++)
        str += QString("%1/").arg(keyList[n]);
    return str;
}


void MGribReader::detectSurfacePressureFieldType(QStringList *availableFiles)
{
    if (surfacePressureFieldType == "")
    {
#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
#endif

        // Scan all grib files contained in the directory and search for
        // a surface pressure field. (Only use grib files directly
        // since there is no speed up when using index files.)
        foreach (QString gribFileName, *availableFiles)
        {
            // (Skip index files.)
            if (gribFileName.endsWith("met3d_grib_index")) continue;
            QString filePath = dataRoot.filePath(gribFileName);
            // Open the file.
            FILE* gribfile = NULL;
            QByteArray ba = filePath.toLocal8Bit();
            gribfile = fopen(ba.data(), "r");
            if (!gribfile)
            {
                continue;
            }
            // Loop over grib messages contained in the file.
            grib_handle* gribHandle = NULL;
            int err = 0;
            while ((gribHandle =
                    grib_handle_new_from_file(0, gribfile, &err)) != NULL)
            {
                QString shortName = getGribStringKey(gribHandle,
                                                     "parameter.shortName");
                if (shortName == "lnsp")
                {
                    setSurfacePressureFieldType("lnsp");
                    break;
                }
                else if (shortName == "sp")
                {
                    setSurfacePressureFieldType("sp");
                    break;
                }
                grib_handle_delete(gribHandle);
            } // while loop grib handles.
            if (surfacePressureFieldType != "")
            {
                break;
            }
            fclose(gribfile);
        } // foreach loop grib files

        // No surface pressure field found.
        if (surfacePressureFieldType == "")
        {
            LOG4CPLUS_DEBUG(mlog, "Could not find surface pressure field."
                            << flush);
            // Avoid another search for the surface pressure field type by
            // setting surfacePressureFieldType to "none".
            surfacePressureFieldType = "none";
        }

#ifdef ENABLE_MET3D_STOPWATCH
        stopwatch.split();
        LOG4CPLUS_DEBUG(mlog, "Surface pressure field type detected in "
                        << stopwatch.getElapsedTime(MStopwatch::SECONDS)
                        << " seconds.\n" << flush);
#endif
    } // if Surface pressure field type empty.
}


void MGribReader::setSurfacePressureFieldType(QString surfacePressureFieldType)
{
    // Set surface pressure field type once only.
    if (this->surfacePressureFieldType == "")
    {
        this->surfacePressureFieldType = surfacePressureFieldType;
        LOG4CPLUS_DEBUG(mlog, "Surface pressure field type detected as '"
                        << surfacePressureFieldType.toStdString() << "'"
                        << flush);
    }
}


bool MGribReader::checkConsistencyOfVariable(MGribVariableInfo *referenceVInfo,
                                             MGribVariableInfo *currentVInfo)
{
    // Get geographical region of the data field & check that
    // all messages of this 3D field have the same bounds.
    if ( referenceVInfo->nlons != currentVInfo->nlons )
    {
        LOG4CPLUS_ERROR(mlog, "detected inconsistency in 'number of longitudes'");
        return false;
    }
    if ( referenceVInfo->nlats != currentVInfo->nlats )
    {
        LOG4CPLUS_ERROR(mlog, "detected inconsistency in 'number of longitudes'");
        return false;
    }
    if ( MMOD(referenceVInfo->lon0, 360.) != MMOD(currentVInfo->lon0, 360.) )
    {
        LOG4CPLUS_ERROR(mlog,
                        "detected inconsistency in 'longitude of first grid"
                        " point'");
        return false;
    }
    if ( referenceVInfo->lat0 != currentVInfo->lat0 )
    {
        LOG4CPLUS_ERROR(mlog,
                        "detected inconsistency in 'latitude of first grid"
                        " point'");
        return false;
    }
    if ( MMOD(referenceVInfo->lon1, 360.) != MMOD(currentVInfo->lon1, 360.) )
    {
        LOG4CPLUS_ERROR(mlog,
                        "detected inconsistency in 'longitude of last grid"
                        " point'");
        return false;
    }
    if ( referenceVInfo->lat1 != currentVInfo->lat1 )
    {
        LOG4CPLUS_ERROR(mlog,
                        "detected inconsistency in 'latitude of last grid"
                        " point'");
        return false;
    }
    if ( referenceVInfo->dlon != currentVInfo->dlon )
    {
        LOG4CPLUS_ERROR(mlog, "detected inconsistency in 'i direction increment'");
        return false;
    }
    if ( referenceVInfo->dlat != currentVInfo->dlat )
    {
        LOG4CPLUS_ERROR(mlog, "detected inconsistency in 'j direction increment'");
        return false;
    }
    return true;
}


/******************************************************************************
***                        MGribMessageIndexInfo                            ***
*******************************************************************************/

void MGribMessageIndexInfo::writeToDataStream(QDataStream *dataStream)
{
    (*dataStream) << (quint32)levelType;
    (*dataStream) << variablename;
    (*dataStream) << longname;
    (*dataStream) << standardname;
    (*dataStream) << units;
    (*dataStream) << (quint32)fcType;
    (*dataStream) << surfacePressureName;
    (*dataStream) << nlons;
    (*dataStream) << nlats;
    (*dataStream) << lon0;
    (*dataStream) << lat0;
    (*dataStream) << lon1;
    (*dataStream) << lat1;
    (*dataStream) << dlon;
    (*dataStream) << dlat;
    (*dataStream) << lats;
    (*dataStream) << lons;
    (*dataStream) << aki_hPa;
    (*dataStream) << bki;
    (*dataStream) << ak_hPa;
    (*dataStream) << bk;
    (*dataStream) << ensMember;
    (*dataStream) << initTime;
    (*dataStream) << validTime;
    (*dataStream) << level;
    (*dataStream) << filePosition;

//    QString msg = QString("Wrote %1/IT%2/VT%3/MEM%4/LEV%5")
//            .arg(variablename)
//            .arg(initTime.toString(Qt::ISODate))
//            .arg(validTime.toString(Qt::ISODate))
//            .arg(ensMember)
//            .arg(level);
//    LOG4CPLUS_DEBUG(mlog, msg.toStdString());
}


void MGribMessageIndexInfo::readFromDataStream(QDataStream *dataStream)
{
    lats.clear();
    lons.clear();
    aki_hPa.clear();
    bki.clear();
    ak_hPa.clear();
    bk.clear();

    // Be careful that the variables appear in the same order as in
    // writeToDataStream()!
    quint32 lT; (*dataStream) >> lT; levelType = MVerticalLevelType(lT);
    (*dataStream) >> variablename;
    (*dataStream) >> longname;
    (*dataStream) >> standardname;
    (*dataStream) >> units;
    quint32 fcT; (*dataStream) >> fcT; fcType = MECMWFForecastType(fcT);
    (*dataStream) >> surfacePressureName;
    (*dataStream) >> nlons;
    (*dataStream) >> nlats;
    (*dataStream) >> lon0;
    (*dataStream) >> lat0;
    (*dataStream) >> lon1;
    (*dataStream) >> lat1;
    (*dataStream) >> dlon;
    (*dataStream) >> dlat;
    (*dataStream) >> lats;
    (*dataStream) >> lons;
    (*dataStream) >> aki_hPa;
    (*dataStream) >> bki;
    (*dataStream) >> ak_hPa;
    (*dataStream) >> bk;
    (*dataStream) >> ensMember;
    (*dataStream) >> initTime;
    (*dataStream) >> validTime;
    (*dataStream) >> level;
    (*dataStream) >> filePosition;

//    QString msg = QString("Read %1/IT%2/VT%3/MEM%4/LEV%5")
//            .arg(variablename)
//            .arg(initTime.toString(Qt::ISODate))
//            .arg(validTime.toString(Qt::ISODate))
//            .arg(ensMember)
//            .arg(level);
//    LOG4CPLUS_DEBUG(mlog, msg.toStdString());
}


} // namespace Met3D
