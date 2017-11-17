/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015-2017 Michael Kern
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
 ***                             CONSTANTS
 *****************************************************************************/

/* - use const datatype instead of #define */
const int NUM_TUBESEGMENTS = 10;
const float TUBE_END_OFFSET = 0.05;

/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

/* - connection between different stages.
   - defines custom input and output variables
   - acts like a struct

    interface VStoFS {
        smooth vec3 pos;
    }
*/

interface VStoGSWorld
{
    smooth vec3 worldPos;
};

interface VStoGSTrajectory
{
    smooth vec3 worldPos;
    smooth float value;
    smooth float thicknessValue;
};

interface VStoGSArrowHeads
{
    smooth vec3 worldPos;
    smooth vec3 direction;
    smooth float value;
};

interface GStoFSSimple
{
    smooth vec3 worldPos;
    smooth vec3 normal;
};

interface GStoFSTrajectory
{
    smooth vec3 worldPos;
    smooth vec4 lightSpacePos;
    smooth vec3 normal;
    smooth float value;
};

interface VStoFSShadowMap
{
    smooth vec2 texCoords;
};

/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform float       tubeRadius;
uniform vec3        geometryColor;
uniform vec2        pToWorldZParams;
uniform int         colorMode;

uniform mat4        mvpMatrix;
uniform mat4        lightMVPMatrix;
uniform sampler1D   transferFunction;
uniform float       tfMinimum;
uniform float       tfMaximum;
uniform vec4        shadowColor;

uniform vec3        cameraPosition;
uniform vec3        lightDirection;

// offset the second vertex by this vector
uniform vec3        offsetDirection;
uniform float       endSegmentOffset;

// tube thickness parameters
uniform bool        thicknessMapping;
uniform vec2        thicknessRange;
uniform vec2        thicknessValueRange;

uniform sampler2D   shadowMap;
uniform bool        enableSelfShadowing;

/*****************************************************************************
 ***                             INCLUDES
 *****************************************************************************/

/*
  - you can include several files via: #include "filename.glsl"
  - #include is simply replaced by glfx with included code
  - error messages carry an index, indicating which file caused the error
  */

/*****************************************************************************
 ***                          UTILITY FUNCTIONS
 *****************************************************************************/

// Info about the current line segment point for tube rendering.
struct TubeGeometryInfo
{
    vec3 pos; // current world position
    vec3 normal; // normal to the line segment tangent
    vec3 binormal; // binormal, perpendicular to the normal and tangent
    vec3 tangent;  // tangent of segment
    float radius; // tube radius
    float value; // [optional value]
};

// -----------------------------------------------------------------------------

// Calculate normal and binormal from a given direction
void calculateRayBasisTangent(in vec3 tangent, in vec3 prevBinormal,
                                inout vec3 normal, inout vec3 binormal)
{
    bool useGivenBinormal = prevBinormal != vec3(0);

    if (useGivenBinormal)
    {
        normal = cross(prevBinormal, tangent);
    }

    if (!useGivenBinormal || length(normal) <= 0.01)
    {
        normal = cross(tangent, vec3(0,0,1));

        if (length(normal) <= 0.01)
        {
            normal = cross(tangent, vec3(1,0,0));

            if (length(normal) <= 0.01) { normal = cross(tangent, vec3(0,0,1)); }
        }
    }

    normal = normalize(normal);

    binormal = cross(tangent, normal);
    binormal = normalize(binormal);

    normal = cross(binormal, tangent);
    normal = normalize(normal);
}


void calculateRayBasis(in vec3 prevPos, in vec3 nextPos,
                        in vec3 prevBinormal,
                        out vec3 normal, out vec3 binormal)
{
    const vec3 tangent = nextPos - prevPos;
    calculateRayBasisTangent(tangent, prevBinormal, normal, binormal);
}

// -----------------------------------------------------------------------------

// Compute color of blinn-phong shaded surface
void getBlinnPhongColor(in vec3 worldPos, in vec3 normalDir, in vec3 ambientColor,
                        in vec3 diffuseColor,
                        out vec3 color)
{
    const vec3 lightColor = vec3(1,1,1);

    const vec3 kA = 0.3 * ambientColor;
    const vec3 kD = 0.5 * ambientColor;
    const float kS = 0.2;
    const float s = 10;

    const vec3 n = normalize(normalDir);
    const vec3 v = normalize(cameraPosition - worldPos);
    const vec3 l = normalize(-lightDirection); // specialCase
    const vec3 h = normalize(v + l);

    vec3 diffuse = kD * clamp(abs(dot(n, l)), 0.0, 1.0) * diffuseColor;
    vec3 specular = kS * pow(clamp(abs(dot(n, h)), 0.0, 1.0), s) * lightColor;

    color = kA + diffuse + specular;
}

