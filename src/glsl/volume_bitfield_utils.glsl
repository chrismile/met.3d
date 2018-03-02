/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2015      Michael Kern
**  Copyright 2017-2018 Bianca Tost
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

/*****************************************************************************
 ***                             CONSTANTS
 *****************************************************************************/

const int RAYCAST_SINGLE_BIT = 0;
const int RAYCAST_ALL_BITS_LOOP = 1;
const int RAYCAST_ALL_BITS_ARRAY = 2;

//uint raycastMode = RAYCAST_SINGLE_BIT;
//const uint maxBits = 51;

uint raycastMode = RAYCAST_ALL_BITS_LOOP;
const uint maxBits = 51;

//uint raycastMode = RAYCAST_ALL_BITS_ARRAY;
//const uint maxBits = 51;


/*****************************************************************************
 ***                             SUBROUTINES
 *****************************************************************************/

// sample bitfield of any data type at world position and given bit
subroutine float sampleDataBitfieldAtPosType(vec3 pos, uint bit);
// sample all bits in bitfield of any datqa type at world position
subroutine float[maxBits] sampleDataAllBitsAtPosType(vec3 pos);

subroutine vec3 computeGradientBitfieldAtPosType(vec3 pos, vec3 h, uint bit);

// define subroutine uniform names
subroutine uniform sampleDataBitfieldAtPosType sampleDataBitfieldAtPos;
subroutine uniform sampleDataAllBitsAtPosType sampleDataAllBitsAtPos;
subroutine uniform computeGradientBitfieldAtPosType computeGradientBitfieldAtPos;


// method to obtain bits from current grid via int indices
float fetchBit(in ivec3 gridPos, uint bit)
{
    uvec2 bitfield = texelFetch(flagsVolume, gridPos, 0).rg;
    uint bitfieldPart = (bit < 32) ? bitfield.r : bitfield.g;
    bit = uint(mod(bit,32));
    bool bitIsSet = bool(bitfieldPart & (1 << bit));
    return (bitIsSet ? 1. : 0.);
}


float[maxBits] fetchAllBits(in ivec3 gridPos)
{
    float result[maxBits];

    uvec2 bitfield = texelFetch(flagsVolume, gridPos, 0).rg;

    for (uint bit = 0; bit < min(maxBits, 32); bit++)
    {
        bool bitIsSet = bool(bitfield.r & (1 << bit));
        result[bit] = (bitIsSet ? 1. : 0.);
    }
    for (uint bit = 32; bit < min(maxBits, 64); bit++)
    {
        bool bitIsSet = bool(bitfield.g & (1 << (bit-32)));
        result[bit] = (bitIsSet ? 1. : 0.);
    }

    return result;
}


// HybridSigmaPressure
// ===================

float sampleHybridSigmaColumnSingleBitAtP(in sampler3D sampler,
                                          in DataVolumeExtent dve,
                                          in sampler2D sfcSampler,
                                          in sampler1D hybCoeffSampler,
                                          in int i, in int j, in float p,
                                          uint bit)
{
    int kL = 0;
    int kU = dve.nLev - 1;

    // pressure in Pa converted to hPa
    float psfc = texelFetch(sfcSampler, ivec2(i,j), 0).a / 100.0;

    // Binary search to find model levels kL, kU that enclose pressure level p.
    while (abs(kU - kL) > 1)
    {
        int kMid = (kL + kU) / 2;

        float a = texelFetch(hybCoeffSampler, kMid, 0).a;
        float b = texelFetch(hybCoeffSampler, kMid + dve.nLev, 0).a;
        float pMid = a + b * psfc;

        if (p >= pMid) kL = kMid; else kU = kMid;
    }

    float aU = texelFetch(hybCoeffSampler, kU, 0).a;
    float bU = texelFetch(hybCoeffSampler, kU + dve.nLev, 0).a;

    float aL = texelFetch(hybCoeffSampler, kL, 0).a;
    float bL = texelFetch(hybCoeffSampler, kL + dve.nLev, 0).a;

    float pU = aU + bU * psfc;
    float pL = aL + bL * psfc;

    float lnPU = log(pU);
    float lnPL = log(pL);
    float lnP  = log(p);

    // compute interpolant
    float mixK = (lnP - lnPL) / (lnPU - lnPL);
    mixK = clamp(mixK, 0., 1.);

    // sample scalar values
    float scalarU = fetchBit(ivec3(i, j, kU), bit);
    float scalarL = fetchBit(ivec3(i, j, kL), bit);

//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if ((mixK < 0.) || (mixK > 1.)) return M_MISSING_VALUE;

    return mix(scalarL, scalarU, mixK);
}


