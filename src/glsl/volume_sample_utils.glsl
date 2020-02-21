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

// global extents of volume data corresponding to the variable and shading variable
uniform DataVolumeExtent dataExtent;


/******************************************************************************
***                               SUBROUTINES                               ***
*******************************************************************************/

// sample any data type at world position
subroutine float sampleDataAtPosType(vec3 pos);
subroutine vec3 computeGradientType(vec3 pos, in vec3 h);

// define subroutine uniform names
subroutine uniform sampleDataAtPosType      sampleDataAtPos;
subroutine uniform computeGradientType      computeGradient;


/******************************************************************************
***                            SAMPLE SUBROUTINES                           ***
*******************************************************************************/

// sample any kind of data at given world position
subroutine(sampleDataAtPosType)
float samplePressureLevel(in vec3 pos)
{
    return samplePressureLevelVolumeAtPos(dataVolume, dataExtent,
                                          pressureTable, pos);
}


#ifdef ENABLE_HYBRID_PRESSURETEXCOORDTABLE
subroutine(sampleDataAtPosType)
float sampleHybridLevel(in vec3 pos)
{
    return sampleHybridSigmaVolumeAtPos_LUT(dataVolume, dataExtent,
                                            surfacePressure, hybridCoefficients,
                                            pressureTexCoordTable2D, pos);
}
#else
subroutine(sampleDataAtPosType)
float sampleHybridLevel(in vec3 pos)
{
    return sampleHybridSigmaVolumeAtPos(dataVolume, dataExtent,
                                        surfacePressure, hybridCoefficients,
                                        pos);
}
#endif


subroutine(sampleDataAtPosType)
float sampleAuxiliaryPressure(in vec3 pos)
{
    return sampleAuxiliaryPressureVolumeAtPos(dataVolume, dataExtent,
                                              auxPressureField3D_hPa, pos);
}


/******************************************************************************
***                            GRADIENT SUBROUTINES                         ***
*******************************************************************************/

/**
  Computes the gradient at position @p pos, using sampling step size @p h.

  Central differences are used for the computation, except for positions close
  to the boundaries of the data volume. Here, h is scaled and reduced to
  simple forward differences for positions less than one grid size away from
  the boundary. Also, correction is performed for missing values.

  For pressure level and hybrid sigma pressure level grids, the vertical
  difference hz used to compute the central/forward differences are scaled
  according to the vertical level distance (which will or can vary with height).
 */

vec3 gradientAtPos(in vec3 pos, in vec3 h, float ztop, float zbot)
{
    vec3 gradient;

    // 1. Sample with horizontal displacement. Check if the grid is cyclic in
    // longitude.

    vec3 pos_east;
    vec3 pos_west;
    if (dataExtent.gridIsCyclicInLongitude)
    {
        pos_east = vec3(pos.x + h.x, pos.yz);
        pos_west = vec3(pos.x - h.x, pos.yz);
    }
    else
    {
        // Non-cyclic grids: clamp to data boundary.
        pos_east = vec3(min(pos.x + h.x, dataExtent.dataSECrnr.x), pos.yz);
        pos_west = vec3(max(pos.x - h.x, dataExtent.dataNWCrnr.x), pos.yz);
    }

    float x1 = sampleDataAtPos(pos_east);
    float x2 = sampleDataAtPos(pos_west);
    if (x1 == M_MISSING_VALUE)
    {
        // Missing value correction: If the grid sampled at position pos.x + h.x
        // is missing, "fall back" to position pos.x. The gradient reduces to
        // a simple forward difference.
        pos_east = pos;
        x1 = sampleDataAtPos(pos_east);
    }
    if (x2 == M_MISSING_VALUE)
    {
//CHECKME (mr, 21Feb2020) -- in rare cases both sampled values could be
// missing. In such cases the gradient would be zero and we'd have a division
// by zero..
        pos_west = pos;
        x2 = sampleDataAtPos(pos_west);
    }
    float hx = pos_east.x - pos_west.x;

    vec3 pos_north = vec3(pos.x, min(pos.y + h.y, dataExtent.dataNWCrnr.y), pos.z);
    vec3 pos_south = vec3(pos.x, max(pos.y - h.y, dataExtent.dataSECrnr.y), pos.z);
    float y1 = sampleDataAtPos(pos_north);
    float y2 = sampleDataAtPos(pos_south);
    if (y1 == M_MISSING_VALUE)
    {
        pos_north = pos;
        y1 = sampleDataAtPos(pos_north);
    }
    if (y2 == M_MISSING_VALUE)
    {
        pos_south = pos;
        y2 = sampleDataAtPos(pos_south);
    }
    float hy = pos_north.y - pos_south.y;

    // 2. Sample with vertical displacement, considering data volume
    // boundaries.

    vec3 pos_top = vec3(pos.xy, min(pos.z + h.z, ztop));
    vec3 pos_bot = vec3(pos.xy, max(pos.z - h.z, zbot));
    float z1 = sampleDataAtPos(pos_top);
    float z2 = sampleDataAtPos(pos_bot);
    if (z1 == M_MISSING_VALUE)
    {
        pos_top = pos;
        z1= sampleDataAtPos(pos_top);
    }
    if (z2 == M_MISSING_VALUE)
    {
        pos_bot = pos;
        z2= sampleDataAtPos(pos_bot);
    }
    float hz = pos_top.z - pos_bot.z;

    // 3. Compute gradient.
    
    gradient = vec3((x1 - x2) / abs(hx), (y1 - y2) / abs(hy), (z1 - z2) / abs(hz));

    return normalize(gradient);
}
 
 
subroutine(computeGradientType)
vec3 pressureLevelGradient(in vec3 pos, in vec3 h)
{
    // Find the pressue levels enclosing pos.z to estimate hz for the
    // vertical difference.

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
    vec3 h_ = vec3(h.x, h.y, abs(hz1 - hz2));

    float ztop = dataExtent.dataNWCrnr.z;
    float zbot = dataExtent.dataSECrnr.z;

    return gradientAtPos(pos, h_, ztop, zbot);
}


