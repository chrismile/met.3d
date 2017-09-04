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

#include "volume_defines.glsl"


/*****************************************************************************
 ***                             CONSTANTS
 *****************************************************************************/

const int MAX_ISOSURFACES = 10;

const int TRANSFER_FUNC_COLOUR = 1;
const int TRANSFER_FUNC_SHADINGVAR = 2;
const int TRANSFER_FUNC_SHADINGVAR_MAXNB = 3;

// Vertical level type; see structuredgrid.h.
const int SURFACE_2D = 0;
const int PRESSURE_LEVELS_3D = 1;
const int HYBRID_SIGMA_PRESSURE_3D = 2;
const int POTENTIAL_VORTICITY_2D = 3;
const int LOG_PRESSURE_LEVELS_3D = 4;
const int AUXILIARY_PRESSURE_3D = 5;

const int SHADOWS_OFF = 0;
const int SHADOWS_MAP = 1;
const int SHADOWS_VOLUME_AND_RAY = 2;


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

interface VStoFS
{
    smooth vec3 worldSpaceCoordinate; // interpolate the world space coordinate
                                      // to get the fragment position in world
                                      // space
};


/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

// textures
// ========
uniform sampler1D transferFunction;
uniform sampler1D transferFunctionShV; // if separate shading var is used
// contains precomputed z-coordinates
uniform sampler1D pressureTable;    // PRESSURE_LEVEL
uniform sampler1D pressureTableShV; // PRESSURE_LEVEL
// contains hybrid coefficients a,b
uniform sampler1D hybridCoefficients;    // HYBRID_SIGMA
uniform sampler1D hybridCoefficientsShV; // HYBRID_SIGMA
// contains surface pressure at grid point (i, j)
uniform sampler2D surfacePressure;    // HYBRID_SIGMA
uniform sampler2D surfacePressureShV; // HYBRID_SIGMA
// contains pressure field
uniform sampler3D auxPressureField3D_hPa;    // AUXILIARY_PRESSURE_3D
uniform sampler3D auxPressureField3DShV_hPa; // AUXILIARY_PRESSURE_3D

#ifdef ENABLE_HYBRID_PRESSURETEXCOORDTABLE
// contains precomputed z-coordinates
uniform sampler2D pressureTexCoordTable2D;    // HYBRID_SIGMA
uniform sampler2D pressureTexCoordTable2DShV; // HYBRID_SIGMA
#endif

#ifdef ENABLE_MINMAX_ACCELERATION
// Regular grid that uniformly divides space and stores min/max for each voxel.
// Is used to skip regions in which an isosurface cannot be located. See
// MStructuredGrid::getMinMaxAccelTexture3D().
uniform sampler3D minMaxAccel3D;
uniform sampler3D minMaxAccel3DShV;
#endif

uniform sampler2D depthTex; // for normal curve integration

uniform sampler3D dataVolume;
uniform sampler3D dataVolumeShV; // shading var data
uniform sampler1D lonLatLevAxes;
uniform sampler1D lonLatLevAxesShV;

// bitfield specific
uniform usampler3D flagsVolume;

// matrices
// ========
uniform mat4    mvpMatrix;

// vectors
// =======
uniform vec3    cameraPosition;
uniform vec3    volumeBottomSECrnr;
uniform vec3    volumeTopNWCrnr;

// scalars
// =======
uniform float   stepSize;
uniform uint    bisectionSteps;

// Multi-isosurface settings
// =========================
uniform bool    isoEnables[MAX_ISOSURFACES];
uniform float   isoValues[MAX_ISOSURFACES];
uniform vec4    isoColors[MAX_ISOSURFACES];
uniform int     isoColorModes[MAX_ISOSURFACES];
uniform int     numIsoValues;

/*** ECMWF-specific ***/
//   ==============   //
// 0 = PRESSURE_LEVEL
// 1 = HYBRID_SIGMA
// 2 = LOG_PRESSURE_LEVEL

uniform vec2    pToWorldZParams;
uniform float   numPressureLevels;
uniform int     ensembleMember;

/*** Lighting settings ***/
//   =================   //
uniform int     lightingMode;
uniform vec3    lightDirection;
uniform float   ambientCoeff;
uniform float   diffuseCoeff;
uniform float   specularCoeff;
uniform float   shininessCoeff;
uniform int     shadowMode;
uniform vec4    shadowColor;


/*****************************************************************************
 ***                             INCLUDES
 *****************************************************************************/

