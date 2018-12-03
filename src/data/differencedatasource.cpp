/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2018 Marc Rautenhaus [*, previously +]
**  Copyright 2018 Bianca Tost [+]
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
    : MProcessingWeatherPredictionDataSource()
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
    } // inputGrids now contains two entries; if sth went wrong they are nullptr

    MStructuredGrid *differenceGrid = nullptr;

    // If all inputs are available and valid, compute the difference
    // input(0) - input(1). If inputs are missing (i.e., they are nullptr),
    // a nullptr is returned.
//TODO (mr, 27Nov2018) -- if the upstream pipeline knows about the available
// times that can be requested no "nullptr" cases should occure, see
// availableValidTimes(). Needs to be implemented.
    if ( inputGrids.at(0) != nullptr && inputGrids.at(1) != nullptr )
    {
        differenceGrid = createAndInitializeResultGrid(inputGrids.at(0));

        QString diffVariableName = QString("difference %1 - %2").arg(
                    inputGrids.at(0)->variableName).arg(
                    inputGrids.at(1)->variableName);
        differenceGrid->setMetaData(inputGrids.at(0)->getInitTime(),
                                    inputGrids.at(0)->getValidTime(),
                                    diffVariableName,
                                    inputGrids.at(0)->getEnsembleMember());

        for (unsigned int k = 0; k < differenceGrid->getNumLevels() ; k++)
            for (unsigned int j = 0; j < differenceGrid->getNumLats() ; j++)
                for (unsigned int i = 0; i < differenceGrid->getNumLons() ; i++)
                {
                    float valueGrid0 = inputGrids.at(0)->getValue(k, j, i);
                    if (valueGrid0 == M_MISSING_VALUE)
                    {
                        differenceGrid->setValue(k, j, i, M_MISSING_VALUE);
                        continue;
                    }
                    float valueGrid1 = inputGrids.at(1)->interpolateValue(
                                differenceGrid->getLons()[i],
                                differenceGrid->getLats()[j],
                                differenceGrid->getPressure(k, j, i));
                    if (valueGrid1 != M_MISSING_VALUE)
                    {
                        differenceGrid->setValue(k, j, i, valueGrid0 - valueGrid1);
                        continue;
                    }
                    differenceGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
    }

    // Release input fields.
    for (int i = 0; i <= 1; i++)
    {
        if (inputGrids[i] == nullptr) continue;
        inputSource[i]->releaseData(inputGrids[i]);
    }

    return differenceGrid; // if inputs have not been available this is a nullptr
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


QString MDifferenceDataSource::variableLongName(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    assert(inputSource[0] != nullptr);
    return inputSource[0]->variableLongName(levelType, variableName);
}


QString MDifferenceDataSource::variableStandardName(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    assert(inputSource[0] != nullptr);
    return inputSource[0]->variableStandardName(levelType, variableName);
}


QString MDifferenceDataSource::variableUnits(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    assert(inputSource[0] != nullptr);
    return inputSource[0]->variableUnits(levelType, variableName);
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
        else if (val == "SPECIALCASE_DATE_INIT_TIME_VALID")
        {
            // Special case: Use the "data" part of the INIT_TIME and the
            // "time" part of the VALID_TIME. This is useful, e.g., to compute
            // a forecast bias: subtract an average analysis for a given
            // time of the day from an average lead time.
//TODO (mr, 26Nov2018) -- This should be replaced by a more flexible framework
// to do computations on the data. Maybe with Python?
            QDateTime initTime = rh.timeValue("INIT_TIME");
            QDateTime validTime = rh.timeValue("VALID_TIME");
            initTime.setTime(validTime.time());
            rhInp.insert(requiredKey, initTime);
        }
    }

    return rhInp.request();
}


} // namespace Met3D
