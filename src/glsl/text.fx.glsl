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
 ***                             INTERFACES
 *****************************************************************************/

interface VStoFS
{
    smooth vec2 texCoord;
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

uniform vec2 offset;

shader VSText(in vec3 vertexCoord : 0, in vec2 texCoordIn : 1, out VStoFS Output)
{
    gl_Position = vec4(offset, 0, 0) + vec4(vertexCoord, 1);
    Output.texCoord = texCoordIn;
}


/*****************************************************************************/

uniform vec3 anchor;
uniform vec2 scale;

shader VSTextPool(in vec3 vertexCoord : 0, in vec2 texCoordIn : 1, out VStoFS Output)
{
    gl_Position = vec4(anchor.x + vertexCoord.x * scale.x,
                       anchor.y + vertexCoord.y * scale.y,
                       anchor.z,
                       1);
    Output.texCoord = texCoordIn;
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler2D textAtlas;
uniform vec4 colour;

shader FSmain(in VStoFS Input, out vec4 fragColour)
{
    fragColour = vec4(1, 1, 1, texture(textAtlas, Input.texCoord).a) * colour;
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Text
{
    vs(330)=VSText();
    fs(330)=FSmain();
};


program TextPool
{
    vs(330)=VSTextPool();
    fs(330)=FSmain();
};
