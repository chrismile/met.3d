
/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2020 Marcel Meyer
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

// standard library imports
#include <iostream>
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/metroutines.h"
#include "util/metroutines_experimental.h"

#include "derived_standard.h"
#include "mcao_indicator.h"

using namespace std;

namespace Met3D
{

// MCAO Index 1
// ============

MMCAOIndexProcessor_Papritz2015::MMCAOIndexProcessor_Papritz2015(QString standardName)
    : MDerivedDataFieldProcessor(
          standardName,
          QStringList() << "air_temperature"
                        << "surface_temperature/SURFACE_2D"
                        << "surface_air_pressure/SURFACE_2D")
{
}

void MMCAOIndexProcessor_Papritz2015::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "air_temperature"
    // input 1 = "surface_air_temperature"
    // input 2 = "surface_air_pressure"

    MStructuredGrid *airTemperatureGrid = inputGrids.at(0);
    MRegularLonLatGrid *surfaceTemperatureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(1));
    MRegularLonLatGrid *surfaceAirPressureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(2));

    // MCAO index 1 is calculated as the difference of the potential temperature
    // at the surface and the potential temperature at a certain pressure level.
    // We first get PT at the surface, then PT at each pressure level and
    // from this compute mcao index 1 and write it to the derivedGrid
    for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
    {
        for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
        {

             // get PT at the surface
             float theta_surface = M_MISSING_VALUE;
             float t_surface_K = surfaceTemperatureGrid->getValue(j, i);
             float p_surface_Pa = surfaceAirPressureGrid->getValue(j, i);

             if ( (t_surface_K != M_MISSING_VALUE) && (p_surface_Pa != M_MISSING_VALUE) )
             {
                theta_surface = potentialTemperature_K(
                        t_surface_K,
                        p_surface_Pa);
             }

             for (unsigned int k = 0; k < derivedGrid->getNumLevels(); k++)
             {

                 // get PT at pressure level k
                 float theta_K = M_MISSING_VALUE;
                 float t_K = airTemperatureGrid->getValue(k, j, i);
                 float p_Pa = airTemperatureGrid->getPressure(k, j, i) * 100.;

                 if ( (t_K != M_MISSING_VALUE) && (p_Pa != M_MISSING_VALUE) )
                 {
                     theta_K = potentialTemperature_K(
                                 t_K,
                                 p_Pa);
                 }

                 // calculate the mcao index 1
                 float mcao_index =  M_MISSING_VALUE;
                 if ( (theta_surface != M_MISSING_VALUE) && (theta_K != M_MISSING_VALUE) )
                 {
                     mcao_index = theta_surface - theta_K;
                 }
                 derivedGrid->setValue(k, j, i, mcao_index);

             }
        }
    }
}


// MCAO Index 2
// ============

MMCAOIndexProcessor_Kolstad2008::MMCAOIndexProcessor_Kolstad2008()
    : MMCAOIndexProcessor_Papritz2015(
     "mcao_index_2_(PTs_-_PTz)/(ps_-_pz)"
     )

{
}

void MMCAOIndexProcessor_Kolstad2008::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{

    // MCAO Index 2 (Kolstad, 2008) is calculated as a function of MCAO index 1
    // (Papritz, 2015) and the pressure difference between the surface and the
    // vertical level of interest. Here we first compute MCAO index 1, then
    // the pressure difference and then MCAO index 2.

    // Compute the values of MCAO Index 1 and store these in derivedGrid.
    MMCAOIndexProcessor_Papritz2015::compute(inputGrids,derivedGrid);

    // get input surface pressure grid
    MRegularLonLatGrid *surfaceAirPressureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(2));

    for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
    {
        for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
        {

            // get surface pressure
            float p_surface_Pa = surfaceAirPressureGrid->getValue(j, i);

            for (unsigned int k = 0; k < derivedGrid->getNumLevels(); k++)
            {

                // get pressure at vertical level k
                float p_Pa = derivedGrid->getPressure(k, j, i) * 100.;

                // calculate MCAO index 2
                float mcao_index2 = M_MISSING_VALUE;
                float mcao_index1 =  derivedGrid->getValue(k, j, i);
                if (mcao_index1 != M_MISSING_VALUE)
                {
                    if ( (p_surface_Pa != M_MISSING_VALUE) &&
                         (p_Pa != M_MISSING_VALUE) )
                    {
                        mcao_index2 = mcao_index1/(p_surface_Pa/100.-p_Pa/100.);
                    }
                 }
                 derivedGrid->setValue(k, j, i, mcao_index2);

            }
       }
   }
}


// MCAO Index 3
// ============

