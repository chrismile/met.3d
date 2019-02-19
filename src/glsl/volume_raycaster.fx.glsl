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

const int RENDER_ISOSURFACES = 0;
const int RENDER_DVR = 2;

// Workaround; see usage below.
const float LAMBDA_OFFSET = 0.001;

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
uniform sampler1D pressureTable; // PRESSURE_LEVEL
uniform sampler1D pressureTableShV; // PRESSURE_LEVEL
// contains hybrid coefficients a,b
uniform sampler1D hybridCoefficients; // HYBRID_SIGMA
uniform sampler1D hybridCoefficientsShV; // HYBRID_SIGMA
// contains surface pressure at grid point (i, j)
uniform sampler2D surfacePressure; // HYBRID_SIGMA
uniform sampler2D surfacePressureShV; // HYBRID_SIGMA
// contains pressure field
uniform sampler3D auxPressureField3D_hPa; // AUXILIARY_PRESSURE_3D
uniform sampler3D auxPressureField3DShV_hPa; // AUXILIARY_PRESSURE_3D

#ifdef ENABLE_HYBRID_PRESSURETEXCOORDTABLE
// contains precomputed z-coordinates
uniform sampler2D pressureTexCoordTable2D; // HYBRID_SIGMA
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
uniform sampler2D depthBufferMirror; // mirrored OpenGL depth buffer for
                                     // DVR ray termination

uniform sampler3D dataVolume;
uniform sampler3D dataVolumeShV; // shading var data
uniform sampler1D lonLatLevAxes;
uniform sampler1D lonLatLevAxesShV;

// matrices
// ========
uniform mat4    mvpMatrix;
uniform mat4    mvpMatrixInverted;

// vectors
// =======
uniform vec3    cameraPosition;
uniform vec3    volumeBottomSECrnr;
uniform vec3    volumeTopNWCrnr;

// scalars
// =======
uniform float   stepSize;
uniform uint    bisectionSteps;

uniform int     viewPortWidth;
uniform int     viewPortHeight;


// Multi-isosurface settings
// =========================
uniform bool    isoEnables[MAX_ISOSURFACES];
uniform float   isoValues[MAX_ISOSURFACES];
uniform vec4    isoColors[MAX_ISOSURFACES];
uniform int     isoColorModes[MAX_ISOSURFACES];
uniform int     numIsoValues;

uniform vec2    pToWorldZParams;
uniform bool    spatialCDFVisEnabled;
uniform float   numPressureLevels;

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

// Mode
// ====
uniform uint    renderingMode;
uniform bool    isOrthographic;


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
 
/**
  Traverse a section of the data volume from lambda to lambdaExit, using 
  constant step size.
  
  Returns true if the entire section is traversed; false if any break condition
  (opacity saturated, normal curve hit) leads to early ray termination.
 */
bool traverseSectionOfDataVolume(
        in vec3 h_gradient, in float depthGlobal, in vec3 rayPosIncrement,
        in float lambdaExit,
        inout float lambda, inout vec3 prevRayPosition, inout float prevLambda,
        inout vec4 rayColor, inout vec3 rayPosition,
        inout int crossingLevelFront, inout int crossingLevelBack,
        inout bool firstCrossing, inout bool lastCrossing,
        inout vec3 crossingPosition)
{
    // Main ray caster loop. Advance the current position along the ray
    // direction through the volume, fetching scalar values from texture memory
    // and testing for isosurface crossing.
    while (lambda < lambdaExit)
    {
        float scalar = sampleDataAtPos(rayPosition);

        if (scalar != M_MISSING_VALUE)
        {
            crossingLevelFront = computeCrossingLevel(scalar);
            if (crossingLevelFront != crossingLevelBack)
            {
                vec3 gradient = vec3(0);
                if (!shadowRay)
                {
                    bisectionCorrection(rayPosition, lambda, prevRayPosition,
                                        prevLambda, crossingLevelFront,
                                        crossingLevelBack);

                    gradient = computeGradient(rayPosition, h_gradient);
                }

                vec4 blendColor = blendIsoSurfaces(
                        crossingLevelFront, crossingLevelBack, rayPosition,
                        gradient, scalar);

                // Remember the position either of the first isosurface crossing
                // or of the first crossing with an opaque (alpha = 1) surface.
                if (!firstCrossing || blendColor.a == 1)
                {
                    crossingPosition = rayPosition;
                    firstCrossing = true;
                }

                rayColor.rgb += (1 - rayColor.a) * blendColor.rgb;
                rayColor.a += (1 - rayColor.a) * blendColor.a;
            }
        }
        
        prevLambda  = lambda;
        prevRayPosition = rayPosition;

        lambda += stepSize;
        rayPosition += rayPosIncrement;

        crossingLevelBack = crossingLevelFront;

        // Depth correction for normal curves.
        // ===================================
        if (!shadowRay)
        {
            // Depth has to be corrected to create correct vis when rendering
            // normal curves.
            vec4 clipSpacePos = mvpMatrix * vec4(rayPosition,1);
            clipSpacePos /= clipSpacePos.w;
            float depth = clipSpacePos.z * 0.5 + 0.5;

            if (depth > depthGlobal)
            {
                if (!lastCrossing)
                {
                    // last crossing correction, approximate depth value of
                    // normal curve
                    bisectionCorrectionDepth(rayPosition, prevRayPosition,
                                             depthGlobal);
                    lastCrossing = true;
                }
                else
                    return false;
            }
        }

        // Terminate ray if alpha is saturated.
        if (rayColor.a > 0.99) return false;
    } // raycaster loop
    
    // The section has been traversed up to lambdaExit: Return true.
    return true;
}