// -----------------------------------------------------------------------------

void computeTubeRadius(in float value, out float tubeRadius)
{
     const float tfDifference = (thicknessValueRange.y - thicknessValueRange.x);
     const float t = (value - thicknessValueRange.x) / tfDifference;

     tubeRadius = mix(thicknessRange.x, thicknessRange.y, t);
}

void computeTubeRadii(in float prevValue, in float nextValue,
                      out vec2 tubeRadii)
{
    const float tfDifference = (thicknessValueRange.y - thicknessValueRange.x);
    const float prevT = (prevValue - thicknessValueRange.x) / tfDifference;
    const float nextT = (nextValue - thicknessValueRange.x) / tfDifference;

    tubeRadii.x = mix(thicknessRange.x, thicknessRange.y, prevT);
    tubeRadii.y = mix(thicknessRange.x, thicknessRange.y, nextT);
}

/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSTrajectory(in vec3 worldPos : 0, in float value : 1,
                    in float thicknessValue : 2,
                    out VStoGSTrajectory Output)
{
    float worldZ = (log(worldPos.z) - pToWorldZParams.x) * pToWorldZParams.y;

    Output.worldPos = vec3(worldPos.xy, worldZ);
    Output.value = (colorMode == 1) ? worldPos.z : value;
    Output.thicknessValue = thicknessValue;
}

// -----------------------------------------------------------------------------

shader VSTrajectoryShadow(in vec3 worldPos : 0, in float value : 1,
                          in float thicknessValue : 2,
                          out VStoGSTrajectory Output)
{
    Output.worldPos = vec3(worldPos.xy, 0.1);
    Output.value = value;
    Output.thicknessValue = thicknessValue;
}

// -----------------------------------------------------------------------------

shader VSArrowHeads(in vec3 worldPos : 0, in vec3 direction : 1,
                    in float value : 2, out VStoGSArrowHeads Output)
{
    float worldZ = (log(worldPos.z) - pToWorldZParams.x) * pToWorldZParams.y;

    Output.worldPos = vec3(worldPos.xy, worldZ);
    Output.direction = direction;
    Output.value = value;
}

shader VSArrowHeadsShadow(in vec3 worldPos : 0, in vec3 direction : 1,
                          in float value : 2, out VStoGSArrowHeads Output)
{
    Output.worldPos = vec3(worldPos.xy, 0.1);
    Output.direction = direction;
    Output.value = value;
}

// -----------------------------------------------------------------------------

shader VSWorldGeometry(in vec3 worldPos : 0, out VStoGSWorld Output)
{
    Output.worldPos = worldPos;
}

// -----------------------------------------------------------------------------

shader VSLonLatPGeometry(in vec3 lonlatP : 0, out VStoGSWorld Output)
{
    float worldZ = (log(lonlatP.z) - pToWorldZParams.x) * pToWorldZParams.y;
    Output.worldPos = vec3(lonlatP.xy, worldZ);
}

shader VSShadowGroundImage(in vec3 worldPos : 0, in vec2 texCoords : 1,
                           out VStoFSShadowMap Output)
{
    gl_Position = mvpMatrix * vec4(worldPos, 1);
    Output.texCoords = texCoords;
}

/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

void generateTube(in TubeGeometryInfo prev, in TubeGeometryInfo next,
                  out vec3 tubeWorlds[(NUM_TUBESEGMENTS + 1) * 2],
                  out vec3 tubeNormals[(NUM_TUBESEGMENTS + 1) * 2],
                  out float tubeValues[(NUM_TUBESEGMENTS + 1) * 2])
{
    // Start tube generation.
    int numTubeSegments = NUM_TUBESEGMENTS;
    float anglePerSegment = 360. / float(numTubeSegments);

    int cc = 0;

    #pragma unroll
    for (int t = 0; t <= numTubeSegments; ++t)
    {
        // Get the next angle in radians
        float angle = radians(anglePerSegment * t);
        // Compute the sine and cosine of the circle
        float cosi = cos(angle);
        float sini = sin(angle);

        // Compute the next vertices along the circle for start and end position
        vec3 startWorldPos = prev.pos
                    + (cosi * prev.normal + sini * prev.binormal) * prev.radius;
        vec3 endWorldPos = next.pos
                    + (cosi * next.normal + sini * next.binormal) * next.radius;

        // Emit two new vertices to create the tube
        tubeValues[cc] = prev.value;
        tubeNormals[cc] = normalize(startWorldPos - prev.pos);
        tubeWorlds[cc] = startWorldPos;

        cc++;

        tubeValues[cc] = next.value;
        tubeNormals[cc] = normalize(endWorldPos - next.pos);
        tubeWorlds[cc] = endWorldPos;

        cc++;
    }
}

