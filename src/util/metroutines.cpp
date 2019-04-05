/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2019 Marc Rautenhaus [*, previously +]
**  Copyright 2015-2018 Michael Kern [+]
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


double ambientTemperatureOfPotentialTemperature_K(double theta_K, double p_Pa)
{
    return theta_K / pow( 100000. / p_Pa,
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


const double coefficients_ambientT_MoisseevaStull[11][21] = {
    {-4.559811259892786989e-49, -1.612607116252546465e-46, -2.025298194696236504e-44, -6.656052926567021360e-43, 7.840358416489628333e-41, 7.635048211278595187e-39, 7.859031280030359095e-38, -1.735458962185854857e-35, -7.051644435298396131e-34, 8.656975041268894382e-33, 1.052019706721087690e-30, 1.324364900285066085e-29, -4.670619298819146183e-28, -1.389203299626408788e-26, -7.683027529023923257e-26, 1.311828666829898514e-24, 4.952846494858583883e-23, 1.242755995082067048e-21, 9.927197312050702977e-21, -1.041821390650925685e-19, -3.056910341802732861e-18},
    {6.034492102739373863e-46, 2.138330162706446697e-43, 2.695771434371917479e-41, 9.021770033439766060e-40, -1.027150857716580263e-37, -1.014699622194909069e-35, -1.111389618374632332e-34, 2.275262294144775882e-32, 9.418541011369929690e-31, -1.076159204687636640e-29, -1.383845892945539557e-27, -1.804926357613298134e-26, 5.990132759494420593e-25, 1.825851792314843773e-23, 1.068879436033122841e-22, -1.561194629254354370e-21, -6.232382243485494639e-20, -1.626866604145226396e-18, -1.382095015569503365e-17, 1.260007302438173007e-16, 4.109621837440803852e-15},
    {-3.530946785429015482e-43, -1.253755580029747476e-40, -1.586822945834642032e-38, -5.409193240313579614e-37, 5.945966212050050156e-35, 5.962995930500440654e-33, 6.937050722825804452e-32, -1.318159044470374801e-29, -5.562826084609257269e-28, 5.876301031590925883e-27, 8.045762030333527196e-25, 1.087250129818108576e-23, -3.391204832856865912e-22, -1.060798785982980591e-20, -6.550720513211737081e-20, 8.146503892387442853e-19, 3.458387671798357012e-17, 9.392092205708831296e-16, 8.440070353299219670e-15, -6.698441796768596834e-14, -2.446236943345063927e-12},
    {1.201663937381821286e-40, 4.275891157710672546e-38, 5.433824477792969252e-36, 1.887089066381153818e-34, -2.000698041519712906e-32, -2.038324703034061754e-30, -2.513978891144084443e-29, 4.439252450632079811e-27, 1.911179560535656955e-25, -1.852345490248510748e-24, -2.719942504239597327e-22, -3.807893627278250222e-21, 1.114870990768946890e-19, 3.584337747119361091e-18, 2.327552074724496587e-17, -2.450719241785481003e-16, -1.113947280254851360e-14, -3.146622986365664955e-13, -2.975899188168124653e-12, 2.061708092431306180e-11, 8.487001322143450624e-10},
    {-2.631271074409739616e-38, -9.383464830154431525e-36, -1.197447169972476560e-33, -4.237200064507488289e-32, 4.328758140816126412e-30, 4.483402477606947765e-28, 5.851138508598203773e-27, -9.614247256874849794e-25, -4.225127152689072616e-23, 3.724688052803381730e-22, 5.914715931651838541e-20, 8.577418028419124502e-19, -2.354642263630730233e-17, -7.793086367471092636e-16, -5.306609665817956188e-15, 4.699113127575865639e-14, 2.305794426902790606e-12, 6.770904285930588496e-11, 6.707124997918236866e-10, -4.070649835833764959e-09, -1.899810075474156997e-07},
    {3.869505176380883434e-36, 1.383043499752013702e-33, 1.772500233116854908e-31, 6.390802522616563328e-30, -6.286299303029825332e-28, -6.623339509748438407e-26, -9.127990357650681451e-25, 1.397711680978416472e-22, 6.273554409328814629e-21, -4.980044743256623205e-20, -8.636396374952953163e-18, -1.297005863672514386e-16, 3.335148822422769713e-15, 1.138219967224252186e-13, 8.107164124042226954e-13, -5.989538420101499665e-12, -3.204521913144606138e-10, -9.772944660480069612e-09, -1.009464011712373137e-07, 5.390893620568617522e-07, 2.865682244236896384e-05},
    {-3.866436879293449094e-34, -1.385153888430247099e-31, -1.782948201885285445e-29, -6.549596602434674052e-28, 6.199491639703019393e-26, 6.648610235215744638e-24, 9.655100061126788407e-23, -1.380072324434665346e-20, -6.329450447627865584e-19, 4.471801599413662214e-18, 8.567364733589076497e-16, 1.331918029326805624e-14, -3.205637596287700136e-13, -1.130042926415171582e-11, -8.400657425932391605e-11, 5.127809938438622188e-10, 3.028225085879620307e-08, 9.578568114506970212e-07, 1.027372554005282298e-05, -4.853919673395682040e-05, -2.948086259154645162e-03},
    {2.589651667883628314e-32, 9.299352330474301678e-30, 1.202274416376926911e-27, 4.498718046530294933e-26, -4.096267521528509599e-24, -4.473714148516136996e-22, -6.829917386984084271e-21, 9.130918537882259985e-19, 4.280436623564242026e-17, -2.654758387046091768e-16, -5.696733023832867220e-14, -9.163186027042358594e-13, 2.063047335086734507e-11, 7.524849363119860331e-10, 5.826827002923419347e-09, -2.903205555747877005e-08, -1.921499867034166968e-06, -6.292750810575006317e-05, -6.981269403323539513e-04, 2.937124780861264262e-03, 2.040996375632872784e-01},
    {-1.111824539607293497e-30, -4.002041218341050628e-28, -5.196934357373460716e-26, -1.980075177516220407e-24, 1.734306879353562292e-22, 1.929597686005690354e-20, 3.089271423450802587e-19, -3.871612839848997572e-17, -1.855447768118277439e-15, 9.936571668246925608e-15, 2.428284326813376501e-12, 4.038369850830650855e-11, -8.503200450925746524e-10, -3.214287499141482665e-08, -2.587878738294326252e-07, 1.036261971706362737e-06, 7.836272166830651374e-05, 2.651375091514224236e-03, 3.031426330224842175e-02, -1.144475935860155280e-01, -9.093075439410078786e+00},
    {2.761379267614784989e-29, 9.963362982220561087e-27, 1.299492535041084171e-24, 5.038871418723494176e-23, -4.246730297089256672e-21, -4.814417803732415436e-19, -8.061699057251630779e-18, 9.495526532271356289e-16, 4.652164228918535867e-14, -2.107993296009125947e-13, -5.988800563456756607e-11, -1.028837177511503214e-09, 2.026152696620013076e-08, 7.949274843402147707e-07, 6.642842441505273217e-06, -2.094081749552657745e-05, -1.855024654454983952e-03, -6.468473467340879468e-02, -7.594983750757552388e-01, 2.605414576815425765e+00, 2.366128018687736585e+02},
    {-3.011673035238626136e-28, -1.089213985875472878e-25, -1.426765921179245547e-23, -5.626677336132666539e-22, 4.565981448850560423e-20, 5.274512760964740057e-18, 9.211975789529499516e-17, -1.022706672408165679e-14, -5.121279414943311622e-13, 1.913159906557939435e-12, 6.487554478960031827e-10, 1.150054907386370992e-08, -2.119308034724405346e-07, -8.640555345227696879e-06, -7.481297487060015400e-05, 1.804306690039786853e-04, 1.936106352626234053e-02, 6.939521375153610006e-01, 8.343447343125914628e+00, -2.608553450529006312e+01, -2.680155514087249685e+03}
};

const double coefficients_ambientT_thetaRef_MoisseevaStull[21] = {
    -1.827763290255405026e-32,
    1.473033277342402879e-29,
    -5.060609629885724477e-27,
    9.239398940420452573e-25,
    -8.295179743597802282e-23,
    -2.599710334090416779e-22,
    8.864438810025267090e-19,
    -7.612129526157592821e-17,
    -2.569265180318347300e-15,
    1.155424475624293460e-12,
    -1.286433950921489119e-10,
    8.686876477370842123e-09,
    -4.056687350784324676e-07,
    1.364980925792754869e-05,
    -3.345521623827504799e-04,
    5.945366793740703912e-03,
    -7.547341231171930676e-02,
    6.703518008164152109e-01,
    -4.123539935862701178e+00,
    1.998458920416675255e+01,
    3.833143316160573733e+01
};

double temperatureAlongSaturatedAdiabat_K_MoisseevaStull(double thetaW_K,
                                                         double p_Pa)
{
    // This implementation directly follows Moisseeva and Stull (ACP, 2017),
    // Section 4.1.
    const int nmax = 10;
    const int m = 20;
    double k[nmax+1];

    double thetaW_degC = thetaW_K - 273.15;
    double p_kPa = p_Pa / 1000.;

    // Moisseeva and Stull Eq. 1.
    for (int n = 0; n <= nmax; n++)
    {
        k[n] = 0.;
        for (int i = 0; i <= m; i++)
        {
            k[n] += coefficients_ambientT_MoisseevaStull[n][i] *
                    pow(thetaW_degC, double(m-i));
        }
    }

    // Moisseeva and Stull Eq. 2.
    double thetaRef_K = 0.;
    for (int j = 0; j <= m; j++)
    {
        thetaRef_K += coefficients_ambientT_thetaRef_MoisseevaStull[j] *
                pow(p_kPa, double(m-j));
    }

    // Moisseeva and Stull Eq. 3.
    double ambientT_K = 0.;
    for (int h = 0; h <= nmax; h++)
    {
        ambientT_K += k[h] * pow(thetaRef_K, double(nmax-h));
    }

    return ambientT_K;
}


const double coefficients_thetaW_MoisseevaStull[11][21] = {
    {-8.940638683600110403e-52, -5.649952302285138055e-49, -1.487346077046662894e-46, -2.020340194595892171e-44, -1.289886316326141203e-42, 7.142197964796062595e-42, 6.835970020730250036e-39, 3.690205570239234936e-37, -3.259546404643971167e-36, -9.920626838273853147e-34, -2.414496622803375773e-32, 8.474597004915692312e-31, 4.385861748280573546e-29, -2.270108823759553412e-31, -2.637482080358998117e-26, -3.273589583508000533e-25, 4.090371987114118800e-24, 1.471915092406395154e-22, -7.455357021835227511e-22, -9.802291876538176218e-21, 1.205141251756201911e-18},
    {-3.025421635669379775e-49, -1.907973457576235857e-46, -5.009539578414177345e-44, -6.778113284560276125e-42, -4.290560163127459580e-40, 2.834159015922900360e-39, 2.304965874224545506e-36, 1.223683972265512268e-34, -1.201390937819225469e-33, -3.325590953062318006e-31, -7.876871399651327517e-30, 2.878023613887928790e-28, 1.455415500164499124e-26, -3.978712452931374329e-27, -8.948442631827862606e-24, -1.074106910525771624e-22, 1.629910678204743188e-21, 4.437548690049338298e-20, -3.252374450453319552e-19, 7.173887173924613352e-20, 4.226613546764778891e-16},
    {-3.590124471395968020e-47, -2.255138574193590901e-44, -5.890866731582641643e-42, -7.909966008958855478e-40, -4.923434952626052882e-38, 4.291535751183542503e-37, 2.715597504642270022e-34, 1.396290822239223049e-32, -1.640808207836564097e-31, -3.878088591727225045e-29, -8.706819992225020346e-28, 3.448525217684722569e-26, 1.664320009878387843e-24, -1.635136884376398269e-24, -1.060003111086483875e-21, -1.169373077215002314e-20, 2.289522416507621925e-19, 4.264050089394370333e-18, -4.594631711888814200e-17, 5.339890072568268287e-16, 5.655856090630035240e-14},
    {-1.373926422308711559e-45, -8.509946007301117540e-43, -2.182394909160870911e-40, -2.849063826880511101e-38, -1.661302507878138370e-36, 2.858244685896165664e-35, 1.012568606803559737e-32, 4.615183938079197325e-31, -9.060029298709493232e-30, -1.398198830905558125e-27, -2.509601340965689691e-26, 1.376655035549352936e-24, 5.559637015734693785e-23, -2.445089249992316038e-22, -3.950438254845161239e-20, -2.892660443869750864e-19, 1.171524164314285227e-17, 6.022668386119112789e-17, -2.431969695028251880e-15, 8.344630197727488351e-14, 3.513486170664082423e-12},
    {4.697528890717645178e-44, 3.053752753969444555e-41, 8.323103686405698284e-39, 1.186603077069213401e-36, 8.330761366125827552e-35, 4.611729174213548355e-34, -3.778308753917730526e-31, -2.433275105790785049e-29, -1.739287904598259822e-29, 5.754424198994494164e-26, 1.817124783143767195e-24, -3.923031993493344124e-23, -2.845889978896253071e-21, -1.494258018218542533e-20, 1.523984114600190333e-18, 2.941771724548768934e-17, -1.297141751504904074e-16, -1.307650374830792442e-14, 2.585126889451515691e-15, 6.429390884608900401e-12, 9.156711370042299756e-11},
    {4.458003827579126480e-42, 2.815613869303576127e-39, 7.405849662747356100e-37, 1.004386161177640799e-34, 6.386008750099562885e-33, -3.862204207187799241e-32, -3.398394362795275986e-29, -1.812487517297141945e-27, 1.708649918771909786e-26, 4.869718202593517441e-24, 1.166681763335792162e-22, -4.110992554041358188e-21, -2.148093647745313037e-19, -1.493628746654474764e-19, 1.357641570096482788e-16, 1.712416820065072256e-15, -2.963464958322094563e-14, -6.498420460309604892e-13, 5.435313205544431723e-12, 3.615490574261750060e-10, 3.131478031513005300e-09},
    {3.344056745296919994e-41, 1.968872047664033548e-38, 4.700872580882837826e-36, 5.431435913555943561e-34, 2.182906743264120332e-32, -1.671992171519192483e-30, -2.189424906715156330e-28, -4.964965635029127440e-27, 4.375044636594485959e-25, 2.547296192431481326e-23, -8.213064636996389266e-23, -3.601349405929277684e-20, -6.397543527764100919e-19, 1.940780285617519961e-17, 8.442048652617385294e-16, -2.730674554356892807e-15, -5.228984716019944003e-13, -1.941955209045107304e-12, 2.703370380141084731e-10, 2.355802200503431582e-08, 4.903624202702267461e-07},
    {-3.194206475765971365e-39, -2.046958484557529342e-36, -5.481518334201288231e-34, -7.623379798365699523e-32, -5.097122999941089170e-30, 8.136025708563864161e-31, 2.510931247278433882e-26, 1.467082276602229411e-24, -6.380416644157642449e-24, -3.700021464509517363e-21, -1.017702233697502408e-19, 2.826769187155087684e-18, 1.721014818757093639e-16, 5.132716469624444971e-16, -9.998444977000884066e-14, -1.422044744692930745e-12, 1.223756875003864647e-11, 2.217788566910524242e-10, 1.735447658947586790e-08, 1.107945723433615397e-06, 6.152426892781532198e-05},
    {-4.024789067876858153e-38, -2.498733601389557520e-35, -6.425737652656297818e-33, -8.419859321801628539e-31, -4.952142544048751167e-29, 7.826418696109355956e-28, 2.945023333396272782e-25, 1.350101631639290053e-23, -2.536952318718131370e-22, -3.978612500948899663e-20, -7.194378253942418895e-19, 3.778386109282431694e-17, 1.542090662036356328e-15, -5.580569396039821215e-15, -9.991581955893650593e-13, -5.817806142832035731e-12, -1.059217894919622279e-11, -6.396650149204728994e-09, 8.594594229811387659e-07, 6.071043372145297170e-05, 2.593963103118521445e-03},
    {5.595510186532496240e-37, 3.647528206694947255e-34, 9.968658037807449110e-32, 1.424491368014597878e-29, 1.000364299062153141e-27, 5.050862964655285742e-27, -4.611308251049928384e-24, -2.949679684176437764e-22, -1.846922471799829635e-24, 7.132120743729025976e-19, 2.229153757741498027e-17, -5.153188993986375867e-16, -3.732365445829397839e-14, -1.331701802164345991e-13, 2.725155733871478009e-11, 2.948069570986472936e-10, -2.089385195488617571e-08, -2.930432546485080457e-07, 8.037246767279610134e-05, 1.246166301673238847e-03, 3.262118772357716079e-01},
    {3.698053839511203536e-36, 2.351239750799062160e-33, 6.230257222164064360e-31, 8.514018535926191911e-29, 5.440440178849733905e-27, -3.842682335816372042e-26, -3.012845853942604151e-23, -1.598581819701335177e-21, 1.959555922636722869e-20, 4.748546131518730344e-18, 1.029451145393159608e-16, -5.768678668778368361e-15, -2.574591440185538047e-13, 5.705096243361321691e-12, 4.292057912253817629e-10, -1.289263101831566130e-08, -1.266415219630790837e-06, 5.026000763706412079e-05, 7.415996644020345085e-03, 5.502041978907826758e-01, 4.423933525831689906e+01}
};

const double coefficients_thetaW_TRef_MoisseevaStull[21] = {
    1.306864907379971575e-33,
    -1.131768149868550176e-30,
    4.223554905869480713e-28,
    -8.545248788753147515e-26,
    9.033798532858421644e-24,
    -1.617352539048255700e-22,
    -8.402223134087994960e-20,
    9.837875105302343364e-18,
    2.066257060992322145e-17,
    -1.229512756539031117e-13,
    1.662874856253646964e-11,
    -1.287117116898725131e-09,
    6.770739132071150343e-08,
    -2.539144597018173791e-06,
    6.865048779848125643e-05,
    -1.331835778840032578e-03,
    1.835211150846765768e-02,
    -1.779012997812081665e-01,
    1.191126625127650440e+00,
    -7.543463795790533943e+00,
    5.641500716650050151e+01
};


double wetBulbPotentialTemperatureOfSaturatedAdiabat_K_MoisseevaStull(
        double T_K, double p_Pa)
{
    // This implementation directly follows Moisseeva and Stull (ACP, 2017),
    // Section 4.2.
    const int nmax = 10;
    const int m = 20;
    double kappa[nmax+1];

    double T_degC = T_K - 273.15;
    double p_kPa = p_Pa / 1000.;

    // Moisseeva and Stull Eq. 4.
    for (int n = 0; n <= nmax; n++)
    {
        kappa[n] = 0.;
        for (int i = 0; i <= m; i++)
        {
            kappa[n] += coefficients_thetaW_MoisseevaStull[n][i] *
                    pow(T_degC, double(m-i));
        }
    }

    // Moisseeva and Stull Eq. 5.
    double TRef_degC = 0.;
    for (int j = 0; j <= m; j++)
    {
        TRef_degC += coefficients_thetaW_TRef_MoisseevaStull[j] *
                pow(p_kPa, double(m-j));
    }

    // Moisseeva and Stull Eq. 6.
    double thetaW_degC = 0.;
    for (int h = 0; h <= nmax; h++)
    {
        thetaW_degC += kappa[h] * pow(TRef_degC, double(nmax-h));
    }

    return thetaW_degC + 273.15;
}


// Test functions for meteorological computations.
namespace MetRoutinesTests
{

void test_EQPT()
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


void test_temperatureAlongSaturatedAdiabat_K_MoisseevaStull()
{
    LOG4CPLUS_INFO(mlog, "Running test for temperatureAlongSaturatedAdiabat_"
                         "K_MoisseevaStull().");

    const int nTests = 10;

    // Test values (p_kPa, thetaW_degC, T_degC); computed using the spreadsheet
    // in the Supplement of Moisseeva and Stull (2017, ACP).
    // https://www.atmos-chem-phys.net/17/15037/2017/acp-17-15037-2017.html
    float values_p_thW_T[nTests][3] = {
        {100, -50,  -50.00},
        { 80, -50,  -63.71},
        { 50, -50,  -89.98},
        { 30, -50, -114.82},
        { 20, -50, -132.14},
        { 50, -30,  -72.97},
        { 50, -10,  -53.43},
        { 50,   0,  -41.08},
        { 50,  10,  -25.86},
        { 50,  30,    7.07},
    };

    for (int i = 0; i < nTests; i++)
    {
        double p_Pa = values_p_thW_T[i][0] * 1000.;
        double thetaW_K = values_p_thW_T[i][1] + 273.15;
        double ambientT_K_target = values_p_thW_T[i][2] + 273.15;
        double ambientT_K_computed =
                temperatureAlongSaturatedAdiabat_K_MoisseevaStull(thetaW_K, p_Pa);

        QString s = QString("(%1) p_Pa = %2  thetaW_K = %3  target T_K = %4  computed T_K = %5")
                .arg(i).arg(p_Pa).arg(thetaW_K).arg(ambientT_K_target).arg(ambientT_K_computed);

        LOG4CPLUS_INFO(mlog, s.toStdString());
    }

    LOG4CPLUS_INFO(mlog, "Test finished.");
}


void test_wetBulbPotentialTemperatureOfSaturatedAdiabat_K_MoisseevaStull()
{
    LOG4CPLUS_INFO(mlog, "Running test for wetBulbPotentialTemperatureOf"
                         "SaturatedAdiabat_K_MoisseevaStull().");

    const int nTests = 10;

    // Test values (p_kPa, T_degC, thetaW_degC); computed using the spreadsheet
    // in the Supplement of Moisseeva and Stull (2017, ACP).
    // https://www.atmos-chem-phys.net/17/15037/2017/acp-17-15037-2017.html
    float values_p_T_thW[nTests][3] = {
        {100, -50, -50.00},
        { 80, -50, -35.67},
        { 50, -50,  -6.86},
        { 30, -50,  14.40},
        { 20, -50,  24.94},
        { 50, -30,   7.71},
        { 50, -10,  19.41},
        { 50,   0,  25.38},
        { 50,  10,  32.01},
        { 50,  30,  47.91},
    };

    for (int i = 0; i < nTests; i++)
    {
        double p_Pa = values_p_T_thW[i][0] * 1000.;
        double T_K = values_p_T_thW[i][1] + 273.15;
        double thetaW_K_target = values_p_T_thW[i][2] + 273.15;
        double thetaW_K_computed =
                wetBulbPotentialTemperatureOfSaturatedAdiabat_K_MoisseevaStull(
                    T_K, p_Pa);

        QString s = QString("(%1) p_Pa = %2  T_K = %3  target thetaW_K = %4  computed thetaW_K = %5")
                .arg(i).arg(p_Pa).arg(T_K).arg(thetaW_K_target).arg(thetaW_K_computed);

        LOG4CPLUS_INFO(mlog, s.toStdString());
    }

    LOG4CPLUS_INFO(mlog, "Test finished.");
}


void runMetRoutinesTests()
{
    test_temperatureAlongSaturatedAdiabat_K_MoisseevaStull();
    test_wetBulbPotentialTemperatureOfSaturatedAdiabat_K_MoisseevaStull();
}

}

// namespace MetRoutinesTests

} // namespace Met3D
