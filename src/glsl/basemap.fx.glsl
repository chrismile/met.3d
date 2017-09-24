/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015-2017 Michael Kern
**  Copyright 2015-2017 Bianca Tost
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
#define M_PI 3.1415926535897932384626433832795
#define DEG2RAD M_PI / 180.0
#define RAD2DEG 180.0 / M_PI

interface VStoFS
{
    smooth vec2 texCoord2D;
    smooth vec2 position2D;
};


uniform float poleLat;
uniform float poleLon;

// Parts of the following method have been ported from the C implementation of
// the methods 'lamrot_to_lam' and 'phirot_to_phi'. The original code has been
// published under GNU GENERAL PUBLIC LICENSE Version 2, June 1991.
// source: https://code.zmaw.de/projects/cdo/files  [Version 1.8.1]
// Necessary code duplicate in naturalearthdataloader.cpp . (Contains copy of
// original code.)
vec2 rotatedToGeograhpicalCoords(vec2 position)
{
    // Early break for rotation values with no effect.
    if ((poleLon == -180. || poleLon == 180.) && poleLat == 90.)
    {
        return position;
    }

    float result = 0.0f;

    // Get longitude and latitude from position.
    float rotLon = position.x;
    float rotLat = position.y;

    if ( rotLon > 180.0f )
    {
        rotLon -= 360.0f;
    }

    // Convert degrees to radians.
    float poleLatRad = DEG2RAD * poleLat;
    float poleLonRad = DEG2RAD * poleLon;
    float rotLonRad = DEG2RAD * rotLon;

    // Compute sinus and cosinus of some coordinates since they are needed more
    // often later on.
    float sinPoleLat = sin(poleLatRad);
    float cosPoleLat = cos(poleLatRad);
    float sinRotLatRad = sin(DEG2RAD * rotLat);
    float cosRotLatRad = cos(DEG2RAD * rotLat);
    float cosRotLonRad = cos(DEG2RAD * rotLon);

    // Apply the transformation (conversation to Cartesian coordinates and  two
    // rotations; difference to original code: no use of polgam).

    float x =
            (cos(poleLonRad) * (((-sinPoleLat) * cosRotLonRad * cosRotLatRad)
                                + (cosPoleLat * sinRotLatRad)))
            + (sin(poleLonRad) * sin(rotLonRad) * cosRotLatRad);
    float y =
            (sin(poleLonRad) * (((-sinPoleLat) * cosRotLonRad * cosRotLatRad)
                                + (cosPoleLat * sinRotLatRad)))
            - (cos(poleLonRad) * sin(rotLonRad) * cosRotLatRad);
    float z = cosPoleLat * cosRotLatRad * cosRotLonRad
            + sinPoleLat * sinRotLatRad;

    // Avoid invalid values for z (Might occure due to inaccuracies in
    // computations).
    z = max(-1., min(1., z));

    // Compute spherical coordinates from Cartesian coordinates and convert
    // radians to degrees.

    if ( abs(x) > 0.f )
    {
        result = RAD2DEG * atan(y, x);
    }
    if ( abs(result) < 9.e-14 )
    {
        result = 0.f;
    }

    position.x = result;
    position.y = RAD2DEG * (asin(z));

    return position;
}


uniform float colourIntensity; // 0..1 with 1 = rgb texture used, 0 = grey scales

// Changes the intensity of the colour value given in rgbaColour according to
// the scale given in colourIntensity [0 -> gray, 1 -> unchanged saturation].
vec4 adaptColourIntensity(vec4 rgbaColour)
{
    // If colourIntensity < 1 turn RGB texture into grey scales. See
    // http://stackoverflow.com/questions/687261/converting-rgb-to-grayscale-intensity
    vec3 rgbColour = rgbaColour.rgb;
    vec3 grey = vec3(0.2989, 0.5870, 0.1140);
    vec3 greyColour = vec3(dot(rgbColour, grey));
    return vec4(mix(greyColour, rgbColour, colourIntensity), rgbaColour.a);
}


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

uniform mat4 mvpMatrix;
// bounding box corners
uniform vec4 cornersBox; //(llcrnrlon, llcrnrlat, urcrnrlon, urcrnrlat)
uniform vec4 cornersData;
uniform vec4 cornersRotatedBox;

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