// include global structs
#include "volume_global_structs_utils.glsl"
// include hybrid model volume sampling methods
#include "volume_hybrid_utils.glsl"
// include model level volume with auxiliary pressure field sampling methods
#include "volume_auxiliarypressure_utils.glsl"
// include pressure levels volume sampling methods
#include "volume_pressure_utils.glsl"
// defines subroutines and auxiliary ray-casting functions
#include "volume_sample_utils.glsl"
// shading variable sampling methods
#include "volume_sample_shv_utils.glsl"
// procedures to determine the color of the surfaces
#include "volume_render_utils.glsl"
// bitfield-specific procedures
#include "volume_bitfield_utils.glsl"


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec3 vertex : 0, out VStoFS Output)
{
    // Convert pressure to world Z coordinate.
    float worldZ = (log(vertex.z) - pToWorldZParams.x) * pToWorldZParams.y;
    // Convert from world space to clip space.
    gl_Position = mvpMatrix * vec4(vertex.xy, worldZ, 1.);
    // Pass the world space coordinate on to the fragment shader. It will be
    // interpolated to yield the fragment position in world space.
    Output.worldSpaceCoordinate = vec3(vertex.xy, worldZ);
}


/*****************************************************************************/

shader VSShadow(in vec2 vertex0 : 0, in vec3 border0 : 1, out VStoFS Output)
{
    // pass vertex unhandled through vertex shader
    Output.worldSpaceCoordinate = border0;
    // ndc coordinates.xy | -1 = near plane | 1 = point
    gl_Position = vec4(vertex0,-1,1);
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

void raycaster(in vec3 h_gradient, in Ray ray, in vec2 lambdaNearFar,
               inout vec4 rayColor, out vec3 rayPosition,
               out vec3 crossingPosition)
{
    // set values for the ray casted into the scene
    float lambda = lambdaNearFar.x;
    rayPosition = ray.origin + lambdaNearFar.x * ray.direction;
    vec3 rayPosIncrement = stepSize * ray.direction;
    vec3 prevRayPosition = rayPosition;
    float prevLambda = lambda;

    // compute global depth in clip space from depth image
    vec4 projPos = mvpMatrix * vec4(rayPosition, 1);
    projPos.xy /= projPos.w;
    vec2 depthTexCoord = projPos.xy * 0.5 + 0.5;
    float depthGlobal = texture(depthTex, depthTexCoord).x;

    // compute current crossing levels
    int crossingLevelBack[maxBits];
    int crossingLevelFront[maxBits];

    // determine the range of the examined bits in the bitfield
    uint startBit = 0;
    uint stopBit  = maxBits;

    // we only investigate on single bit of the 64-bit value
    // if it is set, the current ensembler member contributes to the common
    // probability
    if (raycastMode == RAYCAST_SINGLE_BIT)
    {
        // currentBit is set to ensembleMember
        float scalar = sampleDataBitfieldAtPos(rayPosition, ensembleMember);
        crossingLevelBack[ensembleMember] = crossingLevelFront[ensembleMember] =
                computeCrossingLevel(scalar);
        startBit = ensembleMember;
        stopBit  = ensembleMember + 1;
    }
    // we take into account the contribution of all ensemble members at the
    // same time. Thus every bit is checked
    else if (raycastMode == RAYCAST_ALL_BITS_LOOP)
    {
        for (uint bit = 0; bit < maxBits; bit++)
        {
            //currentBit = bit;
            float scalar = sampleDataBitfieldAtPos(rayPosition, bit);
            crossingLevelBack[bit] = crossingLevelFront[bit] =
                    computeCrossingLevel(scalar);
        }
    }
    // we obtain all bits from the 64-bit value at once
    else if (raycastMode == RAYCAST_ALL_BITS_ARRAY)
    {
        float scalar[maxBits] = sampleDataAllBitsAtPos(rayPosition);
        crossingLevelBack = computeAllCrossingLevels(scalar);
        crossingLevelFront = computeAllCrossingLevels(scalar);
    }

    crossingPosition = rayPosition;

    // depth has to be corrected to create correct vis when rendering normal curves, as well
    bool lastCrossing = false;
    // if semi-transparent iso-surface is rendered only, take the first crossing as iso position
    bool firstCrossing = false;

    // start ray-casting process
    while (lambda < lambdaNearFar.y && rayColor.a < 0.99)
    {
        float scalar;

        // sample the current bit values and determine the corresponding crossing
        // level for these bits
        if (raycastMode == RAYCAST_SINGLE_BIT)
        {
            // currentBit is set to ensembleMember
            scalar = sampleDataBitfieldAtPos(rayPosition, ensembleMember);
            crossingLevelFront[ensembleMember] =
                    computeCrossingLevel(scalar);
        }
        else if (raycastMode == RAYCAST_ALL_BITS_LOOP)
        {
            for (uint bit = 0; bit < maxBits; bit++)
            {
                float scalar = sampleDataBitfieldAtPos(rayPosition, bit);
                crossingLevelFront[bit] = computeCrossingLevel(scalar);
            }
        }
        else if (raycastMode == RAYCAST_ALL_BITS_ARRAY)
        {
            float scalar[maxBits] = sampleDataAllBitsAtPos(rayPosition);
            crossingLevelFront = computeAllCrossingLevels(scalar);
        }

        int test;
        for (uint bit = startBit; bit < stopBit; bit++)
        {
            test |= crossingLevelFront[bit];
            test |= crossingLevelBack[bit];
        }

        // check for every bit if any iso-surface is crossed
        for (uint bit = startBit; bit < stopBit; bit++)
        {
            if (test == 0) { break; }

            if (crossingLevelFront[bit] != crossingLevelBack[bit])
            {
                vec3 gradient = vec3(0);

                // perform a bisection correction to precise the location of the iso-surface
                bisectionCorrectionBitfield(rayPosition, lambda, prevRayPosition, prevLambda,
                                            crossingLevelFront[bit], crossingLevelBack[bit], bit);

                gradient = computeGradientBitfieldAtPos(rayPosition, h_gradient, bit);

                // blending coefficient for surface blending, useful if transfer function is used
                const float t = float(bit) / float(maxBits);

                vec4 blendColor = blendIsoSurfaces(
                                crossingLevelFront[bit], crossingLevelBack[bit],
                                rayPosition, gradient, t);

                if (shadowMode > 0) { blendColor = shadowColor; }

                // Remember the position either of the first isosurface crossing
                // or of the first crossing with an opaque (alpha = 1) surface.
                if (!firstCrossing || blendColor.a == 1)
                {
                    crossingPosition = rayPosition;
                    firstCrossing = true;
                }

                rayColor.rgb += (1 - rayColor.a) * blendColor.rgb;
                rayColor.a += (1 - rayColor.a) * blendColor.a;

                break; // stop loop after the first isosurface hit
            }
        }

        // advance ray to the next position
        prevLambda  = lambda;
        prevRayPosition = rayPosition;

        lambda += stepSize;
        rayPosition += rayPosIncrement;

        // update crossing level
        crossingLevelBack = crossingLevelFront;
    }
}


shader FSmain(in VStoFS Input, out vec4 fragColor)
{
    vec3 h_gradient = initGradientSamplingStep(dataExtent);

    Ray ray;
    vec2 lambdaNearFar = vec2(0,0);
    if (shadowMode == SHADOWS_MAP)
    {
        // Render a shadow image of the scene: The ray is parallel to the
        // z-axis and casted downward through the data volume.
        ray.origin = Input.worldSpaceCoordinate;
        ray.direction = vec3(0., 0., -1.);

        lambdaNearFar.x = 0.;
        lambdaNearFar.y = volumeTopNWCrnr.z - volumeBottomSECrnr.z;

        //shadowRay = true;
    }
    else
    {
        // Volume rendering mode: The ray direction is determined by the camera
        // position and the fragment position, both in world space.
        ray.origin = cameraPosition;
        ray.direction = normalize(Input.worldSpaceCoordinate - cameraPosition);

        // Compute the intersection points of the ray with the volume bounding
        // box. If the ray does not intersect with the box discard this fragment.
        bool rayIntersectsRenderVolume = rayBoxIntersection(
                ray, volumeBottomSECrnr, volumeTopNWCrnr, lambdaNearFar);
        if (!rayIntersectsRenderVolume) { discard; }

        // If the value for lambdaNear is < 0 the camera is located inside the
        // bounding box. It makes no sense to start the ray traversal behind the
        // camera, hence move lambdaNear to 0 to start in front of the camera.
        lambdaNearFar.x = max(lambdaNearFar.x, 0.01);

//TODO (mr, 17Nov2014): The sampling at the position computed from
//      lambdaNearFar.x often fails, resulting in artefacts at the data volume
//      boundaries. This might be due to numerical inaccuracies that cause
//      lambda_near to be located slightly before the box. I move the ray
//      a bit foward here, but that is not a clean solution.
        lambdaNearFar.x += 0.001;
    }

    vec4 rayColor = vec4(0);
    vec3 rayPosition;
    vec3 crossingPosition;
    vec3 depthPosition;

    // perform the raycasting process to detect the bit-wise isosurfaces
    raycaster(h_gradient, ray, lambdaNearFar, rayColor, rayPosition,
              crossingPosition);

    if (rayColor.a == 0) { discard; }
    depthPosition = crossingPosition;

    // compute correct depth value
    gl_FragDepth = computeDepthFromWorld(depthPosition);

    fragColor = rayColor;
}

/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Volume
{
    vs(420)=VSmain();
    fs(420)=FSmain();
};

program Shadow
{
    vs(420)=VSShadow();
    fs(420)=FSmain();
};
