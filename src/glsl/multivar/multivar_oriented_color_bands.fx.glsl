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

interface VSOutput
{
    vec3 vPosition; // Position in world space
    vec3 vNormal; // Orientation normal along line in world space
    vec3 vTangent; // Tangent of line in world space
    int vLineID; // number of line
    int vElementID; // number of line element (original line vertex index)
};

interface FSInput
{
    // Output to fragments
    vec3 fragWorldPos;
    vec3 fragNormal;
    vec3 fragTangent;
    // "Oriented Stripes"-specfic outputs
    flat int fragLineID; // Line index --> required for sampling from global buffer
    flat int fragElementID; // Actual per-line vertex index --> required for sampling from global buffer
#ifdef SUPPORT_LINE_DESATURATION
    flat uint desaturateLine;
#endif
};

/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(
        in vec3 vertexPosition : 0, in vec3 vertexLineNormal : 1, in vec3 vertexLineTangent : 2,
        in int lineID : 3, in int elementID : 4, out VSOutput outputs) {
    outputs.vPosition = vertexPosition;
    outputs.vNormal = normalize(vertexLineNormal);
    outputs.vTangent = normalize(vertexLineTangent);

    outputs.vLineID = lineID;
    outputs.vElementID = elementID;
}

/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

#define ORIENTED_COLOR_BANDS
#include "multivar_geometry_utils.glsl"

