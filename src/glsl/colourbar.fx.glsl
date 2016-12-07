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
    smooth float texCoord;
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec3 vertexCoord : 0, in vec3 texCoordIn : 1, out VStoFS Output)
{
    gl_Position = vec4(vertexCoord, 1);
    Output.texCoord = texCoordIn.r;
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


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(330)=VSmain();
    fs(330)=FSmain();
};
