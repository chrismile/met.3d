/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2017-2018 Bianca Tost
**  Copyright 2017      Philipp Kaiser
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
#include <src/data/trajectorycomputation.h>

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

#include "data/climateforecastreader.h"
#include "data/gribreader.h"
#include "data/verticalregridder.h"
#include "data/structuredgridensemblefilter.h"
#include "data/probabilityregiondetector.h"
#include "data/derivedmetvarsdatasource.h"
#include "data/differencedatasource.h"

#include "data/trajectoryreader.h"
#include "data/trajectorycomputation.h"
#include "data/trajectorynormalssource.h"
#include "data/trajectoryselectionsource.h"
#include "data/deltapressurepertrajectory.h"
#include "data/thinouttrajectoryfilter.h"
#include "data/probdftrajectoriessource.h"
#include "data/probabltrajectoriessource.h"
#include "data/singletimetrajectoryfilter.h"
#include "data/pressuretimetrajectoryfilter.h"
#include "data/bboxtrajectoryfilter.h"


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

    QString filename = "";

    // Scan global application command line arguments for pipeline definitions.
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    foreach (QString arg, sysMC->getApplicationCommandLineArguments())
    {
        if (arg.startsWith("--pipeline="))
        {
            filename = arg.remove("--pipeline=");
            filename = expandEnvironmentVariables(filename);
        }
    }

    QString errMsg = "";
    // If Met.3D is called by Metview and no configuration files are given,
    // use default configuration files stored at
    // $MET3D_HOME/config/metview/default_pipeline.cfg .
    if (sysMC->isConnectedToMetview() && filename.isEmpty())
    {
        filename = "$MET3D_HOME/config/metview/default_pipeline.cfg";
        filename = expandEnvironmentVariables(filename);
        QFileInfo fileInfo(filename);
        if (!fileInfo.isFile())
        {
            errMsg = QString(
                        "ERROR: Default Metview pipeline configuration file"
                        " does not exist. Location: ") + filename;
        }
        LOG4CPLUS_ERROR(mlog, errMsg.toStdString());
    }

    if (!filename.isEmpty())
    {
        // Production build: Read pipeline configuration from file.
        // Disadvantage: Can only read parameters for the predefined
        // pipelines.
        initializeDataPipelineFromConfigFile(filename);
        return;
    }
    else
    {
        errMsg = QString(
                    "ERROR: No data pipeline configuration file specified. "
                    "Use the '--pipeline=<file>' command line argument.");
        LOG4CPLUS_ERROR(mlog, errMsg.toStdString());
    }
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
    QMap<QString, QString> *defaultMemoryManagers =
            sysMC->getDefaultMemoryManagers();
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


    // Default memory managers.
    // ========================
    config.beginGroup("DefaultMemoryManagers");

    QString defaultMemoryManager =
            config.value("defaultNWPMemoryManager", "").toString();
    checkAndStoreDefaultPipelineMemoryManager(
            defaultMemoryManager, "NWP", defaultMemoryManagers, sysMC);

    defaultMemoryManager =
            config.value("defaultAnalysisMemoryManager", "").toString();
    checkAndStoreDefaultPipelineMemoryManager(
            defaultMemoryManager, "Analysis", defaultMemoryManagers, sysMC);

    defaultMemoryManager =
            config.value("defaultTrajectoryMemoryManager", "").toString();
    checkAndStoreDefaultPipelineMemoryManager(
            defaultMemoryManager, "Trajectories", defaultMemoryManagers, sysMC);

    config.endGroup();

    // NWP pipeline(s).
    // ================
    size = config.beginReadArray("NWPPipeline");

    // Get directories and file filters specified by path command line argument
    // if present.
    QList<MetviewGribFilePath> filePathList;
    if (sysMC->isConnectedToMetview())
    {
        getMetviewGribFilePaths(&filePathList);
        // For Metview integration use directories and file filters specified by
        // command line arguments instead of directories and file filters
        // specified by pipeline configuration.
        size = filePathList.size();
    }

    QString path, fileFilter, name;
    config.setArrayIndex(0);

    for (int i = 0; i < size; i++)
    {
        // Read settings from file.

        // If Met.3D is called from Metview use only the first entry in the
        // pipeline configuration file to initialise each data source.
        if (sysMC->isConnectedToMetview())
        {
            // Use name of first NWPPipeline entry in pipeline configuration as
            // name and append index for each new data source.
            name = config.value("name").toString() + QString("_%1").arg(i);
            path = filePathList.at(i).path;
            fileFilter = filePathList.at(i).fileFilter;
        }
        // Use all NWPPipeline entries if we call Met.3D as own program.
        else
        {
            config.setArrayIndex(i);
            name = config.value("name").toString();
            path = expandEnvironmentVariables(config.value("path").toString());
            fileFilter = config.value("fileFilter").toString();
        }
        QString domainID = config.value("domainID").toString();
        QString schedulerID = config.value("schedulerID").toString();
        QString memoryManagerID = config.value("memoryManagerID").toString();
        QString fileFormatStr = config.value("fileFormat").toString();
        bool enableRegridding = config.value("enableRegridding", false).toBool();
        bool enableProbRegionFilter = config.value("enableProbabilityRegionFilter",
                                                   false).toBool();
        bool treatRotatedGridAsRegularGrid =
                config.value("treatRotatedGridAsRegularGrid", false).toBool();
        QString gribSurfacePressureFieldType =
                config.value("gribSurfacePressureFieldType", "auto").toString();
        bool convertGeometricHeightToPressure_ICAOStandard =
                config.value("convertGeometricHeightToPressure_ICAOStandard",
                             false).toBool();
        QString auxiliary3DPressureField =
                config.value("auxiliary3DPressureField", "").toString();
        bool disableGridConsistencyCheck =
                config.value("disableGridConsistencyCheck", "").toBool();
        QString inputVarsForDerivedVars =
                config.value("inputVarsForDerivedVars", "").toString();

//TODO (mr, 16Dec2015) -- compatibility code; remove in Met.3D version 2.0
        // If no fileFilter is specified but a domainID is specified use
        // "*domainID*" as fileFilter. If neither is specified, use "*".
        if (fileFilter.isEmpty())
        {
            if (domainID.isEmpty()) fileFilter = "*";
            else fileFilter = QString("*%1*").arg(domainID);
        }

        LOG4CPLUS_DEBUG(mlog, "initializing NWP pipeline #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  name = " << name.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  path = " << path.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  fileFilter = " << fileFilter.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  schedulerID = " << schedulerID.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  memoryManagerID=" << memoryManagerID.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  fileFormat=" << fileFormatStr.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  regridding="
                        << (enableRegridding ? "enabled" : "disabled"));
        LOG4CPLUS_DEBUG(mlog, "  probability region="
                        << (enableProbRegionFilter ? "enabled" : "disabled"));
        LOG4CPLUS_DEBUG(mlog, "  treat rotated grid as regular grid="
                        << (treatRotatedGridAsRegularGrid ? "enabled" : "disabled"));
        LOG4CPLUS_DEBUG(mlog, "  surfacePressureFieldType="
                        << gribSurfacePressureFieldType.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  convert geometric height to pressure (using"
                              " standard ICAO)="
                        << (convertGeometricHeightToPressure_ICAOStandard
                            ? "enabled" : "disabled"));
        LOG4CPLUS_DEBUG(mlog, "  use auxiliary 3D pressure field="
                        << (auxiliary3DPressureField != "" ?
                    "enabled (name= " + (auxiliary3DPressureField.toStdString())
                    + ")" : "disabled"));
        LOG4CPLUS_DEBUG(mlog, "  grid consistency check="
                        << (!disableGridConsistencyCheck
                            ? "enabled" : "disabled"));
        LOG4CPLUS_DEBUG(mlog, "  input variables for derived variables="
                        << inputVarsForDerivedVars.toStdString());

        MNWPReaderFileFormat fileFormat = INVALID_FORMAT;
        if (fileFormatStr == "CF_NETCDF") fileFormat = CF_NETCDF;
//TODO (mr, 16Dec2015) -- compatibility code; remove in Met.3D version 2.0
        else if (fileFormatStr == "ECMWF_CF_NETCDF") fileFormat = CF_NETCDF;
        else if (fileFormatStr == "ECMWF_GRIB") fileFormat = ECMWF_GRIB;

        QStringList validGribSurfacePressureFieldTypes;
        validGribSurfacePressureFieldTypes << "auto" << "sp" << "lnsp";

        // Check parameter validity.
        if ( name.isEmpty()
             || path.isEmpty()
             || schedulerID.isEmpty()
             || memoryManagerID.isEmpty()
             || (fileFormat == INVALID_FORMAT)
             || (fileFormat == ECMWF_GRIB && !validGribSurfacePressureFieldTypes
                 .contains(gribSurfacePressureFieldType)))
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        // Create new pipeline.
        initializeNWPPipeline(
                    name, path, fileFilter, schedulerID,
                    memoryManagerID, fileFormat, enableRegridding,
                    enableProbRegionFilter, treatRotatedGridAsRegularGrid,
                    gribSurfacePressureFieldType,
                    convertGeometricHeightToPressure_ICAOStandard,
                    auxiliary3DPressureField, disableGridConsistencyCheck,
                    inputVarsForDerivedVars);
    }

    config.endArray();

    // Trajectory pipeline(s).
    // ================================
    size = config.beginReadArray("TrajectoriesPipeline");

    for (int i = 0; i < size; i++)
    {
        config.setArrayIndex(i);

        // Read settings from file.
        QString name = config.value("name").toString();
        bool isEnsemble = config.value("ensemble", true).toBool();
        QString path =
                expandEnvironmentVariables(config.value("path").toString());
        bool ablTrajectories = config.value("ABLTrajectories", false).toBool();
        QString schedulerID = config.value("schedulerID").toString();
        QString memoryManagerID = config.value("memoryManagerID").toString();
        bool precomputed = config.value("precomputed", false).toBool();
        QString NWPDataset =
                config.value("NWPDataset").toString();
        QString windEastwardVariable =
                config.value("eastwardWind_ms").toString();
        QString windNorthwardVariable =
                config.value("northwardWind_ms").toString();
        QString windVerticalVariable =
                config.value("verticalWind_Pas").toString();
        QString windVarsVerticalLevelTypeString =
                config.value("windComponentVariablesVerticalLevelType").toString();

        if (precomputed)
        {
            LOG4CPLUS_DEBUG(mlog, "initializing precomputed trajectories pipeline #"
                            << i << ": ");
            LOG4CPLUS_DEBUG(mlog, "  name = " << name.toStdString());
            LOG4CPLUS_DEBUG(mlog, "  "
                            << (isEnsemble ? "ensemble" : "deterministic"));
            LOG4CPLUS_DEBUG(mlog, "  path = " << path.toStdString());
            LOG4CPLUS_DEBUG(mlog, "  type = "
                            << (ablTrajectories ? "ABL-T" : "DF-T"));
            LOG4CPLUS_DEBUG(mlog, "  schedulerID = "
                            << schedulerID.toStdString());
            LOG4CPLUS_DEBUG(mlog, "  memoryManagerID = "
                            << memoryManagerID.toStdString());

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
            {
                initializePrecomputedTrajectoriesPipeline(
                        name, path, ablTrajectories, schedulerID,
                        memoryManagerID);
            }
            else
            {
                LOG4CPLUS_WARN(mlog, "deterministic precomputed trajectories"
                                     " pipeline has not been implemented yet;"
                                     " skipping.");
            }
        }
        else
        {
            LOG4CPLUS_DEBUG(mlog,
                            "initializing trajectory computation pipeline #"
                            << i << ": ");
            LOG4CPLUS_DEBUG(mlog, "  name = " << name.toStdString());
            LOG4CPLUS_DEBUG(mlog, "  "
                            << (isEnsemble ? "ensemble" : "deterministic"));
            LOG4CPLUS_DEBUG(mlog, "  type = "
                            << (ablTrajectories ? "ABL-T" : "DF-T"));
            LOG4CPLUS_DEBUG(mlog, "  schedulerID = "
                            << schedulerID.toStdString());
            LOG4CPLUS_DEBUG(mlog, "  memoryManagerID = "
                            << memoryManagerID.toStdString());
            LOG4CPLUS_DEBUG(mlog, "  NWPDataset = "
                            << NWPDataset.toStdString());
            LOG4CPLUS_DEBUG(mlog, "  eastward wind variable = "
                            << windEastwardVariable.toStdString());
            LOG4CPLUS_DEBUG(mlog, "  northward wind variable = "
                            << windNorthwardVariable.toStdString());
            LOG4CPLUS_DEBUG(mlog, "  vertical wind variable = "
                            << windVerticalVariable.toStdString());
            LOG4CPLUS_DEBUG(mlog, "  wind vertical level type  = "
                            << windVarsVerticalLevelTypeString.toStdString());

            MVerticalLevelType windVarsVerticalLevelType =
                    MStructuredGrid::verticalLevelTypeFromConfigString(
                        windVarsVerticalLevelTypeString);

            // Check parameter validity.
            if ( name.isEmpty()
                 || NWPDataset.isEmpty()
                 || windEastwardVariable.isEmpty()
                 || windNorthwardVariable.isEmpty()
                 || windVerticalVariable.isEmpty()
                 || schedulerID.isEmpty()
                 || memoryManagerID.isEmpty()
                 || ( windVarsVerticalLevelType == SURFACE_2D
                      || windVarsVerticalLevelType == POTENTIAL_VORTICITY_2D )
                 || ( !windVarsVerticalLevelTypeString.isEmpty()
                      && windVarsVerticalLevelType == SIZE_LEVELTYPES ))
            {
                LOG4CPLUS_WARN(mlog, "invalid parameters encountered;"
                                     " skipping.");
                continue;
            }

            // Create new pipeline.
            if (isEnsemble)
            {
                initializeTrajectoryComputationPipeline(
                        name, ablTrajectories, schedulerID,
                        memoryManagerID, NWPDataset,
                        windEastwardVariable, windNorthwardVariable,
                        windVerticalVariable, windVarsVerticalLevelType);
            }
            else
            {
                LOG4CPLUS_WARN(mlog, "deterministic computed trajectories"
                                     " pipeline has not been implemented yet;"
                                     " skipping.");
            }
        }
    }

    config.endArray();

    // Configurable pipeline(s).
    // ================
    size = config.beginReadArray("ConfigurablePipeline");

    for (int i = 0; i < size; i++)
    {
        config.setArrayIndex(i);

        // Read settings from file.
        QString typeName = config.value("type").toString();
        QString name = config.value("name").toString();
        QString inputSource0 = config.value("input1").toString();
        QString inputSource1 = config.value("input2").toString();
        QString baseRequest0 = config.value("baseRequest1").toString();
        QString baseRequest1 = config.value("baseRequest2").toString();
        QString schedulerID = config.value("schedulerID").toString();
        QString memoryManagerID = config.value("memoryManagerID").toString();
        bool enableRegridding = config.value("enableRegridding").toBool();

        LOG4CPLUS_DEBUG(mlog, "initializing configurable pipeline #" << i << ": ");
        LOG4CPLUS_DEBUG(mlog, "  type = " << typeName.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  name = " << name.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  input1 = " << inputSource0.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  input2 = " << inputSource1.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  baseRequest1 = " << baseRequest0.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  baseRequest2 = " << baseRequest1.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  schedulerID = " << schedulerID.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  memoryManagerID = " << memoryManagerID.toStdString());
        LOG4CPLUS_DEBUG(mlog, "  regridding="
                        << (enableRegridding ? "enabled" : "disabled"));

        MConfigurablePipelineType pipelineType =
                configurablePipelineTypeFromString(typeName);

        // Check parameter validity.
        if ( name.isEmpty()
             || (pipelineType == INVALID_PIPELINE_TYPE)
             || inputSource0.isEmpty()
             || inputSource1.isEmpty()
             || baseRequest0.isEmpty()
             || baseRequest1.isEmpty()
             || schedulerID.isEmpty()
             || memoryManagerID.isEmpty() )
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        // Create new pipeline.
        initializeConfigurablePipeline(
                    pipelineType, name, inputSource0, inputSource1, baseRequest0,
                    baseRequest1, schedulerID, memoryManagerID,
                    enableRegridding);
    }

    config.endArray();

    LOG4CPLUS_INFO(mlog, "Data pipeline has been configured.");
}


void MPipelineConfiguration::initializeNWPPipeline(
        QString name,
        QString fileDir,
        QString fileFilter,
        QString schedulerID,
        QString memoryManagerID,
        MNWPReaderFileFormat dataFormat,
        bool enableRegridding,
        bool enableProbabiltyRegionFilter,
        bool treatRotatedGridAsRegularGrid,
        QString surfacePressureFieldType,
        bool convertGeometricHeightToPressure_ICAOStandard,
        QString auxiliary3DPressureField,
        bool disableGridConsistencyCheck,
        QString inputVarsForDerivedVars)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MAbstractScheduler* scheduler = sysMC->getScheduler(schedulerID);
    MAbstractMemoryManager* memoryManager = sysMC->getMemoryManager(memoryManagerID);

    const QString dataSourceId = name;
    LOG4CPLUS_DEBUG(mlog, "Initializing NWP pipeline ''"
                    << dataSourceId.toStdString() << "'' ...");

    // Pipeline for data fields that are stored on disk.
    // =================================================

    MWeatherPredictionReader *nwpReaderENS = nullptr;
    if (dataFormat == CF_NETCDF)
    {
        nwpReaderENS = new MClimateForecastReader(
                    dataSourceId, treatRotatedGridAsRegularGrid,
                    convertGeometricHeightToPressure_ICAOStandard,
                    auxiliary3DPressureField, disableGridConsistencyCheck);
    }
    else if (dataFormat == ECMWF_GRIB)
    {
        nwpReaderENS = new MGribReader(dataSourceId,
                                       surfacePressureFieldType,
                                       disableGridConsistencyCheck);
    }
    nwpReaderENS->setMemoryManager(memoryManager);
    nwpReaderENS->setScheduler(scheduler);
    nwpReaderENS->setDataRoot(fileDir, fileFilter);

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

    if (enableProbabiltyRegionFilter)
    {
        MProbabilityRegionDetectorFilter *probRegDetectorNWP =
                new MProbabilityRegionDetectorFilter();
        probRegDetectorNWP->setMemoryManager(memoryManager);
        probRegDetectorNWP->setScheduler(scheduler);
        probRegDetectorNWP->setInputSource(ensFilter);

        sysMC->registerDataSource(dataSourceId + QString(" ProbReg"),
                                  probRegDetectorNWP);
    }

    // Pipeline for derived variables (derivedMetVarsSource connects to
    // the reader and computes derived data fields. The rest of the pipeline
    // is the same as above).
    // =====================================================================

    const QString dataSourceIdDerived = dataSourceId + " derived";

    MDerivedMetVarsDataSource *derivedMetVarsSource =
            new MDerivedMetVarsDataSource();
    derivedMetVarsSource->setMemoryManager(memoryManager);
    derivedMetVarsSource->setScheduler(scheduler);
    derivedMetVarsSource->setInputSource(nwpReaderENS);

    QStringList derivedVarsMappingList =
            inputVarsForDerivedVars.split("/", QString::SkipEmptyParts);

    foreach (QString derivedVarsMappingString, derivedVarsMappingList)
    {
        QStringList derivedVarsMapping =
                derivedVarsMappingString.split(":", QString::SkipEmptyParts);
        if (derivedVarsMapping.size() == 2)
        {
            derivedMetVarsSource->setInputVariable(
                        derivedVarsMapping.at(0), derivedVarsMapping.at(1));
        }
    }

    MStructuredGridEnsembleFilter *ensFilterDerived =
            new MStructuredGridEnsembleFilter();
    ensFilterDerived->setMemoryManager(memoryManager);
    ensFilterDerived->setScheduler(scheduler);

    if (!enableRegridding)
    {
        ensFilterDerived->setInputSource(derivedMetVarsSource);
    }
    else
    {
        MStructuredGridEnsembleFilter *ensFilter1Derived =
                new MStructuredGridEnsembleFilter();
        ensFilter1Derived->setMemoryManager(memoryManager);
        ensFilter1Derived->setScheduler(scheduler);
        ensFilter1Derived->setInputSource(derivedMetVarsSource);

        MVerticalRegridder *regridderEPSDerived =
                new MVerticalRegridder();
        regridderEPSDerived->setMemoryManager(memoryManager);
        regridderEPSDerived->setScheduler(scheduler);
        regridderEPSDerived->setInputSource(ensFilter1Derived);

        ensFilterDerived->setInputSource(regridderEPSDerived);
    }

    sysMC->registerDataSource(dataSourceIdDerived + QString(" ENSFilter"),
                              ensFilterDerived);

    if (enableProbabiltyRegionFilter)
    {
        MProbabilityRegionDetectorFilter *probRegDetectorNWPDerived =
                new MProbabilityRegionDetectorFilter();
        probRegDetectorNWPDerived->setMemoryManager(memoryManager);
        probRegDetectorNWPDerived->setScheduler(scheduler);
        probRegDetectorNWPDerived->setInputSource(ensFilterDerived);

        sysMC->registerDataSource(dataSourceIdDerived + QString(" ProbReg"),
                                  probRegDetectorNWPDerived);
    }

    LOG4CPLUS_DEBUG(mlog, "Pipeline ''" << dataSourceId.toStdString()
                    << "'' has been initialized.");
}


