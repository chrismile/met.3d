/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2016 Christoph Heidelmann
**  Copyright 2018      Bianca Tost
**  Copyright 2015-2018 Marc Rautenhaus
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

const int SURFACE_2D = 0;
const int PRESSURE_LEVELS_3D = 1;
const int HYBRID_SIGMA_PRESSURE_3D = 2;
const int POTENTIAL_VORTICITY_2D = 3;
const int LOG_PRESSURE_LEVELS_3D = 4;
const int AUXILIARY_PRESSURE_3D = 5;


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

interface GStoFS
{
    smooth vec2 texCoord;
};


/*****************************************************************************
 ***                                STRUCTS                                ***
 *****************************************************************************/

struct Variables
{
    float p_hPa;
    int nLon, nLat, numberOfLevels;
    float deltaLatLon, minLon, minLat, posX, posY;
    ivec3 texPos;
} v;


struct Area
{
    float left, right, top, bottom;
};


/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform float  topPressure;
uniform float  bottomPressure;
uniform struct Area area;           // diagram outline
uniform mat4   mvpMatrix;           // model-view-projection
uniform vec2   pToWorldZParams;     // convert pressure to y axis
uniform vec2   pToWorldZParams2;     // convert pressure to y axis
uniform vec4   colour;
uniform mat4   viewMatrix;          // view
uniform float  yOffset;             // y-offset for switching between top and
                                    // bottom temperature
uniform vec2  position;
uniform vec2  clipPos;
uniform float temperatureAmplitude, temperatureCenter;
uniform float actTemperature, actPressure;
uniform float humidityVal, temperatureVal;
uniform vec4  humidityColour, temperatureColour;
uniform float layer;
uniform int   ensemble;
uniform int   numberOfLevels;
uniform int   numberOfLats;

uniform int       levelType;        // vertical level type of the data grid
uniform sampler1D lonLatLevAxes;    // 1D texture that holds both lon and lat
uniform sampler3D dataField;        // 3D texture holding the scalar data
uniform sampler2D dataField2D;      // 2D texture holding the scalar data
                                     // in case of surface fields
uniform sampler2D surfacePressure;   // surface pressure field in Pa
uniform sampler1D hybridCoefficients;// hybrid sigma pressure coefficients

uniform bool drawHumidity, drawTemperature;
uniform bool pressureEqualsWorldPressure;
uniform bool fullscreen;
uniform bool uprightOrientation;
uniform bool doWyomingTemp, doWyomingDew;
varying vec4 position_in_world_space;

// Transfer function variables
uniform sampler1D transferFunction; // 1D transfer function
uniform bool      useTransferFunction; // 1D transfer function
uniform float     scalarMinimum;    // min/max data values to scale to 0..1
uniform float     scalarMaximum;

// ProbabilityTubes variables
uniform int       levelTypeMin;         // vertical level type of the data grid
uniform sampler1D lonLatLevAxesMin;    // 1D texture that holds both lon and lat
uniform sampler3D dataFieldMin;         // 3D texture holding the scalar data
uniform sampler2D dataField2DMin;       // 2D texture holding the scalar data
uniform sampler2D surfacePressureMin;   // surface pressure field in Pa
uniform sampler1D hybridCoefficientsMin;// hybrid sigma pressure coefficients
uniform vec4      colourMin;

uniform int       levelTypeMax;        // vertical level type of the data grid
uniform sampler1D lonLatLevAxesMax;    // 1D texture that holds both lon and lat
uniform sampler3D dataFieldMax;         // 3D texture holding the scalar data
uniform sampler2D dataField2DMax;       // 2D texture holding the scalar data
uniform sampler2D surfacePressureMax;   // surface pressure field in Pa
uniform sampler1D hybridCoefficientsMax;// hybrid sigma pressure coefficients
uniform vec4 colourMax;

// Deviation variables

uniform int levelTypeMean;             // vertical level type of the data grid
uniform sampler1D lonLatLevAxesMean;   // 1D texture that holds both lon and lat
uniform sampler3D dataFieldMean;         // 3D texture holding the scalar data
uniform sampler2D dataField2DMean;       // 2D texture holding the scalar data
uniform sampler2D surfacePressureMean;   // surface pressure field in Pa
uniform sampler1D hybridCoefficientsMean;// hybrid sigma pressure coefficients

