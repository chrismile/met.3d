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

// "Checkerboard"-specific uniforms
uniform bool mapTubeDiameter;
uniform float twistOffset;
uniform bool constantTwistOffset;
uniform int checkerboardWidth;
uniform int checkerboardHeight;
uniform int checkerboardIterator;
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
    vec4 lineVariable; // not needed anymore actually
    int vVertexID;
};

/*interface FSInput
{
// Output to fragments
    vec3 fragWorldPos;
    vec3 fragNormal;
    vec3 fragTangent;
    vec2 fragTexCoord;
// "Checkerboard"-specfic inputs
    flat int fragVariableID;
    flat float fragVariableValue;
    vec2 fragBorderInterpolant;
    flat float fragVariableNextValue;
    float fragElementInterpolant;
};*/

#if defined(GL_GEOMETRY_SHADER)
// Output to fragments
out vec3 fragWorldPos;
out vec3 fragNormal;
out vec3 fragTangent;
out vec2 fragTexCoord;
// "Checkerboard"-specfic inputs
flat out int fragVariableID;
flat out float fragVariableValue;
out vec2 fragBorderInterpolant;
flat out float fragVariableNextValue;
out float fragElementInterpolant;
#ifdef SUPPORT_LINE_DESATURATION
flat out uint desaturateLine;
#endif
#elif defined(GL_FRAGMENT_SHADER)
// Output to fragments
in vec3 fragWorldPos;
in vec3 fragNormal;
in vec3 fragTangent;
in vec2 fragTexCoord;
// "Checkerboard"-specfic inputs
flat in int fragVariableID;
flat in float fragVariableValue;
in vec2 fragBorderInterpolant;
flat in float fragVariableNextValue;
in float fragElementInterpolant;
#ifdef SUPPORT_LINE_DESATURATION
flat in uint desaturateLine;
#endif
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
    outputs.vVertexID = gl_VertexID;

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
    const float curAreaHeight = float(instanceID) / checkerboardHeight;
    const float curAreaWidth = float(inputs[0].vVertexID) / checkerboardWidth;
    const int varID = sampleActualVarID((int(curAreaHeight) + int(curAreaWidth) * checkerboardIterator) % numVariables);//inputs[0].vVertexID % 2;

//    const int varID = instanceID % numVariables; // for stripes
    const int elementID = inputs[0].vElementID;
    const int lineID = inputs[0].vLineID;

#ifdef SUPPORT_LINE_DESATURATION
    desaturateLine = 1 - lineSelectedArray[lineID];
#endif

    //float variableValueOrig = 0;
    float variableValue = 0;
    vec2 variableMinMax = vec2(0);

    //    vec4 varInfo = inputs[0].lineVariable;

    if (varID >= 0)     {
        const uint sensitivityOffset = sampleSensitivityOffset(varID);
        sampleVariableFromLineSSBO(lineID, varID, elementID, sensitivityOffset, variableValue, variableMinMax);
        // Normalize value
        //variableValue = (variableValueOrig - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
        //        variableValue = (varInfo.x - varInfo.y) / (varInfo.z - varInfo.y);
    }

    // 2.1) lineWidth / 2.0 mapping
    float minRadius = minRadiusFactor * lineWidth / 2.0;
    float curRadius = lineWidth / 2.0;
    float nextRadius = lineWidth / 2.0;

    if (mapTubeDiameter) {
        curRadius = minRadius;
        nextRadius = minRadius;
        if (varID >= 0) {
            curRadius = computeRadius(lineID, varID, elementID, inputs[0].vElementNextID,
                                      minRadius, lineWidth / 2.0, inputs[0].vElementInterpolant);
//            nextRadius = curRadius;
            nextRadius = computeRadius(lineID, varID, inputs[1].vElementID, inputs[1].vElementNextID,
                                      minRadius, lineWidth / 2.0, inputs[1].vElementInterpolant);
        }
    } else {
        if (varID < 0) {
            curRadius = minRadius;
            nextRadius = minRadius;
        }
    }

    // 2) Create tube circle vertices for current and next point
    createPartialTubeSegments(circlePointsCurrent, vertexNormalsCurrent, currentPoint,
                                normalCurrent, tangentCurrent, curRadius, -1, instanceID,
                                0.0, inputs[0].vVertexID);

    int vertexIDNext = (constantTwistOffset) ? inputs[0].vVertexID : inputs[1].vVertexID;

    createPartialTubeSegments(circlePointsNext, vertexNormalsNext, nextPoint,
                                normalNext, tangentNext, nextRadius, -1, instanceID,
                                0.0, vertexIDNext);


    // 3) Draw Tube Front Sides
    fragVariableValue = variableValue;
    fragVariableID = varID;

