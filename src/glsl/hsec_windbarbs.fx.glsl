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

// Set PIVOT_TIP to draw wind barbs pivoted around their tip; if unset their
// middle of the barb is used as pivot.
//TODO (mr, 03Nov2014): make this a property in the HSecActor.
#define PIVOT_TIP

/*****************************************************************************
 ***                             CONSTANTS
 *****************************************************************************/

const float MS_TO_KNOTS = 1.9438444924574;

const float MISSING_VALUE = -999.E9;

const int SURFACE_2D = 0;
const int PRESSURE_LEVELS_3D =  1;
const int HYBRID_SIGMA_PRESSURE_3D =  2;
const int POTENTIAL_VORTICITY_2D =  3;
const int LOG_PRESSURE_LEVELS_3D =  4;
const int AUXILIARY_PRESSURE_3D =  5;


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

interface GStoFS
{
    smooth vec4 color;
};


/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform mat4        mvpMatrix;
uniform int         width;
uniform vec2        bboxll;     // lower left position of bounding box
uniform float       deltaGridX; // delta between two points in x
uniform float       deltaGridY; // delta between two points in y
uniform float       worldZ;     // world z coordinate of this section

// sample textures for u/v component of wind
uniform sampler3D   dataUComp;
uniform sampler3D   dataVComp;

uniform sampler2D   surfacePressureU;    // surface pressure field in Pa
uniform sampler1D   hybridCoefficientsU; // hybrid sigma pressure coefficients
uniform sampler2D   surfacePressureV;    // surface pressure field in Pa
uniform sampler1D   hybridCoefficientsV; // hybrid sigma pressure coefficients

uniform float       deltaLatLon;

uniform vec2        dataSECrnr;
uniform vec2        dataNWCrnr;

uniform vec2        pToWorldZParams;

uniform vec3        cameraPosition;

uniform float       pressure_hPa; // current pressure of hsec

uniform int         levelType;       // vertical level type of the data grid
uniform sampler1D   latLonAxesData;  // 1D texture that holds both lon and lat
uniform int         latOffset;       // index at which lat axis data starts
uniform int         verticalOffset;  // index at which vertical axis data starts

// adjustable glyph parameters
uniform float       lineWidth;
uniform vec4        glyphColor;
uniform bool        showCalmGlyph;
uniform int         numFlags;


/*****************************************************************************
 ***                             INCLUDES
 *****************************************************************************/

float computeKnots(in float velMeterPerSecond)
{
    return velMeterPerSecond * MS_TO_KNOTS;
}


// compute indices (i,j) and fractions wrt hybrid sigma pressure volume data
void computeIndices(in vec2 pos, out int i, out int j)
{
    float Lon = mod(pos.x - dataNWCrnr.x, 360.) / deltaLatLon;
    float Lat = (dataNWCrnr.y - pos.y) / deltaLatLon;

    i = int(Lon);
    j = int(Lat);
}