uniform int levelTypeDeviation;         // vertical level type of the data grid
uniform sampler1D lonLatLevAxesDeviation;     // 1D texture that holds both lon
                                              // and lat
uniform sampler3D dataFieldDeviation;         // 3D texture holding the scalar
                                              // data
uniform sampler2D dataField2DDeviation;       // 2D texture holding the scalar
                                              // data
uniform sampler2D surfacePressureDeviation;   // surface pressure field in Pa
uniform sampler1D hybridCoefficientsDeviation;// hybrid sigma pressure
                                              // coefficients

/*****************************************************************************
 ***                                METHODS                                ***
 *****************************************************************************/

float calculateDewpoint(float humidity, float pressure)
{
    // mixing ratio
    float w = humidity / (1. - humidity);
    // Compute vapour pressure from pressure and mixing ratio
    // (Wallace and Hobbs 2nd ed. eq. 3.59).
    // (p_hPa * 100) = conversion to pascal
    float e_q = w / (w + 0.622) * (pressure * 100.);
    // method is Bolton
    return 243.5 / (17.67 / log(e_q / 100. / 6.112) - 1.);
}


float projection2D(float scalar, float y)
{
    // (temperature + temperature legend center) / (abs(min) + abs(max))  *
    // (scale to fit into width)
    float t = ((scalar + temperatureCenter) / temperatureAmplitude) *
            (area.right - area.left);
    return (t + y + area.left);
}


vec2 projectOnPlane(float scalar, float pressure, float y)
{
    vec2 ret = vec2(0., y);
    if (drawHumidity)
    {
        float td = calculateDewpoint(scalar, pressure);
        ret.x = projection2D(td, y);
    }
    else
    {
        ret.x = projection2D(scalar - 273.15, y);
    }
    if (!pressureEqualsWorldPressure)
    {
        ret.y = ret.y + 0.05;
    }
    return ret;
}


float interpolate(sampler3D dataField, float deltaLatLon, ivec3 texPos,
                  float posX, float posY)
{
    float t_i0j0 = texelFetch(dataField, texPos, 0).a;
    texPos = texPos + ivec3(deltaLatLon, 0, 0);
    float t_i1j0 = texelFetch(dataField, texPos, 0).a;
    texPos = texPos + ivec3(-deltaLatLon, deltaLatLon, 0);
    float t_i0j1 = texelFetch(dataField, texPos, 0).a;
    texPos = texPos + ivec3(deltaLatLon, 0, 0);
    float t_i1j1 = texelFetch(dataField, texPos, 0).a;
    float mixX = fract(posX);
    float mixY = fract(posY);
    float t_j0 = mix(t_i0j0, t_i1j0, mixX);
    float t_j1 = mix(t_i0j1, t_i1j1, mixX);
    return mix(t_j0, t_j1, mixY);
}


void setVariables(sampler3D dataField, sampler1D hybridCoefficients,
                  sampler1D lonLatLevAxes, int vertexID)
{
    v.p_hPa = 1050.;
    v.nLon = textureSize(dataField, 0).x;
    v.nLat = numberOfLats > 0 ? numberOfLats : textureSize(dataField, 0).y;
    v.numberOfLevels = numberOfLevels > 0
            ? numberOfLevels : textureSize(hybridCoefficients, 0) / 2;
    v.deltaLatLon = abs(texelFetch(lonLatLevAxes, 1, 0).a
                        - texelFetch(lonLatLevAxes, 0, 0).a);
    v.minLon = texelFetch(lonLatLevAxes, 0 , 0).a;
    v.minLat = texelFetch(lonLatLevAxes, v.nLon, 0).a;
    v.posX = mod((position.x - v.minLon) / v.deltaLatLon, 360.);
    v.posY = ( v.minLat - position.y ) / v.deltaLatLon;
    v.texPos = ivec3(v.posX, v.posY, vertexID);
}


