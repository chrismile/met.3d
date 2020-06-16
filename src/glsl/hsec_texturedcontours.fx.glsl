/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016-2017 Marc Rautenhaus
**  Copyright 2016-2017 Bianca Tost
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
    smooth vec2   pos;    // the position of this vertex in grid space,
                          // the value shall be perspectively correct
                          // interpolated to the fragment shader
//TODO: is there a more elegant way to do this?
    smooth float  flag;   // if this flag is set to < 0, the fragment
                          // shader should discard the fragment (for
                          // invalid vertices beneath the surface)
    smooth float  lon;    // holds the longitude coordinate of the fragment in
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

uniform float     scalarMinimum;    // min/max data values to scale to 0..1
uniform float     scalarMaximum;

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
    outStruct.pos = vec2(i, j);
    outStruct.lon = lon;

    if (outStruct.scalar != MISSING_VALUE) outStruct.flag = 0.; else outStruct.flag = -100.;
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler2DArray transferFunction; // 2D transfer function with levels in 3rd
                                    // dimension

uniform float distInterp;           // Range in scalar values used for interpolation.

uniform bool clampMaximum;          // Indicator whether to clamp texture for
                                    // values greater than maxScalar or not.
uniform int numLevels;              // Amount of textures loaded.

uniform float     scaleWidth;       // Scale in longitudes to scale texture with.
uniform float     aspectRatio;      // Aspect ratio of given textures.
uniform float     gridAspectRatio;  // Aspect ratio of given grid.

uniform float height;               // Height of horizontal cross-section.

uniform int alphaBlendingMode;      // Defines which channel to use as alpha.
uniform bool invertAlpha;           // Flag whether to invert alpha value.
uniform bool useConstantColour;     // Flag whether to use a constant instead of
                                    // the texture given colour.
uniform vec4 constantColour;        // Constant colour to use instead of the
                                    // texture given if wished by the user.

uniform vec2      bboxLons;          // western and eastern lon of the bbox

uniform bool      isCyclicGrid;   // indicates whether the grid is cyclic or not
uniform float     leftGridLon;    // leftmost longitude of the grid
uniform float     eastGridLon;    // eastmost longitude of the grid

shader FSmain(in VStoFS inStruct, out vec4 fragColour)
{
    // Discard the element if it is outside the model domain (no scalar value)
    // or outside the scalar range or outside of the bounding box.
    if ((inStruct.flag < 0.)
            || (scalarMinimum > inStruct.scalar)
            || (!clampMaximum && (scalarMaximum < inStruct.scalar))
            || inStruct.lon < bboxLons.x || inStruct.lon > bboxLons.y)
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

    // Scale of texture with respect to one grid cell.
    vec2 scale = vec2(scaleWidth, scaleWidth * aspectRatio * gridAspectRatio);

    // Scalar value mapped to range: [0, max-min].
    float scalar = inStruct.scalar - scalarMinimum;

    // Flip y direction to obtain right orientation of texture.
    vec2 pos = vec2(inStruct.pos.x, height - inStruct.pos.y);

    float scalarRange = scalarMaximum - scalarMinimum;
    // Distribute texture levels equidistantly over scalar range.
    float dist = scalarRange / float(numLevels);
    // Compute texture level from scalar value.
    float level = floor(scalar / dist);

    // Level in texture coordinates. (Clamp to highest level.)
    float levelCoord = min(level, float(numLevels - 1.0f));

    // Texture coordinates with respect to scale.
    vec3 texCoords = vec3(pos / scale, levelCoord);

    // Fetch lod and use it manually since otherwise interpolation between two
    // texture levels won't work correctly.
    float mipmapLevel = textureQueryLod(transferFunction, texCoords.xy).x;

    vec4 colour = textureLod(transferFunction, texCoords, mipmapLevel);

    // Interpolation range in scalar value.
    float interpRange = (distInterp / 2.0f);
    // Scalar value distance to current level.
    float fraction = scalar - (floor(scalar / dist) * dist);
    // Scalar value distance to next level.
    float fraction2 = (ceil(scalar / dist) * dist) - scalar;

    // Interpolation to previous level.
    if (interpRange > fraction && level < float(numLevels))
    {
        // Indicator whether to blend to the previous level.
        float indicator = float(level != 0.0f);

        levelCoord = floor(scalar / dist) - 1.f;
        // Since we interpolate at borders to both sides, our interpolation
        // range is [1.f, 0.5f] for one side. But for the lowest level we want
        // the texture to fade out completely therefore the interpolation range
        // is [1.f, 0.f].
        float interpolation = (fraction / (pow(2.0f, indicator) * interpRange))
                + 0.5f * indicator;

        texCoords = vec3(pos / scale, levelCoord);

        // Blend to previous level. If no previous level exists blend to
        // transparent version of the lowest level.
        colour = (interpolation * colour)
                + ((1.f - interpolation)
                   * vec4(textureLod(transferFunction, texCoords, mipmapLevel).rgb,
                          // Use alpha value of texture to blend to previous
                          // level, otherwise use 0.0f.
                          indicator
                          * textureLod(transferFunction, texCoords, mipmapLevel).a)
                   );
    }
    // Interpolation to higher level. (No interpolation for clamped region.)
    else if (fraction2 < interpRange && level < float(numLevels))
    {
        // Indicator whether to blend to the next level.
        float indicator = float(level != float(numLevels - 1) || clampMaximum);

        levelCoord = min(ceil(scalar / dist), numLevels - 1.0f);
        // Since we interpolate at borders between texture levels to both sides,
        // our interpolation range is [1.f, 0.5f] for one side. But for the
        // highest level we want the texture to fade out completely (if we do
        // not use clamp) therefore the interpolation range is [1.f, 0.f].
        float interpolation = (fraction2 / (pow(2.0f, indicator) * interpRange))
                + 0.5f * indicator;

        texCoords = vec3(pos / scale, levelCoord);

        // Blend to next level. If no next level exists blend to transparent
        // version of the highest level exept the user wants to clamp the maximum
        // value.
        colour = (interpolation * colour)
                + ((1.f - interpolation)
                   * vec4(textureLod(transferFunction, texCoords, mipmapLevel).rgb,
                          // Use alpha value of texture to blend to next level,
                          // otherwise use 0.0f.
                          indicator
                          * textureLod(transferFunction, texCoords, mipmapLevel).a)
                   );
    }

    // Needs to match alphaBlendingMode enum in spatial1dtransferfunction.h.
    switch (alphaBlendingMode)
    {
    case 0: // Use alpha channel.
    {
        colour = colour;
        break;
    }
    case 1: // Use red channel.
    {
        colour = colour.rgbr;
        break;
    }
    case 2: // Use green channel.
    {
        colour = colour.rgbg;
        break;
    }
    case 3: // Use blue channel.
    {
        colour = colour.rgbb;
        break;
    }
    case 4: // Use rgb average.
    {
        float alpha = (colour.r + colour.g + colour.b) / 3.0f;
        colour = vec4(colour.rgb, alpha);
        break;
    }
    case 5: // Use no alpha blending.
    {
        colour = vec4(colour.rgb, 1.0f);
        break;
    }
    default: // Invalid mode.
    {
        discard;
    }
    }

    if (invertAlpha)
    {
        colour.a = 1.0f - colour.a;
    }

    if (useConstantColour)
    {
        colour = vec4(constantColour.rgb, colour.a);
    }

    fragColour = colour;
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(420)=VSmain();
    fs(420)=FSmain();
};

