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

// Compute shader that computes normal curve line vertices from given
// initial points.


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
 ***                             UNIFORMS
 *****************************************************************************/

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

// Shader buffer storage object of computed normal curve vertices.
layout (std430, binding=1) buffer NormalCurveLineBuffer
{
    NormalCurveVertex lines[];
};

// matrices
// ========

uniform sampler3D dataVolume; // SIGMA_HYBRID_PRESSURE | PRESSURE_LEVELS
uniform sampler2D surfacePressure; // SIGMA_HYBRID_PRESSURE
uniform sampler1D hybridCoefficients; // SIGMA_HYBRID_PRESSURE
uniform sampler1D pressureTable; // FOR PRESSURE_LEVELS only
uniform sampler2D pressureTexCoordTable2D; // HYBRID_SIGMA
uniform sampler1D transferFunction;
uniform sampler1D lonLatLevAxes;

uniform bool    isoEnables[MAX_ISOSURFACES];
uniform float   isoValues[MAX_ISOSURFACES];
uniform int     numIsoValues;

uniform mat4    mvpMatrix;

// vectors
// =======

uniform vec3    volumeBottomSECrnr;
uniform vec3    volumeTopNWCrnr;

// scalars
// =======

uniform float   integrationStepSize; // Integration step size
uniform int     maxNumLineSegments; // maximal number of line segments
uniform int     maxNumLines; // maximal number of lines
uniform uint    bisectionSteps;

uniform int     indexOffset; // offset if both directions are chosen
uniform int     integrationMode; // integration direction (backwards: -1, forwards: 1)
uniform int     colorMode;
uniform int     abortCriterion;

// ECMWF-specific
// ==============
uniform float   maxCurveLength;
uniform vec2    pToWorldZParams;

// Normal Curve specific
// =====================

uniform int     maxNumSteps;
uniform float   isoValueBorderInner;
uniform float   isoValueBorderOuter;
uniform float   isoValueBorder;


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
 ***                              UTILS
 *****************************************************************************/

// Classical 4-steps Runge-Kutta method.
vec3 RK4Integration(in vec3 position, in float stepSize, in vec3 h_gradient)
{
    vec3 sample0, sample1, sample2, sample3;
    sample0 = computeGradient(position, h_gradient);
    sample1 = computeGradient(position + stepSize / 2.0 * sample0, h_gradient);
    sample2 = computeGradient(position + stepSize / 2.0 * sample1, h_gradient);
    sample3 = computeGradient(position + stepSize * sample2, h_gradient);

    return stepSize / 6.0 * (sample0 + 2 * (sample1 + sample2) + sample3);
}


// 3-steps Runge-Kutta method.
vec3 RK3Integration(in vec3 position, in float stepSize, in vec3 h_gradient)
{
    vec3 sample0, sample1, sample2;

    sample0 = computeGradient(position, h_gradient);
    sample1 = computeGradient(position + stepSize / 2.0 * sample0, h_gradient);
    sample2 = computeGradient(position - stepSize * sample0 + 2 * stepSize * sample1, h_gradient);

    return stepSize / 6.0 * (sample0 + 4 * sample1 + sample2);
}

// Correct the position of any detected iso-surface by using the
// bisection algorithm.
void bisectionCorrection(inout vec3 position,
                         in vec3 prevPosition,
                         inout float scalar,
                         in float threshold,
                         bool inverted)
{
    vec3 centerPosition;
    const uint NUM_BISECTION_STEPS = 5;
    float scalarCenter = scalar;
    float prevScalar = scalar;

    for (int i = 0; i < NUM_BISECTION_STEPS; ++i)
    {
        centerPosition = (position + prevPosition) / 2.0;
        scalarCenter = sampleDataAtPos(centerPosition);

        bool condition = (inverted) ? (scalarCenter <= threshold) : (scalarCenter >= threshold);

        if (condition)
        {
            position = centerPosition;
            scalar = scalarCenter;
        } else {
            prevPosition = centerPosition;
            prevScalar = scalarCenter;
        }
    }
}

// Function that uses the user-determined threshold to check if the normal curve
// computation has to be aborted or should be carried on.
bool checkThreshold(int steps, float curveLength, float scalar, in vec3 position)
{
    // Do not sample position outside the data volume.
    if (position.x > dataExtent.dataSECrnr.x) { return false; }
    if (position.x < dataExtent.dataNWCrnr.x) { return false; }
    if (position.y < dataExtent.dataSECrnr.y) { return false; }
    if (position.y > dataExtent.dataNWCrnr.y) { return false; }

    if (abortCriterion == 0) // Number of steps.
    {
        return steps < maxNumSteps;
    }
    else if (abortCriterion == 1) // Curve length.
    {
        return curveLength <= maxCurveLength;
    }
    else if (abortCriterion == 2) // Iso-Value.
    {
        return scalar >= isoValueBorder;
    }
    else if (abortCriterion == 3) // Second iso-value.
    {
        return scalar >= isoValueBorderOuter;
    }
    else if (abortCriterion == 4) // First iso-value.
    {
        return scalar <= isoValueBorderInner;
    }  
}

