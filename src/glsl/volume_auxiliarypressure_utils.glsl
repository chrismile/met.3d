/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017-2018 Bianca Tost
**  Copyright 2017-2018 Marc Rautenhaus
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

// Defines sampling routines for a data volume of type HYBRID_SIGMA_PRESSURE_3D.


/******************************************************************************
***                             SAMPLE METHODS                              ***
*******************************************************************************/

// sample auxiliary pressure volume data
//      indices = index coordinates within the volumes
//      p = pressure in hPa
float sampleAuxiliaryPressureColumnAtP(
        in sampler3D sampler, in DataVolumeExtent dve,
        in sampler3D pressureSampler_hPa, in int i, in int j, in float p)
{
    int kL = 0;
    int kU = dve.nLev - 1;

    // Binary search to find model levels kL, kU that enclose pressure level p.
    while (abs(kU - kL) > 1)
    {
        int kMid = (kL + kU) / 2;

        // pressure in Pa converted to hPa
        float pMid = texelFetch(pressureSampler_hPa, ivec3(i, j, kMid), 0).a;

        if (p >= pMid) kL = kMid; else kU = kMid;
    }

    // pressure in Pa converted to hPa
    float pU = texelFetch(pressureSampler_hPa, ivec3(i, j, kU), 0).a;
    float pL = texelFetch(pressureSampler_hPa, ivec3(i, j, kL), 0).a;

    float lnPU = log(pU);
    float lnPL = log(pL);
    float lnP  = log(p);

    // compute interpolant
    float mixK = (lnP - lnPL) / (lnPU - lnPL);
    mixK = clamp(mixK, 0., 1.);

    // sample scalar values
    float scalarU = texelFetch(sampler, ivec3(i, j, kU), 0).a;
    float scalarL = texelFetch(sampler, ivec3(i, j, kL), 0).a;

//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if ((mixK < 0.) || (mixK > 1.)) return M_MISSING_VALUE;
    return mix(scalarL, scalarU, mixK);
}


// sample volume data at world position pos
float sampleAuxiliaryPressureVolumeAtPos(in sampler3D sampler,
                                         in DataVolumeExtent dve,
                                         in sampler3D pressureSampler_hPa,
                                         in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    int i = int(mixI);
    int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    int i1 = i + 1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    int j1 = j + 1;

    float scalar_i0j0 = sampleAuxiliaryPressureColumnAtP(
                sampler, dve, pressureSampler_hPa, i , j , p);
    float scalar_i1j0 = sampleAuxiliaryPressureColumnAtP(
                sampler, dve, pressureSampler_hPa, i1, j , p);
    float scalar_i0j1 = sampleAuxiliaryPressureColumnAtP(
                sampler, dve, pressureSampler_hPa, i , j1, p);
    float scalar_i1j1 = sampleAuxiliaryPressureColumnAtP(
                sampler, dve, pressureSampler_hPa, i1, j1, p);

    // Check for missing values.
//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if (scalar_i0j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i1j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i0j1 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i1j1 == M_MISSING_VALUE) return M_MISSING_VALUE;

    // Interpolate horizontally.
    mixJ = fract(mixJ);
    float scalar_i0 = mix(scalar_i0j0, scalar_i0j1, mixJ);
    float scalar_i1 = mix(scalar_i1j0, scalar_i1j1, mixJ);

    mixI = fract(mixI);
    float scalar = mix(scalar_i0, scalar_i1, mixI);

    return scalar;
}


float sampleAuxiliaryPressureColumnAtP_maxNeighbour(
        in sampler3D sampler, in DataVolumeExtent dve,
        in sampler3D pressureSampler_hPa, in int i, in int j, in float p)
{
    int kL = 0;
    int kU = dve.nLev - 1;

    // Binary search to find model levels kL, kU that enclose pressure level p.
    while (abs(kU - kL) > 1)
    {
        int kMid = (kL + kU) / 2;

        // pressure in Pa converted to hPa
        float pMid = texelFetch(pressureSampler_hPa, ivec3(i, j, kMid), 0).a;

        if (p >= pMid) kL = kMid; else kU = kMid;
    }

    // sample scalar values
    float scalarU = texelFetch(sampler, ivec3(i, j, kU), 0).a;
    float scalarL = texelFetch(sampler, ivec3(i, j, kL), 0).a;

    return max(scalarL, scalarU);
}


