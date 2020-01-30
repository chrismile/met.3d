/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2018 Fabian Schoettl
**  Copyright 2018 Bianca Tost
**  Copyright 2018 Marc Rautenhaus
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
*******************************************************************************/

#include "hsec_sample_data_routine.fx.glsl"

/******************************************************************************
***                             CONSTANTS                                   ***
*******************************************************************************/

const float M_PI = 3.1415926535897932384626433832795;
const int numVerts = 8;


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

interface GStoFS
{
    smooth float pressure;
    smooth vec4 data;
    smooth vec3 normal;
};


interface VStoGS
{
    float adaptedArrowLength;
    float adaptedArrowRadius;
    float adaptedLineWidth;
};

/******************************************************************************
 ***                             UNIFORMS
 ******************************************************************************/

// parameters to convert p[hPa] to world z
uniform vec2 pToWorldZParams;
// model-view-projection
uniform mat4  mvpMatrix;
// inverse model-view-projection
uniform mat4  mvpMatrixInv;

uniform int thinOutRateX;
uniform int thinOutRateY;
uniform int minSelection;
uniform int maxSelection;

uniform sampler2D altitudeBuffer;
uniform sampler1D inSituDataBuffer;
uniform sampler2D curtainDataBuffer;

uniform vec4      glyphColor;

uniform sampler1D transferFunction;
uniform float     scalarMinimum;
uniform float     scalarMaximum;
uniform bool      transferFunctionValid;

uniform vec3      lightDirection;
uniform vec4      shadowColor;

uniform float arrowRadius;
uniform float arrowLength;
uniform float arrowRef;

uniform int pivotPosition;

// Variables for use in horizontal cross-section actor.
// Sample textures for longitudial/latitudial component of vector field.
uniform float       deltaGridX; // delta between two points in x
uniform float       deltaGridY; // delta between two points in y
uniform float       worldZ;     // world z coordinate of this section

uniform sampler3D   vectorDataLonComponent3D;
uniform sampler3D   vectorDataLatComponent3D;

uniform sampler2D   vectorDataLonComponent2D;
uniform sampler2D   vectorDataLatComponent2D;

uniform sampler2D   surfacePressureLon;    // surface pressure field in Pa
uniform sampler1D   hybridCoefficientsLon; // hybrid sigma pressure coefficients
uniform sampler2D   surfacePressureLat;    // surface pressure field in Pa
uniform sampler1D   hybridCoefficientsLat; // hybrid sigma pressure coefficients
uniform sampler3D   auxPressureFieldLon_hPa; // 3D pressure field in hPa
uniform sampler3D   auxPressureFieldLat_hPa; // 3D pressure field in hPa

uniform float       lineWidth;

uniform bool        scaleArrowWithMagnitude;
uniform float       scaleArrowMinScalar;
uniform float       scaleArrowMinLength;
uniform bool        scaleArrowDiscardBelow;
uniform float       scaleArrowMaxScalar;
uniform float       scaleArrowMaxLength;
uniform bool        scaleArrowDiscardAbove;

// =============================================================================


// Check if current data point should be culled.
bool checkSelection(int x, int y)
{
    if (mod(y, thinOutRateY) != 0)
        return false;

    if (x < minSelection || x >= maxSelection)
        return false;

    if (mod(x, thinOutRateX) != 0)
        return false;

    return true;
}


// Read map data value to color via transfer function.
vec4 readTransferFunction(float scalar)
{
    if (transferFunctionValid)
    {
        float s = (scalar - scalarMinimum) / (scalarMaximum - scalarMinimum);
        return texture(transferFunction, s);
    }
    else
        return vec4(vec3(0.2), 1);
}


/******************************************************************************
***                           VERTEX SHADER                                 ***
*******************************************************************************/

shader VSCurtainVertex(in vec3 vertex : 0, out vec4 vs_vertex,
                       out vec4 vs_data, out VStoGS Output)
{
    float pressure = texelFetch(altitudeBuffer,
                                ivec2(gl_VertexID, gl_InstanceID), 0).x;
    vec4 data = texelFetch(curtainDataBuffer,
                           ivec2(gl_VertexID, gl_InstanceID), 0).xyzw;

    // Convert pressure to world Z coordinate.
    float worldPosZ = (log(pressure) - pToWorldZParams.x) * pToWorldZParams.y;

    // Convert from world space to clip space.
    vs_vertex = vec4(vertex.xy, worldPosZ, pressure);

    vs_data = data;
    vs_data.z *= -1;

    Output.adaptedArrowLength = arrowLength;
    if (arrowRef > 0)
    {
        Output.adaptedArrowLength *= length(vs_data.xyz) / arrowRef;
    }
    Output.adaptedArrowRadius = arrowRadius;
    Output.adaptedLineWidth   = 0.3;

    if (!checkSelection(gl_VertexID, gl_InstanceID))
        vs_data.w = 0;
}


