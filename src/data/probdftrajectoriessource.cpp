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
#include "probdftrajectoriessource.h"

// standard library imports
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <gsl/gsl_interp.h>

// local application imports
#include "util/mutil.h"
#include "util/mstopwatch.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MProbDFTrajectoriesSource::MProbDFTrajectoriesSource()
    : MWeatherPredictionDataSource(),
      trajectorySource(nullptr),
      inputSelectionSource(nullptr)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MProbDFTrajectoriesSource::setTrajectorySource(MTrajectoryDataSource* s)
{
    trajectorySource = s;
    registerInputSource(trajectorySource);
}


void MProbDFTrajectoriesSource::setInputSelectionSource(
        MTrajectorySelectionSource *s)
{
    inputSelectionSource = s;
    registerInputSource(inputSelectionSource);
}


MStructuredGrid* MProbDFTrajectoriesSource::produceData(
        MDataRequest request)
{
#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
#endif

    assert(inputSelectionSource != nullptr);
    assert(trajectorySource != nullptr);

    MDataRequestHelper rh(request);

    // Parse request.
    MVerticalLevelType levtype = MVerticalLevelType(rh.intValue("LEVELTYPE"));
    QDateTime validTime        = rh.timeValue("VALID_TIME");
    bool tryPrecomputed        = bool(rh.intValue("TRY_PRECOMPUTED"));
    QStringList member         = rh.value("PWCB_ENSEMBLE_MEMBER").split("/");
    int memberFrom             = member[0].toInt();
    int memberTo               = member[1].toInt();

    rh.removeAll(locallyRequiredKeys());
    rh.insert("VALID_TIME", validTime);
    rh.insert("FILTER_TIMESTEP", validTime);

    if (tryPrecomputed)
    {
        // If precomputed delta-pressure values should be used, only the
        // trajectory positions at validTime need to be read.
        /**
        @note -- mr, 15Jan2014 -- See below in createTaskGraph().
        */
        rh.insert("TRY_PRECOMPUTED", 1);
        rh.insert("TIME_SPAN", validTime);
    }
    else
    {
        rh.insert("TRY_PRECOMPUTED", 0);
        rh.insert("TIME_SPAN", "ALL");
    }


    MStructuredGrid *result = nullptr;

    // Gridding helper.
    int nlon = 0; int nlat = 0; int nlev = 0;
    gsl_interp_accel *lonAccel = gsl_interp_accel_alloc();
    gsl_interp_accel *latAccel = gsl_interp_accel_alloc();
    gsl_interp_accel *levAccel = gsl_interp_accel_alloc();
    double *gridNodesLon = nullptr;
    double *gridNodesLat = nullptr;
    double *gridNodesLev = nullptr;

    float probPerMember = 1. / (memberTo-memberFrom+1);

    // Loop over all ensemble members and accumulate probability information.
    for (int m = memberFrom; m <= memberTo; m++)
    {
        rh.insert("MEMBER", m);

        MTrajectorySelection *selection =
                inputSelectionSource->getData(rh.request());
        MTrajectories *trajectories =
                trajectorySource->getData(rh.request());

        if (result == nullptr)
        {
            // Initialize the target grid when the first trajectory dataset
            // is available.

            QVector3D stride = selection->getStartGridStride();
            int sx = stride.x();
            int sy = stride.y();
            int sz = stride.z();

            // The result grid has the same dimensions as the trajectory start
            // grid.
            shared_ptr<MStructuredGrid> startGrid = trajectories->getStartGrid();
            nlon = ceil(float(startGrid->getNumLons()) / sx);
            nlat = ceil(float(startGrid->getNumLats()) / sy);
            nlev = ceil(float(startGrid->getNumLevels()) / sz);

            // Initialise the result grid according to the requested level
            // type and copy coordinate data from the start grid.
            switch (levtype)
            {
            case PRESSURE_LEVELS_3D:
                result = new MRegularLonLatStructuredPressureGrid(nlev, nlat, nlon);
                if (startGrid->getLevels()[0] > startGrid->getLevels()[nlev-1])
                    for (int i = 0; i < nlev; i++)
                        result->setLevel(i, startGrid->getLevels()[(nlev-1)*sz-i*sz]);
                else
                    for (int i = 0; i < nlev; i++)
                        result->setLevel(i, startGrid->getLevels()[i*sz]);
                break;

            case LOG_PRESSURE_LEVELS_3D:
                result = new MRegularLonLatLnPGrid(nlev, nlat, nlon);
                if (startGrid->getLevels()[0] > startGrid->getLevels()[nlev-1])
                    for (int i = 0; i < nlev; i++)
                        result->setLevel(i, log(startGrid->getLevels()[(nlev-1)*sz-i*sz]));
                else
                    for (int i = 0; i < nlev; i++)
                        result->setLevel(i, log(startGrid->getLevels()[i*sz]));
                break;

            case HYBRID_SIGMA_PRESSURE_3D:
            {
                MLonLatHybridSigmaPressureGrid *hybGrid =
                        new MLonLatHybridSigmaPressureGrid(nlev, nlat, nlon);
                shared_ptr<MLonLatHybridSigmaPressureGrid> hybStartGrid
                        = dynamic_pointer_cast<MLonLatHybridSigmaPressureGrid>(startGrid);
                if (startGrid->getLevels()[0] > startGrid->getLevels()[nlev-1])
                    for (int i = 0; i < nlev; i++)
                    {
                        hybGrid->setLevel(i, startGrid->getLevels()[(nlev-1)*sz-i*sz]);
                        hybGrid->ak_hPa[i] = hybStartGrid->ak_hPa[(nlev-1)*sz-i*sz];
                        hybGrid->bk[i] = hybStartGrid->bk[(nlev-1)*sz-i*sz];
                    }
                else
                    for (int i = 0; i < nlev; i++)
                    {
                        hybGrid->setLevel(i, startGrid->getLevels()[i*sz]);
                        hybGrid->ak_hPa[i] = hybStartGrid->ak_hPa[i*sz];
                        hybGrid->bk[i] = hybStartGrid->bk[i*sz];
                    }

                // The surface pressure field is stored in "startGrid"; however,
                // not automatically stored in the memory manager connected to
                // this filter module. Create a request for this field; if it
                // hasn't been stored by another request create a new field
                // and store it.
                MDataRequestHelper rh_psfc(request);
                rh_psfc.removeAll(locallyRequiredKeys());
//FIXME: Revise the request construction -- which keys are required to uniquely
//       identify the surface pressure field? (mr, 01Aug2014)
                rh_psfc.remove("MEMBER");
                rh_psfc.remove("FILTER_PRESSURE_TIME");
                rh_psfc.insert("LEVELTYPE", SURFACE_2D);
                rh_psfc.insert("VALID_TIME", validTime);
                rh_psfc.insert("VARIABLE", "ProbabilityOfTrajectoryOccurence/PSFC");
                MDataRequest psfcRequest = rh_psfc.request();

                if ( !memoryManager->containsData(this, psfcRequest) )
                {
                    // Surface pressure mean field is not available in cache --
                    // create a new one.
                    MRegularLonLatGrid *psfc = new MRegularLonLatGrid(nlat, nlon);
                    MRegularLonLatGrid *psfcStart = hybStartGrid->getSurfacePressureGrid();
                    for (int i = 0; i < nlon; i++)
                        psfc->setLon(i, psfcStart->getLons()[i*sx]);
                    for (int j = 0; j < nlat; j++)
                        psfc->setLat(j, psfcStart->getLats()[j*sy]);
                    for (int i = 0; i < nlon; i++)
                        for (int j = 0; j < nlat; j++)
                            psfc->setValue(j, i, psfcStart->getValue(j*sy, i*sx));

                    psfc->setGeneratingRequest(psfcRequest);
                    if ( !memoryManager->storeData(this, psfc) )
                    {
                        // In the unlikely event that another thread has stored
                        // the same field in the mean time delete this one.
                        delete psfc;
                    }
                }

                // Get a pointer to the surface pressure field from the memory
                // manager (the field's reference counter is increased in
                // containsData() or storeData()). The surface pressure field
                // is released upon hybGrid destruction.
                hybGrid->surfacePressure = static_cast<MRegularLonLatGrid*>(
                            memoryManager->getData(this, psfcRequest) );

                result = hybGrid;
                break;
            }

            default:
                break;

            } // switch (levtype)

            for (int i = 0; i < nlon; i++)
                result->setLon(i, startGrid->getLons()[i*sx]);
            for (int i = 0; i < nlat; i++)
                result->setLat(i, startGrid->getLats()[i*sy]);

            result->setToZero();
            result->enableFlags(); // allocate flags bitfield

            for (int i = 0; i < trajectorySource->availableEnsembleMembers().size(); i++)
                result->setAvailableMember(trajectorySource->availableEnsembleMembers()[i]);

            // Compute gridding nodes (the midpoints between the grid point
            // coordinates); used for the gsl_interp routine below.
            gridNodesLon = new double[nlon+1];
            gridNodesLon[0] = result->getLons()[0] -
                    (result->getLons()[1] - result->getLons()[0]) / 2.;
            for (int i = 1; i < nlon; i++)
                gridNodesLon[i] = (result->getLons()[i-1] + result->getLons()[i]) / 2.;
            gridNodesLon[nlon] = result->getLons()[nlon-1] +
                    (result->getLons()[nlon-1] - result->getLons()[nlon-2]) / 2.;

            gridNodesLat = new double[nlat+1];
            gridNodesLat[nlat] = result->getLats()[0] -
                    (result->getLats()[1] - result->getLats()[0]) / 2.;
            for (int i = 1; i < nlat; i++)
                gridNodesLat[nlat-i] = (result->getLats()[i-1] + result->getLats()[i]) / 2.;
            gridNodesLat[0] = result->getLats()[nlat-1] +
                    (result->getLats()[nlat-1] - result->getLats()[nlat-2]) / 2.;

            gridNodesLev = new double[nlev+1];
            gridNodesLev[0] = result->getLevels()[0] -
                    (result->getLevels()[1] - result->getLevels()[0]) / 2.;
            for (int i = 1; i < nlev; i++)
                gridNodesLev[i] = (result->getLevels()[i-1] + result->getLevels()[i]) / 2.;
            gridNodesLev[nlev] = result->getLevels()[nlev-1] +
                    (result->getLevels()[nlev-1] - result->getLevels()[nlev-2]) / 2.;

            // result->dumpCoordinateAxes();
        } // initialize target grid

        int numPositions = selection->getNumTrajectories();
        for (int i = 0; i < numPositions; i++)
        {
            QVector3D p = trajectories->getVertices().at(
                        selection->getStartIndices()[i]);

            // Domain checks.
            if (p.x() < gridNodesLon[0]) continue;
            if (p.x() >= gridNodesLon[nlon]) continue;

            if (p.y() > gridNodesLat[nlat]) continue;
            if (p.y() <= gridNodesLat[0]) continue;

            // Find grid indices.
            int ilon = gsl_interp_accel_find(lonAccel, gridNodesLon, nlon+1, p.x());
            int ilat = nlat-1 - gsl_interp_accel_find(latAccel, gridNodesLat, nlat+1, p.y());

            // The same for the vertical coordinate.
            int ilev = -1;
            if (levtype == HYBRID_SIGMA_PRESSURE_3D)
            {
                MLonLatHybridSigmaPressureGrid *hybResult =
                        dynamic_cast<MLonLatHybridSigmaPressureGrid*>(result);

                if (p.z() < hybResult->getTopInterfacePressure(0, ilat, ilon)) continue;
                if (p.z() >= hybResult->getBottomInterfacePressure(nlev-1, ilat, ilon)) continue;

                ilev = hybResult->findClosestLevel(ilat, ilon, p.z());

                // LOG4CPLUS_DEBUG(mlog, i << ": " << p.x() << " " << p.y() << " "
                //                 << p.z() << " || " << ilon
                //                 << " " << ilat << " " << ilev
                //                 << " || grid_p="
                //                 << hybResult->getPressure(ilev, ilat, ilon)
                //                 << " |t " << ((ilev > 0) ? hybResult->getPressure(ilev-1, ilat, ilon) : -1.)
                //                 << " |b " << ((ilev < nlev-1) ? hybResult->getPressure(ilev+1, ilat, ilon) : -1.)
                //                 );
            }
            else
            {
                if (levtype == LOG_PRESSURE_LEVELS_3D) p.setZ(log(p.z()));
                if (p.z() < gridNodesLev[0]) continue;
                if (p.z() >= gridNodesLev[nlev]) continue;

                ilev = gsl_interp_accel_find(levAccel, gridNodesLev, nlev+1, p.z());
            }

            // LOG4CPLUS_DEBUG(mlog, i << ": " << p.x() << " " << p.y() << " "
            //                 << p.z() << " || " << ilon
            //                 << " " << ilat << " " << ilev);

            // Increase the probability "counter" ...
            result->addValue(ilev, ilat, ilon, probPerMember);
            // ... and store a flag that member m contributed to the probability
            // at the current grid point.
            result->setFlag(ilev, ilat, ilon, m);
        }

        // Store that member m contributed to the result.
        result->setContributingMember(m);

        inputSelectionSource->releaseData(selection);
        trajectorySource->releaseData(trajectories);
    }

    delete[] gridNodesLon;
    delete[] gridNodesLat;
    delete[] gridNodesLev;

#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "gridding completed in "
                    << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif
    return result;
}


