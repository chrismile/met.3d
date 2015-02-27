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
    smooth vec2 texCoord2D;
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

uniform mat4 mvpMatrix;
// bounding box corners
uniform vec4 cornersBox; //(llcrnrlon, llcrnrlat, urcrnrlon, urcrnrlat)
uniform vec4 cornersData;

//shader VSmain(in vec4 vertex0 : 0, in vec2 texCoord2D0 : 1, out VStoFS Output)
shader VSmain(out VStoFS Output)
{
    // calculate the indices with the vertex ID
    int idX = gl_VertexID % 2;
    int idY = gl_VertexID / 2;

    // compute map boundaries wtr to its bounding box
    vec2 position;
    position.x = cornersBox.x + idX * (cornersBox.z - cornersBox.x);
    position.y = cornersBox.w - idY * (cornersBox.w - cornersBox.y);

    // calculate the texture coordinates on the fly
    float texCoordX = (position.x - cornersData.x) / (cornersData.z - cornersData.x);
    float texCoordY = (cornersData.w - position.y) / (cornersData.w - cornersData.y);

    gl_Position = mvpMatrix * vec4(position, 0, 1);
    Output.texCoord2D = vec2(texCoordX, texCoordY);
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler2D mapTexture;
uniform float colourIntensity; // 0..1 with 1 = rgb texture used, 0 = grey scales

shader FSmain(in VStoFS Input, out vec4 fragColour)
{
    vec4 rgbaColour = vec4(texture(mapTexture, Input.texCoord2D).rgb, 1);
    
    // If colourIntensity < 1 turn RGB texture into grey scales. See
    // http://stackoverflow.com/questions/687261/converting-rgb-to-grayscale-intensity
    vec3 rgbColour = rgbaColour.rgb;
    vec3 grey = vec3(0.2989, 0.5870, 0.1140);
    vec3 greyColour = vec3(dot(rgbColour, grey));
    fragColour = vec4(mix(greyColour, rgbColour, colourIntensity), rgbaColour.a);
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Basemap
{
    vs(330)=VSmain();
    fs(330)=FSmain();
};
