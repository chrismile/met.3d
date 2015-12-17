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

// global extents of volume data corresponding to the variable and shading variable
uniform DataVolumeExtent dataExtent;


/******************************************************************************
***                               SUBROUTINES                               ***
*******************************************************************************/

// sample any data type at world position
subroutine float sampleDataAtPosType(vec3 pos);
#ifdef ENABLE_HYBRID_NEIGHBOURHOOD_ACCELERATION
subroutine float sampleDataAtPosAccelType(vec3 pos, inout HybridSigmaAccel hca,
                                          bool initAccel);
#endif
subroutine vec3 computeGradientType(vec3 pos, in vec3 h);


// define subroutine uniform names
subroutine uniform sampleDataAtPosType      sampleDataAtPos;
#ifdef ENABLE_HYBRID_NEIGHBOURHOOD_ACCELERATION
subroutine uniform sampleDataAtPosAccelType sampleDataAtPosAccel;
#endif
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
    HybridSigmaAccel hcaDummy; // not required here
    return sampleHybridSigmaVolumeAtPos(dataVolume, dataExtent,
                                        surfacePressure, hybridCoefficients,
                                        pos, hcaDummy);
}
#endif


subroutine(sampleDataAtPosType)
float sampleLogPressureLevel(in vec3 pos)
{
    return sampleLogPressureLevelVolumeAtPos(dataVolume, dataExtent, pos);
}

#ifdef ENABLE_HYBRID_NEIGHBOURHOOD_ACCELERATION

subroutine(sampleDataAtPosAccelType)
float samplePressureLevelAccel(in vec3 pos, inout HybridSigmaAccel hca, bool initAccel)
{
    return samplePressureLevelVolumeAtPos(dataVolume, dataExtent,
                                          pressureTable, pos);
}

#ifdef ENABLE_HYBRID_PRESSURETEXCOORDTABLE
subroutine(sampleDataAtPosAccelType)
float sampleHybridLevelAccel(in vec3 pos, inout HybridSigmaAccel hca, bool initAccel)
{
    return sampleHybridSigmaVolumeAtPos_LUT(dataVolume, dataExtent,
                                            surfacePressure, hybridCoefficients,
                                            pressureTexCoordTable2D, pos);
}
#else
subroutine(sampleDataAtPosAccelType)
float sampleHybridLevelAccel(in vec3 pos, inout HybridSigmaAccel hca, bool initAccel)
{
    return sampleHybridSigmaVolumeAtPos(dataVolume, dataExtent,
                                        surfacePressure, hybridCoefficients,
                                        pos, hca);
}
#endif


subroutine(sampleDataAtPosAccelType)sampleHybridSigmaVolumeSingleBitAtPos
float sampleLogPressureLevelAccel(in vec3 pos, inout HybridSigmaAccel hca, bool initAccel)
{
    return sampleLogPressureLevelVolumeAtPos(dataVolume, dataExtent, pos);
}

#endif // ENABLE_HYBRID_NEIGHBOURHOOD_ACCELERATION


/******************************************************************************
***                            GRADIENT SUBROUTINES                         ***
*******************************************************************************/

/**
  Computes the gradient at position @p pos, using sampling step size @p h.

  Central differences are used for the computation, except for positions close
  to the boundaries of the data volume. Here, h is scaled and reduced to
  simple forward differences for positions less than one grid size away from
  the boundary.

  For pressure level and hybrid sigma pressure level grids, the vertical
  difference hz used to compute the central/forward differences are scaled
  according to the vertical level distance (which will or can vary with height).
 */

