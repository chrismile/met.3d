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
#include "weatherpredictionreader.h"

// standard library imports
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"


namespace Met3D
{


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MWeatherPredictionReader::MWeatherPredictionReader(QString identifier)
    : MWeatherPredictionDataSource(),
      MAbstractDataReader(identifier)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MStructuredGrid* MWeatherPredictionReader::produceData(MDataRequest request)
{
    MDataRequestHelper rh(request);

    MVerticalLevelType levtype = MVerticalLevelType(rh.intValue("LEVELTYPE"));
    QString variable           = rh.value("VARIABLE");
    QDateTime initTime         = rh.timeValue("INIT_TIME");
    QDateTime validTime        = rh.timeValue("VALID_TIME");
    unsigned int member        = rh.intValue("MEMBER");

    if ((levtype == HYBRID_SIGMA_PRESSURE_3D) && (variable.endsWith("/PSFC")))
    {
        // Special request ("/PSFC" has been appended to the name of a hybrid
        // variable): Return the surface pressure field instead of the variable
        // field.
        variable = variableSurfacePressureName(levtype,
                                               variable.replace("/PSFC", ""));
        levtype  = SURFACE_2D;
    }

    MStructuredGrid* result = readGrid(levtype, variable, initTime,
                                       validTime, member);

    if (levtype == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Special treatment for hybrid model levels: Also load the required
        // surface pressure field and set a link to it.
        QString psfcVar = variableSurfacePressureName(levtype, variable);
        rh.insert("LEVELTYPE", SURFACE_2D);
        rh.insert("VARIABLE", psfcVar);

        MDataRequest psfcRequest = rh.request();
        if ( !memoryManager->containsData(this, psfcRequest) )
        {
            // Data field needs to be loaded from disk.
            MRegularLonLatGrid *psfc = static_cast<MRegularLonLatGrid*>(
                        readGrid(SURFACE_2D, psfcVar, initTime,
                                 validTime, member)
                        );
            psfc->setGeneratingRequest(psfcRequest);
            if ( !memoryManager->storeData(this, psfc) )
            {
                // In rare cases another thread could have generated and stored
                // the same data field in the mean time. In such a case the
                // store request will fail. Delete the psfc object, it is not
                // required anymore.
                delete psfc;
            }
        }

        // Get a pointer to the surface pressure field from the memory manger.
        // The field's reference counter was increased by either containsData()
        // or storeData() above. NOTE: The field is released in the destructor
        // of "result" -- the reference is kept for the entire lifetime of
        // "result" to make sure the psfc field is not deleted while "result"
        // is still in memory (notes 09Oct2013).
        static_cast<MLonLatHybridSigmaPressureGrid*>(
                    result
                    )->surfacePressure =
                static_cast<MRegularLonLatGrid*>(
                    memoryManager->getData(this, psfcRequest)
                    );
    }

    return result;
}


MTask* MWeatherPredictionReader::createTaskGraph(MDataRequest request)
{
    // No dependencies, so we create a plain task.
    MTask* task = new MTask(request, this);
    // However, this task accesses the hard drive.
    task->setDiskReaderTask();
    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MWeatherPredictionReader::locallyRequiredKeys()
{
    return (QStringList() << "LEVELTYPE" << "VARIABLE" << "INIT_TIME"
            << "VALID_TIME" << "MEMBER");
}

} // namespace Met3D