shader VSInSituVertex(in vec3 vertex : 0, out vec4 vs_vertex, out vec4 vs_data,
                      out VStoGS Output)
{
    vec4 data = texelFetch(inSituDataBuffer, gl_VertexID, 0).xyzw;

    // Convert pressure to world Z coordinate.
    float worldPosZ = (log(vertex.z) - pToWorldZParams.x) * pToWorldZParams.y;

    // Convert from world space to clip space.
    vs_vertex = vec4(vertex.xy, worldPosZ, vertex.z);

    vs_data = data;
    vs_data.z *= -1;

    Output.adaptedArrowLength = arrowLength;
    if (arrowRef > 0)
    {
        Output.adaptedArrowLength *= length(vs_data.xyz) / arrowRef;
    }
    Output.adaptedArrowRadius = arrowRadius;
    Output.adaptedLineWidth = 0.3;

    if (!checkSelection(gl_VertexID, 0))
        vs_data.w = 0;
}


shader VSHSecVertex(in vec2 vertex : 0, out vec4 vs_vertex, out vec4 vs_data,
                    out VStoGS Output)
{
    vec3 worldPos = vec3(vertex.xy, worldZ);

    // Sample both vector field component textures, obtain scalar value in
    // longitudial and latitudial direction.
    float lonField = sampleDataAtPos(
                worldPos, vectorDataLonComponent3D, vectorDataLonComponent2D,
                surfacePressureLon, hybridCoefficientsLon,
                auxPressureFieldLon_hPa);
    float latField = sampleDataAtPos(
                worldPos, vectorDataLatComponent3D, vectorDataLatComponent2D,
                surfacePressureLat, hybridCoefficientsLat,
                auxPressureFieldLat_hPa);

    // If no data is available, do not draw any glyphs.
    if (lonField == MISSING_VALUE || latField == MISSING_VALUE)
    {
        vs_data.w = 0;
        return;
    }

    // Optain vector field direction at posWorld (e.g. wind).
    vec3 magnitudeDir = vec3(lonField, latField, 0);

    vec4 data = vec4(magnitudeDir, 1).xyzw;

    float deltaGrid = (deltaGridX + deltaGridY) / 2.0f;
    Output.adaptedArrowLength = 0.75 * deltaGrid;

    Output.adaptedArrowRadius = arrowRadius * deltaGrid;

    if (scaleArrowWithMagnitude)
    {
        float magnitude = length(magnitudeDir);
        if (scaleArrowDiscardBelow && magnitude < scaleArrowMinScalar)
        {
            vs_data.w = 0;
            return;
        }
        if (scaleArrowDiscardAbove && magnitude > scaleArrowMaxScalar)
        {
            vs_data.w = 0;
            return;
        }

        float scalar = min(max(magnitude, scaleArrowMinScalar),
                           scaleArrowMaxScalar);
        float scalarScale = (scalar - scaleArrowMinScalar)
                / (scaleArrowMaxScalar - scaleArrowMinScalar);
        float lengthScale =
                scaleArrowMinLength + scalarScale *
                (scaleArrowMaxLength - scaleArrowMinLength);

        Output.adaptedArrowLength = lengthScale * deltaGrid;
    }

    Output.adaptedLineWidth = deltaGrid * lineWidth;

    // Convert from world space to clip space.
    vs_vertex = vec4(worldPos, pressure_hPa);

    vs_data = data;
}


/******************************************************************************
***                          GEOMETRY SHADER                                ***
*******************************************************************************/

// Returns vector which represents vec rotated by angle around axis.
vec3 rotVector(vec3 vec, vec3 axis, float angle)
{
    vec3 a = normalize(vec);
    vec3 b = normalize(axis);

    vec3 a1 = (dot(a, b) / dot(b, b)) * b;
    vec3 a2 = a - a1;

    vec3 w = cross(b, a2);
    float x1 = cos(angle) / length(a2);
    float x2 = sin(angle) / length(w);

    vec3 a3 = length(a2) * (x1 * a2 + x2 * w);

    return a3 + a1;
}


