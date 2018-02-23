/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
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
#include "differencedatasource.h"

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

MDifferenceDataSource::MDifferenceDataSource()
    : MWeatherPredictionDataSource()
{
    inputSource.resize(2);
    baseRequest.resize(2);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MDifferenceDataSource::setInputSource(int id, MWeatherPredictionDataSource* s)
{
    inputSource[id] = s;
    registerInputSource(inputSource[id]);
    //    enablePassThrough(s);
}


void MDifferenceDataSource::setBaseRequest(int id, MDataRequest request)
{
    baseRequest[id] = request;
}


MStructuredGrid* MDifferenceDataSource::produceData(MDataRequest request)
{
    assert(inputSource[0] != nullptr);
    assert(inputSource[1] != nullptr);

    // Get input fields.
    QList<MStructuredGrid*> inputGrids;
    for (int i = 0; i <= 1; i++)
    {
        inputGrids << inputSource[i]->getData(
                          constructInputSourceRequestFromRequest(i, request));
    }

    // Initialize result grid.
    MStructuredGrid *differenceGrid = nullptr;
    if ( !inputGrids.isEmpty() && inputGrids.at(0) != nullptr )
    {
        differenceGrid = createAndInitializeResultGrid(inputGrids.at(0));
    }
    else return nullptr;

    // Compute difference grid.
    for (unsigned int k = 0; k < differenceGrid->getNumLevels() ; k++)
        for (unsigned int j = 0; j < differenceGrid->getNumLats() ; j++)
            for (unsigned int i = 0; i < differenceGrid->getNumLons() ; i++)
            {
                float diff = inputGrids.at(0)->getValue(k, j, i)
                        - inputGrids.at(1)->interpolateValue(
                            differenceGrid->getLons()[i],
                            differenceGrid->getLats()[j],
                            differenceGrid->getPressure(k, j, i));

                differenceGrid->setValue(k, j, i, diff);
            }

    // Release input fields.
    for (int i = 0; i <= 1; i++)
    {
        inputSource[i]->releaseData(inputGrids[i]);
    }

    return differenceGrid;
}


MTask* MDifferenceDataSource::createTaskGraph(MDataRequest request)
{
    assert(inputSource[0] != nullptr);
    assert(inputSource[1] != nullptr);

    MTask *task = new MTask(request, this);

    task->addParent(inputSource[0]->getTaskGraph(
                constructInputSourceRequestFromRequest(0, request)));
    task->addParent(inputSource[1]->getTaskGraph(
                constructInputSourceRequestFromRequest(1, request)));

    return task;
}


QList<MVerticalLevelType> MDifferenceDataSource::availableLevelTypes()
{
//TODO (mr, 26Feb2018) -- see availableValidTimes()
    assert(inputSource[0] != nullptr);
    return inputSource[0]->availableLevelTypes();
}


QStringList MDifferenceDataSource::availableVariables(
        MVerticalLevelType levelType)
{
//TODO (mr, 26Feb2018) -- see availableValidTimes()
    assert(inputSource[0] != nullptr);
    return inputSource[0]->availableVariables(levelType);
}


QSet<unsigned int> MDifferenceDataSource::availableEnsembleMembers(
        MVerticalLevelType levelType, const QString& variableName)
{
//TODO (mr, 26Feb2018) -- see availableValidTimes()
    assert(inputSource[0] != nullptr);
    return inputSource[0]->availableEnsembleMembers(levelType, variableName);
}


QList<QDateTime> MDifferenceDataSource::availableInitTimes(
        MVerticalLevelType levelType, const QString& variableName)
{
//TODO (mr, 26Feb2018) -- see availableValidTimes()
    assert(inputSource[0] != nullptr);
    return inputSource[0]->availableInitTimes(levelType, variableName);
}


QList<QDateTime> MDifferenceDataSource::availableValidTimes(
        MVerticalLevelType levelType,
        const QString& variableName, const QDateTime& initTime)
{
//TODO (mr, 26Feb2018) -- needs to use values from both input sources, depending
// on further usage (i.e. mapping from requested to input times etc.).
    assert(inputSource[0] != nullptr);
    return inputSource[0]->availableValidTimes(levelType, variableName, initTime);
}



/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MDifferenceDataSource::locallyRequiredKeys()
{
    return (QStringList() << "LEVELTYPE" << "VARIABLE" << "INIT_TIME"
            << "VALID_TIME" << "MEMBER");
}


MDataRequest MDifferenceDataSource::constructInputSourceRequestFromRequest(
        int id, MDataRequest request)
{
    MDataRequestHelper rh(request); // request from "downstream" pipeline
    MDataRequestHelper rhInp(baseRequest[id]); // for input source "id"

    // baseRequest has format VARIABLE=Geopotential_height;MEMBER=0;
    // INIT_TIME=REQUESTED_VALID_TIME;VALID_TIME=REQUESTED_VALID_TIME;...

    foreach (QString requiredKey, rhInp.keys())
    {
        QString val = rhInp.value(requiredKey);
        if (val.startsWith("REQUESTED_"))
        {
            QString key = val.mid(10); // remove "REQUESTED_"
            rhInp.insert(requiredKey, rh.value(key)); // replace by value in "request"
        }
    }

    return rhInp.request();
}


MStructuredGrid* MDifferenceDataSource::createAndInitializeResultGrid(
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
        break;
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
