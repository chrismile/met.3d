/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus [*, previously +]
**  Copyright 2015-2016 Christoph Heidelmann [+]
**  Copyright 2020 Marcel Meyer [*]
**
**  + Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
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
 ***                           VERTEX SHADER
 *****************************************************************************/

uniform vec2 pToWorldZParams;         // parameters to convert p[hPa] to world z

uniform mat4  mvpMatrix;              // model-view-projection
uniform float radius;                 // radius of the sphere in world space
uniform bool  scaleRadius;            // if true, radius will be scaled with
                                      // distance camera-vertex
uniform vec3  cameraPosition;         // camera position in world space
uniform vec3  cameraUpDir;            // camera upward axis

uniform vec2  position;                // camera upward axis

uniform int       numObsPerTrajectory; // number of "observations" (i.e. points)
uniform bool      useTransferFunction; // use transfer function or const col?
uniform vec4      constColour;
uniform sampler1D transferFunction;    // 1D transfer function
uniform float     scalarMinimum;       // min/max data values to scale to 0..1
uniform float     scalarMaximum;
uniform vec3      lightDirection;       // light direction in world space

uniform bool renderAuxData;             // define boolean for deciding which
                                        // type of rendering

shader VSmain(in vec3 vertex : 0, in float auxDataAtVertex : 2, out vec4 vs_vertex)
{
     // Convert pressure to world Z coordinate.
     float worldZ = (log(vertex.z) - pToWorldZParams.x) * pToWorldZParams.y;

     // Convert from world space to clip space.
     // Define scalar value for rendering colors in 4th vertex component.
     if (renderAuxData) vs_vertex = vec4(vertex.xy, worldZ, auxDataAtVertex);
     if (!renderAuxData) vs_vertex = vec4(vertex.xy, worldZ, vertex.z);

}

shader VSusePosition(in vec3 vertex : 0, out vec4 vs_vertex)
{
    // Use 2D position for vertex coordinate
    vs_vertex = vec4(position, 0, 1);
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

shader GSmain(in vec4 vs_vertex[], out GStoFS Output)
{

    // Don't draw spheres at invalid positions.
    if (vs_vertex[0].x == M_INVALID_TRAJECTORY_POS
            || vs_vertex[0].y == M_INVALID_TRAJECTORY_POS
            || vs_vertex[0].z == M_INVALID_TRAJECTORY_POS)
    {
        return;
    }

    // Vector from camera to sphere centre.
    vec3 view  = vs_vertex[0].xyz - cameraPosition;
    Output.gs_radius = radius;
    if (scaleRadius) Output.gs_radius = radius * length(view) / 100.;
    view  = normalize(view);
    // Right and up vectors for a plane perpendicular to the view vector.
    vec3 right = normalize(cross(view, cameraUpDir));
    vec3 up    = cross(right, view);

    // Emit vertices for a quad that, seen from the camera, covers the sphere
    // to be rendered. Move the quad towards the camera so that it just touches
    // the foremost point of the sphere.

    vec3 centre_front = vs_vertex[0].xyz - Output.gs_radius * view;
    vec3 scaled_right = Output.gs_radius * right;
    vec3 scaled_up    = Output.gs_radius * up;

    // Sphere centre and scalar value are the same for all emitted vertices.
    Output.gs_centre = vs_vertex[0].xyz;
    Output.gs_scalar = vs_vertex[0].w;

    // Upper right vertex ...
    Output.gs_worldSpacePosition = vec3(centre_front + scaled_right + scaled_up);
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    EmitVertex();

    // ... upper left ...
    Output.gs_worldSpacePosition = vec3(centre_front - scaled_right + scaled_up);
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    EmitVertex();

    // ... lower right ...
    Output.gs_worldSpacePosition = vec3(centre_front + scaled_right - scaled_up);
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    EmitVertex();

    // ... lower left.
    Output.gs_worldSpacePosition = vec3(centre_front - scaled_right - scaled_up);
    gl_Position           = mvpMatrix * vec4(Output.gs_worldSpacePosition, 1.);
    EmitVertex();
};


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

    // Ray-sphere intersection (notes/em 19Mar2013).
    vec3 dir = normalize(Input.gs_worldSpacePosition - cameraPosition);
    vec3 c2p = Input.gs_worldSpacePosition - Input.gs_centre;
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, c2p);
    float c = dot(c2p, c2p) - Input.gs_radius * Input.gs_radius;
    float d = b * b - 4.0 * a * c;
    if (d <= 0.0) discard;

    float t = (-b - sqrt(d)) / (2.0 * a);
    // Position at which the ray from quad pixel to sphere centre intersects
    // with the sphere.
    vec3 intersectionPos = Input.gs_worldSpacePosition + dir * t;

    // .. compute the depth value of the fragment (we know the intersection
    // position in world space -- convert this to clip space and
    // scale to 0..1) ..
    vec4 screenPosition = mvpMatrix * vec4(intersectionPos, 1.);
    screenPosition.z /= screenPosition.w;          // scale to -1 .. 1
    gl_FragDepth = (screenPosition.z + 1.0) / 2.0; // scale to 0 .. 1

    // Diffuse illumination.
    vec3 normal = normalize(intersectionPos - Input.gs_centre);
    vec3 toLight = normalize(-lightDirection);
    fragColour.rgb *= lightIntensity(normal, toLight, -dir, vec4(0.7, .5, 0.2, 8));
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Normal
{
    vs(420)=VSmain();
    gs(420)=GSmain() : in(points), out(triangle_strip, max_vertices = 4);
    fs(420)=FSmain();
};

program UsePosition
{
    vs(420)=VSusePosition();
    gs(420)=GSmain() : in(points), out(triangle_strip, max_vertices = 4);
    fs(420)=FSmain();
};