subroutine(computeGradientType)
vec3 hybridLevelGradient(in vec3 pos, in vec3 h)
{
    // Determine the approximate distance between the model levels above and
    // below the current position ("hz").
    float zbot, ztop;
    getHybridSigmaBotTopLevelAtPos(dataExtent, surfacePressure,
                                   hybridCoefficients, pos, zbot, ztop);

    float hz = getHybridApproxWorldZLevelDistanceAtPos(dataExtent, surfacePressure,
                                                       hybridCoefficients, pos);
    vec3 h_ = vec3(h.x, h.y, hz);
                                                       
    return gradientAtPos(pos, h_, ztop, zbot);
}


subroutine(computeGradientType)
vec3 auxiliaryPressureGradient(in vec3 pos, in vec3 h)
{
    // Determine the approximate distance between the model levels above and
    // below the current position ("hz").
    float zbot, ztop;
    getAuxiliaryPressureBotTopLevelAtPos(dataExtent, auxPressureField3D_hPa,
                                         pos, zbot, ztop);

    float hz = getAuxiliaryPressureApproxWorldZLevelDistanceAtPos(
                dataExtent, auxPressureField3D_hPa, pos);
    vec3 h_ = vec3(h.x, h.y, hz);

    return gradientAtPos(pos, h_, ztop, zbot);
}


/******************************************************************************
***                          RAYCASTING SAMPLE UTILS                        ***
*******************************************************************************/

/**
  Computes the intersection points of a ray with a box. Returns tnear and
  tfar (packed in a vec2), which are the entry and exit parameters of the
  ray into the box when a position pos on the ray is described by
        pos = ray.origin + t * ray.direction

  boxCrnr1 and boxCrnr2 are two opposite corners of the box.

  Literature: Williams et al. (2005), "An efficient and robust ray-box
  intersection algorithm." (notes 29/03/2012).
  */
