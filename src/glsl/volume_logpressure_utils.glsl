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

// Defines sampling routines for a data volume of type LOG_PRESSURE_LEVELS_3D.

/******************************************************************************
***                             SAMPLE METHODS                              ***
*******************************************************************************/

// compute texture coordinates for a given position in log pressure level data
vec3 getRegularLnPTextureCoords(in DataVolumeExtent dve, in vec3 pos)
{
    return vec3(mod(pos.x - dve.westernBoundary, 360.) / dve.eastWestExtent,
                (dve.northernBoundary - pos.y) / dve.northSouthExtent,
                (dve.upperBoundary - pos.z)    / dve.verticalExtent    );
}


// sample log pressure level data at given world position
float sampleLogPressureLevelVolumeAtPos(in sampler3D sampler,
                                        in DataVolumeExtent dve,
                                        in vec3 pos)
{
    vec3 texCoords = getRegularLnPTextureCoords(dve, pos);
    texCoords.z = clamp(texCoords.z, 0., 1.);
    return texture(sampler, texCoords).a;
}


// manual interpolation of ln(p) grids -- for debugging purposes
float s2ampleLogPressureLevelVolumeAtPos(in sampler3D sampler,
                                        in DataVolumeExtent dve, in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLatLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLatLon;
    float mixK = (dve.dataNWCrnr.z - pos.z) / dve.deltaLnP;
    int i = int(mixI);
    int j = int(mixJ);
    int k = int(mixK);

    float scalar_i0j0k0 = texelFetch(sampler, ivec3(i  , j  , k  ), 0).a;
    float scalar_i0j0k1 = texelFetch(sampler, ivec3(i  , j  , k+1), 0).a;
    float scalar_i0j1k0 = texelFetch(sampler, ivec3(i  , j+1, k  ), 0).a;
    float scalar_i0j1k1 = texelFetch(sampler, ivec3(i  , j+1, k+1), 0).a;
    float scalar_i1j0k0 = texelFetch(sampler, ivec3(i+1, j  , k  ), 0).a;
    float scalar_i1j0k1 = texelFetch(sampler, ivec3(i+1, j  , k+1), 0).a;
    float scalar_i1j1k0 = texelFetch(sampler, ivec3(i+1, j+1, k  ), 0).a;
    float scalar_i1j1k1 = texelFetch(sampler, ivec3(i+1, j+1, k+1), 0).a;

    mixK = fract(mixK);
    float scalar_i0j0 = mix(scalar_i0j0k0, scalar_i0j0k1, mixK);
    float scalar_i0j1 = mix(scalar_i0j1k0, scalar_i0j1k1, mixK);
    float scalar_i1j0 = mix(scalar_i1j0k0, scalar_i1j0k1, mixK);
    float scalar_i1j1 = mix(scalar_i1j1k0, scalar_i1j1k1, mixK);

    mixJ = fract(mixJ);
    float scalar_i0 = mix(scalar_i0j0, scalar_i0j1, mixJ);
    float scalar_i1 = mix(scalar_i1j0, scalar_i1j1, mixJ);

    mixI = fract(mixI);
    float scalar = mix(scalar_i0, scalar_i1, mixI);

    return scalar;
}


// sample log pressure level data at given world position
float sampleLogPressureLevelVolumeAtPos_maxNeighbour(in sampler3D sampler,
                                                     in DataVolumeExtent dve,
                                                     in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLatLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLatLon;
    float mixK = (dve.dataNWCrnr.z - pos.z) / dve.deltaLnP;
    int i = int(mixI);
    int j = int(mixJ);
    int k = int(mixK);

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
