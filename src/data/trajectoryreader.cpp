/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2020      Marcel Meyer [*]
**
**  + Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  * Regional Computing Center, Visualization
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
#include "trajectoryreader.h"

// standard library imports
#include <iostream>
#include <limits>

// related third party imports
#include <QThread>
#include <QString>
#include <qtconcurrentrun.h>
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


namespace Met3D
{


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryReader::MTrajectoryReader(QString identifier)
    : MTrajectoryDataSource(),
      MAbstractDataReader(identifier)
{
}


MTrajectoryReader::~MTrajectoryReader()
{
    QMutexLocker openFilesLocker(&openFilesMutex);
    foreach (MTrajectoryFileInfo* finfo, openFiles) delete finfo;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QList<QDateTime> MTrajectoryReader::availableInitTimes()
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    return availableTrajectories.keys();
}


QList<QDateTime> MTrajectoryReader::availableStartTimes(
        const QDateTime& initTime)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableTrajectories.keys().contains(initTime))
        throw MBadDataFieldRequest(
                "unkown init time requested: " +
                initTime.toString(Qt::ISODate).toStdString(),
                __FILE__, __LINE__);
    return availableTrajectories.value(initTime).keys();
}


QList<QDateTime> MTrajectoryReader::availableValidTimes(
        const QDateTime& initTime)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableTrajectories.keys().contains(initTime))
        throw MBadDataFieldRequest(
                "unkown init time requested: " +
                initTime.toString(Qt::ISODate).toStdString(),
                __FILE__, __LINE__);
    //return availableTrajectories.value(initTime).keys();

    const QList<QDateTime>& availableTrajectoriesStartTimes =
            availableTrajectories.value(initTime).keys();

    if (availableTrajectoriesStartTimes.empty() || availableTrajectoriesStartTimes.size() > 1)
    {
        return availableTrajectoriesStartTimes;
    }
    else
    {
        QDateTime validTime = availableTrajectoriesStartTimes.first();

        QReadLocker availableItemsReadLocker(&availableItemsLock);
        if (!availableTrajectories.keys().contains(initTime))
            throw MBadDataFieldRequest(
                    "unkown init time requested: " +
                    initTime.toString(Qt::ISODate).toStdString(),
                    __FILE__, __LINE__);
        else if (!availableTrajectories.value(initTime).keys().contains(validTime))
            throw MBadDataFieldRequest(
                    "unkown start time requested: " +
                    validTime.toString(Qt::ISODate).toStdString(),
                    __FILE__, __LINE__);
        QString filename = availableTrajectories.value(initTime).value(validTime).filename;
        availableItemsReadLocker.unlock();

        checkFileOpen(filename);
        openFilesMutex.lock();
        MTrajectoryFileInfo* finfo = openFiles.value(filename);
        openFilesMutex.unlock();

        return finfo->times.toList();
    }
}


QList<QDateTime> MTrajectoryReader::validTimeOverlap(
        const QDateTime &initTime, const QDateTime &validTime)
{
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableTrajectories.keys().contains(initTime))
        throw MBadDataFieldRequest(
                "unkown init time requested: " +
                initTime.toString(Qt::ISODate).toStdString(),
                __FILE__, __LINE__);
    if (!availableTrajectories.value(initTime).keys().contains(validTime))
        throw MBadDataFieldRequest(
                "unkown valid time requested: " +
                validTime.toString(Qt::ISODate).toStdString(),
                __FILE__, __LINE__);

    return availableTrajectories.value(initTime).value(validTime)
            .validTimeOverlap;
}


QSet<unsigned int> MTrajectoryReader::availableEnsembleMembers()
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    return availableMembers;
}


QStringList MTrajectoryReader::availableAuxiliaryVariables()
{
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    return availableAuxDataVariables.toList();
}


