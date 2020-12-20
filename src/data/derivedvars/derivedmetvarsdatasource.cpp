/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2020 Marcel Meyer [*]
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
**
**  + Computer Graphics and Visualization Group
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
#include "util/metroutines_experimental.h"
#include "derivedmetvars_standard.h"
#include "derivedmetvars_mcaoindicator.h"


using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MDerivedMetVarsDataSource::MDerivedMetVarsDataSource()
    : MProcessingWeatherPredictionDataSource(),
      inputSource(nullptr)
{
    // Register data field processors.
    // ===============================
//!Todo (mr, 11Mar2018). This could possibly be moved out of this constructor
//! and be done outside of the class as a configuration/plug-in mechanism.

    registerDerivedDataFieldProcessor(new MHorizontalWindSpeedProcessor());
    registerDerivedDataFieldProcessor(new MMagnitudeOfAirVelocityProcessor());
    registerDerivedDataFieldProcessor(new MPotentialTemperatureProcessor());
    registerDerivedDataFieldProcessor(new MEquivalentPotentialTemperatureProcessor());
    registerDerivedDataFieldProcessor(new MGeopotentialHeightProcessor());
    registerDerivedDataFieldProcessor(new MGeopotentialHeightFromGeopotentialProcessor());
    registerDerivedDataFieldProcessor(new MDewPointTemperatureProcessor());
    registerDerivedDataFieldProcessor(new MPressureProcessor());
    registerDerivedDataFieldProcessor(new MPotentialVorticityProcessor_LAGRANTOcalvar);

    registerDerivedDataFieldProcessor(
                new MMagnitudeOfVerticallyIntegratedMoistureFluxProcessor(
                    "HYBRID_SIGMA_PRESSURE_3D"));
//!TODO (mr, 13Mar2018) -- there needs to be a more elegant way to handle
//! cases in which the returned data field is of different type than all
//! required input fields. The current solution appends the input level type
//! to the variable name, which is not very elegant...
    registerDerivedDataFieldProcessor(
                new MMagnitudeOfVerticallyIntegratedMoistureFluxProcessor(
                    "PRESSURE_LEVELS_3D"));
    registerDerivedDataFieldProcessor(
                new MMagnitudeOfVerticallyIntegratedMoistureFluxProcessor(
                    "AUXILIARY_PRESSURE_3D"));

    registerDerivedDataFieldProcessor(new MTHourlyTotalPrecipitationProcessor(1));
    registerDerivedDataFieldProcessor(new MTHourlyTotalPrecipitationProcessor(3));
    registerDerivedDataFieldProcessor(new MTHourlyTotalPrecipitationProcessor(6));
    registerDerivedDataFieldProcessor(new MTHourlyTotalPrecipitationProcessor(12));
    registerDerivedDataFieldProcessor(new MTHourlyTotalPrecipitationProcessor(24));


    // Register experimental data field processors.
    // ============================================

#ifdef ENABLE_EXPERIMENTAL_DERIVEDVARS
    registerDerivedDataFieldProcessor(new MMCAOIndexProcessor_Papritz2015());
     registerDerivedDataFieldProcessor(new MMCAOIndexProcessor_Papritz2015_nonMasked());
    registerDerivedDataFieldProcessor(new MMCAOIndexProcessor_Kolstad2008());
    // MMCAOIndexProcessor_BracegirdleGray2008 needs to be fixed -- do not use!
    // registerDerivedDataFieldProcessor(new MMCAOIndexProcessor_BracegirdleGray2008());
    registerDerivedDataFieldProcessor(new MMCAOIndexProcessor_Michel2018());
    registerDerivedDataFieldProcessor(
                new MMCAOIndex2DProcessor_YuliaP(
                    "HYBRID_SIGMA_PRESSURE_3D"));
    registerDerivedDataFieldProcessor(
                new MMCAOIndex2DProcessor_YuliaP(
                    "PRESSURE_LEVELS_3D"));
    registerDerivedDataFieldProcessor(
                new MMCAOIndex2DProcessor_YuliaP(
                    "AUXILIARY_PRESSURE_3D"));
#endif

    // ... <add registration commands here> ...
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
#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
#endif

    assert(inputSource != nullptr);

    // Parse request.
    MDataRequestHelper rh(request);
    QString derivedVarName = rh.value("VARIABLE");
    MVerticalLevelType levelType = MVerticalLevelType(rh.intValue("LEVELTYPE"));
    QDateTime initTime  = rh.timeValue("INIT_TIME");
    QDateTime validTime = rh.timeValue("VALID_TIME");
    rh.removeAll(locallyRequiredKeys()); // removes "VARIABLE"

    // Get input fields.
    QList<MStructuredGrid*> inputGrids;
    foreach (QString requiredVarStdName,
             requiredInputVariablesList[derivedVarName])
    {
        // Handle enforced level types (cf. updateStdNameAndLevelType()).
        MVerticalLevelType ltype = levelType;
        bool argsChanged = updateStdNameAndArguments(
                    &requiredVarStdName, &ltype, &initTime, &validTime);

        QString inputVarName = getInputVariableNameFromStdName(
                    requiredVarStdName);
        rh.insert("VARIABLE", inputVarName);
        rh.insert("LEVELTYPE", ltype); // update if changed
        rh.insert("INIT_TIME", initTime); // update if changed
        rh.insert("VALID_TIME", validTime); // update if changed

        // Check if if times have been changed the requested data fields are
        // available.
        bool everythingAvailable = true;
        if ( argsChanged && !inputSource->availableInitTimes(
                 ltype, inputVarName).contains(initTime) )
        {
            everythingAvailable = false;
        }
        else if ( argsChanged && !inputSource->availableValidTimes(
                      ltype, inputVarName, initTime).contains(validTime) )
        {
            everythingAvailable = false;
        }

        if (everythingAvailable)
        {
            inputGrids << inputSource->getData(rh.request());
        }
        else
        {
            // If an input request fails (e.g. if the field 6-h earlier is
            // also requested but not available), add a nullptr so that
            // the number of entries in the list is consistent. It is the
            // responsibility of the processor module to check...
            inputGrids << nullptr;
        }
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
        if (inputGrid == nullptr) continue;
        inputSource->releaseData(inputGrid);
    }

#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "computed derived data field "
                    << derivedVarName.toStdString()
                    << " in " << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif

    return derivedGrid;
}


