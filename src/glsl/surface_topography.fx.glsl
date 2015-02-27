/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
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
 ***                             INTERFACES
 *****************************************************************************/

interface VStoFS
{
    smooth float  scalar;         // the scalar data that this vertex holds,
                                  // the value shall be perspectively correct
                                  // interpolated to the fragment shader
    smooth vec3   surfaceNormal;
};


/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform sampler1D latLonAxesData;    // 1D texture that holds both lon and lat
uniform int       latOffset;         // index at which lat data starts
uniform sampler2D dataField;         // 2D texture holding the scalar data
uniform sampler2D surfaceTopography; // pressure elevation field in Pa
uniform vec2      pToWorldZParams;   // parameters to convert p[hPa] to world z
uniform mat4      mvpMatrix;         // model-view-projection


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

/**
  Returns the position of the vertex located at grid space position (i,j) in
  world coordinates (lon, lat, ln p).
  */
vec3 fetchVertexPosition(int i, int j)
{
    // Fetch lat/lon/p values from texture.
    float lon   = texelFetch(latLonAxesData, i, 0).a;
    float lat   = texelFetch(latLonAxesData, j + latOffset, 0).a;
    float p_hPa = texelFetch(surfaceTopography, ivec2(i, j), 0).a / 100.;

    // Convert pressure to world Z coordinate.
    float z = (log(p_hPa) - pToWorldZParams.x) * pToWorldZParams.y;

    return vec3(lon, lat, z);
}


shader VSmain(out VStoFS Output)
{
    // Compute grid indices (i, j) of the this vertex (see notes 09Feb2012).
    int i = int(gl_VertexID / 2);
    int j = bool(gl_VertexID & 1) ? (gl_InstanceID + 1) : gl_InstanceID;

    vec3 vertexPosition = fetchVertexPosition(i, j);

    // Convert from world to clip space.
    gl_Position = mvpMatrix * vec4(vertexPosition, 1);

    // Fetch scalar value of this vertex from data texture.
    Output.scalar = texelFetch(dataField, ivec2(i, j), 0).a;

    // Compute the surface normal of this vertex: Compute the vectors between
    // this vertex and the four adjacent vertices. The cross product yields
    // four possible normals, of which we take the mean as the "true" normal.
    // See, for instance, the Lighthouse3D terrain tutorial:
    // http://www.lighthouse3d.com/opengl/terrain/index.php3?normals
    vec3 vThisToLeft   = fetchVertexPosition(i-1, j  ) - vertexPosition;
    vec3 vThisToTop    = fetchVertexPosition(i  , j-1) - vertexPosition;
    vec3 vThisToRight  = fetchVertexPosition(i+1, j  ) - vertexPosition;
    vec3 vThisToBottom = fetchVertexPosition(i  , j+1) - vertexPosition;

    if (i == 0) vThisToLeft = -vThisToRight;
    else if (i == textureSize(dataField, 0).x - 1) vThisToRight = -vThisToLeft;
    if (j == 0) vThisToTop = -vThisToBottom;
    else if (j == textureSize(dataField, 0).y - 1) vThisToBottom = -vThisToTop;

    vec3 cross1 = normalize(cross(vThisToLeft  , vThisToTop));
    vec3 cross2 = normalize(cross(vThisToTop   , vThisToRight));
    vec3 cross3 = normalize(cross(vThisToRight , vThisToBottom));
    vec3 cross4 = normalize(cross(vThisToBottom, vThisToLeft));

    Output.surfaceNormal = normalize(cross1 + cross2 + cross3 + cross4);
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler1D transferFunction; // 1D transfer function
uniform float     scalarMinimum;    // min/max data values to scale to 0..1
uniform float     scalarMaximum;
uniform vec3      lightDirection;   // light direction in world space

shader FSmain(in VStoFS Input, out vec4 fragColour)
{
    // Compute diffuse shading.
    float diffuse = 0.5 + 0.5 * abs(dot(normalize(lightDirection), Input.surfaceNormal));

    // Scale the scalar range to 0..1.
    float scalar_ = (Input.scalar - scalarMinimum) / (scalarMaximum - scalarMinimum);

    // Fetch colour from the transfer function and apply shading term.
    fragColour = texture(transferFunction, scalar_);
    fragColour = vec4(fragColour.rgb * diffuse, fragColour.a);
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(400)=VSmain();
    fs(400)=FSmain();
};
