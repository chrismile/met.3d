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

// Defines sampling routines for a data volume of type HYBRID_SIGMA_PRESSURE_3D.


/******************************************************************************
***                             SAMPLE METHODS                              ***
*******************************************************************************/

// sample hybrid sigma pressure volume data
//      indices = index coordinates within the volumes
//      p = pressure in hPa
float sampleHybridSigmaColumnAtP(in sampler3D sampler, in DataVolumeExtent dve,
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
    float scalarU = texelFetch(sampler, ivec3(i, j, kU), 0).a;
    float scalarL = texelFetch(sampler, ivec3(i, j, kL), 0).a;

//TODO: Why is this not working with computeGradient()? (mr, 03Aug2014)
//     if ((mixK < 0.) || (mixK > 1.)) return M_MISSING_VALUE;
    return mix(scalarL, scalarU, mixK);
}


// sample volume data at world position pos
float sampleHybridSigmaVolumeAtPos(in sampler3D sampler,
                                   in DataVolumeExtent dve,
                                   in sampler2D sfcSampler,
                                   in sampler1D hybCoeffSampler,
                                   in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    int i = int(mixI);
    int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    int i1 = i+1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    int j1 = j+1;

    float scalar_i0j0 = sampleHybridSigmaColumnAtP(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i , j , p);
    float scalar_i1j0 = sampleHybridSigmaColumnAtP(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i1, j , p);
    float scalar_i0j1 = sampleHybridSigmaColumnAtP(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i , j1, p);
    float scalar_i1j1 = sampleHybridSigmaColumnAtP(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i1, j1, p);

    // Check for missing values. If one of the four surrounding grid points
    // is missing, the interpolated value is undefined.
    if (scalar_i0j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
    if (scalar_i1j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
    if (scalar_i0j1 == M_MISSING_VALUE) return M_MISSING_VALUE;
    if (scalar_i1j1 == M_MISSING_VALUE) return M_MISSING_VALUE;

    // Interpolate horizontally.
    mixJ = fract(mixJ);
    float scalar_i0 = mix(scalar_i0j0, scalar_i0j1, mixJ);
    float scalar_i1 = mix(scalar_i1j0, scalar_i1j1, mixJ);

    mixI = fract(mixI);
    float scalar = mix(scalar_i0, scalar_i1, mixI);

    return scalar;
}


float sampleHybridSigmaColumnAtP_maxNeighbour(
                                 in sampler3D sampler, in DataVolumeExtent dve,
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

    // sample scalar values
    float scalarU = texelFetch(sampler, ivec3(i, j, kU), 0).a;
    float scalarL = texelFetch(sampler, ivec3(i, j, kL), 0).a;

    return max(scalarL, scalarU);
}


// sample volume data at world position pos
float sampleHybridSigmaVolumeAtPos_maxNeighbour(
                                   in sampler3D sampler,
                                   in DataVolumeExtent dve,
                                   in sampler2D sfcSampler,
                                   in sampler1D hybCoeffSampler,
                                   in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    int i = int(mixI);
    int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

    int i1 = i+1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    int j1 = j+1;

    float scalar_i0j0 = sampleHybridSigmaColumnAtP_maxNeighbour(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i , j , p);
    float scalar_i1j0 = sampleHybridSigmaColumnAtP_maxNeighbour(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i1, j , p);
    float scalar_i0j1 = sampleHybridSigmaColumnAtP_maxNeighbour(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i , j1, p);
    float scalar_i1j1 = sampleHybridSigmaColumnAtP_maxNeighbour(sampler, dve, sfcSampler,
                                                   hybCoeffSampler, i1, j1, p);

    // Interpolate horizontally.
    float scalar_i0 = max(scalar_i0j0, scalar_i0j1);
    float scalar_i1 = max(scalar_i1j0, scalar_i1j1);

    float scalar = max(scalar_i0, scalar_i1);

    return scalar;
}


// Find the highest bottom level and the lowest top level in the four
// grid columns surrounding the given position.
void getHybridSigmaBotTopLevelAtPos(in DataVolumeExtent dve,
                                    in sampler2D sfcSampler,
                                    in sampler1D hybCoeffSampler,
                                    in vec3 pos,
                                    out float zbot, out float ztop)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    int i = int(mixI);
    int j = int(mixJ);
    int i1 = i+1;
    if (dve.gridIsCyclicInLongitude) i1 %= dve.nLon;
    int j1 = j+1;

    // Hybrid coefficients of highest and lowest level.
    float atop = texelFetch(hybCoeffSampler, 0, 0).a;
    float btop = texelFetch(hybCoeffSampler, 0 + dve.nLev, 0).a;
    float abot = texelFetch(hybCoeffSampler, dve.nLev-1, 0).a;
    float bbot = texelFetch(hybCoeffSampler, dve.nLev-1 + dve.nLev, 0).a;

    // pressure in Pa converted to hPa
    float psfc_i0j0 = texelFetch(sfcSampler, ivec2(i ,j ), 0).a / 100.0;
    float psfc_i0j1 = texelFetch(sfcSampler, ivec2(i ,j1), 0).a / 100.0;
    float psfc_i1j0 = texelFetch(sfcSampler, ivec2(i1,j ), 0).a / 100.0;
    float psfc_i1j1 = texelFetch(sfcSampler, ivec2(i1,j1), 0).a / 100.0;

    float pbot_i0j0 = abot + bbot * psfc_i0j0;
    float pbot_i0j1 = abot + bbot * psfc_i0j1;
    float pbot_i1j0 = abot + bbot * psfc_i1j0;
    float pbot_i1j1 = abot + bbot * psfc_i1j1;

    // Find the highest bottom level, i.e. the one with the lowest pressure.
    float pbot_i0 = min(pbot_i0j0, pbot_i0j1);
    float pbot_i1 = min(pbot_i1j0, pbot_i1j1);
    float pbot = min(pbot_i0, pbot_i1);

    zbot = (log(pbot) - pToWorldZParams.x) * pToWorldZParams.y;

    float ptop_i0j0 = atop + btop * psfc_i0j0;
    float ptop_i0j1 = atop + btop * psfc_i0j1;
    float ptop_i1j0 = atop + btop * psfc_i1j0;
    float ptop_i1j1 = atop + btop * psfc_i1j1;

    // Lowest top level, i.e. highest pressure.
    float ptop_i0 = max(ptop_i0j0, ptop_i0j1);
    float ptop_i1 = max(ptop_i1j0, ptop_i1j1);
    float ptop = max(ptop_i0, ptop_i1);

    ztop = (log(ptop) - pToWorldZParams.x) * pToWorldZParams.y;
}


// Determine the distance (in worldZ coordinates) between the two hybrid levels
// that enclose the sample point at position "pos". The grid column at i,j
// coordinates (west/north corner of the horizontal grid box) is used.
float getHybridApproxWorldZLevelDistanceAtPos(in DataVolumeExtent dve,
                                              in sampler2D sfcSampler,
                                              in sampler1D hybCoeffSampler,
                                              in vec3 pos)
{
    float mixI = mod(pos.x - dve.dataNWCrnr.x, 360.) / dve.deltaLon;
    float mixJ = (dve.dataNWCrnr.y - pos.y) / dve.deltaLat;
    int i = int(mixI);
    int j = int(mixJ);

    // Compute pressure from world z coordinate.
    float p = exp(pos.z / pToWorldZParams.y + pToWorldZParams.x);

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

    float hzU = (log(pU) - pToWorldZParams.x) * pToWorldZParams.y;
    float hzL = (log(pL) - pToWorldZParams.x) * pToWorldZParams.y;
    float hz = abs(hzU - hzL);

    return hz;
}


#ifdef ENABLE_HYBRID_PRESSURETEXCOORDTABLE

float sampleHybridSigmaColumnAtP_LUT(in sampler3D sampler, in DataVolumeExtent dve,
                                     in sampler2D sfcSampler,
                                     in sampler1D hybCoeffSampler,
                                     in sampler2D presTexCoordSampler,
                                     in int i, in int j, in float ln_p)
{
    // Compute the texture coordinate for sampling the column at grid indices
    // i and j at (log) pressue ln_p: First map i and j to texture coordinates
    // (0..1).
    vec3 texCoords = vec3((2.*i + 1.) / (2. * dve.nLon),
                          (2.*j + 1.) / (2. * dve.nLat),
                          0.);

    // The vertical texture coordinate depends on surface pressure. It is
    // interpolated with the pre-defined mapping in the lookup table referenced
    // by presTexCoordSampler. See MLonLatHybridSigmaPressureGrid::
    // getPressureTexCoordTexture2D().

    // Surface pressure in Pa converted to hPa
    float psfc_hPa = texelFetch(sfcSampler, ivec2(i,j), 0).a / 100.0;

    // The first element in the LUT stores the upper boundary of the grid
    // column for this psfc, required to scale ln_p to 0..1. We use texture()
    // instead of texelFetch() to interpolate between the discrete psfc stored
    // in the LUT. Texture coordinate for first element is 1.(2.*nPresTexCoord),
    // with nPresTexCoord being the number of vertical entries in the table.

    // NOTE: The range of values for surface pressure (1050..450 in 0.5 hPa
    // steps) is HARD-CODED in MLonLatHybridSigmaPressureGrid::
    // getPressureTexCoordTexture2D(). Make sure these value correspond to
    // each other.

    int nPresTexCoord = textureSize(presTexCoordSampler, 0).y;
    vec2 texCoordsLUT = vec2(
                (1050.5 - psfc_hPa) / 601., // map psfc to 0..1
                1./(2.*nPresTexCoord));
    float ln_ptop_table = texture(presTexCoordSampler, texCoordsLUT).a;

    // The second element stores the vertical distance log(ptop)-log(pbot) of
    // the column in the table.
    texCoordsLUT.t = 3./(2.*nPresTexCoord);
    float ln_p_extent = texture(presTexCoordSampler, texCoordsLUT).a;

    // With ln_ptop_table and ln_p_extent, we can map ln_p to its LUT texture
    // coordinate. This ranges from 2./nPresTexCoord (excluding the two values
    // just retrieved) to 1.
    texCoordsLUT.t = 2./nPresTexCoord + (ln_p - ln_ptop_table) / ln_p_extent;

    // Alternative: Compute here. Doesn't seem to have a large implact
    // on performance (mr, 02Nov2014).
//     float ak_bot   = texelFetch(hybCoeffSampler, dve.nLev-1, 0).a;
//     float bk_bot   = texelFetch(hybCoeffSampler, dve.nLev-1 + dve.nLev, 0).a;
//     float pbot_hPa = ak_bot + bk_bot * psfc_hPa;
//     float ln_pbot  = log(pbot_hPa);
//
//     float ak_top   = texelFetch(hybCoeffSampler, 0, 0).a;
//     float bk_top   = texelFetch(hybCoeffSampler, 0 + dve.nLev, 0).a;
//     float ptop_hPa = ak_top + bk_top * psfc_hPa;
//     float ln_ptop  = log(ptop_hPa);
//
//     // compute upper table boundary for current psfc
//     // compute vertical table extent (via lower p)
//     int nPresTexCoord = textureSize(presTexCoordSampler, 0).y;
//     float delta_lnp = abs(ln_ptop - ln_pbot) / (nPresTexCoord-1);
//     float ln_ptop_table = ln_ptop - delta_lnp/2.;
//     float ln_p_extent = abs(ln_ptop - ln_pbot) + delta_lnp;

//     vec2 texCoordsLUT = vec2(
//                 (1050.5 - psfc_hPa) / 601., // map psfc to 0..1
//                 (ln_p - ln_ptop_table) / ln_p_extent); // map ln_p tp 0..1

    // Fetch vertical texture coordinate for data volume from
    // pressure-to-texture-coordinate-LUT.
    // NOTE: If the vertical coordinate is outside the data volume
    // (then texCoordsLUT.t is either < 2./nPresTexCoord or > 1.) a missing
    // value shall be returned.
    if ((texCoordsLUT.t >= 2./nPresTexCoord) && (texCoordsLUT.t <= 1.))
    {
        texCoords.p = clamp(texture(presTexCoordSampler, texCoordsLUT).a, 0., 1.);
    }
    else
    {
        return M_MISSING_VALUE;
    }

    return texture(sampler, texCoords).a;
}


// sample volume data at world position pos
float sampleHybridSigmaVolumeAtPos_LUT(in sampler3D sampler,
                                       in DataVolumeExtent dve,
                                       in sampler2D sfcSampler,
                                       in sampler1D hybCoeffSampler,
                                       in sampler2D presTexCoordSampler,
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
    float ln_p = log(exp(pos.z / pToWorldZParams.y + pToWorldZParams.x));

    float scalar_i0j0 = sampleHybridSigmaColumnAtP_LUT(sampler, dve, sfcSampler,
                                                       hybCoeffSampler,
                                                       presTexCoordSampler,
                                                       i , j , ln_p);
    float scalar_i1j0 = sampleHybridSigmaColumnAtP_LUT(sampler, dve, sfcSampler,
                                                       hybCoeffSampler,
                                                       presTexCoordSampler,
                                                       i1, j , ln_p);
    float scalar_i0j1 = sampleHybridSigmaColumnAtP_LUT(sampler, dve, sfcSampler,
                                                       hybCoeffSampler,
                                                       presTexCoordSampler,
                                                       i , j1, ln_p);
    float scalar_i1j1 = sampleHybridSigmaColumnAtP_LUT(sampler, dve, sfcSampler,
                                                       hybCoeffSampler,
                                                       presTexCoordSampler,
                                                       i1, j1, ln_p);

    // Check for missing values. Due to consistency of the acceleration
    // structures, this is done after all four columns have been retrieved.
    if (scalar_i0j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
    if (scalar_i1j0 == M_MISSING_VALUE) return M_MISSING_VALUE;
    if (scalar_i0j1 == M_MISSING_VALUE) return M_MISSING_VALUE;
    if (scalar_i1j1 == M_MISSING_VALUE) return M_MISSING_VALUE;

    // Interpolate horizontally.
    mixJ = fract(mixJ);
    float scalar_i0 = mix(scalar_i0j0, scalar_i0j1, mixJ);
    float scalar_i1 = mix(scalar_i1j0, scalar_i1j1, mixJ);

    mixI = fract(mixI);
    float scalar = mix(scalar_i0, scalar_i1, mixI);

    return scalar;
}

#endif // ENABLE_HYBRID_PRESSURETEXCOORDTABLE