float calculatePressureLevel(int levelType, sampler1D hybridCoefficients,
                             sampler2D surfacePressure, int vertexID )
{
    if (levelType == HYBRID_SIGMA_PRESSURE_3D)
    {
        float ak = texelFetch(hybridCoefficients, vertexID, 0).a;
        float bk = texelFetch(hybridCoefficients, vertexID + v.numberOfLevels,
                              0).a;
        int surfaceMemberPosition = 0;
        int intX = int(v.posX);
        int intY = surfaceMemberPosition + int(v.posY);
        float p_i0j0 = ak + bk
                * (texelFetch(surfacePressure, ivec2(intX, intY), 0).a / 100.);
        float p_i1j0 = ak + bk
                * (texelFetch(surfacePressure,
                              ivec2((intX + 1) % v.nLon, intY), 0).a) / 100.;
        float p_i0j1 = ak + bk
                * texelFetch(surfacePressure,
                             ivec2(intX , (intY + 1) % v.nLat), 0).a / 100.;
        float p_i1j1 = ak + bk
                * (texelFetch(surfacePressure,
                              ivec2((intX + 1) % v.nLon, (intY + 1) % v.nLat),
                              0).a / 100.);
        float mixX = fract(v.posX);
        float mixY = fract(v.posY);
        float p_j0  = mix(p_i0j0, p_i1j0, mixX);
        float p_j1  = mix(p_i0j1, p_i1j1, mixX);
        return mix(p_j0, p_j1, mixY);
    }
    else if (levelType == PRESSURE_LEVELS_3D)
    {
        int vertOffset = v.nLon + v.nLat;
        return texelFetch(lonLatLevAxes, vertOffset + vertexID , 0).a;
    }
}


bool isInBounds(vec4 pos)
{
    if (fullscreen)
    {
        if ((pos.y <= area.top * 2 - 1) && (pos.y >= area.bottom * 2 - 1))
        {
            if ((pos.x >= area.left * 2 - 1) && (pos.x <= area.right * 2 - 1))
            {
                return true;
            }
        }
        return false;
    }
    else
    {
        if ((pos.y <= area.top) && (pos.y >= area.bottom))
        {
            if ((pos.x >= area.left) && (pos.x <= area.right))
            {
                return true;
            }
        }
        return false;
    }
}


bool isInLeftRightBounds(vec4 pos)
{
    if (fullscreen)
    {
        if ((pos.x >= area.left * 2 - 1) && (pos.x <= area.right * 2 - 1))
        {
            return true;
        }
        return false;
    }
    else
    {
        if ((pos.x >= area.left) && (pos.x <= area.right))
        {
            return true;
        }
        return false;
    }

}


bool isInTopBottomBounds(vec4 pos)
{
    if ((pos.y <= area.top) && (pos.y >= area.bottom))
    {
        return true;
    }
    return false;
}


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSVariables(out vec2 worldPos)
{
    setVariables(dataField, hybridCoefficients, lonLatLevAxes, gl_VertexID );
    float p_hPa = calculatePressureLevel(levelType, hybridCoefficients,
                                         surfacePressure, gl_VertexID);
    float scalar = interpolate(dataField, v.deltaLatLon, v.texPos, v.posX,
                               v.posY);
    float log_p = log(p_hPa);
    float y = (((log_p - pToWorldZParams.x) * pToWorldZParams.y) / 36.0);
    worldPos = projectOnPlane(scalar, p_hPa, y);


    if (pressureEqualsWorldPressure)
    {
        float slopePtoZ = 36.0f / (log(20.) - log(1050.));
        worldPos.y = exp(((worldPos.y) / slopePtoZ) + log(1050.));
        worldPos.y = ((log(worldPos.y) - pToWorldZParams2.x) * pToWorldZParams2.y);
    }
}


