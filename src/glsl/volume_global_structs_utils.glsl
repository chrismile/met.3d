/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Michael Kern
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
******************************************************************************/

const float M_MISSING_VALUE = -999.E9;


/*** Global data extents ***/
//   ===================   //
struct DataVolumeExtent
{
    /*** ECMWF-specific ***/
    //   ==============   //
    // 0 = PRESSURE_LEVEL
    // 1 = HYBRID_SIGMA
    int levelType; // vertical level type of volume data
    float deltaLatLon; // space between two grid cells in lat/lon in world space

    vec3 dataSECrnr; // data boundaries in SE
    vec3 dataNWCrnr; // data boundaries in NW

    float tfMinimum; // minimum of variable's transfer function
    float tfMaximum; // minimum of variable's transfer function

    // auxiliary variables
    float westernBoundary; // western lon boundary (- deltaLatLon / 2)
    float eastWestExtent; // lon extent
    float northernBoundary; // northern lat boundary (+ deltaLatLon / 2)
    float northSouthExtent; // lat extent
    bool  gridIsCyclicInLongitude;

    int   nLon; // dimension of lons
    int   nLat; // dimension of lats
    int   nLev; // dimension of levels
    float deltaLnP; // average delta between levels in world space
    float upperBoundary; // upper level boundary (+ deltaLatLon / 2)
    float verticalExtent; // extent in level direction
    float upperPTableBoundary; // upper table boundary in world space
    float vertPTableExtent; // pressure table extent in level direction
};


// defines a simple ray structure containing the origin and direction in world space
struct Ray
{
    vec3 origin; // in world space
    vec3 direction; // in world space
};


struct GridColumnAccel
{
    int   i, j;
    int   k[2];
    float ln_p[2];
    float s[2];
    float psfc_hPa;
    bool  horizontallyMoved;
};


struct HybridSigmaAccel
{
    GridColumnAccel c00, c01, c10, c11;
};


bool shadowRay;
