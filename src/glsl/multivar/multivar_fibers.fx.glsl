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

// "Fibers"-specific uniforms
uniform float fiberRadius;
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
    int vLineID;// number of line
    int vElementID;// number of line element (original line vertex index)
    int vElementNextID; // number of next line element (original next line vertex index)
    float vElementInterpolant; // curve parameter t along curve between element and next element
};

#if defined(GL_GEOMETRY_SHADER)
// Output to fragments
out vec3 fragWorldPos;
out vec3 fragNormal;
out vec3 fragTangent;
out vec2 fragTexCoord;
// "Fibers"-specfic inputs
flat out int fragElementID; // Actual per-line vertex index --> required for sampling from global buffer
flat out int fragElementNextID; // Actual next per-line vertex index --> for linear interpolation
flat out int fragLineID; // Line index --> required for sampling from global buffer
flat out int fragVarID;
out float fragElementInterpolant; // current number of curve parameter t (in [0;1]) within one line segment
#elif defined(GL_FRAGMENT_SHADER)
// Output to fragments
in vec3 fragWorldPos;
in vec3 fragNormal;
in vec3 fragTangent;
in vec2 fragTexCoord;
// "Fibers"-specfic inputs
flat in int fragElementID; // Actual per-line vertex index --> required for sampling from global buffer
flat in int fragElementNextID; // Actual next per-line vertex index --> for linear interpolation
flat in int fragLineID; // Line index --> required for sampling from global buffer
flat in int fragVarID;
in float fragElementInterpolant; // current number of curve parameter t (in [0;1]) within one line segment
#endif


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(
        in vec3 vertexPosition : 0, in vec3 vertexLineNormal : 1, in vec3 vertexLineTangent : 2,
        in vec4 multiVariable : 3, in vec4 variableDesc : 4, out VSOutput outputs) {
    //    uint varID = gl_InstanceID % 6;
    const int lineID = int(variableDesc.y);
    const int elementID = int(variableDesc.x);

    outputs.vPosition = vertexPosition;
    outputs.vNormal = normalize(vertexLineNormal);
    outputs.vTangent = normalize(vertexLineTangent);

    outputs.vLineID = lineID;
    outputs.vElementID = elementID;

    outputs.vElementNextID = int(variableDesc.z);
    outputs.vElementInterpolant = variableDesc.w;
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

#include "multivar_geometry_utils.glsl"

shader GSmain(in VSOutput inputs[]) {
    vec3 currentPoint = inputs[0].vPosition;
    vec3 nextPoint = inputs[1].vPosition;

    vec3 circlePointsCurrent[NUM_SEGMENTS];
    vec3 circlePointsNext[NUM_SEGMENTS];

    vec3 vertexNormalsCurrent[NUM_SEGMENTS];
    vec3 vertexNormalsNext[NUM_SEGMENTS];

    vec2 vertexTexCoordsCurrent[NUM_SEGMENTS];
    vec2 vertexTexCoordsNext[NUM_SEGMENTS];

    vec3 normalCurrent = inputs[0].vNormal;
    vec3 tangentCurrent = inputs[0].vTangent;
    vec3 binormalCurrent = cross(tangentCurrent, normalCurrent);
    vec3 normalNext = inputs[1].vNormal;
    vec3 tangentNext = inputs[1].vTangent;
    vec3 binormalNext = cross(tangentNext, normalNext);

    vec3 tangent = normalize(nextPoint - currentPoint);

    // 0) Setup temp fiber offset on imaginary circle
    const int instanceID = gl_InvocationID;
    const int varID = sampleActualVarID(instanceID % maxNumVariables); // for stripes
    const int elementID = inputs[0].vElementID;
    const int lineID = inputs[0].vLineID;

    float thetaInc = 2 * 3.1415926 / (NUM_INSTANCES);
    float thetaCur = thetaInc * instanceID;

    currentPoint = currentPoint + (cos(thetaCur) * normalCurrent + sin(thetaCur) * binormalCurrent) * lineWidth / 2.0;
    nextPoint = nextPoint + (cos(thetaCur) * normalNext + sin(thetaCur) * binormalNext) * lineWidth / 2.0;

    // 0.5) Map diamter if enabled
    float minRadius = minRadiusFactor * fiberRadius;
    float curRadius = fiberRadius;
    float nextRadius = fiberRadius;

    if (mapTubeDiameter) {
        curRadius = minRadius;
        nextRadius = minRadius;

        if (varID >= 0) {
            curRadius = computeRadius(
                    lineID, varID, elementID, inputs[0].vElementNextID,
                    minRadius, fiberRadius, inputs[0].vElementInterpolant);
            //            nextRadius = curRadius;
            nextRadius = computeRadius(
                    lineID, varID, inputs[1].vElementID, inputs[1].vElementNextID,
                    minRadius, fiberRadius, inputs[1].vElementInterpolant);
        }
    }

    // 1) Create tube circle vertices for current and next point
    createTubeSegments(
            circlePointsCurrent, vertexNormalsCurrent, currentPoint,
            normalCurrent, tangentCurrent, curRadius);
    createTubeSegments(
            circlePointsNext, vertexNormalsNext, nextPoint,
            normalNext, tangentNext, nextRadius);

    // 2) Create NDC AABB for stripe -> tube mapping
    // 2.1) Define orientation of local NDC frame-of-reference
    vec4 currentPointNDC = mvpMatrix * vec4(currentPoint, 1);
    currentPointNDC.xyz /= currentPointNDC.w;
    vec4 nextPointNDC = mvpMatrix * vec4(nextPoint, 1);
    nextPointNDC.xyz /= nextPointNDC.w;

//    vec2 ndcOrientation = normalize(nextPointNDC.xy - currentPointNDC.xy);
//    vec2 ndcOrientNormal = vec2(-ndcOrientation.y, ndcOrientation.x);
//    // 2.2) Matrix to rotate every NDC point back to the local frame-of-reference
//    // (such that tangent is aligned with the x-axis)
//    mat2 invMatNDC = inverse(mat2(ndcOrientation, ndcOrientNormal));

//    // 2.3) Compute AABB in NDC space
//    vec2 bboxMin = vec2(10,10);
//    vec2 bboxMax = vec2(-10,-10);
//    vec2 tangentNDC = vec2(0);
//    vec2 normalNDC = vec2(0);
//    vec2 refPointNDC = vec2(0);
//
//    computeAABBFromNDC(circlePointsCurrent, invMatNDC, bboxMin, bboxMax, tangentNDC, normalNDC, refPointNDC);
//    computeAABBFromNDC(circlePointsNext, invMatNDC, bboxMin, bboxMax, tangentNDC, normalNDC, refPointNDC);

//    // 3) Compute texture coordinates
//    computeTexCoords(vertexTexCoordsCurrent, circlePointsCurrent,
//    invMatNDC, tangentNDC, normalNDC, refPointNDC);
//    computeTexCoords(vertexTexCoordsNext, circlePointsNext,
//    invMatNDC, tangentNDC, normalNDC, refPointNDC);

    // 4) Emit the tube triangle vertices and attributes to the fragment shader
    fragElementID = inputs[0].vElementID;
    fragLineID = inputs[0].vLineID;
    fragVarID = varID;

    for (int i = 0; i < NUM_SEGMENTS; i++) {
        int iN = (i + 1) % NUM_SEGMENTS;

        vec3 segmentPointCurrent0 = circlePointsCurrent[i];
        vec3 segmentPointNext0 = circlePointsNext[i];
        vec3 segmentPointCurrent1 = circlePointsCurrent[iN];
        vec3 segmentPointNext1 = circlePointsNext[iN];

        fragElementNextID = inputs[0].vElementNextID;
        fragElementInterpolant = inputs[0].vElementInterpolant;

        gl_Position = mvpMatrix * vec4(segmentPointCurrent0, 1.0);
        fragNormal = vertexNormalsCurrent[i];
        fragTangent = tangentCurrent;
        fragWorldPos = segmentPointCurrent0;
//        fragTexCoord = vertexTexCoordsCurrent[i];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointCurrent1, 1.0);
        fragNormal = vertexNormalsCurrent[iN];
        fragTangent = tangentCurrent;
        fragWorldPos = segmentPointCurrent1;
//        fragTexCoord = vertexTexCoordsCurrent[iN];
        EmitVertex();

        if (inputs[1].vElementInterpolant < inputs[0].vElementInterpolant) {
            fragElementInterpolant = 1.0f;
            fragElementNextID = int(inputs[0].vElementNextID);
        } else {
            fragElementInterpolant = inputs[1].vElementInterpolant;
            fragElementNextID = int(inputs[1].vElementNextID);
        }

        gl_Position = mvpMatrix * vec4(segmentPointNext0, 1.0);
        fragNormal = vertexNormalsNext[i];
        fragTangent = tangentNext;
        fragWorldPos = segmentPointNext0;
//        fragTexCoord = vertexTexCoordsNext[i];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext1, 1.0);
        fragNormal = vertexNormalsNext[iN];
        fragTangent = tangentNext;
        fragWorldPos = segmentPointNext1;
//        fragTexCoord = vertexTexCoordsNext[iN];
        EmitVertex();

        EndPrimitive();
    }
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

#include "multivar_shading_utils.glsl"

shader FSmain(out vec4 fragColor) {
    float variableValue;
    vec2 variableMinMax;

    float variableNextValue;
    vec2 variableNextMinMax;

    const int varID = fragVarID;
    // 1) Determine variable ID along tube geometry
//    const int varID = int(floor(fragTexCoord.y * numVariables));
//    float varFraction = fragTexCoord.y * numVariables - float(varID);

//
//    // 2) Sample variables from buffers
    sampleVariableFromLineSSBO(fragLineID, varID, fragElementID, variableValue, variableMinMax);
    sampleVariableFromLineSSBO(fragLineID, varID, fragElementNextID, variableNextValue, variableNextMinMax);
//
//    // 3) Normalize values
    //variableValue = (variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
    //variableNextValue = (variableNextValue - variableNextMinMax.x) / (variableNextMinMax.y - variableNextMinMax.x);
//
//    // 4) Determine variable color
    vec4 surfaceColor = determineColorLinearInterpolate(
            varID, variableValue, variableNextValue, fragElementInterpolant);
//    // 4.1) Draw black separators between single stripes.
//    if (separatorWidth > 0)
//    {
//        drawSeparatorBetweenStripes(surfaceColor, varFraction, separatorWidth);
//    }


    ////////////
    // 5) Phong Lighting
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;

    vec4 color = computePhongLighting(surfaceColor, occlusionFactor, shadowFactor,
    fragWorldPos, fragNormal, fragTangent);

//    vec4 color = determineColor(fragVarID, 100);
//    vec4 color = vec4(1, 0, 0, 1);

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
    gs(430)=GSmain() : in(lines, invocations = NUM_INSTANCES), out(triangle_strip, max_vertices = 128);
    fs(430)=FSmain();
};
