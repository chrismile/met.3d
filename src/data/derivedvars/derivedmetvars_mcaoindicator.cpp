/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020 Marcel Meyer [*]
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
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
#include "derivedmetvars_mcaoindicator.h"

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


using namespace std;

namespace Met3D
{

MMCAOIndexProcessor_Papritz2015::MMCAOIndexProcessor_Papritz2015(QString standardName)
    : MDerivedDataFieldProcessor(
        standardName,
        QStringList() << "air_temperature"
                      << "skin_temperature/SURFACE_2D"
                      << "surface_air_pressure/SURFACE_2D"
                      << "land_sea_mask/SURFACE_2D")

{
}


void MMCAOIndexProcessor_Papritz2015::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "air_temperature"
    // input 1 = "skin_temperature"
    // input 2 = "surface_air_pressure"
    // input 3 = "land_sea_mask"

    MStructuredGrid *airTemperatureGrid = inputGrids.at(0);
    MRegularLonLatGrid *skinTemperatureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(1));
    MRegularLonLatGrid *surfaceAirPressureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(2));
    MRegularLonLatGrid *landSeaMaskGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(3));

    // MCAO index 1 is calculated as the difference of the potential temperature
    // at the surface and the potential temperature at a certain pressure level.
    // We first get PT at the surface, then PT at each pressure level and
    // from this compute mcao index 1 and write it to the derivedGrid

    // initialize variable for storing information about the proportion of each grid-cell
    // covered with ocean / land (use land-sea mask from ERA5)
    float propGridCellLand = 0;

    for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
    {
        for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
        {

             // consider only water surfaces. According to the ECMWF all grid cells
             // with values of 0.5 or below can be considered water surfaces
             // https://apps.ecmwf.int/codes/grib/param-db/?id=172
             propGridCellLand = landSeaMaskGrid->getValue(j, i);
             if (propGridCellLand <= 0.5)
             {

                 // get PT at the surface
                 float theta_skin = M_MISSING_VALUE;
                 float t_skin_K = skinTemperatureGrid->getValue(j, i);
                 float p_surface_Pa = surfaceAirPressureGrid->getValue(j, i);

                 if ( (t_skin_K != M_MISSING_VALUE) && (p_surface_Pa != M_MISSING_VALUE) )
                 {
                    theta_skin = potentialTemperature_K(
                            t_skin_K,
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
                     if ( (theta_skin != M_MISSING_VALUE) && (theta_K != M_MISSING_VALUE) )
                     {
                         mcao_index = theta_skin - theta_K;
                     }
                     derivedGrid->setValue(k, j, i, mcao_index);
                }
             }

             // set derived grid to missing values for all grid cells over land
             else if (propGridCellLand > 0.5)
             {
                 for (unsigned int k = 0; k < derivedGrid->getNumLevels(); k++)
                 {
                     derivedGrid->setValue(k, j, i, M_MISSING_VALUE);
                 }
             }
        }
    }
}


MMCAOIndexProcessor_Papritz2015_nonMasked::MMCAOIndexProcessor_Papritz2015_nonMasked(QString standardName)
    : MDerivedDataFieldProcessor(
          standardName,
          QStringList() << "air_temperature"
                        << "skin_temperature/SURFACE_2D"
                        << "surface_air_pressure/SURFACE_2D")
{
}


void MMCAOIndexProcessor_Papritz2015_nonMasked::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "air_temperature"
    // input 1 = "skin_temperature"
    // input 2 = "surface_air_pressure"

    MStructuredGrid *airTemperatureGrid = inputGrids.at(0);
    MRegularLonLatGrid *skinTemperatureGrid =
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
             float theta_skin = M_MISSING_VALUE;
             float t_skin_K = skinTemperatureGrid->getValue(j, i);
             float p_surface_Pa = surfaceAirPressureGrid->getValue(j, i);

             if ( (t_skin_K != M_MISSING_VALUE) && (p_surface_Pa != M_MISSING_VALUE) )
             {
                theta_skin = potentialTemperature_K(
                        t_skin_K,
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
                 if ( (theta_skin != M_MISSING_VALUE) && (theta_K != M_MISSING_VALUE) )
                 {
                     mcao_index = theta_skin - theta_K;
                 }
                 derivedGrid->setValue(k, j, i, mcao_index);
            }

        }
    }
}



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


MMCAOIndexProcessor_BracegirdleGray2008::MMCAOIndexProcessor_BracegirdleGray2008()
    : MDerivedDataFieldProcessor(
          "mcao_index_3_(PTz_wet-sst)",
          QStringList() << "air_temperature"
                        << "surface_temperature/SURFACE_2D")
{
}


