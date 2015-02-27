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
#include "probabltrajectoriessource.h"

// standard library imports
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

MProbABLTrajectoriesSource::MProbABLTrajectoriesSource()
    : MWeatherPredictionDataSource(),
      trajectorySource(nullptr),
      inputSelectionSource(nullptr),
      gslLUTAirParcelThickness(nullptr),
      gslLUTAirParcelThicknessAcc(nullptr),
      pressureValuesLUT(nullptr),
      airParcelThicknessValuesLUT(nullptr)
{
    initializeDeltaPressureLUT();
}


MProbABLTrajectoriesSource::~MProbABLTrajectoriesSource()
{
    cleanUpDeltaPressureLUT();
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MProbABLTrajectoriesSource::setTrajectorySource(MTrajectoryDataSource* s)
{
    trajectorySource = s;
    registerInputSource(trajectorySource);
}


void MProbABLTrajectoriesSource::setInputSelectionSource(
        MTrajectorySelectionSource *s)
{
    inputSelectionSource = s;
    registerInputSource(inputSelectionSource);
}


MStructuredGrid* MProbABLTrajectoriesSource::produceData(
        MDataRequest request)
{
    assert(inputSelectionSource != nullptr);
    assert(trajectorySource != nullptr);

    MDataRequestHelper rh(request);

    // Parse request.
    MVerticalLevelType levtype = MVerticalLevelType(rh.intValue("LEVELTYPE"));
    QDateTime   initTime       = rh.timeValue("INIT_TIME");
    QDateTime   validTime      = rh.timeValue("VALID_TIME");
    bool        tryPrecomputed = bool(rh.intValue("TRY_PRECOMPUTED"));
    QStringList gridGeometry   = rh.value("GRID_GEOMETRY").split("/");
    QStringList member         = rh.value("PWCB_ENSEMBLE_MEMBER").split("/");
    int memberFrom = member[0].toInt();
    int memberTo   = member[1].toInt();
    rh.removeAll(locallyRequiredKeys());

    // (MTrajectoryReader::validTimeOverlap() is thread-safe.)
    QList<QDateTime> inputValidTimes =
            trajectorySource->validTimeOverlap(initTime, validTime);

    rh.insert("INIT_TIME", initTime);
    // We only use the trajectory positions at "validTime".
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

    // Grid geometry.
    float westernlon  = gridGeometry[1].toFloat();
    float dlon        = gridGeometry[2].toFloat();
    int   nlon        = gridGeometry[3].toInt();

    float northernlat = gridGeometry[4].toFloat();
    float dlat        = gridGeometry[5].toFloat();
    int   nlat        = gridGeometry[6].toInt();

    float bottomlev, toplev;
    if (levtype == LOG_PRESSURE_LEVELS_3D)
    {
        bottomlev     = log(gridGeometry[7].toFloat());
        toplev        = log(gridGeometry[8].toFloat());
    }
    else
    {
        bottomlev     = gridGeometry[7].toFloat();
        toplev        = gridGeometry[8].toFloat();
    }
    int   nlev        = gridGeometry[9].toInt();
    float dlev        = (bottomlev - toplev) / (nlev - 1);

    bool scaleAirParcelThickness = bool(gridGeometry[10].toInt());

    // Grids for the probabilities (total and per member).
    MStructuredGrid *result = nullptr;
    MStructuredGrid *memberGrid = nullptr;


    float probPerMember = 1. / (memberTo-memberFrom+1);

    // Loop over all ensemble members and accumulate probability information.
    for (int m = memberFrom; m <= memberTo; m++)
    {
        rh.insert("MEMBER", m);

        // Initialize grids for the probabilities (total and per member) when
        // the first trajectory dataset is available.
        if (result == nullptr)
        {
            switch (levtype)
            {
            case PRESSURE_LEVELS_3D:
                result = new MRegularLonLatStructuredPressureGrid(nlev, nlat, nlon);
                memberGrid = new MRegularLonLatStructuredPressureGrid(nlev, nlat, nlon);
                for (int i = 0; i < nlon; i++) result->setLon(i, westernlon + i*dlon);
                for (int i = 0; i < nlat; i++) result->setLat(i, northernlat - i*dlat);
                for (int i = 0; i < nlev; i++) result->setLevel(i, toplev + i*dlev);
                break;

            case LOG_PRESSURE_LEVELS_3D:
                result = new MRegularLonLatLnPGrid(nlev, nlat, nlon);
                memberGrid = new MRegularLonLatLnPGrid(nlev, nlat, nlon);
                for (int i = 0; i < nlon; i++) result->setLon(i, westernlon + i*dlon);
                for (int i = 0; i < nlat; i++) result->setLat(i, northernlat - i*dlat);
                for (int i = 0; i < nlev; i++) result->setLevel(i, toplev + i*dlev);
                break;

            case HYBRID_SIGMA_PRESSURE_3D:
            {
                rh.insert("VALID_TIME", validTime);

                MTrajectorySelection *selection =
                        inputSelectionSource->getData(rh.request());
                MTrajectories *trajectories =
                        trajectorySource->getData(rh.request());

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

                for (int i = 0; i < nlon; i++)
                    result->setLon(i, startGrid->getLons()[i*sx]);
                for (int i = 0; i < nlat; i++)
                    result->setLat(i, startGrid->getLats()[i*sy]);

                if ( !inputValidTimes.contains(validTime) )
                {
                    // Only release data fields if they are not contained in
                    // inputValidTimes. Otherwise they may be deleted from the
                    // memory manager before they can be retrieved below.
                    // If they are contained, they will be released below.
                    inputSelectionSource->releaseData(selection);
                    trajectorySource->releaseData(trajectories);
                }

                memberGrid = new MLonLatHybridSigmaPressureGrid(nlev, nlat, nlon);

                break;
            }

            default:
                break;

            } // switch (levtype)

            result->setToZero();
            result->enableFlags(); // allocate flags bitfield

            for (int i = 0; i < trajectorySource->availableEnsembleMembers().size(); i++)
                result->setAvailableMember(trajectorySource->availableEnsembleMembers()[i]);
        } // initialize grids

        memberGrid->setToZero();

        for (int v = 0; v < inputValidTimes.size(); v++)
        {
            rh.insert("VALID_TIME", inputValidTimes[v]);

            MTrajectorySelection *selection =
                    inputSelectionSource->getData(rh.request());
            MTrajectories *trajectories =
                    trajectorySource->getData(rh.request());

            int numPositions = selection->getNumTrajectories();

            if (!scaleAirParcelThickness)
            {
                // Do NOT scale air parcel thickness!
                for (int i = 0; i < numPositions; i++)
                {
                    QVector3D p = trajectories->getVertices().at(
                                selection->getStartIndices()[i]);

                    // lround() ensures that the grid points are in the centre
                    // of the grid boxes -- i.e. grid index ilon collects
                    // particle from the area (westernlon + ilon*dlon)-0.5*dlon
                    // .. ()+0.5*dlon.
                    if (p.x() < westernlon) continue;
                    int ilon = lround((p.x() - westernlon) / dlon);
                    if (ilon >= nlon) continue;

                    if (p.y() > northernlat) continue;
                    int ilat = lround((northernlat - p.y()) / dlat);
                    if (ilat >= nlat) continue;


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
                        // particle position given in pressure coords?
                        if (levtype == LOG_PRESSURE_LEVELS_3D) p.setZ(log(p.z()));

                        if (p.z() < toplev) continue;
                        ilev = lround((p.z() - toplev) / dlev);
                        if (ilev >= nlev) continue;
                    }

//                LOG4CPLUS_DEBUG(mlog, i << ": " << p.x() << " " << p.y() << " "
//                                << p.z() << " " << exp(p.z()) << " || " << ilon
//                                << " " << ilat << " " << ilev);

                    memberGrid->setValue(ilev, ilat, ilon, 1.);
                }
            }
            else
            {
                // SCALE air parcel thickness!
                for (int i = 0; i < numPositions; i++)
                {
                    QVector3D p = trajectories->getVertices().at(
                                selection->getStartIndices()[i]);

                    if (p.x() < westernlon) continue;
                    int ilon = lround((p.x() - westernlon) / dlon);
                    if (ilon >= nlon) continue;

                    if (p.y() > northernlat) continue;
                    int ilat = lround((northernlat - p.y()) / dlat);
                    if (ilat >= nlat) continue;

                    if (levtype == LOG_PRESSURE_LEVELS_3D)
                    {
                        // LOG(PRESSURE) levels.
                        float ln_pz = log(p.z());
                        if (ln_pz < toplev) continue;
                        int ilev = lround((ln_pz - toplev) / dlev);
                        if (ilev >= nlev) continue;

                        float parcelThickness = airParcelThickness(p.z()) / 2.;
                        for (int offset = 0; ilev+offset < nlev; offset++)
                        {
                            float plev = exp(result->getLevels()[ilev+offset]);
                            if (fabs(p.z()-plev) < parcelThickness)
                                memberGrid->setValue(ilev+offset, ilat, ilon, 1.);
                            else
                                break;
                        }
                        for (int offset = 0; ilev-offset >= 0; offset++)
                        {
                            float plev = exp(result->getLevels()[ilev-offset]);
                            if (fabs(p.z()-plev) < parcelThickness)
                                memberGrid->setValue(ilev-offset, ilat, ilon, 1.);
                            else
                                break;
                        }
                    }
                    else
                    {
                        // PRESSURE levels.
                        if (p.z() < toplev) continue;
                        int ilev = lround((p.z() - toplev) / dlev);
                        if (ilev >= nlev) continue;

                        float parcelThickness = airParcelThickness(p.z()) / 2.;
                        for (int offset = 0; ilev+offset < nlev; offset++)
                        {
                            float plev = result->getLevels()[ilev+offset];
                            if (fabs(p.z()-plev) < parcelThickness)
                            {
                                memberGrid->setValue(ilev+offset, ilat, ilon, 1.);

//                                // DEBUG
//                                if (ilev+offset == nlev-1)
//                                {
//                                    LOG4CPLUS_DEBUG_FMT(
//                                                mlog, "Contribution to grid point "
//                                                "(%i/%i/%i) at (%.2f, %.2f, %.2f) "
//                                                "from member %i, start time %s, "
//                                                "at position (%.2f, %.2f, %.2f)",
//                                                ilon, ilat, ilev+offset,
//                                                result->getLons()[ilon],
//                                                result->getLats()[ilat],
//                                                result->getLevels()[ilev+offset],
//                                                m, inputValidTimes[v].toString(
//                                                    Qt::ISODate).toStdString().c_str(),
//                                                p.x(), p.y(), p.z());
//                                }
                            }
                            else
                                break;
                        }
                        for (int offset = 1; ilev-offset >= 0; offset++)
                        {
                            float plev = result->getLevels()[ilev-offset];
                            if (fabs(p.z()-plev) < parcelThickness)
                                memberGrid->setValue(ilev-offset, ilat, ilon, 1.);
                            else
                                break;
                        }
                    } // ln(p) or p levels?
                }
            } // scale vertical thickness

            inputSelectionSource->releaseData(selection);
            trajectorySource->releaseData(trajectories);
        }

        for (unsigned int v = 0; v < result->getNumValues(); v++)
            result->addValue(v, memberGrid->getValue(v) * probPerMember);
    }

    delete memberGrid;
    return result;
}


