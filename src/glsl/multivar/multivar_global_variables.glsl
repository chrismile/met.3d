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
// Current number of variables to display
uniform int numVariables;
// Maximum number of variables to display
uniform int maxNumVariables;

// Model-view-projection matrix.
uniform mat4  mvpMatrix;
// Number of instances for rendering
uniform uint numInstances;
// Radius of tubes
uniform float lineWidth;

uniform vec3 cameraPosition; // world space

uniform int renderTubesUpToIndex;

// Structs for SSBOs
struct LineDescData {
    float startIndex; // TODO: int?
//    vec2 dummy;
};

struct VarDescData {
//    float startIndex;
//    vec2 minMax;
//    float dummy;
    vec4 info;
};

struct LineVarDescData {
    vec2 minMax;
};

// SSBOs which contain all data for all variables per trajectory
layout (std430, binding = 2) readonly buffer VariableArray {
    float varArray[];
};

layout (std430, binding = 4) readonly buffer LineDescArray {
    LineDescData lineDescs[];
};

layout (std430, binding = 5) readonly buffer VarDescArray {
    VarDescData varDescs[];
};

layout (std430, binding = 6) readonly buffer LineVarDescArray {
    LineVarDescData lineVarDescs[];
};

layout (std430, binding = 7) readonly buffer VarSelectedArray {
    uint selectedVars[];
};

#ifdef SUPPORT_LINE_DESATURATION
layout (std430, binding = 14) readonly buffer LineSelectedArray {
    uint lineSelectedArray[];
};

vec3 rgbToHsv(vec3 color) {
    float minValue = min(color.r, min(color.g, color.b));
    float maxValue = max(color.r, max(color.g, color.b));

    float C = maxValue - minValue;

    // 1) Compute hue H
    float H = 0;
    if (maxValue == color.r) {
        H = mod((color.g - color.b) / C, 6.0);
    } else if (maxValue == color.g) {
        H = (color.b - color.r) / C + 2.0;
    } else if (maxValue == color.b) {
        H = (color.r - color.g) / C + 4.0;
    } else {
        H = 0.0;
    }

    H *= 60.0; // hue is in degree

    // 2) Compute the value V
    float V = maxValue;

    // 3) Compute saturation S
    float S = 0;
    if (V == 0) {
        S = 0;
    } else {
        S = C / V;
    }

    return vec3(H, S, V);
}

vec3 hsvToRgb(vec3 color) {
    const float H = color.r;
    const float S = color.g;
    const float V = color.b;

    float h = H / 60.0;

    int hi = int(floor(h));
    float f = (h - float(hi));

    float p = V * (1.0 - S);
    float q = V * (1.0 - S * f);
    float t = V * (1.0 - S * (1.0 - f));

    if (hi == 1) {
        return vec3(q, V, p);
    } else if (hi == 2) {
        return vec3(p, V, t);
    } else if (hi == 3) {
        return vec3(p, q, V);
    } else if (hi == 4) {
        return vec3(t, p, V);
    } else if (hi == 5) {
        return vec3(V, p, q);
    } else {
        return vec3(V, t, p);
    }
}

vec4 desaturateColor(vec4 inputColor) {
    vec3 hsvColor = rgbToHsv(inputColor.rgb);
    hsvColor.g *= 0.0;
    hsvColor.b *= 0.8;
    return vec4(hsvToRgb(hsvColor), inputColor.a);
}
#endif

// binding 8 belongs to selected color array



// Sample the actual variable ID from the current user selection
int sampleActualVarID(in uint varID) {
    if (varID < 0 || varID >= maxNumVariables) {
        return -1;
    }

    uint index = varID + 1;

    uint numSelected = 0;
    // HACK: change to dynamic variable
    for (int c = 0; c < maxNumVariables; ++c) {
        if (selectedVars[c] > 0) {
            numSelected++;
        }

        if (numSelected >= index) {
            return c;
        }
    }

    return -1;
}

// Function to sample from SSBOs
void sampleVariableFromLineSSBO(
        in uint lineID, in uint varID, in uint elementID, out float value, out vec2 minMax) {
    uint startIndex = uint(lineDescs[lineID].startIndex);
    VarDescData varDesc = varDescs[maxNumVariables * lineID + varID];
    const uint varOffset = uint(varDesc.info.r);
    //minMax = varDesc.info.gb;

    minMax = minMaxValues[varID];

    value = varArray[startIndex + varOffset + elementID];
}

void sampleVariableFromLineSSBODirect(
        in uint lineID, in uint varID, in uint elementID, out float value, out vec2 minMax) {
    uint startIndex = uint(lineDescs[lineID].startIndex);
    VarDescData varDesc = varDescs[maxNumVariables * lineID + varID];
    const uint varOffset = uint(varDesc.info.r);
    //minMax = varDesc.info.gb;

    minMax = minMaxValues[varID];

    value = varArray[startIndex + varOffset + elementID];
}

// Function to sample distribution from SSBO
void sampleVariableDistributionFromLineSSBO(in uint lineID, in uint varID, out vec2 minMax) {
    LineVarDescData lineVarDesc = lineVarDescs[maxNumVariables * lineID + varID];
    minMax = lineVarDesc.minMax.xy;
}
