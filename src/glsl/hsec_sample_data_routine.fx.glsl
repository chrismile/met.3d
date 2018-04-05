/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2018 Marc Rautenhaus
**  Copyright 2018 Bianca Tost
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
*******************************************************************************/

///******************************************************************************
//***                             CONSTANTS                                   ***
//*******************************************************************************/

const float MISSING_VALUE = -999.E9;

const int SURFACE_2D = 0;
const int PRESSURE_LEVELS_3D =  1;
const int HYBRID_SIGMA_PRESSURE_3D =  2;
const int POTENTIAL_VORTICITY_2D =  3;
const int LOG_PRESSURE_LEVELS_3D =  4;
const int AUXILIARY_PRESSURE_3D =  5;

///******************************************************************************
// ***                             UNIFORMS
// ******************************************************************************/

uniform float       deltaLon;
uniform float       deltaLat;

uniform vec2        dataNWCrnr;

uniform float       pressure_hPa; // current pressure of hsec

uniform int         levelType;       // vertical level type of the data grid
uniform sampler1D   latLonAxesData;  // 1D texture that holds both lon and lat
uniform int         verticalOffset;  // index at which vertical axis data starts


float sampleDataAtPos(  in vec3 pos,
                        in sampler3D volume,
                        in sampler2D dataField2D,
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
    if (levelType == SURFACE_2D)
    {
        dims = ivec3(textureSize(dataField2D, 0), 1);
    }
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

    // Supported grid type 4: 2D SURFACE FIELDS
    // ========================================
    else if (levelType == SURFACE_2D)
    {
        float scalar = texelFetch(dataField2D, ivec2(i, j), 0).a;
        return scalar;
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