MTrajectories* MTrajectoryReader::produceData(MDataRequest request)
{
#ifdef MSTOPWATCH_ENABLED
    MStopwatch stopwatch;
#endif

    MDataRequestHelper rh(request);

    QDateTime initTime  = rh.timeValue("INIT_TIME");
    QDateTime validTime = rh.timeValue("VALID_TIME");
    unsigned int member = rh.intValue("MEMBER");
    QString timeSpan    = rh.value("TIME_SPAN");

    // If only a specified time interval of the trajectories should be read
    // (timeSpan != ALL), timeSpan contains either a single time or a time
    // interval separated by a "/".
    QDateTime startTime, stopTime;
    if (timeSpan != "ALL")
    {
        QStringList args = timeSpan.split("/");
        startTime = QDateTime::fromString(args[0], Qt::ISODate);
        if (args.size() > 1)
            stopTime = QDateTime::fromString(args[1], Qt::ISODate);
        else
            stopTime = startTime;
    }

    LOG4CPLUS_DEBUG(mlog, "Reading trajectories for IT="
                    << initTime.toString(Qt::ISODate).toStdString() << ", VT="
                    << validTime.toString(Qt::ISODate).toStdString() << ", MEM="
                    << member << ", INTERVAL=("
                    << startTime.toString(Qt::ISODate).toStdString() << "/"
                    << stopTime.toString(Qt::ISODate).toStdString() << ")");

    // Check validity of initTime and startTime.
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableTrajectories.keys().contains(initTime))
        throw MBadDataFieldRequest(
                "unkown init time requested: " +
                initTime.toString(Qt::ISODate).toStdString(),
                __FILE__, __LINE__);
    else if (!availableTrajectories.value(initTime).keys().contains(validTime))
        throw MBadDataFieldRequest(
                "unkown start time requested: " +
                validTime.toString(Qt::ISODate).toStdString(),
                __FILE__, __LINE__);

    // Get the name of the file that stores the requested init and start times
    // and open the NetCDF file..
    QString filename = availableTrajectories.value(initTime).value(validTime)
            .filename;
    availableItemsReadLocker.unlock();

    checkFileOpen(filename);
    openFilesMutex.lock();
    MTrajectoryFileInfo* finfo = openFiles.value(filename);
    openFilesMutex.unlock();

    // Lock access (until method return) to "filename" and associated data.
    QMutexLocker accessMutexLocker( &(finfo->accessMutex) );

    unsigned int numTimeSteps    = finfo->numTimeSteps;
    unsigned int numTrajectories = finfo->numTrajectories;
    unsigned int numOutputParameters = finfo->numOutputParameters;

    // Check if the requested member exists.
    if (member > finfo->numEnsembleMembers - 1)
        throw MBadDataFieldRequest(
                "invalid ensemble member requested",
                __FILE__, __LINE__);

    // Correct the number of timesteps if only a part of the trajectory is read.
    unsigned int startIndex = 0;
    QVector<QDateTime> times;
    if (timeSpan != "ALL")
    {
        startIndex = finfo->times.indexOf(startTime);
        unsigned int stopIndex = finfo->times.indexOf(stopTime);
        numTimeSteps = stopIndex - startIndex + 1;
        times = finfo->times.mid(startIndex, numTimeSteps);
    }
    else
    {
        times = finfo->times;
    }

    // Create temporary memory space for data (we can't read directly into the
    // QVectors that are used in MTrajectories).
    int numVertices = numTimeSteps * numTrajectories;
    float *lons = new float[numVertices];
    float *lats = new float[numVertices];
    float *pres = new float[numVertices];
    float *auxData = new float[numVertices];
    float *sensData = new float[numVertices * numOutputParameters];
    uint32_t *outputParameters = new uint32_t[numOutputParameters];
    vector<size_t> startParams = {0};
    vector<size_t> countParams = {numOutputParameters};
    if (!finfo->outputParameterVar.isNull())
    {
        finfo->outputParameterVar.getVar(startParams, countParams, outputParameters);
    }
    else
    {
        std::fill_n(outputParameters, numOutputParameters, 0);
    }

    // Read data from file.
    vector<size_t> start = {member, 0, startIndex};
    vector<size_t> count = {1, numTrajectories, numTimeSteps};
    vector<size_t> startSens = {0, member, 0, startIndex};
    vector<size_t> countSens = {numOutputParameters, 1, numTrajectories, numTimeSteps};

    QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);
    finfo->lonVar.getVar(start, count, lons);
    finfo->latVar.getVar(start, count, lats);
    finfo->prsVar.getVar(start, count, pres);

    // Trajectory pressure coordinate needs to be in hPa; hence scale if
    // given in Pa.
    if (finfo->prsVarUnits == "Pa")
    {
        for (int i = 0; i < numVertices; i++) pres[i] /= 100.;
    }

    // Replace missing values other than "M_INVALID_TRAJECTORY_POS" by
    // "M_INVALID_TRAJECTORY_POS".
    if (finfo->lonMissingValue != M_INVALID_TRAJECTORY_POS)
    {
        for (int i = 0; i < numVertices; i++)
        {
            if (isnan(lons[i]) || floatIsAlmostEqualRelativeAndAbs(
                        lons[i], finfo->lonMissingValue, 1.E-6f))
            {
                lons[i] = M_INVALID_TRAJECTORY_POS;
            }
        }
    }
    if (finfo->latMissingValue != M_INVALID_TRAJECTORY_POS)
    {
        for (int i = 0; i < numVertices; i++)
        {
            if (isnan(lats[i]) || floatIsAlmostEqualRelativeAndAbs(
                        lats[i], finfo->latMissingValue, 1.E-6f))
            {
                lats[i] = M_INVALID_TRAJECTORY_POS;
            }
        }
    }
    if (finfo->prsMissingValue != M_INVALID_TRAJECTORY_POS)
    {
        for (int i = 0; i < numVertices; i++)
        {
            if (isnan(pres[i]) || floatIsAlmostEqualRelativeAndAbs(
                        pres[i], finfo->prsMissingValue, 1.E-6f))
            {
                pres[i] = M_INVALID_TRAJECTORY_POS;
            }
        }
    }


    // Create the trajectory data struct that is returned from this function.
    Met3D::MTrajectories* trajectories =
            new Met3D::MTrajectories(numTrajectories, times);

    trajectories->setMetaData(
                initTime, validTime, "PRECOMPUTED_trajectories", member);

    // Copy temporary data with vertex positions into the return data struct.
    trajectories->copyVertexDataFrom(lons, lats, pres);

    // Read all auxiliary data variables from file and copy to the return
    // data struct.
    for (int iIndexAuxData = 0; iIndexAuxData < finfo->auxDataVars.size();
         iIndexAuxData++)
    {
        // Note: Here we assume that variable is of type float or double,
        // as "auxData" is of type float.

        // Read auxiliary data into temporary data array.
        finfo->auxDataVars[iIndexAuxData].getVar(start, count, auxData);

        // Copy this auxiliary data, iAuxDataVar, to the aux. data struct
        // in the trajectories class.
        trajectories->copyAuxDataPerVertex(auxData, iIndexAuxData);
     }

    // Same for sensitivity variables.
    for (int iIndexSensData = 0; iIndexSensData < finfo->sensDataVars.size();
         iIndexSensData++)
    {
        if (numOutputParameters == 1)
        {
            finfo->sensDataVars[iIndexSensData].getVar(start, count, sensData);
        } else
        {
            finfo->sensDataVars[iIndexSensData].getVar(startSens, countSens, sensData);
        }
        trajectories->copySensDataPerVertex(sensData, iIndexSensData, numOutputParameters);
    }
    trajectories->copyOutputParameter(outputParameters, numOutputParameters);

    ncAccessMutexLocker.unlock();

    // Copy the names of auxiliary data variables.
    trajectories->setAuxDataVariableNames(finfo->auxDataVarNames);
    trajectories->setSensDataVariableNames(finfo->sensDataVarNames);

    // Copy start grid geometry, if available.
    trajectories->setStartGrid(finfo->startGrid);

    // Debug output.
    // trajectories->dumpStartVerticesToLog(300);

    // Delete temporary data fields.
    delete[] lons;
    delete[] lats;
    delete[] pres;
    delete[] auxData;
    delete[] sensData;
    delete[] outputParameters;

