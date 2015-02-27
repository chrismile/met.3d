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

const int TUBE_SEGMENT_COUNT = 8;
const float M_INVALID_TRAJECTORY_POS = -999.99;


/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform vec2 pToWorldZParams; // parameters to convert p[hPa] to world z

uniform mat4  mvpMatrix;            // model-view-projection
uniform float radius;               // radius of the tube in world space
uniform int   numObsPerTrajectory;  // number of "observations" (i.e. points)
                                    // per trajectory

uniform sampler1D transferFunction;      // 1D transfer function
uniform float     scalarMinimum;         // min/max data values to scale to 0..1
uniform float     scalarMaximum;
uniform vec3      lightDirection;        // light direction in world space
uniform vec3      cameraPosition;        // camera position in world space
uniform int       renderTubesUpToIndex;


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec3 vertex : 0, in vec3 normal : 1,
              out vec4 vs_vertex, out vec3 vs_normal, out int vs_vertexID)
{
    // Convert pressure to world Z coordinate.
    float worldZ = (log(vertex.z) - pToWorldZParams.x) * pToWorldZParams.y;
    // Convert from world space to clip space.
    vs_vertex = vec4(vertex.xy, worldZ, vertex.z);

    // Pass normal and vertex ID to geometry shader.
    vs_normal   = normal;
    vs_vertexID = gl_VertexID;
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

// Trajectory tubes, geometry shader. The geometry shader processes a
// trajectory segment and transforms a line into a tube. The implementation is
// based on code by M.T. (notes 08Mar2013). The shader uses adjacency
// information to prevent gaps between the tube segments. The normals at the
// vertex positions have to be precomputed.

shader GSmain(in vec4 vs_vertex[], in vec3 vs_normal[], in int vs_vertexID[],
              out vec3 gs_worldSpaceNormal, out vec3 gs_worldSpacePosition,
              out float gs_scalar)
{
    if (renderTubesUpToIndex == 0) return;
    
    // Test if this is the first line segment of the trajectory. If yes,
    // output an additional tube. The vertexID contains the index of the
    // vertex in the vertex buffer, hence we have to take the modulo to
    // get the index within the current trajectory.
    if (mod(vs_vertexID[0], numObsPerTrajectory) == 0)
    {
       // Generate a tube between vertex 0 and vertex 1.

       vec3 position0 = vs_vertex[0].xyz;
       vec3 position1 = vs_vertex[1].xyz;
       vec3 position2 = vs_vertex[2].xyz;

       float scalar0 = vs_vertex[0].w;
       float scalar1 = vs_vertex[1].w;

       vec3 normal0 = vs_normal[0];
       vec3 normal1 = vs_normal[1];

       // first point: tangent == line segment
       vec3 tangent0 = normalize(position1 - position0);
       // second point: tangent given by the two adjacent points
       vec3 tangent1 = normalize(position2 - position0);

       vec3 binormal0 = cross(tangent0, normal0);
       vec3 binormal1 = cross(tangent1, normal1);

       // not working because of interface scheme
       // interface cannot be passed via parameters

       /*generateTube(position0, position1, normal0, normal1,
                    binormal0, binormal1, scalar0, scalar1,
                    gs_worldSpaceNormal, gs_worldSpacePosition,
                    gs_scalar);*/

       // The first two vertices of the tube are created by displacing the two
       // positions outward by the radius along the normals.
       gs_worldSpaceNormal   = normal1;
       gs_worldSpacePosition = position1 + radius * gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
       gs_scalar             = scalar1;
       EmitVertex();

       gs_worldSpaceNormal   = normal0;
       gs_worldSpacePosition = position0 + radius * gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
       gs_scalar             = scalar0;
       EmitVertex();

       // Rotate around the tube centre.
       for (int i = 1; i < TUBE_SEGMENT_COUNT; i++)
       {
           float t = float(i) / float(TUBE_SEGMENT_COUNT);
           float angle = radians(t * 360.);
           float s = sin(angle);
           float c = cos(angle);

           // Compute a rotated normal by using the coordinate system spanned
           // by normal and binormal.
           gs_worldSpaceNormal   = c * normal1 + s * binormal1;
           gs_worldSpacePosition = position1 + radius * gs_worldSpaceNormal;
           gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
           gs_scalar             = scalar1;
           EmitVertex();

           gs_worldSpaceNormal   = c * normal0 + s * binormal0;
           gs_worldSpacePosition = position0 + radius * gs_worldSpaceNormal;
           gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
           gs_scalar             = scalar0;
           EmitVertex();
       }

       // Repeat the first two points to close the tube.
       gs_worldSpaceNormal   = normal1;
       gs_worldSpacePosition = position1 + radius * gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
       gs_scalar             = scalar1;
       EmitVertex();

       gs_worldSpaceNormal   = normal0;
       gs_worldSpacePosition = position0 + radius * gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
       gs_scalar             = scalar0;
       EmitVertex();

       // A complete tube primitive has been generated.
       EndPrimitive();
    }


    // The usual case: Create a tube between vertex 1 and vertex 2.
    if (mod(vs_vertexID[2], numObsPerTrajectory) > renderTubesUpToIndex) return;

    vec3 position0 = vs_vertex[0].xyz;
    vec3 position1 = vs_vertex[1].xyz;
    vec3 position2 = vs_vertex[2].xyz;
    vec3 position3 = vs_vertex[3].xyz;

    float scalar1 = vs_vertex[1].w;
    float scalar2 = vs_vertex[2].w;

    vec3 normal1 = vs_normal[1];
    vec3 normal2 = vs_normal[2];

    vec3 tangent1 = normalize(position2 - position0);
    vec3 tangent2;
    if (vs_vertex[3].w == M_INVALID_TRAJECTORY_POS)
    {
        tangent2 = normalize(position2 - position1);
    }
    else
    {
        tangent2 = normalize(position3 - position1);
    }
    
    vec3 binormal1 = cross(tangent1, normal1);
    vec3 binormal2 = cross(tangent2, normal2);

   /* generateTube(position1, position2, normal1, normal2,
                 binormal1, binormal2, scalar1, scalar2,
                 gs_worldSpaceNormal, gs_worldSpacePosition,
                 gs_scalar);*/

    // The first two vertices of the tube are created by displacing the two
    // positions outward by the radius along the normals.
    gs_worldSpaceNormal   = normal2;
    gs_worldSpacePosition = position2 + radius * gs_worldSpaceNormal;
    gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
    gs_scalar             = scalar2;
    EmitVertex();

    gs_worldSpaceNormal   = normal1;
    gs_worldSpacePosition = position1 + radius * gs_worldSpaceNormal;
    gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
    gs_scalar             = scalar1;
    EmitVertex();

    // Rotate around the tube centre.
    for (int i = 1; i < TUBE_SEGMENT_COUNT; i++)
    {
        float t = float(i) / float(TUBE_SEGMENT_COUNT);
        float angle = radians(t * 360.);
        float s = sin(angle);
        float c = cos(angle);

        // Compute a rotated normal by using the coordinate system spanned
        // by normal and binormal.
        gs_worldSpaceNormal   = c * normal2 + s * binormal2;
        gs_worldSpacePosition = position2 + radius * gs_worldSpaceNormal;
        gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
        gs_scalar             = scalar2;
        EmitVertex();

        gs_worldSpaceNormal   = c * normal1 + s * binormal1;
        gs_worldSpacePosition = position1 + radius * gs_worldSpaceNormal;
        gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
        gs_scalar             = scalar1;
        EmitVertex();
    }

    // Repeat the first two points to close the tube.
    gs_worldSpaceNormal   = normal2;
    gs_worldSpacePosition = position2 + radius * gs_worldSpaceNormal;
    gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
    gs_scalar             = scalar2;
    EmitVertex();

    gs_worldSpaceNormal   = normal1;
    gs_worldSpacePosition = position1 + radius * gs_worldSpaceNormal;
    gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
    gs_scalar             = scalar1;
    EmitVertex();

    // A complete tube primitive has been generated.
    EndPrimitive();


    if (mod(vs_vertexID[3], numObsPerTrajectory) > renderTubesUpToIndex) return;

    // Test if this is the last line segment of the trajectory. If yes, generate
    // an additional tube between points 2 and 3.
    if (mod(vs_vertexID[3], numObsPerTrajectory) == (numObsPerTrajectory - 1))
    {
       vec3 position1 = vs_vertex[1].xyz;
       vec3 position2 = vs_vertex[2].xyz;
       vec3 position3 = vs_vertex[3].xyz;

       float scalar2 = vs_vertex[2].w;
       float scalar3 = vs_vertex[3].w;

       vec3 normal2 = vs_normal[2];
       vec3 normal3 = vs_normal[3];

       vec3 tangent2 = normalize(position3 - position1);
       vec3 tangent3 = normalize(position3 - position2);

       vec3 binormal2 = cross(tangent2, normal2);
       vec3 binormal3 = cross(tangent3, normal3);

       /*generateTube(position2, position3, normal2, normal3,
                    binormal2, binormal3, scalar2, scalar3,
                    gs_worldSpaceNormal, gs_worldSpacePosition,
                    gs_scalar);*/

       // The first two vertices of the tube are created by displacing the two
       // positions outward by the radius along the normals.
       gs_worldSpaceNormal   = normal3;
       gs_worldSpacePosition = position3 + radius * gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
       gs_scalar             = scalar3;
       EmitVertex();

       gs_worldSpaceNormal   = normal2;
       gs_worldSpacePosition = position2 + radius * gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
       gs_scalar             = scalar2;
       EmitVertex();

       // Rotate around the tube centre.
       for (int i = 1; i < TUBE_SEGMENT_COUNT; i++)
       {
           float t = float(i) / float(TUBE_SEGMENT_COUNT);
           float angle = radians(t * 360.);
           float s = sin(angle);
           float c = cos(angle);

           // Compute a rotated normal by using the coordinate system spanned
           // by normal and binormal.
           gs_worldSpaceNormal   = c * normal3 + s * binormal3;
           gs_worldSpacePosition = position3 + radius * gs_worldSpaceNormal;
           gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
           gs_scalar             = scalar3;
           EmitVertex();

           gs_worldSpaceNormal   = c * normal2 + s * binormal2;
           gs_worldSpacePosition = position2 + radius * gs_worldSpaceNormal;
           gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
           gs_scalar             = scalar2;
           EmitVertex();
       }

       // Repeat the first two points to close the tube.
       gs_worldSpaceNormal   = normal3;
       gs_worldSpacePosition = position3 + radius * gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
       gs_scalar             = scalar3;
       EmitVertex();

       gs_worldSpaceNormal   = normal2;
       gs_worldSpacePosition = position2 + radius * gs_worldSpaceNormal;
       gl_Position           = mvpMatrix * vec4(gs_worldSpacePosition, 1.);
       gs_scalar             = scalar2;
       EmitVertex();

       // A complete tube primitive has been generated.
       EndPrimitive();
    }
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

float lightIntensity(vec3 normal, vec3 toLight, vec3 toEye,
                     vec4 diffuseSpecularAmbientShininess)
{
    vec3 toReflectedLight = reflect(-toLight, normal);

    float diffuse = max(dot(toLight, normal), 0.);
    float specular = max(dot(toReflectedLight, toEye), 0.);
    specular = pow(specular, diffuseSpecularAmbientShininess.w);

    return (diffuseSpecularAmbientShininess.x * diffuse)
           + (diffuseSpecularAmbientShininess.y * specular)
           + diffuseSpecularAmbientShininess.z;
}


shader FSmain(in vec3 gs_worldSpaceNormal, in vec3 gs_worldSpacePosition,
              in float gs_scalar, out vec4 fragColour)
{
    // Scale the scalar range to 0..1.
    float scalar_ = (gs_scalar - scalarMinimum) / (scalarMaximum - scalarMinimum);
    // Fetch colour from the transfer function.
    fragColour = texture(transferFunction, scalar_);

    // Diffuse illumination.
    vec3 normal = normalize(gs_worldSpaceNormal);
    vec3 toLight = normalize(-lightDirection);
    vec3 toEye = normalize(cameraPosition - gs_worldSpacePosition);
    fragColour.rgb *= lightIntensity(normal, toLight, toEye, vec4(0.7, .5, 0.2, 8));
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(400)=VSmain();
    gs(400)=GSmain() : in(lines_adjacency), out(triangle_strip, max_vertices = 36);
    fs(400)=FSmain();
};
