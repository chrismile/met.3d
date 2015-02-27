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

const int ABSOLUTE_DIFFERENCE = 1;
const int RELATIVE_DIFFERENCE = 2;

// Vertical level type; see structuredgrid.h.
const int SURFACE_2D = 0;
const int PRESSURE_LEVELS_3D = 1;
const int HYBRID_SIGMA_PRESSURE_3D = 2;
const int POTENTIAL_VORTICITY_2D = 3;
const int LOG_PRESSURE_LEVELS_3D = 4;
const int AUXILIARY_PRESSURE_3D = 5;


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

uniform int       levelType;         // vertical level type of the data grid
uniform sampler1D latLonAxesData;    // 1D texture that holds both lon and lat
uniform int       verticalOffset;    // index at which vertical axis data starts
uniform sampler3D dataField;         // 3D texture holding the scalar data
uniform sampler2D dataField2D;       // 2D texture holding the scalar data
                                     // in case of surface fields
uniform sampler2D surfacePressure;   // surface pressure field in Pa
uniform sampler1D hybridCoefficients;// hybrid sigma pressure coefficients

uniform float     pressure_hPa;      // isopressure of this cross section
uniform int       iOffset;           // grid index offsets if only a subregion
uniform int       jOffset;           //   of the grid shall be rendered

layout(r32f)                         // texture in which the interpolated grid
uniform image2D   crossSectionGrid;  // is stored.