// sample volume data at world position pos
float sampleHybridSigmaVolumeSingleBitAtPos(in sampler3D sampler,
                                            in DataVolumeExtent dve,
                                            in sampler2D sfcSampler,
                                            in sampler1D hybCoeffSampler,
                                            in vec3 pos,
                                            in uint bit)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    const int i = int(mixI);
    const int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    int i1 = i+1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    const int j1 = j+1;

    const float scalar_i0j0 = sampleHybridSigmaColumnSingleBitAtP(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i , j , p, bit);
    const float scalar_i1j0 = sampleHybridSigmaColumnSingleBitAtP(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i1, j , p, bit);
    const float scalar_i0j1 = sampleHybridSigmaColumnSingleBitAtP(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i , j1, p, bit);
    const float scalar_i1j1 = sampleHybridSigmaColumnSingleBitAtP(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i1, j1, p, bit);

    // Check for missing values. Due to consistency of the acceleration
    // structures, this is done after all four columns have been retrieved.
//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if (scalar_i0j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i1j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i0j1 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i1j1 == M_MISSING_VALUE) return M_MISSING_VALUE;

    // Interpolate horizontally.
    mixJ = fract(mixJ);
    mixI = fract(mixI);

    const float scalar_i0 = mix(scalar_i0j0, scalar_i0j1, mixJ);
    const float scalar_i1 = mix(scalar_i1j0, scalar_i1j1, mixJ);

    return mix(scalar_i0, scalar_i1, mixI);
}


// sample hybrid sigma pressure volume data
//      indices = index coordinates within the volumes
//      p = pressure in hPa
float[maxBits] sampleHybridSigmaColumnBitfieldAtP(in sampler3D sampler,
                                                  in DataVolumeExtent dve,
                                                  in sampler2D sfcSampler,
                                                  in sampler1D hybCoeffSampler,
                                                  in int i, in int j, in float p)
{
    int kL = 0;
    int kU = dve.nLev - 1;

    // pressure in Pa converted to hPa
    float psfc = texelFetch(sfcSampler, ivec2(i,j), 0).a / 100.0;

    // Binary search to find model levels kL, kU that enclose pressure level p.
    while (abs(kU - kL) > 1)
    {
        int kMid = (kL + kU) / 2;

        float a = texelFetch(hybCoeffSampler, kMid, 0).a;
        float b = texelFetch(hybCoeffSampler, kMid + dve.nLev, 0).a;
        float pMid = a + b * psfc;

        if (p >= pMid) kL = kMid; else kU = kMid;
    }

    float aU = texelFetch(hybCoeffSampler, kU, 0).a;
    float bU = texelFetch(hybCoeffSampler, kU + dve.nLev, 0).a;

    float aL = texelFetch(hybCoeffSampler, kL, 0).a;
    float bL = texelFetch(hybCoeffSampler, kL + dve.nLev, 0).a;

    float pU = aU + bU * psfc;
    float pL = aL + bL * psfc;

    float lnPU = log(pU);
    float lnPL = log(pL);
    float lnP  = log(p);

    // compute interpolant
    float mixK = (lnP - lnPL) / (lnPU - lnPL);
    mixK = clamp(mixK, 0., 1.);

    // sample scalar values
    float scalarU[maxBits] = fetchAllBits(ivec3(i, j, kU));
    float scalarL[maxBits] = fetchAllBits(ivec3(i, j, kL));

//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if ((mixK < 0.) || (mixK > 1.)) return M_MISSING_VALUE;
    float scalar[maxBits];

    // bit-wise interpolation
    for (uint bit = 0; bit < maxBits; bit++)
    {
        scalar[bit] = mix(scalarL[bit], scalarU[bit], mixK);
    }

    return scalar;

}


