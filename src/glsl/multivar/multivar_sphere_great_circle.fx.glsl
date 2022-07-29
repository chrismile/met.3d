/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2021-2022 Christoph Neuhauser
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

// "Color bands"-specific uniforms
uniform float separatorWidth;
uniform float sphereRadius;
uniform float lineRadius;

/*****************************************************************************
 ***                            INTERFACES
 *****************************************************************************/

interface VSOutput
{
    vec3 spherePosition;
    vec3 fragmentPosition;
    vec3 fragmentNormal;
    flat int instanceID;
};

/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

layout(std430, binding = 10) readonly buffer SpherePositionsBuffer {
    vec4 spherePositions[];
};

shader VSmain(in vec3 vertexPosition : 0, in vec3 vertexNormal : 1, out VSOutput outputs)
{
    vec3 spherePosition = spherePositions[gl_InstanceID].xyz;
    vec3 position = sphereRadius * vertexPosition + spherePosition;
    gl_Position = mvpMatrix * vec4(position, 1.0);
    outputs.spherePosition = spherePosition;
    outputs.fragmentPosition = position;
    outputs.fragmentNormal = vertexNormal;
    outputs.instanceID = gl_InstanceID;
}

/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

#include "multivar_shading_utils.glsl"

/**
 * Computes the parametrized form of the closest point on a line segment.
 * See: http://geomalgorithms.com/a02-_lines.html
 *
 * @param p The position of the point.
 * @param l0 The first line point.
 * @param l1 The second line point.
 * @return A value satisfying l0 + RETVAL * (l1 - l0) = closest point.
 */
float getClosestPointOnLineSegmentParam(vec3 p, vec3 l0, vec3 l1) {
    vec3 v = l1 - l0;
    vec3 w = p - l0;
    float c1 = dot(v, w);
    if (c1 <= 0.0) {
        return 0.0;
    }
    float c2 = dot(v, v);
    if (c2 <= c1) {
        return 1.0;
    }
    return c1 / c2;
}

#define SQR(x) ((x)*(x))

float squareVec(vec3 v) {
    return SQR(v.x) + SQR(v.y) + SQR(v.z);
}

/**
 * Implementation of ray-sphere intersection (idea from A. Glassner et al., "An Introduction to Ray Tracing").
 * For more details see: https://education.siggraph.org/static/HyperGraph/raytrace/rtinter1.htm
 */
bool raySphereIntersection(
        vec3 rayOrigin, vec3 rayDirection, vec3 sphereCenter, float sphereRadius, out vec3 intersectionPosition) {
    float A = SQR(rayDirection.x) + SQR(rayDirection.y) + SQR(rayDirection.z);
    float B = 2.0 * (
            rayDirection.x * (rayOrigin.x - sphereCenter.x)
            + rayDirection.y * (rayOrigin.y - sphereCenter.y)
            + rayDirection.z * (rayOrigin.z - sphereCenter.z)
    );
    float C =
            SQR(rayOrigin.x - sphereCenter.x)
            + SQR(rayOrigin.y - sphereCenter.y)
            + SQR(rayOrigin.z - sphereCenter.z)
            - SQR(sphereRadius);

    float discriminant = SQR(B) - 4.0*A*C;
    if (discriminant < 0.0) {
        return false; // No intersection.
    }

    float discriminantSqrt = sqrt(discriminant);
    float t0 = (-B - discriminantSqrt) / (2.0 * A);

    // Intersection(s) behind the ray origin?
    intersectionPosition = rayOrigin + t0 * rayDirection;
    if (t0 >= 0.0) {
        return true;
    } else {
        float t1 = (-B + discriminantSqrt) / (2.0 * A);
        intersectionPosition = rayOrigin + t1 * rayDirection;
        if (t1 >= 0.0) {
            return true;
        }
    }

    return false;
}

layout(std430, binding = 11) readonly buffer EntrancePointsBuffer {
    vec4 entrancePoints[];
};

