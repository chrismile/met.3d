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
#include "verticalregridder.h"

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

MVerticalRegridder::MVerticalRegridder()
    : MWeatherPredictionDataSource(),
      inputSource(nullptr)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MVerticalRegridder::setInputSource(MWeatherPredictionDataSource* s)
{
    inputSource = s;
    registerInputSource(inputSource);
    enablePassThrough(s);
}


MStructuredGrid* MVerticalRegridder::produceData(MDataRequest request)
{
    assert(inputSource != nullptr);

#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
#endif

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

    if (params[0] == "ML")
    {
        // Hybrid sigma pressure grid has been requested.
        // ==============================================

        MLonLatHybridSigmaPressureGrid *result =
                new MLonLatHybridSigmaPressureGrid(inputGrid->nlevs,
                                                   inputGrid->nlats,
                                                   inputGrid->nlons);

        // Copy coordinate axes..
        for (unsigned int i = 0; i < inputGrid->nlons; i++)
            result->lons[i] = inputGrid->lons[i];
        for (unsigned int j = 0; j < inputGrid->nlats; j++)
            result->lats[j] = inputGrid->lats[j];
        for (unsigned int k = 0; k < inputGrid->nlevs; k++)
        {
            result->levels[k] = inputGrid->levels[k];
            result->ak_hPa[k] = inputGrid->ak_hPa[k];
            result->bk[k] = inputGrid->bk[k];
        }

        // ..and take care of the surface grid.
        if (params[1] == "CONST_STANDARD_PSFC")
        {
            MDataRequestHelper rh_psfc;
            rh_psfc.insert("LEVELTYPE", SURFACE_2D);
            rh_psfc.insert("VARIABLE", "CONST_SURFACE_PRESSURE");
            rh_psfc.insert("HPA", "1013.25");
            MDataRequest psfcRequest = rh_psfc.request();

            if ( !memoryManager->containsData(this, psfcRequest) )
            {
                MRegularLonLatGrid *psfc = new MRegularLonLatGrid(
                            inputGrid->nlats, inputGrid->nlons);

                for (unsigned int i = 0; i < inputGrid->nlons; i++)
                    psfc->lons[i] = inputGrid->lons[i];
                for (unsigned int j = 0; j < inputGrid->nlats; j++)
                    psfc->lats[j] = inputGrid->lats[j];

                psfc->setToValue(101325.); // in Pa

                psfc->setGeneratingRequest(psfcRequest);
                if ( !memoryManager->storeData(this, psfc) )
                {
                    // In the unlikely event that another thread has stored
                    // the same field in the mean time delete this one.
                    delete psfc;
                }
            }

            // Get a pointer to the surface pressure field from the memory
            // manger. The field's reference counter was increased by either
            // containsData() or storeData() above. NOTE: The field is released
            // in the destructor of "result" -- the reference is kept for the
            // entire lifetime of "result" to make sure the psfc field is not
            // deleted while "result" is still in memory (notes 09Oct2013).
            result->surfacePressure = static_cast<MRegularLonLatGrid*>(
                        memoryManager->getData(this, psfcRequest) );
        }

        else if ((params[1] == "MEAN") || (params[1] == "MIN"))
        {
            rh.insert("VARIABLE", rh.value("VARIABLE") + "/PSFC");
            rh.insert("ENS_OPERATION", params[1]);

            int memberFrom = params[2].toInt();
            int memberTo   = params[3].toInt();
            rh.insert("MEMBER_RANGE",
                      QString("%1/%2").arg(memberFrom).arg(memberTo));

            result->surfacePressure =
                    dynamic_cast<MRegularLonLatGrid*>(
                        inputSource->getData(rh.request()));
        }

        // CPU-based regridding: Loop over all grid columns.
        // =================================================

        for (unsigned int i = 0; i < result->nlons; i++)
            for (unsigned int j = 0; j < result->nlats; j++)
            {
                float targetSurfacePressure_hPa =
                        result->surfacePressure->getValue(j, i) / 100.;

                for (unsigned int k = 0; k < result->nlevs; k++)
                {
                    float targetPressure_hPa = result->ak_hPa[k]
                            + result->bk[k] * targetSurfacePressure_hPa;

                    float targetValue = inputGrid->interpolateGridColumnToPressure(
                                j, i, targetPressure_hPa);

                    result->setValue(k, j, i, targetValue);
                }
            }

        regriddedField = result;
    } // hybrid sigma pressure grid requested

    else if (params[0] == "PL")
    {
        // Pressure level grid has been requested.
        // =======================================

        // Standard behaviour: Use list of pressure levels passed as
        // arguments to PL in "params".
        unsigned int numLevels = params.size() - 1;

        // Alternative: Use the hybrid levels of the input grid, but with
        // a constant surface pressure (so that they can be interpreted as
        // pressure levels).
        if (params[1] == "CONST_STANDARD_PSFC") numLevels = inputGrid->nlevs;

        // Initialise result grid.
        MRegularLonLatStructuredPressureGrid *result =
                new MRegularLonLatStructuredPressureGrid(numLevels,
                                                         inputGrid->nlats,
                                                         inputGrid->nlons);

        // Copy coordinate axes..
        for (unsigned int i = 0; i < inputGrid->nlons; i++)
            result->lons[i] = inputGrid->lons[i];
        for (unsigned int j = 0; j < inputGrid->nlats; j++)
            result->lats[j] = inputGrid->lats[j];

        // Vertical levels are either computed from the hybrid coefficients
        // of the input grid or taken from the input argument list.
        if (params[1] == "CONST_STANDARD_PSFC")
        {
            for (unsigned int k = 0; k < numLevels; k++)
            {
                result->levels[k] = inputGrid->ak_hPa[k]
                        + inputGrid->bk[k] * 1013.25;
            }
        }
        else
        {
            for (unsigned int k = 0; k < numLevels; k++)
                result->levels[k] = params[k+1].toFloat();
        }

        // CPU-based regridding: Loop over all grid columns.
        // =================================================

        for (unsigned int i = 0; i < result->nlons; i++)
            for (unsigned int j = 0; j < result->nlats; j++)
                for (unsigned int k = 0; k < result->nlevs; k++)
                {
                    float targetPressure_hPa = result->levels[k];

                    float targetValue = inputGrid->interpolateGridColumnToPressure(
                                j, i, targetPressure_hPa);

                    result->setValue(k, j, i, targetValue);
                }

        regriddedField = result;
    } // pressure level grid requested

    inputSource->releaseData(inputGrid);

#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "vertical regridding finished in "
                    << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif

    return regriddedField;
}