#ifdef MSTOPWATCH_ENABLED
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "single member trajectory read in "
                    << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif

    return trajectories;
}


MTask* MTrajectoryReader::createTaskGraph(MDataRequest request)
{
    // No dependencies, so we create a plain task.
    MTask* task = new MTask(request, this);
    // However, this task accesses the hard drive.
    task->setDiskReaderTask();
    return task;
}


MFloatPerTrajectorySupplement* MTrajectoryReader::readFloatPerTrajectorySupplement(
        MDataRequest trajectoryRequest, MDataRequest supplementRequest)
{
    // 1) Indentify trajectory file from which supplement should be read.
    // ==================================================================

    MDataRequestHelper rh(trajectoryRequest);

    QDateTime initTime  = rh.timeValue("INIT_TIME");
    QDateTime validTime = rh.timeValue("VALID_TIME");
    unsigned int member = rh.intValue("MEMBER");

    LOG4CPLUS_DEBUG(mlog, "Reading float-pre-trajectory supplement for IT="
                    << initTime.toString(Qt::ISODate).toStdString() << ", VT="
                    << validTime.toString(Qt::ISODate).toStdString() << ", MEM="
                    << member);

    // Check validity of initTime and validTime.
    // cf.  MClimateForecastReader::availableVariables() .
    QReadLocker availableItemsReadLocker(&availableItemsLock);
    if (!availableTrajectories.keys().contains(initTime))
        throw MBadDataFieldRequest(
                "unkown init time requested: " +
                initTime.toString(Qt::ISODate).toStdString(),
                __FILE__, __LINE__);
    else if (!availableTrajectories.value(initTime).keys().contains(validTime))
        throw MBadDataFieldRequest(
                "unkown start time requested: " +
                validTime.toString(Qt::ISODate).toStdString(),
                __FILE__, __LINE__);

    // Get the name of the file that stores the requested init and start times
    // and open the NetCDF file..
    QString filename = availableTrajectories.value(initTime).value(validTime)
            .filename;
    availableItemsReadLocker.unlock();

    checkFileOpen(filename);
    openFilesMutex.lock();
    MTrajectoryFileInfo* finfo = openFiles.value(filename);
    openFilesMutex.unlock();

    // Lock access (until method return) to "filename" and associated data.
    QMutexLocker accessMutexLocker( &(finfo->accessMutex) );

    // 2) Read supplement.
    // ===================

    MDataRequestHelper srh(supplementRequest);
    MFloatPerTrajectorySupplement *supplement = nullptr;

    if (srh.contains("MAX_DELTA_PRESSURE_HOURS"))
    {
        // NetCDF library is not thread-safe (at least the regular C/C++
        // interface is not; hence all NetCDF calls need to be serialized
        // globally in Met.3D! (notes Feb2015).
        QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);

        // Find the index of the requested time interval.
        int timeInterval_hrs = srh.intValue("MAX_DELTA_PRESSURE_HOURS");

        NcDim tintDim = finfo->ncFile->getDim("time_interval");
        if (tintDim.isNull())
        {
            LOG4CPLUS_ERROR(mlog, "ERROR: cannot find precomputed max. delta "
                            "pressure data.");
            return nullptr;
        }
        unsigned int numTimeIntervals = tintDim.getSize();

        NcVar tintVar = finfo->ncFile->getVar("time_interval");
        if (tintVar.isNull())
        {
            LOG4CPLUS_ERROR(mlog, "ERROR: cannot find precomputed max. delta "
                            "pressure data.");
            return nullptr;
        }

        QVector<float> timeIntervals(numTimeIntervals);
        tintVar.getVar(timeIntervals.data());

        int tintIndex = timeIntervals.indexOf(timeInterval_hrs);
        if (tintIndex == -1)
        {
            LOG4CPLUS_ERROR_FMT(mlog, "ERROR: cannot find requested time "
                                "interval %i for max. delta pressure.",
                                timeInterval_hrs);
        }
        else
        {
            // Read delta pressure data.
            NcVar dpVar = finfo->ncFile->getVar(
                        "delta_pressure_per_time_interval");
            if (dpVar.isNull())
            {
                LOG4CPLUS_ERROR(mlog, "ERROR: cannot find precomputed max. "
                                "delta pressure data.");
                return nullptr;
            }

            supplement = new MFloatPerTrajectorySupplement(
                        trajectoryRequest, finfo->numTrajectories);

            vector<size_t> start = {member, 0, size_t(tintIndex)};
            vector<size_t> count = {1, finfo->numTrajectories, 1};
            dpVar.getVar(start, count, supplement->values.data());
        }
    }

    return supplement;
}



