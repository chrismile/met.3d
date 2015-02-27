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
 ***                             CONSTANTS
 *****************************************************************************/

const float MISSING_VALUE = -999.E9;

// Vertical level type; see structuredgrid.h.
const int SURFACE_2D = 0;
const int PRESSURE_LEVELS_3D = 1;
const int HYBRID_SIGMA_PRESSURE_3D = 2;
const int POTENTIAL_VORTICITY_2D = 3;
const int LOG_PRESSURE_LEVELS_3D = 4;
const int AUXILIARY_PRESSURE_3D = 5;


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

interface VStoFS
{
    smooth float  scalar;  // the scalar data that this vertex holds,
                           // the value shall be perspectively correct
                           // interpolated to the fragment shader
//TODO: is there a more elegant way to do this?
    smooth float  flag;    // if this flag is set to < 0, the fragment
                           // shader should discard the fragment
    smooth float  worldZ;  // worldZ coordinate to test for
                           // vertical section bounds in the frag shader
};


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

uniform int       levelType;         // vertical level type of the data grid
uniform sampler1D lonLatLevAxes;     // 1D texture that holds both lon and lat
uniform int       latOffset;         // index at which lat data starts
uniform sampler3D dataField;         // 3D texture holding the scalar data
uniform sampler2D surfacePressure;   // surface pressure field in Pa
uniform sampler1D hybridCoefficients;// hybrid sigma pressure coefficients
uniform sampler1D path;              // points along the vertical section path
uniform vec2      pToWorldZParams;   // parameters to convert p[hPa] to world z
uniform mat4      mvpMatrix;         // model-view-projection

layout(rg32f)
uniform image2D   targetGrid;
uniform bool      fetchFromTarget;   // do not compute the scalar from the data
                                     // field; fetch it from the target grid
                                     // instead