shader GSArrow(in vec4 vs_vertex[], in vec4 vs_data[], in VStoGS Input[],
               out GStoFS Output)
{
    if (vs_data[0].w != 1)
        return;

    float arrowTip = 0.6;
    float baseWidth = Input[0].adaptedLineWidth;

    vec3 dir = normalize(vs_data[0].xyz);

    float l = Input[0].adaptedArrowLength;

    vec3 start = vs_vertex[0].xyz;
    // Place pivot at glyph tip.
    if (pivotPosition == 0)
    {
        start = vs_vertex[0].xyz - dir * l;
    }
    // Place pivot at glyph centre.
    else if (pivotPosition == 1)
    {
        start = vs_vertex[0].xyz - 0.5 * dir * l;
    }
    // Place pivot at glyph tail.
    else if (pivotPosition == 2)
    {
        start = vs_vertex[0].xyz;
    }

    vec3 norm;
    if (dir.x == 0 && dir.y == 0)
    {
        norm = vec3(1, 0, 0);
    }
    else
    {
        norm = normalize(vec3(-dir.y, dir.x, 0));
    }

    vec3 end = start + dir * l;

    Output.data = vs_data[0];
    Output.pressure = vs_vertex[0].w;

    vec3 verts[numVerts];

    for (int i = 0; i < numVerts; i++)
    {
        verts[i] = rotVector(norm, dir, 2 * M_PI / numVerts * i);
    }

    vec3 tip = start + dir * arrowTip * l;


    /*******************************/
    /*     /\       <- tip         */
    /*    /  \                     */
    /*   ------     <- tip cover   */
    /*     ||                      */
    /*     ||       <- base        */
    /*     --       <- base cover  */
    /*******************************/

    // Build base cover geometry.
    for (int i = 0; i < numVerts; i++)
    {
        int index = int(ceil(i / 2.0));
        if (i % 2 != 0)
        {
            index = numVerts - index;
        }

        Output.normal = -dir;
        gl_Position = mvpMatrix * vec4(start + verts[index] * baseWidth, 1);
        EmitVertex();
    }
    EndPrimitive();

    // Build base geometry.
    for (int i = 0; i <= numVerts; i++)
    {
        int j = i;
        if (j == numVerts)
        {
            j = 0;
        }

        Output.normal = verts[j];
        gl_Position = mvpMatrix * vec4(start + verts[j] * baseWidth, 1);
        EmitVertex();

        gl_Position = mvpMatrix * vec4(tip + verts[j] * baseWidth, 1);
        EmitVertex();
    }
    EndPrimitive();

    // Build tip cover geometry.
    for (int i = 0; i < numVerts; i++)
    {
        int index = int(ceil(i / 2.0));
        if (i % 2 != 0)
        {
            index = numVerts - index;
        }

        Output.normal = -dir;
        gl_Position = mvpMatrix
                * vec4(tip + verts[index] * Input[0].adaptedArrowRadius, 1);
        EmitVertex();
    }
    EndPrimitive();

    // Build tip geometry.
    for (int i = 0; i <= numVerts; i++)
    {
        int j = i;
        if (j == numVerts)
            j = 0;

        vec3 b = (tip + verts[j] * Input[0].adaptedArrowRadius) - end;
        vec3 a = verts[j] * Input[0].adaptedArrowRadius;

        vec3 a1 = dot(a, b) * normalize(b);
        vec3 a2 = a - a1;

        Output.normal = normalize(a2);
        gl_Position = mvpMatrix
                * vec4(tip + verts[j] * Input[0].adaptedArrowRadius, 1);
        EmitVertex();

        Output.normal = vec3(0, 0, 0);
        gl_Position = mvpMatrix * vec4(end, 1);
        EmitVertex();
    }
    EndPrimitive();
    return;
}


