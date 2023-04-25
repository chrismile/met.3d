/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020-2021 Christoph Neuhauser
**  Copyright 2020      Michael Kern
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

#define M_PI 3.14159265358979323846

//const int NUM_SEGMENTS = 8;
//const int MAX_NUM_VARIABLES = 20;
//#define USE_MULTI_VAR_TRANSFER_FUNCTION
//#define IS_MULTIVAR_DATA
//
//#define ORIENTED_RIBBON_MODE 0

/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

#include "transferfunction.glsl"
#include "multivar_global_variables.glsl"

// "Color bands"-specific uniforms
uniform float separatorWidth;

/*****************************************************************************
 ***                            INTERFACES
 *****************************************************************************/

struct LinePointData {
    vec3 linePosition;
    int lineID;
    vec3 lineNormal;
    int elementID;
    vec3 lineTangent;
    float padding;
};

layout (std430, binding = 0) readonly buffer LinePointDataBuffer {
    LinePointData linePoints[];
};

interface FSInput
{
    // Output to fragments
    vec3 fragWorldPos;
    vec3 fragNormal;
    vec3 fragTangent;
    // "Oriented Stripes"-specfic outputs
    flat int fragLineID; // Line index --> required for sampling from global buffer
    float fragElementID; // Actual per-line vertex index --> required for sampling from global buffer
#ifdef SUPPORT_LINE_DESATURATION
    flat uint desaturateLine;
#endif
};

/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(out FSInput outputs) {
    uint linePointIdx = gl_VertexID / NUM_SEGMENTS;
    uint circleIdx = gl_VertexID % NUM_SEGMENTS;
    LinePointData linePointData = linePoints[linePointIdx];

    vec3 lineCenterPosition = vec4(linePointData.linePosition, 1.0).xyz;
    vec3 normal = linePointData.lineNormal;
    vec3 tangent = linePointData.lineTangent;
    vec3 binormal = cross(linePointData.lineTangent, linePointData.lineNormal);
    mat3 tangentFrameMatrix = mat3(normal, binormal, tangent);

    float t = float(circleIdx) / float(NUM_SEGMENTS) * 2.0 * M_PI;
    float cosAngle = cos(t);
    float sinAngle = sin(t);

    vec3 localPosition = vec3(cosAngle, sinAngle, 0.0);
    vec3 localNormal = vec3(cosAngle, sinAngle, 0.0);
    vec3 vertexPosition = lineWidth * 0.5 * (tangentFrameMatrix * localPosition) + lineCenterPosition;
    vec3 vertexNormal = normalize(tangentFrameMatrix * localNormal);

    outputs.fragWorldPos = vertexPosition;
    outputs.fragNormal = vertexNormal;
    outputs.fragTangent = tangent;

    outputs.fragLineID = linePointData.lineID;
    outputs.fragElementID = float(linePointData.elementID);

#ifdef SUPPORT_LINE_DESATURATION
    outputs.desaturateLine = 1 - lineSelectedArray[linePointData.lineID];
#endif

    gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}

/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

#if ORIENTED_RIBBON_MODE == 1 // ORIENTED_RIBBON_MODE_VARYING_BAND_WIDTH
uniform vec4 bandBackgroundColor = vec4(0.5, 0.5, 0.5, 1.0);
#endif

#include "multivar_shading_utils.glsl"