shader VSmain(out VStoFS Output)
{
    // m = index of point in "path" (horizontal dimension); k = index of model
    // level (vertical dimension).
    int m = bool(gl_VertexID & 1) ? (gl_InstanceID + 1) : gl_InstanceID;
    int k = int(gl_VertexID / 2);

    // Determine the position of this vertex in world space: Fetch lat/lon
    // values and model grid indices i/j of the closest model grid point from
    // texture. The model grid indices are required to compute the pressure of
    // this vertex. The mixI/mixJ indices that are stored in the "path" texture
    // are actually floats that describe the precise location of the point at
    // (lon/lat) in grid index space. That is, if mixI = 65.5, it would be
    // half-way between the grid cells at index 65 and 66. Using int(mixI), the
    // integer indices can be recovered, using fract(mixI), the interpolation
    // value for the mix() function can be recovered.
    float lon  = texelFetch(path, 4*m    , 0).a;
    float lat  = texelFetch(path, 4*m + 1, 0).a;
    float mixI = texelFetch(path, 4*m + 2, 0).a;
    float mixJ = texelFetch(path, 4*m + 3, 0).a;

    // If mixI < 0, the point is outside the model domain. No grid needs to
    // be constructed, no data fetched from any texture. Flag the vertex as
    // outside the domain, so the fragment shader can discard it.
    if (mixI < 0)
    {
        gl_Position = mvpMatrix * vec4(lon, lat, 0, 1);
        Output.scalar      = 0.;
        Output.flag        = -100.;
        return;
    }

    if (fetchFromTarget)
    {
        // The vertical section stored in the texture "targetGrid" can be
        // reused -- no need to perform the more expensive computations to
        // interpolate from the original 3D data field.
        // NOTE: imageLoad requires "targetGrid" to be defined with the
        // layout qualifier, otherwise an error "..no overload function can
        // be found: imageLoad(struct image2D, ivec2).." is raised.
        vec4 data = imageLoad(targetGrid, ivec2(m,k));
        // The scalar value is stored in the "R" channel, ..
        Output.scalar = data.r;
        // .. the log(p) coordinate in the "G" channel.
        Output.worldZ = (data.g - pToWorldZParams.x) * pToWorldZParams.y;
        gl_Position = mvpMatrix * vec4(lon, lat, Output.worldZ, 1);
        Output.flag = 0.;
        return;
    }

    // Determine the indices to fetch surface pressure and data values;
    // interpolation coefficients (mixI and mixJ), and take care of cyclic
    // grids ((i+1) mod numLons; notes 16Apr2012).
    int i = int(mixI);
    int j = int(mixJ);
    mixI  = fract(mixI);
    mixJ  = fract(mixJ);

    int numLons = textureSize(dataField, 0).x;
    int i1      = (i+1) % numLons;
    int j1      = j+1;

    float p_hPa = 1050.;
    if (levelType == HYBRID_SIGMA_PRESSURE_3D)
    {   
        // The hybridCoefficients texture contains both the ak and bk coefficients,
        // hence the number of levels is half the size of this array.
        int numberOfLevels = textureSize(hybridCoefficients, 0) / 2;
        float ak = texelFetch(hybridCoefficients, k, 0).a;
        float bk = texelFetch(hybridCoefficients, k+numberOfLevels, 0).a;

        // Compute the pressure at the four grid points surrounding (lon/lat).
        float p_i0j0 =
                ak + bk * texelFetch(surfacePressure, ivec2(i , j ), 0).a / 100.;
        float p_i1j0 =
                ak + bk * texelFetch(surfacePressure, ivec2(i1, j ), 0).a / 100.;
        float p_i0j1 =
                ak + bk * texelFetch(surfacePressure, ivec2(i , j1), 0).a / 100.;
        float p_i1j1 =
                ak + bk * texelFetch(surfacePressure, ivec2(i1, j1), 0).a / 100.;

        // Interpolate to get the pressure at (lon/lat).
        float p_j0  = mix(p_i0j0, p_i1j0, mixI);
        float p_j1  = mix(p_i0j1, p_i1j1, mixI);
        p_hPa = mix(p_j0  , p_j1  , mixJ);
    }
    else if (levelType == PRESSURE_LEVELS_3D)
    {
        int numLats = textureSize(dataField, 0).y;
        int vertOffset = numLons + numLats;
        p_hPa = texelFetch(lonLatLevAxes, k + vertOffset, 0).a;
    }
    
    // Convert pressure to world Z coordinate to get the vertex positon.
    float log_p = log(p_hPa);
    Output.worldZ = (log_p - pToWorldZParams.x) * pToWorldZParams.y;
    vec3 vertexPosition = vec3(lon, lat, Output.worldZ);

    // Convert the position from world to clip space.
    gl_Position = mvpMatrix * vec4(vertexPosition, 1);

    // Fetch the scalar values at the four surrounding grid points and
    // interpolate to (lon/lat), similar to the pressure.
    float scalar_i0j0 = texelFetch(dataField, ivec3(i , j , k), 0).a;
    float scalar_i1j0 = texelFetch(dataField, ivec3(i1, j , k), 0).a;
    float scalar_i0j1 = texelFetch(dataField, ivec3(i , j1, k), 0).a;
    float scalar_i1j1 = texelFetch(dataField, ivec3(i1, j1, k), 0).a;

    float scalar_j0 = mix(scalar_i0j0, scalar_i1j0, mixI);
    float scalar_j1 = mix(scalar_i0j1, scalar_i1j1, mixI);
    Output.scalar   = mix(scalar_j0, scalar_j1, mixJ);

    // Store the computed vertex (= grid point) scalar value and worldZ
    // coordinate into the texture "targetGrid" for further processing (e.g.
    // contouring, CPU read-back).
    imageStore(targetGrid, ivec2(m, k), vec4(Output.scalar, log_p, 0, 0));

    // If we have arrived here, the vertex is inside the domain. Set flag to 0,
    // so that all fragments resulting from this vertex are kept by the fragment
    // shader.
    Output.flag = 0.;
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler1D transferFunction; // 1D transfer function
uniform float     scalarMinimum;    // min/max data values to scale to 0..1
uniform float     scalarMaximum;
uniform vec2      verticalBounds;   // lower and upper bound of this section. If
                                    // worldZ is outside this interval discard
                                    // the fragment.
uniform float     opacity;          // Multiply the colour obtained from the
                                    // transfer function with this alpha value


shader FSmain(in VStoFS input, out vec4 fragColour)
{
    // Discard the element if it is outside the model domain (no scalar value).
    if (input.flag < 0.) discard;

    // Also discard if the fragment is outside the vertical bounds of the section.
    if ((input.worldZ < verticalBounds.x) || (input.worldZ > verticalBounds.y)) discard;

    // Scale the scalar range to 0..1.
    float scalar_ = (input.scalar - scalarMinimum) / (scalarMaximum - scalarMinimum);

    // Fetch colour from the transfer function and apply shading term.
    fragColour = texture(transferFunction, scalar_);
    fragColour = vec4(fragColour.rgb, fragColour.a * opacity);
}


shader FSdoNotRender(in VStoFS input, out vec4 fragColour)
{    
    // empty
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(420)=VSmain();
    fs(420)=FSmain();
};


program OnlyUpdateTargetGrid
{
    vs(420)=VSmain();
    fs(420)=FSdoNotRender();
};