// sample volume data at world position pos
float[maxBits] sampleHybridSigmaVolumeAllBitsAtPos(in sampler3D sampler,
                                                   in DataVolumeExtent dve,
                                                   in sampler2D sfcSampler,
                                                   in sampler1D hybCoeffSampler,
                                                   in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    const int i = int(mixI);
    const int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    int i1 = i+1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    const int j1 = j+1;

    const float scalar_i0j0[maxBits] = sampleHybridSigmaColumnBitfieldAtP(
                sampler, dve, sfcSampler, hybCoeffSampler, i , j , p);
    const float scalar_i1j0[maxBits] = sampleHybridSigmaColumnBitfieldAtP(
                sampler, dve, sfcSampler, hybCoeffSampler, i1, j , p);
    const float scalar_i0j1[maxBits] = sampleHybridSigmaColumnBitfieldAtP(
                sampler, dve, sfcSampler, hybCoeffSampler, i , j1, p);
    const float scalar_i1j1[maxBits] = sampleHybridSigmaColumnBitfieldAtP(
                sampler, dve, sfcSampler, hybCoeffSampler, i1, j1, p);

    // Check for missing values. Due to consistency of the acceleration
    // structures, this is done after all four columns have been retrieved.
//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if (scalar_i0j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i1j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i0j1 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i1j1 == M_MISSING_VALUE) return M_MISSING_VALUE;

    // Interpolate horizontally.
    mixJ = fract(mixJ);
    float scalar_i0[maxBits], scalar_i1[maxBits];
    mixI = fract(mixI);
    float scalar[maxBits];

    for (uint bit = 0; bit < maxBits; bit++)
    {
        scalar_i0[bit] = mix(scalar_i0j0[bit], scalar_i0j1[bit], mixJ);
        scalar_i1[bit] = mix(scalar_i1j0[bit], scalar_i1j1[bit], mixJ);

        scalar[bit] = mix(scalar_i0[bit], scalar_i1[bit], mixI);
    }

    return scalar;
}


// Auxiliary Pressure Levels
// =========================

float sampleAuxiliaryPressureColumnSingleBitAtP(in sampler3D sampler,
                                                in DataVolumeExtent dve,
                                                in sampler3D pressureSampler,
                                                in int i, in int j, in float p,
                                                uint bit)
{
    int kL = 0;
    int kU = dve.nLev - 1;

    // Binary search to find model levels kL, kU that enclose pressure level p.
    while (abs(kU - kL) > 1)
    {
        int kMid = (kL + kU) / 2;

        // pressure in Pa converted to hPa
        float pMid = texelFetch(pressureSampler, ivec3(i, j, kMid), 0).a / 100.0;

        if (p >= pMid) kL = kMid; else kU = kMid;
    }

    float pU = texelFetch(pressureSampler, ivec3(i, j, kU), 0).a / 100.0;
    float pL = texelFetch(pressureSampler, ivec3(i, j, kL), 0).a / 100.0;

    float lnPU = log(pU);
    float lnPL = log(pL);
    float lnP  = log(p);

    // compute interpolant
    float mixK = (lnP - lnPL) / (lnPU - lnPL);
    mixK = clamp(mixK, 0., 1.);

    // sample scalar values
    float scalarU = fetchBit(ivec3(i, j, kU), bit);
    float scalarL = fetchBit(ivec3(i, j, kL), bit);

//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if ((mixK < 0.) || (mixK > 1.)) return M_MISSING_VALUE;

    return mix(scalarL, scalarU, mixK);
}


// sample volume data at world position pos
float sampleAuxiliaryPressureVolumeSingleBitAtPos(in sampler3D sampler,
                                                  in DataVolumeExtent dve,
                                                  in sampler3D pressureSampler,
                                                  in vec3 pos,
                                                  in uint bit)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    const int i = int(mixI);
    const int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    int i1 = i + 1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    const int j1 = j + 1;

    const float scalar_i0j0 = sampleAuxiliaryPressureColumnSingleBitAtP(
                sampler, dve, pressureSampler, i , j , p, bit);
    const float scalar_i1j0 = sampleAuxiliaryPressureColumnSingleBitAtP(
                sampler, dve, pressureSampler, i1, j , p, bit);
    const float scalar_i0j1 = sampleAuxiliaryPressureColumnSingleBitAtP(
                sampler, dve, pressureSampler, i , j1, p, bit);
    const float scalar_i1j1 = sampleAuxiliaryPressureColumnSingleBitAtP(
                sampler, dve, pressureSampler, i1, j1, p, bit);

    // Check for missing values. Due to consistency of the acceleration
    // structures, this is done after all four columns have been retrieved.