void MPipelineConfiguration::initializePrecomputedTrajectoriesPipeline(
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
    LOG4CPLUS_DEBUG(mlog,
                    "Initializing precomputed ensemble trajectories pipeline ''"
                    << dataSourceId.toStdString() << "'' ...");

    // Trajectory reader.
    MTrajectoryReader *trajectoryReader =
            new MTrajectoryReader(dataSourceId);
    trajectoryReader->setMemoryManager(memoryManager);
    trajectoryReader->setScheduler(scheduler);
    trajectoryReader->setDataRoot(fileDir, "*");
    sysMC->registerDataSource(dataSourceId + QString(" Reader"),
                              trajectoryReader);

    // Initialize trajectory pipeline.
    initializeEnsembleTrajectoriesPipeline(
                dataSourceId, boundaryLayerTrajectories,
                trajectoryReader, scheduler, memoryManager);

    LOG4CPLUS_DEBUG(mlog, "Pipeline ''" << dataSourceId.toStdString()
                    << "'' has been initialized.");
}


void MPipelineConfiguration::initializeTrajectoryComputationPipeline(
        QString name,
        bool boundaryLayerTrajectories,
        QString schedulerID,
        QString memoryManagerID,
        QString NWPDataset,
        QString windEastwardVariable,
        QString windNorthwardVariable,
        QString windVerticalVariable,
        MVerticalLevelType verticalLevelType)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MAbstractScheduler* scheduler = sysMC->getScheduler(schedulerID);
    MAbstractMemoryManager* memoryManager =
            sysMC->getMemoryManager(memoryManagerID);

    const QString dataSourceId = name;
    LOG4CPLUS_DEBUG(mlog, "Initializing trajectory computation pipeline ''"
                          "" << dataSourceId.toStdString() << "'' ...");

    MWeatherPredictionDataSource* NWPDataSource =
            dynamic_cast<MWeatherPredictionDataSource*>(
                sysMC->getDataSource(NWPDataset));
    if (!NWPDataSource)
    {
        LOG4CPLUS_WARN(mlog, "MWeatherPredictionDataSource ''"
                       << NWPDataset.toStdString()
                       << "'' is invalid; skipping.");
        return;
    }

    // If verical level type is not given, search for it.
    if (verticalLevelType == MVerticalLevelType::SIZE_LEVELTYPES)
    {
        QList<MVerticalLevelType> levelTypes =
                NWPDataSource->availableLevelTypes();
        foreach (MVerticalLevelType level, levelTypes)
        {
            QStringList variables = NWPDataSource->availableVariables(level);
            if (variables.contains(windEastwardVariable)
                    && variables.contains(windNorthwardVariable)
                    && variables.contains(windVerticalVariable))
            {
                verticalLevelType = level;
            }
        }
    }
    else
    {
        QList<MVerticalLevelType> levelTypes =
                NWPDataSource->availableLevelTypes();
        if (!levelTypes.contains(verticalLevelType))
        {
            LOG4CPLUS_WARN(mlog, "MWeatherPredictionDataSource ''"
                           << NWPDataset.toStdString()
                           << "'' does NOT contain level type '"
                           << MStructuredGrid::verticalLevelTypeToString(
                               verticalLevelType).toStdString()
                           << "'; skipping.");
            return;
        }
        QStringList variables = NWPDataSource->availableVariables(
                    verticalLevelType);
        if (!variables.contains(windEastwardVariable)
                || !variables.contains(windNorthwardVariable)
                || !variables.contains(windVerticalVariable))
        {
            LOG4CPLUS_WARN(mlog, "MWeatherPredictionDataSource ''"
                           << NWPDataset.toStdString()
                           << "'' does NOT contain all wind component variables"
                              " with vertical level type '"
                           << MStructuredGrid::verticalLevelTypeToString(
                               verticalLevelType).toStdString()
                           << "'; skipping.");
            return;
        }
    }

    if (MClimateForecastReader *netCDFDataSource =
            dynamic_cast<MClimateForecastReader*>(NWPDataSource))
    {
        MHorizontalGridType hGridTypU =
                netCDFDataSource->variableHorizontalGridType(
                    verticalLevelType, windEastwardVariable);
        MHorizontalGridType hGridTypV =
                netCDFDataSource->variableHorizontalGridType(
                    verticalLevelType, windNorthwardVariable);
        MHorizontalGridType hGridTypW =
                netCDFDataSource->variableHorizontalGridType(
                    verticalLevelType, windVerticalVariable);
        if (hGridTypU == MHorizontalGridType::ROTATED_LONLAT
                || hGridTypV == MHorizontalGridType::ROTATED_LONLAT
                || hGridTypW == MHorizontalGridType::ROTATED_LONLAT)
        {
            LOG4CPLUS_WARN(mlog, "One or more wind variables are defined on"
                                 " a rotated grid coordinates; skipping.");
            return;
        }
    }

    MTrajectoryComputationSource* trajectoryComputation =
            new MTrajectoryComputationSource(dataSourceId);
    trajectoryComputation->setMemoryManager(memoryManager);
    trajectoryComputation->setScheduler(scheduler);
    trajectoryComputation->setInputWindVariables(windEastwardVariable,
                                                 windNorthwardVariable,
                                                 windVerticalVariable);

    trajectoryComputation-> setVerticalLevelType(verticalLevelType);
    trajectoryComputation->setInputSource(NWPDataSource);
    sysMC->registerDataSource(dataSourceId + QString(" Reader"),
                              trajectoryComputation);

    // Initialize trajectory pipeline.
    initializeEnsembleTrajectoriesPipeline(
                dataSourceId, boundaryLayerTrajectories,
                trajectoryComputation, scheduler, memoryManager);

    LOG4CPLUS_DEBUG(mlog, "Pipeline ''" << dataSourceId.toStdString()
                    << "'' has been initialized.");
}


