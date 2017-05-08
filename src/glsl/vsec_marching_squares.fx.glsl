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

interface GStoFS
{
    smooth float worldZ; // worldZ coordinate to test for
                         // vertical section bounds in the frag shader
};


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

layout(rg32f)
uniform image2D   sectionGrid;    // 2D array with the data values of the section
uniform vec2      pToWorldZParams;// parameters to convert p[hPa] to world z
uniform mat4      mvpMatrix;      // model-view-projection
uniform float     isoValue;       // iso value of the contour line
uniform sampler1D path;           // points along the vertical section path

/**
  Emit a vertex between the two edge points p1 and p2 of the currently
  processed grid cell. This function needs to be called twice for each line
  segment to be generated.

  Parameters are the world space position of p1 and p2, their intensities,
  and the iso value of the current contour line.
 */
float emit(vec3 p1_worldposition, vec3 p2_worldposition,
           float p1_intensity, float p2_intensity, float isoValue)
{
  float fraction = (isoValue - p1_intensity) / (p2_intensity - p1_intensity);
  vec3 vertex_worldposition = mix(p1_worldposition, p2_worldposition, fraction);
  gl_Position = mvpMatrix * vec4(vertex_worldposition, 1.0);
  return vertex_worldposition.z;
}


shader GSmain(out GStoFS Output)
{
    // Based on code from this (http://www.twodee.org/blog/?p=805) website.

    // gl_Position.xy contains gl_VertexID and gl_InstanceID. Interpret
    // gl_Position.y as the index to the waypoint ("m" in the vsec vertex
    // shader), and gl_Position.x as the index to the vertical model level ("k"
    // in the vsec vertex shader). The two indices denote the grid indices of
    // the upper left corner of the grid cell processed by this shader
    // instance.
    ivec2 ul = ivec2(gl_in[0].gl_Position.yx);
    ivec2 ur = ul + ivec2(1, 0);
    ivec2 ll = ul + ivec2(0, 1);
    ivec2 lr = ul + ivec2(1, 1);

    // First test whether the current point is inside the model domain. If not,
    // we can stop the shader execution here, as no valid scalar data is
    // available. See the vsec vertex shader for an explanation of the
    // following code.
    float mixI_l = texelFetch(path, 4*ul.x + 2, 0).a;
    float mixI_r = texelFetch(path, 4*ur.x + 2, 0).a;
    if ((mixI_l < 0) || (mixI_r < 0))
    {
        EndPrimitive();
        return;
    }

    // Load scalar and log(p) values from the "sectionGrid". Convert log(p) to
    // worldZ.
    vec2 ul_sectionGrid = imageLoad(sectionGrid, ul).rg;
    vec2 ur_sectionGrid = imageLoad(sectionGrid, ur).rg;
    vec2 ll_sectionGrid = imageLoad(sectionGrid, ll).rg;
    vec2 lr_sectionGrid = imageLoad(sectionGrid, lr).rg;
    ul_sectionGrid.g = (ul_sectionGrid.g - pToWorldZParams.x) * pToWorldZParams.y;
    ur_sectionGrid.g = (ur_sectionGrid.g - pToWorldZParams.x) * pToWorldZParams.y;
    ll_sectionGrid.g = (ll_sectionGrid.g - pToWorldZParams.x) * pToWorldZParams.y;
    lr_sectionGrid.g = (lr_sectionGrid.g - pToWorldZParams.x) * pToWorldZParams.y;

    // Load the scalar values at the four corner points of the grid cell ..
    float ul_intensity = ul_sectionGrid.r;
    float ur_intensity = ur_sectionGrid.r;
    float ll_intensity = ll_sectionGrid.r;
    float lr_intensity = lr_sectionGrid.r;

    // .. and the world space coordinates of these corner points.
    float lon_l = texelFetch(path, 4*ul.x    , 0).a;
    float lat_l = texelFetch(path, 4*ul.x + 1, 0).a;
    float lon_r = texelFetch(path, 4*ur.x    , 0).a;
    float lat_r = texelFetch(path, 4*ur.x + 1, 0).a;
    vec3 ul_worldposition = vec3(lon_l, lat_l, ul_sectionGrid.g);
    vec3 ur_worldposition = vec3(lon_r, lat_r, ur_sectionGrid.g);
    vec3 ll_worldposition = vec3(lon_l, lat_l, ll_sectionGrid.g);
    vec3 lr_worldposition = vec3(lon_r, lat_r, lr_sectionGrid.g);

    // ---- DEBUG statements -----
    //    gl_Position = mvpMatrix * vec4(lon_l, lat_l, 10., 1.0);
    //    EmitVertex();
    //    gl_Position = mvpMatrix * vec4(lon_r, lat_r, 10., 1.0);
    //    EmitVertex();
    //    gl_Position = mvpMatrix * vec4(ul_worldposition.xy, ul.y, 1.0);
    //    EmitVertex();
    //    gl_Position = mvpMatrix * vec4(ur_worldposition.xy, ul.y, 1.0);
    //    EmitVertex();
    //    gl_Position = mvpMatrix * vec4(ul_worldposition, 1.0);
    //    EmitVertex();
    //    gl_Position = mvpMatrix * vec4(ur_worldposition, 1.0);
    //    EmitVertex();
    //    EndPrimitive();
    //    return;

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
        Output.worldZ = emit(ll_worldposition, ul_worldposition,
                             ll_intensity, ul_intensity, isoValue); EmitVertex();
        Output.worldZ = emit(ll_worldposition, lr_worldposition,
                             ll_intensity, lr_intensity, isoValue); EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 2)
    {
        Output.worldZ = emit(ll_worldposition, lr_worldposition,
                             ll_intensity, lr_intensity, isoValue); EmitVertex();
        Output.worldZ = emit(lr_worldposition, ur_worldposition,
                             lr_intensity, ur_intensity, isoValue); EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 3)
    {
        Output.worldZ = emit(ll_worldposition, ul_worldposition,
                             ll_intensity, ul_intensity, isoValue); EmitVertex();
        Output.worldZ = emit(lr_worldposition, ur_worldposition,
                             lr_intensity, ur_intensity, isoValue); EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 4)
    {
        Output.worldZ = emit(ll_worldposition, ul_worldposition,
                             ll_intensity, ul_intensity, isoValue); EmitVertex();
        Output.worldZ = emit(ul_worldposition, ur_worldposition,
                             ul_intensity, ur_intensity, isoValue); EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 5)
    {
        Output.worldZ = emit(ll_worldposition, lr_worldposition,
                             ll_intensity, lr_intensity, isoValue); EmitVertex();
        Output.worldZ = emit(ul_worldposition, ur_worldposition,
                             ul_intensity, ur_intensity, isoValue); EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 6)
    {
        Output.worldZ = emit(ll_worldposition, ul_worldposition,
                             ll_intensity, ul_intensity, isoValue); EmitVertex();
        Output.worldZ = emit(ll_worldposition, lr_worldposition,
                             ll_intensity, lr_intensity, isoValue); EmitVertex();
        EndPrimitive();
        Output.worldZ = emit(ul_worldposition, ur_worldposition,
                             ul_intensity, ur_intensity, isoValue); EmitVertex();
        Output.worldZ = emit(lr_worldposition, ur_worldposition,
                             lr_intensity, ur_intensity, isoValue); EmitVertex();
        EndPrimitive();
    }
    else if (bitfield == 7)
    {
        Output.worldZ = emit(ul_worldposition, ur_worldposition,
                             ul_intensity, ur_intensity, isoValue); EmitVertex();
        Output.worldZ = emit(lr_worldposition, ur_worldposition,
                             lr_intensity, ur_intensity, isoValue); EmitVertex();
        EndPrimitive();
    }
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform vec4 colour;
uniform vec2  verticalBounds;  // lower and upper bound of this section. If
                               // worldZ is outside this interval discard
                               // the fragment.

uniform bool useTransferFunction;

uniform sampler1D transferFunction; // 1D transfer function
uniform float     scalarMinimum;    // min/max data values to scale to 0..1
uniform float     scalarMaximum;


shader FSmain(in GStoFS input, out vec4 fragColour)
{
    if ((input.worldZ < verticalBounds.x) || (input.worldZ > verticalBounds.y))
    {
        discard;
    }

    if (useTransferFunction)
    {
        // Scale the scalar range to 0..1.
        float scalar = (isoValue - scalarMinimum) / (scalarMaximum - scalarMinimum);

        // Fetch colour from the transfer function and apply shading term.
        fragColour = texture(transferFunction, scalar);
    }
    else
    {
        fragColour = colour;
    }
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