//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if (scalar_i0j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i1j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i0j1 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i1j1 == M_MISSING_VALUE) return M_MISSING_VALUE;

    // Interpolate horizontally.
    mixJ = fract(mixJ);
    mixI = fract(mixI);

    const float scalar_i0 = mix(scalar_i0j0, scalar_i0j1, mixJ);
    const float scalar_i1 = mix(scalar_i1j0, scalar_i1j1, mixJ);

    return mix(scalar_i0, scalar_i1, mixI);
}


// sample model levels with auxiliary pressure volume data
//      indices = index coordinates within the volumes
//      p = pressure in hPa
float[maxBits] sampleAuxiliaryPressureColumnBitfieldAtP(
        in sampler3D sampler, in DataVolumeExtent dve,
        in sampler3D pressureSampler, in int i, in int j, in float p)
{
    int kL = 0;
    int kU = dve.nLev - 1;

    // Binary search to find model levels kL, kU that enclose pressure level p.
    while (abs(kU - kL) > 1)
    {
        int kMid = (kL + kU) / 2;

        // pressure in Pa converted to hPa
        float pMid = texelFetch(pressureSampler, ivec3(i, j, kMid), 0).a / 100.0;

        if (p >= pMid) kL = kMid; else kU = kMid;
    }

    float pU = texelFetch(pressureSampler, ivec3(i, j, kU), 0).a / 100.0;
    float pL = texelFetch(pressureSampler, ivec3(i, j, kL), 0).a / 100.0;

    float lnPU = log(pU);
    float lnPL = log(pL);
    float lnP  = log(p);

    // compute interpolant
    float mixK = (lnP - lnPL) / (lnPU - lnPL);
    mixK = clamp(mixK, 0., 1.);

    // sample scalar values
    float scalarU[maxBits] = fetchAllBits(ivec3(i, j, kU));
    float scalarL[maxBits] = fetchAllBits(ivec3(i, j, kL));

//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if ((mixK < 0.) || (mixK > 1.)) return M_MISSING_VALUE;
    float scalar[maxBits];

    // bit-wise interpolation
    for (uint bit = 0; bit < maxBits; bit++)
    {
        scalar[bit] = mix(scalarL[bit], scalarU[bit], mixK);
    }

    return scalar;

}


// sample volume data at world position pos
float[maxBits] sampleAuxiliaryPressureVolumeAllBitsAtPos(
        in sampler3D sampler, in DataVolumeExtent dve,
        in sampler3D pressureSampler, in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    const int i = int(mixI);
    const int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    int i1 = i + 1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    const int j1 = j + 1;

    const float scalar_i0j0[maxBits] = sampleAuxiliaryPressureColumnBitfieldAtP(
                sampler, dve, pressureSampler, i , j , p);
    const float scalar_i1j0[maxBits] = sampleAuxiliaryPressureColumnBitfieldAtP(
                sampler, dve, pressureSampler, i1, j , p);
    const float scalar_i0j1[maxBits] = sampleAuxiliaryPressureColumnBitfieldAtP(
                sampler, dve, pressureSampler, i , j1, p);
    const float scalar_i1j1[maxBits] = sampleAuxiliaryPressureColumnBitfieldAtP(
                sampler, dve, pressureSampler, i1, j1, p);

    // Check for missing values. Due to consistency of the acceleration
    // structures, this is done after all four columns have been retrieved.
//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if (scalar_i0j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i1j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i0j1 == M_MISSING_VALUE) return M_MISSING_VALUE;
//     if (scalar_i1j1 == M_MISSING_VALUE) return M_MISSING_VALUE;

    // Interpolate horizontally.
    mixJ = fract(mixJ);
    float scalar_i0[maxBits], scalar_i1[maxBits];
    mixI = fract(mixI);
    float scalar[maxBits];

    for (uint bit = 0; bit < maxBits; bit++)
    {
        scalar_i0[bit] = mix(scalar_i0j0[bit], scalar_i0j1[bit], mixJ);
        scalar_i1[bit] = mix(scalar_i1j0[bit], scalar_i1j1[bit], mixJ);

        scalar[bit] = mix(scalar_i0[bit], scalar_i1[bit], mixI);
    }

    return scalar;
}


