/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2014 Marc Rautenhaus
**  Copyright 2014 Michael Kern, Technische Universitaet Muenchen
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
**  CHANGELOG:
**  ==========
**
******************************************************************************/


/*****************************************************************************
 ***                             CONSTANTS
 *****************************************************************************/


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform mat4 mvpMatrix;

// contains section boundaries
uniform vec4 cornersSection; //(llcrnrlon, llcrnrlat, width, height)
// contains color of shadow
uniform vec4 colour;

uniform float height;

/*****************************************************************************
 ***                             INCLUDES
 *****************************************************************************/


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

/* - input and output given in function parameters
   - in vec3 vertexElement : location
*/


shader VSmain()
{
    // calculate the indices with the vertex ID
    int idX = gl_VertexID % 2;
    int idY = gl_VertexID / 2;

    // compute map boundaries wtr to its bounding box
    vec2 position;
    position.x = cornersSection.x + idX * cornersSection.z;
    position.y = cornersSection.y + cornersSection.w * (1 - idY);

    gl_Position = mvpMatrix * vec4(position, height, 1);
}

/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

shader FSmain(out vec4 fragColour)
{
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