// -----------------------------------------------------------------------------

void generateTubeEnd(in TubeGeometryInfo end, in float endOffset,
                    out vec3 tubeWorlds[(NUM_TUBESEGMENTS + 1) * 3],
                    out vec3 tubeNormals[(NUM_TUBESEGMENTS + 1) * 3],
                    out float tubeValues[(NUM_TUBESEGMENTS + 1) * 3])
{
    // Start tube generation.
    const int numTubeSegments = NUM_TUBESEGMENTS;
    const float anglePerSegment = 360. / float(numTubeSegments);

    const vec3 endMiddlePos = end.pos + endOffset * end.tangent;

    int cc = 0;

    #pragma unroll
    for (int t = 0; t <= numTubeSegments; ++t)
    {
        // Get the next angle in radians
        float angle = radians(anglePerSegment * t);
        // Compute the sine and cosine of the circle
        float cosi = cos(angle);
        float sini = sin(angle);

        // Compute the next vertices along the circle for start and end position
        vec3 worldPos = end.pos
                       + (cosi * end.normal + sini * end.binormal) * end.radius;
        tubeValues[cc] = end.value;
        tubeNormals[cc] = normalize(worldPos - end.pos);
        tubeWorlds[cc] = worldPos;

        cc++;

        tubeWorlds[cc] = endMiddlePos;
        tubeNormals[cc] = end.tangent;
        tubeValues[cc] = end.value;

        cc++;

        angle = radians(anglePerSegment * (t + 1));
        cosi = cos(angle);
        sini = sin(angle);

        worldPos = end.pos + (cosi * end.normal + sini * end.binormal) * end.radius;

        tubeValues[cc] = end.value;
        tubeNormals[cc] = normalize(worldPos - end.pos);
        tubeWorlds[cc] = worldPos;

        cc++;
    }
}

// -----------------------------------------------------------------------------