void MPipelineConfiguration::initializeEnsembleTrajectoriesPipeline(
        QString dataSourceId,
        bool boundaryLayerTrajectories,
        MTrajectoryDataSource* baseDataSource,
        MAbstractScheduler* scheduler,
        MAbstractMemoryManager* memoryManager)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();

    MDeltaPressurePerTrajectorySource *dpSource =
            new MDeltaPressurePerTrajectorySource();
    dpSource->setMemoryManager(memoryManager);
    dpSource->setScheduler(scheduler);
    dpSource->setTrajectorySource(baseDataSource);

    MThinOutTrajectoryFilter *thinoutFilter =
            new MThinOutTrajectoryFilter();
    thinoutFilter->setMemoryManager(memoryManager);
    thinoutFilter->setScheduler(scheduler);
    thinoutFilter->setTrajectorySource(baseDataSource);

    MPressureTimeTrajectoryFilter *dpdtFilter =
            new MPressureTimeTrajectoryFilter();
    dpdtFilter->setMemoryManager(memoryManager);
    dpdtFilter->setScheduler(scheduler);
    dpdtFilter->setInputSelectionSource(thinoutFilter);
    dpdtFilter->setDeltaPressureSource(dpSource);

    MBoundingBoxTrajectoryFilter *bboxFilter =
            new MBoundingBoxTrajectoryFilter();
    bboxFilter->setMemoryManager(memoryManager);
    bboxFilter->setScheduler(scheduler);
    bboxFilter->setInputSelectionSource(dpdtFilter);
    bboxFilter->setTrajectorySource(baseDataSource);

    MSingleTimeTrajectoryFilter *timestepFilter =
            new MSingleTimeTrajectoryFilter();
    timestepFilter->setMemoryManager(memoryManager);
    timestepFilter->setScheduler(scheduler);
    timestepFilter->setInputSelectionSource(bboxFilter);
    sysMC->registerDataSource(dataSourceId + QString(" timestepFilter"),
                              timestepFilter);

    MTrajectoryNormalsSource *trajectoryNormals =
            new MTrajectoryNormalsSource();
    trajectoryNormals->setMemoryManager(memoryManager);
    trajectoryNormals->setScheduler(scheduler);
    trajectoryNormals->setTrajectorySource(baseDataSource);
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
        source->setTrajectorySource(baseDataSource);
        source->setInputSelectionSource(timestepFilter);

        pwcbSource = source;
    }
    else
    {
        MProbDFTrajectoriesSource* source =
                new MProbDFTrajectoriesSource();
        source->setMemoryManager(memoryManager);
        source->setScheduler(scheduler);
        source->setTrajectorySource(baseDataSource);
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
}