shader VSTubes(out vec2 worldPos, out vec4 sideColour)
{
        if (int(gl_VertexID) % 2 == 0)
        {
            setVariables(dataFieldMax, hybridCoefficientsMax,
                         lonLatLevAxesMax, int(gl_VertexID / 2));
            float p_hPa = calculatePressureLevel(
                        levelTypeMax, hybridCoefficientsMax,
                        surfacePressureMax, int(gl_VertexID / 2));
            float log_p = log(p_hPa);
            float y = (((log_p - pToWorldZParams.x) * pToWorldZParams.y)
                       / 36.0);
            float scalar = interpolate(dataFieldMax, v.deltaLatLon, v.texPos,
                                       v.posX, v.posY);
            worldPos = projectOnPlane(scalar, p_hPa, y);
            sideColour = colourMax;
        }
        else
        {
            setVariables(dataFieldMin, hybridCoefficientsMin,
                         lonLatLevAxesMin, int(gl_VertexID / 2));
            float p_hPa = calculatePressureLevel(
                        levelTypeMin, hybridCoefficientsMin,
                        surfacePressureMin, int(gl_VertexID / 2));
            float log_p = log(p_hPa);
            float y = (((log_p - pToWorldZParams.x) * pToWorldZParams.y) / 36.0);
            float scalar = interpolate(dataFieldMin, v.deltaLatLon, v.texPos,
                                       v.posX, v.posY);
            worldPos = projectOnPlane(scalar, p_hPa, y);
            sideColour = colourMin;
        }

        if (pressureEqualsWorldPressure)
        {
            float slopePtoZ = 36.0f / (log(20.) - log(1050.));
            worldPos.y = exp(((worldPos.y) / slopePtoZ) + log(1050.));
            worldPos.y = ((log(worldPos.y) - pToWorldZParams2.x) * pToWorldZParams2.y);
        }
}


shader VSDeviation(out vec2 worldPos)
{
    float scalarMean = 0, scalarDeviation = 0;
    setVariables(dataFieldMean, hybridCoefficientsMean,
                 lonLatLevAxesMean, int(gl_VertexID / 2));

    float p_hPa = calculatePressureLevel(
                levelTypeMean, hybridCoefficientsMean,
                surfacePressureMean, int(gl_VertexID / 2));

    float log_p = log(p_hPa);

    float y = (((log_p - pToWorldZParams.x) * pToWorldZParams.y) / 36.0 );

    scalarMean = interpolate(dataFieldMean, v.deltaLatLon, v.texPos, v.posX,
                             v.posY);
    scalarDeviation = interpolate(dataFieldDeviation, v.deltaLatLon, v.texPos,
                                  v.posX, v.posY);
    float scalar = 0;
    if (int(gl_VertexID) % 2 == 0)
    {
        scalar = scalarMean - scalarDeviation / 2;
    }
    else
    {
        scalar = scalarDeviation / 2 + scalarMean;
    }

    worldPos = projectOnPlane(scalar, p_hPa, y);

    if (pressureEqualsWorldPressure)
    {
        float slopePtoZ = 36.0f / (log(20.) - log(1050.));
        worldPos.y = exp(((worldPos.y) / slopePtoZ) + log(1050.));
        worldPos.y = ((log(worldPos.y) - pToWorldZParams2.x) * pToWorldZParams2.y);
    }
}


shader VSWyoming(in vec3 vertexCoord : 0, out vec2 worldPos)
{
    float p_hPa = vertexCoord.y;
    float temperature = vertexCoord.x;
    float mixingRatio = vertexCoord.z - 273.15;

    worldPos.y = (((log(p_hPa) - pToWorldZParams.x) * pToWorldZParams.y) / 36.0);

    if (drawHumidity)
    {
        worldPos.x = projection2D(mixingRatio - 273.15, worldPos.y);
    }
    if (drawTemperature)
    {
        worldPos.x = projection2D(temperature - 273.15, worldPos.y);
    }
    if (!pressureEqualsWorldPressure)
    {
        worldPos.y = worldPos.y + 0.05;
    }

    if (pressureEqualsWorldPressure)
    {
        float slopePtoZ = 36.0f / (log(20.) - log(1050.));
        worldPos.y = exp(((worldPos.y) / slopePtoZ) + log(1050.));
        worldPos.y = ((log(worldPos.y) - pToWorldZParams2.x) * pToWorldZParams2.y);
    }
}