MTask* MProbABLTrajectoriesSource::createTaskGraph(MDataRequest request)
{
    assert(inputSelectionSource != nullptr);
    assert(trajectorySource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    QDateTime   initTime  = rh.timeValue("INIT_TIME");
    QDateTime   validTime = rh.timeValue("VALID_TIME");
    bool tryPrecomputed   = bool(rh.intValue("TRY_PRECOMPUTED"));
    QStringList member    = rh.value("PWCB_ENSEMBLE_MEMBER").split("/");
    int memberFrom = member[0].toInt();
    int memberTo = member[1].toInt();
    rh.removeAll(locallyRequiredKeys());

    QList<QDateTime> inputValidTimes =
            trajectorySource->validTimeOverlap(initTime, validTime);

    rh.insert("INIT_TIME", initTime);
    // We only use the trajectory positions at "validTime".
    rh.insert("FILTER_TIMESTEP", validTime);

    if (tryPrecomputed)
    {
        // If precomputed delta-pressure values should be used, only the
        // trajectory positions at validTime need to be read.
        /**
        @note -- mr, 29Jan2014 -- See
        MProbDFTrajectoriesSource::createTaskGraph()
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
        for (int v = 0; v < inputValidTimes.size(); v++)
        {
            rh.insert("VALID_TIME", inputValidTimes[v]);
            task->addParent(trajectorySource->getTaskGraph(rh.request()));
            task->addParent(inputSelectionSource->getTaskGraph(rh.request()));
        }
    }

    return task;
}


QList<MVerticalLevelType> MProbABLTrajectoriesSource::availableLevelTypes()
{
    return (QList<MVerticalLevelType>() << LOG_PRESSURE_LEVELS_3D
            << PRESSURE_LEVELS_3D << HYBRID_SIGMA_PRESSURE_3D);
}


QStringList MProbABLTrajectoriesSource::availableVariables(
        MVerticalLevelType levelType)
{
    if ((levelType == LOG_PRESSURE_LEVELS_3D)
            || (levelType == HYBRID_SIGMA_PRESSURE_3D)
            || (levelType == PRESSURE_LEVELS_3D))
        return (QStringList() << "ProbabilityOfTrajectoryOccurence");
    else
        return QStringList();
}


QList<unsigned int> MProbABLTrajectoriesSource::availableEnsembleMembers(
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


QList<QDateTime> MProbABLTrajectoriesSource::availableInitTimes(
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


QList<QDateTime> MProbABLTrajectoriesSource::availableValidTimes(
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

const QStringList MProbABLTrajectoriesSource::locallyRequiredKeys()
{
    return (QStringList() << "PWCB_ENSEMBLE_MEMBER" << "INIT_TIME"
            << "VALID_TIME" << "TRY_PRECOMPUTED" << "GRID_GEOMETRY"
            << "LEVELTYPE");
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MProbABLTrajectoriesSource::initializeDeltaPressureLUT()
{
    // Define LUTs precomputed with "lut_delta_p.py".

    int numLUTValues = 951;
    pressureValuesLUT = new double[951] {
            100.0, 101.0, 102.0, 103.0, 104.0, 105.0,
            106.0, 107.0, 108.0, 109.0, 110.0, 111.0,
            112.0, 113.0, 114.0, 115.0, 116.0, 117.0,
            118.0, 119.0, 120.0, 121.0, 122.0, 123.0,
            124.0, 125.0, 126.0, 127.0, 128.0, 129.0,
            130.0, 131.0, 132.0, 133.0, 134.0, 135.0,
            136.0, 137.0, 138.0, 139.0, 140.0, 141.0,
            142.0, 143.0, 144.0, 145.0, 146.0, 147.0,
            148.0, 149.0, 150.0, 151.0, 152.0, 153.0,
            154.0, 155.0, 156.0, 157.0, 158.0, 159.0,
            160.0, 161.0, 162.0, 163.0, 164.0, 165.0,
            166.0, 167.0, 168.0, 169.0, 170.0, 171.0,
            172.0, 173.0, 174.0, 175.0, 176.0, 177.0,
            178.0, 179.0, 180.0, 181.0, 182.0, 183.0,
            184.0, 185.0, 186.0, 187.0, 188.0, 189.0,
            190.0, 191.0, 192.0, 193.0, 194.0, 195.0,
            196.0, 197.0, 198.0, 199.0, 200.0, 201.0,
            202.0, 203.0, 204.0, 205.0, 206.0, 207.0,
            208.0, 209.0, 210.0, 211.0, 212.0, 213.0,
            214.0, 215.0, 216.0, 217.0, 218.0, 219.0,
            220.0, 221.0, 222.0, 223.0, 224.0, 225.0,
            226.0, 227.0, 228.0, 229.0, 230.0, 231.0,
            232.0, 233.0, 234.0, 235.0, 236.0, 237.0,
            238.0, 239.0, 240.0, 241.0, 242.0, 243.0,
            244.0, 245.0, 246.0, 247.0, 248.0, 249.0,
            250.0, 251.0, 252.0, 253.0, 254.0, 255.0,
            256.0, 257.0, 258.0, 259.0, 260.0, 261.0,
            262.0, 263.0, 264.0, 265.0, 266.0, 267.0,
            268.0, 269.0, 270.0, 271.0, 272.0, 273.0,
            274.0, 275.0, 276.0, 277.0, 278.0, 279.0,
            280.0, 281.0, 282.0, 283.0, 284.0, 285.0,
            286.0, 287.0, 288.0, 289.0, 290.0, 291.0,
            292.0, 293.0, 294.0, 295.0, 296.0, 297.0,
            298.0, 299.0, 300.0, 301.0, 302.0, 303.0,
            304.0, 305.0, 306.0, 307.0, 308.0, 309.0,
            310.0, 311.0, 312.0, 313.0, 314.0, 315.0,
            316.0, 317.0, 318.0, 319.0, 320.0, 321.0,
            322.0, 323.0, 324.0, 325.0, 326.0, 327.0,
            328.0, 329.0, 330.0, 331.0, 332.0, 333.0,
            334.0, 335.0, 336.0, 337.0, 338.0, 339.0,
            340.0, 341.0, 342.0, 343.0, 344.0, 345.0,
            346.0, 347.0, 348.0, 349.0, 350.0, 351.0,
            352.0, 353.0, 354.0, 355.0, 356.0, 357.0,
            358.0, 359.0, 360.0, 361.0, 362.0, 363.0,
            364.0, 365.0, 366.0, 367.0, 368.0, 369.0,
            370.0, 371.0, 372.0, 373.0, 374.0, 375.0,
            376.0, 377.0, 378.0, 379.0, 380.0, 381.0,
            382.0, 383.0, 384.0, 385.0, 386.0, 387.0,
            388.0, 389.0, 390.0, 391.0, 392.0, 393.0,
            394.0, 395.0, 396.0, 397.0, 398.0, 399.0,
            400.0, 401.0, 402.0, 403.0, 404.0, 405.0,
            406.0, 407.0, 408.0, 409.0, 410.0, 411.0,
            412.0, 413.0, 414.0, 415.0, 416.0, 417.0,
            418.0, 419.0, 420.0, 421.0, 422.0, 423.0,
            424.0, 425.0, 426.0, 427.0, 428.0, 429.0,
            430.0, 431.0, 432.0, 433.0, 434.0, 435.0,
            436.0, 437.0, 438.0, 439.0, 440.0, 441.0,
            442.0, 443.0, 444.0, 445.0, 446.0, 447.0,
            448.0, 449.0, 450.0, 451.0, 452.0, 453.0,
            454.0, 455.0, 456.0, 457.0, 458.0, 459.0,
            460.0, 461.0, 462.0, 463.0, 464.0, 465.0,
            466.0, 467.0, 468.0, 469.0, 470.0, 471.0,
            472.0, 473.0, 474.0, 475.0, 476.0, 477.0,
            478.0, 479.0, 480.0, 481.0, 482.0, 483.0,
            484.0, 485.0, 486.0, 487.0, 488.0, 489.0,
            490.0, 491.0, 492.0, 493.0, 494.0, 495.0,
            496.0, 497.0, 498.0, 499.0, 500.0, 501.0,
            502.0, 503.0, 504.0, 505.0, 506.0, 507.0,
            508.0, 509.0, 510.0, 511.0, 512.0, 513.0,
            514.0, 515.0, 516.0, 517.0, 518.0, 519.0,
            520.0, 521.0, 522.0, 523.0, 524.0, 525.0,
            526.0, 527.0, 528.0, 529.0, 530.0, 531.0,
            532.0, 533.0, 534.0, 535.0, 536.0, 537.0,
            538.0, 539.0, 540.0, 541.0, 542.0, 543.0,
            544.0, 545.0, 546.0, 547.0, 548.0, 549.0,
            550.0, 551.0, 552.0, 553.0, 554.0, 555.0,
            556.0, 557.0, 558.0, 559.0, 560.0, 561.0,
            562.0, 563.0, 564.0, 565.0, 566.0, 567.0,
            568.0, 569.0, 570.0, 571.0, 572.0, 573.0,
            574.0, 575.0, 576.0, 577.0, 578.0, 579.0,
            580.0, 581.0, 582.0, 583.0, 584.0, 585.0,
            586.0, 587.0, 588.0, 589.0, 590.0, 591.0,
            592.0, 593.0, 594.0, 595.0, 596.0, 597.0,
            598.0, 599.0, 600.0, 601.0, 602.0, 603.0,
            604.0, 605.0, 606.0, 607.0, 608.0, 609.0,
            610.0, 611.0, 612.0, 613.0, 614.0, 615.0,
            616.0, 617.0, 618.0, 619.0, 620.0, 621.0,
            622.0, 623.0, 624.0, 625.0, 626.0, 627.0,
            628.0, 629.0, 630.0, 631.0, 632.0, 633.0,
            634.0, 635.0, 636.0, 637.0, 638.0, 639.0,
            640.0, 641.0, 642.0, 643.0, 644.0, 645.0,
            646.0, 647.0, 648.0, 649.0, 650.0, 651.0,
            652.0, 653.0, 654.0, 655.0, 656.0, 657.0,
            658.0, 659.0, 660.0, 661.0, 662.0, 663.0,
            664.0, 665.0, 666.0, 667.0, 668.0, 669.0,
            670.0, 671.0, 672.0, 673.0, 674.0, 675.0,
            676.0, 677.0, 678.0, 679.0, 680.0, 681.0,
            682.0, 683.0, 684.0, 685.0, 686.0, 687.0,
            688.0, 689.0, 690.0, 691.0, 692.0, 693.0,
            694.0, 695.0, 696.0, 697.0, 698.0, 699.0,
            700.0, 701.0, 702.0, 703.0, 704.0, 705.0,
            706.0, 707.0, 708.0, 709.0, 710.0, 711.0,
            712.0, 713.0, 714.0, 715.0, 716.0, 717.0,
            718.0, 719.0, 720.0, 721.0, 722.0, 723.0,
            724.0, 725.0, 726.0, 727.0, 728.0, 729.0,
            730.0, 731.0, 732.0, 733.0, 734.0, 735.0,
            736.0, 737.0, 738.0, 739.0, 740.0, 741.0,
            742.0, 743.0, 744.0, 745.0, 746.0, 747.0,
            748.0, 749.0, 750.0, 751.0, 752.0, 753.0,
            754.0, 755.0, 756.0, 757.0, 758.0, 759.0,
            760.0, 761.0, 762.0, 763.0, 764.0, 765.0,
            766.0, 767.0, 768.0, 769.0, 770.0, 771.0,
            772.0, 773.0, 774.0, 775.0, 776.0, 777.0,
            778.0, 779.0, 780.0, 781.0, 782.0, 783.0,
            784.0, 785.0, 786.0, 787.0, 788.0, 789.0,
            790.0, 791.0, 792.0, 793.0, 794.0, 795.0,
            796.0, 797.0, 798.0, 799.0, 800.0, 801.0,
            802.0, 803.0, 804.0, 805.0, 806.0, 807.0,
            808.0, 809.0, 810.0, 811.0, 812.0, 813.0,
            814.0, 815.0, 816.0, 817.0, 818.0, 819.0,
            820.0, 821.0, 822.0, 823.0, 824.0, 825.0,
            826.0, 827.0, 828.0, 829.0, 830.0, 831.0,
            832.0, 833.0, 834.0, 835.0, 836.0, 837.0,
            838.0, 839.0, 840.0, 841.0, 842.0, 843.0,
            844.0, 845.0, 846.0, 847.0, 848.0, 849.0,
            850.0, 851.0, 852.0, 853.0, 854.0, 855.0,
            856.0, 857.0, 858.0, 859.0, 860.0, 861.0,
            862.0, 863.0, 864.0, 865.0, 866.0, 867.0,
            868.0, 869.0, 870.0, 871.0, 872.0, 873.0,
            874.0, 875.0, 876.0, 877.0, 878.0, 879.0,
            880.0, 881.0, 882.0, 883.0, 884.0, 885.0,
            886.0, 887.0, 888.0, 889.0, 890.0, 891.0,
            892.0, 893.0, 894.0, 895.0, 896.0, 897.0,
            898.0, 899.0, 900.0, 901.0, 902.0, 903.0,
            904.0, 905.0, 906.0, 907.0, 908.0, 909.0,
            910.0, 911.0, 912.0, 913.0, 914.0, 915.0,
            916.0, 917.0, 918.0, 919.0, 920.0, 921.0,
            922.0, 923.0, 924.0, 925.0, 926.0, 927.0,
            928.0, 929.0, 930.0, 931.0, 932.0, 933.0,
            934.0, 935.0, 936.0, 937.0, 938.0, 939.0,
            940.0, 941.0, 942.0, 943.0, 944.0, 945.0,
            946.0, 947.0, 948.0, 949.0, 950.0, 951.0,
            952.0, 953.0, 954.0, 955.0, 956.0, 957.0,
            958.0, 959.0, 960.0, 961.0, 962.0, 963.0,
            964.0, 965.0, 966.0, 967.0, 968.0, 969.0,
            970.0, 971.0, 972.0, 973.0, 974.0, 975.0,
            976.0, 977.0, 978.0, 979.0, 980.0, 981.0,
            982.0, 983.0, 984.0, 985.0, 986.0, 987.0,
            988.0, 989.0, 990.0, 991.0, 992.0, 993.0,
            994.0, 995.0, 996.0, 997.0, 998.0, 999.0,
            1000.0, 1001.0, 1002.0, 1003.0, 1004.0, 1005.0,
            1006.0, 1007.0, 1008.0, 1009.0, 1010.0, 1011.0,
            1012.0, 1013.0, 1014.0, 1015.0, 1016.0, 1017.0,
            1018.0, 1019.0, 1020.0, 1021.0, 1022.0, 1023.0,
            1024.0, 1025.0, 1026.0, 1027.0, 1028.0, 1029.0,
            1030.0, 1031.0, 1032.0, 1033.0, 1034.0, 1035.0,
            1036.0, 1037.0, 1038.0, 1039.0, 1040.0, 1041.0,
            1042.0, 1043.0, 1044.0, 1045.0, 1046.0, 1047.0,
            1048.0, 1049.0, 1050.0 };

    // 25 hPa @ 850 hPa.
    // =================
//    airParcelThicknessValuesLUT = new double[951] {
//            7.099418, 7.146669, 7.193764, 7.240706, 7.287496, 7.334136,
//    7.380628, 7.426975, 7.473177, 7.519237, 7.565156, 7.610936,
//    7.656579, 7.702086, 7.747460, 7.792700, 7.837810, 7.882791,
//    7.927643, 7.972369, 8.016970, 8.061447, 8.105802, 8.150035,
//    8.194149, 8.238145, 8.282023, 8.325786, 8.369433, 8.412968,
//    8.456390, 8.499700, 8.542901, 8.585993, 8.628976, 8.671853,
//    8.714625, 8.757291, 8.799854, 8.842314, 8.884673, 8.926931,
//    8.969089, 9.011148, 9.053109, 9.094973, 9.136741, 9.178414,
//    9.219992, 9.261477, 9.302869, 9.344170, 9.385379, 9.426498,
//    9.467527, 9.508468, 9.549321, 9.590086, 9.630765, 9.671359,
//    9.711867, 9.752291, 9.792632, 9.832889, 9.873065, 9.913158,
//    9.953171, 9.993104, 10.032957, 10.072731, 10.112426, 10.152044,
//    10.191585, 10.231049, 10.270437, 10.309750, 10.348987, 10.388151,
//    10.427241, 10.466258, 10.505202, 10.544074, 10.582875, 10.621605,
//    10.660264, 10.698853, 10.737373, 10.775824, 10.814206, 10.852521,
//    10.890767, 10.928947, 10.967060, 11.005107, 11.043089, 11.081005,
//    11.118857, 11.156644, 11.194367, 11.232027, 11.269624, 11.307158,
//    11.344630, 11.382040, 11.419389, 11.456677, 11.493904, 11.531071,
//    11.568178, 11.605226, 11.642215, 11.679145, 11.716017, 11.752831,
//    11.789587, 11.826286, 11.862929, 11.899514, 11.936044, 11.972518,
//    12.008936, 12.045299, 12.081608, 12.117862, 12.154061, 12.190207,
//    12.226300, 12.257718, 12.286838, 12.315900, 12.344903, 12.373848,
//    12.402736, 12.431566, 12.460340, 12.489057, 12.517717, 12.546322,
//    12.574872, 12.603366, 12.631806, 12.660191, 12.688522, 12.716799,
//    12.745022, 12.773193, 12.801310, 12.829375, 12.857388, 12.885349,
//    12.913258, 12.941116, 12.968922, 12.996679, 13.024384, 13.052040,
//    13.079645, 13.107201, 13.134708, 13.162166, 13.189575, 13.216935,
//    13.244248, 13.271512, 13.298729, 13.325898, 13.353020, 13.380096,
//    13.407124, 13.434107, 13.461043, 13.487933, 13.514778, 13.541577,
//    13.568332, 13.595041, 13.621705, 13.648326, 13.674902, 13.701434,
//    13.727922, 13.754367, 13.780769, 13.807127, 13.833443, 13.859716,
//    13.885947, 13.912135, 13.938282, 13.964387, 13.990450, 14.016472,
//    14.042453, 14.068393, 14.094292, 14.120151, 14.145969, 14.171748,
//    14.197486, 14.223185, 14.248844, 14.274464, 14.300044, 14.325586,
//    14.351089, 14.376554, 14.401980, 14.427367, 14.452717, 14.478029,
//    14.503303, 14.528540, 14.553740, 14.578902, 14.604027, 14.629116,
//    14.654168, 14.679183, 14.704163, 14.729106, 14.754013, 14.778884,
//    14.803720, 14.828520, 14.853285, 14.878015, 14.902710, 14.927370,
//    14.951995, 14.976586, 15.001142, 15.025664, 15.050152, 15.074606,
//    15.099027, 15.123414, 15.147767, 15.172087, 15.196374, 15.220627,
//    15.244848, 15.269036, 15.293192, 15.317315, 15.341405, 15.365464,
//    15.389490, 15.413485, 15.437447, 15.461378, 15.485278, 15.509146,
//    15.532982, 15.556788, 15.580563, 15.604306, 15.628019, 15.651702,
//    15.675354, 15.698975, 15.722566, 15.746127, 15.769658, 15.793159,
//    15.816631, 15.840072, 15.863485, 15.886867, 15.910221, 15.933545,
//    15.956840, 15.980106, 16.003344, 16.026553, 16.049733, 16.072884,
//    16.096007, 16.119102, 16.142169, 16.165208, 16.188218, 16.211201,
//    16.234156, 16.257084, 16.279984, 16.302856, 16.325701, 16.348519,
//    16.371310, 16.394074, 16.416811, 16.439521, 16.462204, 16.484861,
//    16.507491, 16.530095, 16.552673, 16.575224, 16.597749, 16.620248,
//    16.642721, 16.665169, 16.687591, 16.709986, 16.732357, 16.754702,
//    16.777021, 16.799316, 16.821585, 16.843829, 16.866048, 16.888242,
//    16.910411, 16.932556, 16.954675, 16.976771, 16.998841, 17.020888,
//    17.042910, 17.064908, 17.086881, 17.108831, 17.130757, 17.152658,
//    17.174536, 17.196390, 17.218221, 17.240028, 17.261811, 17.283571,
//    17.305308, 17.327022, 17.348712, 17.370379, 17.392023, 17.413645,
//    17.435243, 17.456819, 17.478372, 17.499902, 17.521410, 17.542895,
//    17.564358, 17.585798, 17.607216, 17.628612, 17.649986, 17.671338,
//    17.692668, 17.713976, 17.735262, 17.756527, 17.777770, 17.798991,
//    17.820190, 17.841369, 17.862525, 17.883661, 17.904775, 17.925868,
//    17.946939, 17.967990, 17.989020, 18.010028, 18.031016, 18.051983,
//    18.072930, 18.093855, 18.114760, 18.135645, 18.156509, 18.177353,
//    18.198176, 18.218979, 18.239762, 18.260524, 18.281267, 18.301989,
//    18.322691, 18.343374, 18.364037, 18.384679, 18.405303, 18.425906,
//    18.446490, 18.467054, 18.487599, 18.508124, 18.528630, 18.549117,
//    18.569584, 18.590032, 18.610461, 18.630871, 18.651262, 18.671634,
//    18.691986, 18.712320, 18.732636, 18.752932, 18.773210, 18.793469,
//    18.813709, 18.833931, 18.854135, 18.874320, 18.894486, 18.914635,
//    18.934765, 18.954876, 18.974970, 18.995046, 19.015103, 19.035142,
//    19.055164, 19.075167, 19.095153, 19.115121, 19.135071, 19.155003,
//    19.174918, 19.194815, 19.214694, 19.234556, 19.254401, 19.274228,
//    19.294038, 19.313830, 19.333605, 19.353363, 19.373104, 19.392827,
//    19.412534, 19.432223, 19.451896, 19.471551, 19.491190, 19.510812,
//    19.530417, 19.550005, 19.569576, 19.589131, 19.608669, 19.628191,
//    19.647696, 19.667185, 19.686657, 19.706112, 19.725552, 19.744975,
//    19.764382, 19.783772, 19.803146, 19.822505, 19.841847, 19.861173,
//    19.880483, 19.899777, 19.919055, 19.938317, 19.957564, 19.976794,
//    19.996009, 20.015208, 20.034391, 20.053559, 20.072711, 20.091848,
//    20.110969, 20.130074, 20.149165, 20.168239, 20.187299, 20.206343,
//    20.225371, 20.244385, 20.263383, 20.282366, 20.301334, 20.320287,
//    20.339225, 20.358148, 20.377055, 20.395948, 20.414826, 20.433689,
//    20.452538, 20.471371, 20.490190, 20.508994, 20.527783, 20.546558,
//    20.565318, 20.584063, 20.602794, 20.621510, 20.640212, 20.658900,
//    20.677573, 20.696232, 20.714876, 20.733506, 20.752122, 20.770724,
//    20.789311, 20.807884, 20.826444, 20.844989, 20.863520, 20.882037,
//    20.900540, 20.919029, 20.937504, 20.955965, 20.974413, 20.992846,
//    21.011266, 21.029672, 21.048064, 21.066443, 21.084808, 21.103159,
//    21.121497, 21.139821, 21.158132, 21.176429, 21.194713, 21.212983,
//    21.231240, 21.249483, 21.267713, 21.285930, 21.304134, 21.322324,
//    21.340501, 21.358665, 21.376816, 21.394953, 21.413078, 21.431189,
//    21.449288, 21.467373, 21.485445, 21.503505, 21.521551, 21.539585,
//    21.557606, 21.575614, 21.593609, 21.611591, 21.629561, 21.647518,
//    21.665462, 21.683393, 21.701312, 21.719219, 21.737112, 21.754994,
//    21.772862, 21.790719, 21.808562, 21.826394, 21.844212, 21.862019,
//    21.879813, 21.897595, 21.915364, 21.933122, 21.950867, 21.968599,
//    21.986320, 22.004028, 22.021725, 22.039409, 22.057081, 22.074741,
//    22.092389, 22.110025, 22.127649, 22.145261, 22.162861, 22.180449,
//    22.198025, 22.215590, 22.233142, 22.250683, 22.268212, 22.285729,
//    22.303235, 22.320729, 22.338211, 22.355682, 22.373140, 22.390588,
//    22.408023, 22.425447, 22.442860, 22.460261, 22.477651, 22.495029,
//    22.512396, 22.529751, 22.547095, 22.564427, 22.581749, 22.599058,
//    22.616357, 22.633644, 22.650920, 22.668185, 22.685439, 22.702681,
//    22.719913, 22.737133, 22.754342, 22.771540, 22.788727, 22.805902,
//    22.823067, 22.840221, 22.857364, 22.874496, 22.891617, 22.908727,
//    22.925826, 22.942914, 22.959992, 22.977059, 22.994114, 23.011160,
//    23.028194, 23.045217, 23.062230, 23.079232, 23.096224, 23.113205,
//    23.130175, 23.147135, 23.164084, 23.181022, 23.197950, 23.214868,
//    23.231774, 23.248671, 23.265557, 23.282432, 23.299297, 23.316152,
//    23.332996, 23.349830, 23.366654, 23.383467, 23.400270, 23.417063,
//    23.433846, 23.450618, 23.467380, 23.484132, 23.500873, 23.517605,
//    23.534326, 23.551037, 23.567738, 23.584429, 23.601110, 23.617781,
//    23.634442, 23.651093, 23.667734, 23.684365, 23.700986, 23.717597,
//    23.734198, 23.750789, 23.767370, 23.783942, 23.800504, 23.817055,
//    23.833597, 23.850130, 23.866652, 23.883165, 23.899668, 23.916161,
//    23.932645, 23.949119, 23.965583, 23.982038, 23.998483, 24.014918,
//    24.031344, 24.047761, 24.064167, 24.080565, 24.096952, 24.113331,
//    24.129699, 24.146059, 24.162408, 24.178749, 24.195080, 24.211401,
//    24.227714, 24.244017, 24.260310, 24.276594, 24.292869, 24.309135,
//    24.325391, 24.341638, 24.357876, 24.374105, 24.390324, 24.406534,
//    24.422736, 24.438927, 24.455110, 24.471284, 24.487448, 24.503604,
//    24.519750, 24.535888, 24.552016, 24.568135, 24.584245, 24.600347,
//    24.616439, 24.632522, 24.648596, 24.664662, 24.680718, 24.696766,
//    24.712805, 24.728835, 24.744856, 24.760868, 24.776871, 24.792866,
//    24.808851, 24.824828, 24.840796, 24.856756, 24.872707, 24.888649,
//    24.904582, 24.920507, 24.936423, 24.952330, 24.968229, 24.984119,
//    25.000000, 25.015873, 25.031737, 25.047593, 25.063440, 25.079278,
//    25.095109, 25.110930, 25.126743, 25.142548, 25.158344, 25.174132,
//    25.189911, 25.205682, 25.221444, 25.237198, 25.252944, 25.268681,
//    25.284410, 25.300131, 25.315843, 25.331547, 25.347242, 25.362930,
//    25.378609, 25.394280, 25.409942, 25.425597, 25.441243, 25.456881,
//    25.472511, 25.488132, 25.503746, 25.519351, 25.534948, 25.550538,
//    25.566119, 25.581691, 25.597256, 25.612813, 25.628362, 25.643902,
//    25.659435, 25.674960, 25.690476, 25.705985, 25.721486, 25.736978,
//    25.752463, 25.767940, 25.783409, 25.798870, 25.814323, 25.829768,
//    25.845205, 25.860635, 25.876057, 25.891470, 25.906876, 25.922275,
//    25.937665, 25.953048, 25.968423, 25.983790, 25.999149, 26.014501,
//    26.029845, 26.045181, 26.060509, 26.075830, 26.091143, 26.106449,
//    26.121747, 26.137037, 26.152319, 26.167594, 26.182862, 26.198122,
//    26.213374, 26.228619, 26.243856, 26.259085, 26.274307, 26.289522,
//    26.304729, 26.319928, 26.335121, 26.350305, 26.365482, 26.380652,
//    26.395814, 26.410969, 26.426117, 26.441257, 26.456389, 26.471515,
//    26.486633, 26.501743, 26.516846, 26.531942, 26.547031, 26.562112,
//    26.577186, 26.592253, 26.607312, 26.622364, 26.637409, 26.652447,
//    26.667477, 26.682501, 26.697517, 26.712525, 26.727527, 26.742521,
//    26.757509, 26.772489, 26.787462, 26.802428, 26.817386, 26.832338,
//    26.847283, 26.862220, 26.877150, 26.892074, 26.906990, 26.921899,
//    26.936801, 26.951697, 26.966585, 26.981466, 26.996340, 27.011207,
//    27.026067, 27.040921, 27.055767, 27.070606, 27.085438, 27.100264,
//    27.115083, 27.129894, 27.144699, 27.159497, 27.174288, 27.189072,
//    27.203849, 27.218620, 27.233383, 27.248140, 27.262890, 27.277633,
//    27.292370, 27.307100, 27.321822, 27.336539, 27.351248, 27.365951,
//    27.380646, 27.395336, 27.410018, 27.424694, 27.439363, 27.454025,
//    27.468681, 27.483330, 27.497972, 27.512608, 27.527237, 27.541860,
//    27.556476, 27.571085, 27.585688, 27.600284, 27.614873, 27.629456,
//    27.644033, 27.658602, 27.673166, 27.687722, 27.702273, 27.716816,
//    27.731354, 27.745884, 27.760409, 27.774926, 27.789438, 27.803943,
//    27.818441, 27.832933, 27.847419, 27.861898, 27.876370, 27.890837,
//    27.905297, 27.919750, 27.934197, 27.948638, 27.963072, 27.977501,
//    27.991922, 28.006338, 28.020747 };

    // 5 hPa @ 1000 hPa.
    // =================
    airParcelThicknessValuesLUT = new double[951] {
    1.300623, 1.309280, 1.317908, 1.326507, 1.335079, 1.343624,
    1.352141, 1.360632, 1.369096, 1.377535, 1.385947, 1.394334,
    1.402696, 1.411033, 1.419345, 1.427634, 1.435898, 1.444138,
    1.452355, 1.460549, 1.468720, 1.476868, 1.484994, 1.493098,
    1.501180, 1.509240, 1.517278, 1.525295, 1.533292, 1.541267,
    1.549222, 1.557157, 1.565071, 1.572966, 1.580840, 1.588696,
    1.596531, 1.604348, 1.612145, 1.619924, 1.627684, 1.635426,
    1.643149, 1.650855, 1.658542, 1.666212, 1.673864, 1.681498,
    1.689115, 1.696715, 1.704299, 1.711865, 1.719414, 1.726947,
    1.734464, 1.741965, 1.749449, 1.756917, 1.764370, 1.771806,
    1.779228, 1.786633, 1.794024, 1.801399, 1.808759, 1.816104,
    1.823435, 1.830750, 1.838052, 1.845338, 1.852610, 1.859869,
    1.867112, 1.874342, 1.881558, 1.888760, 1.895949, 1.903124,
    1.910285, 1.917433, 1.924568, 1.931689, 1.938797, 1.945893,
    1.952975, 1.960045, 1.967102, 1.974146, 1.981178, 1.988197,
    1.995204, 2.002198, 2.009181, 2.016151, 2.023109, 2.030056,
    2.036990, 2.043913, 2.050824, 2.057723, 2.064611, 2.071487,
    2.078352, 2.085206, 2.092048, 2.098879, 2.105699, 2.112508,
    2.119306, 2.126094, 2.132870, 2.139636, 2.146391, 2.153135,
    2.159869, 2.166592, 2.173305, 2.180008, 2.186700, 2.193382,
    2.200054, 2.206716, 2.213367, 2.220009, 2.226641, 2.233263,
    2.239875, 2.245631, 2.250966, 2.256290, 2.261603, 2.266906,
    2.272198, 2.277480, 2.282752, 2.288012, 2.293263, 2.298504,
    2.303734, 2.308954, 2.314164, 2.319364, 2.324555, 2.329735,
    2.334906, 2.340067, 2.345218, 2.350359, 2.355491, 2.360614,
    2.365727, 2.370830, 2.375925, 2.381010, 2.386085, 2.391152,
    2.396209, 2.401257, 2.406297, 2.411327, 2.416348, 2.421361,
    2.426365, 2.431359, 2.436346, 2.441323, 2.446292, 2.451252,
    2.456204, 2.461147, 2.466082, 2.471008, 2.475926, 2.480836,
    2.485737, 2.490630, 2.495515, 2.500392, 2.505261, 2.510122,
    2.514974, 2.519819, 2.524656, 2.529485, 2.534306, 2.539119,
    2.543925, 2.548722, 2.553513, 2.558295, 2.563070, 2.567837,
    2.572597, 2.577349, 2.582094, 2.586831, 2.591561, 2.596284,
    2.600999, 2.605707, 2.610408, 2.615102, 2.619788, 2.624467,
    2.629139, 2.633805, 2.638463, 2.643114, 2.647758, 2.652395,
    2.657025, 2.661649, 2.666265, 2.670875, 2.675478, 2.680074,
    2.684664, 2.689247, 2.693823, 2.698393, 2.702956, 2.707512,
    2.712062, 2.716605, 2.721142, 2.725673, 2.730197, 2.734715,
    2.739226, 2.743731, 2.748230, 2.752722, 2.757209, 2.761689,
    2.766163, 2.770630, 2.775092, 2.779547, 2.783997, 2.788440,
    2.792877, 2.797309, 2.801734, 2.806153, 2.810567, 2.814974,
    2.819376, 2.823772, 2.828162, 2.832546, 2.836924, 2.841297,
    2.845664, 2.850025, 2.854381, 2.858731, 2.863075, 2.867413,
    2.871746, 2.876074, 2.880396, 2.884712, 2.889023, 2.893329,
    2.897629, 2.901923, 2.906212, 2.910496, 2.914775, 2.919048,
    2.923315, 2.927578, 2.931835, 2.936087, 2.940333, 2.944575,
    2.948811, 2.953042, 2.957268, 2.961488, 2.965704, 2.969915,
    2.974120, 2.978320, 2.982516, 2.986706, 2.990891, 2.995071,
    2.999247, 3.003417, 3.007582, 3.011743, 3.015899, 3.020049,
    3.024195, 3.028336, 3.032473, 3.036604, 3.040731, 3.044853,
    3.048970, 3.053082, 3.057190, 3.061293, 3.065391, 3.069485,
    3.073574, 3.077658, 3.081738, 3.085813, 3.089883, 3.093949,
    3.098011, 3.102068, 3.106120, 3.110168, 3.114211, 3.118250,
    3.122285, 3.126315, 3.130340, 3.134362, 3.138378, 3.142391,
    3.146399, 3.150403, 3.154402, 3.158397, 3.162388, 3.166374,
    3.170356, 3.174334, 3.178308, 3.182278, 3.186243, 3.190204,
    3.194161, 3.198113, 3.202062, 3.206006, 3.209947, 3.213883,
    3.217815, 3.221743, 3.225666, 3.229586, 3.233502, 3.237414,
    3.241321, 3.245225, 3.249125, 3.253020, 3.256912, 3.260800,
    3.264684, 3.268563, 3.272439, 3.276311, 3.280180, 3.284044,
    3.287904, 3.291761, 3.295613, 3.299462, 3.303307, 3.307148,
    3.310986, 3.314819, 3.318649, 3.322475, 3.326298, 3.330116,
    3.333931, 3.337742, 3.341550, 3.345353, 3.349153, 3.352950,
    3.356742, 3.360532, 3.364317, 3.368099, 3.371877, 3.375652,
    3.379423, 3.383190, 3.386954, 3.390714, 3.394471, 3.398224,
    3.401974, 3.405720, 3.409462, 3.413201, 3.416937, 3.420669,
    3.424398, 3.428123, 3.431845, 3.435563, 3.439278, 3.442990,
    3.446698, 3.450402, 3.454104, 3.457802, 3.461496, 3.465187,
    3.468875, 3.472560, 3.476241, 3.479919, 3.483593, 3.487264,
    3.490932, 3.494597, 3.498258, 3.501917, 3.505571, 3.509223,
    3.512872, 3.516517, 3.520159, 3.523797, 3.527433, 3.531065,
    3.534694, 3.538320, 3.541943, 3.545563, 3.549179, 3.552793,
    3.556403, 3.560010, 3.563614, 3.567215, 3.570813, 3.574408,
    3.577999, 3.581588, 3.585174, 3.588756, 3.592335, 3.595912,
    3.599485, 3.603055, 3.606623, 3.610187, 3.613748, 3.617307,
    3.620862, 3.624414, 3.627964, 3.631510, 3.635054, 3.638594,
    3.642132, 3.645667, 3.649198, 3.652727, 3.656253, 3.659776,
    3.663297, 3.666814, 3.670328, 3.673840, 3.677349, 3.680854,
    3.684357, 3.687858, 3.691355, 3.694849, 3.698341, 3.701830,
    3.705316, 3.708799, 3.712280, 3.715758, 3.719233, 3.722705,
    3.726174, 3.729641, 3.733105, 3.736566, 3.740024, 3.743480,
    3.746933, 3.750384, 3.753831, 3.757276, 3.760718, 3.764158,
    3.767595, 3.771029, 3.774460, 3.777889, 3.781316, 3.784739,
    3.788160, 3.791578, 3.794994, 3.798407, 3.801818, 3.805225,
    3.808631, 3.812033, 3.815433, 3.818831, 3.822226, 3.825618,
    3.829008, 3.832395, 3.835780, 3.839162, 3.842541, 3.845919,
    3.849293, 3.852665, 3.856035, 3.859402, 3.862766, 3.866128,
    3.869488, 3.872845, 3.876199, 3.879551, 3.882901, 3.886248,
    3.889593, 3.892935, 3.896275, 3.899612, 3.902947, 3.906279,
    3.909609, 3.912937, 3.916262, 3.919585, 3.922906, 3.926224,
    3.929539, 3.932852, 3.936163, 3.939472, 3.942778, 3.946082,
    3.949383, 3.952682, 3.955979, 3.959273, 3.962566, 3.965855,
    3.969143, 3.972428, 3.975711, 3.978991, 3.982269, 3.985545,
    3.988819, 3.992090, 3.995359, 3.998626, 4.001890, 4.005152,
    4.008412, 4.011670, 4.014925, 4.018178, 4.021429, 4.024678,
    4.027924, 4.031168, 4.034410, 4.037650, 4.040888, 4.044123,
    4.047356, 4.050587, 4.053816, 4.057042, 4.060267, 4.063489,
    4.066709, 4.069927, 4.073143, 4.076356, 4.079567, 4.082777,
    4.085984, 4.089188, 4.092391, 4.095592, 4.098790, 4.101987,
    4.105181, 4.108373, 4.111563, 4.114751, 4.117937, 4.121120,
    4.124302, 4.127482, 4.130659, 4.133834, 4.137008, 4.140179,
    4.143348, 4.146515, 4.149680, 4.152843, 4.156004, 4.159163,
    4.162319, 4.165474, 4.168627, 4.171778, 4.174926, 4.178073,
    4.181218, 4.184360, 4.187501, 4.190639, 4.193776, 4.196911,
    4.200043, 4.203174, 4.206302, 4.209429, 4.212554, 4.215676,
    4.218797, 4.221916, 4.225033, 4.228147, 4.231260, 4.234371,
    4.237480, 4.240587, 4.243692, 4.246795, 4.249897, 4.252996,
    4.256093, 4.259189, 4.262282, 4.265374, 4.268464, 4.271551,
    4.274637, 4.277721, 4.280803, 4.283884, 4.286962, 4.290038,
    4.293113, 4.296186, 4.299257, 4.302325, 4.305393, 4.308458,
    4.311521, 4.314583, 4.317642, 4.320700, 4.323756, 4.326810,
    4.329863, 4.332913, 4.335962, 4.339008, 4.342053, 4.345097,
    4.348138, 4.351177, 4.354215, 4.357251, 4.360285, 4.363318,
    4.366348, 4.369377, 4.372404, 4.375429, 4.378452, 4.381474,
    4.384494, 4.387512, 4.390528, 4.393543, 4.396555, 4.399566,
    4.402576, 4.405583, 4.408589, 4.411593, 4.414595, 4.417596,
    4.420594, 4.423591, 4.426587, 4.429580, 4.432572, 4.435562,
    4.438551, 4.441537, 4.444522, 4.447506, 4.450487, 4.453467,
    4.456445, 4.459422, 4.462397, 4.465370, 4.468341, 4.471311,
    4.474279, 4.477245, 4.480210, 4.483173, 4.486134, 4.489094,
    4.492052, 4.495009, 4.497963, 4.500916, 4.503868, 4.506818,
    4.509766, 4.512712, 4.515657, 4.518600, 4.521542, 4.524482,
    4.527420, 4.530357, 4.533292, 4.536225, 4.539157, 4.542087,
    4.545016, 4.547943, 4.550868, 4.553792, 4.556714, 4.559635,
    4.562554, 4.565471, 4.568387, 4.571301, 4.574214, 4.577125,
    4.580035, 4.582943, 4.585849, 4.588754, 4.591657, 4.594559,
    4.597459, 4.600357, 4.603254, 4.606150, 4.609043, 4.611936,
    4.614827, 4.617716, 4.620604, 4.623490, 4.626374, 4.629257,
    4.632139, 4.635019, 4.637898, 4.640774, 4.643650, 4.646524,
    4.649396, 4.652267, 4.655137, 4.658005, 4.660871, 4.663736,
    4.666599, 4.669461, 4.672322, 4.675181, 4.678038, 4.680894,
    4.683748, 4.686601, 4.689453, 4.692303, 4.695151, 4.697998,
    4.700844, 4.703688, 4.706531, 4.709372, 4.712212, 4.715050,
    4.717887, 4.720722, 4.723556, 4.726389, 4.729220, 4.732049,
    4.734877, 4.737704, 4.740529, 4.743353, 4.746176, 4.748997,
    4.751816, 4.754634, 4.757451, 4.760266, 4.763080, 4.765893,
    4.768704, 4.771513, 4.774321, 4.777128, 4.779934, 4.782738,
    4.785540, 4.788341, 4.791141, 4.793940, 4.796737, 4.799532,
    4.802326, 4.805119, 4.807911, 4.810701, 4.813490, 4.816277,
    4.819063, 4.821847, 4.824631, 4.827412, 4.830193, 4.832972,
    4.835750, 4.838526, 4.841301, 4.844075, 4.846847, 4.849618,
    4.852388, 4.855156, 4.857923, 4.860689, 4.863453, 4.866216,
    4.868977, 4.871738, 4.874496, 4.877254, 4.880010, 4.882765,
    4.885519, 4.888271, 4.891022, 4.893772, 4.896520, 4.899267,
    4.902013, 4.904757, 4.907500, 4.910242, 4.912982, 4.915722,
    4.918459, 4.921196, 4.923931, 4.926665, 4.929398, 4.932129,
    4.934859, 4.937588, 4.940316, 4.943042, 4.945767, 4.948491,
    4.951213, 4.953934, 4.956654, 4.959373, 4.962090, 4.964806,
    4.967521, 4.970234, 4.972946, 4.975657, 4.978367, 4.981076,
    4.983783, 4.986489, 4.989194, 4.991897, 4.994599, 4.997300,
    5.000000, 5.002698, 5.005396, 5.008092, 5.010787, 5.013480,
    5.016172, 5.018863, 5.021553, 5.024242, 5.026929, 5.029615,
    5.032300, 5.034984, 5.037667, 5.040348, 5.043028, 5.045707,
    5.048385, 5.051061, 5.053736, 5.056410, 5.059083, 5.061755,
    5.064425, 5.067094, 5.069762, 5.072429, 5.075095, 5.077759,
    5.080422, 5.083084, 5.085745, 5.088405, 5.091064, 5.093721,
    5.096377, 5.099032, 5.101686, 5.104338, 5.106990, 5.109640,
    5.112289, 5.114937, 5.117584, 5.120229, 5.122874, 5.125517,
    5.128159, 5.130800, 5.133440 };


    gslLUTAirParcelThicknessAcc = gsl_interp_accel_alloc();
    gslLUTAirParcelThickness = gsl_interp_alloc(gsl_interp_linear, numLUTValues);
    gsl_interp_init(gslLUTAirParcelThickness, pressureValuesLUT,
                    airParcelThicknessValuesLUT, numLUTValues);
}


void MProbABLTrajectoriesSource::cleanUpDeltaPressureLUT()
{
    // Free interpolation object ..
    if (gslLUTAirParcelThickness) gsl_interp_free(gslLUTAirParcelThickness);
    if (gslLUTAirParcelThicknessAcc) gsl_interp_accel_free(gslLUTAirParcelThicknessAcc);
    // .. and memory.
    if (pressureValuesLUT) delete[] pressureValuesLUT;
    if (airParcelThicknessValuesLUT) delete[] airParcelThicknessValuesLUT;
}


float MProbABLTrajectoriesSource::airParcelThickness(float p)
{
    if (p < pressureValuesLUT[0]) p = pressureValuesLUT[0];
    if (p > pressureValuesLUT[950]) p = pressureValuesLUT[950];
    return gsl_interp_eval(gslLUTAirParcelThickness, pressureValuesLUT,
                           airParcelThicknessValuesLUT, p,
                           gslLUTAirParcelThicknessAcc);
}



} // namespace Met3D
