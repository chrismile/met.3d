/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2017-2018 Bianca Tost
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
#include "structuredgridensemblefilter.h"

// standard library imports
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

MStructuredGridEnsembleFilter::MStructuredGridEnsembleFilter()
    : MWeatherPredictionDataSource(),
      inputSource(nullptr)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MStructuredGridEnsembleFilter::setInputSource(MWeatherPredictionDataSource* s)
{
    inputSource = s;
    registerInputSource(inputSource);
    enablePassThrough(s);
}


MStructuredGrid* MStructuredGridEnsembleFilter::produceData(MDataRequest request)
{
    assert(inputSource != nullptr);

    MDataRequestHelper rh(request);

    // Parse request.
    QSet<unsigned int> selectedMembers = rh.uintSetValue("SELECTED_MEMBERS");
    QString operation = rh.value("ENS_OPERATION");

    rh.removeAll(locallyRequiredKeys());

    MStructuredGrid *result = nullptr;

    // Number of ensemble members in total (k) and number of ensemble
    // members that contain valid values (i.e. not M_MISSING_VALUE) for
    // each grid point.
    int k = 0;
    MStructuredGrid *validMembersCounter = nullptr;


    // Operation: MEAN, STDDEV
    // =======================
    if ((operation == "MEAN") || (operation == "STDDEV"))
    {
        /*
        Incremental computation of mean and standard deviation, together in a
        single pass.

        See Donald Knuth's "The Art of Computer Programming, Volume 2:
        Seminumerical Algorithms", section 4.2.2:

        M(1) = x(1), M(k) = M(k-1) + (x(k) - M(k-1)) / k
        S(1) = 0,    S(k) = S(k-1) + (x(k) - M(k-1)) * (x(k) - M(k))

        for 2 <= k <= n, then

        sigma = sqrt(S(n) / (n - 1))

        Also see http://mathcentral.uregina.ca/QQ/database/QQ.09.02/carlos1.html
        Also see notes 19Sep2012.
        */

        MStructuredGrid *mean   = nullptr;
        MStructuredGrid *stddev = nullptr;

        k = 0; // number of ensemble members
        foreach (unsigned int m, selectedMembers)
        {
            rh.insert("MEMBER", m);

            MStructuredGrid *memberGrid = inputSource->getData(rh.request());

            k++;
            if (k == 1)
            {
                // First iteration, k = 1.
                mean   = createAndInitializeResultGrid(memberGrid, selectedMembers);
                stddev = createAndInitializeResultGrid(memberGrid, selectedMembers);

                validMembersCounter = new MLonLatHybridSigmaPressureGrid(
                            memberGrid->nlevs, memberGrid->nlats,
                            memberGrid->nlons);
                validMembersCounter->setToZero();

                for (unsigned int v = 0; v < mean->nvalues; v++)
                {
                    if (memberGrid->data[v] != M_MISSING_VALUE)
                    {
                        validMembersCounter->data[v] += 1;
                        // M(1)         =   x(1)
                        mean->data[v]   = memberGrid->data[v];
                    }
                    else
                    {
                        mean->data[v]   = 0.;
                    }
                    // S(1)         = 0
                    stddev->data[v] = 0.;
                }
            }
            else
            {
                // Iteration k.
                updateAuxDataInResultGrid(mean, memberGrid);
                updateAuxDataInResultGrid(stddev, memberGrid);

                for (unsigned int v = 0; v < mean->nvalues; v++)
                    if (memberGrid->data[v] != M_MISSING_VALUE)
                    {
                        validMembersCounter->data[v] += 1;
                        float prev_mean = mean->data[v];
                        float curr_data = memberGrid->data[v];
                        //  M(k)        =   M(k-1)  + (   x(k)   -  M(k-1)  ) / k
                        mean->data[v]   = prev_mean + (curr_data - prev_mean) / validMembersCounter->data[v];
                        //  S(k)        =   S(k-1)        + (   x(k)   -  M(k-1)  ) * (   x(k)   -      M(k)    )
                        stddev->data[v] = stddev->data[v] + (curr_data - prev_mean) * (curr_data - mean->data[v]);
                    }
            }

            inputSource->releaseData(memberGrid);
        }

        // Divide each summed value by the number of members.
        for (unsigned int v = 0; v < stddev->nvalues; v++)
        {
            if (validMembersCounter->data[v] > 1)
            {
                //   sigma      = sqrt(     S(n)       / (n - 1))
                stddev->data[v] = sqrt(stddev->data[v] / (validMembersCounter->data[v] - 1));
            }
            else
            {
                // Sigma cannot be computed from less than two values.
                stddev->data[v] = M_MISSING_VALUE;
                // Mean is a missing value of all members contained missing
                // values for this grid point.
                if (validMembersCounter->data[v] == 0)
                    mean->data[v] = M_MISSING_VALUE;
            }
        }

        finalizeAuxDataInResultGrid(mean);
        finalizeAuxDataInResultGrid(stddev);

        // The requested field is returned as the result. However, we also
        // store the other field (mean or stddev) in the memory manager cache,
        // in case it is requested at a later time. The get/release call
        // is necessary to avoid blocking of the item in the active cache.
        if (operation == "MEAN")
        {
            result = mean;

            MDataRequestHelper rh2(request);
            rh2.insert("ENS_OPERATION", "STDDEV");
            stddev->setGeneratingRequest(rh2.request());

            if (!memoryManager->storeData(this, stddev)) delete stddev;

            // Release the data in any case; even a failed storeData() will
            // reserve an instance of the data item.
            memoryManager->releaseData(memoryManager->getData(this, rh2.request()));
        }
        else
        {
            result = stddev;

            MDataRequestHelper rh2(request);
            rh2.insert("ENS_OPERATION", "MEAN");
            mean->setGeneratingRequest(rh2.request());

            if (!memoryManager->storeData(this, mean)) delete mean;
            memoryManager->releaseData(memoryManager->getData(this, rh2.request()));
        }

    } // MEAN, STDDEV


    // Operation: MIN/MAX/delta(MAX,MIN)
    // =================================
    if ((operation == "MIN") || (operation == "MAX") || (operation == "MAX-MIN"))
    {
        // Compute min, max and max-min in a single pass.

        MStructuredGrid *minGrid = nullptr;
        MStructuredGrid *maxGrid = nullptr;
        MStructuredGrid *dmaxminGrid = nullptr;

        foreach (unsigned int m, selectedMembers)
        {
            rh.insert("MEMBER", m);

            MStructuredGrid *memberGrid = inputSource->getData(rh.request());

            if (minGrid == nullptr)
            {
                // First iteration.
                minGrid = createAndInitializeResultGrid(memberGrid, selectedMembers);
                minGrid->enableFlags(); // allocate flags bitfield
                minGrid->setToValue(M_MISSING_VALUE);
                maxGrid = createAndInitializeResultGrid(memberGrid, selectedMembers);
                maxGrid->enableFlags();
                maxGrid->setToValue(M_MISSING_VALUE);
                dmaxminGrid = createAndInitializeResultGrid(memberGrid, selectedMembers);
                dmaxminGrid->enableFlags();
                dmaxminGrid->setToValue(M_MISSING_VALUE);
            }
            else
            {
                updateAuxDataInResultGrid(minGrid, memberGrid);
                updateAuxDataInResultGrid(maxGrid, memberGrid);
                updateAuxDataInResultGrid(dmaxminGrid, memberGrid);
            }

            for (unsigned int v = 0; v < memberGrid->nvalues; v++)
                if (memberGrid->data[v] != M_MISSING_VALUE)
                {
                    if ((minGrid->data[v] == M_MISSING_VALUE)
                            || (memberGrid->data[v] < minGrid->data[v]))
                    {
                        minGrid->data[v] = memberGrid->data[v];
                        minGrid->clearFlags(v);
                        minGrid->setFlag(v, m);
                    }
                    if ((maxGrid->data[v] == M_MISSING_VALUE)
                            || (memberGrid->data[v] > maxGrid->data[v]))
                    {
                        maxGrid->data[v] = memberGrid->data[v];
                        maxGrid->clearFlags(v);
                        maxGrid->setFlag(v, m);
                    }
                }

            // Store that member m contributed to the result.
            minGrid->setContributingMember(m);
            maxGrid->setContributingMember(m);
            dmaxminGrid->setContributingMember(m);

            inputSource->releaseData(memberGrid);
        } // loop over member

        // Compute max-min.
        for (unsigned int v = 0; v < dmaxminGrid->nvalues; v++)
            if ((maxGrid->data[v] != M_MISSING_VALUE)
                    && (minGrid->data[v] != M_MISSING_VALUE))
            {
                dmaxminGrid->data[v] = maxGrid->data[v] - minGrid->data[v];
                dmaxminGrid->setFlags(v, maxGrid->getFlags(v) & minGrid->getFlags(v));
            }

        finalizeAuxDataInResultGrid(minGrid);
        finalizeAuxDataInResultGrid(maxGrid);
        finalizeAuxDataInResultGrid(dmaxminGrid);

        if (operation == "MIN")
        {
            result = minGrid;

            MDataRequestHelper rh2(request);
            rh2.insert("ENS_OPERATION", "MAX");
            maxGrid->setGeneratingRequest(rh2.request());

            if (!memoryManager->storeData(this, maxGrid)) delete maxGrid;
            memoryManager->releaseData(memoryManager->getData(this, rh2.request()));

            rh2.insert("ENS_OPERATION", "MAX-MIN");
            dmaxminGrid->setGeneratingRequest(rh2.request());

            if (!memoryManager->storeData(this, dmaxminGrid)) delete dmaxminGrid;
            memoryManager->releaseData(memoryManager->getData(this, rh2.request()));
        }

        else if (operation == "MAX")
        {
            result = maxGrid;

            MDataRequestHelper rh2(request);
            rh2.insert("ENS_OPERATION", "MIN");
            minGrid->setGeneratingRequest(rh2.request());

            if (!memoryManager->storeData(this, minGrid)) delete minGrid;
            memoryManager->releaseData(memoryManager->getData(this, rh2.request()));

            rh2.insert("ENS_OPERATION", "MAX-MIN");
            dmaxminGrid->setGeneratingRequest(rh2.request());

            if (!memoryManager->storeData(this, dmaxminGrid)) delete dmaxminGrid;
            memoryManager->releaseData(memoryManager->getData(this, rh2.request()));
        }

        else if (operation == "MAX-MIN")
        {
            result = dmaxminGrid;

            MDataRequestHelper rh2(request);
            rh2.insert("ENS_OPERATION", "MAX");
            maxGrid->setGeneratingRequest(rh2.request());

            if (!memoryManager->storeData(this, maxGrid)) delete maxGrid;
            memoryManager->releaseData(memoryManager->getData(this, rh2.request()));

            rh2.insert("ENS_OPERATION", "MIN");
            minGrid->setGeneratingRequest(rh2.request());

            if (!memoryManager->storeData(this, minGrid)) delete minGrid;
            memoryManager->releaseData(memoryManager->getData(this, rh2.request()));
        }

    } // MIN/MAX


    // Operation: PROBABILITY THRESHOLD
    // ================================
    if (operation.startsWith("P"))
    {
        // Extract threshold scalar and comparison operation from a string
        // of format "P>273.15".
        float threshold = operation.mid(2).toFloat();
        QString op = operation.at(1);

        k = 0;
        if (op == ">")
            foreach (unsigned int m, selectedMembers)
            {
                rh.insert("MEMBER", m);

                MStructuredGrid *memberGrid = inputSource->getData(rh.request());

                k++;
                if (result == nullptr)
                {
                    // First iteration.
                    result = createAndInitializeResultGrid(memberGrid, selectedMembers);
                    result->enableFlags(); // allocate flags bitfield
                    result->setToZero();
                    validMembersCounter = new MLonLatHybridSigmaPressureGrid(
                                memberGrid->nlevs, memberGrid->nlats,
                                memberGrid->nlons);
                    validMembersCounter->setToZero();
                }
                else
                {
                    updateAuxDataInResultGrid(result, memberGrid);
                }

                for (unsigned int v = 0; v < result->nvalues; v++)
                    if (memberGrid->data[v] != M_MISSING_VALUE)
                    {
                        validMembersCounter->data[v] += 1;
                        if (memberGrid->data[v] > threshold)
                        {
                            result->data[v] += 1;
                            result->setFlag(v, m);
                        }
                    }

                // Store that member m contributed to the result.
                result->setContributingMember(m);

                inputSource->releaseData(memberGrid);
            }

        else if (op == "<")
            foreach (unsigned int m, selectedMembers)
            {
                rh.insert("MEMBER", m);

                MStructuredGrid *memberGrid = inputSource->getData(rh.request());

                k++;
                if (result == nullptr)
                {
                    // First iteration.
                    result = createAndInitializeResultGrid(memberGrid, selectedMembers);
                    result->enableFlags(); // allocate flags bitfield
                    result->setToZero();
                    validMembersCounter = new MLonLatHybridSigmaPressureGrid(
                                memberGrid->nlevs, memberGrid->nlats,
                                memberGrid->nlons);
                    validMembersCounter->setToZero();
                }
                else
                {
                    updateAuxDataInResultGrid(result, memberGrid);
                }

                for (unsigned int v = 0; v < result->nvalues; v++)
                    if (memberGrid->data[v] != M_MISSING_VALUE)
                    {
                        validMembersCounter->data[v] += 1;
                        if (memberGrid->data[v] < threshold)
                        {
                            result->data[v] += 1;
                            result->setFlag(v, m);
                        }
                    }

                // Store that member m contributed to the result.
                result->setContributingMember(m);

                inputSource->releaseData(memberGrid);
            }

        else
            LOG4CPLUS_ERROR(mlog, "Unsupported probability operation: "
                            << op.toStdString()
                            << ". No probability field has been computed.");

        // Divide by the number of members to get a probability.
        if (result != nullptr)
            for (unsigned int v = 0; v < result->nvalues; v++)
            {
                result->data[v] /= validMembersCounter->data[v];
                //result->data[v] *= 100.; // to %
            }

        finalizeAuxDataInResultGrid(result);
    } // PROBABILITY THRESHOLD

    if (validMembersCounter) delete validMembersCounter;
    return result;
}