shader FSmain(in FSInput inputs, out vec4 fragColor) {
    // 1) Determine variable ID along tube geometry
    const vec3 n = normalize(inputs.fragNormal);
    const vec3 v = orthographicModeEnabled == 1 ? cameraLookDirectionNeg : normalize(cameraPosition - inputs.fragWorldPos);
    const vec3 t = normalize(inputs.fragTangent);
    // Project v into plane perpendicular to t to get newV.
    vec3 helperVec = normalize(cross(t, v));
    vec3 newV = normalize(cross(helperVec, t));
    // Get the symmetric ribbon position (ribbon direction is perpendicular to line direction) between 0 and 1.
    // NOTE: len(cross(a, b)) == area of parallelogram spanned by a and b.
    vec3 crossProdVn = cross(newV, n);
    float ribbonPosition = length(crossProdVn);

    // Side note: We can also use the code below, as for a, b with length 1:
    // sqrt(1 - dot^2(a,b)) = len(cross(a,b))
    // Due to:
    // - dot(a,b) = ||a|| ||b|| cos(phi)
    // - len(cross(a,b)) = ||a|| ||b|| |sin(phi)|
    // - sin^2(phi) + cos^2(phi) = 1
    //ribbonPosition = dot(newV, n);
    //ribbonPosition = sqrt(1 - ribbonPosition * ribbonPosition);

    // Get the winding of newV relative to n, taking into account that t is the normal of the plane both vectors lie in.
    // NOTE: dot(a, cross(b, c)) = det(a, b, c), which is the signed volume of the parallelepiped spanned by a, b, c.
    if (dot(t, crossProdVn) < 0.0) {
        ribbonPosition = -ribbonPosition;
    }
    // Normalize the ribbon position: [-1, 1] -> [0, 1].
    ribbonPosition = ribbonPosition / 2.0 + 0.5;

    // Old code:
    //const int varID = int(floor(inputs.fragTexCoord.y * numVariables));
    //float bandPos = inputs.fragTexCoord.y * numVariables - float(varID);

#if ORIENTED_RIBBON_MODE == 0 || ORIENTED_RIBBON_MODE == 1 // ORIENTED_RIBBON_MODE_FIXED_BAND_WIDTH || ORIENTED_RIBBON_MODE_VARYING_BAND_WIDTH
    // 1) Compute the variable ID for this ribbon position and the variable fraction.
    const int varID = int(floor(ribbonPosition * numVariables));
    float bandPos = ribbonPosition * numVariables - float(varID);
#elif ORIENTED_RIBBON_MODE == 2 || ORIENTED_RIBBON_MODE == 3 // ORIENTED_RIBBON_MODE_VARYING_BAND_RATIO || ORIENTED_RIBBON_MODE_VARYING_RIBBON_WIDTH
    // Sample ALL variables.
    float variables[MAX_NUM_VARIABLES];
    for (int varID = 0; varID < numVariables; varID++) {
        // 1.1) Sample variables from buffers.
        vec2 variableMinMax;
        const uint actualVarID = sampleActualVarID(varID);
        const uint sensitivityOffset = sampleSensitivityOffset(actualVarID);
        float value = sampleValueLinear(
                inputs.fragLineID, actualVarID, inputs.fragElementID, sensitivityOffset, variableMinMax);

        // 1.2) Normalize value.
        value = clamp((value - variableMinMax.x) / (variableMinMax.y - variableMinMax.x), 0.0, 1.0);

        uint isDiverging = sampleIsDiverging(actualVarID);
        if (isDiverging > 0) {
            value = abs(value - 0.5) * 2.0;
        }

        // 1.3) Interpolate linearly.
        variables[varID] = value;
    }

    // Compute the sum of all variables
    float variablesSumTotal = 0.0;
    for (int varID = 0; varID < numVariables; varID++) {
        variablesSumTotal += variables[varID];
    }

    // Compute which color we are at.
    float variablesSumLast = 0.0, variablesSumCurrent = 0.0;
    int varID = 0;
    float bandPos = 1.0;
    for (varID = 0; varID < numVariables; varID++) {
        variablesSumCurrent += variables[varID] / variablesSumTotal;
        if (ribbonPosition < variablesSumCurrent) {
            bandPos = (ribbonPosition - variablesSumLast) / (variablesSumCurrent - variablesSumLast);
            break;
        }
        variablesSumLast = variablesSumCurrent;
    }
#endif

    // 2) Sample variables from buffers
    vec2 variableMinMax;
    const uint actualVarID = sampleActualVarID(varID);
    const uint sensitivityOffset = sampleSensitivityOffset(actualVarID);
    float variableValue = sampleValueLinear(
            inputs.fragLineID, actualVarID, inputs.fragElementID, sensitivityOffset, variableMinMax);

    // 3) Normalize values
    //variableValue = (variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
    //variableNextValue = (variableNextValue - variableNextMinMax.x) / (variableNextMinMax.y - variableNextMinMax.x);

    // 4) Determine variable color
    vec4 surfaceColor = determineColorFromValue(actualVarID, variableValue);

#if ORIENTED_RIBBON_MODE == 1 // ORIENTED_RIBBON_MODE_VARYING_BAND_WIDTH
    float symBandPos = abs(bandPos * 2.0 - 1.0);

    // 1.2) Normalize values.
    float interpolatedVariableValue = clamp((variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x), 0.0, 1.0);

    uint isDiverging = sampleIsDiverging(actualVarID);
    if (isDiverging > 0) {
        interpolatedVariableValue = abs(interpolatedVariableValue - 0.5) * 2.0;
    }

    bandPos = (-symBandPos / interpolatedVariableValue + 1.0) * 0.5;
#endif

    // 4.1) Draw black separators between single stripes.
    if (separatorWidth > 0) {
        drawSeparatorBetweenStripes(surfaceColor, bandPos, separatorWidth);
    }

#if ORIENTED_RIBBON_MODE == 1 // ORIENTED_RIBBON_MODE_VARYING_BAND_WIDTH
    if (symBandPos > interpolatedVariableValue) {
        surfaceColor = bandBackgroundColor;
    }
#endif

    ////////////
    // 5) Phong Lighting
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;

    vec4 color = computePhongLighting(
            surfaceColor, occlusionFactor, shadowFactor, inputs.fragWorldPos, inputs.fragNormal, inputs.fragTangent);

#ifdef SUPPORT_LINE_DESATURATION
    if (inputs.desaturateLine == 1) {
        color = desaturateColor(color);
    }
#endif

#ifdef USE_ROI
    bool shallDesaturate = false;
    const uint sensitivityOffsetA = sampleSensitivityOffset(roiVarAIndex);
    float variableValueA = sampleValueLinearNoMinMax(
            inputs.fragLineID, roiVarAIndex, inputs.fragElementID, sensitivityOffsetA);
    const uint sensitivityOffsetB = sampleSensitivityOffset(roiVarBIndex);
    float variableValueB = sampleValueLinearNoMinMax(
            inputs.fragLineID, roiVarBIndex, inputs.fragElementID, sensitivityOffsetB);
    if (variableValueA < roiVarALower || variableValueA > roiVarAUpper) {
        shallDesaturate = true;
    }
    if (variableValueB < roiVarBLower || variableValueB > roiVarBUpper) {
        shallDesaturate = true;
    }
    if (shallDesaturate) {
        color = desaturateColor(color);
    }
#endif

    if (color.a < 1.0/255.0) {
        discard;
    }

    fragColor = color;
}

/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(430)=VSmain();
    fs(430)=FSmain();
};
