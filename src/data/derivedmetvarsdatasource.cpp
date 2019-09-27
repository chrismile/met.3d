/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2019 Marc Rautenhaus [*, previously +]
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


// Magnitude of air velocity (3D wind speed; "wind" is defined as 2D only in
// CF).
// =========================================================================

MMagnitudeOfAirVelocityProcessor::MMagnitudeOfAirVelocityProcessor()
    : MDerivedDataFieldProcessor(
          "magnitude_of_air_velocity",
          QStringList() << "eastward_wind" << "northward_wind"
                        << "upward_air_velocity")
{}


void MMagnitudeOfAirVelocityProcessor::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "eastward_wind"
    // input 1 = "northward_wind"
    // input 2 = "upward_air_velocity"

    for (unsigned int n = 0; n < derivedGrid->getNumValues(); n++)
    {
        float u_ms = inputGrids.at(0)->getValue(n);
        float v_ms = inputGrids.at(1)->getValue(n);
        float w_ms = inputGrids.at(2)->getValue(n);

        if (u_ms == M_MISSING_VALUE || v_ms == M_MISSING_VALUE
                || w_ms == M_MISSING_VALUE)
        {
            derivedGrid->setValue(n, M_MISSING_VALUE);
        }
        else
        {
            float windspeed3D = windSpeed3D_ms(u_ms, v_ms, w_ms);
            derivedGrid->setValue(n, windspeed3D);
        }
    }
}


// Potential temperature
// =====================

MPotentialTemperatureProcessor::MPotentialTemperatureProcessor()
    : MDerivedDataFieldProcessor(
          "air_potential_temperature",
          QStringList() << "air_temperature")
{
}


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


// Equivalent potential temperature
// ================================

MEquivalentPotentialTemperatureProcessor
::MEquivalentPotentialTemperatureProcessor()
    : MDerivedDataFieldProcessor(
          "equivalent_potential_temperature",
          QStringList() << "air_temperature" << "specific_humidity")
{
}


void MEquivalentPotentialTemperatureProcessor::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "air_temperature"
    // input 1 = "specific_humidity"

    // Requires nested k/j/i loops to access pressure at grid point.
    for (unsigned int k = 0; k < derivedGrid->getNumLevels(); k++)
        for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
            for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
            {
                float T_K = inputGrids.at(0)->getValue(k, j, i);
                float q_kgkg = inputGrids.at(1)->getValue(k, j, i);

                if (T_K == M_MISSING_VALUE || q_kgkg == M_MISSING_VALUE)
                {
                    derivedGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
//!TODO (mr, 14Mar2018) -- possibly replace the Bolton equation by a more
//! recent formula. See Davies-Jones (MWR, 2009), "On Formulas for Equiv.
//! Potential Temperature".
                    float thetaE_K = equivalentPotentialTemperature_K_Bolton(
                                T_K,
                                inputGrids.at(0)->getPressure(k, j, i) * 100.,
                                q_kgkg);
                    derivedGrid->setValue(k, j, i, thetaE_K);
                }
            }
}


// Potential vorticity (LAGRANTO libcalvar implementation)
// =======================================================

MPotentialVorticityProcessor_LAGRANTOcalvar
::MPotentialVorticityProcessor_LAGRANTOcalvar()
    : MDerivedDataFieldProcessor(
          "ertel_potential_vorticity",
          QStringList() << "eastward_wind"
                        << "northward_wind"
                        << "air_temperature"
                        << "surface_air_pressure/SURFACE_2D")
{
}


//#define CALVAR_DEBUG
void MPotentialVorticityProcessor_LAGRANTOcalvar::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "eastward_wind"
    // input 1 = "northward_wind"
    // input 2 = "air_temperature"
    // input 3 = "surface_air_pressure"

    // This method uses the LAGRANTO.ECMWF libcalvar funcation "potvort" to
    // compute potential vorticity. To call the FORTRAN function "potvort",
    // the data contained in the MStructuredGrid classes needs to be
    // restructed:
    // * libcalvar requires float arrays that contain the full 3D variable
    //   fields.
    // * libcalvar requires the lat dimension to be reversed (in increasing
    //   lat order) in all spatial fields.
    // * surface pressure needs to be passed in hPa.
    // * ak and bk coefficients need to be passed as float arrays.
    //
    // Also compare to libcalvar usage in ppecmwf.py in the met.dp repository.

    // Grid sizes.
    int nlev = derivedGrid->getNumLevels();
    int nlat = derivedGrid->getNumLats();
    int nlon = derivedGrid->getNumLons();
    int nlatnlon = nlat*nlon;

    // Convert surface pressure from Pa to hPa; reverse lat dimension.
    float *psfc_hPa_revLat = new float[nlat*nlon];
    for (int j = 0; j < nlat; j++)
        for (int i = 0; i < nlon; i++)
        {
            psfc_hPa_revLat[INDEX2yx(j, i, nlon)] = inputGrids.at(3)->getValue(
                        INDEX2yx(nlat-1-j, i, nlon)) / 100.;
        }

    // Cast derived grid to hybrid sigma pressure grid to access ak/bk
    // coefficients; convert the double arrays in MLonLatHybridSigmaPressureGrid
    // to float arrays.
    MLonLatHybridSigmaPressureGrid *derivedHybridGrid =
            dynamic_cast<MLonLatHybridSigmaPressureGrid*>(derivedGrid);

    float *ak_hPa_float = new float[nlev];
    float *bk_float = new float[nlev];
    for (int k = 0; k < nlev; k++)
    {
        ak_hPa_float[k] = derivedHybridGrid->ak_hPa[k];
        bk_float[k] = derivedHybridGrid->bk[k];
    }

