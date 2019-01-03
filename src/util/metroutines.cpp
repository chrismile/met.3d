/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2015-2018 Michael Kern
**  Copyright 2015-2016 Christoph Heidelmann
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
#include "metroutines.h"

// standard library imports
#include <iostream>
#include <cmath>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"

using namespace std;


namespace Met3D
{

inline double degreesToRadians(double angle)
{
    return angle / 180. * M_PI;
}


inline double haversin(double radians)
{
    double sinValue = sin(radians / 2.);
    return sinValue * sinValue;
}


double gcDistanceUnitSphere_2(
        const double lon1_rad, const double lat1_rad,
        const double lon2_rad, const double lat2_rad)
{
    const double deltaLon = lon2_rad - lon1_rad;
    const double deltaLat = lat2_rad - lat1_rad;

    // Compute distance using the haversine formula.
    const double havSinAlpha =
            haversin(deltaLat) + std::cos(lat1_rad) * std::cos(lat2_rad)
            * haversin(deltaLon);

    return 2. * std::asin(std::sqrt(havSinAlpha));
}


double gcDistanceUnitSphere(
        const double lon1_rad, const double lat1_rad,
        const double lon2_rad, const double lat2_rad)
{
    double dlon = lon2_rad - lon1_rad;
    double dlat = lat2_rad - lat1_rad;

    double sin_dlat = sin(dlat/2.);
    double sin_dlon = sin(dlon/2.);
    double a = sin_dlat*sin_dlat
            + cos(lat1_rad) * cos(lat2_rad) * sin_dlon*sin_dlon;
    double c = 2. * asin(min(1.,sqrt(a)));
    return c;
}


double gcDistance(const double lon1_rad, const double lat1_rad,
                  const double lon2_rad, const double lat2_rad,
                  const double radius)
{
    return gcDistanceUnitSphere(
                lon1_rad, lat1_rad, lon2_rad, lat2_rad) * radius;
}


double gcDistance_deg(const double lon1, const double lat1,
                      const double lon2, const double lat2,
                      const double radius)
{
    return gcDistanceUnitSphere(
                degreesToRadians(lon1), degreesToRadians(lat1),
                degreesToRadians(lon2), degreesToRadians(lat2)) * radius;
}


inline double cot(double radians)
{
    return 1. / std::tan(radians);
}


double gcTriangleArea(const double lon1, const double lat1,
                      const double lon2, const double lat2,
                      const double lon3, const double lat3,
                      const double radius)
{
    using namespace std;

    // Great circle distances between the vertices.
    const double c = gcDistanceUnitSphere(
                degreesToRadians(lon1), degreesToRadians(lat1),
                degreesToRadians(lon2), degreesToRadians(lat2));

    const double a = gcDistanceUnitSphere(
                degreesToRadians(lon2), degreesToRadians(lat2),
                degreesToRadians(lon3), degreesToRadians(lat3));

    const double b = gcDistanceUnitSphere(
                degreesToRadians(lon1), degreesToRadians(lat1),
                degreesToRadians(lon3), degreesToRadians(lat3));

    const double PI_HALF = M_PI / 2.0f;

    // Angles between the triangle sides in radians.
    double A,B,C;

    // Sherical excess.
    double E = 0;

    // Handle quadrantal triangle case by applying napier's rule.
    const double PRECISION = 0.000001;

    // If all sides are PI_HALF...
    if (abs(a - PI_HALF) <= PRECISION &&
            abs(b - PI_HALF) <= PRECISION &&
            abs(c - PI_HALF) <= PRECISION)
    {
        // ...all three angles are M_PI / 2.
        E = PI_HALF;
    }

    // If two sides are PI_HALF the corresponding two angles are PI_HALF,
    // the third angle = opposite side.
    else if (abs(a - PI_HALF) <= PRECISION &&
             abs(b - PI_HALF) <= PRECISION)
    {
        E = c;
    }

    else if (abs(b - PI_HALF) <= PRECISION &&
             abs(c - PI_HALF) <= PRECISION)
    {
        E = a;
    }

    else if (abs(a - PI_HALF) <= PRECISION &&
             abs(c - PI_HALF) <= PRECISION)
    {
        E = b;
    }

    // If one side is PI_HALF apply Napier's rule.
    else if (abs(a - PI_HALF) <= PRECISION)
    {
        A = acos(-cot(c) * cot(b) );
        B = asin( sin(A) * sin(b) );
        C = asin( sin(A) * sin(c) );

        E = A + B + C - M_PI;
    }
    else if (abs(b - PI_HALF) <= PRECISION)
    {
        B = acos(-cot(c) * cot(a) );
        A = asin( sin(B) * sin(a) );
        C = asin( sin(B) * sin(c) );

        E = A + B + C - M_PI;
    }
    else if (abs(c - PI_HALF) <= PRECISION)
    {
        C = acos(-cot(a) * cot(b) );
        A = asin( sin(C) * sin(a) );
        B = asin( sin(C) * sin(b) );

        E = A + B + C - M_PI;
    }

    // ...else apply spherical trigonometry.
    else
    {
        const double cosa = cos(a);
        const double cosb = cos(b);
        const double cosc = cos(c);

        const double sina = sin(a);
        const double sinb = sin(b);
        const double sinc = sin(c);

        A = acos( (cosa - cosb * cosc) / (sinb * sinc) );
        B = acos( (cosb - cosc * cosa) / (sinc * sina) );
        C = acos( (cosc - cosa * cosb) / (sina * sinb) );

        E = A + B + C - M_PI;
    }

    return radius * radius * E;
}


double gcQuadrilateralArea(const double lon1, const double lat1,
                           const double lon2, const double lat2,
                           const double lon3, const double lat3,
                           const double lon4, const double lat4,
                           const double radius)
{
    return gcTriangleArea(lon1, lat1, lon2, lat2, lon3, lat3, radius) +
            gcTriangleArea(lon3, lat3, lon2, lat2, lon4, lat4, radius);
}


double pressure2metre_standardICAO(double p_Pa)
{
    // g and R are used by all equations below.
    double g = MetConstants::GRAVITY_ACCELERATION;
    double R = MetConstants::GAS_CONSTANT_DRY_AIR;

    double z = 0.;
    if (p_Pa < 1011.)
    {
        // Pressure to metre conversion not implemented for z > 32km
        // (p ~ 10.11 hPa).
        return M_MISSING_VALUE;
    }

    else if (p_Pa < 5475.006582501095)
    {
        // ICAO standard atmosphere between 20 and 32 km: T(z=20km) = -56.5
        // degC, p(z=20km) = 54.75 hPa. Temperature gradient is -1.0 K/km.
        double z0 = 20000.;
        double T0 = 216.65;
        double gamma = -1.0e-3;
        double p0 = 5475.006582501095;

        // Hydrostatic equation with linear temperature gradient.
        z = 1./gamma * (T0 - (T0-gamma*z0) * exp(gamma*R/g * log(p_Pa/p0)));
    }

    else if (p_Pa < 22632.)
    {
        // ICAO standard atmosphere between 11 and 20 km: T(z=11km) = -56.5
        // degC, p(z=11km) = 226.32 hPa. Temperature is constant at -56.5 degC.
        double z0 = 11000.;
        double p0 = 22632.;
        double T = 216.65;

        // Hydrostatic equation with constant temperature profile.
        z = z0 - (R*T)/g * log(p_Pa/p0);
    }

    else
    {
        // ICAO standard atmosphere between 0 and 11 km: T(z=0km) = 15 degC,
        // p(z=0km) = 1013.25 hPa. Temperature gradient is 6.5 K/km.
        double z0 = 0.;
        double T0 = 288.15;
        double gamma = 6.5e-3;
        double p0 = 101325.;

        // Hydrostatic equation with linear temperature gradient.
        z = 1./gamma * (T0 - (T0-gamma*z0) * exp(gamma*R/g * log(p_Pa/p0)));
    }

    return z;
}


double metre2pressure_standardICAO(double z_m)
{
    // g and R are used by all equations below.
    double g = MetConstants::GRAVITY_ACCELERATION;
    double R = MetConstants::GAS_CONSTANT_DRY_AIR;

    if (z_m <= 11000.)
    {
        // ICAO standard atmosphere between 0 and 11 km: T(z=0km) = 15 degC,
        // p(z=0km) = 1013.25 hPa. Temperature gradient is 6.5 K/km.
        double z0 = 0.;
        double T0 = 288.15;
        double gamma = 6.5e-3;
        double p0 = 101325.;

        // Hydrostatic equation with linear temperature gradient.
        double p = p0 * pow((T0-gamma*z_m) / (T0-gamma*z0), g/(gamma*R));
        return p;
    }

    else if (z_m <= 20000.)
    {
        // ICAO standard atmosphere between 11 and 20 km: T(z=11km) = -56.5
        // degC, p(z=11km) = 226.32 hPa. Temperature is constant at -56.5 degC.
        double z0 = 11000.;
        double p0 = 22632.;
        double T = 216.65;

        // Hydrostatic equation with constant temperature profile.
        double p = p0 * exp(-g * (z_m-z0) / (R*T));
        return p;
    }

    else if (z_m <= 32000.)
    {
        // ICAO standard atmosphere between 20 and 32 km: T(z=20km) = -56.5
        // degC, p(z=20km) = 54.75 hPa. Temperature gradient is -1.0 K/km.
        double z0 = 20000.;
        double T0 = 216.65;
        double gamma = -1.0e-3;
        double p0 = 5475.006582501095;

        // Hydrostatic equation with linear temperature gradient.
        double p = p0 * pow((T0-gamma*z_m) / (T0-gamma*z0), g/(gamma*R));
        return p;
    }

    else
    {
        // Metre to pressure conversion not implemented for z > 32km.
        return M_MISSING_VALUE;
    }
}


double isaTemperature(double z_m)
{
    if (z_m <= 11000.)
    {
        // ICAO standard atmosphere between 0 and 11 km: T(z=0km) = 15 degC,
        // p(z=0km) = 1013.25 hPa. Temperature gradient is 6.5 K/km.
        double T0 = 288.15;
        double gamma = 6.5e-3;
        return T0-gamma*z_m;
    }

    else if (z_m <= 20000.)
    {
        // ICAO standard atmosphere between 11 and 20 km: T(z=11km) = -56.5 degC,
        // p(z=11km) = 226.32 hPa. Temperature is constant at -56.5 degC.
        double T = 216.65;
        return T;
    }

    else if (z_m <= 32000.)
    {
        // ICAO standard atmosphere between 20 and 32 km: T(z=20km) = -56.5 degC,
        // p(z=20km) = 54.75 hPa. Temperature gradient is -1.0 K/km.
        double z0 = 20000.;
        double T0 = 216.65;
        double gamma = -1.0e-3;
        return T0-gamma*(z_m-z0);
    }

    else
    {
        // ISA temperature from flight level not implemented for z > 32km.
        return M_MISSING_VALUE;
    }
}


double flightlevel2metre(double flightlevel)
{
    // Convert flight level (ft) to m (1 ft = 30.48 cm; 1/0.3048m = 3.28...).
    return flightlevel * 100. / 3.28083989501;
}


double metre2flightlevel(double z_m)
{
    // Convert flight level (ft) to m (1 ft = 30.48 cm; 1/0.3048m = 3.28...).
    return z_m * 3.28083989501 / 100.;
}


double columnAirmass(double pbot_Pa, double ptop_Pa, double area_m2)
{
    // Gravity acceleration (m/s2).
    double g = MetConstants::GRAVITY_ACCELERATION;

    // m*g = dp*A --> m = dp/g * A
    double mass_kg = abs(pbot_Pa - ptop_Pa) / g * area_m2;
    return mass_kg;
}


double boxVolume_dry(double p_Pa, double mass_kg, double temp_K)
{
    // Gas constant for dry air.
    double R_dry = MetConstants::GAS_CONSTANT_DRY_AIR;

    // Ideal gas law, pV = mRT.
    double vol = mass_kg * R_dry * temp_K / p_Pa;
    return vol;
}


double boxVolume_dry(double northWestLon, double northWestLat,
                     double southEastLon, double southEastLat,
                     double pmid_Pa, double pbot_Pa, double ptop_Pa,
                     double temp_K)
{
    double area_km2 = gcQuadrilateralArea(northWestLon, southEastLat,
                                          southEastLon, southEastLat,
                                          northWestLon, northWestLat,
                                          southEastLon, northWestLat,
                                          MetConstants::EARTH_RADIUS_km);

    double area_m2 = area_km2 * 1.E6;
    double mass_kg = columnAirmass(pbot_Pa, ptop_Pa, area_m2);

    if (temp_K == M_MISSING_VALUE)
        temp_K  = isaTemperature(pressure2metre_standardICAO(pmid_Pa));

    return boxVolume_dry(pmid_Pa, mass_kg, temp_K);
}


double windSpeed_ms(double u_ms, double v_ms)
{
    return sqrt(pow(u_ms, 2) + pow(v_ms, 2));
}


double windSpeed3D_ms(double u_ms, double v_ms, double w_ms)
{
    return sqrt(pow(u_ms, 2) + pow(v_ms, 2) + pow(w_ms, 2));
}


double potentialTemperature_K(double T_K, double p_Pa)
{
    return T_K * pow( 100000. / p_Pa,
                      MetConstants::GAS_CONSTANT_DRY_AIR /
                      MetConstants::SPECIFIC_HEAT_DRYAIR_CONST_PRESSURE );
}


double virtualTemperature_K(double T_K, double q_kgkg)
{
    return T_K * (q_kgkg + 0.622*(1.-q_kgkg)) / 0.622;
}


double geopotentialThicknessOfLayer_m(
        double layerMeanVirtualTemperature_K, double p_bot, double p_top)
{
    // Layer mean scale height; Holten 3rd ed. p. 21 (before eq. 1.19):
    //   H = R <Tv> / g0, with <Tv> being the layer mean temperature.
    double layerMeanScaleHeight =
            (MetConstants::GAS_CONSTANT_DRY_AIR
             * layerMeanVirtualTemperature_K) /
            MetConstants::GRAVITY_ACCELERATION;

    // Holton eq. 1.19: Z_T = H * ln(p1/p2)
    return layerMeanScaleHeight * log(p_bot / p_top);
}


double mixingRatio_kgkg(double q_kgkg)
{
    return q_kgkg / (1.-q_kgkg);
}


double specificHumidity_kgkg(double w_kgkg)
{
    return w_kgkg / (1.+w_kgkg);
}


double dewPointTemperature_K_Bolton(double p_Pa, double q_kgkg)
{
    // Compute mixing ratio w from specific humidiy q.
    double w = mixingRatio_kgkg(q_kgkg);

    // Compute vapour pressure from pressure and mixing ratio
    // (Wallace and Hobbs 2nd ed. eq. 3.59).
    double eq = w / (w + 0.622) * p_Pa;

    double td = 243.5 / (17.67 / log(eq / 100. / 6.112) - 1.) + 273.15;

    return td;
}


double equivalentPotentialTemperature_K_Bolton(
        double T_K, double p_Pa, double q_kgkg)
{
    // Compute dewpoint Td in [K].
    const double Td_K = dewPointTemperature_K_Bolton(p_Pa, q_kgkg);

    // Mixing ratio in [kg/kg].
    double r = mixingRatio_kgkg(q_kgkg);
    // Convert r from kg/kg to g/kg.
    r *= 1000;

    // Computation based on Bolton's paper:

    // Eq. 15: temperature at lifting condensation level.
    const double Tl = 1.0 / (1.0 / (Td_K - 56.0) +
                             std::log(T_K / Td_K) / 800.0) + 56.;

    // Eq. 43: equivalent potential temperature (as recommended by Bolton).
    const double thetaW_K = T_K * std::pow(100000. / p_Pa,
                                           0.2854 * (1 - 0.28E-3 * r));

    const double thetaE_K = thetaW_K * std::exp((3.376 / Tl - 2.54E-3) *
                                                r * (1 + 0.81E-3 * r));

    return thetaE_K;
}


float moistAdiabaticLapseRate_K(float T_K, float p_Pa)
{
    float TC = T_K - 273.5f;
    // Heat of vaporization of water.
    float H_v = 1000.f * (2500.8f - 2.36f * TC + 0.0016f
                          * pow(TC, 2.f) - 0.00006f * pow(TC, 3.f));
    // Specific gas constant of dry air.
    float R_sd = 287.05f;
    // Specific gas constant of water vapor.
    float R_sw = 461.51f;
    // The dimensionless ratio of the specific gas constant of dry air to the
    // specific gas constant for water vapor.
    float epsilon = R_sd / R_sw;
    // The water vapor pressure of the saturated air.
    float e =  6.112f * exp(17.67f * TC / (TC + 243.12f)) * 100.f;
    // The mixing ratio of the mass of water vapor to the mass of dry air.
    float r = epsilon * (e / (p_Pa - e));
    // The specific heat of dry air at constant pressure.
    float c_pd = 1004.6f;
    float dividend = 1.f + (H_v * r) / ( R_sd * T_K);
    float divisor = 1.f + (pow(H_v, 2.f) * r * epsilon)
            / ( c_pd * R_sd * pow(T_K, 2.f));
    float tempv = T_K * (1.f + 0.6f * r);
    float Rho = p_Pa / (R_sd * tempv);
    return (dividend / divisor) / (c_pd * Rho);
}


// Test functions for meteorological computations.
namespace MetRoutinesTests
{

void testEQPT()
{
    // Test values from https://www.ncl.ucar.edu/Document/Functions/
    //  Contributed/pot_temp_equiv.shtml.
    // p in [Pa], T in [K], mixing ratio w in [kg/kg].
    float values_p_T_w[27][3] = {
        {97067.80, 291.15, 0.012258},
        {96040.00, 291.60, 0.012111},
        {94825.50, 292.05, 0.011914},
        {93331.30, 292.13, 0.011483},
        {91371.40, 292.06, 0.010575},
        {88947.80, 291.17, 0.008992},
        {86064.70, 289.11, 0.006021},
        {82495.50, 287.49, 0.002559},
        {78140.20, 286.25, 0.005169},
        {73035.40, 282.14, 0.005746},
        {67383.70, 277.42, 0.001608},
        {61327.50, 272.91, 0.001645},
        {54994.70, 266.99, 0.001382},
        {48897.30, 261.64, 0.000235},
        {43034.60, 254.40, 0.000094},
        {37495.20, 246.38, 0.000178},
        {32555.80, 238.10, 0.000136},
        {28124.40, 229.76, 0.000079},
        {24201.00, 220.88, 0.000050},
        {20693.00, 213.65, 0.000025},
        {17600.60, 212.42, 0.000023},
        {14877.30, 212.58, 0.000023},
        {12477.20, 212.91, 0.000014},
        {10400.20, 213.34, 0.000010},
        {8553.98, 213.73, 0.000008},
        {6984.69, 214.55, 0.000007},
        {646.18, 216.59, 0.000007}
    };

    for (int i = 0; i < 27; i++)
    {
        float p_Pa = values_p_T_w[i][0];
        float T_K = values_p_T_w[i][1];
        float w_kgkg = values_p_T_w[i][2];
        float q_kgkg = specificHumidity_kgkg(w_kgkg);
        float eqpt_K = equivalentPotentialTemperature_K_Bolton(T_K, p_Pa, q_kgkg);

        QString s = QString("(%1) p = %2  T = %3  w = %4  q = %5  eqpt = %6")
                .arg(i).arg(p_Pa).arg(T_K).arg(w_kgkg).arg(q_kgkg).arg(eqpt_K);

        LOG4CPLUS_INFO(mlog, s.toStdString());
    }
}

} // namespace MetRoutinesTests

} // namespace Met3D