bool traverseSectionOfDataVolume_DVR(
        in vec3 h_gradient, in float depthGlobal, in vec3 rayPosIncrement,
        in float lambdaExit,
        inout float lambda, inout vec3 prevRayPosition, inout float prevLambda,
        inout vec4 rayColor, inout vec3 rayPosition,
        inout int crossingLevelFront, inout int crossingLevelBack,
        inout bool firstCrossing, inout bool lastCrossing,
        inout vec3 crossingPosition)
{
    // Pre-multiply colour by alpha.
    rayColor.rgb = rayColor.rgb * rayColor.a;

    // Main ray caster loop. Advance the current position along the ray
    // direction through the volume, fetching scalar values from texture memory
    // and testing for isosurface crossing.
    bool fullTraversal = true;
    while (lambda < lambdaExit)
    {
        float scalar = sampleDataAtPos(rayPosition);

        if (scalar != M_MISSING_VALUE)
        {
            // Fetch segment blend colour from transfer function.
            float t = (scalar - dataExtent.tfMinimum)
                        / (dataExtent.tfMaximum - dataExtent.tfMinimum);
            vec4 blendColor = texture(transferFunction, t).rgba;
            
            // Shadow mode: Alpha of shadow is determined by alpha of transfer
            // function colour, multiplied by alpha value of shadow colour to
            // enable user to control lightness of shadow.
            if (shadowRay)
            {
                blendColor = vec4(shadowColor.rgb,
                                  blendColor.a * shadowColor.a);
            }

            // Opacity correction.
            // Reference: Engel et al. (Real-time volume graphics, 2006), Eq. 1.17
            float stepSizeRatio = stepSize / 1.; // reference step size is 1.
            blendColor.a = 1. - pow(1. - blendColor.a, stepSizeRatio);
            // Alternative from TUM.3D practical course IVDA, WS2016/16 slide 14:
            //blendColor.a = 1. - exp(-1. * blendColor.a * stepSize);

            // Front to back alpha blending.
            blendColor.rgb *= blendColor.a;
            rayColor.rgb += (1. - rayColor.a) * blendColor.rgb;
            rayColor.a += (1. - rayColor.a) * blendColor.a;
        }
        
        prevLambda  = lambda;
        prevRayPosition = rayPosition;

        lambda += stepSize;
        rayPosition += rayPosIncrement;
        
        // Terminate ray if alpha is saturated.
        if (rayColor.a > 0.99)
        {
            fullTraversal = false;
            break;
        }
    } // raycaster loop

    // Avoid divison by 0.
    if (rayColor.a != 0.f)
    {
        // Restore ray colour from alpha-multiplied value.
        rayColor.rgb = rayColor.rgb / rayColor.a;
    }
    
    return fullTraversal;
}


/**
  Set up variables required for raycasting and invoke volume traversal loop.
 */
