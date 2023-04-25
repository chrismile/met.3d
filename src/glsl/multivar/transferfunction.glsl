/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020-2021 Christoph Neuhauser
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
#ifndef TRANSFER_FUNCTION_GLSL
#define TRANSFER_FUNCTION_GLSL

// Transfer function color lookup table.
#if defined(USE_MULTI_VAR_TRANSFER_FUNCTION)

uniform sampler1DArray transferFunctionTexture;
layout (std430, binding = 8) readonly buffer VarDivergingArray {
    uint varDiverging[];
};
layout (std430, binding = 9) readonly buffer MinMaxBuffer {
    vec2 minMaxValues[];
};
layout (std430, binding = 15) readonly buffer UseLogScaleBuffer {
    uint useLogScaleArray[];
};

#ifdef IS_MULTIVAR_DATA
uniform int useColorIntensity = 1;
#endif

// Function to sample from SSBOs
uint sampleIsDiverging(in uint varID) {
    return varDiverging[varID];
}

vec4 transferFunction(float attr, uint variableIndex) {
    vec2 minMaxValue = minMaxValues[variableIndex];
    uint useLogScale = useLogScaleArray[variableIndex];
    float minAttributeValue = minMaxValue.x;
    float maxAttributeValue = minMaxValue.y;

    // Transfer to range [0, 1].
    float posFloat;
    if (useLogScale != 0) {
        float log10factor = 1 / log(10);
        float logMin = log(abs(minAttributeValue)) * log10factor;
        float logMax = log(abs(maxAttributeValue)) * log10factor;
        float logAttr = log(attr) * log10factor;
        posFloat = clamp((logAttr - logMin) / (logMax - logMin), 0.0, 1.0);
    } else {
        posFloat = clamp((attr - minAttributeValue) / (maxAttributeValue - minAttributeValue), 0.0, 1.0);
    }

#ifdef IS_MULTIVAR_DATA
    if (useColorIntensity == 0) {
        if (sampleIsDiverging(variableIndex) > 0 && posFloat < 0.5) {
            posFloat = 0.0;
        } else {
            posFloat = 1.0;
        }
    }
#endif

    // Look up the color value.
    return texture(transferFunctionTexture, vec2(posFloat, variableIndex));
}

vec4 transferFunctionArtificial(float posFloat, uint variableIndex) {
    // Look up the color value.
    return texture(transferFunctionTexture, vec2(posFloat, variableIndex));
}

#else

uniform float minAttributeValue = 0.0;
uniform float maxAttributeValue = 1.0;
uniform sampler1D transferFunctionTexture;

vec4 transferFunction(float attr) {
    // Transfer to range [0,1].
    float posFloat = clamp((attr - minAttributeValue) / (maxAttributeValue - minAttributeValue), 0.0, 1.0);
    // Look up the color value.
    return texture(transferFunctionTexture, posFloat);
}

#endif

//#ifdef USE_PRINCIPAL_STRESS_DIRECTION_INDEX
//vec4 transferFunction(float attr, uint fragmentPrincipalStressIndex) {
//    // Transfer to range [0,1].
//    float posFloat = clamp((attr - minAttributeValue) / (maxAttributeValue - minAttributeValue), 0.0, 1.0);
//    vec4 color;
//    float alpha = texture(transferFunctionTexture, posFloat).a;
//    if (fragmentPrincipalStressIndex == 0) {
//        color = vec4(1.0, 0.0, 0.0, alpha);
//    } else if (fragmentPrincipalStressIndex == 1) {
//        color = vec4(0.0, 1.0, 0.0, alpha);
//    } else {
//        color = vec4(0.0, 0.0, 1.0, alpha);
//    }
//#ifdef DIRECT_BLIT_GATHER
//    color.rgb = mix(color.rgb, vec3(1.0), pow((1.0 - posFloat), 10.0) * 0.5);
//#endif
//    return color;
//}
//#endif

#endif
