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
    registerDerivedDataFieldProcessor(new MPotentialTemperatureProcessor());
    registerDerivedDataFieldProcessor(new MGeopotentialHeightProcessor());
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
    rh.removeAll(locallyRequiredKeys()); // removes "VARIABLE"

    // Get input fields.
    QList<MStructuredGrid*> inputGrids;
    foreach (QString requiredVarStdName,
             requiredInputVariablesList[derivedVarName])
    {
        // Handle enforced level types (cf. updateStdNameAndLevelType()).
        MVerticalLevelType ltype = levelType;
        updateStdNameAndLevelType(&requiredVarStdName, &ltype);

        rh.insert("VARIABLE", getInputVariableNameFromStdName(
                      requiredVarStdName));
        rh.insert("LEVELTYPE", ltype); // update requested level type

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
    rh.removeAll(locallyRequiredKeys()); // removes "VARIABLE"

    foreach (QString requiredVarStdName,
             requiredInputVariablesList[derivedVarName])
    {
        // Handle enforced level types (cf. updateStdNameAndLevelType()).
        MVerticalLevelType ltype = levelType;
        updateStdNameAndLevelType(&requiredVarStdName, &ltype);

        rh.insert("VARIABLE", getInputVariableNameFromStdName(
                      requiredVarStdName));
        rh.insert("LEVELTYPE", ltype); // update requested level type
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
            // Handle enforced level types.
            MVerticalLevelType ltype = levelType;
            updateStdNameAndLevelType(&requiredVarStdName, &ltype);

            // If one of the required variables is not available from the
            // input source, skip.
            if ( !inputSource->availableVariables(ltype).contains(
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
        updateStdNameAndLevelType(&inputVarStdName, &ltype);

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
        updateStdNameAndLevelType(&inputVarStdName, &ltype);

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
        updateStdNameAndLevelType(&inputVarStdName, &ltype);

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
        updateStdNameAndLevelType(&inputVarStdName, &ltype);

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


bool MDerivedMetVarsDataSource::updateStdNameAndLevelType(
        QString *stdName, MVerticalLevelType *levelType)
{
    // Assume somthing like "surface_geopotential/SURFACE_2D" passed in stdName.
    // If only a variable name is passed (e.g., ""surface_geopotential"),
    // nothing is changed.
    bool changedLevel = false;

    QStringList definitionsList = stdName->split("/");
    if (definitionsList.size() == 2)
    {
        MVerticalLevelType newLevelType =
                MStructuredGrid::verticalLevelTypeFromConfigString(
                    definitionsList.at(1));

        // If a valid leveltype has been defined, update the passed variables.
        if (newLevelType != SIZE_LEVELTYPES)
        {
            *levelType = newLevelType;
            *stdName = definitionsList.at(0);
            changedLevel = true;
        }
    }

    return changedLevel;
}



/******************************************************************************
***                            DATA PROCESSORS                              ***
*******************************************************************************/

// Wind Speed
// ==========

MHorizontalWindSpeedProcessor::MHorizontalWindSpeedProcessor()
    : MDerivedDataFieldProcessor(
          "wind_speed",
          QStringList() << "eastward_wind" << "northward_wind")
{}


void MHorizontalWindSpeedProcessor::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "eastward_wind"
    // input 1 = "northward_wind"

    for (unsigned int n = 0; n < derivedGrid->getNumValues(); n++)
    {
        float u_ms = inputGrids.at(0)->getValue(n);
        float v_ms = inputGrids.at(1)->getValue(n);

        if (u_ms == M_MISSING_VALUE || v_ms == M_MISSING_VALUE)
        {
            derivedGrid->setValue(n, M_MISSING_VALUE);
        }
        else
        {
            float windspeed = windSpeed_ms(u_ms, v_ms);
            derivedGrid->setValue(n, windspeed);
        }
    }
}


MPotentialTemperatureProcessor::MPotentialTemperatureProcessor()
    : MDerivedDataFieldProcessor(
          "air_potential_temperature",
          QStringList() << "air_temperature")
{
}


// Potential temperature
// =====================

void MPotentialTemperatureProcessor::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "air_temperature"

    // Requires nested k/j/i loops to access pressure at grid point.
    for (unsigned int k = 0; k < derivedGrid->getNumLevels(); k++)
        for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
            for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
            {
                float T_K = inputGrids.at(0)->getValue(k, j, i);

                if (T_K == M_MISSING_VALUE)
                {
                    derivedGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    float theta_K = potentialTemperature_K(
                                T_K,
                                inputGrids.at(0)->getPressure(k, j, i) * 100.);
                    derivedGrid->setValue(k, j, i, theta_K);
                }
            }
}


// Geopotential height
// ===================

MGeopotentialHeightProcessor::MGeopotentialHeightProcessor()
    : MDerivedDataFieldProcessor(
          "geopotential_height",
          QStringList() << "air_temperature" << "specific_humidity"
                        << "surface_geopotential/SURFACE_2D"
                        << "surface_air_pressure/SURFACE_2D"
                        << "surface_temperature/SURFACE_2D")
{
}


void MGeopotentialHeightProcessor::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "air_temperature"
    // input 1 = "specific_humidity"
    // input 2 = "surface_geopotential"
    // input 3 = "surface_air_pressure"
    // input 4 = "surface_temperature"

    MStructuredGrid *airTemperatureGrid = inputGrids.at(0);
    MStructuredGrid *specificHumidityGrid = inputGrids.at(1);

    // Cast surface grids to get access to 2D getValue() method.
    MRegularLonLatGrid *surfaceGeopotentialGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(2));
    MRegularLonLatGrid *surfaceAirPressureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(3));
    MRegularLonLatGrid *surfaceTemperatureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(4));

    // Integrate geopotential height from surface to top. k = 0 denotes the
    // uppermost level, k = nlev-1 the lowest model level.

    // Start by computing the thickness of the layer between the surface and
    // the lowest model level.
    for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
        for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
        {
            unsigned int k_lowest = airTemperatureGrid->getNumLevels() - 1;
            float p_bot_hPa = surfaceAirPressureGrid->getValue(j, i) / 100.;
            float p_top_hPa = airTemperatureGrid->getPressure(k_lowest, j, i);

            // If pressure level data are used, the lower levels can be BELOW
            // the surface. Here we cannot compute geopotential height, the
            // corresponding values need to be set to M_MISSING_VALUE.
            while (p_top_hPa > p_bot_hPa)
            {
                derivedGrid->setValue(k_lowest, j, i, M_MISSING_VALUE);
                // Decrement k; pay attention to staying in the valid range.
                if (k_lowest == 0) continue; else k_lowest--;
                // Update current p_top_hPa.
                p_top_hPa = airTemperatureGrid->getPressure(k_lowest, j, i);
            }

//!TODO (mr, 11Mar2018) We're currently assuming specific humidity to be
//! constant in the lowermost layer. This needs to be replaced by an
//! implementation that uses surface dew point to compute virtual temperature.
//! The impact should be small at least for hybrid levels, though, as the
//! lowest layer usually is fairly thin.
            float virtualTemperature_bot_K = virtualTemperature_K(
                        surfaceTemperatureGrid->getValue(j, i),
                        specificHumidityGrid->getValue(k_lowest, j, i));

            float virtualTemperature_top_K = virtualTemperature_K(
                        airTemperatureGrid->getValue(k_lowest, j, i),
                        specificHumidityGrid->getValue(k_lowest, j, i));

            float layerMeanVirtualTemperature_K =
                    (virtualTemperature_bot_K + virtualTemperature_top_K) / 2.;

            float surfaceGeopotentialHeight_m =
                    surfaceGeopotentialGrid->getValue(j, i) /
                    MetConstants::GRAVITY_ACCELERATION;

            float geopotentialHeight_m = surfaceGeopotentialHeight_m +
                    geopotentialThicknessOfLayer_m(
                        layerMeanVirtualTemperature_K,
                        p_bot_hPa, p_top_hPa);

            derivedGrid->setValue(k_lowest, j, i, geopotentialHeight_m);
        }

    // Add thicknesses of all layers above.
    for (int k = int(derivedGrid->getNumLevels()) - 2; k >= 0; k--)
        for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
            for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
            {
                // Check if the current grid point has already been flagged
                // as missing value (.. pressure levels .., see above).
                if (derivedGrid->getValue(k, j, i) == M_MISSING_VALUE)
                {
                    continue;
                }

                float p_bot_hPa = airTemperatureGrid->getPressure(k+1, j, i);
                float p_top_hPa = airTemperatureGrid->getPressure(k  , j, i);

                float virtualTemperature_bot_K = virtualTemperature_K(
                            airTemperatureGrid->getValue(k+1, j, i),
                            specificHumidityGrid->getValue(k+1, j, i));

                float virtualTemperature_top_K = virtualTemperature_K(
                            airTemperatureGrid->getValue(k, j, i),
                            specificHumidityGrid->getValue(k, j, i));

                float layerMeanVirtualTemperature_K =
                        (virtualTemperature_bot_K + virtualTemperature_top_K) / 2.;

                float geopotentialHeight_m =
                        derivedGrid->getValue(k+1, j, i) + // z of bot level
                        geopotentialThicknessOfLayer_m(
                            layerMeanVirtualTemperature_K,
                            p_bot_hPa, p_top_hPa);

                derivedGrid->setValue(k, j, i, geopotentialHeight_m);

//                // Debug output.
//                if (j == 0 && i == 0)
//                {
//                    LOG4CPLUS_DEBUG(mlog, "GEOP HEIGHT DBG: k=" << k
//                                    << " lev=" << derivedGrid->getLevels()[k]
//                                    << " p_bot=" << p_bot_hPa
//                                    << " p_top=" << p_top_hPa
//                                    << " T_bot=" << airTemperatureGrid->getValue(k+1, j, i)
//                                    << " q_bot=" << specificHumidityGrid->getValue(k+1, j, i)
//                                    << " T_top=" << airTemperatureGrid->getValue(k, j, i)
//                                    << " q_top=" << specificHumidityGrid->getValue(k, j, i)
//                                    << " Tv_bot=" << virtualTemperature_bot_K
//                                    << " Tv_top=" << virtualTemperature_top_K
//                                    << " Tv_mean=" << layerMeanVirtualTemperature_K
//                                    << " z(-1)=" << derivedGrid->getValue(k+1, j, i)
//                                    << " z=" << geopotentialHeight_m
//                                    << " \n" << flush);
//                }
            }
}


} // namespace Met3D