MTask* MStructuredGridEnsembleFilter::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    QSet<unsigned int> selectedMembers = rh.uintSetValue("SELECTED_MEMBERS");
    rh.removeAll(locallyRequiredKeys());

    foreach (unsigned int m, selectedMembers)
    {
        rh.insert("MEMBER", m);
        task->addParent(inputSource->getTaskGraph(rh.request()));
    }

    return task;
}


QList<MVerticalLevelType> MStructuredGridEnsembleFilter::availableLevelTypes()
{
    assert(inputSource != nullptr);
    return inputSource->availableLevelTypes();
}


QStringList MStructuredGridEnsembleFilter::availableVariables(
        MVerticalLevelType levelType)
{
    assert(inputSource != nullptr);
    return inputSource->availableVariables(levelType);
}


QSet<unsigned int> MStructuredGridEnsembleFilter::availableEnsembleMembers(
        MVerticalLevelType levelType, const QString& variableName)
{
    assert(inputSource != nullptr);
    return inputSource->availableEnsembleMembers(levelType, variableName);
}


QList<QDateTime> MStructuredGridEnsembleFilter::availableInitTimes(
        MVerticalLevelType levelType, const QString& variableName)
{
    assert(inputSource != nullptr);
    return inputSource->availableInitTimes(levelType, variableName);
}