//TODO (mr, 03Nov2014): move all HSec sampling methods to common include.
// sample any kind of data at given world position
float sampleDataAtPos(  in vec3 pos,
                        in sampler3D volume,
                        in sampler2D surfacePressure,
                        in sampler1D hybridCoefficients)
{
    // Variables for binary level search and interpolation.

    int i,j;

    computeIndices(pos.xy, i, j);

    ivec3 dims = textureSize(volume, 0);
    i = max(0, min(i, dims.x - 1));
    j = max(0, min(j, dims.y - 1));

    int numberOfLevels = 0;
    int klower = 0;
    int kupper = 0;

    float ln_plower = 0.;
    float ln_pupper = 0.;
    float ln_p = log(pressure_hPa);


    // Supported grid type 1: HYBRID MODEL LEVELS
    // ==========================================
    if (levelType == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Now we need to determine the indices of the two model levels that
        // enclose the requestred iso-pressure value "pressure_hPa". A binary
        // search is performed, similar to the "locate" function in the
        // Numerical Recipes, 3rd ed., p.115: Two indices klower and kupper at
        // first point to the first and last available model levels, depending
        // on the value of the element midway between both, the interval is cut
        // in half until it consists of two adjacent levels.

        // The hybridCoefficients texture contains both the ak and bk
        // coefficients, hence the number of levels is half the size of this
        // array.
        numberOfLevels = textureSize(hybridCoefficients, 0) / 2;

        // Initial position of klower and kupper.
        klower = 0;
        kupper = numberOfLevels - 1;

        // Fetch the surface pressure at (i, j). It is used to compute the
        // pressure at the model levels via p = ak + bk * psfc.
        float psfc_hPa = texelFetch(surfacePressure, ivec2(i, j), 0).a / 100.;

        // Perform the binary search.
        while ((kupper-klower) > 1)
        {
            // Element midway between klower and kupper.
            int kmid  = (kupper+klower) / 2;
            // Compute pressure at kmid.
            float ak = texelFetch(hybridCoefficients, kmid, 0).a;
            float bk = texelFetch(hybridCoefficients, kmid+numberOfLevels, 0).a;
            float pressureAt_kmid_hPa = ak + bk * psfc_hPa;
            // Cut interval in half.
            if (pressure_hPa >= pressureAt_kmid_hPa)
                klower = kmid;
            else
                kupper = kmid;
        }

        // The model level pressures at klower and kupper now enclose the
        // requested value pressure_hPa. Next, we fetch the scalar value at
        // these two levels and interpolate lineraly int ln(p) between the two
        // to get the scalar value at the vertex position.

        // Fetch coefficients at klower and kupper.
        float aklower = texelFetch(hybridCoefficients, klower, 0).a;
        float bklower = texelFetch(hybridCoefficients, klower+numberOfLevels, 0).a;
        float akupper = texelFetch(hybridCoefficients, kupper, 0).a;
        float bkupper = texelFetch(hybridCoefficients, kupper+numberOfLevels, 0).a;

        // Compute the log of the pressure values.
        ln_plower = log(aklower + bklower * psfc_hPa);
        ln_pupper = log(akupper + bkupper * psfc_hPa);
        //ln_p      = log(pressure_hPa);

        // Alternative: Interpolate linerarly in p.
        //    float ln_plower = (aklower + bklower * psfc_hPa);
        //    float ln_pupper = (akupper + bkupper * psfc_hPa);
        //    float ln_p      = (pressure_hPa);
    } // levelType == HYBRID_SIGMA_PRESSURE_3D


    // Supported grid type 2: PRESSURE LEVELS
    // ======================================
    else if (levelType == PRESSURE_LEVELS_3D)
    {
        numberOfLevels = textureSize(latLonAxesData, 0) - verticalOffset;
        klower = 0;
        kupper = numberOfLevels - 1;

        while ((kupper-klower) > 1)
        {
            // Element midway between klower and kupper.
            int kmid = (kupper+klower) / 2;
            // Get pressure at kmid.
            float pressureAt_kmid_hPa =
                texelFetch(latLonAxesData, verticalOffset+kmid, 0).a;
            // Cut interval in half.
            if (pressure_hPa >= pressureAt_kmid_hPa)
                klower = kmid;
            else
                kupper = kmid;
        }

        ln_plower = log(texelFetch(latLonAxesData, verticalOffset+klower, 0).a);
        ln_pupper = log(texelFetch(latLonAxesData, verticalOffset+kupper, 0).a);
    }


    // Supported grid type 3: LOG(PRESSURE) LEVELS
    // ===========================================
    else if (levelType == LOG_PRESSURE_LEVELS_3D)
    {
        numberOfLevels = textureSize(latLonAxesData, 0) - verticalOffset;
        klower = 0;
        kupper = numberOfLevels - 1;

        while ((kupper-klower) > 1)
        {
            // Element midway between klower and kupper.
            int kmid = (kupper+klower) / 2;
            // Get pressure at kmid.
            float ln_pressureAt_kmid =
                texelFetch(latLonAxesData, verticalOffset+kmid, 0).a;
            // Cut interval in half.
            if (ln_p >= ln_pressureAt_kmid)
                klower = kmid;
            else
                kupper = kmid;
        }

        ln_plower = texelFetch(latLonAxesData, verticalOffset+klower, 0).a;
        ln_pupper = texelFetch(latLonAxesData, verticalOffset+kupper, 0).a;
    }


    // Interpolated scalar value.
    float scalar;

    if (ln_pupper < ln_p)
    {
        // The requested iso-pressure value is below the lowest pressure-value
        // in the model grid (i.e., it is below the surface). No valid scalar
        // value can be computed, set the "flag" so the fragment shader can
        // discard all fragments that touch this vertex.
        scalar = MISSING_VALUE;
    }
    else
    {
        // Fetch the scalar values at klower and kupper.
        float scalar_klower = texelFetch(volume, ivec3(i, j, klower), 0).a;
        float scalar_kupper = texelFetch(volume, ivec3(i, j, kupper), 0).a;

        if ((scalar_klower == MISSING_VALUE) || (scalar_kupper == MISSING_VALUE))
        {
            scalar = MISSING_VALUE;
        }
        else
        {
            // Linearly interpolate in ln(p) between the scalar values at level
            // klower and level kupper.
            float a = (ln_p - ln_plower) / (ln_pupper - ln_plower);
            scalar = mix(scalar_klower, scalar_kupper, a);
        }
    }

    return scalar;
}


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec2 worldXY, out vec3 worldPos)
{
    worldPos.x = worldXY.x;
    worldPos.y = worldXY.y;
    worldPos.z = worldZ;
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

// filled triangle
void createPennantTri(  in vec3 dir,
                        in vec3 normal,
                        in vec3 pos,
                        in float adaptedLineWidth,
                        in float largeWidth,
                        in float peakOffset,
                        in float offset,
                        in float posOffset)
{
    vec3 pos0, pos1, pos2;

    //float scaleFactor = adaptedLineWidth / lineWidth;
    pos += normal * adaptedLineWidth / 2.0;

    pos0 = pos - posOffset / 2.0 * dir;
    pos1 = pos0 - dir * posOffset / 2.0 + dir * peakOffset + normal * largeWidth;
    pos2 = pos0 - dir * posOffset / 2.0 + dir * (offset + peakOffset);

    gl_Position = mvpMatrix * vec4(pos0, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos1, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos2, 1);
    EmitVertex();
    EndPrimitive();
}


// two circles with 128 vertices
void createCalmWindTri( in vec3 pos,
                        in float adaptedLineWidth,
                        in float smallRadius,
                        in float largeRadius)
{
    const int tubeSegments = 16;
    const float angleStep = 360.0 / float(tubeSegments);
    const vec3 normal = vec3(1,0,0);
    const vec3 binormal = vec3(0,1,0);

    vec3 prevPos0, prevPos1;
    vec3 currPos0, currPos1;
    for (int i = 0; i <= tubeSegments; ++i)
    {
        float angle = radians(angleStep * i);
        float cosi = cos(angle) * smallRadius;
        float sini = sin(angle) * smallRadius;
        float sinig = sin(angle) * (smallRadius - adaptedLineWidth);
        float cosig = cos(angle) * (smallRadius - adaptedLineWidth);

        if (i == 0)
        {
            prevPos0 = pos + normal * cosi + binormal * sini;
            prevPos1 = pos + normal * cosig + binormal * sinig;
            continue;
        }

        currPos0 = pos + normal * cosi + binormal * sini;
        currPos1 = pos + normal * cosig + binormal * sinig;

        gl_Position = mvpMatrix * vec4(prevPos0, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(currPos0, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(prevPos1, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(currPos1, 1);
        EmitVertex();
        EndPrimitive();

        prevPos0 = currPos0;
        prevPos1 = currPos1;
    }

    for (int i = 0; i <= tubeSegments; ++i)
    {
        float angle = radians(angleStep * i);
        float cosi = cos(angle) * largeRadius;
        float sini = sin(angle) * largeRadius;
        float sinig = sin(angle) * (largeRadius - adaptedLineWidth);
        float cosig = cos(angle) * (largeRadius - adaptedLineWidth);

        if (i == 0)
        {
            prevPos0 = pos + normal * cosi + binormal * sini;
            prevPos1 = pos + normal * cosig + binormal * sinig;
            continue;
        }

        currPos0 = pos + normal * cosi + binormal * sini;
        currPos1 = pos + normal * cosig + binormal * sinig;

        gl_Position = mvpMatrix * vec4(prevPos0, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(currPos0, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(prevPos1, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(currPos1, 1);
        EmitVertex();
        EndPrimitive();

        prevPos0 = currPos0;
        prevPos1 = currPos1;
    }
}


void createHalfFlagTri( in vec3 dir,
                        in vec3 normal,
                        in vec3 pos,
                        in float adaptedLineWidth,
                        in float smallWidth,
                        in float offset,
                        in float posOffset)
{
    vec3 pos0, pos1, pos2, pos3;

    pos += normal * adaptedLineWidth / 2.0;

    // left bottom
    pos0 = pos - dir * adaptedLineWidth / 2.0;
    // left top
    pos1 = pos0 - dir * offset + normal * smallWidth;
    // right bottom
    pos2 = pos + dir * adaptedLineWidth / 2.0;
    // right top
    pos3 = pos2 - dir * offset + normal * smallWidth;

    gl_Position = mvpMatrix * vec4(pos0, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos1, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos2, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos3, 1);
    EmitVertex();
    EndPrimitive();
}


void createFullFlagTri( in vec3 dir,
                        in vec3 normal,
                        in vec3 pos,
                        in float adaptedLineWidth,
                        in float largeWidth,
                        in float offset,
                        in float posOffset)
{
    vec3 pos0, pos1, pos2, pos3;

    pos += normal * adaptedLineWidth / 2.0;

    // left bottom
    pos0 = pos - dir * adaptedLineWidth / 2.0;
    // left top
    pos1 = pos0 - dir * offset + normal * largeWidth;
    // right bottom
    pos2 = pos + dir * adaptedLineWidth / 2.0;
    // right top
    pos3 = pos2 - dir * offset + normal * largeWidth;

    gl_Position = mvpMatrix * vec4(pos0, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos1, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos2, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos3, 1);
    EmitVertex();
    EndPrimitive();
}


// describes a rectangle bar
void createBaseTri( in vec3 dir,
                    in vec3 normal,
                    in vec3 pos,
                    in float adaptedLineWidth,
                    in float barbLength)
{
    vec3 pos0, pos1, pos2, pos3;

#ifdef PIVOT_TIP
    // left bottom
    pos0 = pos - dir * barbLength - normal * adaptedLineWidth / 2.0;
    // left top
    pos1 = pos                    - normal * adaptedLineWidth / 2.0;
    // right bottom
    pos2 = pos - dir * barbLength + normal * adaptedLineWidth / 2.0;
    // right top
    pos3 = pos                    + normal * adaptedLineWidth / 2.0;
#else
    // left bottom
    pos0 = pos - dir * barbLength / 2.0 - normal * adaptedLineWidth / 2.0;
    // left top
    pos1 = pos + dir * barbLength / 2.0 - normal * adaptedLineWidth / 2.0;
    // right bottom
    pos2 = pos - dir * barbLength / 2.0 + normal * adaptedLineWidth / 2.0;
    // right top
    pos3 = pos + dir * barbLength / 2.0 + normal * adaptedLineWidth / 2.0;
#endif

    gl_Position = mvpMatrix * vec4(pos0, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos1, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos2, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos3, 1);
    EmitVertex();
    EndPrimitive();

}


shader GSmain(in vec3 worldPos[], out GStoFS Output)
{
    Output.color = glyphColor;

    // current world position
    const vec3 pos = worldPos[0];

    // sample both wind textures, obtain wind speed in u and v direction
    float windU = sampleDataAtPos(pos, dataUComp, surfacePressureU, hybridCoefficientsU);
    float windV = sampleDataAtPos(pos, dataVComp, surfacePressureV, hybridCoefficientsV);

    // if no data is available, do not draw any glyphs
    if (windU == MISSING_VALUE || windV == MISSING_VALUE)
    {
        return;
    }

    // wind direction
    vec3 windDir = vec3(windU, windV, 0);

    // normalized wind direction
    const vec3 dir = normalize(windDir);
    const vec3 normal = normalize( vec3( -dir.y, dir.x, 0));

    // wind velocity in m/s
    const float velocity = length(windDir);
    // wind velocity in kt
    float knots = computeKnots(velocity);

    // distance from camera to glyph position
    //float camDist = cameraPosition.z - pos.z;
    // clamp distance factor from 0 to 2
    //camDist = clamp(camDist / 50.0 + 1, 0.25, 10.0);
    // compute scale factor [1; 3.5]
    //const float scaleDistFactor = camDist * 0.6;

    // compute useful glyph parameters
    const float deltaGrid = (deltaGridX + deltaGridY) / 2.0f;

    // adapt line width of glyph elements
    const float adaptedLineWidth = deltaGrid * lineWidth;
    // compute barb length
    const float barbLength = 0.75 * deltaGrid;
    // compute small radius of calm wind
    const float smallRadius = 0.15 * deltaGrid;
    // compute large radius of calm wind
    const float largeRadius = 0.3 * deltaGrid;

    // set posOffset
    const float posOffset = barbLength / numFlags;

    // set pennant triangle offset
    const float pennantPeakOffset = 0.3 * posOffset;
    // set offset of flags and pennant
    const float pennantOffset = 2 * 0.95 * posOffset;
    // set flag offset
    const float flagOffset = pennantOffset;

    // compute width of small flag
    const float smallWidth = 0.2 * 0.6 * deltaGrid;
    // compute width of large flag
    const float largeWidth = 0.2 * deltaGrid;

    // distance between elements
    //const float pennantDist = (offset  + pennantOffset);
    //const float flagDist = offset * 1    * scaleDistFactor;

    if (knots >= 5.0)
    {
        createBaseTri(dir, normal, pos,
                        adaptedLineWidth, barbLength);

#ifdef PIVOT_TIP                        
        vec3 elemPos = pos - dir * barbLength + dir * (posOffset / 2.0);
#else
        vec3 elemPos = pos - dir * (barbLength / 2.0 * 0.9) + dir * (posOffset / 2.0);
#endif

        for(int i = 0; i < floor(knots / 50); ++i)
        {
            createPennantTri(dir, normal, elemPos,
                            adaptedLineWidth, largeWidth, pennantPeakOffset, pennantOffset, posOffset);
            elemPos += 2 * posOffset * dir;
        }

        knots = mod(knots,50);

        //elemPos += flagDist * 0.001 * dir;

        for(int j = 0; j < floor(knots / 10); ++j)
        {
            createFullFlagTri(dir, normal, elemPos,
                                adaptedLineWidth, largeWidth, flagOffset, posOffset);
            elemPos += posOffset * dir;
        }

        knots = mod(knots,10);

        for(int k = 0; k < floor(knots / 5); ++k)
        {
            createHalfFlagTri(dir, normal, elemPos,
                                adaptedLineWidth, smallWidth, flagOffset * 0.6, posOffset);
            elemPos += posOffset * dir;
        }

        knots = mod(knots,5);
   }
   else
   {
        if (showCalmGlyph)
        {
            createCalmWindTri(pos, adaptedLineWidth, smallRadius, largeRadius);
        }
   }
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

shader FSmain(in GStoFS Input, out vec4 fragColor)
{
    fragColor = Input.color;
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Standard
{
    vs(420)=VSmain();
    gs(420)=GSmain() : in(points), out(triangle_strip, max_vertices = 128);
    fs(420)=FSmain();
};
