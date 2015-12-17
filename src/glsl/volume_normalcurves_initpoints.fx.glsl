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

#extension GL_ARB_enhanced_layouts : enable

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

/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

// textures
// ========
uniform sampler1D transferFunction;
// contains precomputed z-coordinates
uniform sampler1D pressureTable; // PRESSURE_LEVELS
// contains hybrid coefficients a,b
uniform sampler1D hybridCoefficients; // HYBRID_SIGMA
// contains surface pressure at grid point (i, j)
uniform sampler2D surfacePressure; // HYBRID_SIGMA
uniform sampler2D pressureTexCoordTable2D; // HYBRID_SIGMA
uniform sampler2D distortTex;
uniform sampler3D dataVolume;
uniform sampler1D lonLatLevAxes;
// Denotes an imaginary grid along the bounding box, indicating if there has
// been found an init point at a certain grid cell. Can also be used as voxelized model.
layout(r32i) uniform iimage3D ghostGrid;

uniform bool    isoEnables[MAX_ISOSURFACES];
uniform float   isoValues[MAX_ISOSURFACES];
uniform int     numIsoValues;

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
uniform float   isoValue;

uniform uint     bisectionSteps;

/*** ECMWF-specific ***/
//   ==============   //
uniform vec2    pToWorldZParams;
uniform float   numPressureLevels;

/*** Normal Curve-specific ***/
//   =====================   //
uniform vec3    castingDirection; // The direction the rays are casted along.
uniform float   maxRayLength; // Maximal length of the ray from the origin.
uniform vec3    initWorldPos; // Initial world position of the ray.
uniform vec3    deltaGridX; // Distance between two rays in x-direction.
uniform vec3    deltaGridY; // Distance between two rays in y-direction.
uniform ivec2   maxNumRays; // Maximum number of rays in x/y-dimension.
uniform bool    doubleIntegration; // Indicates if double integration is activated.

uniform vec3    bboxMin; // Minimal bounding box boundaries.
uniform vec3    bboxMax; // Maximal bounding box boundaries.

// Atomic counter counting the number of init points.
layout(binding=0, offset=0) uniform atomic_uint counter;

// Normal curve vertex element.
struct NormalCurveVertex
{
    vec3 position;
    float value;
};

// Shader buffer storage object of the found init points.
layout (std430, binding=0) buffer InitPointBuffer
{
    NormalCurveVertex initPoints[];
};

/*****************************************************************************
 ***                             INCLUDES
 *****************************************************************************/

// include global definitions
#include "volume_defines.glsl"
// include global structs
#include "volume_global_structs_utils.glsl"
// include hybrid model volume sampling methods
#include "volume_hybrid_utils.glsl"
// include pressure levels volume sampling methods
#include "volume_pressure_utils.glsl"
// defines subroutines and auxiliary ray-casting functions
#include "volume_sample_utils.glsl"


/*****************************************************************************
 ***                          COMPUTE SHADER
 *****************************************************************************/

// Correct the position of any detected iso-surface by using the
// bisection algorithm.
void bisectionCorrection(inout vec3 rayPosition, inout float lambda,
                         in vec3 prevRayPosition, in float prevLambda,
                         in bool inverted)
{
    vec3 rayCenterPosition;
    float centerLambda;

    for (int i = 0; i < bisectionSteps; ++i)
    {
        rayCenterPosition = (rayPosition + prevRayPosition) / 2.0;
        centerLambda = (lambda + prevLambda) / 2.0;

        vec3 texCoords;
        ivec3 iTexCoords;
        float scalar;

        scalar = sampleDataAtPos(rayCenterPosition);

        // Condition test depends on from which direction the ray was shot.
        // This could also be implemented via crossing levels.
        bool condition = (inverted) ? scalar <= isoValue : scalar >= isoValue;

        if (condition)
        {
            // If central position is located after the iso-surface, then
            // set the ray position to the center of the interval.
            rayPosition = rayCenterPosition;
            lambda = centerLambda;
        }
        else
        {
            // Otherwise, set the previous ray position to the center.
            prevRayPosition = rayCenterPosition;
            prevLambda = centerLambda;
        }
    }
}


