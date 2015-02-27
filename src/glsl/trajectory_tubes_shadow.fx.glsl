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
 ***                             UNIFORMS
 *****************************************************************************/

uniform vec2 pToWorldZParams; // parameters to convert p[hPa] to world z

uniform mat4  mvpMatrix;            // model-view-projection
uniform float radius;               // radius of the tube in world space
uniform int   numObsPerTrajectory;  // number of "observations" (i.e. points)

uniform bool      useTransferFunction;   // use transfer function or const col?
uniform vec4      constColour;
uniform sampler1D transferFunction;      // 1D transfer function
uniform float     scalarMinimum;         // min/max data values to scale to 0..1
uniform float     scalarMaximum;
uniform vec3      lightDirection;        // light direction in world space
uniform vec3      cameraPosition;        // camera position in world space
uniform int       renderTubesUpToIndex;


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

interface VStoGS
{
    vec4 vs_vertex;       // output: vertex position in world space
    vec3 vs_normal;       // pass through of normal
    int  vs_vertexID;     // pass through of vertex ID
};

interface GStoFS
{
    vec3  gs_worldSpaceNormal;  // output: world space normal
    vec3  gs_worldSpacePosition;// output: world space position
    float gs_scalar;            // scalar value copied from vertex.w
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec3 vertex : 0,
              in vec3 normal : 1,
              out VStoGS Output)
{
    // Convert pressure to world Z coordinate.
    float worldZ = (log(vertex.z) - pToWorldZParams.x) * pToWorldZParams.y;
    // Convert from world space to clip space.
    Output.vs_vertex = vec4(vertex.xy, 0.1, vertex.z);

    // Pass normal and vertex ID to geometry shader.
    Output.vs_normal   = normal;
    Output.vs_vertexID = gl_VertexID;
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

shader GSmain(in VStoGS Input[], out GStoFS Output)
{
    if (renderTubesUpToIndex == 0) return;
    if (Input[1].vs_vertex.x == M_INVALID_TRAJECTORY_POS) return;
    
    // Test if this is the first line segment of the trajectory. If yes, output
    // an additional quadrilateral. The vertexID contains the index of the
    // vertex in the vertex buffer, hence we have to take the modulo to get the
    // index within the current trajectory.
    if (mod(Input[0].vs_vertexID, numObsPerTrajectory) == 0)
    {
       // Generate a tube between vertex 0 and vertex 1.

       vec3 position0 = Input[0].vs_vertex.xyz;
       vec3 position1 = Input[1].vs_vertex.xyz;
       vec3 position2 = Input[2].vs_vertex.xyz;

       float scalar0 = Input[0].vs_vertex.w;
       float scalar1 = Input[1].vs_vertex.w;

       vec3 normal0 = Input[0].vs_normal;
       vec3 normal1 = Input[1].vs_normal;

       // first point: tangent == line segment
       vec3 tangent0 = normalize(position1 - position0);
       // second point: tangent given by the two adjacent points
       vec3 tangent1 = normalize(position2 - position0);

       vec3 binormal0 = cross(tangent0, normal0);
       vec3 binormal1 = cross(tangent1, normal1);


       // Rotate the normal so that it is aligned with the x/y-plane (i.e. its
       // z-component should be 0). If new_normal = c(a)*normal + s(a)*binormal,
       // we need to find a so that new_normal_z = 0: new_normal_z = 0 =
       // c(a)*normal_z + s(a)*binormal_z. Solving for s(a)/c(a) = tan(a) yields
       // tan(a) = -normal_z/binormal_z.
       float angle1 = atan(-normal1.z, binormal1.z);
       vec3 worldSpaceNormal1 = cos(angle1) * normal1 + sin(angle1) * binormal1;

       float angle0 = atan(-normal0.z, binormal0.z);
       vec3 worldSpaceNormal0 = cos(angle0) * normal0 + sin(angle0) * binormal0;

       // Compute the four qudrilateral edge vertices.
       Output.gs_worldSpaceNormal   = worldSpaceNormal1;
       Output.gs_worldSpacePosition = position1 + radius * Output.gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
       Output.gs_scalar             = scalar1;
       EmitVertex();

       Output.gs_worldSpaceNormal   = worldSpaceNormal0;
       Output.gs_worldSpacePosition = position0 + radius * Output.gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
       Output.gs_scalar             = scalar0;
       EmitVertex();

       Output.gs_worldSpaceNormal   = worldSpaceNormal1;
       Output.gs_worldSpacePosition = position1 - radius * Output.gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
       Output.gs_scalar             = scalar1;
       EmitVertex();

       Output.gs_worldSpaceNormal   = worldSpaceNormal0;
       Output.gs_worldSpacePosition = position0 - radius * Output.gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
       Output.gs_scalar             = scalar0;
       EmitVertex();

       // A complete tube primitive has been generated.
       EndPrimitive();
    }

    // The usual case: Create a quadrilateral between vertex 1 and vertex 2.
    if (mod(Input[2].vs_vertexID, numObsPerTrajectory) > renderTubesUpToIndex) return;
    if (Input[2].vs_vertex.x == M_INVALID_TRAJECTORY_POS) return;

    vec3 position2 = Input[0].vs_vertex.xyz;
    vec3 position0 = Input[1].vs_vertex.xyz;
    vec3 position1 = Input[2].vs_vertex.xyz;
    vec3 position3 = Input[3].vs_vertex.xyz;

    float scalar0 = Input[1].vs_vertex.w;
    float scalar1 = Input[2].vs_vertex.w;

    vec3 normal0 = Input[1].vs_normal;
    vec3 normal1 = Input[2].vs_normal;

    vec3 tangent0 = normalize(position1 - position2);
    
    vec3 tangent1;
    if (Input[3].vs_vertex.w == M_INVALID_TRAJECTORY_POS)
    {
        tangent1 = normalize(position1 - position0);
    }
    else
    {
        tangent1 = normalize(position3 - position0);
    }
    
    vec3 binormal0 = cross(tangent0, normal0);
    vec3 binormal1 = cross(tangent1, normal1);

    float angle1 = atan(-normal1.z, binormal1.z);
    vec3 worldSpaceNormal1 = cos(angle1) * normal1 + sin(angle1) * binormal1;

    float angle0 = atan(-normal0.z, binormal0.z);
    vec3 worldSpaceNormal0 = cos(angle0) * normal0 + sin(angle0) * binormal0;

    // Compute the four qudrilateral edge vertices.
    Output.gs_worldSpaceNormal   = worldSpaceNormal1;
    Output.gs_worldSpacePosition = position1 + radius * Output.gs_worldSpaceNormal;
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    Output.gs_scalar             = scalar1;
    EmitVertex();

    Output.gs_worldSpaceNormal   = worldSpaceNormal0;
    Output.gs_worldSpacePosition = position0 + radius * Output.gs_worldSpaceNormal;
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    Output.gs_scalar             = scalar0;
    EmitVertex();

    Output.gs_worldSpaceNormal   = worldSpaceNormal1;
    Output.gs_worldSpacePosition = position1 - radius * Output.gs_worldSpaceNormal;
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    Output.gs_scalar             = scalar1;
    EmitVertex();

    Output.gs_worldSpaceNormal   = worldSpaceNormal0;
    Output.gs_worldSpacePosition = position0 - radius * Output.gs_worldSpaceNormal;
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    Output.gs_scalar             = scalar0;
    EmitVertex();

    // A complete tube primitive has been generated.
    EndPrimitive();

        
    // Check if last line segment should be rendered.
    if (mod(Input[3].vs_vertexID, numObsPerTrajectory) > renderTubesUpToIndex) return;
    if (Input[3].vs_vertex.w == M_INVALID_TRAJECTORY_POS) return;
        
    // Test if this is the last line segment of the trajectory. If yes, generate
    // an additional quadrilateral between points 2 and 3.
    if (mod(Input[3].vs_vertexID, numObsPerTrajectory) == (numObsPerTrajectory - 1))
    {
       vec3 position2 = Input[1].vs_vertex.xyz;
       vec3 position0 = Input[2].vs_vertex.xyz;
       vec3 position1 = Input[3].vs_vertex.xyz;

       float scalar0 = Input[2].vs_vertex.w;
       float scalar1 = Input[3].vs_vertex.w;

       vec3 normal0 = Input[2].vs_normal;
       vec3 normal1 = Input[3].vs_normal;

       vec3 tangent0 = normalize(position1 - position2);
       vec3 tangent1 = normalize(position1 - position0);

       vec3 binormal0 = cross(tangent0, normal0);
       vec3 binormal1 = cross(tangent1, normal1);

       float angle1 = atan(-normal1.z, binormal1.z);
       vec3 worldSpaceNormal1 = cos(angle1) * normal1 + sin(angle1) * binormal1;

       float angle0 = atan(-normal0.z, binormal0.z);
       vec3 worldSpaceNormal0 = cos(angle0) * normal0 + sin(angle0) * binormal0;

       // Compute the four qudrilateral edge vertices.
       Output.gs_worldSpaceNormal   = worldSpaceNormal1;
       Output.gs_worldSpacePosition = position1 + radius * Output.gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
       Output.gs_scalar             = scalar1;
       EmitVertex();

       Output.gs_worldSpaceNormal   = worldSpaceNormal0;
       Output.gs_worldSpacePosition = position0 + radius * Output.gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
       Output.gs_scalar             = scalar0;
       EmitVertex();

       Output.gs_worldSpaceNormal   = worldSpaceNormal1;
       Output.gs_worldSpacePosition = position1 - radius * Output.gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
       Output.gs_scalar             = scalar1;
       EmitVertex();

       Output.gs_worldSpaceNormal   = worldSpaceNormal0;
       Output.gs_worldSpacePosition = position0 - radius * Output.gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
       Output.gs_scalar             = scalar0;
       EmitVertex();

       // A complete tube primitive has been generated.
       EndPrimitive();
    }
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

shader FSmain(in GStoFS Input, out vec4 fragColour)
{
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
    gs(400)=GSmain() : in(lines_adjacency), out(triangle_strip, max_vertices = 8);
    fs(400)=FSmain();
};

