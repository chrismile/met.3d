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
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain()
{
    gl_Position = vec4(gl_VertexID, gl_InstanceID, 0, 0);
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

uniform mat4      mvpMatrix;      // model-view-projection
layout(r32f)
uniform image2D   sectionGrid;    // 2D array with the data values of the section
uniform float     worldZ;         // world z coordinate of this section
uniform sampler1D latLonAxesData; // 1D texture that holds both lon and lat
uniform int       latOffset;      // index at which lat data starts
uniform int       iOffset;        // grid index offsets if only a subregion
uniform int       jOffset;        //   of the grid shall be rendered
uniform float     isoValue;       // iso value of the contour line
uniform vec2      bboxLons;       // western and eastern lon of the bbox

/**
  Emit a vertex between the two edge points p1 and p2 of the currently
  processed grid cell. This function needs to be called twice for each line
  segment to be generated.

  Parameters are the world space position of p1 and p2, their intensities,
  and the iso value of the current contour line.
 */
void emit(vec2 p1_worldposition, vec2 p2_worldposition,
          float p1_intensity, float p2_intensity, float isoValue)
{
  float fraction = (isoValue - p1_intensity) / (p2_intensity - p1_intensity);
  vec2 vertex_worldposition = mix(p1_worldposition, p2_worldposition, fraction);
  gl_Position = mvpMatrix * vec4(vertex_worldposition, worldZ, 1.0);
//FIX (06Dec2016, mr) -- emit vertex has been moved out of the function
//                       in order to run with newer Nvidia drivers -- check this
}


// we do not need any input, we use gl_in instead:
/*
    in gl_PerVertex
    {
          vec4 gl_Position;
          float gl_PointSize;
          float gl_ClipDistance[];
    } gl_in[];   */

shader GSmain()
{
    // Based on code from this (http://www.twodee.org/blog/?p=805) website.

    int numLons = latOffset;

    // gl_Position.xy encodes the i and j grid indices of the upper left corner
    // of the grid cell processed by this shader instance.
    ivec2 ul = ivec2(gl_in[0].gl_Position.xy) + ivec2(iOffset, jOffset);
    ul.x = ul.x % numLons;
    ivec2 ur = ul + ivec2(1, 0);
    ur.x = ur.x % numLons;
    ivec2 ll = ul + ivec2(0, 1);
    ivec2 lr = ur + ivec2(0, 1);

    // Load the scalar values at the four corner points of the grid cell.
    float ul_intensity = imageLoad(sectionGrid, ul).r;
    float ur_intensity = imageLoad(sectionGrid, ur).r;
    float ll_intensity = imageLoad(sectionGrid, ll).r;
    float lr_intensity = imageLoad(sectionGrid, lr).r;

    // .. and the world space coordinates of these corner points.
    vec2 ul_worldposition = vec2(texelFetch(latLonAxesData, ul.x, 0).a,
                                 texelFetch(latLonAxesData, ul.y + latOffset, 0).a);
    vec2 ur_worldposition = vec2(texelFetch(latLonAxesData, ur.x, 0).a,
                                 ul_worldposition.y);
    vec2 ll_worldposition = vec2(ul_worldposition.x,
                                 texelFetch(latLonAxesData, ll.y + latOffset, 0).a);
    vec2 lr_worldposition = vec2(ur_worldposition.x,
                                 ll_worldposition.y);

    // Check if the longitude fetched from the grid axis is within the
    // requested bounding box. If not, shift (see notes 17Apr2012).
    float bboxWestLon = bboxLons.x;
    float bboxEastLon = bboxLons.y;
    if (ul_worldposition.x > bboxEastLon)
    {
        ul_worldposition.x += int((bboxWestLon - ul_worldposition.x) / 360.) * 360.;
        ll_worldposition.x = ul_worldposition.x;
    }
    else if (ul_worldposition.x < bboxWestLon)
    {
        ul_worldposition.x += int((bboxEastLon - ul_worldposition.x) / 360.) * 360.;
        ll_worldposition.x = ul_worldposition.x;
    }
    if (ur_worldposition.x > bboxEastLon)
    {
        ur_worldposition.x += int((bboxWestLon - ur_worldposition.x) / 360.) * 360.;
        lr_worldposition.x = ur_worldposition.x;
    }
    else if (ur_worldposition.x < bboxWestLon)
    {
        ur_worldposition.x += int((bboxEastLon - ur_worldposition.x) / 360.) * 360.;
        lr_worldposition.x = ur_worldposition.x;
    }

    // Determine the marching squares case we are handling..
    int bitfield = 0;
    bitfield += int(ll_intensity < isoValue) * 1;
    bitfield += int(lr_intensity < isoValue) * 2;
    bitfield += int(ul_intensity < isoValue) * 4;
    bitfield += int(ur_intensity < isoValue) * 8;
    // ..use symmetry.
    if (bitfield > 7)
    {
        bitfield = 15 - bitfield;
    }

    // Emit vertices according to the case determined above.
    if (bitfield == 0)
    {
        EndPrimitive();
    }
    else if (bitfield == 1)
    {
        emit(ll_worldposition, ul_worldposition, ll_intensity, ul_intensity, isoValue);
        EmitVertex();
        emit(ll_worldposition, lr_worldposition, ll_intensity, lr_intensity, isoValue);
        EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 2)
    {
        emit(ll_worldposition, lr_worldposition, ll_intensity, lr_intensity, isoValue);
        EmitVertex();
        emit(lr_worldposition, ur_worldposition, lr_intensity, ur_intensity, isoValue);
        EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 3)
    {
        emit(ll_worldposition, ul_worldposition, ll_intensity, ul_intensity, isoValue);
        EmitVertex();
        emit(lr_worldposition, ur_worldposition, lr_intensity, ur_intensity, isoValue);
        EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 4)
    {
        emit(ll_worldposition, ul_worldposition, ll_intensity, ul_intensity, isoValue);
        EmitVertex();
        emit(ul_worldposition, ur_worldposition, ul_intensity, ur_intensity, isoValue);
        EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 5)
    {
        emit(ll_worldposition, lr_worldposition, ll_intensity, lr_intensity, isoValue);
        EmitVertex();
        emit(ul_worldposition, ur_worldposition, ul_intensity, ur_intensity, isoValue);
        EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 6)
    {
        emit(ll_worldposition, ul_worldposition, ll_intensity, ul_intensity, isoValue);
        EmitVertex();
        emit(ll_worldposition, lr_worldposition, ll_intensity, lr_intensity, isoValue);
        EmitVertex();
        EndPrimitive();
        emit(ul_worldposition, ur_worldposition, ul_intensity, ur_intensity, isoValue);
        EmitVertex();
        emit(lr_worldposition, ur_worldposition, lr_intensity, ur_intensity, isoValue);
        EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 7)
    {
        emit(ul_worldposition, ur_worldposition, ul_intensity, ur_intensity, isoValue);
        EmitVertex();
        emit(lr_worldposition, ur_worldposition, lr_intensity, ur_intensity, isoValue);
        EmitVertex();
        EndPrimitive();
    }
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform vec4 colour;

shader FSmain(out vec4 fragColour)
{
    fragColour = colour;
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(420)=VSmain();
    gs(420)=GSmain() : in(points), out(line_strip, max_vertices = 4);
    fs(420)=FSmain();
};
