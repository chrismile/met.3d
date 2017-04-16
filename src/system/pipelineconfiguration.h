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
  and for LAGRANTO trajectory data. Pipeline parameters are read from a
  configuration file.
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

    void initializeNWPPipeline(
            QString name,
            QString fileDir,
            QString fileFilter,
            QString schedulerID,
            QString memoryManagerID,
            MNWPReaderFileFormat dataFormat,
            bool enableRegridding,
            bool enableProbabiltyRegionFilter);

    void initializeLagrantoEnsemblePipeline(
            QString name,
            QString fileDir,
            bool boundaryLayerTrajectories,
            QString schedulerID,
            QString memoryManagerID);

    void initializeComputationEnsemblePipeline(
            QString name,
            bool boundaryLayerTrajectories,
            QString schedulerID,
            QString memoryManagerID,
            QString resourceID,
            QString variableU,
            QString variableV,
            QString variableP);

    void initializeEnsemblePipeline(
            QString dataSourceId,
            bool boundaryLayerTrajectories,
            MTrajectoryDataSource* baseDataSource,
            MAbstractScheduler* scheduler,
            MAbstractMemoryManager* memoryManager);

    /**
     Initializes hard-coded pipelines. Use this method for development
     purposes.
     */
    void initializeDevelopmentDataPipeline();
};

} // namespace Met3D

#endif // MDEFAULTPIPELINECONFIGURATION_H