shader VSWorld(out vec4 worldPos)
{
    // Convert from world space to clip space.
    worldPos = vec4(position + vec2(0, yOffset), 0, 1.);
}


shader VS2D(in vec2 vertexCoord : 0, out vec2 worldPos)
{
    worldPos = vertexCoord;
}


shader VS2DVertexYInPressure(in vec2 vertexCoord : 0, out vec2 worldPos)
{
    worldPos.x = vertexCoord.x;
    if (pressureEqualsWorldPressure)
    {
        worldPos.y = ((log(vertexCoord.y) - log(1050.)) * pToWorldZParams2.y);
    }
    else
    {
        float slopePtoZ = (36.0f - 0.1f * 36.0f) / (log(topPressure) - log(bottomPressure));
        worldPos.y = ((log(vertexCoord.y) - log(bottomPressure)) * slopePtoZ);
    }
}


shader VSMarkingCircles(out vec2 worldPos, out vec4 markercolour)
{
    if (drawHumidity)
    {
        worldPos.x = ((humidityVal + temperatureCenter)
                      / temperatureAmplitude) * (area.right - area.left);
        worldPos.y = (clipPos.y + 1.0) / 2.0;
        worldPos.x = (worldPos.x + worldPos.y);
    }
    if (drawTemperature)
    {
        worldPos.x = ((temperatureVal - 273.15 + temperatureCenter)
                      / temperatureAmplitude) * (area.right - area.left);
        worldPos.y = (clipPos.y + 1.0) / 2.0;
        worldPos.x = (worldPos.x + worldPos.y);
    }
}


/*****************************************************************************
 ***                           GEOMETRY SHADER
 *****************************************************************************/

shader GSMarkingCircles(in vec2 worldPos[])
{
    float size = 0.0025;
    position_in_world_space = vec4(worldPos[0] - vec2(size, size), 0, 1);
    gl_Position = position_in_world_space * vec4(2, 2, 0, 1)
            + vec4(-1.0, -1.0, 0, 0);
    EmitVertex();

    position_in_world_space = vec4(worldPos[0] + vec2(-size, size), 0, 1);
    gl_Position = position_in_world_space * vec4(2, 2, 0, 1)
            + vec4(-1.0, -1.0, 0, 0);
    EmitVertex();

    position_in_world_space = vec4(worldPos[0] + vec2(size, size), 0, 1);
    gl_Position = position_in_world_space * vec4(2, 2, 0, 1)
            + vec4(-1.0, -1.0, 0, 0);
    EmitVertex();

    position_in_world_space = vec4(worldPos[0] + vec2(size, size), 0, 1);
    gl_Position = position_in_world_space * vec4(2, 2, 0, 1)
            + vec4(-1.0, -1.0, 0, 0);
    EmitVertex();

    position_in_world_space = vec4(worldPos[0] + vec2(size, -size), 0, 1);
    gl_Position = position_in_world_space * vec4(2, 2, 0, 1)
            + vec4(-1.0, -1.0, 0, 0);
    EmitVertex();

    position_in_world_space = vec4(worldPos[0] - vec2(size, size), 0, 1);
    gl_Position = position_in_world_space * vec4(2, 2, 0, 1)
            + vec4(-1.0, -1.0, 0, 0);
    EmitVertex();

    EndPrimitive();

}


shader GSMouseOverCross()
{
    position_in_world_space = vec4(
                clipPos.x - clipPos.y + ((area.top * 2.0) - 1.0),
                (area.top * 2.0) - 1.0 , -1.0 , 1.0);
    gl_Position = position_in_world_space;
    EmitVertex();

    position_in_world_space = vec4(
                clipPos.x - clipPos.y - ((area.top * 2.0) - 1.0),
                (area.bottom * 2.0)- 1.0 , -1.0, 1.0);
    gl_Position = position_in_world_space;
    EmitVertex();

    EndPrimitive();

    position_in_world_space = vec4((area.left * 2.0)- 1.0, clipPos.y, -1.0, 1.0);
    gl_Position = position_in_world_space;
    EmitVertex();

    position_in_world_space = vec4((area.right * 2.0)- 1.0, clipPos.y, -1.0, 1.0);
    gl_Position = position_in_world_space;
    EmitVertex();

    EndPrimitive();
}


