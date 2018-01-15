/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2016 Marc Rautenhaus
**  Copyright 2016      Bianca Tost
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

interface VStoFS
{
    smooth vec2 texCoords;
    smooth float texCoord;
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec3 vertexCoord : 0, in vec3 texCoordIn : 1, out VStoFS Output)
{
    gl_Position = vec4(vertexCoord, 1);
    Output.texCoord = texCoordIn.r;

    Output.texCoords.x = texCoordIn.g; // float(gl_VertexID > 1);
    Output.texCoords.y = texCoordIn.r;
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler1D transferTexture;
uniform bool enableAlpha;

shader FSmain(in VStoFS Input, out vec4 fragColour)
{
    fragColour = texture(transferTexture, Input.texCoord);
    if (!enableAlpha) fragColour.a = 1.0;
}


uniform sampler2DArray spatialTransferTexture;

uniform float distInterp; // user

uniform float minimumValue;
uniform float maximumValue;

uniform int numLevels; // Amount of textures loaded.

uniform int viewPortWidth;
uniform int viewPortHeight;

uniform int textureWidth;
uniform int textureHeight;

uniform float barWidthF;
uniform float barHeightF;

uniform int alphaBlendingMode;
uniform bool invertAlpha;
uniform bool useConstantColour;
uniform vec4 constantColour;
uniform bool useWhiteBgForBar;

shader FSspatialTF(in VStoFS Input, out vec4 fragColour)
{
    // Float versions of the given sizes.
    float viewPortWidthF = float(viewPortWidth);
    float viewPortHeightF = float(viewPortHeight);

    float textureWidthF = float(textureWidth);
    float textureHeightF = float(textureHeight);

    // Scale of texture in colour bar.
    vec2 scale = vec2((textureWidthF / viewPortWidthF) * (2.0f / barWidthF),
                      (textureHeightF / viewPortHeightF) * (2.0f / barHeightF));

    float dist = 1.0f / float(numLevels);

    vec2 pos = Input.texCoords;

    // Get level position in 3D texture.
    float level = floor(pos.y / dist);

    vec3 texCoords = vec3(pos / scale, level);

    vec4 colour = textureLod(spatialTransferTexture, texCoords, 0.0f);

    // Interpolation range in texture coordinates.
    float interpRange = (distInterp / 2.0f) / (maximumValue - minimumValue);
    // Scalar value distance to current level in texture coordinates.
    float fraction = pos.y - (floor(pos.y / dist) * dist);
    // Scalar value distance to next level in texture coordinates.
    float fraction2 = (ceil(pos.y / dist) * dist) - pos.y;

    // Interpolation to previous level. (No interpolation for lowest level!)
    if (interpRange > fraction && floor(pos.y / dist) > 0.0f)
    {
        level = floor(pos.y / dist) - 1.0f;
        float interpolation = fraction / (2.0f * interpRange) + 0.5f;

        texCoords = vec3(pos / scale, level);

        colour = (interpolation * colour)
                + ((1.f - interpolation)
                   * textureLod(spatialTransferTexture, texCoords, 0.0f));
    }
    // Interpolation to next level. (No interpolation for highest level!)
    else if (fraction2 < interpRange
             && floor(pos.y / dist) < float(numLevels - 1))
    {
        level = ceil(pos.y / dist);
        float interpolation = fraction2 / (2.0f * interpRange) + 0.5f;

        texCoords = vec3(pos / scale, level);

        colour = (interpolation * colour)
                + ((1.f - interpolation)
                   * textureLod(spatialTransferTexture, texCoords, 0.0f));
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

    if (useWhiteBgForBar)
    {
        colour = vec4(colour.rgb * colour.a + vec3(1.0f) * (1.f - colour.a),
                      1.0f);
    }

    fragColour = colour;

}

/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program colourbarTF
{
    vs(330)=VSmain();
    fs(330)=FSmain();
};

program spatialTF
{
    vs(330)=VSmain();
//    fs(330)=FSleveled();
    fs(330)=FSspatialTF();
};
