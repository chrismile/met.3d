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
#if !defined NUM_SEGMENTS
#define NUM_SEGMENTS 9
#endif

#if !defined(NUM_LINESEGMENTS)
#define NUM_LINESEGMENTS 10
#endif

uniform float minRadiusFactor;

void createTubeSegments(
        inout vec3 positions[NUM_SEGMENTS], inout vec3 normals[NUM_SEGMENTS],
        in vec3 center, in vec3 normal, in vec3 tangent, in float curRadius) {
    const float theta = 2.0 * 3.1415926 / float(NUM_SEGMENTS);
    const float tangetialFactor = tan(theta); // opposite / adjacent
    const float radialFactor = cos(theta); // adjacent / hypotenuse

    vec3 binormal = cross(tangent, normal);
    mat3 matFrame = mat3(normal, binormal, tangent);

    vec2 position = vec2(curRadius, 0.0);
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        vec3 pointOnCricle = matFrame * vec3(position, 0.0);
        positions[i] = pointOnCricle + center;
        normals[i] = normalize(pointOnCricle);

        // Add the tangent vector and correct the position using the radial factor.
        vec2 circleTangent = vec2(-position.y, position.x);
        position += tangetialFactor * circleTangent;
        position *= radialFactor;
    }
}

void computeAABBFromNDC(
        inout vec3 positions[NUM_SEGMENTS], inout mat2 invMatNDC, inout vec2 bboxMin,
        inout vec2 bboxMax, inout vec2 tangentNDC, inout vec2 normalNDC, inout vec2 refPointNDC) {
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        vec4 pointNDC = mvpMatrix * vec4(positions[i], 1);
        pointNDC.xyz /= pointNDC.w;
        pointNDC.xy = invMatNDC * pointNDC.xy;

        bboxMin.x = min(pointNDC.x, bboxMin.x);
        bboxMax.x = max(pointNDC.x, bboxMax.x);
        bboxMin.y = min(pointNDC.y, bboxMin.y);
        bboxMax.y = max(pointNDC.y, bboxMax.y);
    }

    refPointNDC = vec2(bboxMin.x, bboxMax.y);
    normalNDC = vec2(0, bboxMin.y - bboxMax.y);
    tangentNDC = vec2(bboxMax.x - bboxMin.x, 0);
}

void computeTexCoords(
        inout vec2 texCoords[NUM_SEGMENTS], inout vec3 positions[NUM_SEGMENTS],
        inout mat2 invMatNDC, inout vec2 tangentNDC, inout vec2 normalNDC, inout vec2 refPointNDC) {
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        vec4 pointNDC = mvpMatrix * vec4(positions[i], 1);
        pointNDC.xyz /= pointNDC.w;
        pointNDC.xy = invMatNDC * pointNDC.xy;

        vec2 offsetNDC = pointNDC.xy - refPointNDC;
        texCoords[i] = vec2(dot(offsetNDC, normalize(tangentNDC)) / length(tangentNDC),
        dot(offsetNDC, normalize(normalNDC)) / length(normalNDC));
    }
}

#if !defined(NUM_CIRCLE_POINTS_PER_INSTANCE)
    #define NUM_CIRCLE_POINTS_PER_INSTANCE 2
#endif

void createPartialTubeSegments(
        inout vec3 positions[NUM_CIRCLE_POINTS_PER_INSTANCE], inout vec3 normals[NUM_CIRCLE_POINTS_PER_INSTANCE],
        in vec3 center, in vec3 normal, in vec3 tangent, in float curRadius, in float minRadius,
        in int varID, in float offset, in int vertexID) {
    float theta = 2.0 * 3.1415926 / float(NUM_LINESEGMENTS);
    float tangetialFactor = tan(theta); // opposite / adjacent
    float radialFactor = cos(theta); // adjacent / hypotenuse

    // Set position to the offset for ribbons (and rolls for varying radius)
    vec2 position = vec2(1.0, 0.0);

    for (int i = 0; i < varID; i++) {
        vec2 circleTangent = vec2(-position.y, position.x);
        position += tangetialFactor * circleTangent;
        position *= radialFactor;
    }

    // Shift positions for twisted rolls
    if (offset > 0) {
        float thetaOffset = offset * theta * vertexID;
        float tangetialFactorOffset = tan(thetaOffset); // opposite / adjacent
        float radialFactorOffset = cos(thetaOffset); // adjacent / hypotenuse

        for (int i = 0; i < 1; i++) {
            vec2 circleTangent = vec2(-position.y, position.x);
            position += tangetialFactorOffset * circleTangent;
            position *= radialFactorOffset;
        }
    }

    if (minRadius > 0) {
        position *= minRadius;
    } else {
        position *= curRadius;
    }


    vec3 binormal = cross(tangent, normal);
    mat3 matFrame = mat3(normal, binormal, tangent);

    // Subdivide theta in number of segments per instance
    theta /= float(NUM_CIRCLE_POINTS_PER_INSTANCE - 1);
    tangetialFactor = tan(theta); // opposite / adjacent
    radialFactor = cos(theta); // adjacent / hypotenuse

    for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE; i++) {
        vec3 pointOnCricle = matFrame * vec3(position, 0.0);
        positions[i] = pointOnCricle + center;
        normals[i] = normalize(pointOnCricle);

        // Add the tangent vector and correct the position using the radial factor.
        vec2 circleTangent = vec2(-position.y, position.x);
        position += tangetialFactor * circleTangent;
        position *= radialFactor;
    }

    if (minRadius > 0) {
        vec3 avgNormal = vec3(0);
        for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE; i++) {
            avgNormal += normals[i];
        }
        avgNormal = normalize(avgNormal);
        for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE; i++) {
            positions[i] += avgNormal * (curRadius - minRadius);
        }
    }
}