void raycaster(in vec3 h_gradient, in Ray ray, in vec2 lambdaNearFar,
               inout vec4 rayColor, out vec3 rayPosition,
               out vec3 crossingPosition)
{
    // Initialize ray casting parameters.
    float lambda = lambdaNearFar.x;
    rayPosition = ray.origin + lambdaNearFar.x * ray.direction;
    vec3 rayPosIncrement = stepSize * ray.direction;
    vec3 prevRayPosition = rayPosition;
    float prevLambda = lambda;

    float scalar = sampleDataAtPos(rayPosition);

    int crossingLevelBack, crossingLevelFront;
    crossingLevelBack = crossingLevelFront = computeCrossingLevel(scalar);
    crossingPosition = rayPosition;
    bool firstCrossing = false;
    bool lastCrossing = false;

    // Compute global depth in clip space from depth image.
    vec4 projPos = mvpMatrix * vec4(rayPosition, 1.);
    projPos.xy /= projPos.w;
    vec2 depthTexCoord = projPos.xy * 0.5 + 0.5;
    float depthGlobal = texture(depthTex, depthTexCoord).x;

#ifndef ENABLE_MINMAX_ACCELERATION
    // Empty space skipping acceleration is not enabled:
    // Traverse entire data volume with fixed step size.
    // =================================================
    
    if (renderingMode == RENDER_ISOSURFACES)
    {
        traverseSectionOfDataVolume(
            h_gradient, depthGlobal, rayPosIncrement,
            lambdaNearFar.y,
            lambda, prevRayPosition, prevLambda,
            rayColor, rayPosition,
            crossingLevelFront, crossingLevelBack,
            firstCrossing, lastCrossing,
            crossingPosition);
    }
    else if (renderingMode == RENDER_DVR)
    {
        traverseSectionOfDataVolume_DVR(
            h_gradient, depthGlobal, rayPosIncrement,
            lambdaNearFar.y,
            lambda, prevRayPosition, prevLambda,
            rayColor, rayPosition,
            crossingLevelFront, crossingLevelBack,
            firstCrossing, lastCrossing,
            crossingPosition);
    }
        
    // =================================================
        
#else
    // Empty space skipping acceleration is enabled:
    // Step through acceleration structure and only traverse volume bricks
    // that potentially contain an isosurface.
    // ===================================================================

    // Don't use empty space skipping for DVR-mode if neither the minimum value
    // nor the maximum value have an alpha equal to 0 (This means, no space to
    // skip).
    if (renderingMode == RENDER_DVR && texture(transferFunction, 0.).a > 0.
            && texture(transferFunction, 1.).a > 0.)
    {
        traverseSectionOfDataVolume_DVR(
                    h_gradient, depthGlobal, rayPosIncrement,
                    lambdaNearFar.y,
                    lambda, prevRayPosition, prevLambda,
                    rayColor, rayPosition,
                    crossingLevelFront, crossingLevelBack,
                    firstCrossing, lastCrossing,
                    crossingPosition);
        return;
    }

    // Grid spacing of the acceleration structure. 

    ivec3 nMinMaxAccel3D = ivec3(textureSize(minMaxAccel3D, 0).x,
                                 textureSize(minMaxAccel3D, 0).y,
                                 textureSize(minMaxAccel3D, 0).z);
    
    // Special case: If the grid is cyclic in longitude, shift the eastern 
    // longitude one grid spacing east (e.g. make 359. 360.).
    float dataSECrnr_x = dataExtent.dataSECrnr.x;
    if (dataExtent.gridIsCyclicInLongitude) dataSECrnr_x += dataExtent.deltaLon;

    vec3 deltaAccel = vec3(
            abs(dataSECrnr_x - dataExtent.dataNWCrnr.x) /
                float(nMinMaxAccel3D.x),
            abs(dataExtent.dataSECrnr.y - dataExtent.dataNWCrnr.y) /
                float(nMinMaxAccel3D.y),
            abs(dataExtent.dataSECrnr.z - dataExtent.dataNWCrnr.z) /
                float(nMinMaxAccel3D.z));

    // In longitude, the world space coordinate system can be shifted by
    // multiples of 360 degrees with respect to the data coordinate system
    // (e.g. if the data volume is defined from -60..60 degrees it may be
    // rendered in the range 300..420 degrees; or cyclic global grids may
    // be repeated at the boundaries).
    // To compute the correct entry indices into the acceleration structure,
    // find closest (shifted) dataNWCrnr.x WEST of rayPosition.x by shifting by
    // multiples of 360. degrees longitude.
    // Example: if dataNWCrnr.x == -60. and rayPosition.x == 320., then
    // dataNWCrnr.x is shifted to 300. If rayPosition.x == -70., then
    // dataNWCrnr.x is shifted to -420.
    float distanceRayPosDataNW_x = rayPosition.x - dataExtent.dataNWCrnr.x;
    int numShift360degrees = int(distanceRayPosDataNW_x / 360.);
    if (rayPosition.x < dataExtent.dataNWCrnr.x) numShift360degrees -= 1;
    float dataNWCrnr_x = dataExtent.dataNWCrnr.x + numShift360degrees * 360.;

    // Indices of the position of ray entry into the acceleration structure
    // (the min/max map covers the same world space as the data volume).
    // "iaccel" is updated during the traversal to keep track of the current
    // brick.
    float mixI = (rayPosition.x - dataNWCrnr_x) / deltaAccel.x;
    float mixJ = (dataExtent.dataNWCrnr.y - rayPosition.y) / deltaAccel.y;
    float mixK = (dataExtent.dataNWCrnr.z - rayPosition.z) / deltaAccel.z;
    ivec3 iaccel = ivec3(int(mixI), int(mixJ), int(mixK)); 
    
    // Intersection coordinates of the ray with the axis-perpendicular planes upon
    // exit of the initial acceleration brick (these are three independent
    // coordinates that do not form a point; they are stored in a vector for
    // convenience).
    vec3 posExit = vec3(
            dataNWCrnr_x
            + (iaccel.x + (ray.direction.x >= 0.0 ? 1 : 0)) * deltaAccel.x,
            dataExtent.dataNWCrnr.y
            - (iaccel.y + (ray.direction.y >= 0.0 ? 0 : 1)) * deltaAccel.y,
            dataExtent.dataNWCrnr.z
            - (iaccel.z + (ray.direction.z >= 0.0 ? 0 : 1)) * deltaAccel.z);

    vec3 rayDirInv = vec3(1./ray.direction.x, 
                          1./ray.direction.y,
                          1./ray.direction.z);                

    // The above plane-ray intersection coordinates parameterised to lambda.
    vec3 lambdaExitAccel = vec3((posExit.x - ray.origin.x) * rayDirInv.x,
                                (posExit.y - ray.origin.y) * rayDirInv.y,
                                (posExit.z - ray.origin.z) * rayDirInv.z);
    lambdaExitAccel = abs(lambdaExitAccel);

    // Distance in lambda-"coordinates" between two bricks in x/y/z direction.
    vec3 deltaLambdaAccel = vec3(deltaAccel.x * rayDirInv.x,
                                 deltaAccel.y * rayDirInv.y,
                                 deltaAccel.z * rayDirInv.z);
    deltaLambdaAccel = abs(deltaLambdaAccel);

    // Traversal direction for the acceleration structure in grid index space.
    ivec3 iincr = ivec3(ray.direction.x >= 0.0 ? 1 : -1, // world x = lon = same axis as data
                        ray.direction.y >= 0.0 ? -1 : 1, 
                        ray.direction.z >= 0.0 ? -1 : 1);

    // The min/max isovalues of the isosurfaces that are rendered in this
    // render pass.
    vec2 isoMinMax = vec2(isoValues[0], isoValues[0]);
    if (renderingMode == RENDER_ISOSURFACES)
    {
        for (int i = 1; i < numIsoValues; i++)
        {
            if (isoValues[i] < isoMinMax.x) isoMinMax.x = isoValues[i];
            if (isoValues[i] > isoMinMax.y) isoMinMax.y = isoValues[i];
        }
    }
    // For DVR use the minimum and maximum value of the transfer function.
    else if (renderingMode == RENDER_DVR)
    {
        // Set isoMin to -infinity. (No skip if not changed.)
        isoMinMax.x = -1.0 / 0.0;
        // Set isoMax to infinity. (No skip if not changed.)
        isoMinMax.y = 1.0 / 0.0;

        // Since the user is free to set the maximum value of the transfer
        // function to a value smaller than the minimum value of the transfer
        // function, both cases have to be considered.
        if (dataExtent.tfMinimum < dataExtent.tfMaximum)
        {
            // Only skip isovalues smaller minimum isovalue of transfer function
            // if alpha of minimum equals 0.
            if (texture(transferFunction, 0.).a == 0.)
            {
                isoMinMax.x = dataExtent.tfMinimum;
            }
            // Only skip isovalues greater maximum isovalue of transfer function
            // if alpha of maximum equals 0.
            if (texture(transferFunction, 1.).a == 0.)
            {
                isoMinMax.y = dataExtent.tfMaximum;
            }

        }
        else
        {
            // Only skip isovalues greater maximum isovalue of transfer function
            // if alpha of maximum equals 0. (In this case tfMinimum stores max)
            if (texture(transferFunction, 0.).a == 0.)
            {
                isoMinMax.y = dataExtent.tfMinimum;
            }
            // Only skip isovalues smaller minimum isovalue of transfer function
            // if alpha of minimum equals 0. (In this case tfMaximum stores min)
            if (texture(transferFunction, 1.).a == 0.)
            {
                isoMinMax.x = dataExtent.tfMaximum;
            }

        }
    }

    // Loop that traverses the min/max structure. "Empty" (with respect to the
    // targeted isosurfaces) space is skipped; bricks that potentially contain
    // an isosurface are traversed.
    // References: Krueger and Westermann (2003); Shirley, Fundamentals of
    // Computer Graphics, 3rd ed. (2009), Ch. 12.2.3.
    while (lambda < lambdaNearFar.y)
    {
        // Get min/max scalar values contained in the current brick. Check
        // if an isosurface can be contained.
        vec2 brickMinMax = texelFetch(minMaxAccel3D, iaccel, 0).rg;
        bool doBrickTraversal = (! ((brickMinMax.y < isoMinMax.x)
                                        || (brickMinMax.x > isoMinMax.y)));
        //doBrickTraversal = false;

        float lambdaBrickExit = lambda;
        
        // DEBUG -- Uncomment to visualise the boundary artefacts described
        // in the TODO (mr, 17Nov2014) in the fragment shader.
        //rayColor = vec4(brickMinMax.g / 30., 0., 0., 1.); break;
        
        // DEBUG -- Uncomment to see whether we're getting the right entry
        // brick.
        //rayColor = vec4(iaccel.z / 32., iaccel.y / 32., iaccel.x / 32., 1.); break;
        
        // Determine which axis-perpendicular plane is crossed next. This 
        // corresponds to the currently smallest "lambdaExit".
        if (lambdaExitAccel.x < lambdaExitAccel.y)
        {
            if (lambdaExitAccel.x < lambdaExitAccel.z) // x is smallest
            {
                // The ray exits the brick through the boundary perpendicular
                // to the x (i.e. longitude) coordinate. If the brick is to
                // be traversed, set lambdaBrickExit to the exit point.
                if (doBrickTraversal) lambdaBrickExit = lambdaExitAccel.x;
                // If the brick cannot not contain any isosurface, advance ray
                // to next brick.
                else lambda = lambdaExitAccel.x;

                // Update lambdaExitAccel.x and iaccel.x to represent the exit
                // position and index of the next brick.
                lambdaExitAccel.x += deltaLambdaAccel.x;
                iaccel.x += iincr.x;
                
                // If the grid is cyclic in longitude, map iaccel.x range to
                // (0 .. nMinMaxAccel3D.x - 1).
                if (dataExtent.gridIsCyclicInLongitude) iaccel.x %= nMinMaxAccel3D.x;
            }
        
            else // z < x < y => z is smallest
            {
                if (doBrickTraversal) lambdaBrickExit = lambdaExitAccel.z;
                else lambda = lambdaExitAccel.z;
                                
                lambdaExitAccel.z += deltaLambdaAccel.z;
                iaccel.z += iincr.z;                    
            }
        }
        
        else
        {
            if (lambdaExitAccel.y < lambdaExitAccel.z) // y is smallest
            {
                if (doBrickTraversal) lambdaBrickExit = lambdaExitAccel.y;
                else lambda = lambdaExitAccel.y;
                                
                lambdaExitAccel.y += deltaLambdaAccel.y;
                iaccel.y += iincr.y;                    
            }
        
            else // z < y < x => z is smallest
            {
                if (doBrickTraversal) lambdaBrickExit = lambdaExitAccel.z;
                else lambda = lambdaExitAccel.z;
                                
                lambdaExitAccel.z += deltaLambdaAccel.z;
                iaccel.z += iincr.z;                    
            }
        }
        
        if (doBrickTraversal)
        {
            // The brick is to be traversed, invoke traversal.
            bool brickHasBeenFullyTraversed = false;
            if (renderingMode == RENDER_ISOSURFACES)
            {
                brickHasBeenFullyTraversed = traverseSectionOfDataVolume(
                        h_gradient, depthGlobal, rayPosIncrement,
                        min(lambdaBrickExit, lambdaNearFar.y), // don't overshoot!
                        lambda, prevRayPosition, prevLambda,
                        rayColor, rayPosition,
                        crossingLevelFront, crossingLevelBack,
                        firstCrossing, lastCrossing,
                        crossingPosition);
            }
            else if (renderingMode == RENDER_DVR)
            {
                brickHasBeenFullyTraversed = traverseSectionOfDataVolume_DVR(
                        h_gradient, depthGlobal, rayPosIncrement,
                        min(lambdaBrickExit, lambdaNearFar.y), // don't overshoot!
                        lambda, prevRayPosition, prevLambda,
                        rayColor, rayPosition,
                        crossingLevelFront, crossingLevelBack,
                        firstCrossing, lastCrossing,
                        crossingPosition);
            }

            // Has the ray been terminated in the brick? Then also terminate the
            // min/max map traversal.
            if (!brickHasBeenFullyTraversed) break;
        }
        else
        {
            // The brick shall not be traversed, skip the space and advance
            // ray position etc. to the brick exit.
            prevLambda  = lambda - stepSize;
            rayPosition = ray.origin + lambda * ray.direction;;
            prevRayPosition = rayPosition - rayPosIncrement;
        }

        // Special case for non-cyclic grids. If parts of a non-cyclic data
        // volume are repeated (e.g. a volume defined in the range -60..40 deg
        // in longitude is rendered from 0..360 deg) and the samping position
        // has left valid data space, skip the empty space and re-enter the
        // data volume where its next recurrence starts. The case is simply
        // detected by testing whether iaccel.x still referes to valid brick
        // indidces.
        if ( (iaccel.x < 0) || (iaccel.x >= nMinMaxAccel3D.x) )
        {
            // Skip empty space and compute the lambda of the position where
            // the ray enters the next recurrence of the data volume.
            if (iaccel.x >= nMinMaxAccel3D.x)
            {
                // ray.direction.x >= 0.0: move eastward.
                dataNWCrnr_x += 360.;
                lambda = abs((dataNWCrnr_x - ray.origin.x) * rayDirInv.x);
            }
            else
            {
                // ray.direction.x < 0.0: move westward.
                dataNWCrnr_x -= 360.;
                float widthOfDataVolume = dataExtent.dataSECrnr.x - dataExtent.dataNWCrnr.x;
                lambda = abs((dataNWCrnr_x + widthOfDataVolume - ray.origin.x) * rayDirInv.x);
            }

//TODO -- avoid noise -- see other usage of LAMBDA_OFFSET below.
            lambda += LAMBDA_OFFSET;

            // Compute the new "rayPosition" from lambda.
            prevLambda  = lambda - stepSize;
            rayPosition = ray.origin + lambda * ray.direction;;
            prevRayPosition = rayPosition - rayPosIncrement;

            // Reset the current crossinglevel (otherwise isosurfaces that
            // should be open can be closed, e.g. if the last scalar value
            // before the empty-space-skip is below the isovalue and the first
            // scalar after re-entering the data volume is above...).
            float scalar = sampleDataAtPos(rayPosition);
            crossingLevelBack = crossingLevelFront = computeCrossingLevel(scalar);

            // Repeat statements from the initialization above: compute
            // iaccel, posExit and lambdaExitAccel at the new "rayPosition".
            float mixI = (rayPosition.x - dataNWCrnr_x) / deltaAccel.x;
            float mixJ = (dataExtent.dataNWCrnr.y - rayPosition.y) / deltaAccel.y;
            float mixK = (dataExtent.dataNWCrnr.z - rayPosition.z) / deltaAccel.z;
            iaccel = ivec3(int(mixI), int(mixJ), int(mixK));

            posExit = vec3(
                    dataNWCrnr_x
                    + (iaccel.x + (ray.direction.x >= 0.0 ? 1 : 0)) * deltaAccel.x,
                    dataExtent.dataNWCrnr.y
                    - (iaccel.y + (ray.direction.y >= 0.0 ? 0 : 1)) * deltaAccel.y,
                    dataExtent.dataNWCrnr.z
                    - (iaccel.z + (ray.direction.z >= 0.0 ? 0 : 1)) * deltaAccel.z);

            lambdaExitAccel = vec3(
                    (posExit.x - ray.origin.x) * rayDirInv.x,
                    (posExit.y - ray.origin.y) * rayDirInv.y,
                    (posExit.z - ray.origin.z) * rayDirInv.z);
            lambdaExitAccel = abs(lambdaExitAccel);
        }
    }
    
    // DEBUG.
    //rayColor = vec4(iaccel.z / 32., iaccel.y / 32., iaccel.x / 32., 1.);
#endif        
}


