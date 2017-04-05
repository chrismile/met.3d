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
#ifndef COLOUR_H
#define COLOUR_H

// standard library imports

// related third party imports

// local application imports

namespace Met3D
{
namespace TFEditor
{

/**
  @brief Color class for storing 8bit rgb colors values.
 */
class MColorRGB8
{
public:
    /**
     * Default constructor. Initializes black color.
     */
    MColorRGB8();
    /**
     * @param rgb Array of red, green and blue component (Range: [0, 255]).
     */
    MColorRGB8(const unsigned char (&rgb)[3]);
    /**
     * @param rgb Array of red, green and blue component (Range: [0, 1]).
     */
    MColorRGB8(const double (&rgb)[3]);
    /**
     * @param R red component (Range: [0, 1]).
     * @param G green component (Range: [0, 1]).
     * @param B blue component (Range: [0, 1]).
     */
    MColorRGB8(double R, double G, double B);

    /**
     * @return red component (Range: [0, 1]).
     */
    double getR() const;
    /**
     * Sets red component.
     * @param R (Range: [0, 1]).
     */
    void setR(double R);

    /**
     * @return green component (Range: [0, 1]).
     */
    double getG() const;
    /**
     * Sets green component.
     * @param G (Range: [0, 1]).
     */
    void setG(double G);

    /**
     * @return blue component (Range: [0, 1]).
     */
    double getB() const;
    /**
     * Sets blue component.
     * @param B (Range: [0, 1]).
     */
    void setB(double B);

    unsigned char r; /**< 8-bit red component (Range: [0, 255])*/
    unsigned char g; /**< 8-bit green component (Range: [0, 255])*/
    unsigned char b; /**< 8-bit blue component (Range: [0, 255])*/
};


/**
  @brief Color class for storing 16bit hcl colors values.
 */
class MColorHCL16
{
public:
    /**
     * Default constructor. Initializes black color.
     */
    MColorHCL16();
    /**
     * @param rgb Array of hue, chroma and luminace component
     * (Range: [-360, 360], [0, 100], [0, 100]).
     */
    MColorHCL16(const double (&hcl)[3]);
    /**
     * @param H hue component (Range: [-360, 360]).
     * @param C chroma component (Range: [0, 100]).
     * @param L luminance component (Range: [0, 100]).
     */
    MColorHCL16(double H, double C, double L);

    /**
     * @return hue component (Range: [-360, 360]).
     */
    double getH() const;
    /**
     * Sets hue component.
     * @param H (Range: [-360, 360]).
     */
    void setH(double H);

    /**
     * @return chroma component (Range: [0, 100])
     */
    double getC() const;
    /**
     * Sets chroma component.
     * @param C (Range: [0, 100]).
     */
    void setC(double C);

    /**
     * @return luminance component (Range: [0, 100]).
     */
    double getL() const;
    /**
     * Sets luminance component.
     * @param L (Range: [0, 100]).
     */
    void setL(double L);

    short h; /**< 16-bit hue component (Range: [-32767, 32767])*/
    unsigned short c; /**< 16-bit red component (Range: [0, 65535])*/
    unsigned short l; /**< 16-bit red component (Range: [0, 65535])*/
};


/**
  @brief Color class for storing 64bit xyz colors values.
 */
class MColorXYZ64
{
public:
    /**
     * Default constructor. Initializes color at D65 whitepoint.
     */
    MColorXYZ64();
    bool operator==(const MColorXYZ64& rhs) const;
    bool operator!=(const MColorXYZ64& rhs) const;

    /**
     * ColorRGB8 to ColorXYZ64 conversion.
     */
    explicit MColorXYZ64(const MColorRGB8& rgb);
    /**
     * ColorHCL16 to ColorXYZ64 conversion.
     */
    explicit MColorXYZ64(const MColorHCL16& hcl);

    /**
     * ColorXYZ64 to ColorRGB8 conversion.
     *
     * Same as ColorXYZ64::toRGB but with fixed e = 1e-4.
     */
    explicit operator MColorRGB8() const;

    /**
     * ColorXYZ64 to ColorHCL16 conversion.
     *
     * Same as ColorXYZ64::toHCL but with fixed e = 1.0.
     */
    explicit operator MColorHCL16() const;

    /**
     * ColorXYZ64 to ColorRGB8 conversion. RGB colors with values outside the
     * range of [0 - e, 1 + e] will be mapped to black.
     * @param e maximum error.
     */
    MColorRGB8 toRGB(double e) const;
    /**
     * ColorXYZ64 to ColorHCL16 conversion. HCL colors with values outside the
     * range of [-360 - e, 360 + e] or [0 - e, 100 + e] will be mapped to black.
     * @param e maximum error.
     */
    MColorHCL16 toHCL(double e) const;

    double x;
    double y;
    double z;
    /**
     * Flag to indicate whether the hue value of the original HCL color was
     * negative.
     *
     * Only used for conversions between ColorHCL16 <--> ColorXYZ64.
     */
    bool hueSgn;
};

/**
 * @brief Linear componentwise interpolation between two ColorRGB8's.
 * @param c1 The first color.
 * @param c2 The second color.
 * @param pos Position at which the interpolation is evaluated (Range: [0, 1]).
 * @return ColorRGB8 between c1 and c2 at position pos.
 */
MColorRGB8 lerp(const MColorRGB8& c1, const MColorRGB8& c2, float pos);

/**
 * @brief Linear componentwise interpolation between two ColorHCL16's.
 * @param c1 The first color.
 * @param c2 The second color.
 * @param pos Position at which the interpolation is evaluated, (Range: [0, 1]).
 * @return ColorHCL16 between c1 and c2 at position pos.
 */
MColorHCL16 lerp(const MColorHCL16& c1, const MColorHCL16& c2, float pos);

/**
 * @brief Linear componentwise interpolation between two ColorXYZ64's.
 * @param c1 The first color.
 * @param c2 The second color.
 * @param pos Position at which the interpolation is evaluated, (Range: [0, 1]).
 * @return ColorXYZ64 between c1 and c2 at position pos.
 */
MColorXYZ64 lerp(const MColorXYZ64& c1, const MColorXYZ64& c2, float pos);

/**
 * @brief Linear interpolation between two ColorXYZ64's, based on their
 * RGB representation.
 * @param c1 The first color.
 * @param c2 The second color.
 * @param pos Position at which the interpolation is evaluated, (Range: [0, 1]).
 * @return ColorXYZ64 between (ColorRGB8)c1 and (ColorRGB8)c2 at position pos.
 */
MColorXYZ64 lerpRGB(const MColorXYZ64& c1, const MColorXYZ64& c2, float pos);

/**
 * @brief Linear interpolation between two ColorXYZ64's, based on their
 * HCL representation.
 * @param c1 The first color.
 * @param c2 The second color.
 * @param pos Position at which the interpolation is evaluated, (Range: [0, 1]).
 * @return ColorXYZ64 between (ColorHCL16)c1 and (ColorHCL16)c2 at position pos.
 */
MColorXYZ64 lerpHCL(const MColorXYZ64& c1, const MColorXYZ64& c2, float pos);


} // namespace TFEditor
} // namespace Met3D

#endif // COLOUR_H