subroutine(computeGradientType)
vec3 pressureLevelGradient(in vec3 pos, in vec3 h)
{
    vec3 gradient;

    vec3 pos_east = vec3(min(pos.x + h.x, dataExtent.dataSECrnr.x), pos.yz);
    vec3 pos_west = vec3(max(pos.x - h.x, dataExtent.dataNWCrnr.x), pos.yz);

    float x1 = samplePressureLevelVolumeAtPos(dataVolume, dataExtent, pressureTable, pos_east);
    float x2 = samplePressureLevelVolumeAtPos(dataVolume, dataExtent, pressureTable, pos_west);
    float hx = pos_east.x - pos_west.x;

    vec3 pos_north = vec3(pos.x, min(pos.y + h.y, dataExtent.dataNWCrnr.y), pos.z);
    vec3 pos_south = vec3(pos.x, max(pos.y - h.y, dataExtent.dataSECrnr.y), pos.z);

    float y1 = samplePressureLevelVolumeAtPos(dataVolume, dataExtent, pressureTable, pos_north);
    float y2 = samplePressureLevelVolumeAtPos(dataVolume, dataExtent, pressureTable, pos_south);
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

    float z1 = samplePressureLevelVolumeAtPos(dataVolume, dataExtent, pressureTable, pos_top);
    float z2 = samplePressureLevelVolumeAtPos(dataVolume, dataExtent, pressureTable, pos_bot);
    hz = pos_top.z - pos_bot.z;

    gradient = vec3((x1 - x2) / abs(hx), (y1 - y2) / abs(hy), (z1 - z2) / abs(hz));

    return normalize(gradient);
}


subroutine(computeGradientType)
vec3 hybridLevelGradient(in vec3 pos, in vec3 h)
{
    vec3 gradient;

    HybridSigmaAccel hca;

    // 1. Sample data field at pos to determine grid column and levels.
//FIXME: The sampling at "pos" is unneeded. However, replacing this by the
//       sampling call for pos_east below breaks something. Fix this!
//       Some more things could be sped up as well... (mr, 04Aug2014)
    float v = sampleHybridSigmaVolumeAtPos(dataVolume, dataExtent,
                                           surfacePressure, hybridCoefficients,
                                           pos, hca);

    float hz1 = (hca.c00.ln_p[0] - pToWorldZParams.x) * pToWorldZParams.y;
    float hz2 = (hca.c00.ln_p[1] - pToWorldZParams.x) * pToWorldZParams.y;
    float hz = abs(hz1 - hz2);

    // 2. Sample with horizontal displacement, using clampToDataBoundary.
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

    float x1 = sampleHybridSigmaVolumeAtPos_accel(dataVolume, dataExtent,
                                                  surfacePressure, hybridCoefficients,
                                                  pos_east, true, hca);
    float x2 = sampleHybridSigmaVolumeAtPos_accel(dataVolume, dataExtent,
                                                  surfacePressure, hybridCoefficients,
                                                  pos_west, true, hca);
    float hx = pos_east.x - pos_west.x;

    vec3 pos_north = vec3(pos.x, min(pos.y + h.y, dataExtent.dataNWCrnr.y), pos.z);
    vec3 pos_south = vec3(pos.x, max(pos.y - h.y, dataExtent.dataSECrnr.y), pos.z);
//FIXME: Why do the accelerated sampling methods return other values here?
//       (mr, 04Aug2014) ?
     float y1 = sampleHybridSigmaVolumeAtPos_accel(dataVolume, dataExtent,
                                                   surfacePressure, hybridCoefficients,
                                                   pos_north, true, hca);
     float y2 = sampleHybridSigmaVolumeAtPos_accel(dataVolume, dataExtent,
                                                   surfacePressure, hybridCoefficients,
                                                   pos_south, true, hca);
    //float y1 = sampleDataAtPos(pos_north);
    //float y2 = sampleDataAtPos(pos_south);
    float hy = pos_north.y - pos_south.y;

    // 3. Sample with vertical displacement, considering data volume
    // boundaries.

    float zbot, ztop;
    getHybridSigmaBotTopLevelAtPos(dataExtent, surfacePressure,
                                   hybridCoefficients, pos, zbot, ztop);

    vec3 pos_top = vec3(pos.xy, min(pos.z + hz, ztop));
    vec3 pos_bot = vec3(pos.xy, max(pos.z - hz, zbot));

    float z1 = sampleHybridSigmaVolumeAtPos_accel(dataVolume, dataExtent,
                                                  surfacePressure, hybridCoefficients,
                                                  pos_top, true, hca);
    float z2 = sampleHybridSigmaVolumeAtPos_accel(dataVolume, dataExtent,
                                                  surfacePressure, hybridCoefficients,
                                                  pos_bot, true, hca);
    hz = pos_top.z - pos_bot.z;

    gradient = vec3((x1 - x2) / abs(hx), (y1 - y2) / abs(hy), (z1 - z2) / abs(hz));

    return normalize(gradient);
}