shader GSmain(in VSOutput inputs[], out FSInput outputs) {
    //if (inputs[0].vElementID, numObsPerTrajectory > renderTubesUpToIndex) return;

    vec3 currentPoint = inputs[0].vPosition;
    vec3 nextPoint = inputs[1].vPosition;

    vec3 circlePointsCurrent[NUM_SEGMENTS];
    vec3 circlePointsNext[NUM_SEGMENTS];

    vec3 vertexNormalsCurrent[NUM_SEGMENTS];
    vec3 vertexNormalsNext[NUM_SEGMENTS];

    vec3 normalCurrent = inputs[0].vNormal;
    vec3 tangentCurrent = inputs[0].vTangent;
    //vec3 binormalCurrent = cross(tangentCurrent, normalCurrent);
    vec3 normalNext = inputs[1].vNormal;
    vec3 tangentNext = inputs[1].vTangent;
    //vec3 binormalNext = cross(tangentNext, normalNext);

    vec3 tangent = normalize(nextPoint - currentPoint);

    // 1) Create tube circle vertices for current and next point
#if ORIENTED_RIBBON_MODE != 3 // ORIENTED_RIBBON_MODE_VARYING_RIBBON_WIDTH
    createTubeSegments(
            circlePointsCurrent, vertexNormalsCurrent, currentPoint, normalCurrent, tangentCurrent, lineWidth / 2.0);
    createTubeSegments(
            circlePointsNext, vertexNormalsNext, nextPoint, normalNext, tangentNext, lineWidth / 2.0);
#else
    // Sample ALL variables.
    float variables[MAX_NUM_VARIABLES];
    for (int varID = 0; varID < numVariables; varID++) {
        // 1) Sample variable from buffers.
        float variableValue;
        vec2 variableMinMax;
        const uint actualVarID = sampleActualVarID(varID);
        sampleVariableFromLineSSBO(inputs[0].vLineID, actualVarID, inputs[0].vElementID, variableValue, variableMinMax);

        // 2) Normalize value.
        variableValue = clamp((variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x), 0.0, 1.0);

        uint isDiverging = sampleIsDiverging(actualVarID);
        if (isDiverging > 0) {
            variableValue = abs(variableValue - 0.5) * 2.0;
        }

        variables[varID] = variableValue;
    }

    // Compute the sum of all variables
    float variablesSumTotal = 0.0;
    for (int varID = 0; varID < numVariables; varID++) {
        variablesSumTotal += variables[varID];
    }
    const float maxRadius = lineWidth * 0.5;
    const float minRadius = minRadiusFactor * maxRadius;
    const float lineRadius0 = mix(minRadius, maxRadius, variablesSumTotal / float(max(numVariables, 1)));


    // Sample ALL variables.
    for (int varID = 0; varID < numVariables; varID++) {
        // 1) Sample variables from buffers.
        float variableValue;
        vec2 variableMinMax;
        const uint actualVarID = sampleActualVarID(varID);
        sampleVariableFromLineSSBO(inputs[0].vLineID, actualVarID, inputs[0].vElementID, variableValue, variableMinMax);

        // 2) Normalize values.
        variableValue = clamp((variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x), 0.0, 1.0);

        uint isDiverging = sampleIsDiverging(actualVarID);
        if (isDiverging > 0) {
            variableValue = abs(variableValue - 0.5) * 2.0;
        }

        variables[varID] = variableValue;
    }

    // Compute the sum of all variables
    variablesSumTotal = 0.0;
    for (int varID = 0; varID < numVariables; varID++) {
        variablesSumTotal += variables[varID];
    }
    const float lineRadius1 = mix(minRadius, maxRadius, variablesSumTotal / float(max(numVariables, 1)));

    createTubeSegments(
            circlePointsCurrent, vertexNormalsCurrent, currentPoint, normalCurrent, tangentCurrent, lineRadius0);
    createTubeSegments(
            circlePointsNext, vertexNormalsNext, nextPoint, normalNext, tangentNext, lineRadius1);
#endif

    // 2) Create NDC AABB for stripe -> tube mapping
    // 2.1) Define orientation of local NDC frame-of-reference
    //vec4 currentPointNDC = mvpMatrix * vec4(currentPoint, 1);
    //currentPointNDC.xyz /= currentPointNDC.w;
    //vec4 nextPointNDC = mvpMatrix * vec4(nextPoint, 1);
    //nextPointNDC.xyz /= nextPointNDC.w;

    //vec2 ndcOrientation = normalize(nextPointNDC.xy - currentPointNDC.xy);
    //vec2 ndcOrientNormal = vec2(-ndcOrientation.y, ndcOrientation.x);
    // 2.2) Matrix to rotate every NDC point back to the local frame-of-reference
    // (such that tangent is aligned with the x-axis)
    //mat2 invMatNDC = inverse(mat2(ndcOrientation, ndcOrientNormal));

    // 2.3) Compute AABB in NDC space
    //vec2 bboxMin = vec2(10,10);
    //vec2 bboxMax = vec2(-10,-10);
    //vec2 tangentNDC = vec2(0);
    //vec2 normalNDC = vec2(0);
    //vec2 refPointNDC = vec2(0);

    //computeAABBFromNDC(circlePointsCurrent, invMatNDC, bboxMin, bboxMax, tangentNDC, normalNDC, refPointNDC);
    //computeAABBFromNDC(circlePointsNext, invMatNDC, bboxMin, bboxMax, tangentNDC, normalNDC, refPointNDC);

    // 3) Compute texture coordinates
    //computeTexCoords(vertexTexCoordsCurrent, circlePointsCurrent, invMatNDC, tangentNDC, normalNDC, refPointNDC);
    //computeTexCoords(vertexTexCoordsNext, circlePointsNext, invMatNDC, tangentNDC, normalNDC, refPointNDC);

    // 4) Emit the tube triangle vertices and attributes to the fragment shader
    outputs.fragElementID = inputs[0].vElementID;
    outputs.fragLineID = inputs[0].vLineID;

#ifdef SUPPORT_LINE_DESATURATION
    outputs.desaturateLine = 1 - lineSelectedArray[inputs[0].vLineID];
#endif

    for (int i = 0; i < NUM_SEGMENTS; i++) {
        int iN = (i + 1) % NUM_SEGMENTS;

        vec3 segmentPointCurrent0 = circlePointsCurrent[i];
        vec3 segmentPointNext0 = circlePointsNext[i];
        vec3 segmentPointCurrent1 = circlePointsCurrent[iN];
        vec3 segmentPointNext1 = circlePointsNext[iN];

        gl_Position = mvpMatrix * vec4(segmentPointCurrent0, 1.0);
        outputs.fragNormal = vertexNormalsCurrent[i];
        outputs.fragTangent = tangentCurrent;
        outputs.fragWorldPos = segmentPointCurrent0;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointCurrent1, 1.0);
        outputs.fragNormal = vertexNormalsCurrent[iN];
        outputs.fragTangent = tangentCurrent;
        outputs.fragWorldPos = segmentPointCurrent1;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext0, 1.0);
        outputs.fragNormal = vertexNormalsNext[i];
        outputs.fragTangent = tangentNext;
        outputs.fragWorldPos = segmentPointNext0;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext1, 1.0);
        outputs.fragNormal = vertexNormalsNext[iN];
        outputs.fragTangent = tangentNext;
        outputs.fragWorldPos = segmentPointNext1;
        EmitVertex();

        EndPrimitive();
    }
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
    const vec3 v = normalize(cameraPosition - inputs.fragWorldPos);
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
        float variableValue;
        vec2 variableMinMax;
        const uint actualVarID = sampleActualVarID(varID);
        sampleVariableFromLineSSBO(inputs.fragLineID, actualVarID, inputs.fragElementID, variableValue, variableMinMax);

        // 1.2) Normalize values.
        variableValue = clamp((variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x), 0.0, 1.0);

        uint isDiverging = sampleIsDiverging(actualVarID);
        if (isDiverging > 0) {
            variableValue = abs(variableValue - 0.5) * 2.0;
        }

        variables[varID] = variableValue;
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
    float variableValue;
    vec2 variableMinMax;
    const uint actualVarID = sampleActualVarID(varID);
    sampleVariableFromLineSSBO(inputs.fragLineID, actualVarID, inputs.fragElementID, variableValue, variableMinMax);

    // 3) Normalize values
    //variableValue = (variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);

    // 4) Determine variable color
    vec4 surfaceColor = determineColor(actualVarID, variableValue);

#if ORIENTED_RIBBON_MODE == 1 // ORIENTED_RIBBON_MODE_VARYING_BAND_WIDTH
    float symBandPos = abs(bandPos * 2.0 - 1.0);

    // 1.2) Normalize values.
    float variableValuePrime = clamp((variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x), 0.0, 1.0);

    uint isDiverging = sampleIsDiverging(actualVarID);
    if (isDiverging > 0) {
        variableValuePrime = abs(variableValuePrime - 0.5) * 2.0;
    }

    bandPos = (-symBandPos / variableValuePrime + 1.0) * 0.5;
#endif

    // 4.1) Draw black separators between single stripes.
    if (separatorWidth > 0) {
        drawSeparatorBetweenStripes(surfaceColor, bandPos, separatorWidth);
    }

#if ORIENTED_RIBBON_MODE == 1 // ORIENTED_RIBBON_MODE_VARYING_BAND_WIDTH
    if (symBandPos > variableValuePrime) {
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
    gs(430)=GSmain() : in(lines), out(triangle_strip, max_vertices = 128);
    fs(430)=FSmain();
};