shader GSTrajectoryTube(in VStoGSTrajectory Input[], out GStoFSTrajectory Output)
{

    Output.value = 0;

    bool startPrimitive = gl_PrimitiveIDIn == 0;

    if (Input[1].value == -1 || Input[2].value == -1) { return; }
    //    o-----o______o-----o
    //    0     1      2     3 // IDs

    // Obtain the scalar values at each segment point
    float value1 = Input[1].value;
    float value2 = Input[2].value;

    // Obtain the scalar values used for thickness mapping
    float valueT1 = Input[1].thicknessValue;
    float valueT2 = Input[2].thicknessValue;

    // Obtain world positions, and handle line boundaries.
    vec3 pos1 = Input[1].worldPos;
    vec3 pos2 = Input[2].worldPos;
    vec3 pos0 = (Input[0].value == -1) ? pos1 : Input[0].worldPos;
    vec3 pos3 = (Input[3].value == -1) ? pos2 : Input[3].worldPos;

    vec3 tangent = normalize(pos2 - pos1);
    vec3 normalPrev, binormalPrev, normalNext, binormalNext;
    calculateRayBasis(pos0, pos2, vec3(0), normalPrev, binormalPrev);
    calculateRayBasis(pos1, pos3, binormalPrev, normalNext, binormalNext);

    const int numVertices = (NUM_TUBESEGMENTS + 1) * 2;
    vec3 tubeWorlds[numVertices]; vec3 tubeNormals[numVertices];
    float tubeValues[numVertices];

    vec2 tubeRadii = vec2(tubeRadius);
    if (thicknessMapping)
    {
        computeTubeRadii(valueT1, valueT2, tubeRadii);
    }


    TubeGeometryInfo prevInfo = {   pos1, normalPrev, binormalPrev,
                                    tangent, tubeRadii.x, value1 };
    TubeGeometryInfo nextInfo = {   pos2, normalNext, binormalNext,
                                    tangent, tubeRadii.y, value2 };

    // Generate tube with varying tube radii
    generateTube(prevInfo, nextInfo, tubeWorlds, tubeNormals, tubeValues);

    int numTubeSegments = NUM_TUBESEGMENTS;
    float anglePerSegment = 360. / float(numTubeSegments);

    for (int t = 0; t < numVertices; ++t)
    {
        gl_Position = mvpMatrix * vec4(tubeWorlds[t], 1);
        Output.value = tubeValues[t];
        Output.normal = tubeNormals[t];
        Output.worldPos = tubeWorlds[t];
        Output.lightSpacePos = lightMVPMatrix * vec4(tubeWorlds[t], 1);
        EmitVertex();
    }

    EndPrimitive();

    // Handle the ends of each tube
    bool isStart = startPrimitive || pos0 == pos1;
    bool isEnd = pos2 == pos3;

    if (!isStart && !isEnd) { return; }

    TubeGeometryInfo endInfo;

    const int numVerticesEnd = (NUM_TUBESEGMENTS + 1) * 3;

    vec3 tubeWorldsEnd[numVerticesEnd]; vec3 tubeNormalsEnd[numVerticesEnd];
    float tubeValuesEnd[numVerticesEnd];

    if (isStart)
    {
        vec3 startTangent = normalize(pos1 - pos2);

        endInfo = TubeGeometryInfo(pos1, normalPrev, binormalPrev,
                    startTangent, tubeRadii.x, value1);

        // Generate the ends of each tube
        generateTubeEnd(endInfo, TUBE_END_OFFSET, tubeWorldsEnd, tubeNormalsEnd, tubeValuesEnd);
        Output.value = endInfo.value;

        for (int t = 0; t < numVerticesEnd; ++t)
        {
            gl_Position = mvpMatrix * vec4(tubeWorldsEnd[t], 1);
            Output.normal = tubeNormalsEnd[t];
            Output.worldPos = tubeWorldsEnd[t];
            Output.lightSpacePos = lightMVPMatrix * vec4(tubeWorldsEnd[t], 1);
            EmitVertex();
        }
    }

    if (isEnd)
    {
        vec3 endTangent = normalize(pos2 - pos1);

        endInfo = TubeGeometryInfo(pos2, normalNext, binormalNext,
                    endTangent, tubeRadii.y, value2);

        // Generate the ends of each tube
        generateTubeEnd(endInfo, TUBE_END_OFFSET, tubeWorldsEnd, tubeNormalsEnd, tubeValuesEnd);
        Output.value = endInfo.value;

        for (int t = 0; t < numVerticesEnd; ++t)
        {
            gl_Position = mvpMatrix * vec4(tubeWorldsEnd[t], 1);
            Output.normal = tubeNormalsEnd[t];
            Output.worldPos = tubeWorldsEnd[t];
            Output.lightSpacePos = lightMVPMatrix * vec4(tubeWorldsEnd[t], 1);
            EmitVertex();
        }
    }
}

// -----------------------------------------------------------------------------

