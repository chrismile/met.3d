/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020-2023 Christoph Neuhauser
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
    vec3 lineNormal;
    vec3 fragTangent;
    // "Oriented Stripes"-specfic outputs
    flat int fragLineID; // Line index --> required for sampling from global buffer
    flat int fragElementID; // Actual per-line vertex index --> required for sampling from global buffer
    flat int fragElementNextID; // Actual next per-line vertex index --> for linear interpolation
    float fragElementInterpolant; // current number of curve parameter t (in [0;1]) within one line segment
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
    createTubeSegments(
            circlePointsCurrent, vertexNormalsCurrent, currentPoint, normalCurrent, tangentCurrent, lineWidth / 2.0);
    createTubeSegments(
            circlePointsNext, vertexNormalsNext, nextPoint, normalNext, tangentNext, lineWidth / 2.0);

    // 4) Emit the tube triangle vertices and attributes to the fragment shader
    outputs.fragLineID = inputs[0].vLineID;
    outputs.fragElementID = inputs[0].vElementID;
    outputs.fragElementNextID = inputs[1].vElementID;

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
        outputs.lineNormal = normalCurrent;
        outputs.fragTangent = tangentCurrent;
        outputs.fragWorldPos = segmentPointCurrent0;
        outputs.fragElementInterpolant = 0.0f;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointCurrent1, 1.0);
        outputs.fragNormal = vertexNormalsCurrent[iN];
        outputs.lineNormal = normalCurrent;
        outputs.fragTangent = tangentCurrent;
        outputs.fragWorldPos = segmentPointCurrent1;
        outputs.fragElementInterpolant = 0.0f;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext0, 1.0);
        outputs.fragNormal = vertexNormalsNext[i];
        outputs.lineNormal = normalNext;
        outputs.fragTangent = tangentNext;
        outputs.fragWorldPos = segmentPointNext0;
        outputs.fragElementInterpolant = 1.0f;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext1, 1.0);
        outputs.fragNormal = vertexNormalsNext[iN];
        outputs.lineNormal = normalNext;
        outputs.fragTangent = tangentNext;
        outputs.fragWorldPos = segmentPointNext1;
        outputs.fragElementInterpolant = 1.0f;
        EmitVertex();

        EndPrimitive();
    }
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

#include "multivar_shading_utils.glsl"

#define M_PI 3.14159265358979323846

shader FSmain(in FSInput inputs, out vec4 fragColor) {
    // 1) Determine variable ID along tube geometry
    const vec3 n = normalize(inputs.fragNormal);
    const vec3 ln = normalize(inputs.lineNormal);
    const vec3 t = normalize(inputs.fragTangent);
    // Get the ribbon position between 0 and 1.
    float angle = atan(dot(cross(ln, n), t), dot(ln, n));
    float ribbonPosition = mod(angle + 2.0 * M_PI * 1.5, 2.0 * M_PI) / (2.0 * M_PI);
    ribbonPosition *= 2.0;
    ribbonPosition = mod(ribbonPosition, 1.0);

    // 1) Compute the variable ID for this ribbon position and the variable fraction.
    const int varID = int(floor(ribbonPosition * numVariables));
    float bandPos = ribbonPosition * numVariables - float(varID);

    // 2) Sample variables from buffers
    float variableValue, variableNextValue;
    vec2 variableMinMax, variableNextMinMax;
    const uint actualVarID = sampleActualVarID(varID);
    const uint sensitivityOffset = sampleSensitivityOffset(actualVarID);
    sampleVariableFromLineSSBO(inputs.fragLineID, actualVarID, inputs.fragElementID, sensitivityOffset, variableValue, variableMinMax);
    sampleVariableFromLineSSBO(inputs.fragLineID, actualVarID, inputs.fragElementNextID, sensitivityOffset, variableNextValue, variableNextMinMax);

    // 3) Normalize values
    //variableValue = (variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
    //variableNextValue = (variableNextValue - variableNextMinMax.x) / (variableNextMinMax.y - variableNextMinMax.x);

    // 4) Determine variable color
    vec4 surfaceColor = determineColorLinearInterpolate(
            actualVarID, variableValue, variableNextValue, inputs.fragElementInterpolant);

    // 4.1) Draw black separators between single stripes.
    if (separatorWidth > 0) {
        drawSeparatorBetweenStripes(surfaceColor, bandPos, separatorWidth);
    }

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