shader GSVertices(in vec2 worldPos[])
{
    if (fullscreen)
    {
        position_in_world_space = vec4(worldPos[0] * 2.0 - vec2(1.0, 1.0), layer,
                1);
        gl_Position = position_in_world_space;
        EmitVertex();

        position_in_world_space = vec4(worldPos[1] * 2.0 - vec2(1.0, 1.0), layer,
                1);
        gl_Position = position_in_world_space;
        EmitVertex();
    }
    else
    {
        vec3 CameraRight = vec3(viewMatrix[0][0], viewMatrix[1][0],
                viewMatrix[2][0]);
        vec3 CameraUp;
        if (!uprightOrientation)
        {
            CameraUp = vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);
        }
        else
        {
            CameraUp = vec3(0, 0, 1.0);
        }
        vec3 CameraFront = vec3(viewMatrix[0][2], viewMatrix[1][2],
                viewMatrix[2][2]);
        float size =  36;

        position_in_world_space = vec4(worldPos[0], layer, 1);
        gl_Position = mvpMatrix
                * vec4(((position_in_world_space.x - 0.05)
                        * CameraRight * size + position_in_world_space.y
                        * CameraUp * size) + vec3(position.x, position.y, 0)
                       - CameraFront * layer, 1);
        EmitVertex();

        position_in_world_space = vec4(worldPos[1], layer, 1);
        gl_Position = mvpMatrix
                * vec4(((position_in_world_space.x - 0.05)
                        * CameraRight * size +  position_in_world_space.y
                        * CameraUp * size) + vec3(position.x, position.y, 0)
                       - CameraFront * layer, 1);
        EmitVertex();
    }
    EndPrimitive();
}


shader GSTubes(in vec2 worldPos[])
{
    if (fullscreen)
    {
        position_in_world_space = vec4(worldPos[0] * 2 - vec2(1.0, 1.0), layer,
                1);
        gl_Position = position_in_world_space;
        EmitVertex();

        position_in_world_space = vec4(worldPos[1] * 2 - vec2(1.0, 1.0), layer,
                1);
        gl_Position = position_in_world_space;
        EmitVertex();

        position_in_world_space = vec4(worldPos[2] * 2 - vec2(1.0, 1.0), layer,
                1);
        gl_Position = position_in_world_space;
        EmitVertex();
    }
    else
    {
        vec3 CameraRight = vec3(viewMatrix[0][0], viewMatrix[1][0],
                viewMatrix[2][0]);
        vec3 CameraUp;
        if (!uprightOrientation)
        {
            CameraUp = vec3(viewMatrix[0][1], viewMatrix[1][1],
                    viewMatrix[2][1]);
        }
        else
        {
            CameraUp = vec3(0, 0, 1.0);
        }
        vec3 CameraFront = vec3(viewMatrix[0][2], viewMatrix[1][2],
                viewMatrix[2][2]);
        float size =  36;

        position_in_world_space = vec4(worldPos[0], layer, 1);
        gl_Position = mvpMatrix
                * vec4(((position_in_world_space.x - 0.05)
                        * CameraRight * size +  position_in_world_space.y
                        * CameraUp * size) + vec3(position, 0)
                       - CameraFront * layer, 1);
        EmitVertex();

        position_in_world_space = vec4(worldPos[1], layer, 1);
        gl_Position = mvpMatrix
                * vec4(((position_in_world_space.x - 0.05)
                        * CameraRight * size +  position_in_world_space.y
                        * CameraUp * size) + vec3(position, 0)
                       - CameraFront * layer, 1);
        EmitVertex();

        position_in_world_space = vec4(worldPos[2], layer, 1);
        gl_Position = mvpMatrix
                * vec4(((position_in_world_space.x - 0.05)
                        * CameraRight * size +  position_in_world_space.y
                        * CameraUp * size) + vec3(position, 0)
                       - CameraFront * layer, 1);
        EmitVertex();
    }
    EndPrimitive();
}


