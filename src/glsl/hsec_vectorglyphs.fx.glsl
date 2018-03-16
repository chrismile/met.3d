/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2015      Michael Kern
**  Copyright 2017-2018 Bianca Tost
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
 ***                             STRUCTS
 *****************************************************************************/

struct VectorFieldData
{
    vec3 dir;               // normalised vector field direction
    vec3 normal;            // normal vector of vector field direction
    float magnitude;        // magnitude of vector field
    float deltaGrid;        // scaling factor
    float adaptedLineWidth; // line width adapted to scaling factor (delta grid)
    float glyphLength;      // adapted gylph length
} vectorField;


/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform mat4        mvpMatrix;
uniform float       deltaGridX; // delta between two points in x
uniform float       deltaGridY; // delta between two points in y
uniform float       worldZ;     // world z coordinate of this section

// Sample textures for longitudial/latitudial component of vector field.
uniform sampler3D   dataLonComp;
uniform sampler3D   dataLatComp;

uniform sampler2D   surfacePressureLon;    // surface pressure field in Pa
uniform sampler1D   hybridCoefficientsLon; // hybrid sigma pressure coefficients
uniform sampler2D   surfacePressureLat;    // surface pressure field in Pa
uniform sampler1D   hybridCoefficientsLat; // hybrid sigma pressure coefficients
uniform sampler3D   auxPressureFieldLon_hPa; // 3D pressure field in hPa
uniform sampler3D   auxPressureFieldLat_hPa; // 3D pressure field in hPa

uniform float       deltaLon;
uniform float       deltaLat;

uniform vec2        dataNWCrnr;

uniform float       pressure_hPa; // current pressure of hsec

uniform int         levelType;       // vertical level type of the data grid
uniform sampler1D   latLonAxesData;  // 1D texture that holds both lon and lat
uniform int         verticalOffset;  // index at which vertical axis data starts

// Adjustable glyph parameters.
uniform float       lineWidth;
uniform vec4        glyphColor;
uniform int         pivotPosition;

// Adjustable wind barbs parameters.
uniform bool        showCalmGlyph;
uniform int         numFlags;

// Adjustable arrows parameters.
uniform float       arrowHeadHeight;
uniform bool        arrowDrawOutline;
uniform float       arrowOutlineWidth;
uniform vec4        arrowOutlineColour;
uniform bool        scaleArrowWithMagnitude;
uniform float       scaleArrowMinScalar;
uniform float       scaleArrowMinLength;
uniform bool        scaleArrowDiscardBelow;
uniform float       scaleArrowMaxScalar;
uniform float       scaleArrowMaxLength;
uniform bool        scaleArrowDiscardAbove;


/*****************************************************************************
 ***                             INCLUDES
 *****************************************************************************/

float computeKnots(in float velMeterPerSecond)
{
    return velMeterPerSecond * MS_TO_KNOTS;
}


