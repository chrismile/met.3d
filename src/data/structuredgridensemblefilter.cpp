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

    bool doNotReleaseInputDatafields = false;
    if ( rh.contains("DO_NOT_RELEASE_INPUT_DATAFIELDS") )
    {
        // If this is true: Special case if produceData() is called from
        // createAndInitializeResultGrid(). Do NOT release data fields in this
        // case; see comments below in createAndInitializeResultGrid().
        doNotReleaseInputDatafields = true;
        rh.remove("DO_NOT_RELEASE_INPUT_DATAFIELDS");

        // Correct the request string, as it is used below to derive request
        // keys for storing additional mean/stddev/min/max/etc fields.
        request = rh.request();
    }

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

            if ( !doNotReleaseInputDatafields )
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

            if ( !doNotReleaseInputDatafields )
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

                if ( !doNotReleaseInputDatafields )
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

                if ( !doNotReleaseInputDatafields )
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


MStructuredGrid *MStructuredGridEnsembleFilter::createAndInitializeResultGrid(
        MStructuredGrid *firstMemberGrid,
        const QSet<unsigned int> &selectedMembers)
{
    MStructuredGrid *result = nullptr;

    // Initialize the result grid when the first member grid is
    // available.
    switch (firstMemberGrid->leveltype)
    {
    case PRESSURE_LEVELS_3D:
        result = new MRegularLonLatStructuredPressureGrid(
                    firstMemberGrid->nlevs, firstMemberGrid->nlats,
                    firstMemberGrid->nlons);
        break;
    case HYBRID_SIGMA_PRESSURE_3D:
        result = new MLonLatHybridSigmaPressureGrid(
                    firstMemberGrid->nlevs, firstMemberGrid->nlats,
                    firstMemberGrid->nlons);
        break;
    case AUXILIARY_PRESSURE_3D:
        break;
    case POTENTIAL_VORTICITY_2D:
        break;
    case SURFACE_2D:
        result = new MRegularLonLatGrid(firstMemberGrid->nlats,
                                        firstMemberGrid->nlons);
        break;
    case LOG_PRESSURE_LEVELS_3D:
        result = new MRegularLonLatLnPGrid(
                    firstMemberGrid->nlevs, firstMemberGrid->nlats,
                    firstMemberGrid->nlons);
        break;
    default:
        break;
    }

    if (result == nullptr)
    {
        QString msg = QString("ERROR: Cannot intialize result grid. Level "
                              "type %1 not implemented.")
                .arg(MStructuredGrid::verticalLevelTypeToString(
                         firstMemberGrid->leveltype));
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }

    // Copy coordinate axes.
    for (unsigned int i = 0; i < firstMemberGrid->nlons; i++)
        result->lons[i] = firstMemberGrid->lons[i];
    for (unsigned int j = 0; j < firstMemberGrid->nlats; j++)
        result->lats[j] = firstMemberGrid->lats[j];
    for (unsigned int i = 0; i < firstMemberGrid->nlevs; i++)
        result->levels[i] = firstMemberGrid->levels[i];

    result->setAvailableMembers(firstMemberGrid->getAvailableMembers());

    if (firstMemberGrid->leveltype == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Special treatment for hybrid model levels: copy ak/bk coeffs.
        MLonLatHybridSigmaPressureGrid *hybmember =
                dynamic_cast<MLonLatHybridSigmaPressureGrid*>(firstMemberGrid);
        MLonLatHybridSigmaPressureGrid *hybresult =
                dynamic_cast<MLonLatHybridSigmaPressureGrid*>(result);
        for (unsigned int i = 0; i < hybmember->nlevs; i++)
        {
            hybresult->ak_hPa[i] = hybmember->ak_hPa[i];
            hybresult->bk[i] = hybmember->bk[i];
        }

        // ..and take care of the surface grid.
        MDataRequestHelper rh_psfc(hybmember->getSurfacePressureGrid()
                                   ->getGeneratingRequest());

        // If the keyword "MEMBER" is contained in the psfc request of the
        // first member grid, it is assumed that all input grids are defined on
        // different surface grids. If "MEMBER" is not present in the request
        // (e.g. the input members are regridded to a common grid), simply take
        // the surface grid of the first member grid.
//TODO: This only works if input source and this ensemble filter share the same
//      memory manager. A possible solution might be to give MAbstractDataItem a
//      "getMemoryManager()" method and use that mem.mgr. here.
        MDataRequest psfcRequest;
        if (!selectedMembers.isEmpty() && rh_psfc.contains("MEMBER"))
        {
            rh_psfc.remove("MEMBER");
            rh_psfc.insert("SELECTED_MEMBERS", selectedMembers);
//TODO: Is it correct to reference all ensemble filter results to the mean
//      surface pressure field?
            rh_psfc.insert("ENS_OPERATION", "MEAN");
            psfcRequest = rh_psfc.request();

            if ( !memoryManager->containsData(this, psfcRequest) )
            {
                // Surface pressure mean field is not available in cache --
                // call produceData() to compute it.

                // NOTE: This is a special case, as produceData() should
                // usually only be called from task graph processing in
                // processRequest(). produceData() only calls the memory
                // manager's getData() methods; the reference counter of the
                // corresponding fields is increased from processRequest().
                // However, data fields are released in produceData(). Hence,
                // if we call produceData() directly the data fields do NOT
                // have to be released! Otherwise we'd be missing a few
                // afterwards --> crash.
                rh_psfc.insert("DO_NOT_RELEASE_INPUT_DATAFIELDS", "");

                MRegularLonLatGrid *psfc = static_cast<MRegularLonLatGrid*>(
                            produceData(rh_psfc.request()) );

                psfc->setGeneratingRequest(psfcRequest);
                if ( !memoryManager->storeData(this, psfc) )
                {
                    // In the unlikely event that another thread has stored
                    // the same field in the mean time delete this one.
                    delete psfc;
                }
            }

            // Get a pointer to the surface pressure field from the memory manager.
            hybresult->surfacePressure = static_cast<MRegularLonLatGrid*>(
                        memoryManager->getData(this, psfcRequest) );
        } // get or create new ensemble mean surface pressure field
        else
        {
            hybresult->surfacePressure = hybmember->getSurfacePressureGrid();

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
                                      "cache.").arg(psfcRequest);
                throw MMemoryError(msg.toStdString(), __FILE__, __LINE__);
            }
        } // copy hybmember surface pressure field
    } // grid is hybrid sigma pressure

    return result;
}


} // namespace Met3D
