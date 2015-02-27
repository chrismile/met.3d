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
    smooth float  sectionPosition;
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

uniform sampler1D path;              // points along the vertical section path
uniform sampler1D pressureLevels;    // levels at which lines should be drawn
uniform vec2      pToWorldZParams;   // parameters to convert p[hPa] to world z
uniform mat4      mvpMatrix;         // model-view-projection


shader VSmain(out VStoFS Output)
{
    // m = index of point in "path" (horizontal dimension); kp = index of
    // pressure level in "pressureLevels".
    int m  = gl_VertexID;
    int kp = gl_InstanceID;

    float lon   = texelFetch(path, 4*m    , 0).a;
    float lat   = texelFetch(path, 4*m + 1, 0).a;
    float p_hPa = texelFetch(pressureLevels, kp, 0).a;

    // Convert pressure to world Z coordinate.
    float z = (log(p_hPa) - pToWorldZParams.x) * pToWorldZParams.y;

    vec3 vertexPosition = vec3(lon, lat, z);

    // Convert the position from world to clip space.
    gl_Position = mvpMatrix * vec4(vertexPosition, 1);

    Output.sectionPosition = float(4 * m) / float(textureSize(path, 0));
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

shader FSmain(in VStoFS Input, out vec4 fragColour)
{
    int pos = int(Input.sectionPosition * 200);
    if (bool(pos & 1)) discard;
    fragColour = vec4(0, 0, 0, 1);
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(420)=VSmain();
    fs(420)=FSmain();
};
