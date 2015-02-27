/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Michael Kern
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
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

// modelview projection matrix
uniform mat4 mvpMatrix;

// vertex data (containing position of volume box vec3(lon,lat,pressure Pa)
shader VSmain(in vec3 vertex0 : 0, in vec2 texcoord0 : 1, out VStoFS Output)
{
    // position of vertex in projection space
    gl_Position = mvpMatrix * vec4(vertex0,1);
    Output.texCoords = texcoord0;
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler2D texImage;

shader FSmain(in VStoFS Input, out vec4 fragColor)
{
    fragColor = textureLod(texImage,Input.texCoords,0);
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(420)=VSmain();
    fs(420)=FSmain();
};