MTask* MProbDFTrajectoriesSource::createTaskGraph(MDataRequest request)
{
    assert(inputSelectionSource != nullptr);
    assert(trajectorySource != nullptr);

    MTask *task = new MTask(request, this);

    // Get required arguments.
    MDataRequestHelper rh(request);
    QDateTime   validTime      = rh.timeValue("VALID_TIME");
    bool        tryPrecomputed = bool(rh.intValue("TRY_PRECOMPUTED"));
    QStringList member         = rh.value("PWCB_ENSEMBLE_MEMBER").split("/");
    int         memberFrom     = member[0].toInt();
    int         memberTo       = member[1].toInt();

    rh.removeAll(locallyRequiredKeys());

    // Insert arguments for modules further down the pipeline.
    rh.insert("VALID_TIME", validTime);
    rh.insert("FILTER_TIMESTEP", validTime); // We only use the trajectory
                                             // positions at "validTime".

    if (tryPrecomputed)
    {
        // If precomputed delta-pressure values should be used, only the
        // trajectory positions at validTime need to be read.
        /**
        @note -- mr, 15Jan2014 -- It would be nicer to not decide at this point
        whether the entire trajectory data set is required or not. This filter
        always requires the particle positions at valid time only. However, if
        the delta-pressure filter needs to compute the pressure differences, it
        requires the entire trajectory dataset. As the time span of the
        trajectories is encoded in the request and the the filter data is
        connected with the trajectory request, we need to insert the keywords
        here. Is there a better solution?
        */
        rh.insert("TRY_PRECOMPUTED", 1);
        rh.insert("TIME_SPAN", validTime);
    }
    else
    {
        rh.insert("TRY_PRECOMPUTED", 0);
        rh.insert("TIME_SPAN", "ALL");
    }

    for (int m = memberFrom; m <= memberTo; m++)
    {
        rh.insert("MEMBER", m);
        task->addParent(trajectorySource->getTaskGraph(rh.request()));
        task->addParent(inputSelectionSource->getTaskGraph(rh.request()));
    }

    return task;
}


