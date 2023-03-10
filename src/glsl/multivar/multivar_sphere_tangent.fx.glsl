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

shader FSmain(in VSOutput inputs, out vec4 fragColor)
{
    vec3 entrancePoint = entrancePoints[inputs.instanceID].xyz;
    vec3 exitPoint = exitPoints[inputs.instanceID].xyz;
    LineElementIdData lineElementId = lineElementIds[inputs.instanceID];
    int fragmentLineID = lineElementId.lineId;
    float entranceIdx = lineElementId.entranceIdx;
    float exitIdx = lineElementId.exitIdx;

    const vec3 n = normalize(inputs.fragmentNormal);
    const vec3 v = orthographicModeEnabled == 1 ? cameraLookDirectionNeg : normalize(cameraPosition - inputs.fragmentPosition);
    const vec3 l = normalize(exitPoint - entrancePoint);

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
    vec3 up = normalize(cross(l, v));
    vec3 pn = normalize(cross(v, up));
    vec3 helperVecN = normalize(cross(pn, n));
    vec3 newN = normalize(cross(helperVecN, pn));
    vec3 crossProdVn = cross(v, newN);

    float ribbonPosition = length(crossProdVn);
    if (dot(l, crossProdVn) < 0.0) {
        ribbonPosition = -ribbonPosition;
    }
    // Normalize the ribbon position: [-1, 1] -> [0, 1].
    ribbonPosition = ribbonPosition / 2.0 + 0.5;

    float pattern0 = mod(ribbonPosition, 0.1) < 0.05 ? 1.0 : 0.0;
    float pattern1 = mod(param, 0.1) < 0.05 ? 1.0 : 0.0;
    float pattern = pattern0 == pattern1 ? 1.0 : 0.0;

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

    // 4.1) Adapt the separator width to the sphere to be independent of the radius.
    vec3 crossProdLn = cross(n, newN);
    float centerDist = length(crossProdLn);
    if (dot(l, crossProdVn) < 0.0) {
        centerDist = -centerDist;
    }
    float h = sqrt(1.0 - centerDist * centerDist);
    float separatorWidthSphere = separatorWidth * lineRadius / (sphereRadius * h);

    // 4.2) Draw black separators between single stripes.
    if (separatorWidth > 0) {
        drawSeparatorBetweenStripes(surfaceColor, bandPos, separatorWidthSphere);
    }

    // 5) Phong lighting
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;
    vec4 color = computePhongLightingSphere(
            surfaceColor, occlusionFactor, shadowFactor, inputs.fragmentPosition, n,
            abs((ribbonPosition - 0.5) * 2.0), separatorWidthSphere);

    //color = vec4(vec3(pattern), 1.0);

#ifdef SUPPORT_LINE_DESATURATION
    uint desaturateLine = 1 - lineSelectedArray[fragmentLineID];
    if (desaturateLine == 1) {
        color = desaturateColor(color);
    }
#endif

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