void MPipelineConfiguration::initializeConfigurablePipeline(
        MConfigurablePipelineType pipelineType,
        QString name,
        QString inputSource0,
        QString inputSource1,
        QString baseRequest0,
        QString baseRequest1,
        QString schedulerID,
        QString memoryManagerID,
        bool enableRegridding)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MAbstractScheduler* scheduler = sysMC->getScheduler(schedulerID);
    MAbstractMemoryManager* memoryManager = sysMC->getMemoryManager(memoryManagerID);

    const QString dataSourceId = name;
    LOG4CPLUS_DEBUG(mlog, "Initializing configurable pipeline ''"
                    << dataSourceId.toStdString() << "'' ...");

    switch (pipelineType)
    {
    case DIFFERENCE:
    {
        // Pipeline for difference variables.
        // ==================================
        const QString dataSourceIdDifference = dataSourceId;

        MDifferenceDataSource *differenceSource = new MDifferenceDataSource();
        differenceSource->setMemoryManager(memoryManager);
        differenceSource->setScheduler(scheduler);

        differenceSource->setInputSource(
                    0, dynamic_cast<MWeatherPredictionDataSource*>(
                        sysMC->getDataSource(inputSource0)));
        differenceSource->setInputSource(
                    1, dynamic_cast<MWeatherPredictionDataSource*>(
                        sysMC->getDataSource(inputSource1)));

        differenceSource->setBaseRequest(0, baseRequest0);
        differenceSource->setBaseRequest(1, baseRequest1);

        MStructuredGridEnsembleFilter *ensFilterDifference =
                new MStructuredGridEnsembleFilter();
        ensFilterDifference->setMemoryManager(memoryManager);
        ensFilterDifference->setScheduler(scheduler);

        if (!enableRegridding)
        {
            ensFilterDifference->setInputSource(differenceSource);
        }
        else
        {
            MStructuredGridEnsembleFilter *ensFilter1Difference =
                    new MStructuredGridEnsembleFilter();
            ensFilter1Difference->setMemoryManager(memoryManager);
            ensFilter1Difference->setScheduler(scheduler);
            ensFilter1Difference->setInputSource(differenceSource);

            MVerticalRegridder *regridderEPSDerived =
                    new MVerticalRegridder();
            regridderEPSDerived->setMemoryManager(memoryManager);
            regridderEPSDerived->setScheduler(scheduler);
            regridderEPSDerived->setInputSource(ensFilter1Difference);

            ensFilterDifference->setInputSource(regridderEPSDerived);
        }

        sysMC->registerDataSource(dataSourceIdDifference + QString(" ENSFilter"),
                                  ensFilterDifference);
        break;
    }
    default:
    {
        LOG4CPLUS_ERROR(mlog,
                        "Invalid configurable pipeline type. Could not"
                        " initialize pipeline ''" << dataSourceId.toStdString()
                        << "''.");
        return;
    }
    }

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

    initializeNWPPipeline(
                "ECMWF DET EUR_LL015",
                "/home/local/data/mss/grid/ecmwf/netcdf",
                "*ecmwf_forecast*EUR_LL015*.nc",
                "SingleThread",
                "NWP",
                CF_NETCDF,
                false,
                true,
                false,
                "auto",
                false,
                "",
                false,
                "");

    initializeNWPPipeline(
                "ECMWF ENS EUR_LL10",
                "/home/local/data/mss/grid/ecmwf/netcdf",
                "*ecmwf_ensemble_forecast*EUR_LL10*.nc",
                "SingleThread",
                "NWP",
                CF_NETCDF,
                false,
                true,
                false,
                "auto",
                false,
                "",
                false,
                "");

    sysMC->registerMemoryManager("Trajectories DF-T psfc_1000hPa_L62",
               new MLRUMemoryManager("Trajectories DF-T psfc_1000hPa_L62",
                                     10.*1024.*1024.));

    initializePrecomputedTrajectoriesPipeline(
                "Lagranto ENS EUR_LL10 DF-T psfc_1000hPa_L62",
                "/mnt/ssd/data/trajectories/EUR_LL10/psfc_1000hPa_L62",
                false,
                "SingleThread",
                "Trajectories DF-T psfc_1000hPa_L62");

    sysMC->registerMemoryManager("Trajectories DF-T psfc_min_L62",
               new MLRUMemoryManager("Trajectories  DF-T psfc_min_L62",
                                     12.*1024.*1024.));

    initializePrecomputedTrajectoriesPipeline(
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
    initializePrecomputedTrajectoriesPipeline(
                "Lagranto ENS EUR_LL10 ABL-T psfc_min_L62_abl",
                "/mnt/ssd/data/trajectories/EUR_LL10/psfc_min_L62_abl",
                true,
                "SingleThread",
                "Trajectories ABL-T psfc_min_L62_abl");

    sysMC->registerMemoryManager("Trajectories ABL-T 10hPa",
               new MLRUMemoryManager("Trajectories ABL-T 10hPa",
                                     10.*1024.*1024.));
    initializePrecomputedTrajectoriesPipeline(
                "Lagranto ENS EUR_LL10 ABL-T 10hPa",
                "/mnt/ssd/data/trajectories/EUR_LL10/blt_PL10hPa",
                true,
                "SingleThread",
                "Trajectories ABL-T 10hPa");
}


