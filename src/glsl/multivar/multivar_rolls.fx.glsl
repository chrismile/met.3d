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
 ***                             UNIFORMS
 *****************************************************************************/

#include "transferfunction.glsl"
#include "multivar_global_variables.glsl"

// "Color Bands"-specific uniforms
uniform int rollWidth;
uniform bool mapTubeDiameter;
uniform float separatorWidth;


/*****************************************************************************
 ***                            INTERFACES
 *****************************************************************************/

interface VSOutput
{
    vec3 vPosition;// Position in world space
    vec3 vNormal;// Orientation normal along line in world space
    vec3 vTangent;// Tangent of line in world space
    int vVariableID; // variable index (for alternating variable indices per line curve)
    int vLineID;// number of line
    int vElementID;// number of line element (original line vertex index)
    int vElementNextID; // number of next line element (original next line vertex index)
    float vElementInterpolant; // curve parameter t along curve between element and next element
    vec4 lineVariable;
};

#if defined(GL_GEOMETRY_SHADER)
// Output to fragments
out vec3 fragWorldPos;
out vec3 fragNormal;
out vec3 fragTangent;
out vec2 fragTexCoord;
// "Rolls"-specfic inputs
flat out int fragVariableID;
flat out float fragVariableValue;
out float fragBorderInterpolant;
#elif defined(GL_FRAGMENT_SHADER)
// Output to fragments
in vec3 fragWorldPos;
in vec3 fragNormal;
in vec3 fragTangent;
in vec2 fragTexCoord;
// "Rolls"-specfic inputs
flat in int fragVariableID;
flat in float fragVariableValue;
in float fragBorderInterpolant;
#endif


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(
        in vec3 vertexPosition : 0, in vec3 vertexLineNormal : 1, in vec3 vertexLineTangent : 2,
        in vec4 multiVariable : 3, in vec4 variableDesc : 4, out VSOutput outputs) {
    int varID = int(multiVariable.w);
    const int lineID = int(variableDesc.y);
    const int elementID = int(variableDesc.x);

    outputs.vPosition = vertexPosition;
    outputs.vNormal = normalize(vertexLineNormal);
    outputs.vTangent = normalize(vertexLineTangent);

    outputs.vVariableID = varID;
    outputs.vLineID = lineID;
    outputs.vElementID = elementID;

    outputs.vElementNextID = int(variableDesc.z);
    outputs.vElementInterpolant = variableDesc.w;

    outputs.lineVariable = multiVariable;
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

#include "multivar_geometry_utils.glsl"

shader GSmain(in VSOutput inputs[]) {
    vec3 currentPoint = inputs[0].vPosition;
    vec3 nextPoint = inputs[1].vPosition;

    vec3 circlePointsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE];
    vec3 circlePointsNext[NUM_CIRCLE_POINTS_PER_INSTANCE];

    vec3 vertexNormalsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE];
    vec3 vertexNormalsNext[NUM_CIRCLE_POINTS_PER_INSTANCE];

    vec3 normalCurrent = inputs[0].vNormal;
    vec3 tangentCurrent = inputs[0].vTangent;
    vec3 binormalCurrent = cross(tangentCurrent, normalCurrent);
    vec3 normalNext = inputs[1].vNormal;
    vec3 tangentNext = inputs[1].vTangent;
    vec3 binormalNext = cross(tangentNext, normalNext);

    vec3 tangent = normalize(nextPoint - currentPoint);

    // 1) Sample variables at each tube roll
    const int instanceID = gl_InvocationID;
    const float mappedVarIDRatio = mod(float(inputs[0].vVariableID) / float(rollWidth), 1.0);
    const int mappedVarID = (inputs[0].vVariableID >= 0) ? inputs[0].vVariableID / rollWidth : -1;
    const int varID = sampleActualVarID(mappedVarID); // instanceID % numVariables for stripes
    const int elementID = inputs[0].vElementID;
    const int lineID = inputs[0].vLineID;

    //float variableValueOrig = 0;
    float variableValue = 0;
    vec2 variableMinMax = vec2(0);

