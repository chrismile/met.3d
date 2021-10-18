/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2021 Christoph Neuhauser
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
 ***                             UNIFORMS
 *****************************************************************************/

uniform mat4 mvpMatrix;
uniform sampler2D blitTexture;
uniform sampler2DMS blitTextureMS;
uniform int numSamples;


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

interface VStoFS
{
    vec2 texCoord;
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec3 vertexPosition : 0, in vec2 vertexTexCoord : 1, out VStoFS Output)
{
    Output.texCoord = vertexTexCoord;
    gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

shader FSmain(in VStoFS Input, out vec4 fragColor)
{
    fragColor = texture(blitTexture, Input.texCoord);
}

shader FSmainMS(in VStoFS Input, out vec4 fragColor)
{
    ivec2 size = textureSize(blitTextureMS);
    ivec2 iCoords = ivec2(int(Input.texCoord.x * size.x), int(Input.texCoord.y * size.y));
    vec4 color = vec4(0.0);
    vec4 totalSum = vec4(0.0);
    for (int currSample = 0; currSample < numSamples; currSample++)
    {
        vec4 fetchedColor = texelFetch(blitTextureMS, iCoords, currSample);
        totalSum += fetchedColor;
        color.rgb += fetchedColor.rgb * fetchedColor.a;
        color.a += fetchedColor.a;
    }
    color /= float(numSamples);
    if (color.a > 1.0 / 256.0)
    {
        fragColor = vec4(color.rgb / color.a, color.a);
    }
    else
    {
        fragColor = totalSum / float(numSamples);
    }
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(430)=VSmain();
    fs(430)=FSmain();
};

program Multisampled
{
    vs(430)=VSmain();
    fs(430)=FSmainMS();
};