// Pressure Levels
// ===============

// manual interpolation
float samplePressureLevelVolumeSingleBitAtPos(in sampler3D sampler,
                                              in DataVolumeExtent dve,
                                              in sampler1D pTableSampler,
                                              in vec3 pos,
                                              in uint bit)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    int i = int(mixI);
    int j = int(mixJ);

    int i1 = i+1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    int j1 = j+1;
    
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
    float scalar_i0j0k0 = fetchBit(ivec3(i , j , k  ), bit);
    float scalar_i0j0k1 = fetchBit(ivec3(i , j , k+1), bit);
    float scalar_i0j1k0 = fetchBit(ivec3(i , j1, k  ), bit);
    float scalar_i0j1k1 = fetchBit(ivec3(i , j1, k+1), bit);
    float scalar_i1j0k0 = fetchBit(ivec3(i1, j , k  ), bit);
    float scalar_i1j0k1 = fetchBit(ivec3(i1, j , k+1), bit);
    float scalar_i1j1k0 = fetchBit(ivec3(i1, j1, k  ), bit);
    float scalar_i1j1k1 = fetchBit(ivec3(i1, j1, k+1), bit);

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


// sample all bits of the pressure level volume's bitfield
float[maxBits] samplePressureLevelVolumeAllBitsAtPos(in sampler3D sampler,
                                                     in DataVolumeExtent dve,
                                                     in sampler1D pTableSampler,
                                                     in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    int i = int(mixI);
    int j = int(mixJ);

    int i1 = i+1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    int j1 = j+1;   
    
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
    const float scalar_i0j0k0[maxBits] = fetchAllBits(ivec3(i , j , k  ));
    const float scalar_i0j0k1[maxBits] = fetchAllBits(ivec3(i , j , k+1));
    const float scalar_i0j1k0[maxBits] = fetchAllBits(ivec3(i , j1, k  ));
    const float scalar_i0j1k1[maxBits] = fetchAllBits(ivec3(i , j1, k+1));
    const float scalar_i1j0k0[maxBits] = fetchAllBits(ivec3(i1, j , k  ));
    const float scalar_i1j0k1[maxBits] = fetchAllBits(ivec3(i1, j , k+1));
    const float scalar_i1j1k0[maxBits] = fetchAllBits(ivec3(i1, j1, k  ));
    const float scalar_i1j1k1[maxBits] = fetchAllBits(ivec3(i1, j1, k+1));

    float scalar_i0j0[maxBits], scalar_i0j1[maxBits];
    float scalar_i1j0[maxBits], scalar_i1j1[maxBits];

    mixJ = fract(mixJ);
    float scalar_i0[maxBits], scalar_i1[maxBits];
    mixI = fract(mixI);
    float scalar[maxBits];

    // interpolate the values bit-wise
    for (uint bit = 0; bit < maxBits; bit++)
    {
        // Interpolate vertically ...
        scalar_i0j0[bit] = mix(scalar_i0j0k0[bit], scalar_i0j0k1[bit], mixK);
        scalar_i0j1[bit] = mix(scalar_i0j1k0[bit], scalar_i0j1k1[bit], mixK);
        scalar_i1j0[bit] = mix(scalar_i1j0k0[bit], scalar_i1j0k1[bit], mixK);
        scalar_i1j1[bit] = mix(scalar_i1j1k0[bit], scalar_i1j1k1[bit], mixK);

        // ...and horizontally.
        scalar_i0[bit] = mix(scalar_i0j0[bit], scalar_i0j1[bit], mixJ);
        scalar_i1[bit] = mix(scalar_i1j0[bit], scalar_i1j1[bit], mixJ);

        scalar[bit] = mix(scalar_i0[bit], scalar_i1[bit], mixI);
    }

    return scalar;
}


// Subroutines of sampleDataAllBitsAtPos(...)
// ==========================================

