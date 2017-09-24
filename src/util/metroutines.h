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
*******************************************************************************/
#ifndef METROUTINES_H
#define METROUTINES_H

// standard library imports

// related third party imports

// local application imports
#include "util/mutil.h"
#include "QVector2D"


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

// Water substance

// Earth and sun
const double GRAVITY_ACCELERATION = 9.80665; // m s^-2
const double EARTH_RADIUS_km = 6371.; // km
}


/******************************************************************************
***                                METHODS                                  ***
*******************************************************************************/

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


inline double crossProduct(const QVector2D& a, const QVector2D& b)
{
    return a.x() * b.y() - a.y() * b.x();
}


/**
  Calculates the intersecton between two line segments in 2D

   @p p,@p p2: end points of the first line
   @p q,@p q2: end points of the second line

  @return intersection point
 */
QVector2D getLineSegmentsIntersectionPoint(const QVector2D& p, const QVector2D& p2,
                                          const QVector2D& q, const QVector2D& q2);


} // namespace Met3D

#endif // METROUTINES_H
