/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2016      Bianca Tost
**  Copyright 2017      Michael Kern
**  Copyright 2015-2016 Christoph Heidelmann
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
#include "applicationconfiguration.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "gxfw/mglresourcesmanager.h"

#include "system/pipelineconfiguration.h"
#include "system/frontendconfiguration.h"
#include "system/developmentaidsconfiguration.h"

#include "actors/basemapactor.h"
#include "actors/graticuleactor.h"
#include "actors/volumebboxactor.h"
#include "actors/transferfunction1d.h"
#include "actors/movablepoleactor.h"
#include "actors/trajectoryactor.h"
#include "actors/nwphorizontalsectionactor.h"
#include "actors/nwpverticalsectionactor.h"
#include "actors/nwpvolumeraycasteractor.h"
#include "actors/nwpsurfacetopographyactor.h"
#include "actors/spatial1dtransferfunction.h"
#include "jetcores/isosurfaceintersectionactor.h"
#include "jetcores/jetcoredetectionactor.h"
#include "actors/skewtactor.h"

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MApplicationConfigurationManager::MApplicationConfigurationManager()
{
    registerActorFactories();
    registerApplicationConfigurations();
}


MApplicationConfigurationManager::~MApplicationConfigurationManager()
{
    foreach (MAbstractApplicationConfiguration* appConfig, appConfigs)
    {
        delete appConfig;
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MApplicationConfigurationManager::loadConfiguration()
{
    foreach (MAbstractApplicationConfiguration* appConfig, appConfigs)
    {
        appConfig->configure();
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MApplicationConfigurationManager::registerActorFactories()
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    glRM->registerActorFactory(new MBaseMapActorFactory());
    glRM->registerActorFactory(new MVolumeBoundingBoxActorFactory());
    glRM->registerActorFactory(new MGraticuleActorFactory());
    glRM->registerActorFactory(new MPressurePoleActorFactory());
    glRM->registerActorFactory(new MNWPHorizontalSectionActorFactory());
    glRM->registerActorFactory(new MNWPVerticalSectionActorFactory());
    glRM->registerActorFactory(new MNWPSurfaceTopographyActorFactory());
    glRM->registerActorFactory(new MNWPVolumeRaycasterActorFactory());
    glRM->registerActorFactory(new MTransferFunction1DFactory());
    glRM->registerActorFactory(new MJetcoreDetectionActorFactory());
    glRM->registerActorFactory(new MIsosurfaceIntersectionActorFactory());
    glRM->registerActorFactory(new MSpatial1DTransferFunctionFactory());
    glRM->registerActorFactory(new MTrajectoryActorFactory());
    glRM->registerActorFactory(new MSkewTActorFactory());
}


void MApplicationConfigurationManager::registerApplicationConfigurations()
{
    appConfigs << new MPipelineConfiguration();
    appConfigs << new MFrontendConfiguration();
    appConfigs << new MDevelopmentAidsConfiguration();
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


} // namespace Met3D