/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTrajectoryReader::scanDataRoot()
{
    // dataRoot has been set in MAbstractDataReader::setDataRoot().

    // Lock access to all availableXX data fields.
    QWriteLocker availableItemsWriteLocker(&availableItemsLock);

    LOG4CPLUS_INFO_FMT(mlog, "Scanning directory %s for files with trajectory "
                       "forecast data.",
                       dataRoot.absolutePath().toStdString().c_str());

    // Get a list of all files in the directory that match the wildcard name
    // filter in "dirFileFilters".
    QStringList availableFiles;
    getAvailableFilesFromFilters(availableFiles);

    // Create and initialise progress bar.
    initializeFileScanProgressDialog(availableFiles.size());

    // For each file, extract information about the contained start time and
    // valid times. Store these information in "availableTrajectories".
    for (int i = 0; i < availableFiles.size(); i++)
    {
        updateFileScanProgressDialog();
        QString filename = availableFiles[i];

        LOG4CPLUS_DEBUG_FMT(mlog, "\tParsing file %s ..",
                            filename.toStdString().c_str());

        // NetCDF library is not thread-safe (at least the regular C/C++
        // interface is not; hence all NetCDF calls need to be serialized
        // globally in Met.3D! (notes Feb2015).
        QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);

        // Open the file.
        NcFile *ncFile;
        try
        {
            ncFile = new NcFile(dataRoot.filePath(filename).toStdString(),
                                NcFile::read);
        }
        catch (NcException& e)
        {
            LOG4CPLUS_ERROR_FMT(mlog, "Cannot open file \"%s\"..",
                                filename.toStdString().c_str());
            continue;
        }

        // Get start time and forecast init time of this file. Assume that
        // there is a variable "pressure" from which the time variable can
        // be found.
        NcCFVar currCFVar(ncFile->getVar("pressure"));
        QDateTime startTime = currCFVar.getBaseTime();
        QDateTime initTime  = NcCFVar(currCFVar.getTimeVar())
                .getTimeFromAttribute(QString("forecast_inittime"));

        LOG4CPLUS_TRACE_FMT(mlog, "\tstart time: %s, init time: %s",
                            startTime.toString(Qt::ISODate).toStdString().c_str(),
                            initTime.toString(Qt::ISODate).toStdString().c_str());

        // Store the time values with the current filename in
        // "availableTrajectories".
        availableTrajectories[initTime][startTime].filename = filename;
        availableTrajectories[initTime][startTime].isStartTime = true;

        // Add this valid (=start) time to all other valid times it overlaps
        // with. This might introduce wrong valid times -- they are removed
        // below.
        QList<QDateTime> trajectoryTimes = currCFVar.getTimeValues();
        for (int t = 0; t < trajectoryTimes.size(); t++)
        {
            availableTrajectories[initTime][trajectoryTimes[t]]
                    .validTimeOverlap.append(startTime);
        }

        // Determine the available ensemble members.
        NcDim        ensembleDim = ncFile->getDim("ensemble");
        unsigned int numMembers  = ensembleDim.getSize();
        for (unsigned int m = 0; m < numMembers; m++)
        {
            availableMembers.insert(m);
        }

        // Determine available auxiliary data variables.
        // Get auxiliary data along trajectories by screening
        // all available ncvars in the input file and picking
        // those vars indicated as "aux. data" by the
        // nc var attribute "auxiliary_data".
        multimap<string, NcVar> ncVariables = ncFile->getVars();
        for (multimap<string, NcVar>::iterator iVar = ncVariables.begin();
             iVar != ncVariables.end(); iVar++)
        {
            QString varName = QString::fromStdString(iVar->first);
            NcCFVar iCFVar(ncFile->getVar(varName.toStdString()));

            string auxDataIndicator = "";
            try { iCFVar.getAtt("auxiliary_data").getValues(auxDataIndicator); }
            catch (NcException) {}

            if (auxDataIndicator == "yes")
            {
                availableAuxDataVariables.insert(varName);
            }
        }

        delete ncFile;
    } // for (files)

    deleteFileScanProgressDialog();

    // After all files have been scanned remove wrong valid times (see above).
    // (Do not use availableInitTimes() and availableValidTimes() as they
    // would lock for read and produce a deadlock.)
    QList<QDateTime> initTimes = availableTrajectories.keys();
    for (int i = 0; i < initTimes.size(); i++)
    {
        QList<QDateTime> validTimes = availableTrajectories[initTimes[i]].keys();
        for (int j = 0; j < validTimes.size(); j++)
            if (!availableTrajectories[initTimes[i]][validTimes[j]].isStartTime)
                availableTrajectories[initTimes[i]].remove(validTimes[j]);
    }
}