//    float test = float(instanceID) / checkerboardHeight;
//    int rem = int((test - floor(test)) * checkerboardHeight);
    float interpIncrement = 1.0 / (NUM_CIRCLE_POINTS_PER_INSTANCE - 1) * (1.0 / (checkerboardHeight));
    float curInterpolant = curAreaHeight - floor(curAreaHeight);

//    if (varID > 1) {  return; }

    float borderFracX = curAreaWidth - floor(curAreaWidth);
    float borderInc = 1.0 / float(checkerboardWidth);

    for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE - 1; i++) {
        int iN = (i + 1) % NUM_CIRCLE_POINTS_PER_INSTANCE;

        vec3 segmentPointCurrent0 = circlePointsCurrent[i];
        vec3 segmentPointNext0 = circlePointsNext[i];
        vec3 segmentPointCurrent1 = circlePointsCurrent[iN];
        vec3 segmentPointNext1 = circlePointsNext[iN];

        ////////////////////////
        // For linear interpolation: define next element and interpolant on curve
        int elementNextID = inputs[0].vElementNextID;

//        float variableNextValueOrig = 0;
        float variableNextValue = 0;

        if (elementNextID >= 0)
        {
            const uint sensitivityOffset = sampleSensitivityOffset(varID);
            sampleVariableFromLineSSBO(lineID, varID, elementNextID, sensitivityOffset, variableNextValue, variableMinMax);
            // Normalize value
            //fragVariableNextValue = (variableNextValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
            fragVariableNextValue = variableNextValue;
        }
//        fragVariableNextID = elementNextID;
        fragElementInterpolant = inputs[0].vElementInterpolant;

        ////////////////////////

        gl_Position = mvpMatrix * vec4(segmentPointCurrent0, 1.0);
        fragTangent = normalize(segmentPointNext0 - segmentPointCurrent0);//tangentCurrent;
        if (mapTubeDiameter) {
            vec3 intermediateBinormal = normalize(cross(vertexNormalsCurrent[i], fragTangent));
            fragNormal = normalize(cross(fragTangent, intermediateBinormal));//vertexNormalsCurrent[i];
        } else {
            fragNormal = vertexNormalsCurrent[i];
        }
        fragWorldPos = segmentPointCurrent0;
        fragBorderInterpolant = vec2(borderFracX, curInterpolant);
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointCurrent1, 1.0);
        fragTangent = normalize(segmentPointNext1 - segmentPointCurrent1);//tangentCurrent;
        if (mapTubeDiameter) {
            vec3 intermediateBinormal = normalize(cross(vertexNormalsCurrent[iN], fragTangent));
            fragNormal = normalize(cross(fragTangent, intermediateBinormal) + vertexNormalsCurrent[iN]);//vertexNormalsCurrent[iN];
        } else {
            fragNormal = vertexNormalsCurrent[iN];
        }
        fragWorldPos = segmentPointCurrent1;
        fragBorderInterpolant = vec2(borderFracX, curInterpolant + interpIncrement);
        EmitVertex();

        ////////////////////////
        // For linear interpolation: define next element and interpolant on curve
        elementNextID = inputs[0].vElementNextID;

        if (inputs[1].vElementInterpolant < inputs[0].vElementInterpolant)
        {
            fragElementInterpolant = 1.0f;
//            fragElementNextID = int(inputs[0].vElementNextID);
        }
        else
        {
//            fragElementNextID = int(inputs[1].vElementNextID);
            fragElementInterpolant = inputs[1].vElementInterpolant;
        }