subroutine(sampleDataAllBitsAtPosType)
float[maxBits] samplePressureVolumeAllBits(in vec3 pos)
{
    return samplePressureLevelVolumeAllBitsAtPos(dataVolume, dataExtent,
                                                 pressureTable, pos);
}


subroutine(sampleDataAllBitsAtPosType)
float[maxBits] sampleHybridVolumeAllBits(in vec3 pos)
{
    return sampleHybridSigmaVolumeAllBitsAtPos(
                dataVolume, dataExtent, surfacePressure, hybridCoefficients,
                pos);
}


subroutine(sampleDataAllBitsAtPosType)
float[maxBits] sampleAuxiliaryPressureVolumeAllBits(in vec3 pos)
{
    return sampleAuxiliaryPressureVolumeAllBitsAtPos(dataVolume, dataExtent,
                                                     auxPressureField3D_hPa, pos);
}


// Subroutines of sampleDataBitfieldAtPos(...)
// ===========================================

subroutine(sampleDataBitfieldAtPosType)
float samplePressureLevelVolumeBitfield(in vec3 pos, uint bit)
{
    return samplePressureLevelVolumeSingleBitAtPos(dataVolume, dataExtent,
                                                   pressureTable, pos, bit);
}


subroutine(sampleDataBitfieldAtPosType)
float sampleHybridSigmaVolumeBitfield(in vec3 pos, uint bit)
{
    return sampleHybridSigmaVolumeSingleBitAtPos(
                dataVolume, dataExtent, surfacePressure, hybridCoefficients,
                pos, bit);
}


subroutine(sampleDataBitfieldAtPosType)
float sampleAuxiliaryPressureVolumeBitfield(in vec3 pos, uint bit)
{
    return sampleAuxiliaryPressureVolumeSingleBitAtPos(
                dataVolume, dataExtent, auxPressureField3D_hPa, pos, bit);
}


// Bitfield gradient computation
// =============================

// subroutines for bitfield gradients of data types
subroutine(computeGradientBitfieldAtPosType)
vec3 pressureLevelGradientBitfield(vec3 pos, vec3 h, uint bit)
{
    vec3 gradient;

    vec3 pos_east = vec3(min(pos.x + h.x, dataExtent.dataSECrnr.x), pos.yz);
    vec3 pos_west = vec3(max(pos.x - h.x, dataExtent.dataNWCrnr.x), pos.yz);

    float x1 = samplePressureLevelVolumeSingleBitAtPos(dataVolume, dataExtent, pressureTable, pos_east, bit);
    float x2 = samplePressureLevelVolumeSingleBitAtPos(dataVolume, dataExtent, pressureTable, pos_west, bit);
    float hx = pos_east.x - pos_west.x;

    vec3 pos_north = vec3(pos.x, min(pos.y + h.y, dataExtent.dataNWCrnr.y), pos.z);
    vec3 pos_south = vec3(pos.x, max(pos.y - h.y, dataExtent.dataSECrnr.y), pos.z);

    float y1 = samplePressureLevelVolumeSingleBitAtPos(dataVolume, dataExtent, pressureTable, pos_north, bit);
    float y2 = samplePressureLevelVolumeSingleBitAtPos(dataVolume, dataExtent, pressureTable, pos_south, bit);
    float hy = pos_north.y - pos_south.y;

    // Find the pressue levels enclosing pos.z to estimate hz for the
    // vertical difference.
    // ==============================================================

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    // Binary search to find model levels k, k1 that enclose pressure level p.
    int k = 0;
    int k1 = dataExtent.nLev - 1;
    int vertOffset = dataExtent.nLon + dataExtent.nLat;

    while (abs(k1 - k) > 1)
    {
        int kMid = (k + k1) / 2;
        float pMid = texelFetch(lonLatLevAxes, kMid + vertOffset, 0).a;
        if (p >= pMid) k = kMid; else k1 = kMid;
    }

    float lnPk  = log(texelFetch(lonLatLevAxes, k + vertOffset, 0).a);
    float lnPk1 = log(texelFetch(lonLatLevAxes, k1 + vertOffset, 0).a);

    float hz1 = (lnPk - pToWorldZParams.x) * pToWorldZParams.y;
    float hz2 = (lnPk1 - pToWorldZParams.x) * pToWorldZParams.y;
    float hz = abs(hz1 - hz2);

    vec3 pos_top = vec3(pos.xy, min(pos.z + hz, dataExtent.dataNWCrnr.z));
    vec3 pos_bot = vec3(pos.xy, max(pos.z - hz, dataExtent.dataSECrnr.z));

    float z1 = samplePressureLevelVolumeSingleBitAtPos(dataVolume, dataExtent, pressureTable, pos_top, bit);
    float z2 = samplePressureLevelVolumeSingleBitAtPos(dataVolume, dataExtent, pressureTable, pos_bot, bit);
    hz = pos_top.z - pos_bot.z;

    gradient = vec3((x1 - x2) / abs(hx), (y1 - y2) / abs(hy), (z1 - z2) / abs(hz));

    return normalize(gradient);
}