MTask* MDerivedMetVarsDataSource::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    QString derivedVarName = rh.value("VARIABLE");
    MVerticalLevelType levelType = MVerticalLevelType(rh.intValue("LEVELTYPE"));
    QDateTime initTime  = rh.timeValue("INIT_TIME");
    QDateTime validTime = rh.timeValue("VALID_TIME");
    rh.removeAll(locallyRequiredKeys()); // removes "VARIABLE"

    foreach (QString requiredVarStdName,
             requiredInputVariablesList[derivedVarName])
    {
        // Handle enforced level types (cf. updateStdNameAndLevelType()).
        MVerticalLevelType ltype = levelType;
        bool argsChanged = updateStdNameAndArguments(
                    &requiredVarStdName, &ltype, &initTime, &validTime);

        QString inputVarName = getInputVariableNameFromStdName(
                    requiredVarStdName);
        rh.insert("VARIABLE", inputVarName);
        rh.insert("LEVELTYPE", ltype); // update if changed
        rh.insert("INIT_TIME", initTime); // update if changed
        rh.insert("VALID_TIME", validTime); // update if changed

        // Check if if times have been changed the requested data fields are
        // available.
        bool everythingAvailable = true;
        if ( argsChanged && !inputSource->availableInitTimes(
                 ltype, inputVarName).contains(initTime) )
        {
            everythingAvailable = false;
        }
        else if ( argsChanged && !inputSource->availableValidTimes(
                      ltype, inputVarName, initTime).contains(validTime) )
        {
            everythingAvailable = false;
        }

        if (everythingAvailable)
        {
            task->addParent(inputSource->getTaskGraph(rh.request()));
        }
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
            // Handle enforced level types.
            MVerticalLevelType ltype = levelType;
            updateStdNameAndArguments(&requiredVarStdName, &ltype);

            // If one of the required variables is not available from the
            // input source, skip.            
            if ( !inputSource->availableLevelTypes().contains(ltype) ||
                 !inputSource->availableVariables(ltype).contains(
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
        // Handle enforced level types.
        MVerticalLevelType ltype = levelType;
        updateStdNameAndArguments(&inputVarStdName, &ltype);

        if (members.isEmpty())
        {
            members = inputSource->availableEnsembleMembers(
                        ltype,
                        getInputVariableNameFromStdName(inputVarStdName));
        }
        else
        {
            members = members.intersect(
                        inputSource->availableEnsembleMembers(
                            ltype,
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
        // Handle enforced level types.
        MVerticalLevelType ltype = levelType;
        updateStdNameAndArguments(&inputVarStdName, &ltype);

        if (times.isEmpty())
        {
            times = inputSource->availableInitTimes(
                        ltype,
                        getInputVariableNameFromStdName(inputVarStdName));
        }
        else
        {
            QList<QDateTime> inputTimes = inputSource->availableInitTimes(
                        ltype,
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
        // Handle enforced level types.
        MVerticalLevelType ltype = levelType;
        updateStdNameAndArguments(&inputVarStdName, &ltype);

        if (times.isEmpty())
        {
            times = inputSource->availableValidTimes(
                        ltype,
                        getInputVariableNameFromStdName(inputVarStdName),
                        initTime);
        }
        else
        {
            QList<QDateTime> inputTimes = inputSource->availableValidTimes(
                        ltype,
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
        // Handle enforced level types.
        MVerticalLevelType ltype = levelType;
        updateStdNameAndArguments(&inputVarStdName, &ltype);

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
    return (QStringList() << "LEVELTYPE" << "VARIABLE" << "INIT_TIME"
            << "VALID_TIME");
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


bool MDerivedMetVarsDataSource::updateStdNameAndArguments(
        QString *stdName, MVerticalLevelType *levelType,
        QDateTime *initTime, QDateTime *validTime)
{
    bool changedArguments = false;

    // Assume somthing like "surface_geopotential/SURFACE_2D" passed in stdName.
    // If only a variable name is passed (e.g., ""surface_geopotential"),
    // nothing is changed.
    QStringList definitionsList = stdName->split("/");
    if (definitionsList.size() >= 2)
    {
        *stdName = definitionsList.at(0);

        MVerticalLevelType newLevelType =
                MStructuredGrid::verticalLevelTypeFromConfigString(
                    definitionsList.at(1));

        // If a valid leveltype has been defined, update the passed variables.
        if (newLevelType != SIZE_LEVELTYPES)
        {
            *levelType = newLevelType;
            changedArguments = true;
        }
    }

    // Assume something like "lwe_thickness_of_precipitation_amount//-43200"
    // is passed. This will substract 43200 seconds = 12 hours from the
    // INIT_TIME.
    if (definitionsList.size() >= 3 && initTime != nullptr)
    {
        bool ok;
        int timeDifference_sec = definitionsList.at(2).toInt(&ok);
        if (ok)
        {
            *initTime = initTime->addSecs(timeDifference_sec);
            changedArguments = true;
        }
    }

    // Assume something like "lwe_thickness_of_precipitation_amount///-21600"
    // is passed. This will substract 21600 seconds = 6 hours from the
    // VALID_TIME.
    if (definitionsList.size() == 4 && validTime != nullptr)
    {
        bool ok;
        int timeDifference_sec = definitionsList.at(3).toInt(&ok);
        if (ok)
        {
            *validTime = validTime->addSecs(timeDifference_sec);
            changedArguments = true;
        }
    }

    return changedArguments;
}








} // namespace Met3D
