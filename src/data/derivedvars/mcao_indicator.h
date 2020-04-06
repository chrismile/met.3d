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
#ifndef MCAO_INDICATOR_H
#define MCAO_INDICATOR_H

// Summary:
// Each of the classes MMCAOIndexProcessor_xyz computes an index that indicates
// the occurence of a Marine Cold Air Outbreak (MCAO).
// Different variants (_xyz) of the MCAO index are calculated corresponding to
// different MCAO indicators that have been used in the literature on MCAOs


// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "data/processingwpdatasource.h"
#include "data/structuredgrid.h"
#include "data/datarequest.h"

#include "derivedmetvarsdatasource.h"


namespace Met3D
{


/**
 @brief MCAO Index 1: Difference in Potential Temperature (PT) at the sea surface
 and Potential Temperature at some pressure level:
 (PT_surface - PT_pressureLevel).
 Motivated by the use of PT at different pressure levels in the literature
  - (Papritz, 2015; Journal of Climate) and (Yulia P., MPI) using:
    PT_surface - PT_850hPa
  - (Fletcher, 2016; Journal of Climate) using:
    PT_surface - PT_800hPa
 we compute the index for all pressure levels.
*/
class MMCAOIndexProcessor_Papritz2015
        : public MDerivedDataFieldProcessor
{
public:
    MMCAOIndexProcessor_Papritz2015(QString standardName =
            "mcao_index_1_(PTs-PTz)");

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};

/**
 @brief MCAO Index 2: Difference in Potential Temperature (PT) at the sea
 surface and PT at some vertical level divided by the pressure difference
 between the surface and the vertical level:
 (PT_surface - PT_pressureLevel)/(P_surface - P_at_pressureLevel).
 Motivated by the use of PT at different pressure levels in the literature
  - (Kolstad, 2008; Clim Dyn) using:
    (PT_surface - PT_700hPa)/(P_surface-P_at_700hPa)
  - (Landgren,2019; Clim Dyn) using:
    (PT_surface - PT_500hPa)/(P_surface-P_at_500hPa)
 we compute the index for all pressure levels.
 As MCAO Index 2 is very similar to MCAO Index 1, it is implemented as a
 derived class of MMCAOIndexProcessor_Papritz2015.
*/
class MMCAOIndexProcessor_Kolstad2008
        : public MMCAOIndexProcessor_Papritz2015
{
public:
    MMCAOIndexProcessor_Kolstad2008();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};

/**
 @brief MCAO Index 3: Difference in potential wet bulb temperature (PT_wet) and
 temperature at the sea surface (PT_wet_pressureLevel - SST)
 We compute the index
  - (Gray, 2008; Int. J. Clim.): Theta_wet_700hPa - SST
 for all pressure levels.
*/
class MMCAOIndexProcessor_Gray2008
        : public MDerivedDataFieldProcessor
{
public:
    MMCAOIndexProcessor_Gray2008();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};

/**
 @brief MCAO Index 4: Difference in Potential Temperature (PT) at the sea
 surface and PT at a fixed pressure level of 850 hPa
 (PT_surface - PT_850hPa).
 This 2-D index field is calculated for direct comparison with the work of
 Yulia P. from MPI-M. Note that MCAO Index 4 is equivalent to MCAO Index 1
 taken at pressure level 850 hPa.
 */
class MMCAOIndex2DProcessor_YuliaP
        : public MDerivedDataFieldProcessor
{
public:
    MMCAOIndex2DProcessor_YuliaP(
            QString levelTypeString);

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};

}

#endif // MCAO_INDICATOR_H