subroutine(computeGradientType)
vec3 logPressureLevelGradient(in vec3 pos, in vec3 h)
{
    vec3 gradient;

    // The easiest case: No orography can be crossed in the horizontal,
    // the vertical level distance is the same at all positions.

    vec3 pos_east = vec3(max(pos.x + h.x, dataExtent.dataSECrnr.x), pos.yz);
    vec3 pos_west = vec3(min(pos.x - h.x, dataExtent.dataNWCrnr.x), pos.yz);
    float x1 = sampleLogPressureLevelVolumeAtPos(dataVolume, dataExtent, pos_east);
    float x2 = sampleLogPressureLevelVolumeAtPos(dataVolume, dataExtent, pos_west);
    float hx = pos_east.x - pos_west.x;

    vec3 pos_north = vec3(pos.x, min(pos.y + h.y, dataExtent.dataNWCrnr.y), pos.z);
    vec3 pos_south = vec3(pos.x, max(pos.y - h.y, dataExtent.dataSECrnr.y), pos.z);
    float y1 = sampleLogPressureLevelVolumeAtPos(dataVolume, dataExtent, pos_north);
    float y2 = sampleLogPressureLevelVolumeAtPos(dataVolume, dataExtent, pos_south);
    float hy = pos_north.y - pos_south.y;

    vec3 pos_top = vec3(pos.xy, min(pos.z + h.z, dataExtent.dataNWCrnr.z));
    vec3 pos_bot = vec3(pos.xy, max(pos.z - h.z, dataExtent.dataSECrnr.z));
    float z1 = sampleLogPressureLevelVolumeAtPos(dataVolume, dataExtent, pos_top);
    float z2 = sampleLogPressureLevelVolumeAtPos(dataVolume, dataExtent, pos_bot);
    float hz = pos_top.z - pos_bot.z;

    gradient = vec3((x1 - x2) / abs(hx), (y1 - y2) / abs(hy), (z1 - z2) / abs(hz));

    return normalize(gradient);
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
void computeDepthFromWorld(in vec3 rayPosition)
{
    // first convert position to clip space
    vec4 clipSpacePos = mvpMatrix * vec4(rayPosition,1);
    // perform perspective division
    clipSpacePos.z /= clipSpacePos.w;
    // -> z = OPENGL ? [-1;1] : [0;1]
    // calculate depth value -> convert to [0,1]
    gl_FragDepth = clipSpacePos.z * 0.5 + 0.5;
}

vec3 initGradientSamplingStep(in DataVolumeExtent dve)
{
    // mk(04Nov2014): dve.deltaLnP is only used for computation in logPressure
    //                grid...
    return vec3(dve.deltaLatLon, dve.deltaLatLon, dve.deltaLnP);

    /*
    //vec3 h = vec3(0., 0., 0.);

    // case PRESSURE_LEVEL_3D
    if (dve.levelType == 0)
    {
//TODO: vertical h is an approximation and only correct for grids regular in p
        h = vec3(dve.deltaLatLon, dve.deltaLatLon, dve.deltaLnP);
    }
    // case HYBRID_SIGMA_PRESSURE_3D
    else if (dve.levelType == 1)
    {
        // Determine average worldZ-interval between model levels.
//TODO: NOTE that this is a simple average that does not account for the
// vertical stretching of the levels. We simply use the distance between the
// lower and upper data volume boundaries and divide it by the number of
// vertical levels.
        h = vec3(dve.deltaLatLon, dve.deltaLatLon, dve.deltaLnP);
    }
    // case LOG_PRESSURE_LEVEL_3D
    else
    {
        // correct vertical h
        h = vec3(dve.deltaLatLon, dve.deltaLatLon, dve.deltaLnP);
    }

    return h;*/
}