layout(std430, binding = 12) readonly buffer ExitPointsBuffer {
    vec4 exitPoints[];
};

struct LineElementIdData {
    float centerIdx;
    float entranceIdx;
    float exitIdx;
    int lineId;
};

layout(std430, binding = 13) readonly buffer LineElementIdBuffer {
    LineElementIdData lineElementIds[];
};

float computeCosAngleBetweenPlanes(vec3 n0, vec3 n1) {
    float numerator = abs(n0.x * n1.x + n0.x * n1.x + n0.x * n1.x);
    float denominator = sqrt(n0.x * n0.x + n0.y * n0.y + n0.z * n0.z) * sqrt(n1.x * n1.x + n1.y * n1.y + n1.z * n1.z);
    return numerator / denominator;
}

#define M_PI 3.14159265358979323846

shader FSmain(in VSOutput inputs, out vec4 fragColor)
{
    vec3 entrancePoint = entrancePoints[inputs.instanceID].xyz;
    vec3 exitPoint = exitPoints[inputs.instanceID].xyz;
    LineElementIdData lineElementId = lineElementIds[inputs.instanceID];
    int fragmentLineID = lineElementId.lineId;
    float entranceIdx = lineElementId.entranceIdx;
    float exitIdx = lineElementId.exitIdx;

    const vec3 n = normalize(inputs.fragmentNormal);
    const vec3 v = normalize(cameraPosition - inputs.fragmentPosition);
    const vec3 l = normalize(exitPoint - entrancePoint);

    //vec3 planeNormalZero = normalize(cross(l, cameraPosition - entrancePoint));
    //vec3 planeNormalZero = normalize(cross(l, inputs.spherePosition - entrancePoint));
    vec3 intersectionPosition;
    raySphereIntersection(
            inputs.spherePosition, normalize(cameraPosition - inputs.spherePosition),
            inputs.spherePosition, sphereRadius, intersectionPosition);
    vec3 planeNormalZero = normalize(cross(l, intersectionPosition - entrancePoint));
    vec3 planeNormalX = normalize(cross(l, inputs.fragmentPosition - entrancePoint));
    //float planeCosAngle = computeCosAngleBetweenPlanes(planeNormalZero, planeNormalX);

    // 1) Compute the closest point on the line segment spanned by the entrance and exit point.
    float param = getClosestPointOnLineSegmentParam(inputs.fragmentPosition, entrancePoint, exitPoint);
    float interpolatedIdx = mix(entranceIdx, exitIdx, param);
    int fragElementID = int(interpolatedIdx);
    int fragElementNextID = fragElementID + 1;
    float interpolationFactor = fract(interpolatedIdx);

    // 2) Project v and n into plane perpendicular to l to get newV and newN.
    /*vec3 helperVecV = normalize(cross(l, v));
    vec3 newV = normalize(cross(helperVecV, l));
    vec3 helperVecN = normalize(cross(l, n));
    vec3 newN = normalize(cross(helperVecN, l));
    vec3 crossProdVn = cross(newV, newN);*/

    //vec3 pn = normalize(cross(v, vec3(0.0, 0.0, 1.0)));


    // TODO
    vec3 crossProdVnCircle = cross(planeNormalZero, planeNormalX);
    float ribbonPosition = length(crossProdVnCircle);
    //ribbonPosition = asin(length(crossProdVnCircle)) / M_PI * 2.0;
    if (dot(l, crossProdVnCircle) < 0.0) {
        ribbonPosition = -ribbonPosition;
    }
    // Normalize the ribbon position: [-1, 1] -> [0, 1].
    ribbonPosition = ribbonPosition / 2.0 + 0.5;


    // 3) Sample variables from buffers
    float variableValue, variableNextValue;
    vec2 variableMinMax, variableNextMinMax;
    const int varID = int(floor(ribbonPosition * numVariables));
    float bandPos = ribbonPosition * numVariables - float(varID);
    const uint actualVarID = sampleActualVarID(varID);
    const uint sensitivityOffset = sampleSensitivityOffset(actualVarID);
    sampleVariableFromLineSSBO(
            fragmentLineID, actualVarID, fragElementID, sensitivityOffset, variableValue, variableMinMax);
    sampleVariableFromLineSSBO(
            fragmentLineID, actualVarID, fragElementNextID, sensitivityOffset, variableNextValue, variableNextMinMax);

    // 4) Determine variable color
    vec4 surfaceColor = determineColorLinearInterpolate(
            actualVarID, variableValue, variableNextValue, interpolationFactor);

    float separatorWidthCopy = separatorWidth;
    if (isnan(param)) {
        separatorWidthCopy = 0.0;
    }

    // 4.1) Adapt the separator width to the sphere to be independent of the radius.
    float ribbonPositionCentered = length(crossProdVnCircle);
    float h = 1.0 / sqrt(1.0 - ribbonPositionCentered * ribbonPositionCentered);
    float separatorWidthSphere = separatorWidthCopy * lineRadius / (sphereRadius * h);

    float sphereRadiusSq = sphereRadius * sphereRadius;
    float distanceEntrance = acos(dot(entrancePoint - inputs.spherePosition, inputs.fragmentPosition - inputs.spherePosition) / sphereRadiusSq);
    float distanceExit = acos(dot(exitPoint - inputs.spherePosition, inputs.fragmentPosition - inputs.spherePosition) / sphereRadiusSq);
    float distanceToPole = min(distanceEntrance, distanceExit);
    float distanceToEquator = M_PI * 0.5 - distanceToPole;
    separatorWidthSphere /= cos(distanceToEquator);// / M_PI * 2.0;
    if ((varID == 0 && bandPos < 0.5) || (varID == numVariables - 1 && bandPos > 0.5)) {
        separatorWidthSphere *= 0.5;
    }


    // 4.2) Draw black separators between single stripes.
    if (separatorWidthCopy > 0) {
        drawSeparatorBetweenStripes(surfaceColor, bandPos, separatorWidthSphere);
    }

    // 5.1) Phong lighting
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;
    vec4 color = computePhongLightingSphere(
            surfaceColor, occlusionFactor, shadowFactor, inputs.fragmentPosition, n,
            abs((ribbonPosition - 0.5) * 2.0), separatorWidthCopy * 0.005f);

    // 5.2) Draw outside stripe.
    if (separatorWidthCopy > 0) {
        vec3 up = normalize(cross(v, l));
        vec3 pn = normalize(cross(up, v));
        vec3 helperVecN = normalize(cross(pn, n));
        vec3 newN = normalize(cross(helperVecN, pn));
        vec3 crossProdVn = cross(v, newN);

        float ribbonPosition2 = length(crossProdVn);
        if (dot(l, crossProdVn) < 0.0) {
            ribbonPosition2 = -ribbonPosition2;
        }

        vec3 crossProdLn = cross(n, newN);
        float centerDist = length(crossProdLn);
        if (dot(l, crossProdVn) < 0.0) {
            centerDist = -centerDist;
        }

        float h = sqrt(1.0 - centerDist * centerDist);
        float separatorWidthSphere = separatorWidthCopy * lineRadius / (sphereRadius * h) * 0.5;

        drawSeparatorBetweenStripes(color, ribbonPosition2 / 2.0 + 0.5, separatorWidthSphere);
    }

    //color = vec4(vec3(pattern), 1.0);

#ifdef SUPPORT_LINE_DESATURATION
    uint desaturateLine = 1 - lineSelectedArray[fragmentLineID];
    if (desaturateLine == 1) {
        color = desaturateColor(color);
    }
#endif

    //color.rgb = vec3(ribbonPosition);
    //color.rgb = vec3(distanceMeet);

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