void MPipelineConfiguration::getMetviewGribFilePaths(
        QList<MPipelineConfiguration::MetviewGribFilePath> *gribFilePaths)
{
    gribFilePaths->clear();
    QStringList gribFilePathsStringList;
    gribFilePathsStringList.clear();
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    // Scan global application command line arguments for metview definition.
    foreach (QString arg, sysMC->getApplicationCommandLineArguments())
    {
        if (arg.startsWith("--path="))
        {
            QString path = arg.remove("--path=");
            // Remove quotes if not already removed by shell.
            path.remove(QChar('"'), Qt::CaseSensitive);
            // Get list of paths (directory and file filter).
            gribFilePathsStringList = path.split(";", QString::SkipEmptyParts);
            // Extract directory and file filter from given paths.
            foreach (QString path, gribFilePathsStringList)
            {
                QFileInfo fileInfo(expandEnvironmentVariables(path));
                QString fileFilter = fileInfo.fileName();
                path.chop(fileFilter.length());
                MetviewGribFilePath metviewGribFilePath;
                metviewGribFilePath.path = path;
                metviewGribFilePath.fileFilter = fileFilter;
                gribFilePaths->append(metviewGribFilePath);
            }
            break;
        }
    }
}


MPipelineConfiguration::MConfigurablePipelineType
MPipelineConfiguration::configurablePipelineTypeFromString(QString typeName)
{
    if (typeName == "DIFFERENCE")
    {
        return MConfigurablePipelineType::DIFFERENCE;
    }
    else
    {
        return MConfigurablePipelineType::INVALID_PIPELINE_TYPE;
    }
}