// subroutines for bitfield gradients of data types
subroutine(computeGradientBitfieldAtPosType)
vec3 hybridLevelGradientBitfield(vec3 pos, vec3 h, uint bit)
{
    vec3 gradient;

    // 2. Sample with horizontal displacement, using clampToDataBoundary.
    vec3 pos_east = vec3(min(pos.x + h.x, dataExtent.dataSECrnr.x), pos.yz);
    vec3 pos_west = vec3(max(pos.x - h.x, dataExtent.dataNWCrnr.x), pos.yz);

    float x1 = sampleHybridSigmaVolumeSingleBitAtPos(dataVolume, dataExtent,
                                                  surfacePressure, hybridCoefficients,
                                                  pos_east, bit);
    float x2 = sampleHybridSigmaVolumeSingleBitAtPos(dataVolume, dataExtent,
                                                  surfacePressure, hybridCoefficients,
                                                  pos_west, bit);
    float hx = pos_east.x - pos_west.x;

    vec3 pos_north = vec3(pos.x, min(pos.y + h.y, dataExtent.dataNWCrnr.y), pos.z);
    vec3 pos_south = vec3(pos.x, max(pos.y - h.y, dataExtent.dataSECrnr.y), pos.z);
//FIXME: Why do the accelerated sampling methods return other values here?
//       (mr, 04Aug2014) ?
     float y1 = sampleHybridSigmaVolumeSingleBitAtPos(dataVolume, dataExtent,
                                                      surfacePressure, hybridCoefficients,
                                                      pos_north, bit);
     float y2 = sampleHybridSigmaVolumeSingleBitAtPos(dataVolume, dataExtent,
                                                      surfacePressure, hybridCoefficients,
                                                      pos_south, bit);
    //float y1 = sampleDataAtPos(pos_north);
    //float y2 = sampleDataAtPos(pos_south);
    float hy = pos_north.y - pos_south.y;

    // 3. Sample with vertical displacement, considering data volume
    // boundaries.

    float zbot, ztop;
    getHybridSigmaBotTopLevelAtPos(dataExtent, surfacePressure,
                                   hybridCoefficients, pos, zbot, ztop);

    float hz = getHybridApproxWorldZLevelDistanceAtPos(dataExtent, surfacePressure,
                                                       hybridCoefficients, pos);

    vec3 pos_top = vec3(pos.xy, min(pos.z + hz, ztop));
    vec3 pos_bot = vec3(pos.xy, max(pos.z - hz, zbot));

    float z1 = sampleHybridSigmaVolumeSingleBitAtPos(dataVolume, dataExtent,
                                                  surfacePressure, hybridCoefficients,
                                                  pos_top, bit);
    float z2 = sampleHybridSigmaVolumeSingleBitAtPos(dataVolume, dataExtent,
                                                  surfacePressure, hybridCoefficients,
                                                  pos_bot, bit);
    hz = pos_top.z - pos_bot.z;

    gradient = vec3((x1 - x2) / abs(hx), (y1 - y2) / abs(hy),
                    (z1 - z2) / abs(hz));

    return normalize(gradient);
}