QList<QDateTime> MStructuredGridEnsembleFilter::availableValidTimes(
        MVerticalLevelType levelType,
        const QString& variableName, const QDateTime& initTime)
{
    assert(inputSource != nullptr);
    return inputSource->availableValidTimes(levelType, variableName, initTime);
}


QString MStructuredGridEnsembleFilter::variableLongName(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    assert(inputSource != nullptr);
    return inputSource->variableLongName(levelType, variableName);
}


QString MStructuredGridEnsembleFilter::variableStandardName(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    assert(inputSource != nullptr);
    return inputSource->variableStandardName(levelType, variableName);
}


QString MStructuredGridEnsembleFilter::variableUnits(
        MVerticalLevelType levelType,
        const QString& variableName)
{
    assert(inputSource != nullptr);
    return inputSource->variableUnits(levelType, variableName);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MStructuredGridEnsembleFilter::locallyRequiredKeys()
{
    return (QStringList() << "ENS_OPERATION" << "SELECTED_MEMBERS");
}


MStructuredGrid *MStructuredGridEnsembleFilter::createAndInitializeResultGrid(MStructuredGrid *templateGrid,
        const QSet<unsigned int> &selectedMembers)
{
    MStructuredGrid *result = nullptr;

    // Initialize the result grid from the template grid.
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
    {
        MLonLatAuxiliaryPressureGrid *templateGridAuxCast =
                dynamic_cast<MLonLatAuxiliaryPressureGrid*>(templateGrid);
        result = new MLonLatAuxiliaryPressureGrid(
                    templateGrid->nlevs, templateGrid->nlats,
                    templateGrid->nlons, templateGridAuxCast->reverseLevels);
        break;
    }
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

    // Special case: HYBRID_SIGMA_PRESSURE_3D
    // ======================================

    if (templateGrid->leveltype == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Special treatment for hybrid model levels: copy ak/bk coeffs.
        MLonLatHybridSigmaPressureGrid *hybTemplate =
                dynamic_cast<MLonLatHybridSigmaPressureGrid*>(templateGrid);
        MLonLatHybridSigmaPressureGrid *hybResult =
                dynamic_cast<MLonLatHybridSigmaPressureGrid*>(result);
        for (unsigned int i = 0; i < hybTemplate->nlevs; i++)
        {
            hybResult->ak_hPa[i] = hybTemplate->ak_hPa[i];
            hybResult->bk[i] = hybTemplate->bk[i];
        }

        // ..and take care of the surface grid:
        // If a list of ensemble members is specified in "selectedMembers" and
        // the keyword "MEMBER" is contained in the psfc request of the
        // template member grid, it is assumed that all input grids are defined
        // on different surface grids. If "selectedMembers" is empty, or if
        // "MEMBER" is not present in the request (e.g. the input members are
        // regridded to a common grid), simply take the surface grid of the
        // template grid.

        MDataRequest psfcRequest = hybTemplate->getSurfacePressureGrid()
                ->getGeneratingRequest();
        MDataRequestHelper rh_psfc(psfcRequest);

        if (!selectedMembers.isEmpty() && rh_psfc.contains("MEMBER"))
        {
            rh_psfc.remove("MEMBER");
            rh_psfc.insert("SELECTED_MEMBERS", selectedMembers);
            rh_psfc.insert("ENS_OPERATION", "MULTIMEMBER_AUX_REFERENCE");
            psfcRequest = rh_psfc.request();

            if (memoryManager->containsData(this, psfcRequest))
            {
                // Multimember reference computation (currently the ensemble
                // mean) of the surface pressure field is already available
                // in the memory manager. Simply use this field.

                // Get a pointer to the surface pressure field from the
                // memory manager.
                hybResult->surfacePressure = static_cast<MRegularLonLatGrid*>(
                            memoryManager->getData(this, psfcRequest) );

                // NOTE: If this already existing field is used the two methods
                // updateAuxDataInResultGrid() and finalizeAuxDataInResultGrid()
                // will recognize that the surface pressure grid is already
                // memory managed and do nothing.
            }
            else
            {
                // The multimember reference needs to be computed. Initialize
                // the corresponding fields. In this case, the additional
                // methods updateAuxDataInResultGrid() and
                // finalizeAuxDataInResultGrid() should carry out the required
                // computations.

                // Initialize new surface pressure field from the surface
                // pressure field of the current template grid.
                MRegularLonLatGrid *psfcTemplate =
                        hybTemplate->getSurfacePressureGrid();
                MRegularLonLatGrid *psfc = dynamic_cast<MRegularLonLatGrid*>(
                            createAndInitializeResultGrid(psfcTemplate));

                psfc->setGeneratingRequest(psfcRequest);
                hybResult->surfacePressure = psfc;

                // NOTE: The grid is not yet stored in the memory manager!
                // This is done in finalizeAuxDataInResultGrid().

                // Initialize field to store the number of valid members
                // that contribute to the ensemble mean computation (see
                // updateAuxDataInResultGrid() and
                // finalizeAuxDataInResultGrid() ).
                MStructuredGrid *validMembersCounter = new MRegularLonLatGrid(
                            result->nlats, result->nlons);
                resultAuxComputationValidMembersCounter.insert(
                            psfc, validMembersCounter);
                validMembersCounter->setToZero();

//TODO (mr): Is it correct to reference all ensemble filter results to the mean
//      surface pressure field?

                // Initial iteration of computation of surface pressure mean.
                for (unsigned int v = 0; v < psfc->nvalues; v++)
                {
                    if (psfcTemplate->data[v] != M_MISSING_VALUE)
                    {
                        validMembersCounter->data[v] += 1;
                        // M(1)         =   x(1)
                        psfc->data[v]   = psfcTemplate->data[v];
                    }
                    else
                    {
                        psfc->data[v]   = 0.;
                    }
                }
            }
        } // get or create new multimember reference surface pressure field

        else
        {
            // Use the surface grid of the template grid.
            hybResult->surfacePressure = hybTemplate->getSurfacePressureGrid();

            // NOTE: If this already existing field is used the two methods
            // updateAuxDataInResultGrid() and finalizeAuxDataInResultGrid()
            // will recognize that the surface pressure grid is already
            // memory managed and do nothing.

            // Increase the reference counter for this field. NOTE: The field
            // is released in the destructor of "result" -- the reference is
            // kept for the entire lifetime of "result" to make sure the psfc
            // field is not deleted while "result" is still in memory.
            if ( !hybResult->surfacePressure->increaseReferenceCounter() )
            {
                // This should not happen.
                QString msg = QString("This is embarrassing: The data item "
                                      "for request %1 should have been in "
                                      "cache.").arg(psfcRequest);
                throw MMemoryError(msg.toStdString(), __FILE__, __LINE__);
            }
        } // copy pointer to template surface pressure field

    } // grid is hybrid sigma pressure


    // Special case: AUXILIARY_PRESSURE_3D
    // ===================================

    else if (templateGrid->leveltype == AUXILIARY_PRESSURE_3D)
    {
        // Special treatment for auxiliary pressure levels: copy pointer to
        // auxiliary 3D pressure field.

        MLonLatAuxiliaryPressureGrid *auxPTemplate =
                dynamic_cast<MLonLatAuxiliaryPressureGrid*>(templateGrid);
        MLonLatAuxiliaryPressureGrid *auxPResult =
                dynamic_cast<MLonLatAuxiliaryPressureGrid*>(result);

        // The following is similar to the surface pressure field above.

        MDataRequest auxPRequest = auxPTemplate->getAuxiliaryPressureFieldGrid()
                ->getGeneratingRequest();
        MDataRequestHelper rh_auxP(auxPRequest);

        if (!selectedMembers.isEmpty() && rh_auxP.contains("MEMBER"))
        {
            rh_auxP.remove("MEMBER");
            rh_auxP.insert("SELECTED_MEMBERS", selectedMembers);
            rh_auxP.insert("ENS_OPERATION", "MULTIMEMBER_AUX_REFERENCE");
            auxPRequest = rh_auxP.request();

            if (memoryManager->containsData(this, auxPRequest))
            {
                // See comments for surface pressure above.
                auxPResult->auxPressureField_hPa =
                        static_cast<MLonLatAuxiliaryPressureGrid*>(
                            memoryManager->getData(this, auxPRequest) );
            }
            else
            {
                // See comments for surface pressure above.
                MLonLatAuxiliaryPressureGrid *auxPOfTemplateGrid =
                        auxPTemplate->getAuxiliaryPressureFieldGrid();
                MLonLatAuxiliaryPressureGrid *auxP =
                        dynamic_cast<MLonLatAuxiliaryPressureGrid*>(
                            createAndInitializeResultGrid(auxPOfTemplateGrid));

                auxP->setGeneratingRequest(auxPRequest);
                auxP->auxPressureField_hPa = auxP; // self-reference
                auxPResult->auxPressureField_hPa = auxP;

                MStructuredGrid *validMembersCounter =
                        new MLonLatHybridSigmaPressureGrid(
                            result->nlevs, result->nlats, result->nlons);
                resultAuxComputationValidMembersCounter.insert(
                            auxP, validMembersCounter);
                validMembersCounter->setToZero();

                // Initial iteration of computation of aux pressure mean.
                for (unsigned int v = 0; v < auxP->nvalues; v++)
                {
                    if (auxPOfTemplateGrid->data[v] != M_MISSING_VALUE)
                    {
                        validMembersCounter->data[v] += 1;
                        // M(1)         =   x(1)
                        auxP->data[v]   = auxPOfTemplateGrid->data[v];
                    }
                    else
                    {
                        auxP->data[v]   = 0.;
                    }
                }
            }
        } // get or create new multimember reference aux pressure field

        else
        {
            // See comments for surface pressure above.
            auxPResult->auxPressureField_hPa =
                    auxPTemplate->getAuxiliaryPressureFieldGrid();

            if ( !auxPResult->auxPressureField_hPa->increaseReferenceCounter() )
            {
                QString msg = QString("This is embarrassing: The data item "
                                      "for request %1 should have been in "
                                      "cache.").arg(auxPRequest);
                throw MMemoryError(msg.toStdString(), __FILE__, __LINE__);
            }
        } // copy pointer to template aux pressure field

   } // grid is auxiliary pressure 3D

    return result;
}


void MStructuredGridEnsembleFilter::updateAuxDataInResultGrid(
        MStructuredGrid *resultGrid, MStructuredGrid *currentMemberGrid)
{
    MStructuredGrid *resultAuxGrid = nullptr;
    MStructuredGrid *memberAuxGrid = nullptr;

    // Iteratively compute the mean of either ...

    if (MLonLatHybridSigmaPressureGrid *hybridResult =
            dynamic_cast<MLonLatHybridSigmaPressureGrid*>(resultGrid))
    {
        if (MLonLatHybridSigmaPressureGrid *hybridMember =
                dynamic_cast<MLonLatHybridSigmaPressureGrid*>(currentMemberGrid))
        {
            // ... surface pressure for hybrid grids ...
            resultAuxGrid = hybridResult->getSurfacePressureGrid();
            memberAuxGrid = hybridMember->getSurfacePressureGrid();
        }
    }
    else if (MLonLatAuxiliaryPressureGrid *auxPResult =
             dynamic_cast<MLonLatAuxiliaryPressureGrid*>(resultGrid))
    {
        if (MLonLatAuxiliaryPressureGrid *auxPMember =
                dynamic_cast<MLonLatAuxiliaryPressureGrid*>(currentMemberGrid))
        {
            // ... or auxiliary pressure for aux-p grids.
            resultAuxGrid = auxPResult->getAuxiliaryPressureFieldGrid();
            memberAuxGrid = auxPMember->getAuxiliaryPressureFieldGrid();
        }
    }

    // If the result grid is either hybrid or aux-p, add the increment to the
    // ensemble mean. If the grid is of another type, nothing will be done here.
    if (resultAuxGrid != nullptr && memberAuxGrid != nullptr)
    {
        if (resultAuxGrid->getMemoryManager() != nullptr)
        {
            // The auxiliary grid is already memory-managed, i.e., the grid
            // corresponding to the required aux-request was already computed
            // when createAndInitializeResultGrid() was
            // called. We don't need to carry out any operations
            // here anymore and can safely skip.
            return;
        }

        MStructuredGrid *validMembersCounter =
                resultAuxComputationValidMembersCounter[resultAuxGrid];
        for (unsigned int v = 0; v < resultAuxGrid->nvalues; v++)
        {
            if (memberAuxGrid->data[v] != M_MISSING_VALUE)
            {
                validMembersCounter->data[v] += 1;
                float prev_mean = resultAuxGrid->data[v];
                float curr_data = memberAuxGrid->data[v];
                //  M(k)                 =   M(k-1)  + (   x(k)   -  M(k-1)  ) / k
                resultAuxGrid->data[v]   = prev_mean + (curr_data - prev_mean) /
                        validMembersCounter->data[v];
            }
        }
    }
}


void MStructuredGridEnsembleFilter::finalizeAuxDataInResultGrid(
        MStructuredGrid *resultGrid)
{
    // Finalize computation of ensemble mean of either surface pressure
    // field or auxiliary pressure field by checking for missing values.
    MStructuredGrid *resultAuxGrid = nullptr;

    if (MLonLatHybridSigmaPressureGrid *hybridResult =
            dynamic_cast<MLonLatHybridSigmaPressureGrid*>(resultGrid))
    {
        resultAuxGrid = hybridResult->getSurfacePressureGrid();
    }
    else if (MLonLatAuxiliaryPressureGrid *auxPResult =
             dynamic_cast<MLonLatAuxiliaryPressureGrid*>(resultGrid))
    {
        resultAuxGrid = auxPResult->getAuxiliaryPressureFieldGrid();
    }

    // If the result grid is neither hybrid sigma pressure nor auxiliary
    // pressure, resultAuxGrid is still a nullptr at this time.
    if (resultAuxGrid != nullptr)
    {
        if (resultAuxGrid->getMemoryManager() != nullptr)
        {
            // See comment in updateAuxDataInResultGrid(); handling for grids
            // that were already computed when createAndInitializeResultGrid()
            // was called.
            return;
        }

        // Correct missing values (set missing value for those grid points at
        // which no member has contributed).
        MStructuredGrid *validMembersCounter =
                resultAuxComputationValidMembersCounter[resultAuxGrid];
        for (unsigned int v = 0; v < resultAuxGrid->nvalues; v++)
        {
            if (validMembersCounter->data[v] == 0)
            {
                resultAuxGrid->data[v] = M_MISSING_VALUE;
            }
        }

        // Free valid members counter.
        if (resultAuxComputationValidMembersCounter.contains(resultAuxGrid))
        {
            MStructuredGrid* counter =
                    resultAuxComputationValidMembersCounter.take(resultAuxGrid);
            delete counter;
        }

        // Store aux grid in memory manager. The call to "storeData()" will
        // place an initial reference of "1" on the item, hence upon success
        // everything is fine. In case "storeData()" fails (e.g. in the
        // unlikely event that another thread has stored a field with the same
        // request in the mean time, this one needs to be deleted and a
        // reference to the already stored field needs to be obtained.
        if ( !memoryManager->storeData(this, resultAuxGrid) )
        {
            MDataRequest auxRequest = resultAuxGrid->getGeneratingRequest();
            delete resultAuxGrid;

            if (MLonLatHybridSigmaPressureGrid *hybridResult =
                    dynamic_cast<MLonLatHybridSigmaPressureGrid*>(resultGrid))
            {
                hybridResult->surfacePressure = static_cast<MRegularLonLatGrid*>(
                            memoryManager->getData(this, auxRequest) );
            }
            else if (MLonLatAuxiliaryPressureGrid *auxPResult =
                     dynamic_cast<MLonLatAuxiliaryPressureGrid*>(resultGrid))
            {
                auxPResult->auxPressureField_hPa =
                        static_cast<MLonLatAuxiliaryPressureGrid*>(
                            memoryManager->getData(this, auxRequest) );
            }
        }
    } // resultAuxGrid != nullptr
}


} // namespace Met3D