// sample volume data at world position pos
float sampleAuxiliaryPressureVolumeAtPos_maxNeighbour(
                                   in sampler3D sampler,
                                   in DataVolumeExtent dve,
                                   in sampler3D pressureSampler_hPa,
                                   in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    int i = int(mixI);
    int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    int i1 = i + 1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    int j1 = j + 1;

    float scalar_i0j0 = sampleAuxiliaryPressureColumnAtP_maxNeighbour(
                sampler, dve, pressureSampler_hPa, i , j , p);
    float scalar_i1j0 = sampleAuxiliaryPressureColumnAtP_maxNeighbour(
                sampler, dve, pressureSampler_hPa, i1, j , p);
    float scalar_i0j1 = sampleAuxiliaryPressureColumnAtP_maxNeighbour(
                sampler, dve, pressureSampler_hPa, i , j1, p);
    float scalar_i1j1 = sampleAuxiliaryPressureColumnAtP_maxNeighbour(
                sampler, dve, pressureSampler_hPa, i1, j1, p);

    // Interpolate horizontally.
    float scalar_i0 = max(scalar_i0j0, scalar_i0j1);
    float scalar_i1 = max(scalar_i1j0, scalar_i1j1);

    float scalar = max(scalar_i0, scalar_i1);

    return scalar;
}


// Find the highest bottom level and the lowest top level in the four
// grid columns surrounding the given position.
void getAuxiliaryPressureBotTopLevelAtPos(
        in DataVolumeExtent dve, in sampler3D pressureSampler_hPa, in vec3 pos,
        out float zbot, out float ztop)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    int i = int(mixI);
    int j = int(mixJ);
    int i1 = i + 1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    int j1 = j + 1;

    // pressure in Pa converted to hPa
    float pbot_i0j0 =
            texelFetch(pressureSampler_hPa, ivec3(i ,j , dve.nLev - 1), 0).a;
    float pbot_i0j1 =
            texelFetch(pressureSampler_hPa, ivec3(i ,j1, dve.nLev - 1), 0).a;
    float pbot_i1j0 =
            texelFetch(pressureSampler_hPa, ivec3(i1,j , dve.nLev - 1), 0).a;
    float pbot_i1j1 =
            texelFetch(pressureSampler_hPa, ivec3(i1,j1, dve.nLev - 1), 0).a;

    // Find the highest bottom level, i.e. the one with the lowest pressure.
    float pbot_i0 = min(pbot_i0j0, pbot_i0j1);
    float pbot_i1 = min(pbot_i1j0, pbot_i1j1);
    float pbot = min(pbot_i0, pbot_i1);

    zbot = (log(pbot) - pToWorldZParams.x) * pToWorldZParams.y;

    // pressure in Pa converted to hPa
    float ptop_i0j0 = texelFetch(pressureSampler_hPa, ivec3(i ,j , 0), 0).a;
    float ptop_i0j1 = texelFetch(pressureSampler_hPa, ivec3(i ,j1, 0), 0).a;
    float ptop_i1j0 = texelFetch(pressureSampler_hPa, ivec3(i1,j , 0), 0).a;
    float ptop_i1j1 = texelFetch(pressureSampler_hPa, ivec3(i1,j1, 0), 0).a;

    // Lowest top level, i.e. highest pressure.
    float ptop_i0 = max(ptop_i0j0, ptop_i0j1);
    float ptop_i1 = max(ptop_i1j0, ptop_i1j1);
    float ptop = max(ptop_i0, ptop_i1);

    ztop = (log(ptop) - pToWorldZParams.x) * pToWorldZParams.y;
}


// Determine the distance (in worldZ coordinates) between the two hybrid levels
// that enclose the sample point at position "pos". The grid column at i,j
// coordinates (west/north corner of the horizontal grid box) is used.
float getAuxiliaryPressureApproxWorldZLevelDistanceAtPos(
        in DataVolumeExtent dve, in sampler3D pressureSampler_hPa, in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    int i = int(mixI);
    int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    int kL = 0;
    int kU = dve.nLev - 1;

    // Binary search to find model levels kL, kU that enclose pressure level p.
    while (abs(kU - kL) > 1)
    {
        int kMid = (kL + kU) / 2;

        // pressure in Pa converted to hPa
        float pMid = texelFetch(pressureSampler_hPa, ivec3(i, j, kMid), 0).a;

        if (p >= pMid) kL = kMid; else kU = kMid;
    }

    // pressure in Pa converted to hPa
    float pU = texelFetch(pressureSampler_hPa, ivec3(i, j, kU), 0).a;
    float pL = texelFetch(pressureSampler_hPa, ivec3(i, j, kL), 0).a;

    float hzU = (log(pU) - pToWorldZParams.x) * pToWorldZParams.y;
    float hzL = (log(pL) - pToWorldZParams.x) * pToWorldZParams.y;
    float hz = abs(hzU - hzL);

    return hz;
}