MMCAOIndexProcessor_Gray2008::MMCAOIndexProcessor_Gray2008()
    : MDerivedDataFieldProcessor(
          "mcao_index_3_(PTz_wet-sst)",
          QStringList() << "air_temperature"
                        << "surface_temperature/SURFACE_2D")
{
}

void MMCAOIndexProcessor_Gray2008::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "air_temperature"
    // input 1 = "surface_air_temperature"
    // input 2 = "surface_air_pressure"

    MStructuredGrid *airTemperatureGrid = inputGrids.at(0);
    MRegularLonLatGrid *surfaceTemperatureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(1));

    // MCAO index 3 is calculated as the difference of the wet bulb potential
    // temperature and the sea surface temperature. We first get SST for each
    // horizontal grid-cell, then the wet bulb potential temperature at each
    // pressure level and then compute mcao index 3.
    for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
    {
        for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
        {

            // get SST
            float t_surface_K = surfaceTemperatureGrid->getValue(j, i);

            for (unsigned int k = 0; k < derivedGrid->getNumLevels(); k++)
            {

                // get wet bulb potential temperature at pressure level k
                float theta_w_K = M_MISSING_VALUE;
                float t_K = airTemperatureGrid->getValue(k, j, i);
                float p_Pa = airTemperatureGrid->getPressure(k, j, i) * 100.;

                if ( (t_K != M_MISSING_VALUE) && (p_Pa != M_MISSING_VALUE) )
                {
                    theta_w_K = wetBulbPotentialTemperatureOfSaturatedAdiabat_K_MoisseevaStull(
                                t_K,
                                p_Pa);
                }

                // calculate MCAO index 3
                float mcao_index =  M_MISSING_VALUE;
                if ( (t_surface_K != M_MISSING_VALUE) && (theta_w_K != M_MISSING_VALUE) )
                {
                    mcao_index = theta_w_K - t_surface_K;
                }
                derivedGrid->setValue(k, j, i, mcao_index);

            }
       }
   }
}


// MCAO Index 4
// ============

MMCAOIndex2DProcessor_YuliaP::MMCAOIndex2DProcessor_YuliaP(QString levelTypeString)
    : MDerivedDataFieldProcessor(
          QString("mcao_index_4_(PTs-PT_850hPa)"
                  "_fixed_level__from_%1").arg(levelTypeString),
          QStringList() << "surface_air_pressure"
                        << QString("air_temperature/%1").arg(levelTypeString)
                        << "surface_temperature/SURFACE_2D"
                        << "surface_air_pressure/SURFACE_2D")


{
}

void MMCAOIndex2DProcessor_YuliaP::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{

    // input 0 dummy
    // input 3 = "surface_air_pressure"
    // input 2 = "surface_air_temperature"
    // input 1 = "air_temperature"

    MRegularLonLatGrid *surfaceAirPressureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(3));
    MRegularLonLatGrid *surfaceTemperatureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(2));
    MStructuredGrid *airTemperatureGrid = inputGrids.at(1);

    // MCAO index 4 is calculate as the difference between the potential
    // temperature at the surface and the potential temperature at a fixed
    // pressure level of 850 hPa. MCAO index 4 yields a 2-D derived grid
    // whereas MCAO indicators 1-3 result in 3-D grids.
    for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
    {
       for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
       {

            // calculate potential temperature at the surface (j,i)
            float theta_surface = M_MISSING_VALUE;
            float t_surface_K = surfaceTemperatureGrid->getValue(j, i);
            float p_surface_Pa = surfaceAirPressureGrid->getValue(j, i);

            if ( (t_surface_K != M_MISSING_VALUE) && (p_surface_Pa != M_MISSING_VALUE) )
            {
                theta_surface = potentialTemperature_K(
                        t_surface_K,
                        p_surface_Pa);
            }

            // calculate potential temperature at 850 hPa
            float theta_K = M_MISSING_VALUE;
            float iLong=airTemperatureGrid->getLons()[i];
            float iLat=airTemperatureGrid->getLats()[j];
            float t_K = airTemperatureGrid->interpolateValue(iLong, iLat, 850);

            if (t_K != M_MISSING_VALUE)
            {
                theta_K = potentialTemperature_K(
                            t_K,
                            850*100.);
            }

            // calculate MCAO index 4
            float mcao_index =  M_MISSING_VALUE;
            if ( (theta_surface != M_MISSING_VALUE) && (theta_K != M_MISSING_VALUE) )
            {
                mcao_index = theta_surface - theta_K;
            }

            static_cast<MRegularLonLatGrid*>(derivedGrid)->setValue(
                        j, i, mcao_index);

       }
   }
}


} // namespace Met3D