//    vec4 varInfo = inputs[0].lineVariable;

    if (varID >= 0) {
        sampleVariableFromLineSSBO(lineID, varID, elementID, variableValue, variableMinMax);
        // Normalize value
        //variableValue = (variableValueOrig - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
//        variableValue = (varInfo.x - varInfo.y) / (varInfo.z - varInfo.y);
    }

    // 2.1) Radius mapping
    float curRadius = lineWidth / 2.0;
    float minRadius = minRadiusFactor * lineWidth / 2.0;
    float histOffset = 0.0;

    if (mapTubeDiameter) {
        minRadius = minRadiusFactor * lineWidth / 2.0;
        curRadius = minRadius;
        if (varID >= 0) {
            vec2 lineVarMinMax = vec2(0);
            if (mapTubeDiameterMode == 0) {
                lineVarMinMax = variableMinMax;
            } else {
                sampleVariableDistributionFromLineSSBO(lineID, varID, lineVarMinMax);
            }

            if (lineVarMinMax.x != lineVarMinMax.y) {
                float interpolant = (variableValue - lineVarMinMax.x) / (lineVarMinMax.y - lineVarMinMax.x);
//                interpolant = max(0.0, min(1.0, interpolant));
                curRadius = mix(minRadius, lineWidth / 2.0, interpolant);
            }
        }

    } else {
        if (varID < 0) {
            curRadius = minRadius;
        }
    }

    // 2) Create tube circle vertices for current and next point
    createPartialTubeSegments(
            circlePointsCurrent, vertexNormalsCurrent, currentPoint,
            normalCurrent, tangentCurrent, curRadius, curRadius, instanceID, 0, 0);
    createPartialTubeSegments(
            circlePointsNext, vertexNormalsNext, nextPoint,
            normalNext, tangentNext, curRadius, curRadius, instanceID, 0, 0);

    // 3) Draw Tube Front Sides
    fragVariableValue = variableValue;
    fragVariableID = varID;

    for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE - 1; i++) {
        int iN = (i + 1) % NUM_CIRCLE_POINTS_PER_INSTANCE;

        vec3 segmentPointCurrent0 = circlePointsCurrent[i];
        vec3 segmentPointNext0 = circlePointsNext[i];
        vec3 segmentPointCurrent1 = circlePointsCurrent[iN];
        vec3 segmentPointNext1 = circlePointsNext[iN];

        gl_Position = mvpMatrix * vec4(segmentPointCurrent0, 1.0);
        fragNormal = vertexNormalsCurrent[i];
        fragTangent = tangentCurrent;
        fragWorldPos = segmentPointCurrent0;
        fragBorderInterpolant = mappedVarIDRatio;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointCurrent1, 1.0);
        fragNormal = vertexNormalsCurrent[iN];
        fragTangent = tangentCurrent;
        fragWorldPos = segmentPointCurrent1;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext0, 1.0);
        fragNormal = vertexNormalsNext[i];
        fragTangent = tangentNext;
        fragWorldPos = segmentPointNext0;
        fragBorderInterpolant = mappedVarIDRatio + 1.0f / float(rollWidth);
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext1, 1.0);
        fragNormal = vertexNormalsNext[iN];
        fragTangent = tangentNext;
        fragWorldPos = segmentPointNext1;
        EmitVertex();

        EndPrimitive();
    }

    // Render lids

    // 3) Ŕender lids
    for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE - 1; i++) {
        fragNormal = normalize(-tangent);
        //! TODO compute tangent
        int iN = (i + 1) % NUM_CIRCLE_POINTS_PER_INSTANCE;

        vec3 segmentPointCurrent0 = circlePointsCurrent[i];
        vec3 segmentPointCurrent1 = circlePointsCurrent[iN];

        drawTangentLid(segmentPointCurrent0, currentPoint, segmentPointCurrent1);
    }

    for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE - 1; i++) {
        fragNormal = normalize(tangent);
        //! TODO compute tangent
        int iN = (i + 1) % NUM_CIRCLE_POINTS_PER_INSTANCE;

        vec3 segmentPointCurrent0 = circlePointsNext[i];
        vec3 segmentPointCurrent1 = circlePointsNext[iN];

        drawTangentLid(segmentPointCurrent0, segmentPointCurrent1, nextPoint);
    }

    // 3) Render partial circle lids
    drawPartialCircleLids(
            circlePointsCurrent, vertexNormalsCurrent,
            circlePointsNext, vertexNormalsNext,
            currentPoint, nextPoint, tangent);
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

#include "multivar_shading_utils.glsl"

shader FSmain(out vec4 fragColor) {
    // 1) Determine variable color
    vec4 surfaceColor = determineColor(fragVariableID, fragVariableValue);
    // 1.1) Draw black separators between single stripes.
    float varFraction = fragBorderInterpolant;
    if (separatorWidth > 0) {
        drawSeparatorBetweenStripes(surfaceColor, varFraction, separatorWidth);
    }

    ////////////
    // 2) Phong Lighting
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;

    vec4 color = computePhongLighting(
            surfaceColor, occlusionFactor, shadowFactor, fragWorldPos, fragNormal, fragTangent);

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
    gs(430)=GSmain() : in(lines, invocations = NUM_LINESEGMENTS), out(triangle_strip, max_vertices = 128);
    fs(430)=FSmain();
};