shader GSArrowShadow(in vec4 vs_vertex[], in vec4 vs_data[], in VStoGS Input[])
{
    if (vs_data[0].w != 1)
        return;

    float arrowTip = 0.6;
    float baseWidth = Input[0].adaptedLineWidth;

    float l = Input[0].adaptedArrowLength;

    vec3 dir = normalize(vs_data[0].xyz);
    dir.z = 0;

    vec3 start = vs_vertex[0].xyz;
    // Place pivot at glyph tip.
    if (pivotPosition == 0)
    {
        start = vs_vertex[0].xyz - dir * l;
    }
    // Place pivot at glyph centre.
    else if (pivotPosition == 1)
    {
        start = vs_vertex[0].xyz - 0.5 * dir * l;
    }
    // Place pivot at glyph tail.
    else if (pivotPosition == 2)
    {
        start = vs_vertex[0].xyz;
    }
    start.z = 0.1;

    vec3 norm;
    if (dir.x == 0 && dir.y == 0)
    {
        norm = vec3(1, 0, 0);
    }
    else
    {
        norm = normalize(vec3(-dir.y, dir.x, 0));
    }

    // Build base shadow geometry.
    gl_Position = mvpMatrix * vec4(start + norm * baseWidth , 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(start - norm * baseWidth, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(start + norm * baseWidth
                                   + dir * arrowTip * l, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(start - norm * baseWidth
                                   + dir * arrowTip * l, 1);
    EmitVertex();
    EndPrimitive();

    // Build tip shadow geometry.
    gl_Position = mvpMatrix * vec4(start + norm * Input[0].adaptedArrowRadius
            + dir * arrowTip * l, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(start + dir * l, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(start - norm * Input[0].adaptedArrowRadius
            + dir * arrowTip * l, 1);
    EmitVertex();
    EndPrimitive();
}

/******************************************************************************
***                          FRAGMENT SHADER                                ***
*******************************************************************************/

shader FSArrowFixed(in GStoFS Input, out vec4 fragColor)
{
    fragColor = vec4(glyphColor.rgb, 1);

    vec3 normal = normalize(Input.normal);
    fragColor.rgb *= max(dot(normalize(-lightDirection), normal), 0) * 0.8 + 0.2;
}


shader FSArrowPressure(in GStoFS Input, out vec4 fragColor)
{
    fragColor = readTransferFunction(Input.pressure * 100);

    vec3 normal = normalize(Input.normal);
    fragColor.rgb *= max(dot(normalize(-lightDirection), normal), 0) * 0.8 + 0.2;
}


shader FSArrowData(in GStoFS Input, out vec4 fragColor)
{
    fragColor = readTransferFunction(length(Input.data.xyz));

    vec3 normal = normalize(Input.normal);
    fragColor.rgb *= max(dot(normalize(-lightDirection), normal), 0) * 0.8 + 0.2;
}


shader FSShadow(out vec4 fragColor)
{
    fragColor = vec4(shadowColor);
}

/******************************************************************************
***                             PROGRAMS                                    ***
*******************************************************************************/


program CurtainArrowFixed
{
    vs(400) = VSCurtainVertex();
    gs(400) = GSArrow() : in(points), out(triangle_strip, max_vertices = 128);
    fs(400) = FSArrowFixed();
};


program CurtainArrowPressure
{
    vs(400) = VSCurtainVertex();
    gs(400) = GSArrow() : in(points), out(triangle_strip, max_vertices = 128);
    fs(400) = FSArrowPressure();
};


program CurtainArrowData
{
    vs(400) = VSCurtainVertex();
    gs(400) = GSArrow() : in(points), out(triangle_strip, max_vertices = 128);
    fs(400) = FSArrowData();
};


program InSituArrowFixed
{
    vs(400) = VSInSituVertex();
    gs(400) = GSArrow() : in(points), out(triangle_strip, max_vertices = 128);
    fs(400) = FSArrowFixed();
};


program InSituArrowPressure
{
    vs(400) = VSInSituVertex();
    gs(400) = GSArrow() : in(points), out(triangle_strip, max_vertices = 128);
    fs(400) = FSArrowPressure();
};


program InSituArrowData
{
    vs(400) = VSInSituVertex();
    gs(400) = GSArrow() : in(points), out(triangle_strip, max_vertices = 128);
    fs(400) = FSArrowData();
};


program HSecArrowData
{
    vs(400) = VSHSecVertex();
    gs(400) = GSArrow() : in(points), out(triangle_strip, max_vertices = 128);
    fs(400) = FSArrowFixed();
};


program CurtainArrowShadow
{
    vs(400) = VSCurtainVertex();
    gs(400) = GSArrowShadow() : in(points), out(triangle_strip,
                                                max_vertices = 128);
    fs(400) = FSShadow();
};


program InSituArrowShadow
{
    vs(400) = VSInSituVertex();
    gs(400) = GSArrowShadow() : in(points), out(triangle_strip,
                                                max_vertices = 128);
    fs(400) = FSShadow();
};


program HSecArrowShadow
{
    vs(400) = VSHSecVertex();
    gs(400) = GSArrowShadow() : in(points), out(triangle_strip,
                                                max_vertices = 128);
    fs(400) = FSShadow();
};