QList<MVerticalLevelType> MProbDFTrajectoriesSource::availableLevelTypes()
{
    return (QList<MVerticalLevelType>() << LOG_PRESSURE_LEVELS_3D
            << PRESSURE_LEVELS_3D << HYBRID_SIGMA_PRESSURE_3D);
}


QStringList MProbDFTrajectoriesSource::availableVariables(
        MVerticalLevelType levelType)
{
    if ((levelType == LOG_PRESSURE_LEVELS_3D)
            || (levelType == HYBRID_SIGMA_PRESSURE_3D)
            || (levelType == PRESSURE_LEVELS_3D))
        return (QStringList() << "ProbabilityOfTrajectoryOccurence");
    else
        return QStringList();
}


QList<unsigned int> MProbDFTrajectoriesSource::availableEnsembleMembers(
        MVerticalLevelType levelType, const QString& variableName)
{
    Q_UNUSED(variableName);
    assert(trajectorySource != nullptr);
    if ((levelType == LOG_PRESSURE_LEVELS_3D)
            || (levelType == HYBRID_SIGMA_PRESSURE_3D)
            || (levelType == PRESSURE_LEVELS_3D))
        return trajectorySource->availableEnsembleMembers();
    else
        return QList<unsigned int>();
}


QList<QDateTime> MProbDFTrajectoriesSource::availableInitTimes(
        MVerticalLevelType levelType, const QString& variableName)
{
    Q_UNUSED(variableName);
    assert(trajectorySource != nullptr);
    if ((levelType == LOG_PRESSURE_LEVELS_3D)
            || (levelType == HYBRID_SIGMA_PRESSURE_3D)
            || (levelType == PRESSURE_LEVELS_3D))
        return trajectorySource->availableInitTimes();
    else
        return QList<QDateTime>();
}


QList<QDateTime> MProbDFTrajectoriesSource::availableValidTimes(
        MVerticalLevelType levelType,
        const QString& variableName, const QDateTime& initTime)
{
    Q_UNUSED(variableName);
    assert(trajectorySource != nullptr);
    if ((levelType == LOG_PRESSURE_LEVELS_3D)
            || (levelType == HYBRID_SIGMA_PRESSURE_3D)
            || (levelType == PRESSURE_LEVELS_3D))
        return trajectorySource->availableValidTimes(initTime);
    else
        return QList<QDateTime>();
}



/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MProbDFTrajectoriesSource::locallyRequiredKeys()
{
    return (QStringList() << "PWCB_ENSEMBLE_MEMBER" << "VALID_TIME"
            << "TRY_PRECOMPUTED" << "LEVELTYPE");
}



} // namespace Met3D
