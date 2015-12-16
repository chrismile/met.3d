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
#include "thermodatasource.h"

// standard library imports
#include <iostream>
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MThermodynamicsDataSource::MThermodynamicsDataSource()
    : MWeatherPredictionDataSource(),
      inputSource(nullptr)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MThermodynamicsDataSource::setInputSource(MWeatherPredictionDataSource* s)
{
    inputSource = s;
    registerInputSource(inputSource);
//    enablePassThrough(s);
}


MStructuredGrid* MThermodynamicsDataSource::produceData(MDataRequest request)
{
    assert(inputSource != nullptr);

    MDataRequestHelper rh(request);

    // Parse request.
    QString regridMode = rh.value("REGRID");
    QStringList params = regridMode.split("/");
    rh.removeAll(locallyRequiredKeys());

    MLonLatHybridSigmaPressureGrid *inputGrid =
            dynamic_cast<MLonLatHybridSigmaPressureGrid*>(
                inputSource->getData(rh.request()));

    if (!inputGrid) return nullptr;

    MStructuredGrid *regriddedField = nullptr;

//TODO: implement functionality ...

    return regriddedField;
}


MTask* MThermodynamicsDataSource::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

//TODO: implement functionality ...

    return task;
}


QList<MVerticalLevelType> MThermodynamicsDataSource::availableLevelTypes()
{
    assert(inputSource != nullptr);
    return inputSource->availableLevelTypes();
}


QStringList MThermodynamicsDataSource::availableVariables(
        MVerticalLevelType levelType)
{
    assert(inputSource != nullptr);
    return inputSource->availableVariables(levelType);
}


QSet<unsigned int> MThermodynamicsDataSource::availableEnsembleMembers(
        MVerticalLevelType levelType, const QString& variableName)
{
    assert(inputSource != nullptr);
    return inputSource->availableEnsembleMembers(levelType, variableName);
}


QList<QDateTime> MThermodynamicsDataSource::availableInitTimes(
        MVerticalLevelType levelType, const QString& variableName)
{
    assert(inputSource != nullptr);
    return inputSource->availableInitTimes(levelType, variableName);
}


QList<QDateTime> MThermodynamicsDataSource::availableValidTimes(
        MVerticalLevelType levelType,
        const QString& variableName, const QDateTime& initTime)
{
    assert(inputSource != nullptr);
    return inputSource->availableValidTimes(levelType, variableName, initTime);
}



/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MThermodynamicsDataSource::locallyRequiredKeys()
{
    return (QStringList() << "VARIABLE");
}

} // namespace Met3D
