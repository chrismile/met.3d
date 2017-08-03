/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
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

uniform vec4 colour;
uniform vec3 lightDirection;      // light direction in world space
// normal: normalized face normal.
vec3 flatShading(in vec3 normal)
{
    vec3 toLight = normalize(-lightDirection);
    // Material properties.
    vec3  Ka = .2f * colour.xyz;
    vec3  Kd = .8f * colour.xyz;

    // Add the ambient term to the resulting shading colour.
    vec3 shadingColour = Ka;

    // abs(dot()) used for double-sided lighting.
    float diffuseLight = max(dot(toLight, normal), 0.);
    vec3 diffuse = Kd * diffuseLight;

    shadingColour += diffuse;

    return shadingColour;
}

// Calculation of the front face normal given three point of the face.
vec3 calculateNormal(vec3 centre, vec3 right, vec3 up)
{
    return normalize(cross(right - centre, up - centre));
}

/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain()
{
    gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

uniform mat4      mvpMatrix;      // model-view-projection
uniform mat4      rotationMatrix; // scene rotation matrix
uniform float     horizontalScale;
uniform float     verticalScale;
uniform float     lon;
uniform float     lat;
uniform float     worldZ;

shader GSmain(out vec3 shadingColour)
{
    vec4 base = vec4(lon, lat, worldZ, 0.f);
    vec3 size = vec3(horizontalScale * vec2(1.5f, 1.f), verticalScale);

    vec4 tip = base + vec4(0.f, 4.f * size.y, 0.f, 1.f);
    vec4 front = base + vec4(0.f, size.y, size.z, 1.f);
    vec4 back = base + vec4(0.f, size.y, -size.z, 1.f);
    vec4 left = base + vec4(-size.x, 0.f, 0.f, 1.f);
    vec4 right = base + vec4(size.x, 0.f, 0.f, 1.f);

    // Coordinates in world position.
    vec3 tip_w = (rotationMatrix * tip).xyz;
    vec3 front_w = (rotationMatrix * front).xyz;
    vec3 back_w = (rotationMatrix * back).xyz;
    vec3 left_w = (rotationMatrix * left).xyz;
    vec3 right_w = (rotationMatrix * right).xyz;

    tip = mvpMatrix * tip;
    front = mvpMatrix * front;
    back = mvpMatrix * back;
    left = mvpMatrix * left;
    right = mvpMatrix * right;

    // Set last coordinate to one since we don't want the arrow to be
    // distorted due to its position on the screen.

    // Front left.
    shadingColour = flatShading(calculateNormal(left_w, front_w, tip_w));
    gl_Position = vec4(front.xyzw);
    EmitVertex();
    gl_Position = vec4(tip.xyzw);
    EmitVertex();
    gl_Position = vec4(left.xyzw);
    EmitVertex();
    EndPrimitive();

    // Front right.
    shadingColour = flatShading(calculateNormal(front_w, right_w, tip_w));
    gl_Position = vec4(right.xyzw);
    EmitVertex();
    gl_Position = vec4(tip.xyzw);
    EmitVertex();
    gl_Position = vec4(front.xyzw);
    EmitVertex();
    EndPrimitive();

    // Back left.
    shadingColour = flatShading(calculateNormal(tip_w, back_w, left_w));
    gl_Position = vec4(tip.xyzw);
    EmitVertex();
    gl_Position = vec4(back.xyzw);
    EmitVertex();
    gl_Position = vec4(left.xyzw);
    EmitVertex();
    EndPrimitive();

    // Back right.
    shadingColour = flatShading(calculateNormal(tip_w, right_w, back_w));
    gl_Position = vec4(tip.xyzw);
    EmitVertex();
    gl_Position = vec4(right.xyzw);
    EmitVertex();
    gl_Position = vec4(back.xyzw);
    EmitVertex();
    EndPrimitive();

    // Bottom left.
    shadingColour = flatShading(calculateNormal(front_w, left_w, back_w));
    gl_Position = vec4(front.xyzw);
    EmitVertex();
    gl_Position = vec4(left.xyzw);
    EmitVertex();
    gl_Position = vec4(back.xyzw);
    EmitVertex();
    EndPrimitive();

    // Bottom right.
    shadingColour = flatShading(calculateNormal(front_w, back_w, right_w));
    gl_Position = vec4(front.xyzw);
    EmitVertex();
    gl_Position = vec4(back.xyzw);
    EmitVertex();
    gl_Position = vec4(right.xyzw);
    EmitVertex();
    EndPrimitive();
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/


shader FSmain(in vec3 shadingColour, out vec4 fragColour)
{
//    fragColour = colour;
    fragColour = vec4(shadingColour, 1.f);
    // Set fragment depth manually to draw arrow in front of all other geometry,
    // but without struggeling with the clipping of the near plan.
//    gl_FragDepth = -1.0f;
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(420)=VSmain();
    gs(420)=GSmain() : in(points), out(triangle_strip, max_vertices = 24);
    fs(420)=FSmain();
};