shader GSTrajectoryTubeShadow(in VStoGSTrajectory Input[], out GStoFSTrajectory Output)
{
    // set value of all to zero
    Output.value = 0;

    if (Input[1].value == -1 || Input[2].value == -1) { return; }
    //    o-----o______o-----o
    //    0     1      2     3

    // Obtain the scalar values at each segment point
    float value1 = Input[1].value;
    float value2 = Input[2].value;

    // Obtain the scalar values used for thickness mapping
    float valueT1 = Input[1].thicknessValue;
    float valueT2 = Input[2].thicknessValue;

    // Obtain world positions, and handle line boundaries.
    vec3 pos1 = Input[1].worldPos;
    vec3 pos2 = Input[2].worldPos;
    vec3 pos0 = (Input[0].value == -1) ? pos1 : Input[0].worldPos;
    vec3 pos3 = (Input[3].value == -1) ? pos2 : Input[3].worldPos;

    // we only need the normal parallel to the x/y plane
    vec3 normalPrev, normalNext;
    // approximated tangent of pos1
    const vec3 prevDir = normalize(pos2 - pos0);
    // approximated tangent of pos2
    const vec3 nextDir = normalize(pos3 - pos1);

    // compute normals of tangents
    normalPrev = cross(prevDir, vec3(0,0,1));
    normalNext = cross(nextDir, vec3(0,0,1));

    // set all normals of output to zero
    Output.normal = normalize(vec3(0,0,0));

    vec2 tubeRadii = vec2(tubeRadius);
    if (thicknessMapping)
    {
        computeTubeRadii(valueT1, valueT2, tubeRadii);
    }

    // and calculate the boundaries of the projected quad in x/y plane
    Output.worldPos = pos1 - normalPrev * tubeRadii.x;
    gl_Position = mvpMatrix * vec4(Output.worldPos, 1);
    Output.lightSpacePos = lightMVPMatrix * vec4(Output.worldPos, 1);
    EmitVertex();

    Output.worldPos = pos1 + normalPrev * tubeRadii.x;
    gl_Position = mvpMatrix * vec4(Output.worldPos, 1);
    Output.lightSpacePos = lightMVPMatrix * vec4(Output.worldPos, 1);
    EmitVertex();

    Output.worldPos = pos2 - normalNext * tubeRadii.y;
    gl_Position = mvpMatrix * vec4(Output.worldPos, 1);
    Output.lightSpacePos = lightMVPMatrix * vec4(Output.worldPos, 1);
    EmitVertex();

    Output.worldPos = pos2 + normalNext * tubeRadii.y;
    gl_Position = mvpMatrix * vec4(Output.worldPos, 1);
    Output.lightSpacePos = lightMVPMatrix * vec4(Output.worldPos, 1);
    EmitVertex();

    EndPrimitive();
}

// -----------------------------------------------------------------------------

shader GSSimpleTube(in VStoGSWorld Input[], out GStoFSSimple Output)
{
    vec3 prevPos = Input[0].worldPos;
    vec3 nextPos = Input[1].worldPos;

    vec3 tangent = normalize(nextPos - prevPos);
    vec3 normal, binormal;
    calculateRayBasis(prevPos, nextPos, vec3(0), normal, binormal);

    const int numVertices = (NUM_TUBESEGMENTS + 1) * 2;
    vec3 tubeWorlds[numVertices]; vec3 tubeNormals[numVertices];
    float tubeValues[numVertices];

    TubeGeometryInfo prevInfo = {   prevPos, normal, binormal,
                                    tangent, tubeRadius, 0 };
    TubeGeometryInfo nextInfo = {   nextPos, normal, binormal,
                                    tangent, tubeRadius, 0 };

    generateTube(prevInfo, nextInfo, tubeWorlds, tubeNormals, tubeValues);

    int numTubeSegments = NUM_TUBESEGMENTS;
    float anglePerSegment = 360. / float(numTubeSegments);

    for (int t = 0; t < numVertices; ++t)
    {
        gl_Position = mvpMatrix * vec4(tubeWorlds[t], 1);
        Output.normal = tubeNormals[t];
        Output.worldPos = tubeWorlds[t];
        EmitVertex();
    }

    EndPrimitive();

    const int numVerticesEnd = (NUM_TUBESEGMENTS + 1) * 3;

    vec3 tubeWorldsEnd[numVerticesEnd]; vec3 tubeNormalsEnd[numVerticesEnd];
    float tubeValuesEnd[numVerticesEnd];

    tangent = normalize(prevPos - nextPos);

    TubeGeometryInfo endInfo = TubeGeometryInfo(prevPos, normal, binormal,
                                                tangent, tubeRadius, 0);

    // Generate the ends of each tube
    generateTubeEnd(endInfo, endSegmentOffset, tubeWorldsEnd,
                    tubeNormalsEnd, tubeValuesEnd);

    for (int t = 0; t < numVerticesEnd; ++t)
    {
        gl_Position = mvpMatrix * vec4(tubeWorldsEnd[t], 1);
        Output.normal = tubeNormalsEnd[t];
        Output.worldPos = tubeWorldsEnd[t];
        EmitVertex();
    }

    tangent = normalize(nextPos - prevPos);
    endInfo.pos = nextPos;
    endInfo.tangent = tangent;

    // Generate the ends of each tube
    generateTubeEnd(endInfo, endSegmentOffset, tubeWorldsEnd,
                    tubeNormalsEnd, tubeValuesEnd);

    for (int t = 0; t < numVerticesEnd; ++t)
    {
        gl_Position = mvpMatrix * vec4(tubeWorldsEnd[t], 1);
        Output.normal = tubeNormalsEnd[t];
        Output.worldPos = tubeWorldsEnd[t];
        EmitVertex();
    }

    EndPrimitive();
}