bool rayBoxIntersection(
        in Ray ray, in vec3 boxCrnr1, in vec3 boxCrnr2, out vec2 tNearFar)
{
    float tnear = 0.;
    float tfar  = 0.;

    vec3 rayDirInv = vec3(1./ray.direction.x, 1./ray.direction.y,
                          1./ray.direction.z);
    if(rayDirInv.x >= 0.0)
    {
        tnear = (boxCrnr1.x - ray.origin.x) * rayDirInv.x;
        tfar  = (boxCrnr2.x - ray.origin.x) * rayDirInv.x;
    }
    else
    {
        tnear = (boxCrnr2.x - ray.origin.x) * rayDirInv.x;
        tfar  = (boxCrnr1.x - ray.origin.x) * rayDirInv.x;
    }

    if(rayDirInv.y >= 0.0)
    {
        tnear = max(tnear, (boxCrnr1.y - ray.origin.y) * rayDirInv.y);
        tfar  = min(tfar,  (boxCrnr2.y - ray.origin.y) * rayDirInv.y);
    }
    else
    {
        tnear = max(tnear, (boxCrnr2.y - ray.origin.y) * rayDirInv.y);
        tfar  = min(tfar,  (boxCrnr1.y - ray.origin.y) * rayDirInv.y);
    }

    if(rayDirInv.z >= 0.0)
    {
        tnear = max(tnear, (boxCrnr1.z - ray.origin.z) * rayDirInv.z);
        tfar  = min(tfar,  (boxCrnr2.z - ray.origin.z) * rayDirInv.z);
    }
    else
    {
        tnear = max(tnear, (boxCrnr2.z - ray.origin.z) * rayDirInv.z);
        tfar  = min(tfar,  (boxCrnr1.z - ray.origin.z) * rayDirInv.z);
    }

    tNearFar = vec2(tnear, tfar);
    return (tnear < tfar);
}


bool insideBox(
        in Ray ray, in float t, in vec3 boxCrnr1, in vec3 boxCrnr2)
{
    vec3 rayPosition = ray.origin + t * ray.direction;
    if (rayPosition.x < min(boxCrnr1.x, boxCrnr2.x)) return false;
    if (rayPosition.x > max(boxCrnr1.x, boxCrnr2.x)) return false;
    if (rayPosition.y < min(boxCrnr1.y, boxCrnr2.y)) return false;
    if (rayPosition.y > max(boxCrnr1.y, boxCrnr2.y)) return false;
    if (rayPosition.z < min(boxCrnr1.z, boxCrnr2.z)) return false;
    if (rayPosition.z > max(boxCrnr1.z, boxCrnr2.z)) return false;
    return true;
}


int computeCrossingLevel(in float scalar)
{
    int level = 0;

    // Isovalues are sorted in MLonLatMultiPurposeVolumeActor
    // ::RayCasterSettings::sortIsoValues() according to increasing value.
    // Disabled values are put to the back of the array and the number of
    // enabled values is passed to the shader. Hence we do not need to test
    // for an isovalue to be enabled here; all values with indices 
    // < numIsoValues are enabled and valid.
    
    for (int i = 0; i < numIsoValues; i++)
        level += int(scalar >= isoValues[i]);

    return level;
}


void bisectionCorrection(inout vec3 rayPosition, inout float lambda,
                         in vec3 prevRayPosition, in float prevLambda,
                         inout int crossingLevelFront, inout int crossingLevelBack)
{
    vec3 rayCenterPosition;
    float centerLambda;
    int crossingLevelCenter;

    for (int i = 0; i < bisectionSteps; ++i)
    {
        rayCenterPosition = (rayPosition + prevRayPosition) / 2.0;
        centerLambda = (lambda + prevLambda) / 2.0;

        const float scalar = sampleDataAtPos(rayCenterPosition);

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


void bisectionCorrectionDepth(inout vec3 rayPosition,
                              in vec3 prevRayPosition,
                              in float depthValue)
{
    vec3 rayCenterPosition;

    for (int i = 0; i < 5; ++i)
    {
        rayCenterPosition = (rayPosition + prevRayPosition) / 2.0;

        vec4 clipSpacePos = mvpMatrix * vec4(rayCenterPosition,1);
        clipSpacePos.z /= clipSpacePos.w;
        float depth = clipSpacePos.z * 0.5 + 0.5;

        if (depth > depthValue) {
            rayPosition = rayCenterPosition;
        } else {
            prevRayPosition = rayCenterPosition;
        }
    }
}


// depth computation
float computeDepthFromWorld(in vec3 rayPosition)
{
    // first convert position to clip space
    vec4 clipSpacePos = mvpMatrix * vec4(rayPosition,1);
    // perform perspective division
    clipSpacePos.z /= clipSpacePos.w;
    // -> z = OPENGL ? [-1;1] : [0;1]
    // calculate depth value -> convert to [0,1]
    return clipSpacePos.z * 0.5 + 0.5;
}


vec3 initGradientSamplingStep(in DataVolumeExtent dve)
{
    // dve.deltaLnP is not used... (instead computed in gradient methods).
    return vec3(dve.deltaLon, dve.deltaLat, dve.deltaLnP);
}

