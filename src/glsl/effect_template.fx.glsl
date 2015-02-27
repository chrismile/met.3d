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
 ***                             CONSTANTS
 *****************************************************************************/

/* - use const datatype instead of #define */


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

/* - connection between different stages.
   - defines custom input and output variables
   - acts like a struct

    interface VStoFS {
        smooth vec3 pos;
    }
*/


/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

/* - variables common to all shader stages */


/*****************************************************************************
 ***                             INCLUDES
 *****************************************************************************/

/*
  - you can include several files via: #include "filename.glsl"
  - #include is simply replaced by glfx with included code
  - error messages carry an index, indicating which file caused the error
  */


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

/* - input and output given in function parameters
   - in vec3 vertexElement : location
*/

shader VSmain(in vec3 pos : 0)
{

}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

/* - in and output geometry layout is defined in program sections
   - input of several custom vertexData: in VStoGS input[] (must be array!)
*/


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

shader FSmain(out vec4 fragColour)
{

}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

/*
  - defines one program
  - vs(number_of_gl_version)=functionName();
  - gs(number_of_gl_version)=functionName() : in(geom_in), out(geom_out, max_vertices=max, stream=num)
  - fs(number_of_gl_version)=functionName();
*/

program Standard
{
    vs(420)=VSmain();
    //gs(420)=GSmain() : in(points), out(line_strip, max_vertices = 4);
    fs(420)=FSmain();
};