#if defined(GL_GEOMETRY_SHADER) && !defined(ORIENTED_COLOR_BANDS)
void drawTangentLid(in vec3 p0, in vec3 p1, in vec3 p2) {
    gl_Position = mvpMatrix * vec4(p0, 1.0);
    fragWorldPos = p0;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(p1, 1.0);
    fragWorldPos = p1;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(p2, 1.0);
    fragWorldPos = p2;
    EmitVertex();

    EndPrimitive();
}

void drawPartialCircleLids(
        inout vec3 circlePointsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE],
        inout vec3 vertexNormalsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE],
        inout vec3 circlePointsNext[NUM_CIRCLE_POINTS_PER_INSTANCE],
        inout vec3 vertexNormalsNext[NUM_CIRCLE_POINTS_PER_INSTANCE],
        in vec3 centerCurrent, in vec3 centerNext, in vec3 tangent) {
    vec3 pointCurFirst = circlePointsCurrent[0];
    vec3 pointNextFirst = circlePointsNext[0];

    vec3 pointCurLast = circlePointsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE - 1];
    vec3 pointNextLast = circlePointsNext[NUM_CIRCLE_POINTS_PER_INSTANCE - 1];

    // 1) First half
    gl_Position = mvpMatrix * vec4(pointCurFirst, 1.0);
    fragNormal = normalize(cross(normalize(pointCurFirst - centerCurrent), normalize(tangent)));
    fragTangent = tangent;//normalize(cross(tangent, fragNormal));
    fragWorldPos = pointCurFirst;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(pointNextFirst, 1.0);
    fragWorldPos = pointNextFirst;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(centerCurrent, 1.0);
    fragWorldPos = centerCurrent;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(centerNext, 1.0);
    fragWorldPos = centerNext;
    EmitVertex();

    EndPrimitive();

    // 2) Second half
    gl_Position = mvpMatrix * vec4(pointCurLast, 1.0);
    fragNormal = normalize(cross(normalize(tangent), normalize(pointCurLast - centerCurrent)));
    fragTangent = -tangent;//normalize(cross(tangent, fragNormal)); //! TODO compute actual tangent
    fragWorldPos = pointCurLast;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(centerCurrent, 1.0);
    fragWorldPos = centerCurrent;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(pointNextLast, 1.0);
    fragWorldPos = pointNextLast;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(centerNext, 1.0);
    fragWorldPos = centerNext;
    EmitVertex();

    EndPrimitive();
}
#endif

uniform int mapTubeDiameterMode;

float computeRadius(
        in int lineID, in int varID, in int elementID, in int elementNextID,
        in float minRadius, in float maxRadius, in float interpolant) {
    float curRadius = minRadius;
    float nextRadius = minRadius;

    vec2 lineVarMinMax = vec2(0);
    float variableValueOrig = 0;
    sampleVariableFromLineSSBO(lineID, varID, elementID , variableValueOrig, lineVarMinMax);
    if (mapTubeDiameterMode == 1) {
        sampleVariableDistributionFromLineSSBO(lineID, varID, lineVarMinMax);
    }
    if (lineVarMinMax.x != lineVarMinMax.y) {
        float interpolant = (variableValueOrig - lineVarMinMax.x) / (lineVarMinMax.y - lineVarMinMax.x);
        //                interpolant = max(0.0, min(1.0, interpolant));
        curRadius = mix(minRadius, maxRadius, interpolant);
    }

    sampleVariableFromLineSSBO(lineID, varID, elementNextID , variableValueOrig, lineVarMinMax);
    if (mapTubeDiameterMode == 1) {
        sampleVariableDistributionFromLineSSBO(lineID, varID, lineVarMinMax);
    }
    //            float variableNextValue = (variableNextValueOrig - variableNextMinMax.x) / (variableNextMinMax.y - variableNextMinMax.x);
    if (lineVarMinMax.x != lineVarMinMax.y) {
        float interpolant = (variableValueOrig - lineVarMinMax.x) / (lineVarMinMax.y - lineVarMinMax.x);
        //                interpolant = max(0.0, min(1.0, interpolant));
        nextRadius = mix(minRadius, maxRadius, interpolant);
    }

    return mix(curRadius, nextRadius, interpolant);
}
