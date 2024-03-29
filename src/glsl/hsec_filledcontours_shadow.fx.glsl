/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2017 Bianca Tost
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
    smooth float lon;     // holds the longitude coordinate of the fragment in
                          // world space coordinates
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

uniform float     shiftForWesternLon; // shift in multiples of 360 to get the
                                      // correct starting position (depends on
                                      // the distance between the left border of
                                      // the grid and the left border of the bbox)

shader VSmain(out VStoFS outStruct)
{
    // Compute grid indices (i, j) of the this vertex from vertex and instance
    // ID (see notes 09Feb2012).
    int i = int(gl_VertexID / 2);
    int j = bool(gl_VertexID & 1) ? (gl_InstanceID + 1) : gl_InstanceID;

    int numLons = latOffset;

    // In case of repeated grid region parts, this shift places the vertex to
    // the correct global position. This is necessary since the modulo operation
    // maps repeated parts to the same position. It needs to be mulitplied by a
    // factor of 360 to place the vertex correctly.
    float numGlobalLonShifts = floor((i + iOffset) / latOffset);

    i = (i + iOffset) % numLons;
    j = j + jOffset;

    // Fetch lat/lon values from texture to obtain the position of this vertex
    // in world space.
    float lon = texelFetch(latLonAxesData, i, 0).a;
    float lat = texelFetch(latLonAxesData, j + latOffset, 0).a;

    lon += (numGlobalLonShifts * 360.) + shiftForWesternLon;

    vec3 vertexPosition = vec3(lon, lat, worldZ);

    // Convert the position from world to clip space.
    gl_Position = mvpMatrix * vec4(vertexPosition, 1);

    // NOTE: imageLoad requires "targetGrid" to be defined with the
    // layout qualifier, otherwise an error "..no overload function can
    // be found: imageLoad(struct image2D, ivec2).." is raised.
    vec4 data = imageLoad(crossSectionGrid, ivec2(i, j));
    outStruct.scalar = data.r;
    outStruct.lon = lon;

    if (outStruct.scalar != MISSING_VALUE) outStruct.flag = 0.; else outStruct.flag = -100.;
}

/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

// contains color of shadow
uniform vec4 colour;

uniform sampler1D transferFunction; // 1D transfer function
uniform float     scalarMinimum;    // min/max data values to scale to 0..1
uniform float     scalarMaximum;

uniform bool      isCyclicGrid;   // indicates whether the grid is cyclic or not
uniform float     leftGridLon;    // leftmost longitude of the grid
uniform float     eastGridLon;    // eastmost longitude of the grid

shader FSmain(in VStoFS inStruct, out vec4 fragColour)
{
    // Discard the element if it is outside the model domain (no scalar value).
    if (inStruct.flag < 0. || inStruct.lon < bboxLons.x || inStruct.lon > bboxLons.y)
    {
        discard;
    }

    // In the case of the rendered region falling apart into disjunct region
    // discard fragments between separated regions.
    // (cf. computeRenderRegionParameters of MNWP2DHorizontalActorVariable in
    // nwpactorvariable.cpp).
    if (!isCyclicGrid
            && (mod(inStruct.lon - leftGridLon, 360.) >= (eastGridLon - leftGridLon)))
    {
        discard;
    }

    // Scale the scalar range to 0..1.
    float scalar_ = (inStruct.scalar - scalarMinimum) / (scalarMaximum - scalarMinimum);

    // Fetch colour from the transfer function and apply shading term.
    vec4 colourOut = texture(transferFunction, scalar_);

    if (colourOut.a < 0.001)
    {
        discard;
    }

    fragColour = vec4(colour.rgb, colour.a * colourOut.a);
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(420)=VSmain();
    fs(420)=FSmain();
};