// -----------------------------------------------------------------------------

shader GSSimpleArrowHead(in VStoGSArrowHeads Input[],
                         out GStoFSTrajectory Output)
{
    vec3 normal, binormal;
    vec3 tangent = Input[0].direction;
    calculateRayBasisTangent(Input[0].direction, vec3(0), normal, binormal);

    float radius = tubeRadius;
    if (thicknessMapping)
    {
        computeTubeRadius(Input[0].value, radius);
    }
    radius *= 2.5;

    TubeGeometryInfo endInfo = TubeGeometryInfo(
                Input[0].worldPos, normal, binormal, tangent, radius, 0);

    Output.value = Input[0].value;

    const int numVerticesEnd = (NUM_TUBESEGMENTS + 1) * 3;

    vec3 tubeWorldsEnd[numVerticesEnd]; vec3 tubeNormalsEnd[numVerticesEnd];
    float tubeValuesEnd[numVerticesEnd];

    // Generate the ends of each tube.
    generateTubeEnd(endInfo, radius, tubeWorldsEnd,
                    tubeNormalsEnd, tubeValuesEnd);

    for (int t = 0; t < numVerticesEnd; ++t)
    {
        gl_Position = mvpMatrix * vec4(tubeWorldsEnd[t], 1);
        Output.normal = tubeNormalsEnd[t];
        Output.worldPos = tubeWorldsEnd[t];
        Output.lightSpacePos = lightMVPMatrix * vec4(tubeWorldsEnd[t], 1);
        EmitVertex();
    }

    EndPrimitive();

    // Generate the ends of each tube.
    generateTubeEnd(endInfo, 0, tubeWorldsEnd,
                    tubeNormalsEnd, tubeValuesEnd);

    for (int t = 0; t < numVerticesEnd; ++t)
    {
        gl_Position = mvpMatrix * vec4(tubeWorldsEnd[t], 1);
        Output.normal = tubeNormalsEnd[t];
        Output.worldPos = tubeWorldsEnd[t];
        Output.lightSpacePos = lightMVPMatrix * vec4(tubeWorldsEnd[t], 1);
        EmitVertex();
    }

    EndPrimitive();
}