const QStringList MTrajectoryReader::locallyRequiredKeys()
{
    return (QStringList() << "INIT_TIME" << "VALID_TIME" << "MEMBER"
            << "TIME_SPAN");
}


void MTrajectoryReader::checkFileOpen(QString filename)
{
    openFilesMutex.lock();
    // Check if a new MTrajectoryFileInfo object needs to be inserted into
    // the dictionary.
    if ( !openFiles.contains(filename) )
        openFiles.insert(filename, new MTrajectoryFileInfo());
    MTrajectoryFileInfo* finfo = openFiles.value(filename);
    openFilesMutex.unlock();

    // Lock access to "filename" and associated data.
    QMutexLocker accessMutexLocker( &(finfo->accessMutex) );

    // Is this file opened for the first time?
    bool initialFileAccess = (finfo->ncFile == nullptr);

    if (initialFileAccess)
    {
        // The file is accessed for the first time -- open.
        LOG4CPLUS_DEBUG(mlog, "Initial file access for trajectory file "
                        << filename.toStdString() << "; opening file.");

        // NetCDF library is not thread-safe (at least the regular C/C++
        // interface is not; hence all NetCDF calls need to be serialized
        // globally in Met.3D! (notes Feb2015).
        QMutexLocker ncAccessMutexLocker(&staticNetCDFAccessMutex);

        try
        {
            finfo->ncFile = new NcFile(
                        dataRoot.filePath(filename).toStdString(), NcFile::read);
        }
        catch (NcException& e)
        {
            LOG4CPLUS_ERROR(mlog, "cannot open file " << filename.toStdString());
            throw;
        }

        NcFile *ncFile = finfo->ncFile;

        // Query dimension sizes.
        NcDim timeDim       = ncFile->getDim("time");
        NcDim trajectoryDim = ncFile->getDim("trajectory");
        NcDim ensembleDim   = ncFile->getDim("ensemble");
        int numDims = ncFile->getDimCount();
        if (numDims == 4) {
            NcDim outputParameterDim = ncFile->getDim("Output_Parameter_ID");
            finfo->numOutputParameters = outputParameterDim.getSize();

        }
        else
        {
            finfo->numOutputParameters  = 1;
        }
        finfo->outputParameterVar = ncFile->getVar("Output_Parameter_ID");

        finfo->numTimeSteps       = timeDim.getSize();
        finfo->numTrajectories    = trajectoryDim.getSize();
        finfo->numEnsembleMembers = ensembleDim.getSize();

        // Get coordinate data variables.
        finfo->lonVar = ncFile->getVar("lon");
        finfo->latVar = ncFile->getVar("lat");
        finfo->prsVar = ncFile->getVar("pressure");

        // Get units of pressure variable.
        string units = "";
        try
        {
            finfo->prsVar.getAtt("units").getValues(units);
        }
        catch (NcException)
        {
            LOG4CPLUS_WARN(mlog, "WARNING: cannot determine units of trajectory "
                           "pressure variable. Assuming 'hPa'.");
            units = "hPa";
        }
        finfo->prsVarUnits = units.c_str();

        // Check if missing values are provided.
//TODO (mr, 02Nov2020) -- eliminate the code repition below...
        try
        {
            finfo->lonVar.getAtt("_FillValue").getValues(
                        &(finfo->lonMissingValue));
        }
        catch (NcException)
        {
            try
            {
                finfo->lonVar.getAtt("missing_value").getValues(
                            &(finfo->lonMissingValue));
            }
            catch (NcException) {}
        }
        if (finfo->lonMissingValue != M_INVALID_TRAJECTORY_POS)
        {
            LOG4CPLUS_DEBUG(mlog, "Trajectories: Missing value has been "
                                  "provided for 'lon': "
                            << finfo->lonMissingValue << ".");
        }

        try
        {
            finfo->latVar.getAtt("_FillValue").getValues(
                        &(finfo->latMissingValue));
        }
        catch (NcException)
        {
            try
            {
                finfo->latVar.getAtt("missing_value").getValues(
                            &(finfo->latMissingValue));
            }
            catch (NcException) {}
        }
        if (finfo->latMissingValue != M_INVALID_TRAJECTORY_POS)
        {
            LOG4CPLUS_DEBUG(mlog, "Trajectories: Missing value has been "
                                  "provided for 'lat': "
                            << finfo->latMissingValue << ".");
        }

        try
        {
            finfo->prsVar.getAtt("_FillValue").getValues(
                        &(finfo->prsMissingValue));
        }
        catch (NcException)
        {
            try
            {
                finfo->prsVar.getAtt("missing_value").getValues(
                            &(finfo->prsMissingValue));
            }
            catch (NcException) {}
        }
        if (finfo->prsMissingValue != M_INVALID_TRAJECTORY_POS)
        {
            LOG4CPLUS_DEBUG(mlog, "Trajectories: Missing value has been "
                                  "provided for 'pressure': "
                            << finfo->prsMissingValue << ".");
        }


        // Get auxiliary data along trajectories by screening
        // all available ncvars in the input file and picking
        // those vars indicated as "aux. data" by the
        // nc var attribute "auxiliary_data".
        multimap<string, NcVar> ncVariables = ncFile->getVars();
        for (multimap<string, NcVar>::iterator iVar = ncVariables.begin();
             iVar != ncVariables.end(); iVar++)
        {
            QString varName = QString::fromStdString(iVar->first);
            NcCFVar iCFVar(ncFile->getVar(varName.toStdString()));

            string auxDataIndicator = "";
            try { iCFVar.getAtt("auxiliary_data").getValues(auxDataIndicator); }
            catch (NcException) {}

            if (auxDataIndicator == "yes")
            {
                // Currently we can only handle variables of type float or
                // double.
                NcType varType = iCFVar.getType();
                if (varType == NcType::nc_FLOAT || varType == NcType::nc_DOUBLE)
                {
                    if (varName.startsWith('d') && varName != "deposition")
                    {
                        finfo->sensDataVarNames.append(varName);
                        finfo->sensDataVars.append(ncFile->getVar(
                                varName.toStdString()));
                    } else {
                        finfo->auxDataVarNames.append(varName);
                        finfo->auxDataVars.append(ncFile->getVar(
                                varName.toStdString()));
                    }
                }
            }
        }

        // Get time values.
        NcCFVar cfVar(finfo->prsVar);
        finfo->times = QVector<QDateTime>::fromList(cfVar.getTimeValues());

        // Read start grid geometry (positions on which the trajectories were
        // started), if available in file.
        int nSLons = 0;
        NcDim sLonDim = ncFile->getDim("start_lon");
        if (!sLonDim.isNull()) nSLons = sLonDim.getSize();

        int nSLats = 0;
        NcDim sLatDim = ncFile->getDim("start_lat");
        if (!sLatDim.isNull()) nSLats = sLatDim.getSize();

        int nSLevels = 0;
        NcDim sLevelDim = ncFile->getDim("start_isobaric");
        if (!sLevelDim.isNull())
        {
            nSLevels = sLevelDim.getSize();
            NcVar sLevelVar = ncFile->getVar("start_isobaric");
            string axistype;
            sLevelVar.getAtt("axistype").getValues(axistype);

            if (axistype == "pressure levels")
                finfo->startGrid = std::shared_ptr<MStructuredGrid>(
                            new MRegularLonLatStructuredPressureGrid(
                                nSLevels, nSLats, nSLons));
            else if (axistype == "regular ln(p) levels")
                finfo->startGrid = std::shared_ptr<MStructuredGrid>(
                            new MRegularLonLatLnPGrid(
                                nSLevels, nSLats, nSLons));

            NcVar sLonVar = ncFile->getVar("start_lon");
            sLonVar.getVar(finfo->startGrid->lons);
            NcVar sLatVar = ncFile->getVar("start_lat");
            sLatVar.getVar(finfo->startGrid->lats);
            sLevelVar = ncFile->getVar("start_isobaric");
            sLevelVar.getVar(finfo->startGrid->levels);
        }
        else
        {
            sLevelDim = ncFile->getDim("start_hybrid");
            if (!sLevelDim.isNull())
            {
                nSLevels = sLevelDim.getSize();
                MLonLatHybridSigmaPressureGrid *hybGrid =
                        new MLonLatHybridSigmaPressureGrid(
                            nSLevels, nSLats, nSLons);

                NcVar akVar = ncFile->getVar("hyam");
                akVar.getVar(hybGrid->ak_hPa);
                NcVar bkVar = ncFile->getVar("hybm");
                bkVar.getVar(hybGrid->bk);

                // Check the units of the ak coefficients: Pa or hPa? We need hPa!
                string units = "";
                try { akVar.getAtt("units").getValues(units); }
                catch (NcException) {}

                if (units == "Pa")
                    // .. hence, if the ak are given in Pa, convert to hPa.
                    for (int i = 0; i < nSLevels; i++) hybGrid->ak_hPa[i] /= 100.;
                else if (units != "hPa")
                    // .. and if they're in completely different units, raise an
                    // exception.
                    throw MNcException(
                            "NcException",
                            "invalid units for ak coefficients (must be Pa or hPa)",
                            __FILE__, __LINE__);

                MRegularLonLatGrid *psfcGrid =
                        new MRegularLonLatGrid(nSLats, nSLons);
                NcVar psfcVar = ncFile->getVar(
                            "ensemble_minimum_of_surface_pressure");
                psfcVar.getVar(psfcGrid->data);

                // We need Pa for surface pressure.
                try { psfcVar.getAtt("units").getValues(units); }
                catch (NcException) {}
                if (units == "hPa")
                {
                    for (int i = 0; i < nSLons*nSLats; i++)
                        psfcGrid->data[i] *= 100.;
                }
                else if (units != "Pa")
                {
                    throw MNcException(
                                "NcException", "invalid units for "
                                "ensemble_minimum_of_surface_pressure (must "
                                "be Pa or hPa)", __FILE__, __LINE__);
                }

                NcVar sLonVar = ncFile->getVar("start_lon");
                sLonVar.getVar(hybGrid->lons);
                sLonVar.getVar(psfcGrid->lons);
                NcVar sLatVar = ncFile->getVar("start_lat");
                sLatVar.getVar(hybGrid->lats);
                sLatVar.getVar(psfcGrid->lats);
                NcVar sLevelVar = ncFile->getVar("start_hybrid");
                sLevelVar.getVar(hybGrid->levels);

                hybGrid->surfacePressure = psfcGrid;

                finfo->startGrid = std::shared_ptr<MStructuredGrid>(hybGrid);
            }
        }
    } // initialFileAccess
}


} // namespace Met3D