shader GSBackground(in vec4 worldPos[])
{
    if (fullscreen)
    {
        gl_Position = vec4(-1.0, -1.0, 0, 1);
        EmitVertex();

        gl_Position = vec4(-1.0, 1.0, 0, 1);
        EmitVertex();

        gl_Position = vec4(1.0, -1.0, 0, 1);
        EmitVertex();

        gl_Position = vec4(1.0, 1.0, 0, 1);
        EmitVertex();

        EndPrimitive();
    }
    else
    {
        vec3 CameraRight = vec3(viewMatrix[0][0], viewMatrix[1][0],
                viewMatrix[2][0]);
        vec3 CameraUp;
        if (!uprightOrientation)
        {
            CameraUp = vec3(viewMatrix[0][1], viewMatrix[1][1],
                    viewMatrix[2][1]);
        }
        else
        {
            CameraUp = vec3(0, 0, 1.0);
        }
        vec3 CameraFront = vec3(viewMatrix[0][2], viewMatrix[1][2],
                viewMatrix[2][2]);

        float size =  36;

        gl_Position = mvpMatrix
                * vec4((CameraRight * size * (area.right - 0.05)
                        + CameraUp * size * area.bottom)
                       + vec3(position.x, position.y, 0.)
                       - CameraFront * layer, 1.);
        EmitVertex();

        gl_Position = mvpMatrix
                * vec4((CameraRight * size * (area.right - 0.05)
                        + CameraUp * size  * area.top)
                       + vec3(position.x, position.y, 0.)
                       - CameraFront * layer, 1.);
        EmitVertex();

        gl_Position = mvpMatrix * vec4((CameraUp * size * area.bottom)
                                       + vec3(position.x, position.y, 0.), 1.);
        EmitVertex();

        gl_Position = mvpMatrix * vec4((CameraUp * size * area.top)
                                       + vec3(position.x, position.y, 0.), 1.);
        EmitVertex();

        EndPrimitive();
    }
}


shader GSMeasurementPoint(in vec4 worldPos[])
{
    // getting camera up vector
    vec3 CameraUp;
    if (!uprightOrientation)
    {
        CameraUp = vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);
    }
    else
    {
        CameraUp = vec3(0., 0., 1.);
    }
    float height = ((log(topPressure) - pToWorldZParams.x) * pToWorldZParams.y);

    gl_Position = mvpMatrix * vec4(worldPos[0].xyz, 1.0);
    EmitVertex();

    gl_Position = mvpMatrix
            * vec4(worldPos[0].xyz + vec3(0., 0., CameraUp * height), 1.0);
    EmitVertex();

    EndPrimitive();
}


shader GSLegendBackground(in vec4 worldPos[])
{
    gl_Position = vec4(-0.85, 0.85, 0., 1.);
    EmitVertex();

    gl_Position = vec4(-0.4, 0.85, 0., 1.);
    EmitVertex();

    gl_Position = vec4(-0.85, 0.45, 0., 1.);
    EmitVertex();

    gl_Position = vec4(-0.4, 0.45, 0., 1.);
    EmitVertex();

}


/*****************************************************************************
 ***                           FRAGMENT SHADER
 *****************************************************************************/

uniform sampler2D backbufferTexture;

shader FSDiagramFrame(in vec2 texCoord, out vec4 fragColour)
{
   fragColour = texture(backbufferTexture, texCoord);
}


shader FSColour(in vec4 worldPos, out vec4 fragColour)
{
    fragColour = colour;
}


shader FSVertOrHoriCheck(in vec4 worldPos, out vec4 fragColour)
{
    if (pressureEqualsWorldPressure)
    {
        if (isInTopBottomBounds(position_in_world_space))
        {
            fragColour = colour;
        }
        else
        {
            discard;
        }
    }
    else
    {
        if (isInLeftRightBounds(position_in_world_space))
        {
            fragColour = colour;
        }
        else
        {
            discard;
        }
    }
}


shader FSColourWithAreaTest(in vec4 worldPos, out vec4 fragColour)
{
    if (isInBounds(position_in_world_space))
    {
        fragColour = colour;
    }
    else
    {
        discard;
    }
}


