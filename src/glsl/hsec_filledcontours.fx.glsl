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

interface VStoFS
{
    smooth float  scalar; // the scalar data that this vertex holds,
                          // the value shall be perspectively correct
                          // interpolated to the fragment shader
//TODO: is there a more elegant way to do this?
    smooth float  flag;   // if this flag is set to < 0, the fragment
                          // shader should discard the fragment (for
                          // invalid vertices beneath the surface)
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

uniform sampler1D latLonAxesData;    // 1D texture that holds both lon and lat
uniform int       latOffset;         // index at which lat axis data starts
uniform float     worldZ;            // world z coordinate of this section
uniform mat4      mvpMatrix;         // model-view-projection

layout(r32f)
uniform image2D   crossSectionGrid;  // 2D data grid
uniform int       iOffset;           // grid index offsets if only a subregion
uniform int       jOffset;           //   of the grid shall be rendered
uniform vec2      bboxLons;          // western and eastern lon of the bbox

shader VSmain(out VStoFS output)
{
    // Compute grid indices (i, j) of the this vertex from vertex and instance
    // ID (see notes 09Feb2012).
    int i = int(gl_VertexID / 2);
    int j = bool(gl_VertexID & 1) ? (gl_InstanceID + 1) : gl_InstanceID;

    int numLons = latOffset;
    i = (i + iOffset) % numLons;
    j = j + jOffset;

    // Fetch lat/lon values from texture to obtain the position of this vertex
    // in world space.
    float lon = texelFetch(latLonAxesData, i, 0).a;
    float lat = texelFetch(latLonAxesData, j + latOffset, 0).a;

    // Check if the longitude fetched from the grid axis is within the
    // requested bounding box. If not, shift (see notes 17Apr2012).
    float bboxWestLon = bboxLons.x;
    float bboxEastLon = bboxLons.y;
    if (lon > bboxEastLon) lon += int((bboxWestLon - lon) / 360.) * 360.;
    if (lon < bboxWestLon) lon += int((bboxEastLon - lon) / 360.) * 360.;

    vec3 vertexPosition = vec3(lon, lat, worldZ);

    // Convert the position from world to clip space.
    gl_Position = mvpMatrix * vec4(vertexPosition, 1);

    // NOTE: imageLoad requires "targetGrid" to be defined with the
    // layout qualifier, otherwise an error "..no overload function can
    // be found: imageLoad(struct image2D, ivec2).." is raised.
    vec4 data = imageLoad(crossSectionGrid, ivec2(i,j));
    output.scalar = data.r;

    if (output.scalar != MISSING_VALUE) output.flag = 0.; else output.flag = -100.;
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler1D transferFunction; // 1D transfer function
uniform float     scalarMinimum;    // min/max data values to scale to 0..1
uniform float     scalarMaximum;

shader FSmain(in VStoFS input, out vec4 fragColour)
{
    // Discard the element if it is outside the model domain (no scalar value).
    if (input.flag < 0.) discard;

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
    vs(420)=VSmain();
    fs(420)=FSmain();
};