MTask* MVerticalRegridder::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    QString regridMode = rh.value("REGRID");
    rh.removeAll(locallyRequiredKeys());
    task->addParent(inputSource->getTaskGraph(rh.request()));

    // Depending on the regridding mode, surface pressure fields may have to
    // be requested.
    QStringList params = regridMode.split("/");
    if ((params[0] == "ML") && ((params[1] == "MEAN") || (params[1] == "MIN")))
    {
        rh.insert("VARIABLE", rh.value("VARIABLE") + "/PSFC");
        rh.insert("ENS_OPERATION", params[1]);

        int memberFrom = params[2].toInt();
        int memberTo   = params[3].toInt();
        rh.insert("MEMBER_RANGE",
                  QString("%1/%2").arg(memberFrom).arg(memberTo));

        task->addParent(inputSource->getTaskGraph(rh.request()));
    }

    return task;
}


QList<MVerticalLevelType> MVerticalRegridder::availableLevelTypes()
{
    assert(inputSource != nullptr);
    return inputSource->availableLevelTypes();
}


QStringList MVerticalRegridder::availableVariables(
        MVerticalLevelType levelType)
{
    assert(inputSource != nullptr);
    return inputSource->availableVariables(levelType);
}


QSet<unsigned int> MVerticalRegridder::availableEnsembleMembers(
        MVerticalLevelType levelType, const QString& variableName)
{
    assert(inputSource != nullptr);
    return inputSource->availableEnsembleMembers(levelType, variableName);
}


QList<QDateTime> MVerticalRegridder::availableInitTimes(
        MVerticalLevelType levelType, const QString& variableName)
{
    assert(inputSource != nullptr);
    return inputSource->availableInitTimes(levelType, variableName);
}


QList<QDateTime> MVerticalRegridder::availableValidTimes(
        MVerticalLevelType levelType,
        const QString& variableName, const QDateTime& initTime)
{
    assert(inputSource != nullptr);
    return inputSource->availableValidTimes(levelType, variableName, initTime);
}



/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MVerticalRegridder::locallyRequiredKeys()
{
    return (QStringList() << "REGRID");
}

} // namespace Met3D