shader VSmain()
{
    // Compute grid indices (i, j) of the this vertex from vertex and instance
    // ID (notes 09Feb2012).
    int i = int(gl_VertexID / 2);
    int j = bool(gl_VertexID & 1) ? (gl_InstanceID + 1) : gl_InstanceID;

    // Subregion shift?
    int numLons = textureSize(dataField, 0).x;
    if (levelType == SURFACE_2D) numLons = textureSize(dataField2D, 0).x;
    i = (i + iOffset) % numLons;
    j = j + jOffset;

    // Variables for binary level search and interpolation.
    int numberOfLevels = 0;
    int klower = 0;
    int kupper = 0;

    float ln_plower = 0.;
    float ln_pupper = 0.;
    float ln_p = log(pressure_hPa);;


    // Supported grid type 1: HYBRID MODEL LEVELS
    // ==========================================
    if (levelType == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Now we need to determine the indices of the two model levels that
        // enclose the requested iso-pressure value "pressure_hPa". A binary
        // search is performed: Two indices klower and kupper at first point to
        // the first and last available model levels, depending on the value of
        // the element midway between both, the interval is cut in half until
        // it consists of two adjacent levels.

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

    // Supported grid type 4: 2D SURFACE FIELDS
    // ========================================
    else if (levelType == SURFACE_2D)
    {
        float scalar = texelFetch(dataField2D, ivec2(i, j), 0).a;
        imageStore(crossSectionGrid, ivec2(i, j), vec4(scalar, 0, 0, 0));
        return;
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
        float scalar_klower = texelFetch(dataField, ivec3(i, j, klower), 0).a;
        float scalar_kupper = texelFetch(dataField, ivec3(i, j, kupper), 0).a;

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

    // Store the computed vertex (= grid point) scalar value into the texture
    // "crossSectionGrid" for further processing (e.g. contouring, CPU
    // read-back).
    imageStore(crossSectionGrid, ivec2(i, j), vec4(scalar, 0, 0, 0));
}


/*****************************************************************************/
/*****************************************************************************/

// Vertex shader for difference mode (compute the per-vertex difference
// between two fields).

uniform int       levelType1;         // vertical level type of the data grid
uniform sampler1D latLonAxesData1;    // 1D texture that holds both lon and lat
uniform int       verticalOffset1;    // index at which vertical axis data starts
uniform sampler3D dataField1;         // 3D texture holding the scalar data
uniform sampler2D dataField2D1;       // 2D texture holding surface scalar data
uniform sampler2D surfacePressure1;   // surface pressure field in Pa
uniform sampler1D hybridCoefficients1;// hybrid sigma pressure coefficients

uniform int       levelType2;         // vertical level type of the data grid
uniform sampler1D latLonAxesData2;    // 1D texture that holds both lon and lat
uniform int       verticalOffset2;    // index at which vertical axis data starts
uniform sampler3D dataField2;         // 3D texture holding the scalar data
uniform sampler2D dataField2D2;       // 2D texture holding surface scalar data
uniform sampler2D surfacePressure2;   // surface pressure field in Pa
uniform sampler1D hybridCoefficients2;// hybrid sigma pressure coefficients

uniform int       mode;               // 1 = absolute difference, 2 = relative

shader VSmainDiff()
{
    // Compute grid indices (i, j) of the this vertex from vertex and instance
    // ID (see notes 09Feb2012).
    int i = int(gl_VertexID / 2);
    int j = bool(gl_VertexID & 1) ? (gl_InstanceID + 1) : gl_InstanceID;

    int numLons = textureSize(dataField1, 0).x;
    if (levelType1 == SURFACE_2D) numLons = textureSize(dataField2D1, 0).x;
    i = (i + iOffset) % numLons;
    j = j + jOffset;

    // Compute horizontal section of first variable.
    // ========================================================================

    int numberOfLevels = 0;
    int klower = 0;
    int kupper = 0;

    float ln_plower = 0.;
    float ln_pupper = 0.;
    float ln_p = log(pressure_hPa);

    // Interpolated scalar value.
    float scalar;

    // Supported grid type 1: HYBRID MODEL LEVELS
    // ==========================================
    if (levelType1 == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Now we need to determine the indices of the two model levels that
        // enclose the requestred iso-pressure value "pressure_hPa". A binary
        // search is performed, similar to the "locate" function in the
        // Numerical Recipes, 3rd ed., p.115: Two indices klower and kupper at
        // first point to the first and last available model levels, depending
        // on the value of the element midway between both, the interval is cut
        // in half until it consists of two adjacent levels.

        // The hybridCoefficients1 texture contains both the ak and bk
        // coefficients, hence the number of levels is half the size of this
        // array.
        numberOfLevels = textureSize(hybridCoefficients1, 0) / 2;

        // Initial position of klower and kupper.
        klower = 0;
        kupper = numberOfLevels - 1;

        // Fetch the surface pressure at (i, j). It is used to compute the
        // pressure at the model levels via p = ak + bk * psfc.
        float psfc_hPa = texelFetch(surfacePressure1, ivec2(i, j), 0).a / 100.;

        // Perform the binary search.
        while ((kupper-klower) > 1)
        {
            // Element midway between klower and kupper.
            int kmid  = (kupper+klower) / 2;
            // Compute pressure at kmid.
            float ak = texelFetch(hybridCoefficients1, kmid, 0).a;
            float bk = texelFetch(hybridCoefficients1, kmid+numberOfLevels, 0).a;
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
        float aklower = texelFetch(hybridCoefficients1, klower, 0).a;
        float bklower = texelFetch(hybridCoefficients1, klower+numberOfLevels, 0).a;
        float akupper = texelFetch(hybridCoefficients1, kupper, 0).a;
        float bkupper = texelFetch(hybridCoefficients1, kupper+numberOfLevels, 0).a;

        // Compute the log of the pressure values.
        ln_plower = log(aklower + bklower * psfc_hPa);
        ln_pupper = log(akupper + bkupper * psfc_hPa);
        // ln_p      = log(pressure_hPa);

        // Alternative: Interpolate linerarly in p.
        //    ln_plower = (aklower + bklower * psfc_hPa);
        //    ln_pupper = (akupper + bkupper * psfc_hPa);
    } // levelType == HYBRID_SIGMA_PRESSURE_3D


    // Supported grid type 2: PRESSURE LEVELS
    // ======================================
    else if (levelType1 == PRESSURE_LEVELS_3D)
    {
        numberOfLevels = textureSize(latLonAxesData1, 0) - verticalOffset1;
        klower = 0;
        kupper = numberOfLevels - 1;

        while ((kupper-klower) > 1)
        {
            int kmid = (kupper+klower) / 2;
            float pressureAt_kmid_hPa =
                texelFetch(latLonAxesData1, verticalOffset1+kmid, 0).a;
            if (pressure_hPa >= pressureAt_kmid_hPa)
                klower = kmid;
            else
                kupper = kmid;
        }

        ln_plower = log(texelFetch(latLonAxesData1, verticalOffset1+klower, 0).a);
        ln_pupper = log(texelFetch(latLonAxesData1, verticalOffset1+kupper, 0).a);
        //    ln_plower = (texelFetch(latLonAxesData1, verticalOffset1+klower, 0).a);
        //    ln_pupper = (texelFetch(latLonAxesData1, verticalOffset1+kupper, 0).a);
    }


    // Supported grid type 3: LOG(PRESSURE) LEVELS
    // ===========================================
    else if (levelType1 == LOG_PRESSURE_LEVELS_3D)
    {
        numberOfLevels = textureSize(latLonAxesData1, 0) - verticalOffset1;
        klower = 0;
        kupper = numberOfLevels - 1;

        while ((kupper-klower) > 1)
        {
            // Element midway between klower and kupper.
            int kmid = (kupper+klower) / 2;
            // Get pressure at kmid.
            float ln_pressureAt_kmid =
                texelFetch(latLonAxesData1, verticalOffset1+kmid, 0).a;
            // Cut interval in half.
            if (ln_p >= ln_pressureAt_kmid)
                klower = kmid;
            else
                kupper = kmid;
        }

        ln_plower = texelFetch(latLonAxesData1, verticalOffset1+klower, 0).a;
        ln_pupper = texelFetch(latLonAxesData1, verticalOffset1+kupper, 0).a;
    }

    // Supported grid type 4: 2D SURFACE FIELDS
    // ========================================
    if (levelType1 == SURFACE_2D)
    {
        scalar = texelFetch(dataField2D1, ivec2(i, j), 0).a;
    }
    else
    {
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
            float scalar_klower = texelFetch(dataField1, ivec3(i, j, klower), 0).a;
            float scalar_kupper = texelFetch(dataField1, ivec3(i, j, kupper), 0).a;

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
    }

    // Compute horizontal section of second variable.
    // ========================================================================

    if (levelType2 == HYBRID_SIGMA_PRESSURE_3D)
    {
        numberOfLevels = textureSize(hybridCoefficients2, 0) / 2;

        klower = 0;
        kupper = numberOfLevels - 1;

        float psfc_hPa = texelFetch(surfacePressure2, ivec2(i, j), 0).a / 100.;

        while ((kupper-klower) > 1)
        {
            int kmid  = (kupper+klower) / 2;
            float ak = texelFetch(hybridCoefficients2, kmid, 0).a;
            float bk = texelFetch(hybridCoefficients2, kmid+numberOfLevels, 0).a;
            float pressureAt_kmid_hPa = ak + bk * psfc_hPa;
            if (pressure_hPa >= pressureAt_kmid_hPa)
                klower = kmid;
            else
                kupper = kmid;
        }

        float aklower = texelFetch(hybridCoefficients2, klower, 0).a;
        float bklower = texelFetch(hybridCoefficients2, klower+numberOfLevels, 0).a;
        float akupper = texelFetch(hybridCoefficients2, kupper, 0).a;
        float bkupper = texelFetch(hybridCoefficients2, kupper+numberOfLevels, 0).a;

        ln_plower = log(aklower + bklower * psfc_hPa);
        ln_pupper = log(akupper + bkupper * psfc_hPa);
        // ln_p      = log(pressure_hPa);

        // ln_plower = (aklower + bklower * psfc_hPa);
        // ln_pupper = (akupper + bkupper * psfc_hPa);
    }

    else if (levelType2 == PRESSURE_LEVELS_3D)
    {
        numberOfLevels = textureSize(latLonAxesData2, 0) - verticalOffset2;
        klower = 0;
        kupper = numberOfLevels - 1;

        while ((kupper-klower) > 1)
        {
            int kmid = (kupper+klower) / 2;
            float pressureAt_kmid_hPa =
                texelFetch(latLonAxesData2, verticalOffset2+kmid, 0).a;
            if (pressure_hPa >= pressureAt_kmid_hPa)
                klower = kmid;
            else
                kupper = kmid;
        }

        ln_plower = log(texelFetch(latLonAxesData2, verticalOffset2+klower, 0).a);
        ln_pupper = log(texelFetch(latLonAxesData2, verticalOffset2+kupper, 0).a);
        // ln_plower = (texelFetch(latLonAxesData2, verticalOffset2+klower, 0).a);
        // ln_pupper = (texelFetch(latLonAxesData2, verticalOffset2+kupper, 0).a);
    }

    else if (levelType2 == LOG_PRESSURE_LEVELS_3D)
    {
        numberOfLevels = textureSize(latLonAxesData2, 0) - verticalOffset2;
        klower = 0;
        kupper = numberOfLevels - 1;

        while ((kupper-klower) > 1)
        {
            // Element midway between klower and kupper.
            int kmid = (kupper+klower) / 2;
            // Get pressure at kmid.
            float ln_pressureAt_kmid =
                texelFetch(latLonAxesData2, verticalOffset2+kmid, 0).a;
            // Cut interval in half.
            if (ln_p >= ln_pressureAt_kmid)
                klower = kmid;
            else
                kupper = kmid;
        }

        ln_plower = texelFetch(latLonAxesData2, verticalOffset2+klower, 0).a;
        ln_pupper = texelFetch(latLonAxesData2, verticalOffset2+kupper, 0).a;
    }

    float scalar2 = MISSING_VALUE;

    // Supported grid type 4: 2D SURFACE FIELDS
    // ========================================
    if (levelType2 == SURFACE_2D)
    {
        scalar2 = texelFetch(dataField2D2, ivec2(i, j), 0).a;
    }
    else
    {
        if (ln_pupper > ln_p)
        {
            // Fetch the scalar values at klower and kupper.
            float scalar_klower = texelFetch(dataField2, ivec3(i, j, klower), 0).a;
            float scalar_kupper = texelFetch(dataField2, ivec3(i, j, kupper), 0).a;

            if ((scalar_klower == MISSING_VALUE) || (scalar_kupper == MISSING_VALUE))
            {
                scalar2 = MISSING_VALUE;
            }
            else
            {
                // Linearly interpolate in ln(p) between the scalar values at level
                // klower and level kupper.
                float a = (ln_p - ln_plower) / (ln_pupper - ln_plower);
                scalar2 = mix(scalar_klower, scalar_kupper, a);
            }
        }
    }
    
    // Compute difference between both variables.
    // ========================================================================

    if (scalar == MISSING_VALUE || scalar2 == MISSING_VALUE)
    {
        scalar = MISSING_VALUE;
    }
    else
    {
        if (mode == ABSOLUTE_DIFFERENCE)
            scalar = scalar - scalar2;
        else // mode == RELATIVE_DIFFERENCE
        {
//            if (scalar != 0.)
                scalar = (scalar - scalar2) / scalar * 100.;
//            else
//                scalar = 0.;
//            if (isnan(scalar)) scalar = 0;
        }
    }

    // Store the computed vertex (= grid point) scalar value into the texture
    // "crossSectionGrid" for further processing (e.g. contouring, CPU read-back).
    imageStore(crossSectionGrid, ivec2(i, j), vec4(scalar, 0, 0, 0));
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

shader FSmain()
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

program Difference
{
    vs(420)=VSmainDiff();
    fs(420)=FSmain();
};
