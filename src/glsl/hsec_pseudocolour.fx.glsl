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
 ***                             CONSTANTS
 *****************************************************************************/

const float MISSING_VALUE = -999.E9;


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

interface GStoFS
{
    float   scalar;
    float   flag;
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain()
{
    // Pass vertex and instance ID to the geometry shader (i and j grid
    // indices).
    gl_Position = vec4(gl_VertexID, gl_InstanceID, 0, 0);
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

layout(r32f)
uniform image2D   sectionGrid;    // 2D array with the data values of the section
uniform float     worldZ;         // world z coordinate of this section
uniform mat4      mvpMatrix;      // model-view-projection
uniform sampler1D latLonAxesData; // 1D texture that holds both lon and lat
uniform int       latOffset;      // index at which lat data starts
uniform int       iOffset;        // grid index offsets if only a subregion
uniform int       jOffset;        //   of the grid shall be rendered

// bug if 'out GStoFS output' is used?
shader GSmain(out GStoFS vertex)
{
    // gl_Position.xy encodes the i and j grid indices of the grid point
    // processed by this shader instance.
    ivec2 ij = ivec2(gl_in[0].gl_Position.xy);

    int numLons = imageSize(sectionGrid).x;
    int numLats = imageSize(sectionGrid).y;
    int i = (ij.x + iOffset) % numLons;
    int j = ij.y + jOffset;

    float lon, lon_west, lon_east;
    lon = texelFetch(latLonAxesData, i, 0).a;
    if (i > 0)
        lon_west = mix(lon, texelFetch(latLonAxesData, i-1, 0).a, 0.5);
    else
        lon_west = lon;
    if (i < numLons-1)
        lon_east = mix(lon, texelFetch(latLonAxesData, i+1, 0).a, 0.5);
    else
        lon_east = lon;

    float lat, lat_north, lat_south;
    lat = texelFetch(latLonAxesData, j + latOffset, 0).a;
    if (j > 0)
        lat_north = mix(lat, texelFetch(latLonAxesData, j-1 + latOffset, 0).a, 0.5);
    else
        lat_north = lat;
    if (j < numLats-1)
        lat_south = mix(lat, texelFetch(latLonAxesData, j+1 + latOffset, 0).a, 0.5);
    else
        lat_south = lat;

    // Quad centre and scalar value are the same for all emitted vertices.
    vertex.scalar = imageLoad(sectionGrid, ij).r;
    if (vertex.scalar != MISSING_VALUE) vertex.flag = 0.; else vertex.flag = -100.;

    // Upper right vertex ...
    vec3 worldSpacePosition = vec3(lon_east, lat_north, worldZ);
    gl_Position             = mvpMatrix * vec4(worldSpacePosition, 1.);
    EmitVertex();

    // ... upper left ...
    worldSpacePosition = vec3(lon_west, lat_north, worldZ);
    gl_Position        = mvpMatrix * vec4(worldSpacePosition, 1.);
    EmitVertex();

    // ... lower right ...
    worldSpacePosition = vec3(lon_east, lat_south, worldZ);
    gl_Position        = mvpMatrix * vec4(worldSpacePosition, 1.);
    EmitVertex();

    // ... lower left.
    worldSpacePosition = vec3(lon_west, lat_south, worldZ);
    gl_Position        = mvpMatrix * vec4(worldSpacePosition, 1.);
    EmitVertex();
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler1D transferFunction; // 1D transfer function
uniform float     scalarMinimum;    // min/max data values to scale to 0..1
uniform float     scalarMaximum;

shader FSmain(in GStoFS input, out vec4 fragColour)
{

    // Discard the element if it is outside the model domain (no scalar value).
    //if (input.flag < 0.) discard;

    // Scale the scalar range to 0..1.
    float scalar_ = (input.scalar - scalarMinimum) / (scalarMaximum - scalarMinimum);

    // Fetch colour from the transfer function and apply shading term.
    fragColour = texture(transferFunction, scalar_);
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(430)=VSmain();
    gs(430)=GSmain() : in(points), out(triangle_strip, max_vertices = 4);
    fs(430)=FSmain();
};