// subroutines for bitfield gradients of data types
subroutine(computeGradientBitfieldAtPosType)
vec3 auxiliaryPressureGradientBitfield(vec3 pos, vec3 h, uint bit)
{
    vec3 gradient;

    // 2. Sample with horizontal displacement, using clampToDataBoundary.
    vec3 pos_east = vec3(min(pos.x + h.x, dataExtent.dataSECrnr.x), pos.yz);
    vec3 pos_west = vec3(max(pos.x - h.x, dataExtent.dataNWCrnr.x), pos.yz);

    float x1 = sampleAuxiliaryPressureVolumeSingleBitAtPos(
                dataVolume, dataExtent, auxPressureField3D_hPa, pos_east, bit);
    float x2 = sampleAuxiliaryPressureVolumeSingleBitAtPos(
                dataVolume, dataExtent, auxPressureField3D_hPa, pos_west, bit);
    float hx = pos_east.x - pos_west.x;

    vec3 pos_north = vec3(pos.x, min(pos.y + h.y, dataExtent.dataNWCrnr.y),
                          pos.z);
    vec3 pos_south = vec3(pos.x, max(pos.y - h.y, dataExtent.dataSECrnr.y),
                          pos.z);
//FIXME: Why do the accelerated sampling methods return other values here?
//       (mr, 04Aug2014) ?
     float y1 = sampleAuxiliaryPressureVolumeSingleBitAtPos(
                 dataVolume, dataExtent, auxPressureField3D_hPa, pos_north, bit);
     float y2 = sampleAuxiliaryPressureVolumeSingleBitAtPos(
                 dataVolume, dataExtent, auxPressureField3D_hPa, pos_south, bit);
    //float y1 = sampleDataAtPos(pos_north);
    //float y2 = sampleDataAtPos(pos_south);
    float hy = pos_north.y - pos_south.y;

    // 3. Sample with vertical displacement, considering data volume
    // boundaries.

    float zbot, ztop;
    getAuxiliaryPressureBotTopLevelAtPos(dataExtent, auxPressureField3D_hPa, pos,
                                         zbot, ztop);

    float hz = getAuxiliaryPressureApproxWorldZLevelDistanceAtPos(
                dataExtent, auxPressureField3D_hPa, pos);

    vec3 pos_top = vec3(pos.xy, min(pos.z + hz, ztop));
    vec3 pos_bot = vec3(pos.xy, max(pos.z - hz, zbot));

    float z1 = sampleAuxiliaryPressureVolumeSingleBitAtPos(
                dataVolume, dataExtent, auxPressureField3D_hPa, pos_top, bit);
    float z2 = sampleAuxiliaryPressureVolumeSingleBitAtPos(
                dataVolume, dataExtent, auxPressureField3D_hPa, pos_bot, bit);
    hz = pos_top.z - pos_bot.z;

    gradient = vec3((x1 - x2) / abs(hx), (y1 - y2) / abs(hy),
                    (z1 - z2) / abs(hz));

    return normalize(gradient);
}


// Bitfield sampling utilities
// ===========================

int[maxBits] computeAllCrossingLevels(in float scalar[maxBits])
{
    int crossingLevels[maxBits];
    for (uint bit = 0; bit < maxBits; bit++)
    {
        crossingLevels[bit] = computeCrossingLevel(scalar[bit]);
    }

    return crossingLevels;
}


void bisectionCorrectionBitfield(
        inout vec3 rayPosition, inout float lambda,
        in vec3 prevRayPosition, in float prevLambda,
        inout int crossingLevelFront, inout int crossingLevelBack, uint bit)
{
    vec3 rayCenterPosition;
    float centerLambda;
    int crossingLevelCenter;

    for (int i = 0; i < bisectionSteps; ++i)
    {
        rayCenterPosition = (rayPosition + prevRayPosition) / 2.0;
        centerLambda = (lambda + prevLambda) / 2.0;

        const float scalar = sampleDataBitfieldAtPos(rayCenterPosition, bit);

        crossingLevelCenter = computeCrossingLevel(scalar);

        if (crossingLevelCenter != crossingLevelBack)
        {
            rayPosition = rayCenterPosition;
            lambda = centerLambda;
            crossingLevelFront = crossingLevelCenter;
        }
        else
        {
            prevRayPosition = rayCenterPosition;
            prevLambda = centerLambda;
            crossingLevelBack = crossingLevelCenter;
        }
    }
}

