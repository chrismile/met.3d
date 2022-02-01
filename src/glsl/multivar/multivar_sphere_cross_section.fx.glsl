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
 * For more details see: https://www.siggraph.org//education/materials/HyperGraph/raytrace/rtinter1.htm
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

#define M_PI 3.14159265358979323846

bool rayPlaneIntersection(vec3 rayOrigin, vec3 rayDirection, vec4 plane, out float t) {
    float ln = dot(plane.xyz, rayDirection);
    if (abs(ln) < 1e-5) {
        // Plane and ray are parallel.
        return false;
    } else {
        float pos = dot(plane.xyz, rayOrigin) + plane.w;
        t = -pos / ln;
        return t >= 0.0;
    }
}

shader FSmain(in VSOutput inputs, out vec4 fragColor)
{
    vec3 entrancePoint = entrancePoints[inputs.instanceID].xyz;
    vec3 exitPoint = exitPoints[inputs.instanceID].xyz;
    if (length(entrancePoint - inputs.spherePosition) < 1e-5) {
        entrancePoint += 1e-3 * normalize(inputs.spherePosition - exitPoint);
    }
    if (length(exitPoint - inputs.spherePosition) < 1e-5) {
        exitPoint += 1e-3 * normalize(entrancePoint - inputs.spherePosition);
    }
    LineElementIdData lineElementId = lineElementIds[inputs.instanceID];
    int fragmentLineID = lineElementId.lineId;
    //float entranceIdx = lineElementId.entranceIdx;
    //float exitIdx = lineElementId.exitIdx;
    float centerIdx = lineElementId.centerIdx;
    int fragElementID = int(round(centerIdx));

    const vec3 n = normalize(inputs.fragmentNormal);
    const vec3 v = normalize(cameraPosition - inputs.fragmentPosition);
    const vec3 l = normalize(exitPoint - entrancePoint);
    const vec3 vp = inverse(vMatrix)[2].xyz;

    //vec3 intersectionPosition;
    //raySphereIntersection(inputs.spherePosition, normalize(cameraPosition - inputs.spherePosition), inputs.spherePosition, sphereRadius, intersectionPosition);

    vec3 entranceDir = normalize(entrancePoint - inputs.spherePosition);
    vec3 entranceFront;
    raySphereIntersection(entrancePoint + lineRadius * v, entranceDir, inputs.spherePosition, sphereRadius, entranceFront);

    vec3 exitDir = normalize(inputs.spherePosition - entrancePoint);
    vec3 exitFront;
    raySphereIntersection(exitPoint + lineRadius * v, exitDir, inputs.spherePosition, sphereRadius, exitFront);

    //const vec3 right = normalize(cross(l, v));
    const vec3 right = normalize(cross(l, vp));

    vec4 planeEntrance, planeExit;
    planeEntrance.xyz = normalize(cross(right, inputs.spherePosition - entranceFront));
    planeEntrance.w = -dot(planeEntrance.xyz, inputs.spherePosition);
    planeExit.xyz = -normalize(cross(right, inputs.spherePosition - exitFront));
    planeExit.w = -dot(planeExit.xyz, inputs.spherePosition);

    bool isCulled0 = dot(planeEntrance.xyz, inputs.fragmentPosition) + planeEntrance.w > 0.0f;
    bool isCulled1 = dot(planeExit.xyz, inputs.fragmentPosition) + planeExit.w > 0.0f;

    float separatorWidthSphere = separatorWidth * lineRadius / sphereRadius;
    float centerDist = 1.0 - 2.0 * separatorWidthSphere;
    vec3 surfaceNormal = n;

    if (isCulled0 && isCulled1) {
        vec3 rayDirection = -v;
        float t0 = 1e6, t1 = 1e6;
        bool intersectsPlaneEntrance = rayPlaneIntersection(cameraPosition, rayDirection, planeEntrance, t0);
        bool intersectsPlaneExit = rayPlaneIntersection(cameraPosition, rayDirection, planeExit, t1);
        //intersectsPlaneEntrance = false;
        //t0 = 1e6;
        if (intersectsPlaneEntrance || intersectsPlaneExit) {
            float t = min(t0, t1);
            vec3 hitPos = cameraPosition + t * rayDirection;
            centerDist = length(hitPos - inputs.spherePosition);
            //fragColor = vec4(vec3(centerDist <= sphereRadius ? 1.0 : 0.0), 1.0);
            //return;
            if (centerDist <= sphereRadius) {
                if (intersectsPlaneEntrance && t0 < t1) {
                    surfaceNormal = planeEntrance.xyz;
                    if (dot(surfaceNormal, v) < 0.0) {
                        surfaceNormal = -surfaceNormal;
                    }
                } else if (intersectsPlaneExit && t1 < t0) {
                    surfaceNormal = planeExit.xyz;
                    if (dot(surfaceNormal, v) < 0.0) {
                        surfaceNormal = -surfaceNormal;
                    }
                }
                centerDist = centerDist / sphereRadius;
            } else {
                discard;
                //centerDist = 1.0 - 2.0 * separatorWidthSphere;
            }
        }
    }


    // 3) Sample variables from buffers
    float variableValue;
    vec2 variableMinMax;
    const int varID = int(floor(centerDist * numVariables));
    float bandPos = centerDist * numVariables - float(varID);
    const uint actualVarID = sampleActualVarID(varID);
    sampleVariableFromLineSSBO(fragmentLineID, actualVarID, fragElementID, variableValue, variableMinMax);

    // 4) Determine variable color
    vec4 surfaceColor = determineColor(actualVarID, variableValue);

    // 4.2) Draw black separators between single stripes.
    if (separatorWidth > 0) {
        drawSeparatorBetweenStripes(surfaceColor, bandPos, separatorWidthSphere);
    }

    // 5.1) Phong lighting
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;
    vec4 color = computePhongLightingSphere(
            surfaceColor, occlusionFactor, shadowFactor, inputs.fragmentPosition, surfaceNormal,
            /* abs((ribbonPosition - 0.5) * 2.0) */ 0.5, separatorWidth * 0.005f);

    // 5.2) Draw outside stripe.
    if (separatorWidth > 0) {
        vec3 up = normalize(cross(l, v));
        vec3 pn = normalize(cross(v, up));
        vec3 helperVecN = normalize(cross(pn, n));
        vec3 newN = normalize(cross(helperVecN, pn));
        vec3 crossProdVn = cross(v, newN);

        float ribbonPosition2 = length(crossProdVn);
        if (dot(l, crossProdVn) < 0.0) {
            ribbonPosition2 = -ribbonPosition2;
        }

        float h = sqrt(1.0 - centerDist * centerDist);
        float separatorWidthSphere = separatorWidth * lineRadius / (sphereRadius * h);

        drawSeparatorBetweenStripes(color, ribbonPosition2 / 2.0 + 0.5, separatorWidthSphere * 0.5);
    }

    //color = vec4(vec3(pattern), 1.0);

#ifdef SUPPORT_LINE_DESATURATION
    uint desaturateLine = 1 - lineSelectedArray[fragmentLineID];
    if (desaturateLine == 1) {
        color = desaturateColor(color);
    }
#endif

    //color.rgb = vec3(ribbonPosition);

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