//        fragVariableNextID = elementNextID;
        if (elementNextID >= 0) {
            const uint sensitivityOffset = sampleSensitivityOffset(varID);
            sampleVariableFromLineSSBO(lineID, varID, elementNextID, sensitivityOffset, variableNextValue, variableMinMax);
            // Normalize value
            //fragVariableNextValue = (variableNextValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
            fragVariableNextValue = variableNextValue;
        }
        ////////////////////////

        gl_Position = mvpMatrix * vec4(segmentPointNext0, 1.0);
        fragTangent = normalize(segmentPointNext0 - segmentPointCurrent0);//tangentNext;
        if (mapTubeDiameter) {
            vec3 intermediateBinormal = normalize(cross(vertexNormalsNext[i], fragTangent));
            fragNormal = normalize(cross(fragTangent, intermediateBinormal) + vertexNormalsNext[i]);//vertexNormalsNext[i];
        } else {
            fragNormal = vertexNormalsNext[i];
        }
        fragWorldPos = segmentPointNext0;
        fragBorderInterpolant = vec2(borderFracX + borderInc, curInterpolant);
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext1, 1.0);
        fragTangent = normalize(segmentPointNext1 - segmentPointCurrent1);//tangentNext;
        if (mapTubeDiameter) {
            vec3 intermediateBinormal = normalize(cross(vertexNormalsNext[iN], fragTangent));
            fragNormal = normalize(cross(fragTangent, intermediateBinormal) + vertexNormalsNext[iN]);//vertexNormalsNext[iN];
        } else {
            fragNormal = vertexNormalsNext[iN];
        }
        fragWorldPos = segmentPointNext1;
        fragBorderInterpolant = vec2(borderFracX + borderInc, curInterpolant + interpIncrement);
        EmitVertex();

        EndPrimitive();

        curInterpolant += interpIncrement;
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
    drawPartialCircleLids(circlePointsCurrent, vertexNormalsCurrent,
                        circlePointsNext, vertexNormalsNext,
                        currentPoint, nextPoint, tangent);
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

#include "multivar_shading_utils.glsl"

shader FSmain(out vec4 fragColor) {
    // 1) Determine variable color
//    vec4 surfaceColor = determineColor(fragVariableID, fragVariableValue);
    vec4 surfaceColor = determineColorLinearInterpolate(
            fragVariableID, fragVariableValue,
            fragVariableNextValue, fragElementInterpolant);
    // 1.1) Draw black separators between single stripes.
    float varFractionY = fragBorderInterpolant.y;
    float varFractionX = fragBorderInterpolant.x;
    if (separatorWidth > 0) {
//        vec4 c0 = surfaceColor;
//        vec4 c1 = surfaceColor;
//        drawSeparatorBetweenStripes(c0, varFractionX, separatorWidth);
//        drawSeparatorBetweenStripes(c1, varFractionY, separatorWidth);
//
//        surfaceColor = (c0 + c1) / 2;
        float distFactor = float(checkerboardHeight) / float(checkerboardWidth) * 0.35;
        drawSeparatorBetweenStripes(surfaceColor, varFractionX, separatorWidth * distFactor);
        drawSeparatorBetweenStripes(surfaceColor, varFractionY, separatorWidth);
    }

    ////////////
    // 2) Phong Lighting
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;

    vec4 color = computePhongLighting(
            surfaceColor, occlusionFactor, shadowFactor, fragWorldPos, fragNormal, fragTangent);

#ifdef SUPPORT_LINE_DESATURATION
    if (desaturateLine == 1) {
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
    gs(430)=GSmain() : in(lines, invocations = NUM_LINESEGMENTS), out(triangle_strip, max_vertices = 128);
    fs(430)=FSmain();
};