// Vertex shader for base maps on rotated grid. Uses coordinates of bounding box
// in rotated coordinates to get potential render area. Does not compute texture
// coordinates since we first need to trasform the rotated coordinates back to
// real geographical coordinates before sampling in texture (done in fragment
// shader).
shader VSBasemapForRotatedGrid(out VStoFS Output)
{
    // Calculate the indices with the vertex ID.
    int idX = gl_VertexID % 2;
    int idY = gl_VertexID / 2;

    // Compute map boundaries wtr to its bounding box in rotated coordinates.
    vec2 position;
    position.x = cornersRotatedBox.x
            + idX * (cornersRotatedBox.z - cornersRotatedBox.x);
    position.y = cornersRotatedBox.w
            - idY * (cornersRotatedBox.w - cornersRotatedBox.y);

    // Store position in rotated coordinates but not projected to be able to
    // sample the map in the fragment shader.
    Output.position2D = vec2(position.x, position.y);
    gl_Position = mvpMatrix * vec4(position, 0, 1);
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler2D mapTexture;

shader FSmain(in VStoFS Input, out vec4 fragColour)
{
    vec4 rgbaColour = vec4(texture(mapTexture, Input.texCoord2D).rgb, 1);

    fragColour = adaptColourIntensity(rgbaColour);
}


// Fragment shader for base map on rotated grids but with bounding box
// coordinates given by the user treated as rotated coordinates.
shader FSBasemapForRotatedGridWithRotatedBBox(in VStoFS Input,
                                              out vec4 fragColour)
{
    // Get position in real geographical coordinates.
    vec2 position = rotatedToGeograhpicalCoords(Input.position2D);

    // Get texture coordinates of position.
    float texCoordX = (position.x - cornersData.x) / (cornersData.z - cornersData.x);
    float texCoordY = (cornersData.w - position.y) / (cornersData.w - cornersData.y);
    vec4 rgbaColour = vec4(texture(mapTexture, vec2(texCoordX, texCoordY)).rgb, 1);

    fragColour = adaptColourIntensity(rgbaColour);
}


// Fragment shader for base map on rotated grids but with bounding box
// coordinates given by the user treated as geographical coordinates. Since the
// projection does not always results in rectagle map the bounding box might be
// chosen to be greater than the map. Thus it is necessary to discard fragments
// not part of the map to draw.
shader FSBasemapForRotatedGrid(in VStoFS Input,
                               out vec4 fragColour)
{
    // Get position in real geographical coordinates.
    vec2 position = rotatedToGeograhpicalCoords(Input.position2D);
    bool fragmentToDisard = false;

    // Left corner of the bounding box has a longitude coordinate smaller than
    // the longitude coordinate of the right corner, i.e. box can be treated as
    // one, continuous box in x direction.
    if (cornersBox.x < cornersBox.z)
    {
        fragmentToDisard =
                (position.x < cornersBox.x || position.x > cornersBox.z);
    }
    // Left corner of the bounding box has a longitude coordinate greater than
    // the longitude coordinate of the right corner, i.e. box consists of two
    // seperate parts in x direction (due to the transformation mapping to
    // [-180, 180] only).
    else
    {
        fragmentToDisard =
                (position.x < cornersBox.x && position.x > cornersBox.z);
    }

    // Lower corner of the bounding box has a latitude coordinate smaller than
    // the latitude coordinate of the upper corner, i.e. box can be treated as
    // one, continuous box in y direction.
    if (cornersBox.y < cornersBox.w)
    {
        fragmentToDisard = fragmentToDisard ||
                (position.y < cornersBox.y || position.y > cornersBox.w);
    }
    // Lower corner of the bounding box has a latitude coordinate greater than
    // the latitude coordinate of the upper corner, i.e. box consists of two
    // seperate parts in y direction (due to the transformation mapping to
    // [-90, 90] only).
    else
    {
        fragmentToDisard = fragmentToDisard ||
                (position.y < cornersBox.y && position.y > cornersBox.w);
    }

    // Discard fragment if it is not part of the map to be shown.
    if (fragmentToDisard)
    {
        discard;
    }

    float texCoordX = (position.x - cornersData.x) / (cornersData.z - cornersData.x);
    float texCoordY = (cornersData.w - position.y) / (cornersData.w - cornersData.y);
    vec4 rgbaColour = vec4(texture(mapTexture, vec2(texCoordX, texCoordY)).rgb, 1);

    fragColour = adaptColourIntensity(rgbaColour);
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Basemap
{
    vs(330)=VSmain();
    fs(330)=FSmain();
};

program BasemapRotation
{
    vs(330)=VSBasemapForRotatedGrid();
    fs(330)=FSBasemapForRotatedGrid();
};

program BasemapRotationRotatedBBox
{
    vs(330)=VSBasemapForRotatedGrid();
    fs(330)=FSBasemapForRotatedGridWithRotatedBBox();
};
