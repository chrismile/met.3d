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
uniform vec3 cameraUp;

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

shader FSmain(in VSOutput inputs, out vec4 fragColor)
{
    vec3 entrancePoint = entrancePoints[inputs.instanceID].xyz;
    vec3 exitPoint = exitPoints[inputs.instanceID].xyz;
    LineElementIdData lineElementId = lineElementIds[inputs.instanceID];
    int fragmentLineID = lineElementId.lineId;
    float centerIdx = lineElementId.centerIdx;
    int fragElementID = int(round(centerIdx));

    //if (fragmentLineID == 5) {
    //    discard;
    //}

    const vec3 n = normalize(inputs.fragmentNormal);
    const vec3 v = normalize(cameraPosition - inputs.fragmentPosition);
    const vec3 l = normalize(exitPoint - entrancePoint);

    // 2) Project v and n into plane perpendicular to l to get newV and newN.
    /*vec3 helperVecV = normalize(cross(l, v));
    vec3 newV = normalize(cross(helperVecV, l));
    vec3 helperVecN = normalize(cross(l, n));
    vec3 newN = normalize(cross(helperVecN, l));
    vec3 crossProdVn = cross(newV, newN);*/

    //vec3 pn = normalize(cross(v, vec3(0.0, 0.0, 1.0)));

    vec3 upWorld = normalize(cross(l, v));
    vec3 pn = normalize(cross(v, upWorld));
    vec3 up = normalize(cross(pn, v));
    vec3 helperVecN = normalize(cross(pn, n));
    vec3 newN = normalize(cross(helperVecN, pn));
    vec3 crossProdVn = cross(v, newN);

    float ribbonPosition = length(crossProdVn);
    if (dot(l, crossProdVn) < 0.0) {
        ribbonPosition = -ribbonPosition;
    }
    // Normalize the ribbon position: [-1, 1] -> [0, 1].
    ribbonPosition = ribbonPosition / 2.0 + 0.5;

    vec3 nPlane = n - dot(n, v) * v;
    float nPlaneLength = length(nPlane);
    if (nPlaneLength > 1e-6) {
        nPlane /= nPlaneLength;
    }

    vec3 upWorld2 = normalize(cross(cameraUp, v));
    vec3 pn2 = normalize(cross(v, upWorld2));
    vec3 up2 = normalize(cross(pn2, v));
    float angle = atan(dot(cross(nPlane, up2), v), dot(nPlane, up2));
    //float angle = atan(length(cross(nPlane, up2)), dot(nPlane, up2));
    //float angle = atan(dot(cross(nPlane, up), v), dot(nPlane, up));
    angle += M_PI;
    angle = mod(angle + M_PI * 1.5, 2.0 * M_PI);
    angle /= 2.0 * M_PI;

#ifdef PIE_CHART_AREA
    // Sample ALL variables.
    float variables[MAX_NUM_VARIABLES];
    for (int varID = 0; varID < numVariables; varID++) {
        // 1.1) Sample variables from buffers.
        float variableValue;
        vec2 variableMinMax;
        const uint actualVarID = sampleActualVarID(varID);
        sampleVariableFromLineSSBO(fragmentLineID, actualVarID, fragElementID, variableValue, variableMinMax);

        // 1.2) Normalize values.
        variableValue = clamp((variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x), 0.0, 1.0);

        uint isDiverging = sampleIsDiverging(actualVarID);
        if (isDiverging > 0) {
            variableValue = abs(variableValue - 0.5) * 2.0;
        }

        // 1.3) Interpolate linearly.
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
        if (angle < variablesSumCurrent) {
            bandPos = (angle - variablesSumLast) / (variablesSumCurrent - variablesSumLast);
            break;
        }
        variablesSumLast = variablesSumCurrent;
    }

    // 3) Sample variables from buffers
    float variableValue;
    vec2 variableMinMax;
    const uint actualVarID = sampleActualVarID(varID);
    sampleVariableFromLineSSBO(fragmentLineID, actualVarID, fragElementID, variableValue, variableMinMax);
    variableValue = variableMinMax.y;
#endif

#ifdef PIE_CHART_COLOR
    // 3) Sample variables from buffers
    float variableValue;
    vec2 variableMinMax;
    const int varID = int(floor(angle * numVariables));
    float bandPos = angle * numVariables - float(varID);
    const uint actualVarID = sampleActualVarID(varID);
    sampleVariableFromLineSSBO(fragmentLineID, actualVarID, fragElementID, variableValue, variableMinMax);
#endif

    // 4) Determine variable color
    vec4 surfaceColor = determineColor(actualVarID, variableValue);

    float h = nPlaneLength;
    float separatorWidthSphere = separatorWidth * lineRadius / (sphereRadius * h * M_PI);

    // 4.2) Draw black separators between single stripes.
    if (numVariables > 1 && separatorWidth > 0) {
        drawSeparatorBetweenStripes(surfaceColor, bandPos, separatorWidthSphere);
    }

    // 5) Phong lighting
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;
    vec4 color = computePhongLightingSphere(
            surfaceColor, occlusionFactor, shadowFactor, inputs.fragmentPosition, n,
            abs((ribbonPosition - 0.5) * 2.0), separatorWidthSphere);

    //color = vec4(vec3(angle), 1.0);

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