shader FSmain(in VStoFS Input, out vec4 fragColor : 0)
{
    vec3 h_gradient = initGradientSamplingStep(dataExtent);

    // Ray in parameterised form and parameter bounds for the render volume.
    Ray ray;
    vec2 lambdaNearFar = vec2(0., 0.);

    // Compute the bottom south-west corner and the top north-east corner
    // of the minimum bounding box of the region that is to be
    // rendered. For latitude and worldZ (both cannot be cyclic), use
    // the inner coordinates of user-specified bounding box and data
    // volume bounding box. Longitude is a special case (can be cyclic)
    // and is treated below (here, longitude values of the user-specified
    // bounding box are kept).
    vec3 renderRegionBottomSWCrnr = vec3(
            volumeTopNWCrnr.x,
            max(volumeBottomSECrnr.y, dataExtent.dataSECrnr.y),
            max(volumeBottomSECrnr.z, dataExtent.dataSECrnr.z));

    vec3 renderRegionTopNECrnr = vec3(
            volumeBottomSECrnr.x,
            min(volumeTopNWCrnr.y, dataExtent.dataNWCrnr.y),
            min(volumeTopNWCrnr.z, dataExtent.dataNWCrnr.z));

    if (shadowMode == SHADOWS_MAP)
    {
        // Render a shadow image of the scene: The ray is parallel to the
        // z-axis and casted downward through the data volume.
        ray.origin = Input.worldSpaceCoordinate;
        ray.direction = vec3(0., 0., -1.);

        lambdaNearFar.x = 0.;
        lambdaNearFar.y = volumeTopNWCrnr.z - volumeBottomSECrnr.z;

        shadowRay = true;
    }
    else if (isOrthographic)
    {
        ray.origin = Input.worldSpaceCoordinate;
        // Set ray position to the top of the render bounding box
        ray.origin.z = renderRegionTopNECrnr.z;
        ray.direction = vec3(0, 0, -1.);

        lambdaNearFar.x = 0;
        lambdaNearFar.y = volumeTopNWCrnr.z - volumeBottomSECrnr.z;
    }
    else
    {
        // Volume rendering mode: The ray direction is determined by the camera
        // position and the fragment position, both in world space.
        ray.origin = cameraPosition;
        ray.direction = normalize(Input.worldSpaceCoordinate - cameraPosition);

        // Compute the intersection points of the ray with this bounding box.
        // If the ray does not intersect with the box discard this fragment.
        bool rayIntersectsRenderVolume = rayBoxIntersection(
                ray, renderRegionBottomSWCrnr, renderRegionTopNECrnr, lambdaNearFar);
        if (!rayIntersectsRenderVolume)
        {
            discard;
        }

        // If the value for lambdaNear is < 0 the camera is located inside the
        // bounding box. It makes no sense to start the ray traversal behind the
        // camera, hence move lambdaNear to 0 to start in front of the camera.
        lambdaNearFar.x = max(lambdaNearFar.x, 0.01);

        // Treatment of longitude: The bounding box above uses the user-specified
        // east/west boundaries. In the longitude dimension, parts of the data
        // grid can be repeated in the display or shifted in location (example:
        // data is defined in the range -60..+40 degrees but displayed in the
        // range 0..360 degrees => the part from -60..0 needs to be displayed
        // at 300..360).
        // To avoid sampling at locations where no data values are defined,
        // the longitude boundaries of the bounding box need to be adjusted
        // (example continued: the ray intersects with the bounding box at
        // 270 deg, looking EASTward (ray.direction.x > 0). No data is defined
        // at 270 deg. The next cyclic entry position into the data volume
        // (-60 deg + N * 360 deg) in eastward direction is at 300 deg. Hence,
        // move the western boundary of the bounding box from 0 deg (the user
        // specified value) to 300 deg. (If the camera is looking WESTward,
        // the next EASTERN boundary is at 40 deg).

        // Compute the next shifted western boundary of the data volume
        // (dataNWCrnr_x) WEST of the computed intersection (entry) point
        // (given by lambdaNearFar.x).
        vec3 rayEntryIntoBBox = ray.origin + lambdaNearFar.x * ray.direction;
        float distanceRayEntryLonToDataWestLon = rayEntryIntoBBox.x - dataExtent.dataNWCrnr.x;
        int numShift360degrees = int(distanceRayEntryLonToDataWestLon / 360.);
        if (rayEntryIntoBBox.x < dataExtent.dataNWCrnr.x) numShift360degrees -= 1;
        float dataNWCrnr_x = dataExtent.dataNWCrnr.x + numShift360degrees * 360.;

        // Determine whether the current entry position is located inside or
        // outside of the data volume (inside = distance of entry to next
        // western data boundary needs to be smaller than width of data volume).
        distanceRayEntryLonToDataWestLon = rayEntryIntoBBox.x - dataNWCrnr_x;
        float widthOfDataVolume = dataExtent.dataSECrnr.x - dataExtent.dataNWCrnr.x;
        // If the grid is cyclic in longitude, add one grid width to avoid
        // a gap at the data volume boundary.
        if (dataExtent.gridIsCyclicInLongitude) widthOfDataVolume += dataExtent.deltaLon;

        if (distanceRayEntryLonToDataWestLon > widthOfDataVolume)
        {
            // Ray entry position is outside of data volume.

            if (ray.direction.x > 0)
            {
                // Camera is looking EAST: modify western boundary of bbox.
                renderRegionBottomSWCrnr.x = max(
                        volumeTopNWCrnr.x, dataNWCrnr_x + 360.);
            }
            else
            {
                // Camera is looking WEST: modify eastern boundary of bbox.
                renderRegionTopNECrnr.x = min(
                        volumeBottomSECrnr.x, dataNWCrnr_x + widthOfDataVolume);
            }

            // Again compute intersection points of ray and modified bounding
            // box.
            bool rayIntersectsRenderVolume = rayBoxIntersection(
                ray, renderRegionBottomSWCrnr, renderRegionTopNECrnr, lambdaNearFar);
            if (!rayIntersectsRenderVolume) discard;
        }

//TODO (mr, 17Nov2014): The sampling at the position computed from 
//      lambdaNearFar.x often fails, resulting in artefacts at the data volume
//      boundaries. This might be due to numerical inaccuracies that cause
//      lambda_near to be located slightly before the box. I move the ray
//      a bit foward here, but that is not a clean solution. INVESTIGATE THE
//      ISSUE!
        lambdaNearFar.x += LAMBDA_OFFSET;
       
        //shadowRay = false;
    }

    vec4 rayColor = vec4(0);
    vec3 rayPosition;
    vec3 crossingPosition;
    vec3 depthPosition;

    if ((renderingMode == RENDER_DVR) && (shadowMode != SHADOWS_MAP))
    {
        // For direct volume rendering (not necessary for shadow map generation
        // and also breaks shadow map code..):
        // Correct "far" lambda so that the raycaster terminates at the depth
        // that is stored in the depth buffer *before* this raycaster shader is
        // started. Read the current depth from the depth buffer texture that
        // mirrors the depth buffer (see CPP code, method
        // mirrorDepthBufferToTexture()), reconstruct its world space position
        // (assuming that the existing "back" fragment is located on the ray)
        // and compute its distance to the ray origin. Since ray.direction is
        // if unit length, the distance is the lambda value.
        // References:
        // https://forums.khronos.org/showthread.php/79310-Convert-to-Worldspace-from-depth-buffer
        // https://www.khronos.org/opengl/wiki/Compute_eye_space_from_window_space
        vec2 texCoordDepthBuffer = vec2(gl_FragCoord.x / viewPortWidth,
                                        gl_FragCoord.y / viewPortHeight);
        float depthFromBuffer = texture(depthBufferMirror, texCoordDepthBuffer, 0).x;
        // For DEBUG see: http://glampert.com/2014/01-26/visualizing-the-depth-buffer/
        // rayColor = vec4((depthFromBuffer-0.95)/0.05, 0., 0., 1.); break;
        vec4 posBackFragment = vec4(texCoordDepthBuffer.xy * 2. - 1.,
                                    depthFromBuffer * 2. - 1., 1.);
        posBackFragment = mvpMatrixInverted * posBackFragment;
        posBackFragment /= posBackFragment.w;
        float lambdaDepth = distance(posBackFragment.xyz, ray.origin);
        lambdaNearFar.y = min(lambdaNearFar.y, lambdaDepth);
    }

    // Invoke Raycaster.
    raycaster(h_gradient, ray, lambdaNearFar, rayColor, rayPosition,
              crossingPosition);

    // If the shadow mode is set to SHADOWS_VOLUME_AND_RAY cast an additional
    // ray upward through the volume to compute bottom surface shadow.
    if ((shadowMode == SHADOWS_VOLUME_AND_RAY)
        && (rayColor.a < 1.)
        && (rayPosition.z < volumeBottomSECrnr.z + stepSize))
    {
        shadowRay = true;

        if (rayColor.a == 0.)
            // Place this fragment at the bottom of the volume.
            depthPosition = vec3(rayPosition.xy, 0.1);
        else
            depthPosition = crossingPosition;

        // Cast a ray upward from the current position at the bottom of the
        // render volume.
        ray.origin = rayPosition;
        ray.direction = vec3(0., 0., 1.);
        lambdaNearFar.x = 0.;
        lambdaNearFar.y = volumeTopNWCrnr.z - volumeBottomSECrnr.z;

        raycaster(h_gradient, ray, lambdaNearFar, rayColor, rayPosition,
                  crossingPosition);

//TODO: Something wrong with alpha blending?
//         vec4 rayColorShadow = vec4(0);
//         raycaster(h_gradient, ray, lambdaNearFar, rayColorShadow, rayPosition,
//                   crossingPosition);
//         rayColor.rgb += (1 - rayColor.a) * rayColorShadow.a * rayColorShadow.rgb;
//         rayColor.a += (1 - rayColor.a) * rayColorShadow.a;

        if (rayColor.a == 0.) discard;
    }
    else
    {
        // No shadow is rendered -- discard fragment if no isosurface has
        // been crossed by the ray.
        if (rayColor.a == 0.) discard;
        depthPosition = crossingPosition;
    }

    // Compute correct depth value.
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