shader GSSimpleArrowShadow(in VStoGSArrowHeads Input[],
                           out GStoFSTrajectory Output)
{
    vec3 tangent = Input[0].direction;
    vec3 normal = vec3(-tangent.y, tangent.x, 0);

    float radius = tubeRadius;
    if (thicknessMapping)
    {
        computeTubeRadius(Input[0].value, radius);
    }
    radius *= 2.5;

    vec3 arrowBase = Input[0].worldPos;
    vec3 arrowHead = arrowBase + tangent * radius;
    vec3 arrowUp = arrowBase + normal * radius;
    vec3 arrowDown = arrowBase - normal * radius;

    Output.value = Input[0].value;
    Output.normal = normalize(vec3(0,0,0));

    gl_Position = mvpMatrix * vec4(arrowDown, 1);
    Output.worldPos = arrowBase;
    Output.lightSpacePos = lightMVPMatrix * vec4(arrowDown, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(arrowHead, 1);
    Output.worldPos = arrowHead;
    Output.lightSpacePos = lightMVPMatrix * vec4(arrowHead, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(arrowBase, 1);
    Output.worldPos = arrowHead;
    Output.lightSpacePos = lightMVPMatrix * vec4(arrowBase, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(arrowUp, 1);
    Output.worldPos = arrowHead;
    Output.lightSpacePos = lightMVPMatrix * vec4(arrowUp, 1);
    EmitVertex();

    EndPrimitive();
}

// -----------------------------------------------------------------------------

shader GSSimpleLine(in VStoGSWorld Input[])
{
    vec3 prevPos = Input[0].worldPos;
    vec3 nextPos = Input[1].worldPos;

    gl_Position = mvpMatrix * vec4(prevPos, 1);
    EmitVertex();

    gl_Position = mvpMatrix * vec4(nextPos, 1);
    EmitVertex();
}

// -----------------------------------------------------------------------------

shader GSTickTube(in VStoGSWorld Input[], out GStoFSSimple Output)
{
    vec3 prevPos = Input[0].worldPos;
    vec3 nextPos = prevPos + offsetDirection;

    vec3 tangent = normalize(nextPos - prevPos);
    vec3 normal, binormal;
    calculateRayBasis(prevPos, nextPos, vec3(0), normal, binormal);

    const int numVertices = (NUM_TUBESEGMENTS + 1) * 2;
    vec3 tubeWorlds[numVertices]; vec3 tubeNormals[numVertices];
    float tubeValues[numVertices];

    TubeGeometryInfo prevInfo = {   prevPos, normal, binormal,
                                    tangent, tubeRadius, 0 };
    TubeGeometryInfo nextInfo = {   nextPos, normal, binormal,
                                    tangent, tubeRadius, 0 };

    generateTube(prevInfo, nextInfo, tubeWorlds, tubeNormals, tubeValues);

    int numTubeSegments = NUM_TUBESEGMENTS;
    float anglePerSegment = 360. / float(numTubeSegments);

    for (int t = 0; t < numVertices; ++t)
    {
        gl_Position = mvpMatrix * vec4(tubeWorlds[t], 1);
        Output.normal = tubeNormals[t];
        Output.worldPos = tubeWorlds[t];
        EmitVertex();
    }

    EndPrimitive();

    const int numVerticesEnd = (NUM_TUBESEGMENTS + 1) * 3;

    vec3 tubeWorldsEnd[numVerticesEnd]; vec3 tubeNormalsEnd[numVerticesEnd];
    float tubeValuesEnd[numVerticesEnd];

    TubeGeometryInfo endInfo = TubeGeometryInfo(nextPos, normal, binormal,
                                tangent, tubeRadius, 0);

    // Generate the ends of each tube.
    generateTubeEnd(endInfo, endSegmentOffset, tubeWorldsEnd,
                    tubeNormalsEnd, tubeValuesEnd);

    for (int t = 0; t < numVerticesEnd; ++t)
    {
        gl_Position = mvpMatrix * vec4(tubeWorldsEnd[t], 1);
        Output.normal = tubeNormalsEnd[t];
        Output.worldPos = tubeWorldsEnd[t];
        EmitVertex();
    }

    EndPrimitive();
}

// -----------------------------------------------------------------------------

shader GSTickLine(in VStoGSWorld Input[])
{
    const vec3 prevPos = Input[0].worldPos;
    const vec3 nextPos = prevPos + offsetDirection;

    gl_Position = mvpMatrix * vec4(prevPos, 1);
    EmitVertex();

    gl_Position = mvpMatrix * vec4(nextPos, 1);
    EmitVertex();
}

/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

shader FSSimpleColor(out vec4 fragColor)
{
    fragColor = vec4(geometryColor, 1);
}

// -----------------------------------------------------------------------------

shader FSSurfaceColor(in GStoFSSimple Input, out vec4 fragColor)
{
    vec3 surfaceColor = vec3(0);
    vec3 ambientColor = geometryColor;
    vec3 diffuseColor = vec3(1, 1, 1);
    getBlinnPhongColor(Input.worldPos, Input.normal, ambientColor, diffuseColor,
                       surfaceColor);

    fragColor = vec4(surfaceColor, 1);
}

// -----------------------------------------------------------------------------

shader FSJetcores(in GStoFSTrajectory Input, out vec4 fragColor)
{
    float value = Input.value;

    float shadowFactor = 1;

    if (enableSelfShadowing)
    {
        shadowFactor = 0;

        vec3 lightPosNDC = Input.lightSpacePos.xyz / Input.lightSpacePos.w;
        lightPosNDC = lightPosNDC * 0.5 + 0.5;

        float closestDepth = texture(shadowMap, lightPosNDC.xy).r;
        float currentDepth = lightPosNDC.z;
        const float cosTheta = dot(normalize(Input.normal), vec3(0, 0, 1));
        float bias = 0.005 * tan(acos(cosTheta));

        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

        const int kernelSize = 2;

        for(int x = -kernelSize; x <= kernelSize; ++x)
        {
            for(int y = -kernelSize; y <= kernelSize; ++y)
            {
                float depth = texture(shadowMap,
                                      lightPosNDC.xy + vec2(x, y) * texelSize).r;
                shadowFactor += (currentDepth - bias < depth) ? 1.0 : 0.0;
            }
        }

        shadowFactor /= (kernelSize * 2.0 + 1) * (kernelSize * 2.0 + 1);
    }

    float t = (value - tfMinimum) / (tfMaximum - tfMinimum);

    vec3 ambientColor = vec3(0);

    switch (colorMode)
    {
        // Default tube color mode
        case 0:
            ambientColor = geometryColor;
            break;

        // Pressure height color mode.
        case 1:
        // Variable color mode
        case 2:
            ambientColor = texture(transferFunction, t).rgb;
            break;
    }

    vec3 surfaceColor;
    vec3 diffuseColor = vec3(1) * shadowFactor;
    getBlinnPhongColor(Input.worldPos, Input.normal, ambientColor, diffuseColor,
                       surfaceColor);

    fragColor = vec4(surfaceColor, 1);
}

// -----------------------------------------------------------------------------

shader FSShadow(in GStoFSTrajectory Input, out vec4 fragColor)
{
    fragColor = vec4(shadowColor.rgba);
}

// -----------------------------------------------------------------------------

shader FSShadowMap(in GStoFSTrajectory Input, out vec4 fragColor)
{
    // gl_FragDepth = gl_FragCoord.z;
}

// -----------------------------------------------------------------------------

shader FSShadowGroundImage(in VStoFSShadowMap Input, out vec4 fragColor)
{
    float depthValue = texture(shadowMap, Input.texCoords).r;

    float shadowFactor = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float depth = texture(shadowMap,
                                  Input.texCoords + vec2(x, y) * texelSize).r;
            shadowFactor += (depth < 1) ? 1.0 : 0.0;
        }
    }

    shadowFactor /= 9.0;

    if (shadowFactor > 0)
    {
        fragColor = vec4(shadowColor.rgb, shadowColor.a * shadowFactor);
    }
    else
    {
        discard;
        //fragColor = vec4(1, 0, 0, 1);
    }
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Trajectory
{
    vs(420)=VSTrajectory();
    gs(420)=GSTrajectoryTube() : in(lines_adjacency),
                                        out(triangle_strip, max_vertices = 64);
    fs(420)=FSJetcores();
};

program TrajectoryShadow
{
    vs(420)=VSTrajectoryShadow();
    gs(420)=GSTrajectoryTubeShadow() : in(lines_adjacency),
                                        out(triangle_strip, max_vertices = 128);
    fs(420)=FSShadow();
};

program TrajectoryShadowMap
{
    vs(420)=VSTrajectory();
    gs(420)=GSTrajectoryTube() : in(lines_adjacency),
                                        out(triangle_strip, max_vertices = 64);
    fs(420)=FSShadowMap();
};

program ArrowHeads
{
    vs(420)=VSArrowHeads();
    gs(420)=GSSimpleArrowHead() : in(points), out(triangle_strip,
                                                  max_vertices = 64);
    fs(420)=FSJetcores();
};

program ArrowHeadsShadow
{
    vs(420)=VSArrowHeadsShadow();
    gs(420)=GSSimpleArrowShadow() : in(points), out(triangle_strip,
                                                    max_vertices = 128);
    fs(420)=FSShadow();
};

program ArrowHeadsShadowMap
{
    vs(420)=VSArrowHeads();
    gs(420)=GSSimpleArrowHead() : in(points), out(triangle_strip,
                                                  max_vertices = 64);
    fs(420)=FSShadowMap();
};

program WorldTubes
{
    vs(420)=VSWorldGeometry();
    gs(420)=GSSimpleTube() : in(lines), out(triangle_strip, max_vertices = 128);
    fs(420)=FSSurfaceColor();
};

program LonLatPLines
{
    vs(420)=VSLonLatPGeometry();
    gs(420)=GSSimpleLine() : in(lines), out(line_strip, max_vertices = 128);
    fs(420)=FSSimpleColor();
};

program LonLatPTubes
{
    vs(420)=VSLonLatPGeometry();
    gs(420)=GSSimpleTube() : in(lines), out(triangle_strip, max_vertices = 128);
    fs(420)=FSSurfaceColor();
};

program TickLines
{
    vs(420)=VSLonLatPGeometry();
    gs(420)=GSTickLine() : in(points), out(line_strip, max_vertices = 128);
    fs(420)=FSSimpleColor();
};

program TickTubes
{
    vs(420)=VSLonLatPGeometry();
    gs(420)=GSTickTube() : in(points), out(triangle_strip, max_vertices = 128);
    fs(420)=FSSurfaceColor();
};

program ShadowGroundMap
{
    vs(420)=VSShadowGroundImage();
    fs(420)=FSShadowGroundImage();
};


