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
uniform float lineRadius;
uniform float rollsRadius;
uniform float rollsWidth;

/*****************************************************************************
 ***                            INTERFACES
 *****************************************************************************/

interface VSOutput
{
    vec3 fragmentPosition;
    vec3 fragmentNormal;
    vec3 fragmentTangent;
    float fragmentRollPosition;
    flat int fragmentLineID;
    float fragmentLinePointIdx;
    flat uint fragmentVariableIdAndIsCap;
};

/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

layout(std430, binding = 10) readonly buffer SpherePositionsBuffer {
    vec4 spherePositions[];
};

shader VSmain(
        in vec3 vertexPosition : 0, in vec3 vertexNormal : 1, in vec3 vertexTangent : 2,
        in float vertexRollPosition : 3, in int vertexLineID : 4, in float vertexLinePointIdx : 5,
        in uint vertexVariableIdAndIsCap : 6,
        out VSOutput outputs)
{
    gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
    outputs.fragmentPosition = vertexPosition;
    outputs.fragmentNormal = vertexNormal;
    outputs.fragmentTangent = vertexTangent;
    outputs.fragmentRollPosition = vertexRollPosition;
    outputs.fragmentLineID = vertexLineID;
    outputs.fragmentLinePointIdx = vertexLinePointIdx;
    outputs.fragmentVariableIdAndIsCap = vertexVariableIdAndIsCap;
}

/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

#include "multivar_shading_utils.glsl"

shader FSmain(in VSOutput inputs, out vec4 fragColor)
{
    const bool fragmentIsCap = (inputs.fragmentVariableIdAndIsCap & (1u << 31u)) != 0;

    // 1) Compute the closest point on the line segment spanned by the entrance and exit point.
    int fragElementID = int(inputs.fragmentLinePointIdx);
    int fragElementNextID = fragElementID + 1;
    float interpolationFactor = fract(inputs.fragmentLinePointIdx);

    // 2) Sample variables from buffers
    float variableValue, variableNextValue;
    vec2 variableMinMax, variableNextMinMax;
    const uint varID = inputs.fragmentVariableIdAndIsCap & 0x7FFFFFFFu;
    const uint actualVarID = sampleActualVarID(varID);
    const uint sensitivityOffset = sampleSensitivityOffset(actualVarID);

    sampleVariableFromLineSSBO(
            inputs.fragmentLineID, actualVarID, fragElementID, sensitivityOffset, variableValue, variableMinMax);
    sampleVariableFromLineSSBO(
            inputs.fragmentLineID, actualVarID, fragElementNextID, sensitivityOffset, variableNextValue, variableNextMinMax);

    // 3) Determine variable color
    vec4 surfaceColor = determineColorLinearInterpolate(
            actualVarID, variableValue, variableNextValue, interpolationFactor);

    if (fragmentIsCap) {
        surfaceColor.rgb = vec3(0.0, 0.0, 0.0);
    }

    // 3.1) Draw black separators between single stripes.
    float varFraction = inputs.fragmentRollPosition;
    if (separatorWidth > 0) {
        drawSeparatorBetweenStripes(surfaceColor, varFraction, separatorWidth);
    }

    // 4) Phong Lighting
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;

    vec4 color = computePhongLighting(
            surfaceColor, occlusionFactor, shadowFactor,
            inputs.fragmentPosition, inputs.fragmentNormal, inputs.fragmentTangent);

#ifdef SUPPORT_LINE_DESATURATION
    uint desaturateLine = 1 - lineSelectedArray[inputs.fragmentLineID];
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
