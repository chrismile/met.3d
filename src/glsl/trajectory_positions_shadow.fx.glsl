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

const float M_INVALID_TRAJECTORY_POS = -999.99;

/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

interface GStoFS
{
    vec3  gs_worldSpacePosition; // output: world space position
    float gs_scalar;             // scalar value copied from vertex.w
    vec3  gs_centre;             // centre of the sphere in world space
    float gs_radius;             // (scaled) radius of the sphere
};


/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform vec2 pToWorldZParams; // parameters to convert p[hPa] to world z

uniform mat4  mvpMatrix;             // model-view-projection
uniform float radius;                // radius of the sphere in world space
uniform bool  scaleRadius;           // if true, radius will be scaled with
                                     // distance camera-vertex
uniform vec3  cameraPosition;        // camera position in world space

uniform bool      useTransferFunction;   // use transfer function or const col?
uniform vec4      constColour;
uniform sampler1D transferFunction;      // 1D transfer function
uniform float     scalarMinimum;         // min/max data values to scale to 0..1
uniform float     scalarMaximum;

// whether the input vertices are already given in world space
uniform int isInputWorldSpace = 0;


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec3 vertex : 0, out vec4 vs_vertex)
{
    vec3 position = vertex;
    if (isInputWorldSpace == 0)
    {
        // Convert pressure to world Z coordinate.
        float worldZ = (log(vertex.z) - pToWorldZParams.x) * pToWorldZParams.y;
        position.z = worldZ;
    }
    // Convert from world space to clip space.
    vs_vertex = vec4(vertex.xy, position.z, vertex.z);
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

shader GSmain(in vec4 vs_vertex[], out GStoFS Output)
{
    Output.gs_radius = radius;

    // Don't draw spheres at invalid positions.
    if (vs_vertex[0].x == M_INVALID_TRAJECTORY_POS
            || vs_vertex[0].y == M_INVALID_TRAJECTORY_POS
            || vs_vertex[0].z == M_INVALID_TRAJECTORY_POS)
    {
        return;
    }

    if (scaleRadius)
    {
        // Vector from camera to sphere centre.
        vec3 view  = vs_vertex[0].xyz - cameraPosition;
        Output.gs_radius = radius * length(view) / 100.;
    }

    // Sphere centre and scalar value are the same for all emitted vertices.
    Output.gs_centre = vec3(vs_vertex[0].xy, 0.1);
    Output.gs_scalar = vs_vertex[0].w;
    vec3 scaled_right = Output.gs_radius * vec3(1, 0, 0);
    vec3 scaled_up    = Output.gs_radius * vec3(0, 1, 0);

    // Upper right vertex ...
    Output.gs_worldSpacePosition = vec3(Output.gs_centre + scaled_right + scaled_up);
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    EmitVertex();

    // ... upper left ...
    Output.gs_worldSpacePosition = vec3(Output.gs_centre - scaled_right + scaled_up);
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    EmitVertex();

    // ... lower right ...
    Output.gs_worldSpacePosition = vec3(Output.gs_centre + scaled_right - scaled_up);
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    EmitVertex();

    // ... lower left.
    Output.gs_worldSpacePosition = vec3(Output.gs_centre - scaled_right - scaled_up);
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    EmitVertex();
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

shader FSmain(in GStoFS Input, out vec4 fragColour)
{
    vec3 distanceToCentre = Input.gs_worldSpacePosition - Input.gs_centre;
    if (length(distanceToCentre) > Input.gs_radius) discard;

    if (useTransferFunction)
    {
        // Scale the scalar range to 0..1.
        float scalar_ = (Input.gs_scalar - scalarMinimum) /
                (scalarMaximum - scalarMinimum);
        // Fetch colour from the transfer function.
        fragColour = texture(transferFunction, scalar_);
    }
    else
    {
        fragColour = constColour;
    }
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(400)=VSmain();
    gs(400)=GSmain() : in(points), out(triangle_strip, max_vertices = 4);
    fs(400)=FSmain();
};
