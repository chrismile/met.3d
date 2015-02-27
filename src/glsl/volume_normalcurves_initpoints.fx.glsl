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

interface VStoFS
{
    smooth vec3 worldSpaceCoordinate;
    smooth vec2 screenCoords;
};


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
uniform sampler2D disturbTex;
uniform sampler3D dataVolume;
uniform sampler1D lonLatLevAxes;

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
uniform float   isoValueOuter;

uniform uint     bisectionSteps;

/*** ECMWF-specific ***/
//   ==============   //
uniform vec2    pToWorldZParams;
uniform bool    spatialCDFVisEnabled;
uniform float   numPressureLevels;

/*** Normal Curve-specific ***/
//   =====================   //
uniform vec3    lightDirection;
uniform float   boxDistance;
uniform vec3    gridSpacing; // to disturb ray position


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
// include log pressure levels volume sampling methods
#include "volume_logpressure_utils.glsl"
// defines subroutines and auxiliary ray-casting functions
#include "volume_sample_utils.glsl"


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec2 vertex0 : 0, in vec3 border0 : 1,
              out VStoFS Output)
{
    // pass vertex unhandled through vertex shader
    //world_position = vec3(vertex + 1 / 2.0,0);
    Output.worldSpaceCoordinate = border0;
    // ndc coordinates.xy | -1 = near plane | 1 = point
    gl_Position = vec4(vertex0,-1,1);
    Output.screenCoords = vertex0 * 0.5 + 0.5;
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

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

        bool condition = false;

        if (inverted)
        {
            condition = scalar <= isoValue;
        }
        else
        {
            condition = scalar >= isoValue;
        }

        if (condition)
        {
            rayPosition = rayCenterPosition;
            lambda = centerLambda;
        }
        else
        {
            prevRayPosition = rayCenterPosition;
            prevLambda = centerLambda;
        }
    }
}


shader FSmain(in VStoFS Input,
              out vec4 fragColor : 0,
              out vec4 fragColor2 : 1,
              out vec4 fragColor3 : 2,
              out vec4 fragColor4 : 3,
              out vec4 fragColor5 : 4,
              out vec4 fragColor6 : 5,
              out vec4 fragColor7 : 6,
              out vec4 fragColor8 : 7)
{
    // temporary color outputs;
    vec4 initPoints[8] = vec4[8](vec4(0.0), vec4(0.0),
                                 vec4(0.0), vec4(0.0),
                                 vec4(0.0), vec4(0.0),
                                 vec4(0.0), vec4(0.0));

    //initGlobalDataExtentVars();

    int currentCrossingLevel = 0;
    bool invertCondition = false;

    vec2 lambdaMinMax = vec2(0,0);

    vec2 disturb = textureLod(disturbTex, Input.screenCoords, 0).rg;

    vec3 right, up;

    if (lightDirection.x != 0)
    {
        right = vec3(0, gridSpacing.y, 0);
        up = vec3(0,0,gridSpacing.z);
    }
    else if (lightDirection.y != 0)
    {
        right = vec3(gridSpacing.x,0,0);
        up = vec3(0,0,gridSpacing.z);
    }
    else
    {
        right = vec3(gridSpacing.x,0,0);
        up = vec3(0,gridSpacing.y,0);
    }

    Ray ray;
    ray.origin = Input.worldSpaceCoordinate + (right * disturb.x + up * disturb.y);
    ray.direction = lightDirection;

    lambdaMinMax.x = 0.0;
    lambdaMinMax.y = boxDistance;

    float lambda = lambdaMinMax.x;
    vec3 rayPosition = ray.origin + lambdaMinMax.x * ray.direction;
    vec3 rayPosIncrement = stepSize * ray.direction;
    vec3 prevRayPosition = rayPosition;
    float prevLambda = lambda;

    while (lambda < lambdaMinMax.y)
    {
        float scalar = sampleDataAtPos(rayPosition);

        bool condition = false;
        if (invertCondition)
            condition = scalar <= isoValue;
        else
            condition = scalar >= isoValue;

        if (condition)
        {
            bisectionCorrection(rayPosition, lambda, prevRayPosition, prevLambda, invertCondition);

            initPoints[currentCrossingLevel] = vec4(rayPosition, 1);

            if (++currentCrossingLevel >= 8) { break; }

            invertCondition = !invertCondition;
        }

        prevLambda  = lambda;
        prevRayPosition = rayPosition;

        lambda += stepSize;
        rayPosition += rayPosIncrement;
    }

    fragColor = initPoints[0];
    fragColor2 = initPoints[1];
    fragColor3 = initPoints[2];
    fragColor4 = initPoints[3];
    fragColor5 = initPoints[4];
    fragColor6 = initPoints[5];
    fragColor7 = initPoints[6];
    fragColor8 = initPoints[7];
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(420)=VSmain();
    fs(420)=FSmain();
};
