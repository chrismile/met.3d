/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2017-2018 Bianca Tost [+]
**  Copyright 2017      Philipp Kaiser [+]
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
#ifndef MDEFAULTPIPELINECONFIGURATION_H
#define MDEFAULTPIPELINECONFIGURATION_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "system/applicationconfiguration.h"
#include "data/trajectorydatasource.h"
#include "data/structuredgrid.h"
#include "mainwindow.h"


namespace Met3D
{

/**
  @brief MPipelineConfiguration initializes the Met.3D data pipeline. A number
  of predefined pipelines are available (currently for NetCDF-CF and GRIB data,
  and for TRAJECTORY data. Pipeline parameters are read from a configuration
  file.

  Special case: If Met.3D is called with command line argument "--metview",
  it uses directory paths and file filters given by the command line argument
  "--path=" instead of the ones defined in the configuration file. Each
  directory file filter pairing results in its own data source and must be
  separated in the path argument by a semicolon from other paths. For file
  filters Met.3D supports the use of wildcard expressions. If no configuration
  file is given via the command line, in this mode Met.3D uses a default
  configuration file stored at $MET3D_HOME/config/metview/default_pipeline.cfg
  if present. To configure the NWPPipeline data sources Met.3D uses only the
  first entry of NWPPipeline in the pipeline configuration file and append
  "_index" to the name with index being a integer incremented for each data
  source by one starting from zero.

  Example for path arguement: -\-path="path/to/filefilter1;path/to/filefilter2".
  [The quotation marks are mendatory since some shells use semicolons as one
   possible seperator.]
  */
class MPipelineConfiguration : public MAbstractApplicationConfiguration
{
public:
    MPipelineConfiguration();

    void configure();

protected:
    // Friend class MMainWindow so that its method addDataset() can call
    // initializeNWPPipeline() from this class.
    friend class MMainWindow;

    enum MNWPReaderFileFormat
    {
        INVALID_FORMAT  = 0,
        CF_NETCDF       = 1,
        ECMWF_GRIB      = 2
    };

    enum MConfigurablePipelineType
    {
        INVALID_PIPELINE_TYPE  = 0,
        DIFFERENCE       = 1
    };

    /**
     Represents one directory path and file filter passed to Met.3D by Metview
     via path command line argument.
     */
    struct MetviewGribFilePath
    {
        MetviewGribFilePath() {}
        QString path;
        QString fileFilter;
    };

    /**
      Initializes the default scheduler (required for the pipelines to execute
      the generated task graphs).
     */
    void initializeScheduler();

    /**
      Loads a pipeline configuration from file. Can only read parameters
      for the predefined pipelines.

      @see initializeNWPPipeline()
      @see initializeLagrantoEnsemblePipeline()
     */
    void initializeDataPipelineFromConfigFile(QString filename);

    void initializeNWPPipeline(QString name,
                               QString fileDir,
                               QString fileFilter,
                               QString schedulerID,
                               QString memoryManagerID,
                               MNWPReaderFileFormat dataFormat,
                               bool enableRegridding,
                               bool enableProbabiltyRegionFilter,
                               bool treatRotatedGridAsRegularGrid,
                               bool treatProjectedGridAsRegularLonLatGrid,
                               QString surfacePressureFieldType,
                               bool convertGeometricHeightToPressure_ICAOStandard,
                               QString auxiliary3DPressureField,
                               bool disableGridConsistencyCheck,
                               QString inputVarsForDerivedVars);

    void initializePrecomputedTrajectoriesPipeline(
            QString name,
            QString fileDir,
            bool boundaryLayerTrajectories,
            QString schedulerID,
            QString memoryManagerID);

    void initializeConfigurablePipeline(MConfigurablePipelineType pipelineType,
                                        QString name,
                                        QString inputSource0,
                                        QString inputSource1,
                                        QString baseRequest0,
                                        QString baseRequest1,
                                        QString schedulerID,
                                        QString memoryManagerID,
                                        bool enableRegridding);

    void initializeTrajectoryComputationPipeline(
            QString name,
            bool boundaryLayerTrajectories,
            QString schedulerID,
            QString memoryManagerID,
            QString NWPDataset,
            QString windEastwardVariable,
            QString windNorthwardVariable,
            QString windVerticalVariable,
            QString auxDataVariableNames,
            MVerticalLevelType verticalLevelType);

    void initializeEnsembleTrajectoriesPipeline(
            QString dataSourceId,
            bool boundaryLayerTrajectories,
            MTrajectoryDataSource* baseDataSource,
            MAbstractScheduler* scheduler,
            MAbstractMemoryManager* memoryManager,
            bool trajectoriesComputedInMet3D);

    /**
     Initializes hard-coded pipelines. Use this method for development
     purposes.
     */
    void initializeDevelopmentDataPipeline();

    /**
     Extracts all paths and filefilters defined in the path command line
     argument and stores them in @p gribFilePaths.
     */
    void getMetviewGribFilePaths(QList<MetviewGribFilePath> *gribFilePaths);

    MConfigurablePipelineType configurablePipelineTypeFromString(QString typeName);

    /**
      Checks if the memory manager @p defaultMemoryManager exists and if so,
      registers it as default memory manager for the pipeline with ID
      @p PipelineID in @p defaultMemoryManagers.

      If @p defaultMemoryManager is empty or does not exists, the first entry
      of @ref MSystemManagerAndControl::getMemoryManagerIdentifiers() is used
      as default memory manager.
     */
    void checkAndStoreDefaultPipelineMemoryManager(
            QString defaultMemoryManager, QString PipelineID,
            QMap<QString, QString> *defaultMemoryManagers,
            MSystemManagerAndControl *sysMC);

    /**
      Checks if one of the name strings contained in @p dataSources matches the
      name of any already existing data source.

      If a match can be found, QMessageBox::warning is displayed to inform the
      user using the name of the data set given by @p dataSetName and the
      method returns false. If no match can be found no message is displayed
      and method returns true.
     */
    bool checkUniquenessOfDataSourceNames(QString dataSetName,
                                          QStringList &dataSources) const;
};

} // namespace Met3D

#endif // MDEFAULTPIPELINECONFIGURATION_H
