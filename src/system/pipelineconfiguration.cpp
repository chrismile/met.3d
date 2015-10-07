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
#include "pipelineconfiguration.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "data/scheduler.h"
#include "data/lrumemorymanager.h"
#include "gxfw/msystemcontrol.h"
#include "mainwindow.h"
#include "gxfw/synccontrol.h"
#include "gxfw/mscenecontrol.h"
#include "gxfw/mglresourcesmanager.h"
#include "data/waypoints/waypointstablemodel.h"

#include "data/ecmwfcfreader.h"
#include "data/gribreader.h"
#include "data/verticalregridder.h"
#include "data/structuredgridensemblefilter.h"
#include "data/probabilityregiondetector.h"

#include "data/trajectoryreader.h"
#include "data/trajectorynormalssource.h"
#include "data/trajectoryselectionsource.h"
#include "data/deltapressurepertrajectory.h"
#include "data/thinouttrajectoryfilter.h"
#include "data/probdftrajectoriessource.h"
#include "data/probabltrajectoriessource.h"
#include "data/singletimetrajectoryfilter.h"
#include "data/pressuretimetrajectoryfilter.h"


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MPipelineConfiguration::MPipelineConfiguration()
    : MAbstractApplicationConfiguration()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MPipelineConfiguration::configure()
{
    // If you develop new pipeline modules it might be easier to use a
    // hard-coded pipeline configuration in the development process.
//    initializeDevelopmentDataPipeline();
//    return;

    // Scan global application command line arguments for pipeline definitions.
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    foreach (QString arg, sysMC->getApplicationCommandLineArguments())
    {
        if (arg.startsWith("--pipeline="))
        {
            QString filename = arg.remove("--pipeline=");

            // Production build: Read pipeline configuration from file.
            // Disadvantage: Can only read parameters for the predefined
            // pipelines.
            initializeDataPipelineFromConfigFile(filename);
            return;
        }
    }

    QString errMsg = QString(
                "ERROR: No data pipeline configuration file specified. "
                "Use the '--pipeline=<file>' command line argument.");
    LOG4CPLUS_ERROR(mlog, errMsg.toStdString());
    throw MInitialisationError(errMsg.toStdString(), __FILE__, __LINE__);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MPipelineConfiguration::initializeScheduler()
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();

    sysMC->registerScheduler("SingleThread", new MSingleThreadScheduler());
    sysMC->registerScheduler("MultiThread", new MMultiThreadScheduler());
}


