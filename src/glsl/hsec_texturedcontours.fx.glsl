/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016 Marc Rautenhaus
**  Copyright 2016 Bianca Tost
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
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

uniform sampler1D latLonAxesData;    // 1D texture that holds both lon and lat
uniform int       latOffset;         // index at which lat axis data starts
uniform float     worldZ;            // world z coordinate of this section
uniform mat4      mvpMatrix;         // model-view-projection

layout(r32f)
// TODO:
//layout(rg32f)
uniform image2D   crossSectionGrid;  // 2D data grid
uniform int       iOffset;           // grid index offsets if only a subregion
uniform int       jOffset;           //   of the grid shall be rendered
uniform vec2      bboxLons;          // western and eastern lon of the bbox

uniform float     scalarMinimum;    // min/max data values to scale to 0..1
uniform float     scalarMaximum;

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
    else if (lon < bboxWestLon) lon += int((bboxEastLon - lon) / 360.) * 360.;

    vec3 vertexPosition = vec3(lon, lat, worldZ);

    // Convert the position from world to clip space.
    gl_Position = mvpMatrix * vec4(vertexPosition, 1);

    // NOTE: imageLoad requires "targetGrid" to be defined with the
    // layout qualifier, otherwise an error "..no overload function can
    // be found: imageLoad(struct image2D, ivec2).." is raised.
    vec4 data = imageLoad(crossSectionGrid, ivec2(i, j));
    output.scalar = data.r;
    output.pos = vec2(i, j);

    if (output.scalar != MISSING_VALUE) output.flag = 0.; else output.flag = -100.;
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler3D transferFunction; // 2D transfer function with levels in 3rd
                                    // dimension

uniform float distInterp;           // Range in scalar values used for interpolation.

uniform bool clampMaximum;          // Indicator whether to clamp texture for
                                    // values greater than maxScalar or not.
uniform int numLevels;              // Amount of textures loaded.

uniform float     scaleWidth;       // Scale in longitudes to scale texture with.
uniform float     aspectRatio;      // Aspect ratio of given textures.

shader FSmain(in VStoFS input, out vec4 fragColour)
{
    // Discard the element if it is outside the model domain (no scalar value)
    // or outside the scalar range.
    if ((input.flag < 0.)
            || (scalarMinimum > input.scalar)
            || (!clampMaximum && (scalarMaximum < input.scalar))
            )
    {
        discard;
    }

    // Adapt to change scale of texture.
    vec2 scale = vec2(scaleWidth, scaleWidth * aspectRatio);

    // Scalar value in range mapped to [0, max-min].
    float scalar = input.scalar - scalarMinimum;

    float scalarRange = scalarMaximum - scalarMinimum;
    float dist = scalarRange / float(numLevels);
    float level = floor(scalar / dist);
    // Texture level in 3d texture coordinates. (With first level at
    // 1/(2*numLevels))
    float levelCoord = (((min(level, float(numLevels - 1.0f))
                          * 2.0f) + 1.0f) / float(2.0f * numLevels));

    // todo Why is flip necessary?
    // Texture coordinates with respect to scale. Flip y-direction since
    // otherwise texture "stands on its head".
    vec3 texCoords =
            vec3(vec2(0.0f, 1.0f) + vec2(1.f, -1.f) * fract(input.pos / scale),
                 levelCoord);

    vec3 colour = textureLod(transferFunction, texCoords, 0.0f).rgb;

    // Interpolation range in scalar value.
    float interpRange = (distInterp / 2.0f);
    // Scalar value distance to current level.
    float fraction = scalar - (floor(scalar / dist) * dist);
    // Scalar value distance to next level.
    float fraction2 = (ceil(scalar / dist) * dist) - scalar;

    // Interpolation to lower level. (No interpolation for lowest level or
    // clamped region!)
    if (interpRange > fraction && level < float(numLevels))
    {
        levelCoord = (((floor(scalar / dist) - 1.f) * 2.0f) + 1.0f)
                / float(2.0f * numLevels);
        float interpolation = (fraction / (2.0f * interpRange)) + 0.5f;

        texCoords = vec3(vec2(0.0f, 1.0f) + vec2(1.f, -1.f)
                         * fract(input.pos / scale), levelCoord);

        // Blend to previous level.If no previous level exists blend to white.
        colour = (interpolation * colour)
                + ((1.f - interpolation)
                   * max(textureLod(transferFunction, texCoords, 0.0f).rgb,
                         float(level == 0.0f))
                   );
    }
    // Interpolation to upper level. (No interpolation for highest level!)
    else if (fraction2 < interpRange && level < float(numLevels))
    {
        levelCoord =
                ((min(ceil(scalar / dist), numLevels - 1.0f) * 2.0f) + 1.0f) / float(2.0f * numLevels);
        float interpolation = (fraction2 / (2.0f * interpRange)) + 0.5f;

        texCoords = vec3(vec2(0.0f, 1.0f) + vec2(1.f, -1.f)
                         * fract(input.pos / scale), levelCoord);

        // Blend to next level. If no next level exists blend to white exept the
        // user wants to clamp the maximum value.
        colour = (interpolation * colour)
                + ((1.f - interpolation)
                   * max(textureLod(transferFunction, texCoords, 0.0f).rgb,
                         float(level == float(numLevels - 1) && !clampMaximum))
                   );
    }

    colour = min(vec3(.5f), colour);

    fragColour = vec4(colour, 1.0f - 2.0f * colour.r);
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(420)=VSmain();
    fs(420)=FSmain();
};