#ifdef CALVAR_DEBUG
    // DEBUG section to check correct return values from FORTRAN module.
    // Compare potential temperature computed by libcalvar with Met.3D method.
    float *T_revLat = new float[nlev*nlat*nlon];
    for (int k = 0; k < nlev; k++)
        for (int j = 0; j < nlat; j++)
            for (int i = 0; i < nlon; i++)
            {
                T_revLat[INDEX3zyx_2(k, j, i, nlatnlon, nlon)] =
                        inputGrids.at(2)->data[INDEX3zyx_2(k, nlat-1-j, i,
                                                           nlatnlon, nlon)];
            }

    float *pottemp_calvar_levLat = new float[nlev*nlat*nlon];
    potentialTemperature_K_calvar(pottemp_calvar_levLat, T_revLat,
                                psfc_hPa_revLat, nlon, nlat, nlev,
                                ak_hPa_float, bk_float);
#endif

    // "potvort" requires potential temperature as input; compute this field
    // and reverse lat dimension.
    float *pottemp_K_revLat = new float[nlev*nlat*nlon];
    for (int k = 0; k < nlev; k++)
        for (int j = 0; j < nlat; j++)
            for (int i = 0; i < nlon; i++)
            {
                float T_K = inputGrids.at(2)->getValue(k, nlat-1-j, i);
                float p_Pa = inputGrids.at(2)->getPressure(k, nlat-1-j, i) * 100.;
                pottemp_K_revLat[INDEX3zyx_2(k, j, i, nlatnlon, nlon)] =
                        potentialTemperature_K(T_K, p_Pa);
#ifdef CALVAR_DEBUG
                cout << "PotTemp Met.3D: "
                     << pottemp_K_revLat[INDEX3zyx_2(k, j, i, nlatnlon, nlon)]
                     << " vs. libcalvar: "
                     << pottemp_calvar_levLat[INDEX3zyx_2(k, j, i, nlatnlon, nlon)]
                     << "\n" << flush;
#endif
            }

#ifdef CALVAR_DEBUG
    delete[] pottemp_calvar_levLat;
