/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2019 Marc Rautenhaus [*, previously +]
**  Copyright 2015-2017 Michael Kern [+]
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
**
**  + Computer Graphics and Visualization Group
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
*******************************************************************************/
#ifndef METROUTINES_H
#define METROUTINES_H

// standard library imports

// related third party imports

// local application imports
#include "util/mutil.h"


namespace Met3D
{

/******************************************************************************
***                               CONSTANTS                                 ***
*******************************************************************************/

// Definition of atmospheric science constants. Reference: e.g. Wallace and
// Hobbs (2006), pp. 467ff.
namespace MetConstants
{
// Universal constants

// Air
const double GAS_CONSTANT_DRY_AIR = 287.058; // J K^-1 kg^-1
const double SPECIFIC_HEAT_DRYAIR_CONST_PRESSURE = 1004.; // J K^-1 kg^-1

// Water substance

// Earth and sun
const double GRAVITY_ACCELERATION = 9.80665; // m s^-2
const double EARTH_RADIUS_km = 6371.; // km

// Source: http://hpiers.obspm.fr/eop-pc/models/constants.html
const double EARTHS_ANGULAR_SPEED_OF_ROTATION = 7.292115E-5; // rad/s

// Scaling factor for projections
const double scaleFactorToFitProjectedCoordsTo360Range = 1000.0;
}


/******************************************************************************
***                                METHODS                                  ***
*******************************************************************************/

inline double kelvinToDegC(double temperature_K)
{ return temperature_K - 273.15; }


inline double degCToKelvin(double temperature_degC)
{ return temperature_degC + 273.15; }


inline double degreesToRadians(double angle);


/**
  Computes the haversine: haversin(theta) = sin^2(theta/2.).
  @see https://en.wikipedia.org/wiki/Haversine_formula
 */
inline double haversin(double radians);


double gcDistanceUnitSphere_2(
        const double lon1_rad, const double lat1_rad,
        const double lon2_rad, const double lat2_rad);


/**
  Great circle distance between two points on a unit sphere, in radians.

  Reference: @see http://www.movable-type.co.uk/scripts/gis-faq-5.1.html

  "Presuming a spherical Earth with radius R (see below), and the locations of
  the two points in spherical coordinates (longitude and latitude) are
  lon1,lat1 and lon2,lat2 then the

  Haversine Formula (from R.W. Sinnott, "Virtues of the Haversine", Sky and
  Telescope, vol. 68, no. 2, 1984, p. 159):

        dlon = lon2 - lon1
        dlat = lat2 - lat1
        a = sin^2(dlat/2) + cos(lat1) * cos(lat2) * sin^2(dlon/2)
        c = 2 * arcsin(min(1,sqrt(a)))
        d = R * c

  will give mathematically and computationally exact results. The intermediate
  result c is the great circle distance in radians. The great circle distance d
  will be in the same units as R."

  @see gcDistance()

  @return The intermediate result c (the great circle distance in radians).
 */
double gcDistanceUnitSphere(
        const double lon1_rad, const double lat1_rad,
        const double lon2_rad, const double lat2_rad);


/**
  Great circle distance between two points on a sphere with radius @p radius,
  in the same units as @p radius.
 */
double gcDistance(const double lon1_rad, const double lat1_rad,
                         const double lon2_rad, const double lat2_rad,
                         const double radius);


/**
  Same as @ref gcDistance() but with lon/lat given in degrees.
 */
double gcDistance_deg(const double lon1, const double lat1,
                             const double lon2, const double lat2,
                             const double radius);


inline double cot(double radians);


/**
  Computes the area of a spherical triangle given by three points on a sphere.

  Similar to NCL's gc_tarea() function:
  @ref http://www.ncl.ucar.edu/Document/Functions/Built-in/gc_tarea.shtml
 */
double gcTriangleArea(const double lon1, const double lat1,
                      const double lon2, const double lat2,
                      const double lon3, const double lat3,
                      const double radius);


/**
  Computes the area of of a quadrilateral patch on a sphere of radius @p
  radius.

  Similar to NCL's gc_qarea() function:
  @ref http://www.ncl.ucar.edu/Document/Functions/Built-in/gc_qarea.shtml

  The area is computed by dividing the quadrilateral into two triangles (@see
  gcTriangleArea()) and summing the areas of those triangles.
 */
double gcQuadrilateralArea(const double lon1, const double lat1,
                           const double lon2, const double lat2,
                           const double lon3, const double lat3,
                           const double lon4, const double lat4,
                           const double radius);


/**
    Conversion of pressure (Pa) to geometric height (m) with hydrostatic
    equation, according to the profile of the ICAO standard atmosphere.

    Reference:
        For example, H. Kraus, Die Atmosphaere der Erde, Springer, 2001,
        470pp., Sections II.1.4. and II.6.1.2.

    @param p_Pa pressure (Pa)
    @return geometric elevation in m
 */
double pressure2metre_standardICAO(double p_Pa);


/**
    Conversion of geometric height (given in m) to pressure (Pa) with
    hydrostatic equation, according to the profile of the ICAO standard
    atmosphere.

    Reference:
        For example, H. Kraus, Die Atmosphaere der Erde, Springer, 2001,
        470pp., Sections II.1.4. and II.6.1.2.

    @param z_m elevation in m
    @return pressure in Pa
 */
double metre2pressure_standardICAO(double z_m);


/**
    International standard atmosphere temperature at the given elevation.

    Reference:
        For example, H. Kraus, Die Atmosphaere der Erde, Springer, 2001,
        470pp., Sections II.1.4. and II.6.1.2.

    @param z_m elevation in m
    @return temperature (K)
 */
double isaTemperature(double z_m);


/**
  Convert @p flightlevel in hft to metres.
 */
double flightlevel2metre(double flightlevel);


/**
  Convert @p z_m in m to flightlevel in hft.
 */
double metre2flightlevel(double z_m);


/**
  Computes the mass (kg) of the air in a column with surface area @p area_m2
  (m^2), between bottom and top pressures @p pbot_Pa (Pa) and @p ptop_Pa (Pa).

  Reference:
    Wallace and Hobbs (2006), Atmospheric Science 2nd ed., p 68, Fig. 3.1:
    downward force Fd = mass * grav. accel. == upward force Fu = deltap * area.

  @return airmass (kg)
 */
double columnAirmass(double pbot_Pa, double ptop_Pa, double area_m2);


/**
  Computes the volume (m^3) of a box with air pressure @p p_Pa (Pa), air mass
  @p mass_kg (kg) and air temperature @p temp_K (K). The ideal gas law with
  the gas constant for dry air is used.

  Reference:
    Wallace and Hobbs (2006), Atmospheric Science 2nd ed., Ch. 3.
    Ideal gas law: p*V = m*R*T --> V = m*R*T / p

  @return volume in m^3
 */
double boxVolume_dry(double p_Pa, double mass_kg, double temp_K);


/**
  Computes the volume of a regular lon/lat/pressure grid box.

  If @p temp_K is not specified, the temperature is estimated from the
  international standard atmosphere. @see isaTemperature().

  @return volume in m^3
 */
double boxVolume_dry(double northWestLon, double northWestLat,
                     double southEastLon, double southEastLat,
                     double pmid_Pa, double pbot_Pa, double ptop_Pa,
                     double temp_K = M_MISSING_VALUE);

/**
  Computes wind speed in [m.s-1] from eastward wind (@p u_ms) and northward
  wind (@p v_ms).
 */
double windSpeed_ms(double u_ms, double v_ms);


/**
  Computes 3D wind speed (aka magnitude of air velocity in CF naming) in
  [m.s-1] from eastward wind (@p u_ms), northward wind (@p v_ms) and
  upward_air_velocity (@p w_ms).
 */
double windSpeed3D_ms(double u_ms, double v_ms, double w_ms);


/**
  Computes potential temperature in [K] from temperature @p T_K in [K] and
  pressure @p p_Pa in [Pa].

  Method:
                            theta = T * (p0/p)^(R/cp)
      with p0 = 100000. Pa, R = 287.058 JK-1kg-1, cp = 1004 JK-1kg-1.
 */
double potentialTemperature_K(double T_K, double p_Pa);


/**
  Computes ambient temperature in [K] of a given potential temperature in [K]
  at pressure @p p_Pa in [Pa].

  Method:
                            T = theta / (p0/p)^(R/cp)
      with p0 = 100000. Pa, R = 287.058 JK-1kg-1, cp = 1004 JK-1kg-1.
 */
double ambientTemperatureOfPotentialTemperature_K(double theta_K, double p_Pa);


/**
  Computes the virtual temperature in [K] from temperature @p T_K in [K] and
  specific humidity @p q_kgkg in [kg/kg].

  Method:
                  Tv = T * (q + 0.622(1-q)) / 0.622

  Reference: Wallace&Hobbs 2nd ed., eq. 3.16, 3.59, and 3.60
             (substitute w=q/(1-q) in 3.16 and 3.59 to obtain the exact
             formula).
 */
double virtualTemperature_K(double T_K, double q_kgkg);


/**
  Computes the thickness of an atmospheric layer in terms of geopotential
  height in [m], using the hypsometric question (equation (1.17) from Holton,
  "An Introduction to Dynamic Meteorology", 3rd edition, 1992) in its
  geopotential height formulation (equation (1.18 and 1.19) from Holton).

  @p layerMeanVirtualTemperature_K specifies the layer mean (virtual)
  temperature (use the virtual temperature to account for humidity) in [K],
  @p p_bot and @p p_top the bounding pressure heights (can be either both in
  [Pa] or both in [hPa]; only their ratio is used).
 */
double geopotentialThicknessOfLayer_m(double layerMeanVirtualTemperature_K,
                                      double p_bot, double p_top);


/**
  Computes the mixing ratio w from specific humidity @p q_kgkg in [kg/kg].

  Method: w = q / (1.-q)
 */
double mixingRatio_kgkg(double q_kgkg);


/**
  Computes the specific humidity from mixing ratio @p w_kgkg in [kg/kg].

  Method: q = w / (1.+w)
 */
double specificHumidity_kgkg(double w_kgkg);


/**
  Computes dew point in [K] from pressure @p p_Pa in [Pa] and specific
  humidity @p q_kgkg in [kg/kg].

  Method: Use the inversion of Bolton (1980), eq. 10, to compute dewpoint.
  According to Bolton, this is accurate to 0.03 K in the range 238..308 K. See
  also Emanuel (1994, 'Atmospheric Convection', eq. 4.6.2).
 */
double dewPointTemperature_K_Bolton(double p_Pa, double q_kgkg);


/**
  Computes equivalent potential temperature in [K] from temperature @p T_K
  in [K], pressure @p p_Pa in [Pa] and specific humidity @p q_kgkg in [kg/kg].

  Method: Equation (43) of Bolton (MWR, 1980), "The Computation of Equivalent
  Potential Temperature".
 */
double equivalentPotentialTemperature_K_Bolton(double T_K, double p_Pa,
                                               double q_kgkg);


/**
  Computes the ambient temperature at pressure @p p_Pa along a saturated
  adiabat defined by wet-bulb potential temperature @p thetaW_K. Computation
  uses the (approximative) noniterative approach described by Moisseeva and
  Stull (ACP, 2017), "Technical note: A noniterative approach to modelling
  moist thermodynamics".
 */
double temperatureAlongSaturatedAdiabat_K_MoisseevaStull(
        double thetaW_K, double p_Pa);

/**
  Inverse of @ref temperatureAlongSaturatedAdiabat_K_MoisseevaStull().
 */
double wetBulbPotentialTemperatureOfSaturatedAdiabat_K_MoisseevaStull(
        double T_K, double p_Pa);

/**
  Coriolis parameter at specified latitude @p lat (in degrees).
 */
double coriolisParameter_deg(double lat);


/******************************************************************************
***            WRAPPER for LAGRANTO LIBCALVAR FORTRAN FUNCTIONS             ***
*******************************************************************************/

/**
  Wrapper for Fortran implementation of potential temperature in the LAGRANTO
  package (libcalvar.f; https://svn.iac.ethz.ch/websvn/pub/wsvn/lagranto.ecmwf/).

  See http://www.yolinux.com/TUTORIALS/LinuxTutorialMixingFortranAndC.html
  for information about how to mix Fortran and C++. In particular the index
  dimension of the arrays is of importance. However, the methods in libcalvar
  are conveniently written so that arrays are accessed in FORTRAN i,j,k order,
  which corresponds to the k,j,i oder used in thje C++ code of Met.3D.

  @note All fields below are in the index order that is used in MStructuredGrid
  EXCEPT the lat dimension, which needs to be reversed prior to calling this
  method.

  @param[out] pt Potential temperature in K; pointer to 3D field.
  @param[in] t Temperature in K; pointer to 3D field.
  @param[in] sp Surface pressure in hPa; pointer to 2D field.
  @param ie Array size in lon dimension.
  @param je Array size in lat dimension.
  @param ke Array size in lev dimension.
  @param[in] ak Hybrid coefficients.
  @param[in] bk Hybrid coefficients.
 */
void potentialTemperature_K_calvar(
        float *pt, float *t, float *sp,
        int ie, int je, int ke,
        float *ak, float *bk);


/**
  Wrapper for Fortran implementation of potential vorticity in the LAGRANTO
  package (libcalvar.f; https://svn.iac.ethz.ch/websvn/pub/wsvn/lagranto.ecmwf/).

  See above comment of @ref potentialTemperature_K_calvar.

  @param[out] pv Potential vorticity in PVU; pointer to 3D field.
  @param[in] uu u-wind in ms-1; pointer to 3D field.
  @param[in] vv v-wind in ms-1; pointer to 3D field.
  @param[in] th Potential temperature in K; pointer to 3D field.
  @param[in] sp Surface pressure in hPa; pointer to 2D field.
  @param[in] cl cos(lat); pointer to 2D field.
  @param[in] f Coriolis parameter; pointer to 2D field.
  @param ie Array size in lon dimension.
  @param je Array size in lat dimension.
  @param ke Array size in lev dimension.
  @param[in] ak Hybrid coefficients.
  @param[in] bk Hybrid coefficients.
  @param[in] vmin 4-element array containing [min lon, min lat, ?, ?]
  @param[in] vmax 4-element array containing [max lon, max lat, ?, ?]
 */
void potentialVorticity_PVU_calvar(
        float *pv, float *uu, float *vv, float *th,
        float *sp, float *cl, float *f,
        int ie, int je, int ke,
        float *ak, float *bk,
        float *vmin, float *vmax);


/******************************************************************************
***            Test functions for meteorological computations.              ***
*******************************************************************************/

namespace MetRoutinesTests
{

void test_EQPT();
void test_temperatureAlongSaturatedAdiabat_K_MoisseevaStull();

void runMetRoutinesTests();

} // namespace MetRoutinesTests

} // namespace Met3D

#endif // METROUTINES_H
