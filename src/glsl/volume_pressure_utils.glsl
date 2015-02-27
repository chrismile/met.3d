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

// Defines sampling routines for a data volume of type PRESSURE_LEVELS_3D.


/******************************************************************************
***                             SAMPLE METHODS                              ***
*******************************************************************************/

// compute texture coordinates for a given position in pressure level data
vec3 getPressureLevelTextureCoords(in DataVolumeExtent dve,
                                   in sampler1D pTableSampler, in vec3 pos)
{
    vec3 texCoords = vec3(
                mod(pos.x - dve.westernBoundary, 360.) / dve.eastWestExtent,
                (dve.northernBoundary - pos.y) / dve.northSouthExtent,
                (dve.upperPTableBoundary - pos.z) / dve.vertPTableExtent);

    // "Correct" p-coordinate (z-dir) from regular-ln(p) to irregular-p grid.
    // (notes 17Mar2014).
    // NOTE: If the vertical coordinate is outside the data volume texCoords.p
    // is left to be < 0 or > 1 -- this is interpreted by
    // samplePressureLevelVolumeAtPos() to return a missing value.
    if ((texCoords.p >= 0.) && (texCoords.p <= 1.))
        texCoords.p = clamp(texture(pTableSampler, texCoords.p).a, 0., 1.);

    return texCoords;
}


// samples pressure level data at given world position
float samplePressureLevelVolumeAtPos(in sampler3D sampler,
                                     in DataVolumeExtent dve,
                                     in sampler1D pTableSampler,
                                     in vec3 pos)
{
    vec3 texCoords = getPressureLevelTextureCoords(dve, pTableSampler, pos);
    if ((texCoords.p < 0.) || (texCoords.p > 1.)) return M_MISSING_VALUE;
    return texture(sampler, texCoords).a;
}


// manual interpolation of ln(p) grids -- for debugging purposes
float s2amplePressureLevelVolumeAtPos(in sampler3D sampler,
                                      in DataVolumeExtent dve,
                                      in sampler1D pTableSampler,
                                      in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLatLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLatLon;
    int i = int(mixI);
    int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    // Binary search to find model levels k, k1 that enclose pressure level p.
    int k = 0;
    int k1 = dve.nLev - 1;
    int vertOffset = dve.nLon + dve.nLat;

    while (abs(k1 - k) > 1)
    {
        int kMid = (k + k1) / 2;
        float pMid = texelFetch(lonLatLevAxes, kMid + vertOffset, 0).a;
        if (p >= pMid) k = kMid; else k1 = kMid;
    }

    float lnPk  = log(texelFetch(lonLatLevAxes, k + vertOffset, 0).a);
    float lnPk1 = log(texelFetch(lonLatLevAxes, k1 + vertOffset, 0).a);
    float lnP   = log(p);

    // Interpolate linearly in ln(p).
    float mixK = (lnP - lnPk) / (lnPk1 - lnPk);

    // How to handle positions outside the vertical bounds?
    // if ((mixK < 0.) || (mixK > 1.)) return 0.;
    mixK = clamp(mixK, 0., 1.);

    // Fetch the scalar values at the eight grid points surrounding pos.
    float scalar_i0j0k0 = texelFetch(sampler, ivec3(i  , j  , k  ), 0).a;
    float scalar_i0j0k1 = texelFetch(sampler, ivec3(i  , j  , k+1), 0).a;
    float scalar_i0j1k0 = texelFetch(sampler, ivec3(i  , j+1, k  ), 0).a;
    float scalar_i0j1k1 = texelFetch(sampler, ivec3(i  , j+1, k+1), 0).a;
    float scalar_i1j0k0 = texelFetch(sampler, ivec3(i+1, j  , k  ), 0).a;
    float scalar_i1j0k1 = texelFetch(sampler, ivec3(i+1, j  , k+1), 0).a;
    float scalar_i1j1k0 = texelFetch(sampler, ivec3(i+1, j+1, k  ), 0).a;
    float scalar_i1j1k1 = texelFetch(sampler, ivec3(i+1, j+1, k+1), 0).a;

    // Interpolate vertically ...
    float scalar_i0j0 = mix(scalar_i0j0k0, scalar_i0j0k1, mixK);
    float scalar_i0j1 = mix(scalar_i0j1k0, scalar_i0j1k1, mixK);
    float scalar_i1j0 = mix(scalar_i1j0k0, scalar_i1j0k1, mixK);
    float scalar_i1j1 = mix(scalar_i1j1k0, scalar_i1j1k1, mixK);

    // ...and horizontally.
    mixJ = fract(mixJ);
    float scalar_i0 = mix(scalar_i0j0, scalar_i0j1, mixJ);
    float scalar_i1 = mix(scalar_i1j0, scalar_i1j1, mixJ);

    mixI = fract(mixI);
    float scalar = mix(scalar_i0, scalar_i1, mixI);

    return scalar;
}


float samplePressureLevelVolumeAtPos_maxNeighbour(in sampler3D sampler,
                                                  in DataVolumeExtent dve,
                                                  in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLatLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLatLon;
    int i = int(mixI);
    int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    // Binary search to find model levels k, k1 that enclose pressure level p.
    int k = 0;
    int k1 = dve.nLev - 1;
    int vertOffset = dve.nLon + dve.nLat;

    while (abs(k1 - k) > 1)
    {
        int kMid = (k + k1) / 2;
        float pMid = texelFetch(lonLatLevAxes, kMid + vertOffset, 0).a;
        if (p >= pMid) k = kMid; else k1 = kMid;
    }

    // Fetch the scalar values at the eight grid points surrounding pos.
    float scalar_i0j0k0 = texelFetch(sampler, ivec3(i  , j  , k  ), 0).a;
    float scalar_i0j0k1 = texelFetch(sampler, ivec3(i  , j  , k+1), 0).a;
    float scalar_i0j1k0 = texelFetch(sampler, ivec3(i  , j+1, k  ), 0).a;
    float scalar_i0j1k1 = texelFetch(sampler, ivec3(i  , j+1, k+1), 0).a;
    float scalar_i1j0k0 = texelFetch(sampler, ivec3(i+1, j  , k  ), 0).a;
    float scalar_i1j0k1 = texelFetch(sampler, ivec3(i+1, j  , k+1), 0).a;
    float scalar_i1j1k0 = texelFetch(sampler, ivec3(i+1, j+1, k  ), 0).a;
    float scalar_i1j1k1 = texelFetch(sampler, ivec3(i+1, j+1, k+1), 0).a;

    float scalar_i0j0 = max(scalar_i0j0k0, scalar_i0j0k1);
    float scalar_i0j1 = max(scalar_i0j1k0, scalar_i0j1k1);
    float scalar_i1j0 = max(scalar_i1j0k0, scalar_i1j0k1);
    float scalar_i1j1 = max(scalar_i1j1k0, scalar_i1j1k1);

    float scalar_i0 = max(scalar_i0j0, scalar_i0j1);
    float scalar_i1 = max(scalar_i1j0, scalar_i1j1);

    float scalar = max(scalar_i0, scalar_i1);

    return scalar;
}