shader FSColourWhite(in vec4 woTemperaturerldPos, out vec4 fragColour)
{
   fragColour = vec4(1.0, 1.0, 1.0, 1.0);
}


shader FSMeasurementPointColour(in vec4 worldPos, out vec4 fragColour)
{
   fragColour = vec4(0.0, 0.0, 0.0, 1.0);
}


shader FSVariables(out vec4 fragColour)
{
    // Scale the scalar range to 0..1.
    if (isInBounds(position_in_world_space))
    {
        if (useTransferFunction && (ensemble >= 0)
                && (scalarMaximum - scalarMinimum > 0))
        {
            float colourTransferFunction = (ensemble - scalarMinimum)
                    / (scalarMaximum - scalarMinimum);
            // Fetch colour from the transfer function and apply shading term.
            fragColour = texture(transferFunction, colourTransferFunction);
        }
        else
        {
            fragColour = colour;
        }
    }
    else
    {
        discard;
    }
}


shader FSMarkingCircles(out vec4 fragColour)
{
    if (drawHumidity)
    {
        fragColour = humidityColour;
    }
    if (drawTemperature)
    {
        fragColour = temperatureColour;
    }

}


shader FSTubes(in vec4 actColour, out vec4 fragColour)
{
    if (isInBounds(position_in_world_space))
    {
        fragColour = actColour;
    }
    else
    {
        discard;
    }
}


// ============================================================================
shader VSEmpty()
{
}

shader FSEmpty()
{
    discard;
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program DiagramBackground
{
    vs(400)=VSWorld();
    gs(400)=GSBackground() : in(points), out(triangle_strip, max_vertices = 4);
    fs(400)=FSColourWhite();
};

program DiagramVerticesWithoutAreaCheck
{
    vs(400)=VS2DVertexYInPressure();
    gs(400)=GSVertices() : in(lines), out(line_strip, max_vertices = 2);
    fs(400)=FSColour();
};

program DiagramVerticesVertOrHoriCheck
{
    vs(400)=VS2DVertexYInPressure();
    gs(400)=GSVertices() : in(lines), out(line_strip, max_vertices = 2);
    fs(400)=FSVertOrHoriCheck();
};

program DiagramVertices
{
    vs(400)=VS2DVertexYInPressure();
    gs(400)=GSVertices() : in(lines), out(line_strip, max_vertices = 2);
    fs(400)=FSColourWithAreaTest();
};

program MeasurementPoint
{
    vs(400)=VSWorld();
    gs(400)=GSMeasurementPoint() : in(points), out(line_strip,
                                                   max_vertices = 2);
    fs(400)=FSMeasurementPointColour();
};

program DiagramVariables
{
    vs(400)=VSVariables();
    gs(400)=GSVertices() : in(lines), out(line_strip, max_vertices = 2);
    fs(400)=FSVariables();
};

program WyomingTestData
{
    vs(400)=VSWyoming();
    gs(400)=GSVertices() : in(lines), out(line_strip, max_vertices = 2);
    fs(400)=FSVariables();
};

program MouseOverCross
{
    vs(400)=VS2D();
    gs(400)=GSMouseOverCross() : in(lines), out(line_strip, max_vertices = 4);
    fs(400)=FSColourWithAreaTest();
};

program MarkingCircles
{
    vs(400)=VSMarkingCircles();
    gs(400)=GSMarkingCircles() : in(points), out(triangle_strip,
                                                 max_vertices = 6);
    fs(400)=FSMarkingCircles();
};

program DiagramTubes
{
    vs(400)=VSTubes();
    gs(400)=GSTubes() : in(triangles), out(triangle_strip, max_vertices = 3);
    fs(400)=FSVariables();
};

program DiagramDeviation
{
    vs(400)=VSDeviation();
    gs(400)=GSTubes() : in(triangles), out(triangle_strip, max_vertices = 3);
    fs(400)=FSVariables();
};

program LegendBackground
{
    vs(400)=VSWorld();
    gs(400)=GSLegendBackground() : in(points), out(triangle_strip,
                                                   max_vertices = 4);
    fs(400)=FSColourWhite();
};