shader CSmain()
{
    // Determine the indices of the ray grid.
    const uint indexX = gl_GlobalInvocationID.x;
    const uint indexY = gl_GlobalInvocationID.y;

    // Make sure that there are no rays outside the defined ray grid.
    // So, the indices must not exceed the maximum number of rays.
    if (indexX >= maxNumRays.x || indexY >= maxNumRays.y) { return; }

    // Compute the texture coordinates of the distortion texture in range [0;1].
    const vec2 distortTexCoords = vec2(indexX / float(maxNumRays.x - 1),
                                       indexY / float(maxNumRays.y - 1));

    // Obtain the random values in x/y direction.
    const vec2 distortVec = textureLod(distortTex, distortTexCoords, 0).rg;
    // Compute world position by using the current indices and grid cell distances.
    vec3 worldPos = initWorldPos + indexX * deltaGridX + indexY * deltaGridY;
    // Distort the original world position.
    worldPos += deltaGridX * distortVec.x + deltaGridY * distortVec.y;

    // Create the corresponding ray.
    Ray ray;
    ray.origin = worldPos;
    ray.direction = normalize(castingDirection);

    // Define the maximum of detectable iso-surfaces along the ray.
    const uint MAX_CROSSINGS = 8;
    uint numCurrentCrossings = 0;
    const vec2 lambdaMinMax = vec2(0, maxRayLength);

    float lambda = lambdaMinMax.x;
    vec3 rayPosition = ray.origin + lambdaMinMax.x * ray.direction;
    vec3 rayPosIncrement = stepSize * ray.direction;
    vec3 prevRayPosition = rayPosition;
    float prevLambda = lambda;

    bool invertCondition = false;
    bool condition = false;

    // Traverse the grid across the ray and detect any intersection points.
    while (lambda < lambdaMinMax.y && numCurrentCrossings < MAX_CROSSINGS)
    {
        float scalar = sampleDataAtPos(rayPosition);

        // Check depends on if the ray position is inside/outside the iso-surface.
        if (invertCondition) { condition = scalar <= isoValue; }
        else { condition = scalar >= isoValue; }

        // If test passes then one intersection point is detected.
        if (condition)
        {
            numCurrentCrossings++;

            // Search for an exact intersection point along the surface.
            bisectionCorrection(rayPosition, lambda, prevRayPosition, prevLambda, invertCondition);

            scalar = sampleDataAtPos(rayPosition);

            // Use the ghost grid to check if there has been detected an intersection point
            // at the current position belonging to a cell of the ghost grid.
            // If the index is 1, then we've already found an intersection point and do not update
            // the init points buffer. Otherwise, increment the counter and add the current position
            // and scalar value.
            const ivec3 ghostGridSize = imageSize(ghostGrid) - ivec3(1,1,1);

            const vec3 boxExtent = abs(bboxMax - bboxMin);
            const vec3 ghostGridNormTexCoords = abs(rayPosition - bboxMin) / boxExtent;

            // Calculate the indices within the ghost grid
            ivec3 ghostGridTexCoords = ivec3(0,0,0);
            ghostGridTexCoords.x = int(ghostGridNormTexCoords.x * ghostGridSize.x);
            ghostGridTexCoords.y = int(ghostGridNormTexCoords.y * ghostGridSize.y);
            ghostGridTexCoords.z = int(ghostGridNormTexCoords.z * ghostGridSize.z);

            // Obtain the old value and only exchange the current value with 1
            // if the value was zero before.
            int ghostCount = imageAtomicCompSwap(ghostGrid, ghostGridTexCoords, 0, 1);

            // If there is no currently detected init point at the grid cell, add the current position
            // to the init points and set the cell as marked.
            if (ghostCount == 0)
            {
                // Increment the init point counter by one.
                uint index = atomicCounterIncrement(counter);

                // Write to the shader buffer.
                initPoints[index].position = rayPosition;
                initPoints[index].value = scalar;

                // If we are in double integration mode, add another entry to the list.
                if (doubleIntegration)
                {
                    index = atomicCounterIncrement(counter);

                    initPoints[index].position = rayPosition;
                    initPoints[index].value = scalar;
                }
            }

            invertCondition = !invertCondition;
        }

        prevLambda  = lambda;
        prevRayPosition = rayPosition;

        lambda += stepSize;
        rayPosition += rayPosIncrement;
    }
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    cs(430)=CSmain()  : in(local_size_x = 64, local_size_y = 2, local_size_z = 1);
};