void MPipelineConfiguration::initializeDataPipelineFromConfigFile(
        QString filename)
{
    LOG4CPLUS_INFO(mlog, "Loading data pipeline configuration from file "
                   << filename.toStdString() << "...");

    if ( !QFile::exists(filename) )
    {
        QString errMsg = QString(
                    "Cannot open file %1: file does not exist.").arg(filename);
        LOG4CPLUS_ERROR(mlog, errMsg.toStdString());
        throw MInitialisationError(errMsg.toStdString(), __FILE__, __LINE__);
    }

    initializeScheduler();

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    QSettings config(filename, QSettings::IniFormat);

    // Initialize memory manager(s).
    // =============================
    int size = config.beginReadArray("MemoryManager");

    for (int i = 0; i < size; i++)
    {
        config.setArrayIndex(i);

        // Read settings from file.
        QString name = config.value("name").toString();
        int size_MB = config.value("size_MB").toInt();

        LOG4CPLUS_DEBUG(mlog, "initializing memory manager #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  name = " << name.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  size = " << size_MB << " MB");

        // Check parameter validity.
        if ( name.isEmpty()
             || (size <= 0) )
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        // Create new memory manager.
        sysMC->registerMemoryManager(
                    name, new MLRUMemoryManager(name, size_MB * 1024.));
    }

    config.endArray();

    // NWP pipeline(s).
    // ================
    size = config.beginReadArray("NWPPipeline");

    for (int i = 0; i < size; i++)
    {    
        config.setArrayIndex(i);

        // Read settings from file.
        QString name = config.value("name").toString();
        bool isEnsemble = config.value("ensemble").toBool();
        QString path = expandEnvironmentVariables(config.value("path").toString());
        QString domainID = config.value("domainID").toString();
        QString schedulerID = config.value("schedulerID").toString();
        QString memoryManagerID = config.value("memoryManagerID").toString();
        QString fileFormatStr = config.value("fileFormat").toString();
        bool enableRegridding = config.value("enableRegridding").toBool();

        LOG4CPLUS_DEBUG(mlog, "initializing NWP pipeline #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  name = " << name.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  " << (isEnsemble ? "ensemble" : "deterministic"));
        LOG4CPLUS_DEBUG(mlog, "  path = " << path.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  domainID = " << domainID.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  schedulerID = " << schedulerID.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  memoryManagerID=" << memoryManagerID.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  fileFormat=" << fileFormatStr.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  regridding=" << (enableRegridding ? "enabled" : "disabled"));

        MNWPReaderFileFormat fileFormat = INVALID_FORMAT;
        if (fileFormatStr == "CF_NETCDF") fileFormat = CF_NETCDF;
        else if (fileFormatStr == "ECMWF_CF_NETCDF") fileFormat = ECMWF_CF_NETCDF;
        else if (fileFormatStr == "ECMWF_GRIB") fileFormat = ECMWF_GRIB;

        // Check parameter validity.
        if ( name.isEmpty()
             || path.isEmpty()
             || (fileFormat != CF_NETCDF && domainID.isEmpty())
             || schedulerID.isEmpty()
             || memoryManagerID.isEmpty()
             || (fileFormat == INVALID_FORMAT) )
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        // Create new pipeline.
        if (isEnsemble)
            initializeNWPEnsemblePipeline(
                        name, path, domainID, schedulerID,
                        memoryManagerID, fileFormat, enableRegridding);
        else
            initializeNWPDeterministicPipeline(
                        name, path, domainID, schedulerID,
                        memoryManagerID, fileFormat, enableRegridding);
    }

    config.endArray();

    // Lagranto trajectory pipeline(s).
    // ================================
    size = config.beginReadArray("LagrantoPipeline");

    for (int i = 0; i < size; i++)
    {
        config.setArrayIndex(i);

        // Read settings from file.
        QString name = config.value("name").toString();
        bool isEnsemble = config.value("ensemble").toBool();
        QString path = expandEnvironmentVariables(config.value("path").toString());
        bool ablTrajectories = config.value("ABLTrajectories").toBool();
        QString schedulerID = config.value("schedulerID").toString();
        QString memoryManagerID = config.value("memoryManagerID").toString();

        LOG4CPLUS_DEBUG(mlog, "initializing LAGRANTO pipeline #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  name = " << name.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  " << (isEnsemble ? "ensemble" : "deterministic"));
        LOG4CPLUS_DEBUG(mlog, "  path = " << path.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  type = " << (ablTrajectories ? "ABL-T" : "DF-T"));
        LOG4CPLUS_DEBUG(mlog, "  schedulerID = " << schedulerID.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  memoryManagerID = " << memoryManagerID.toStdString());

        // Check parameter validity.
        if ( name.isEmpty()
             || path.isEmpty()
             || schedulerID.isEmpty()
             || memoryManagerID.isEmpty() )
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        // Create new pipeline.
        if (isEnsemble)
            initializeLagrantoEnsemblePipeline(
                        name, path, ablTrajectories, schedulerID,
                        memoryManagerID);
        else
            LOG4CPLUS_WARN(mlog, "deterministic LAGRANTO pipeline has not"
                           "been implemented yet; skipping.");
    }

    config.endArray();

    LOG4CPLUS_INFO(mlog, "Data pipeline has been configured.");
}


void MPipelineConfiguration::initializeNWPDeterministicPipeline(
        QString name,
        QString fileDir,
        QString domain,
        QString schedulerID,
        QString memoryManagerID,
        MNWPReaderFileFormat dataFormat,
        bool enableRegridding)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MAbstractScheduler* scheduler = sysMC->getScheduler(schedulerID);
    MAbstractMemoryManager* memoryManager = sysMC->getMemoryManager(memoryManagerID);

    const QString dataSourceId = name;
    LOG4CPLUS_DEBUG(mlog, "Initializing deterministic pipeline ''"
                    << dataSourceId.toStdString() << "'' ...");

    MWeatherPredictionReader *nwpReaderDET = nullptr;
    if (dataFormat == CF_NETCDF)
    {
        MClimateForecastReader *cfReaderDET =
                new MClimateForecastReader(dataSourceId);
        cfReaderDET->setMemoryManager(memoryManager);
        cfReaderDET->setScheduler(scheduler);
        cfReaderDET->setDataRoot(fileDir);
        nwpReaderDET = cfReaderDET;
    }
    else if (dataFormat == ECMWF_CF_NETCDF)
    {
        MECMWFClimateForecastReader *ecmwfReaderDET =
                new MECMWFClimateForecastReader(dataSourceId);
        ecmwfReaderDET->setMemoryManager(memoryManager);
        ecmwfReaderDET->setScheduler(scheduler);
        ecmwfReaderDET->setDataRoot(fileDir, domain);
        nwpReaderDET = ecmwfReaderDET;
    }
    else if (dataFormat == ECMWF_GRIB)
    {
        MGribReader *gribReaderDET =
                new MGribReader(dataSourceId, DETERMINISTIC_FORECAST);
        gribReaderDET->setMemoryManager(memoryManager);
        gribReaderDET->setScheduler(scheduler);
        gribReaderDET->setDataRoot(fileDir);
        nwpReaderDET = gribReaderDET;
    }

    if (!enableRegridding)
    {
        sysMC->registerDataSource(dataSourceId, nwpReaderDET);
    }
    else
    {
        MVerticalRegridder *regridderDET =
                new MVerticalRegridder();
        regridderDET->setMemoryManager(memoryManager);
        regridderDET->setScheduler(scheduler);
        regridderDET->setInputSource(nwpReaderDET);

        sysMC->registerDataSource(dataSourceId, regridderDET);
    }

    LOG4CPLUS_DEBUG(mlog, "Pipeline ''" << dataSourceId.toStdString()
                    << "'' has been initialized.");
}


void MPipelineConfiguration::initializeNWPEnsemblePipeline(
        QString name,
        QString fileDir,
        QString domain,
        QString schedulerID,
        QString memoryManagerID,
        MNWPReaderFileFormat dataFormat,
        bool enableRegridding)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MAbstractScheduler* scheduler = sysMC->getScheduler(schedulerID);
    MAbstractMemoryManager* memoryManager = sysMC->getMemoryManager(memoryManagerID);

    const QString dataSourceId = name;
    LOG4CPLUS_DEBUG(mlog, "Initializing ensemble pipeline ''"
                    << dataSourceId.toStdString() << "'' ...");

    MWeatherPredictionReader *nwpReaderENS = nullptr;
    if (dataFormat == CF_NETCDF)
    {
        MClimateForecastReader *cfReaderENS =
                new MClimateForecastReader(dataSourceId);
        cfReaderENS->setMemoryManager(memoryManager);
        cfReaderENS->setScheduler(scheduler);
        cfReaderENS->setDataRoot(fileDir);
        nwpReaderENS = cfReaderENS;
    }
    else if (dataFormat == ECMWF_CF_NETCDF)
    {
        MECMWFEnsembleClimateForecastReader *ecmwfReaderENS =
                new MECMWFEnsembleClimateForecastReader(dataSourceId);
        ecmwfReaderENS->setMemoryManager(memoryManager);
        ecmwfReaderENS->setScheduler(scheduler);
        ecmwfReaderENS->setDataRoot(fileDir, domain);
        nwpReaderENS = ecmwfReaderENS;
    }
    else if (dataFormat == ECMWF_GRIB)
    {
        MGribReader *gribReaderENS =
                new MGribReader(dataSourceId, ENSEMBLE_FORECAST);
        gribReaderENS->setMemoryManager(memoryManager);
        gribReaderENS->setScheduler(scheduler);
        gribReaderENS->setDataRoot(fileDir);
        nwpReaderENS = gribReaderENS;
    }
    sysMC->registerDataSource(dataSourceId, nwpReaderENS);

    MStructuredGridEnsembleFilter *ensFilter =
            new MStructuredGridEnsembleFilter();
    ensFilter->setMemoryManager(memoryManager);
    ensFilter->setScheduler(scheduler);

    if (!enableRegridding)
    {
        ensFilter->setInputSource(nwpReaderENS);
    }
    else
    {
        MStructuredGridEnsembleFilter *ensFilter1 =
                new MStructuredGridEnsembleFilter();
        ensFilter1->setMemoryManager(memoryManager);
        ensFilter1->setScheduler(scheduler);
        ensFilter1->setInputSource(nwpReaderENS);

        MVerticalRegridder *regridderEPS =
                new MVerticalRegridder();
        regridderEPS->setMemoryManager(memoryManager);
        regridderEPS->setScheduler(scheduler);
        regridderEPS->setInputSource(ensFilter1);

        ensFilter->setInputSource(regridderEPS);
    }

    sysMC->registerDataSource(dataSourceId + QString(" ENSFilter"),
                              ensFilter);

    MProbabilityRegionDetectorFilter *probRegDetectorNWP =
            new MProbabilityRegionDetectorFilter();
    probRegDetectorNWP->setMemoryManager(memoryManager);
    probRegDetectorNWP->setScheduler(scheduler);
    probRegDetectorNWP->setInputSource(ensFilter);

    sysMC->registerDataSource(dataSourceId + QString(" ProbReg"),
                              probRegDetectorNWP);

    LOG4CPLUS_DEBUG(mlog, "Pipeline ''" << dataSourceId.toStdString()
                    << "'' has been initialized.");
}


void MPipelineConfiguration::initializeLagrantoEnsemblePipeline(
        QString name,
        QString fileDir,
        bool boundaryLayerTrajectories,
        QString schedulerID,
        QString memoryManagerID)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MAbstractScheduler* scheduler = sysMC->getScheduler(schedulerID);
    MAbstractMemoryManager* memoryManager =
            sysMC->getMemoryManager(memoryManagerID);

    const QString dataSourceId = name;
    LOG4CPLUS_DEBUG(mlog, "Initializing LAGRANTO ensemble pipeline ''"
                    << dataSourceId.toStdString() << "'' ...");

    // Trajectory reader.
    MTrajectoryReader *trajectoryReader =
            new MTrajectoryReader(dataSourceId);
    trajectoryReader->setMemoryManager(memoryManager);
    trajectoryReader->setScheduler(scheduler);
    trajectoryReader->setDataRoot(fileDir);
    sysMC->registerDataSource(dataSourceId + QString(" Reader"),
                              trajectoryReader);

    MDeltaPressurePerTrajectorySource *dpSource =
            new MDeltaPressurePerTrajectorySource();
    dpSource->setMemoryManager(memoryManager);
    dpSource->setScheduler(scheduler);
    dpSource->setTrajectorySource(trajectoryReader);

    MThinOutTrajectoryFilter *thinoutFilter =
            new MThinOutTrajectoryFilter();
    thinoutFilter->setMemoryManager(memoryManager);
    thinoutFilter->setScheduler(scheduler);
    thinoutFilter->setTrajectorySource(trajectoryReader);

    MPressureTimeTrajectoryFilter *dpdtFilter =
            new MPressureTimeTrajectoryFilter();
    dpdtFilter->setMemoryManager(memoryManager);
    dpdtFilter->setScheduler(scheduler);
    dpdtFilter->setInputSelectionSource(thinoutFilter);
    dpdtFilter->setDeltaPressureSource(dpSource);

    MSingleTimeTrajectoryFilter *timestepFilter =
            new MSingleTimeTrajectoryFilter();
    timestepFilter->setMemoryManager(memoryManager);
    timestepFilter->setScheduler(scheduler);
    timestepFilter->setInputSelectionSource(dpdtFilter);
    sysMC->registerDataSource(dataSourceId + QString(" timestepFilter"),
                              timestepFilter);

    MTrajectoryNormalsSource *trajectoryNormals =
            new MTrajectoryNormalsSource();
    trajectoryNormals->setMemoryManager(memoryManager);
    trajectoryNormals->setScheduler(scheduler);
    trajectoryNormals->setTrajectorySource(trajectoryReader);
    sysMC->registerDataSource(dataSourceId + QString(" Normals"),
                              trajectoryNormals);

    // Probability filter.
    MWeatherPredictionDataSource* pwcbSource;
    if (boundaryLayerTrajectories)
    {
        MProbABLTrajectoriesSource* source =
                new MProbABLTrajectoriesSource();
        source->setMemoryManager(memoryManager);
        source->setScheduler(scheduler);
        source->setTrajectorySource(trajectoryReader);
        source->setInputSelectionSource(timestepFilter);

        pwcbSource = source;
    }
    else
    {
        MProbDFTrajectoriesSource* source =
                new MProbDFTrajectoriesSource();
        source->setMemoryManager(memoryManager);
        source->setScheduler(scheduler);
        source->setTrajectorySource(trajectoryReader);
        source->setInputSelectionSource(timestepFilter);

        pwcbSource = source;
    }
    sysMC->registerDataSource(dataSourceId, pwcbSource);

    // Region detection filter.
    MProbabilityRegionDetectorFilter *probRegDetector =
            new MProbabilityRegionDetectorFilter();
    probRegDetector->setMemoryManager(memoryManager);
    probRegDetector->setScheduler(scheduler);
    probRegDetector->setInputSource(pwcbSource);
    sysMC->registerDataSource(dataSourceId + QString(" ProbReg"),
                              probRegDetector);

    LOG4CPLUS_DEBUG(mlog, "Pipeline ''" << dataSourceId.toStdString()
                    << "'' has been initialized.");
}


void MPipelineConfiguration::initializeDevelopmentDataPipeline()
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();

    initializeScheduler();

    sysMC->registerMemoryManager("NWP",
               new MLRUMemoryManager("NWP", 10.*1024.*1024.));
    sysMC->registerMemoryManager("Analysis",
               new MLRUMemoryManager("Analysis", 10.*1024.));

    initializeNWPDeterministicPipeline(
                "ECMWF DET EUR_LL015",
                "/home/local/data/mss/grid/ecmwf/netcdf",
                "EUR_LL015",
                "SingleThread",
                "NWP",
                ECMWF_CF_NETCDF,
                false);

    initializeNWPEnsemblePipeline(
                "ECMWF ENS EUR_LL10",
                "/home/local/data/mss/grid/ecmwf/netcdf",
                "EUR_LL10",
                "SingleThread",
                "NWP",
                ECMWF_CF_NETCDF,
                false);

    sysMC->registerMemoryManager("Trajectories DF-T psfc_1000hPa_L62",
               new MLRUMemoryManager("Trajectories DF-T psfc_1000hPa_L62",
                                     10.*1024.*1024.));

    initializeLagrantoEnsemblePipeline(
                "Lagranto ENS EUR_LL10 DF-T psfc_1000hPa_L62",
                "/mnt/ssd/data/trajectories/EUR_LL10/psfc_1000hPa_L62",
                false,
                "SingleThread",
                "Trajectories DF-T psfc_1000hPa_L62");

    sysMC->registerMemoryManager("Trajectories DF-T psfc_min_L62",
               new MLRUMemoryManager("Trajectories  DF-T psfc_min_L62",
                                     12.*1024.*1024.));

    initializeLagrantoEnsemblePipeline(
                "Lagranto ENS EUR_LL10 DF-T psfc_min_L62",
                "/mnt/ssd/data/trajectories/EUR_LL10/psfc_min_L62",
                false,
                "SingleThread",
                "Trajectories DF-T psfc_min_L62");

//    sysMC->registerMemoryManager("Trajectories DF-T psfc_min_L62",
//               new MLRUMemoryManager("Trajectories  DF-T psfc_min_L62",
//                                     12.*1024.*1024.));
//    initializeLagrantoEnsemblePipeline(
//                "EUR_LL025 DF-T psfc_min_L62",
//                "/mnt/ssd/data/trajectories/EUR_LL025/psfc_min_L62",
//                false,
//                "Trajectories DF-T psfc_min_L62",
//                "ECMWF ENS EUR_LL10");

    sysMC->registerMemoryManager("Trajectories ABL-T psfc_min_L62_abl",
               new MLRUMemoryManager("Trajectories ABL-T psfc_min_L62_abl",
                                     10.*1024.*1024.));
    initializeLagrantoEnsemblePipeline(
                "Lagranto ENS EUR_LL10 ABL-T psfc_min_L62_abl",
                "/mnt/ssd/data/trajectories/EUR_LL10/psfc_min_L62_abl",
                true,
                "SingleThread",
                "Trajectories ABL-T psfc_min_L62_abl");

    sysMC->registerMemoryManager("Trajectories ABL-T 10hPa",
               new MLRUMemoryManager("Trajectories ABL-T 10hPa",
                                     10.*1024.*1024.));
    initializeLagrantoEnsemblePipeline(
                "Lagranto ENS EUR_LL10 ABL-T 10hPa",
                "/mnt/ssd/data/trajectories/EUR_LL10/blt_PL10hPa",
                true,
                "SingleThread",
                "Trajectories ABL-T 10hPa");
}


} // namespace Met3D