void MPipelineConfiguration::checkAndStoreDefaultPipelineMemoryManager(
        QString defaultMemoryManager, QString PipelineID,
        QMap<QString, QString> *defaultMemoryManagers,
        MSystemManagerAndControl *sysMC)
{
    if (defaultMemoryManager.isEmpty())
    {
        defaultMemoryManager = sysMC->getMemoryManagerIdentifiers().first();

        LOG4CPLUS_WARN(mlog,
                       "No memory manager set as default for '"
                       << PipelineID.toStdString() << "' pipeline.");
    }
    else
    {
        if (!sysMC->getMemoryManagerIdentifiers().contains(defaultMemoryManager))
        {
            defaultMemoryManager = sysMC->getMemoryManagerIdentifiers().first();

            LOG4CPLUS_WARN(mlog,
                           "Memory manager '"
                           << defaultMemoryManager.toStdString()
                           << "' is set as default for '"
                           << PipelineID.toStdString()
                           << "' pipeline but it does not exist.");
        }
    }
    if (!defaultMemoryManager.isEmpty())
    {
        LOG4CPLUS_DEBUG(mlog,
                        "Using '" << defaultMemoryManager.toStdString()
                        << "' as default memory manager for '"
                        << PipelineID.toStdString() << "' pipeline.");
    }
    defaultMemoryManagers->insert(PipelineID, defaultMemoryManager);
}


} // namespace Met3D