//TODO((mr, 06Apr2020): currently wrong; theta-w computation needs to be fixed.
//    DO NOT USE.
void MMCAOIndexProcessor_BracegirdleGray2008::compute(
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
//TODO (mr, 06Apr2020): This needs to be replaced by the non-saturated theta-w!
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


MMCAOIndex2DProcessor_YuliaP::MMCAOIndex2DProcessor_YuliaP(QString levelTypeString)
    : MDerivedDataFieldProcessor(
          QString("mcao_index_4_(PTs_-_PT_850hPa)"
                  "_fixed_level__from_%1").arg(levelTypeString),
          QStringList() << "surface_air_pressure"
                        << QString("air_temperature/%1").arg(levelTypeString)
                        << "skin_temperature/SURFACE_2D"
                        << "surface_air_pressure/SURFACE_2D"
                        << "land_sea_mask/SURFACE_2D")
{
}


void MMCAOIndex2DProcessor_YuliaP::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 dummy
    // input 1 = "air_temperature"
    // input 2 = "skin_temperature"
    // input 3 = "surface_air_pressure"
    // input 4 = "land sea mask"

    MStructuredGrid *airTemperatureGrid = inputGrids.at(1);
    MRegularLonLatGrid *skinTemperatureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(2));
    MRegularLonLatGrid *surfaceAirPressureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(3));
    MRegularLonLatGrid *landSeaMaskGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(4));

    // MCAO index 4 is calculateD as the difference between the potential
    // temperature at the surface and the potential temperature at a fixed
    // pressure level of 850 hPa. MCAO index 4 yields a 2-D derived grid
    // whereas MCAO indicators 1-3 result in 3-D grids.

    // initialize variable for storing information about the proportion of each grid-cell
    // covered with ocean / land (use land-sea mask from ERA5)
    float propGridCellLand = 0;

    for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
    {
       for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
       {
            // calculate potential temperature at the surface (j,i)
            float theta_surface = M_MISSING_VALUE;
            float t_surface_K = surfaceTemperatureGrid->getValue(j, i);
            float p_surface_Pa = surfaceAirPressureGrid->getValue(j, i);

           // consider only water surfaces. According to the ECMWF all grid cells
           // with values of 0.5 or below can be considered water surfaces
           // https://apps.ecmwf.int/codes/grib/param-db/?id=172
           propGridCellLand = landSeaMaskGrid->getValue(j, i);
           if (propGridCellLand <= 0.5)
           {

                // calculate potential temperature at the surface (j,i)
                float theta_skin = M_MISSING_VALUE;
                float t_skin_K = skinTemperatureGrid->getValue(j, i);
                float p_surface_Pa = surfaceAirPressureGrid->getValue(j, i);

                if ( (t_skin_K != M_MISSING_VALUE) && (p_surface_Pa != M_MISSING_VALUE) )
                {
                    theta_skin = potentialTemperature_K(
                            t_skin_K,
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
                if ( (theta_skin != M_MISSING_VALUE) && (theta_K != M_MISSING_VALUE) )
                {
                    mcao_index = theta_skin - theta_K;
                }

                static_cast<MRegularLonLatGrid*>(derivedGrid)->setValue(
                            j, i, mcao_index);
           }
           else if (propGridCellLand > 0.5)
           {
               // set derived grid to missing values for all grid cells over land
               static_cast<MRegularLonLatGrid*>(derivedGrid)->setValue(
                           j, i, M_MISSING_VALUE);
           }
       }
   }
}


MMCAOIndexProcessor_Michel2018::MMCAOIndexProcessor_Michel2018()
    : MDerivedDataFieldProcessor(
        "mcao_index_5_(SST - T)",
        QStringList() << "air_temperature"
                      << "sea_surface_temperature/SURFACE_2D"
                      << "surface_air_pressure/SURFACE_2D"
                      << "land_sea_mask/SURFACE_2D")

{
}


void MMCAOIndexProcessor_Michel2018::compute(
        QList<MStructuredGrid *> &inputGrids, MStructuredGrid *derivedGrid)
{
    // input 0 = "air_temperature"
    // input 1 = "sea_surface_temperature"
    // input 2 = "surface_air_pressure"
    // input 3 = "land_sea_mask"

    MStructuredGrid *airTemperatureGrid = inputGrids.at(0);
    MRegularLonLatGrid *seasurfaceTemperatureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(1));
    MRegularLonLatGrid *surfaceAirPressureGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(2));
    MRegularLonLatGrid *landSeaMaskGrid =
            dynamic_cast<MRegularLonLatGrid*>(inputGrids.at(3));

    // MCAO index 5 is calculated as the difference of SST and T

    // initialize variable for storing information about the proportion of each grid-cell
    // covered with ocean / land (use land-sea mask from ERA5)
    float propGridCellLand = 0;

    for (unsigned int j = 0; j < derivedGrid->getNumLats(); j++)
    {
        for (unsigned int i = 0; i < derivedGrid->getNumLons(); i++)
        {

             // consider only water surfaces. According to the ECMWF all grid cells
             // with values of 0.5 or below can be considered water surfaces
             // https://apps.ecmwf.int/codes/grib/param-db/?id=172
             propGridCellLand = landSeaMaskGrid->getValue(j, i);
             if (propGridCellLand <= 0.5)
             {

                 // get SST (2-D)
                 float sst_K = seasurfaceTemperatureGrid->getValue(j, i);

                 // get T (3-D)
                 for (unsigned int k = 0; k < derivedGrid->getNumLevels(); k++)
                 {

                     // get T at pressure level k
                     float t_K = airTemperatureGrid->getValue(k, j, i);

                     // calculate the mcao index
                     float mcao_index =  M_MISSING_VALUE;
                     if ( (sst_K != M_MISSING_VALUE) && (t_K != M_MISSING_VALUE) )
                     {
                         mcao_index = sst_K - t_K;
                     }
                     derivedGrid->setValue(k, j, i, mcao_index);
                }
             }

             // set derived grid to missing values for all grid cells over land
             else if (propGridCellLand > 0.5)
             {
                 for (unsigned int k = 0; k < derivedGrid->getNumLevels(); k++)
                 {
                     derivedGrid->setValue(k, j, i, M_MISSING_VALUE);
                 }
             }
        }
    }
}


} // namespace Met3D
