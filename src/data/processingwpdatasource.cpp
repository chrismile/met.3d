/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2018 Bianca Tost
**  Copyright 2018 Marc Rautenhaus
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
#include "processingwpdatasource.h"

// standard library imports
#include <iostream>
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"


using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MProcessingWeatherPredictionDataSource::MProcessingWeatherPredictionDataSource()
    : MWeatherPredictionDataSource()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

MStructuredGrid*
MProcessingWeatherPredictionDataSource::createAndInitializeResultGrid(
        MStructuredGrid *templateGrid)
{
    MStructuredGrid *result = nullptr;

    switch (templateGrid->leveltype)
    {
    case PRESSURE_LEVELS_3D:
        result = new MRegularLonLatStructuredPressureGrid(
                    templateGrid->nlevs, templateGrid->nlats,
                    templateGrid->nlons);
        break;
    case HYBRID_SIGMA_PRESSURE_3D:
        result = new MLonLatHybridSigmaPressureGrid(
                    templateGrid->nlevs, templateGrid->nlats,
                    templateGrid->nlons);
        break;
    case AUXILIARY_PRESSURE_3D:
    {
        MLonLatAuxiliaryPressureGrid *templateGridAux =
                dynamic_cast<MLonLatAuxiliaryPressureGrid*>(templateGrid);
        result = new MLonLatAuxiliaryPressureGrid(
                    templateGrid->nlevs, templateGrid->nlats,
                    templateGrid->nlons, templateGridAux->reverseLevels);
        break;
    }
    case POTENTIAL_VORTICITY_2D:
        break;
    case SURFACE_2D:
        result = new MRegularLonLatGrid(templateGrid->nlats,
                                        templateGrid->nlons);
        break;
    case LOG_PRESSURE_LEVELS_3D:
        result = new MRegularLonLatLnPGrid(
                    templateGrid->nlevs, templateGrid->nlats,
                    templateGrid->nlons);
        break;
    default:
        break;
    }

    if (result == nullptr)
    {
        QString msg = QString("ERROR: Cannot intialize result grid. Level "
                              "type %1 not implemented.")
                .arg(MStructuredGrid::verticalLevelTypeToString(
                         templateGrid->leveltype));
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }

    // Copy coordinate axes.
    for (unsigned int i = 0; i < templateGrid->nlons; i++)
        result->lons[i] = templateGrid->lons[i];
    for (unsigned int j = 0; j < templateGrid->nlats; j++)
        result->lats[j] = templateGrid->lats[j];
    for (unsigned int i = 0; i < templateGrid->nlevs; i++)
        result->levels[i] = templateGrid->levels[i];

    result->setAvailableMembers(templateGrid->getAvailableMembers());

    if (templateGrid->leveltype == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Special treatment for hybrid model levels: copy ak/bk coeffs.
        MLonLatHybridSigmaPressureGrid *hybtemplate =
                dynamic_cast<MLonLatHybridSigmaPressureGrid*>(templateGrid);
        MLonLatHybridSigmaPressureGrid *hybresult =
                dynamic_cast<MLonLatHybridSigmaPressureGrid*>(result);
        for (unsigned int i = 0; i < hybtemplate->nlevs; i++)
        {
            hybresult->ak_hPa[i] = hybtemplate->ak_hPa[i];
            hybresult->bk[i] = hybtemplate->bk[i];
        }

        // Take care of the surface grid: take the surface grid of the template
        // grid.
        hybresult->surfacePressure = hybtemplate->getSurfacePressureGrid();

        // Increase the reference counter for this field (as done above by
        // containsData() or storeData()). NOTE: The field is released in
        // the destructor of "result" -- the reference is kept for the
        // entire lifetime of "result" to make sure the psfc field is not
        // deleted while "result" is still in memory.
        if ( !hybresult->surfacePressure->increaseReferenceCounter() )
        {
            // This should not happen.
            QString msg = QString("This is embarrassing: The data item "
                                  "for request %1 should have been in "
                                  "cache.").arg(
                        hybresult->surfacePressure->getGeneratingRequest());
            throw MMemoryError(msg.toStdString(), __FILE__, __LINE__);
        }
    } // grid is hybrid sigma pressure

    return result;
}


} // namespace Met3D
