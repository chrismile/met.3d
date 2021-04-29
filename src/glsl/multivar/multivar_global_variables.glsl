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
layout (std430, binding = 2) buffer VariableArray {
    float varArray[];
};

layout (std430, binding = 4) buffer LineDescArray {
    LineDescData lineDescs[];
};

layout (std430, binding = 5) buffer VarDescArray {
    VarDescData varDescs[];
};

layout (std430, binding = 6) buffer LineVarDescArray {
    LineVarDescData lineVarDescs[];
};

layout (std430, binding = 7) buffer VarSelectedArray {
    uint selectedVars[];
};

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

// Function to sample distribution from SSBO
void sampleVariableDistributionFromLineSSBO(in uint lineID, in uint varID, out vec2 minMax) {
    LineVarDescData lineVarDesc = lineVarDescs[maxNumVariables * lineID + varID];
    minMax = lineVarDesc.minMax.xy;
}