#endif

    // Compute two more input fields required by "potvort": a 2D field of
    // cos(lat) and a 2D field of the Coriolis parameter f. Reverse lat
    // dimension.
    float *coslat_revLat = new float[nlat*nlon];
    for (int j = 0; j < nlat; j++)
        for (int i = 0; i < nlon; i++)
        {
            coslat_revLat[INDEX2yx(j, i, nlon)] =
                    cos(derivedGrid->getLats()[nlat-1-j] / 180. * M_PI);
        }

    float *coriolis_revLat = new float[nlat*nlon];
    for (int j = 0; j < nlat; j++)
        for (int i = 0; i < nlon; i++)
        {
            coriolis_revLat[INDEX2yx(j, i, nlon)] =
                    coriolisParameter_deg(derivedGrid->getLats()[nlat-1-j]);
        }

    // "potvort" requires two 4-element vectors that contain the lon/lat range.
    // Compare to the Python implementation in ppecmwf.py.
    float *varmin = new float[4];
    varmin[0] = derivedGrid->getLons()[0]; // min lon
    varmin[1] = derivedGrid->getLats()[nlat-1]; // min lat
    varmin[2] = 0.;
    varmin[3] = 0.;
    float *varmax = new float[4];
    varmax[0] = derivedGrid->getLons()[nlon-1]; // max lon
    varmax[1] = derivedGrid->getLats()[0]; // max lat
    varmax[2] = 0.;
    varmax[3] = 0.;

    // Reverse the lat dimension of the u and v wind component fields.
    float *u_revLat = new float[nlev*nlat*nlon];
    float *v_revLat = new float[nlev*nlat*nlon];
    for (int k = 0; k < nlev; k++)
        for (int j = 0; j < nlat; j++)
            for (int i = 0; i < nlon; i++)
            {
                u_revLat[INDEX3zyx_2(k, j, i, nlatnlon, nlon)] =
                        inputGrids.at(0)->data[INDEX3zyx_2(k, nlat-1-j, i,
                                                           nlatnlon, nlon)];
                v_revLat[INDEX3zyx_2(k, j, i, nlatnlon, nlon)] =
                        inputGrids.at(1)->data[INDEX3zyx_2(k, nlat-1-j, i,
                                                           nlatnlon, nlon)];
            }

    // Call the "potvort" LAGRANTO function in the FORTRAN libcalvar library.
    float *potvort_revLat = new float[nlev*nlat*nlon];
    potentialVorticity_PVU_calvar(
                potvort_revLat, u_revLat, v_revLat, pottemp_K_revLat,
                psfc_hPa_revLat, coslat_revLat, coriolis_revLat,
                nlon, nlat, nlev, ak_hPa_float, bk_float, varmin, varmax);

    // Reverse the lat dimension of the computed PV field and store PV in
    // derived grid. Change units from PVU to SI units (* 1.E-6).
    for (int k = 0; k < nlev; k++)
        for (int j = 0; j < nlat; j++)
            for (int i = 0; i < nlon; i++)
            {
                derivedGrid->setValue(k, j, i,
                                      potvort_revLat[INDEX3zyx_2(
                            k, nlat-1-j, i, nlatnlon, nlon)] * 1.E-6);
            }

    // [Check for missing values in input and correct output.]

    // Delete temporary memory.
    delete[] pottemp_K_revLat;
    delete[] coslat_revLat;
    delete[] coriolis_revLat;
    delete[] varmin;
    delete[] varmax;
    delete[] ak_hPa_float;
    delete[] bk_float;
    delete[] psfc_hPa_revLat;
    delete[] potvort_revLat;
    delete[] u_revLat;
    delete[] v_revLat;
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
                // Check if the bottom level of the current grid point has
                // already been flagged as missing value (.. pressure levels
                // .., see above).
                if (derivedGrid->getValue(k+1, j, i) == M_MISSING_VALUE)
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


// Geopotential height from geopotential
// =====================================

MGeopotentialHeightFromGeopotentialProcessor
::MGeopotentialHeightFromGeopotentialProcessor()
    : MDerivedDataFieldProcessor(
          "geopotential_height_from_geopotential",
          QStringList() << "geopotential")
{
}

void MGeopotentialHeightFromGeopotentialProcessor::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "geopotential"

    for (unsigned int n = 0; n < derivedGrid->getNumValues(); n++)
    {
        float geopotential = inputGrids.at(0)->getValue(n);

        if (geopotential == M_MISSING_VALUE)
        {
            derivedGrid->setValue(n, M_MISSING_VALUE);
        }
        else
        {
            derivedGrid->setValue(n, geopotential /
                                  MetConstants::GRAVITY_ACCELERATION);
        }
    }
}


// Dew point temperature
// =====================

MDewPointTemperatureProcessor::MDewPointTemperatureProcessor()
    : MDerivedDataFieldProcessor(
          "dew_point_temperature",
          QStringList() << "specific_humidity")
{}


void MDewPointTemperatureProcessor::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "specific_humidity"

    // Requires nested k/j/i loops to access pressure at grid point.
    for (unsigned int k = 0; k < derivedGrid->getNumLevels(); k++)
        for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
            for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
            {
                float q_kgkg = inputGrids.at(0)->getValue(k, j, i);

                if (q_kgkg == M_MISSING_VALUE)
                {
                    derivedGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    float dewPoint_K = dewPointTemperature_K_Bolton(
                                inputGrids.at(0)->getPressure(k, j, i) * 100.,
                                q_kgkg);
                    derivedGrid->setValue(k, j, i, dewPoint_K);
                }
            }
}


// Total precipitation per time interval
// =====================================

MTHourlyTotalPrecipitationProcessor::MTHourlyTotalPrecipitationProcessor(
        int hours)
    : MDerivedDataFieldProcessor(
          QString("lwe_thickness_of_precipitation_amount_%1h").arg(hours),
          QStringList() << "lwe_thickness_of_precipitation_amount"
                        << QString(
                               "lwe_thickness_of_precipitation_amount///-%1")
                           .arg(hours*3600))
{
}


