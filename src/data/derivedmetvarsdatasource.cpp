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
#include "derivedmetvarsdatasource.h"

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

MDerivedMetVarsDataSource::MDerivedMetVarsDataSource()
    : MStructuredGridEnsembleFilter(),
      inputSource(nullptr)
{
//TODO (mr, 2016May17) -- this table should be put into a configuration file
    requiredInputVariablesList["windspeed (an)"] =
            (QStringList() << "u (an)" << "v (an)");
    requiredInputVariablesList["windspeed (fc)"] =
            (QStringList() << "u (fc)" << "v (fc)");
    requiredInputVariablesList["windspeed (ens)"] =
            (QStringList() << "u (ens)" << "v (ens)");
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MDerivedMetVarsDataSource::setInputSource(MWeatherPredictionDataSource* s)
{
    inputSource = s;
    registerInputSource(inputSource);
//    enablePassThrough(s);
}


MStructuredGrid* MDerivedMetVarsDataSource::produceData(MDataRequest request)
{
    assert(inputSource != nullptr);

    // Parse request.
    MDataRequestHelper rh(request);
    QString derivedVarName = rh.value("VARIABLE");
    rh.removeAll(locallyRequiredKeys());

    // Get input fields.
    QList<MStructuredGrid*> inputGrids;
    foreach (QString requiredVar, requiredInputVariablesList[derivedVarName])
    {
        rh.insert("VARIABLE", requiredVar);
        inputGrids << inputSource->getData(rh.request());
    }

    // Initialize result grid.
    MStructuredGrid *derivedGrid = nullptr;
    if ( !inputGrids.isEmpty() && inputGrids.at(0) != nullptr )
    {
        derivedGrid = createAndInitializeResultGrid(inputGrids.at(0));
    }
    else return nullptr;

    // Compute derived grid.
    if ( (derivedVarName == "windspeed (an)")
         || (derivedVarName == "windspeed (fc)")
         || (derivedVarName == "windspeed (ens)") )
    {
        for (unsigned int n = 0; n < derivedGrid->getNumValues(); n++)
        {
            float windspeed = sqrt(pow(inputGrids.at(0)->getValue(n), 2)
                                   + pow(inputGrids.at(1)->getValue(n), 2));
            derivedGrid->setValue(n, windspeed);
        }
    }

    // Release input fields.
    foreach (MStructuredGrid *inputGrid, inputGrids)
        inputSource->releaseData(inputGrid);

    return derivedGrid;
}


MTask* MDerivedMetVarsDataSource::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    QString derivedVarName = rh.value("VARIABLE");
    rh.removeAll(locallyRequiredKeys());

    foreach (QString requiredVar, requiredInputVariablesList[derivedVarName])
    {
        rh.insert("VARIABLE", requiredVar);
        task->addParent(inputSource->getTaskGraph(rh.request()));
    }

    return task;
}


QList<MVerticalLevelType> MDerivedMetVarsDataSource::availableLevelTypes()
{
    assert(inputSource != nullptr);
    return inputSource->availableLevelTypes();
}


QStringList MDerivedMetVarsDataSource::availableVariables(
        MVerticalLevelType levelType)
{
    assert(inputSource != nullptr);

    QStringList availableVars;

    // For each variable that can be derived, check if required input variables
    // are available. If yes, add the derived variable to the list of available
    // variables.
    foreach (QString derivedVarName, requiredInputVariablesList.keys())
    {
        // If one of the required variables is not available from the input
        // source, skip.
        bool requiredInputVarsAvailable = true;
        foreach (QString requiredVar, requiredInputVariablesList[derivedVarName])
            if ( !inputSource->availableVariables(levelType).contains(requiredVar) )
            {
                requiredInputVarsAvailable = false;
                break;
            }

        if (requiredInputVarsAvailable)
            availableVars << derivedVarName;
    }

    return availableVars;
}


QSet<unsigned int> MDerivedMetVarsDataSource::availableEnsembleMembers(
        MVerticalLevelType levelType, const QString& variableName)
{
    assert(inputSource != nullptr);

    QSet<unsigned int> members;
    foreach (QString inputVar, requiredInputVariablesList[variableName])
    {
        if (members.isEmpty())
        {
            members = inputSource->availableEnsembleMembers(
                        levelType, inputVar);
        }
        else
        {
            members = members.intersect(inputSource->availableEnsembleMembers(
                                            levelType, inputVar));
        }
    }

    return members;
}


QList<QDateTime> MDerivedMetVarsDataSource::availableInitTimes(
        MVerticalLevelType levelType, const QString& variableName)
{
    assert(inputSource != nullptr);

//NOTE: Qt4.8 does not yet support QList<QDateTime> with the intersect()
// method (qHash doesn't support QDateTime types).
//TODO (mr, 2016May17) -- Change this to QSet when switching to Qt 5.X.
    QList<QDateTime> times;
    foreach (QString inputVar, requiredInputVariablesList[variableName])
    {
        if (times.isEmpty())
        {
            times = inputSource->availableInitTimes(levelType, inputVar);
        }
        else
        {
            QList<QDateTime> inputTimes = inputSource->availableInitTimes(
                        levelType, inputVar);

            foreach (QDateTime dt, times)
                if ( !inputTimes.contains(dt) ) times.removeOne(dt);
        }
    }

    return times;
}


QList<QDateTime> MDerivedMetVarsDataSource::availableValidTimes(
        MVerticalLevelType levelType,
        const QString& variableName, const QDateTime& initTime)
{
    assert(inputSource != nullptr);

    QList<QDateTime> times;
    foreach (QString inputVar, requiredInputVariablesList[variableName])
    {
        if (times.isEmpty())
        {
            times = inputSource->availableValidTimes(levelType, inputVar, initTime);
        }
        else
        {
            QList<QDateTime> inputTimes = inputSource->availableValidTimes(
                        levelType, inputVar, initTime);

            foreach (QDateTime dt, times)
                if ( !inputTimes.contains(dt) ) times.removeOne(dt);
        }
    }

    return times;
}


QString MDerivedMetVarsDataSource::variableLongName(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    Q_UNUSED(levelType); Q_UNUSED(variableName);
    return QString();
}


QString MDerivedMetVarsDataSource::variableStandardName(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    Q_UNUSED(levelType); Q_UNUSED(variableName);
    return QString();
}


QString MDerivedMetVarsDataSource::variableUnits(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    Q_UNUSED(levelType); Q_UNUSED(variableName);
    return QString();
}



/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MDerivedMetVarsDataSource::locallyRequiredKeys()
{
    return (QStringList() << "VARIABLE");
}

} // namespace Met3D
