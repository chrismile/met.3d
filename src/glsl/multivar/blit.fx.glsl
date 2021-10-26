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
uniform int supersamplingFactor;


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


shader FSmainDownscale(in VStoFS Input, out vec4 fragColor)
{
    ivec2 inputSize = textureSize(blitTexture, 0);
    ivec2 outputSize = inputSize / supersamplingFactor;
    ivec2 outputLocation = ivec2(int(Input.texCoord.x * outputSize.x), int(Input.texCoord.y * outputSize.y));
    vec4 color = vec4(0.0);
    vec4 totalSum = vec4(0.0);
    for (int sampleIdxY = 0; sampleIdxY < supersamplingFactor; sampleIdxY++)
    {
        for (int sampleIdxX = 0; sampleIdxX < supersamplingFactor; sampleIdxX++)
        {
            ivec2 inputLocation = outputLocation * supersamplingFactor + ivec2(sampleIdxX, sampleIdxY);
            vec4 sampleColor = texelFetch(blitTexture, inputLocation, 0);
            totalSum += sampleColor;
            color.rgb += sampleColor.rgb * sampleColor.a;
            color.a += sampleColor.a;
        }
    }

    int totalNumSamples = supersamplingFactor * supersamplingFactor;
    color /= float(totalNumSamples);
    if (color.a > 1.0 / 256.0)
    {
        fragColor = vec4(color.rgb / color.a, color.a);
    } else
    {
        fragColor = totalSum / float(totalNumSamples);
    }
}

shader FSmainDownscaleMS(in VStoFS Input, out vec4 fragColor)
{
    ivec2 inputSize = textureSize(blitTextureMS);
    ivec2 outputSize = inputSize / supersamplingFactor;
    ivec2 outputLocation = ivec2(int(Input.texCoord.x * outputSize.x), int(Input.texCoord.y * outputSize.y));
    vec4 color = vec4(0.0);
    vec4 totalSum = vec4(0.0);
    for (int sampleIdxY = 0; sampleIdxY < supersamplingFactor; sampleIdxY++)
    {
        for (int sampleIdxX = 0; sampleIdxX < supersamplingFactor; sampleIdxX++)
        {
            ivec2 inputLocation = outputLocation * supersamplingFactor + ivec2(sampleIdxX, sampleIdxY);
            for (int sampleIdx = 0; sampleIdx < numSamples; sampleIdx++)
            {
                vec4 sampleColor = texelFetch(blitTextureMS, inputLocation, sampleIdx);
                totalSum += sampleColor;
                color.rgb += sampleColor.rgb * sampleColor.a;
                color.a += sampleColor.a;
            }
        }
    }

    int totalNumSamples = supersamplingFactor * supersamplingFactor * numSamples;
    color /= float(totalNumSamples);
    if (color.a > 1.0 / 256.0)
    {
        fragColor = vec4(color.rgb / color.a, color.a);
    }
    else
    {
        fragColor = totalSum / float(totalNumSamples);
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

program Downscale
{
    vs(430)=VSmain();
    fs(430)=FSmainDownscale();
};

program DownscaleMultisampled
{
    vs(430)=VSmain();
    fs(430)=FSmainDownscaleMS();
};
