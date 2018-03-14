/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Fabian Schöttl
**  Copyright 2017 Bianca Tost
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
#include "colour.h"

// standard library imports
#include <assert.h>
#include <cmath>

// related third party imports

// local application imports
#include "gxfw/colourmap.h"

const double Xn = 95.047; /* Use D65 by default. */
const double Yn = 100.000;
const double Zn = 108.883;

namespace Met3D
{
namespace TFEditor
{

/******************************************************************************
***                               MColorRGB8                                ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/
MColourRGB8::MColourRGB8() :
    r(0),
    g(0),
    b(0)
{}


MColourRGB8::MColourRGB8(const unsigned char (&rgb)[3]) :
    r(rgb[0]),
    g(rgb[1]),
    b(rgb[2])
{}


MColourRGB8::MColourRGB8(const double (&rgb)[3])
{
    setR(rgb[0]);
    setG(rgb[1]);
    setB(rgb[2]);
}


MColourRGB8::MColourRGB8(double R, double G, double B)
{
    setR(R);
    setG(G);
    setB(B);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

double MColourRGB8::getR() const
{
    return (double)r / 255;
}


void MColourRGB8::setR(double R)
{
    R = std::min(std::max(0.0, R), 1.0);
    r = (int)(std::round(R * 255));
}


double MColourRGB8::getG() const
{
    return (double)g / 255;
}


void MColourRGB8::setG(double G)
{
    G = std::min(std::max(0.0, G), 1.0);
    g = (int)(std::round(G * 255));
}


double MColourRGB8::getB() const
{
    return (double)b / 255;
}


void MColourRGB8::setB(double B)
{
    B = std::min(std::max(0.0, B), 1.0);
    b = (int)(std::round(B * 255));
}


/******************************************************************************
***                               MColorHCL16                               ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MColourHCL16::MColourHCL16() :
    h(0),
    c(0),
    l(0)
{}


MColourHCL16::MColourHCL16(const double (&hcl)[3])
{
    setH(hcl[0]);
    setC(hcl[1]);
    setL(hcl[2]);
}


MColourHCL16::MColourHCL16(double H, double C, double L)
{
    setH(H);
    setC(C);
    setL(L);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

double MColourHCL16::getH() const
{
    return (double)h / 32767 * 360;
}


void MColourHCL16::setH(double H)
{
    H = std::min(std::max(-360.0, H), 360.0);
    h = std::floor(H / 360 * 32767);
}


double MColourHCL16::getC() const
{
    return (double)c / 65535 * 100;
}


void MColourHCL16::setC(double C)
{
    C = std::min(std::max(0.0, C), 100.0);
    c = std::floor(C / 100 * 65535);
}


double MColourHCL16::getL() const
{
    return (double)l / 65535 * 100;
}


void MColourHCL16::setL(double L)
{
    L = std::min(std::max(0.0, L), 100.0);
    l = std::floor(L / 100 * 65535);
}


/******************************************************************************
***                               MColorXYZ64                               ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MColourXYZ64::MColourXYZ64() :
    x(Xn),
    y(Yn),
    z(Zn),
    hueSgn(true)
{}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

bool MColourXYZ64::operator==(const MColourXYZ64& rhs) const
{
    return memcmp(this, &rhs, sizeof(MColourXYZ64)) == 0;
}


bool MColourXYZ64::operator!=(const MColourXYZ64& rhs) const
{
    return memcmp(this, &rhs, sizeof(MColourXYZ64)) != 0;
}


// Colorconversion based on colorspace.c from
// https://cran.r-project.org/web/packages/colorspace/index.html
MColourXYZ64::MColourXYZ64(const MColourRGB8& rgb)
{
    Colourspace::sRGB_to_XYZ(rgb.getR(), rgb.getG(), rgb.getB(), Xn, Yn, Zn, &x,
                             &y, &z);
}


MColourXYZ64::MColourXYZ64(const MColourHCL16& hcl)
{
    double L_, U_, V_;
    Colourspace::polarLUV_to_LUV(hcl.getL(), hcl.getC(), hcl.getH(), &L_, &U_,
                                 &V_);

    Colourspace::LUV_to_XYZ(L_, U_, V_, Xn, Yn, Zn, &x, &y, &z);
    hueSgn = hcl.h >= 0;
}


MColourXYZ64::operator MColourRGB8() const
{
    return toRGB(1e-4);
}


MColourXYZ64::operator MColourHCL16() const
{
    return toHCL(1);
}


MColourRGB8 MColourXYZ64::toRGB(double e) const
{
    double R, G, B;
    Colourspace::XYZ_to_sRGB(x, y, z, Xn, Yn, Zn, &R, &G, &B);


    double r = std::min(std::max(0.0, R), 1.0);
    double g = std::min(std::max(0.0, G), 1.0);
    double b = std::min(std::max(0.0, B), 1.0);

    if ((std::abs(R - r) > e) || (std::abs(G - g) > e) || (std::abs(B - b) > e))
    {
        r = g = b = 0;
    }

    return MColourRGB8(r, g, b);
}


MColourHCL16 MColourXYZ64::toHCL(double e) const
{
    double L_, U_, V_;
    Colourspace::XYZ_to_LUV(x, y, z, Xn, Yn, Zn, &L_, &U_, &V_);

    double H, C, L;
    Colourspace::LUV_to_polarLUV(L_, U_, V_, &L, &C, &H);


    double h = std::min(std::max(-360.0, H), 360.0);
    double c = std::min(std::max(0.0, C), 100.0);
    double l = std::min(std::max(0.0, L), 100.0);

    if ((std::abs(H - h) > e * 360) || (std::abs(C - c) > e * 100)
            || (std::abs(L - l) > e * 100))
    {
        h = c = l = 0;
    }

    if (!hueSgn)
    {
        h -= 360;
    }

    return MColourHCL16(h, c, l);
}


/******************************************************************************
***                                 Colours                                 ***
*******************************************************************************/

MColourRGB8 lerp(const MColourRGB8& c1, const MColourRGB8& c2, float pos)
{
    MColourRGB8 result;
    pos = std::min(std::max(0.f, pos), 1.f);
    result.setR(c1.getR() * (1 - pos) + c2.getR() * pos);
    result.setG(c1.getG() * (1 - pos) + c2.getG() * pos);
    result.setB(c1.getB() * (1 - pos) + c2.getB() * pos);
    return result;
}


MColourHCL16 lerp(const MColourHCL16& c1, const MColourHCL16& c2, float pos)
{
    MColourHCL16 result;
    pos = std::min(std::max(0.f, pos), 1.f);
    result.setH(c1.getH() * (1 - pos) + c2.getH() * pos);
    result.setC(c1.getC() * (1 - pos) + c2.getC() * pos);
    result.setL(c1.getL() * (1 - pos) + c2.getL() * pos);
    return result;
}


MColourXYZ64 lerp(const MColourXYZ64& c1, const MColourXYZ64& c2, float pos)
{
    MColourXYZ64 result;
    pos = std::min(std::max(0.f, pos), 1.f);
    result.x = c1.x * (1 - pos) + c2.x * pos;
    result.y = c1.y * (1 - pos) + c2.y * pos;
    result.z = c1.z * (1 - pos) + c2.z * pos;
    return result;
}


MColourXYZ64 lerpRGB(const MColourXYZ64& c1, const MColourXYZ64& c2, float pos)
{
    return MColourXYZ64(lerp((MColourRGB8)c1, (MColourRGB8)c2, pos));
}


MColourXYZ64 lerpHCL(const MColourXYZ64& c1, const MColourXYZ64& c2, float pos)
{
    return MColourXYZ64(lerp((MColourHCL16)c1, (MColourHCL16)c2, pos));
}


} // namespace TFEditor
} // namespace Met3D