void MTHourlyTotalPrecipitationProcessor::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "lwe_thickness_of_precipitation_amount"
    // input 1 = "lwe_thickness_of_precipitation_amount", valid - T hours

    if (inputGrids.at(0) == nullptr || inputGrids.at(1) == nullptr)
    {
        // In case the previous timestep is not available, a nullptr will be
        // passed as input. In this case, simply return a field of missing
        // values.
        derivedGrid->setToValue(M_MISSING_VALUE);
    }
    else for (unsigned int n = 0; n < derivedGrid->getNumValues(); n++)
    {
        float precip_VT = inputGrids.at(0)->getValue(n);
        float precip_VT_minus_Th = inputGrids.at(1)->getValue(n);

        float precip_difference = precip_VT - precip_VT_minus_Th;
        derivedGrid->setValue(n, precip_difference);
    }
}


// Vertically integrated moisture flux
// ===================================

// NOTE: surface_air_pressure is requested as a dummy grid to
// initialize the derived grid as a 2D field.
MMagnitudeOfVerticallyIntegratedMoistureFluxProcessor
::MMagnitudeOfVerticallyIntegratedMoistureFluxProcessor(QString levelTypeString)
    : MDerivedDataFieldProcessor(
          QString("magnitude_of_vertically_integrated_horizontal_"
                  "transport_of_moisture__from_%1").arg(levelTypeString),
          QStringList() << "surface_air_pressure"
                        << QString("eastward_wind/%1").arg(levelTypeString)
                        << QString("northward_wind/%1").arg(levelTypeString)
                        << QString("specific_humidity/%1").arg(levelTypeString))
{
}


void MMagnitudeOfVerticallyIntegratedMoistureFluxProcessor::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "surface_air_pressure" -- never used, just for initialization
    // input 1 = "eastward_wind"
    // input 2 = "northward_wind"
    // input 3 = "specific_humidity"
    MStructuredGrid *eastwardWindGrid = inputGrids.at(1);
    MStructuredGrid *northwardWindGrid = inputGrids.at(2);
    MStructuredGrid *specificHumidityGrid = inputGrids.at(3);

    for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
        for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
        {
            // For each horizontal grid point, compute the total
            // horizontal transport of moisture.
            // See: https://en.wikipedia.org/wiki/Moisture_advection#Moisture_flux
            // * horizontal moisture flux f = (fu, fv) = (u, v)
            //                              * mixing ratio / specific humidity
            // * vertical integral: int(psfc, 0, of: f/g dp)
            //
            // NOTE: This implementation used specific humidity; mixing ration
            //  can also be used.
            // Also cf. to Eq. (1) and (2) in Zebaze et al. (AtSciLet, 2017),
            // "Interaction between moisture transport...".

            float totalEastwardMoistureflux = 0.;
            float totalNorthwardMoistureflux = 0.;

            for (unsigned int k = 0; k < eastwardWindGrid->getNumLevels(); k++)
            {
                float layerDeltaPressure_Pa =
                        (eastwardWindGrid->getBottomInterfacePressure(k, j, i) -
                         eastwardWindGrid->getTopInterfacePressure(k, j, i)) * 100.;

                //float humidity = mixingRatio_kgkg(
                //            specificHumidityGrid->getValue(k, j, i));
                float humidity = specificHumidityGrid->getValue(k, j, i);

                totalEastwardMoistureflux +=
                        // eastward moisture flux = r*u
                        (humidity * eastwardWindGrid->getValue(k, j, i)) *
                        // * dp
                        layerDeltaPressure_Pa;

                totalNorthwardMoistureflux +=
                        // eastward moisture flux = r*v
                        (humidity * northwardWindGrid->getValue(k, j, i)) *
                        // * dp
                        layerDeltaPressure_Pa;
            }

            totalEastwardMoistureflux /= MetConstants::GRAVITY_ACCELERATION;
            totalNorthwardMoistureflux /= MetConstants::GRAVITY_ACCELERATION;

            // Computed total moisture flux magnitude is in [kg m-1 s-1].
            float totalMoistureFlux = pow(
                        totalEastwardMoistureflux * totalEastwardMoistureflux +
                        totalNorthwardMoistureflux * totalNorthwardMoistureflux,
                        0.5);

            static_cast<MRegularLonLatGrid*>(derivedGrid)->setValue(
                        j, i, totalMoistureFlux);
        }
}


// Pressure
// ========

MPressureProcessor::MPressureProcessor()
    : MDerivedDataFieldProcessor(
          "air_pressure",
          QStringList() << "air_temperature")
{
}


void MPressureProcessor::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "air_temperature"

    // Requires nested k/j/i loops to access pressure at grid point.
    for (unsigned int k = 0; k < derivedGrid->getNumLevels(); k++)
        for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
            for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
            {
                float p_Pa = inputGrids.at(0)->getPressure(k, j, i) * 100.;
                derivedGrid->setValue(k, j, i, p_Pa);
            }
}

} // namespace Met3D