// Determines the color of one normal curve by using the curve length,
// the number of line segments (steps) or simply the scalar value.
float determineColorValue(int steps, float curveLength, float scalar)
{
    if (colorMode == 0) // Number of steps.
    {
        return 1.0 - min(1.0, max(0.0, float(steps) / maxNumSteps));
    }
    else if (colorMode == 1) // Curve length.
    {
        return min(1.0f, max(0.0, curveLength / maxCurveLength));
    }
    else // Scalar value.
    {
        return scalar;
    }
}

// Determine the threshold that has to be used to perform the
// bisection correction and thus to find the exact iso-surface position.
float determineThreshold()
{
    if (abortCriterion == 2) { return isoValueBorder; }
    else if (abortCriterion == 3) { return isoValueBorderOuter; }
    else if (abortCriterion == 4) { return isoValueBorderInner; }
    else return 0;
}


void computeLine(uint initPointIndex, int direction)
{
    // Compute index in normal curve line buffer.
    uint index = initPointIndex * (maxNumLineSegments + 2);

    // Initialize gradient step size.
    vec3 h_gradient = initGradientSamplingStep(dataExtent);

    // Initialize attributes per normal curve line.
    int numLineSegments = 0;
    float lineLength = 0;

    // Determine the entry position of the normal curve
    vec3 currentPos = initPoints[initPointIndex].position;
    vec3 prevPos = currentPos;
    vec3 gradient = vec3(0,0,0);
    vec3 prevGradient = vec3(0,0,0);

    float scalar = initPoints[initPointIndex].value;

    // Initialize first entry of line buffer
    lines[index].position = currentPos;
    lines[index].value = scalar;

    // Threshold to conduct bisection correction.
    const float threshold = determineThreshold();
    // Determine if we have to invert the scalar condition
    // depending on how we integrate in the data volume
    const bool inverted = (direction == 1) ? false : true;

    // Start integrating process and computation of normal curve.
    int i = 1;
    for (; i <= maxNumLineSegments; ++i)
    {
        // Determine the gradient by Runge-Kutta.
        gradient = RK4Integration(currentPos, integrationStepSize, h_gradient);
        currentPos += gradient * direction;
        // Compute the new line length.
        lineLength += length(gradient);
        numLineSegments++;
        scalar = sampleDataAtPos(currentPos);

        // Update the line buffer.
        lines[index + i].position = currentPos;
        lines[index + i].value = scalar;

        // If we exceeded a certain threshold then correct the last position and
        // update the scalar value.
        if (!checkThreshold(numLineSegments, lineLength, scalar, currentPos))
        {
            if (abortCriterion >= 2)
            {
                vec3 oldCurPos = currentPos;
                bisectionCorrection(currentPos, prevPos, scalar, threshold, inverted);

                lineLength -= length(oldCurPos - currentPos);

                lines[index + i].position = currentPos;
                lines[index + i].value = scalar;
            }

            break;
        }

        prevPos = currentPos;
        prevGradient = gradient;
    }

    // Determine the color of all line segments.
    for (int j = 0; j <= i; j++)
    {
        lines[index + j].value = determineColorValue(
                numLineSegments, lineLength, lines[index + j].value);
    }
}


/*****************************************************************************
 ***                           COMPUTE SHADER
 *****************************************************************************/

// Shader that integrates only in one certain direction
shader CSsingleIntegration()
{
    uint initPointIndex = gl_GlobalInvocationID.x;

    if (initPointIndex >= maxNumLines)
        return;

    computeLine(initPointIndex, integrationMode);
}

// Shader that integrates in both directions.
shader CSdoubleIntegration()
{
    uint initPointIndex = gl_GlobalInvocationID.x * 2;

    if (initPointIndex >= maxNumLines)
        return;

    computeLine(initPointIndex, 1);
    computeLine(initPointIndex + 1, -1);
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program SingleIntegration
{
    cs(430)=CSsingleIntegration() : in(local_size_x = 128);
};

program DoubleIntegration
{
    cs(430)=CSdoubleIntegration() : in(local_size_x = 128);
};
