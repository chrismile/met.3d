/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
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
#include "util/metroutines.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MDerivedDataFieldProcessor::MDerivedDataFieldProcessor(
        QString standardName, QStringList requiredInputVariables)
    : standardName(standardName),
      requiredInputVariables(requiredInputVariables)
{
}

MDerivedMetVarsDataSource::MDerivedMetVarsDataSource()
    : MProcessingWeatherPredictionDataSource(),
      inputSource(nullptr)
{
    // Register data field processors.
//!Todo (mr, 11Mar2018). This could possibly be moved out of this constructor
//! and be done outside of the class as a configuration/plug-in mechanism.

    registerDerivedDataFieldProcessor(new MHorizontalWindSpeedProcessor());
}


MDerivedMetVarsDataSource::~MDerivedMetVarsDataSource()
{
    foreach (MDerivedDataFieldProcessor* p, registeredDerivedDataProcessors)
    {
        delete p;
    }
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


void MDerivedMetVarsDataSource::setInputVariable(
        QString standardName, QString inputVariableName)
{
    variableStandardNameToInputNameMapping[standardName] = inputVariableName;
}


void MDerivedMetVarsDataSource::registerDerivedDataFieldProcessor(
        MDerivedDataFieldProcessor *processor)
{
    if (processor != nullptr)
    {
        registeredDerivedDataProcessors.insert(
                    processor->getStandardName(), processor);

        requiredInputVariablesList[processor->getStandardName()] =
                processor->getRequiredInputVariables();
    }
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
    foreach (QString requiredVarStdName,
             requiredInputVariablesList[derivedVarName])
    {
        rh.insert("VARIABLE", getInputVariableNameFromStdName(
                      requiredVarStdName));
        inputGrids << inputSource->getData(rh.request());
    }

    // Initialize result grid.
    MStructuredGrid *derivedGrid = nullptr;
    if ( !inputGrids.isEmpty() && inputGrids.at(0) != nullptr )
    {
        derivedGrid = createAndInitializeResultGrid(inputGrids.at(0));
        derivedGrid->setMetaData(inputGrids.at(0)->getInitTime(),
                                 inputGrids.at(0)->getValidTime(),
                                 derivedVarName,
                                 inputGrids.at(0)->getEnsembleMember());
    }

    // Compute derived grid.
    if ((derivedGrid != nullptr) &&
            registeredDerivedDataProcessors.contains(derivedVarName))
    {
        registeredDerivedDataProcessors[derivedVarName]->compute(
                    inputGrids, derivedGrid);
    }

    // Release input fields.
    foreach (MStructuredGrid *inputGrid, inputGrids)
    {
        inputSource->releaseData(inputGrid);
    }

    return derivedGrid;
}


MTask* MDerivedMetVarsDataSource::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    QString derivedVarName = rh.value("VARIABLE");
    rh.removeAll(locallyRequiredKeys());

    foreach (QString requiredVarStdName,
             requiredInputVariablesList[derivedVarName])
    {
        rh.insert("VARIABLE", getInputVariableNameFromStdName(
                      requiredVarStdName));
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
        bool requiredInputVarsAvailable = true;

        foreach (QString requiredVarStdName,
                 requiredInputVariablesList[derivedVarName])
        {
            // If one of the required variables is not available from the
            // input source, skip.
            if ( !inputSource->availableVariables(levelType).contains(
                     getInputVariableNameFromStdName(requiredVarStdName)) )
            {
                requiredInputVarsAvailable = false;
                break;
            }
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
    foreach (QString inputVarStdName, requiredInputVariablesList[variableName])
    {
        if (members.isEmpty())
        {
            members = inputSource->availableEnsembleMembers(
                        levelType,
                        getInputVariableNameFromStdName(inputVarStdName));
        }
        else
        {
            members = members.intersect(
                        inputSource->availableEnsembleMembers(
                            levelType,
                            getInputVariableNameFromStdName(inputVarStdName)));
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
    foreach (QString inputVarStdName, requiredInputVariablesList[variableName])
    {
        if (times.isEmpty())
        {
            times = inputSource->availableInitTimes(
                        levelType,
                        getInputVariableNameFromStdName(inputVarStdName));
        }
        else
        {
            QList<QDateTime> inputTimes = inputSource->availableInitTimes(
                        levelType,
                        getInputVariableNameFromStdName(inputVarStdName));

            foreach (QDateTime dt, times)
            {
                if ( !inputTimes.contains(dt) )
                {
                    times.removeOne(dt);
                }
            }
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
    foreach (QString inputVarStdName, requiredInputVariablesList[variableName])
    {
        if (times.isEmpty())
        {
            times = inputSource->availableValidTimes(
                        levelType,
                        getInputVariableNameFromStdName(inputVarStdName),
                        initTime);
        }
        else
        {
            QList<QDateTime> inputTimes = inputSource->availableValidTimes(
                        levelType,
                        getInputVariableNameFromStdName(inputVarStdName),
                        initTime);

            foreach (QDateTime dt, times)
            {
                if ( !inputTimes.contains(dt) )
                {
                    times.removeOne(dt);
                }
            }
        }
    }

    return times;
}


QString MDerivedMetVarsDataSource::variableLongName(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    Q_UNUSED(levelType);

    QString longName = QString("%1, computed from ").arg(variableName);

    foreach (QString inputVarStdName, requiredInputVariablesList[variableName])
    {
        longName += QString("%1/").arg(
                    getInputVariableNameFromStdName(inputVarStdName));
    }
    // Remove last "/" character.
    longName.resize(longName.length()-1);

    return longName;
}


QString MDerivedMetVarsDataSource::variableStandardName(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    Q_UNUSED(levelType);

    // Special property of this data source: variable names equal CF standard
    // names.
    return variableName;
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


QString MDerivedMetVarsDataSource::getInputVariableNameFromStdName(
        QString stdName)
{
    QString inputVariableName;
    if (variableStandardNameToInputNameMapping.contains(stdName))
    {
        inputVariableName = variableStandardNameToInputNameMapping[stdName];
    }
    return inputVariableName;
}



/******************************************************************************
***                            DATA PROCESSORS                              ***
*******************************************************************************/

MHorizontalWindSpeedProcessor::MHorizontalWindSpeedProcessor()
    : MDerivedDataFieldProcessor(
          "wind_speed",
          QStringList() << "eastward_wind" << "northward_wind")
{}


void MHorizontalWindSpeedProcessor::compute(
        QList<MStructuredGrid *> &inputGrids,
        MStructuredGrid *derivedGrid)
{
    // input 0 = "eastward_wind"
    // input 1 = "northward_wind"

    for (unsigned int n = 0; n < derivedGrid->getNumValues(); n++)
    {
        float windspeed = windSpeed_ms(inputGrids.at(0)->getValue(n),
                                       inputGrids.at(1)->getValue(n));
        derivedGrid->setValue(n, windspeed);
    }
}


} // namespace Met3D