//TODO (mr, 03Nov2014): move all HSec sampling methods to common include.
// sample any kind of data at given world position
float sampleDataAtPos(  in vec3 pos,
                        in sampler3D volume,
                        in sampler2D surfacePressure,
                        in sampler1D hybridCoefficients,
                        in sampler3D auxPressureField_hPa)
{
    float mixI = mod(pos.x - dataNWCrnr.x, 360.) / deltaLon;
    float mixJ = (dataNWCrnr.y - pos.y) / deltaLat;
//NOTE (mr, 23May2017): This function currently implements nearest-neighbour
//                      interpolation in the horizontal!
    int i = int(round(mixI));
    int j = int(round(mixJ));

    ivec3 dims = textureSize(volume, 0);
    if (i < 0 || i >= dims.x) return MISSING_VALUE;
    if (j < 0 || j >= dims.y) return MISSING_VALUE;

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

    // Supported grid type 5: AUXILIARY PRESSURE 3D
    // ============================================
    else if (levelType == AUXILIARY_PRESSURE_3D)
    {
        // Now we need to determine the indices of the two model levels that
        // enclose the requested iso-pressure value "pressure_hPa". A binary
        // search is performed: Two indices klower and kupper at first point to
        // the first and last available model levels, depending on the value of
        // the element midway between both, the interval is cut in half until
        // it consists of two adjacent levels.

        // Initial position of klower and kupper.
        klower = 0;
        kupper = textureSize(auxPressureField_hPa, 0).z - 1;

        // Perform the binary search.
        while ((kupper - klower) > 1)
        {
            // Element midway between klower and kupper.
            int kmid  = (kupper + klower) / 2;
            // Look up pressure at (i, j, kmid). Since the pressure is stored in
            // Pa, we need to convert the received value to hPa.
            float pressureAt_kmid_hPa =
                    texelFetch(auxPressureField_hPa, ivec3(i, j, kmid), 0).a;
            // Cut interval in half.
            if (pressure_hPa >= pressureAt_kmid_hPa)
            {
                klower = kmid;
            }
            else
            {
                kupper = kmid;
            }
        }

        // The model level pressures at klower and kupper now enclose the
        // requested value pressure_hPa. Next, we fetch the scalar value at
        // these two levels and interpolate lineraly int ln(p) between the two
        // to get the scalar value at the vertex position.

        // Compute the log of the pressure values.
        ln_plower =
                log(texelFetch(auxPressureField_hPa, ivec3(i, j, klower), 0).a);
        ln_pupper =
                log(texelFetch(auxPressureField_hPa, ivec3(i, j, kupper), 0).a);
        //ln_p      = log(pressure_hPa);

        // Alternative: Interpolate linerarly in p.
        //    float ln_plower =
        //        log(texelFetch(auxPressureField_hPa, ivec3(i, j, klower), 0).a);
        //    float ln_pupper =
        //        log(texelFetch(auxPressureField_hPa, ivec3(i, j, kupper), 0).a);
        //    float ln_p      = (pressure_hPa);
    } // levelType == AUXILIARY_PRESSURE_3D


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


// Compute vector field specific data (vector field direction, magnitude,
// direction normal, etc.) at posWorld used to render both wind barbs and arrow
// glyphs.
bool computeVectorFieldData(in vec3 posWorld)
{
    // Sample both vector field component textures, obtain scalar value in
    // longitudial and latitudial direction.
    float lonField = sampleDataAtPos(
                posWorld, dataLonComp, surfacePressureLon, hybridCoefficientsLon,
                auxPressureFieldLon_hPa);
    float latField = sampleDataAtPos(
                posWorld, dataLatComp, surfacePressureLat, hybridCoefficientsLat,
                auxPressureFieldLat_hPa);

    // If no data is available, do not draw any glyphs.
    if (lonField == MISSING_VALUE || latField == MISSING_VALUE)
    {
        return false;
    }

    // Optain vector field direction at posWorld (e.g. wind).
    vec3 magnitudeDir = vec3(lonField, latField, 0);

    // Normalize direction vector.
    vectorField.dir = normalize(magnitudeDir);
    vectorField.normal = normalize(vec3(-vectorField.dir.y, vectorField.dir.x,
                                        0));

    // Get vector magnitude (e.g. wind speed).
    vectorField.magnitude = length(magnitudeDir);

    // distance from camera to glyph position
    //float camDist = cameraPosition.z - pos.z;
    // clamp distance factor from 0 to 2
    //camDist = clamp(camDist / 50.0 + 1, 0.25, 10.0);
    // compute scale factor [1; 3.5]
    //const float scaleDistFactor = camDist * 0.6;

    // Compute useful glyph parameters.
    vectorField.deltaGrid = (deltaGridX + deltaGridY) / 2.0f;

    // Adapt line width of glyph elements to (automatic) scaling.
    vectorField.adaptedLineWidth = vectorField.deltaGrid * lineWidth;
    // Compute glyph length.
    vectorField.glyphLength = 0.75 * vectorField.deltaGrid;

    return true;
}


// Determine the positions of the four vertices of the glyph's base line
// located at pos. The base line is a line in vector field direction at which
// the arrow head / wind barb pennants are attached. The positions are written
// to pos0, pos1, pos2, pos3.
void determineBaseLineVertices(in vec3 pos, out vec3 pos0, out vec3 pos1,
                               out vec3 pos2, out vec3 pos3)
{
    // Place pivot at glyph tip.
    if (pivotPosition == 0)
    {
        // Calculate position of left bottom vertex.
        pos0 = pos - vectorField.dir * vectorField.glyphLength
                - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of left top vertex.
        pos1 = pos - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right bottom vertex.
        pos2 = pos - vectorField.dir * vectorField.glyphLength
                + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right top vertex.
        pos3 = pos + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
    }
    // Place pivot at glyph centre.
    else if (pivotPosition == 1)
    {
        // Calculate position of left bottom vertex.
        pos0 = pos - vectorField.dir * vectorField.glyphLength / 2.0
                - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of left top vertex.
        pos1 = pos + vectorField.dir * vectorField.glyphLength / 2.0
                - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right bottom vertex.
        pos2 = pos - vectorField.dir * vectorField.glyphLength / 2.0
                + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right top vertex.
        pos3 = pos + vectorField.dir * vectorField.glyphLength / 2.0
                + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
    }
    // Place pivot at glyph tail.
    else if (pivotPosition == 2)
    {
        // Calculate position of left bottom vertex.
        pos0 = pos - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of left top vertex.
        pos1 = pos + vectorField.dir * vectorField.glyphLength
                - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right bottom vertex.
        pos2 = pos + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right top vertex.
        pos3 = pos + vectorField.dir * vectorField.glyphLength
                + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
    }
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


shader GSwindBarbs(in vec3 worldPos[], out GStoFS Output)
{
    Output.color = glyphColor;

    // Current world position.
    vec3 posWorld = worldPos[0];

    if (!computeVectorFieldData(posWorld))
    {
        return;
    }

    // Convert wind velocity in m/s into kt.
    float knots = computeKnots(vectorField.magnitude);

    // Compute small radius of calm wind.
    float smallRadius = 0.15 * vectorField.deltaGrid;
    // Compute large radius of calm wind.
    float largeRadius = 0.3 * vectorField.deltaGrid;

    // Set posOffset.
    float posOffset = vectorField.glyphLength / numFlags;

    // Set pennant triangle offset.
    float pennantPeakOffset = 0.3 * posOffset;
    // Set offset of flags and pennant.
    float pennantOffset = 2 * 0.95 * posOffset;
    // Set flag offset.
    float flagOffset = pennantOffset;

    // Compute width of small flag.
    float smallWidth = 0.2 * 0.6 * vectorField.deltaGrid;
    // Compute width of large flag.
    float largeWidth = 0.2 * vectorField.deltaGrid;

    // Compute distance between elements.
    //const float pennantDist = (offset  + pennantOffset);
    //const float flagDist = offset * 1    * scaleDistFactor;

    //! NOTE (Kern, 2016-11-03): Using EmitVertex() and EndPrimitve() in local
    // functions is not possible after NVidia driver 340 since the latest driver
    // do not allow to use stage-specific functions in all shaders. GLFX always
    // includes local functions into all shader stage programs (VS/FS) which
    // leads to compilation errors for all stages except the geometry shader.

// -----------------------------------------------------------------------------
// Create the base line of the wind barb glyph.

    if (knots >= 5.0)
    {
        vec3 pos0, pos1, pos2, pos3;
        vec3 pos = posWorld;

        determineBaseLineVertices(pos, pos0, pos1, pos2, pos3);

        gl_Position = mvpMatrix * vec4(pos0, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(pos1, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(pos2, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(pos3, 1);
        EmitVertex();
        EndPrimitive();

        vec3 elemPos;
        // Place pivot at glyph tip.
        if (pivotPosition == 0)
        {
            elemPos = posWorld - vectorField.dir * vectorField.glyphLength
                    + vectorField.dir * (posOffset / 2.0);
        }
        // Place pivot at glyph centre.
        else if (pivotPosition == 1)
        {
            elemPos = posWorld
                    - vectorField.dir * (vectorField.glyphLength / 2.0 * 0.9)
                    + vectorField.dir * (posOffset / 2.0);
        }
        // Place pivot at glyph tail.
        else if (pivotPosition == 2)
        {
            elemPos = posWorld + vectorField.dir * (posOffset / 2.0);
        }

// -----------------------------------------------------------------------------
// Create the pennants at the current wind barb position.

        for(int i = 0; i < floor(knots / 50); ++i)
        {
            vec3 pos0, pos1, pos2;
            vec3 pos = elemPos;

            //float scaleFactor = vectorField.adaptedLineWidth / lineWidth;
            pos += vectorField.normal * vectorField.adaptedLineWidth / 2.0;

            pos0 = pos - posOffset / 2.0 * vectorField.dir;
            pos1 = pos0 - vectorField.dir * posOffset / 2.0
                    + vectorField.dir * pennantPeakOffset
                    + vectorField.normal * largeWidth;
            pos2 = pos0 - vectorField.dir * posOffset / 2.0
                    + vectorField.dir * (pennantOffset + pennantPeakOffset);

            gl_Position = mvpMatrix * vec4(pos0, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos1, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos2, 1);
            EmitVertex();
            EndPrimitive();

            elemPos += 2 * posOffset * vectorField.dir;
        }

        knots = mod(knots,50);

        //elemPos += flagDist * 0.001 * vectorField.dir;

// -----------------------------------------------------------------------------
// Create the full flag triangle at the current wind barb position.

        for(int j = 0; j < floor(knots / 10); ++j)
        {
            vec3 pos0, pos1, pos2, pos3;
            vec3 pos = elemPos;

            pos += vectorField.normal * vectorField.adaptedLineWidth / 2.0;

            // left bottom
            pos0 = pos - vectorField.dir * vectorField.adaptedLineWidth / 2.0;
            // left top
            pos1 = pos0 - vectorField.dir * flagOffset
                    + vectorField.normal * largeWidth;
            // right bottom
            pos2 = pos + vectorField.dir * vectorField.adaptedLineWidth / 2.0;
            // right top
            pos3 = pos2 - vectorField.dir * flagOffset
                    + vectorField.normal * largeWidth;

            gl_Position = mvpMatrix * vec4(pos0, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos1, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos2, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos3, 1);
            EmitVertex();
            EndPrimitive();


            elemPos += posOffset * vectorField.dir;
        }

        knots = mod(knots,10);

// -----------------------------------------------------------------------------
// Create the half flag triangle at the current wind barb position.

        for(int k = 0; k < floor(knots / 5); ++k)
        {
            vec3 pos0, pos1, pos2, pos3;
            vec3 pos = elemPos;

            pos += vectorField.normal * vectorField.adaptedLineWidth / 2.0;

            // left bottom
            pos0 = pos - vectorField.dir * vectorField.adaptedLineWidth / 2.0;
            // left top
            pos1 = pos0 - vectorField.dir * flagOffset * 0.6
                    + vectorField.normal * smallWidth;
            // right bottom
            pos2 = pos + vectorField.dir * vectorField.adaptedLineWidth / 2.0;
            // right top
            pos3 = pos2 - vectorField.dir * flagOffset * 0.6
                    + vectorField.normal * smallWidth;

            gl_Position = mvpMatrix * vec4(pos0, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos1, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos2, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos3, 1);
            EmitVertex();
            EndPrimitive();

            elemPos += posOffset * vectorField.dir;
        }

        knots = mod(knots,5);
   }
   else
   {
// -----------------------------------------------------------------------------
// Create the glyph for calm wind with 2 circles consiting of 128 vertices each.
        
        if (showCalmGlyph)
        {

            vec3 pos = posWorld;

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
                float sinig = sin(angle)
                        * (smallRadius - vectorField.adaptedLineWidth);
                float cosig = cos(angle)
                        * (smallRadius - vectorField.adaptedLineWidth);

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
                float sinig = sin(angle)
                        * (largeRadius - vectorField.adaptedLineWidth);
                float cosig = cos(angle)
                        * (largeRadius - vectorField.adaptedLineWidth);

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

// -----------------------------------------------------------------------------
   }
}


shader GSarrows(in vec3 worldPos[], out GStoFS Output)
{
    Output.color = glyphColor;

    // Current world position.
    vec3 posWorld = worldPos[0];

    if (!computeVectorFieldData(posWorld))
    {
        return;
    }

    // distance between elements
    //const float pennantDist = (offset  + pennantOffset);
    //const float flagDist = offset * 1    * scaleDistFactor;

    //! NOTE (Kern, 2016-11-03): Using EmitVertex() and EndPrimitve() in local
    // functions is not possible after NVidia driver 340 since the latest driver
    // do not allow to use stage-specific functions in all shaders. GLFX always
    // includes local functions into all shader stage programs (VS/FS) which
    // leads to compilation errors for all stages except the geometry shader.

    // -------------------------------------------------------------------------
    // Create the base line of the arrow glyph.

    vec3 pos0, pos1, pos2, pos3;
    vec3 pos = posWorld;

    if (scaleArrowWithMagnitude)
    {
        if (scaleArrowDiscardBelow && vectorField.magnitude < scaleArrowMinScalar)
        {
            return;
        }
        if (scaleArrowDiscardAbove && vectorField.magnitude > scaleArrowMaxScalar)
        {
            return;
        }

        float scalar = min(max(vectorField.magnitude, scaleArrowMinScalar),
                           scaleArrowMaxScalar);
        float scalarScale = (scalar - scaleArrowMinScalar)
                / (scaleArrowMaxScalar - scaleArrowMinScalar);
        float lengthScale =
                scaleArrowMinLength + scalarScale *
                (scaleArrowMaxLength - scaleArrowMinLength);

        vectorField.glyphLength = lengthScale * vectorField.deltaGrid;
    }

    determineBaseLineVertices(pos, pos0, pos1, pos2, pos3);

    float arrowHeight = min(vectorField.glyphLength,
                            arrowHeadHeight * vectorField.deltaGrid);

    if (arrowHeight == 0.)
    {
        return;
    }

    vec3 posTip = pos1 + vectorField.normal * vectorField.adaptedLineWidth / 2.f;
    vec3 posBase = posTip - vectorField.dir * arrowHeight;
    vec3 sideVec = vectorField.normal * (arrowHeight);
    vec3 posLeft = posBase - sideVec;
    vec3 posRight = posBase + sideVec;

    pos1 = pos1 - vectorField.dir * arrowHeight;
    pos3 = pos3 - vectorField.dir * arrowHeight;


    // Draw base line.

    gl_Position = mvpMatrix * vec4(pos0, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos1, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos2, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos3, 1);
    EmitVertex();
    EndPrimitive();

    // Draw arrowhead.

    gl_Position = mvpMatrix * vec4(posBase + sideVec, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posTip, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posBase, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posBase - sideVec, 1);
    EmitVertex();
    EndPrimitive();

    if (!arrowDrawOutline)
    {
        return;
    }

    // Draw outline as triangle strip. Don't draw at the same position as the
    // inside of the arrow since this leads to z-fighting.

    float adaptedArrowOutlineWidth = vectorField.deltaGrid * arrowOutlineWidth;

    vec3 outlineOffsetDir = adaptedArrowOutlineWidth * vectorField.dir;
    vec3 outlineOffsetNormal = adaptedArrowOutlineWidth * vectorField.normal;

    vec3 pos0Out = pos0 - outlineOffsetDir - outlineOffsetNormal;
    vec3 pos1Out = pos1 - outlineOffsetDir - outlineOffsetNormal;
    vec3 pos2Out = pos2 - outlineOffsetDir + outlineOffsetNormal;
    vec3 pos3Out = pos3 - outlineOffsetDir + outlineOffsetNormal;

    // Since the arrowhead is a rectangular and isosceles triangle, the
    // positions of the outline vertices can be computed with the help of
    // parallelograms and rectangular and isosceles triangles.
    vec3 posTipOut = posTip + (sqrt(2) * outlineOffsetDir);
    vec3 posBaseOut = posBase - outlineOffsetDir;
    vec3 sideVecOut = sideVec + (sqrt(2.) + 1.) * adaptedArrowOutlineWidth
            * vectorField.normal;
    vec3 posLeftOut = posBaseOut - sideVecOut;
    vec3 posRightOut = posBaseOut + sideVecOut;

    Output.color = arrowOutlineColour;

    // Triangle strip which defines the outline.

    gl_Position = mvpMatrix * vec4(posLeftOut, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posLeft, 1.);
    EmitVertex();

    gl_Position = mvpMatrix * vec4(posTipOut, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posTip, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posRightOut, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posRight, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos3Out, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos3, 1.);
    EmitVertex();
    // Needs to be doubled since otherwise the outline won't be rendered
    // correctly.
    gl_Position = mvpMatrix * vec4(pos3Out, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos2, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos2Out, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos0, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos0Out, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos1, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos1Out, 1.);
    EmitVertex();

    gl_Position = mvpMatrix * vec4(posLeft, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posLeftOut, 1.);
    EmitVertex();
    EndPrimitive();
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

program WindBarbs
{
    vs(330)=VSmain();
    gs(330)=GSwindBarbs() : in(points), out(triangle_strip, max_vertices = 128);
    fs(330)=FSmain();
};

program Arrows
{
    vs(330)=VSmain();
    gs(330)=GSarrows() : in(points), out(triangle_strip, max_vertices = 128);
    fs(330)=FSmain();
};

program ArrowsOutline
{
    vs(330)=VSmain();
    gs(330)=GSarrows() : in(points), out(line_strip, max_vertices = 128);
//    gs(330)=GSarrows() : in(points), out(triangle_strip, max_vertices = 128);
    fs(330)=FSmain();
};
