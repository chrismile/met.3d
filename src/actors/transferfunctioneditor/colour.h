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
  @brief Colour class for storing 8bit rgb colors values.
 */
class MColourRGB8
{
public:
    /**
     * Default constructor. Initializes black colour.
     */
    MColourRGB8();
    /**
     * @param rgb Array of red, green and blue component (Range: [0, 255]).
     */
    MColourRGB8(const unsigned char (&rgb)[3]);
    /**
     * @param rgb Array of red, green and blue component (Range: [0, 1]).
     */
    MColourRGB8(const double (&rgb)[3]);
    /**
     * @param R red component (Range: [0, 1]).
     * @param G green component (Range: [0, 1]).
     * @param B blue component (Range: [0, 1]).
     */
    MColourRGB8(double R, double G, double B);

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
  @brief Colour class for storing 16bit hcl colours values.
 */
class MColourHCL16
{
public:
    /**
     * Default constructor. Initializes black colour.
     */
    MColourHCL16();
    /**
     * @param rgb Array of hue, chroma and luminace component
     * (Range: [-360, 360], [0, 100], [0, 100]).
     */
    MColourHCL16(const double (&hcl)[3]);
    /**
     * @param H hue component (Range: [-360, 360]).
     * @param C chroma component (Range: [0, 100]).
     * @param L luminance component (Range: [0, 100]).
     */
    MColourHCL16(double H, double C, double L);

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
  @brief Colour class for storing 64bit xyz colours values.
 */
class MColourXYZ64
{
public:
    /**
     * Default constructor. Initializes colour at D65 whitepoint.
     */
    MColourXYZ64();
    bool operator==(const MColourXYZ64& rhs) const;
    bool operator!=(const MColourXYZ64& rhs) const;

    /**
     * ColorRGB8 to ColorXYZ64 conversion.
     */
    explicit MColourXYZ64(const MColourRGB8& rgb);
    /**
     * ColorHCL16 to ColorXYZ64 conversion.
     */
    explicit MColourXYZ64(const MColourHCL16& hcl);

    /**
     * ColorXYZ64 to ColorRGB8 conversion.
     *
     * Same as ColorXYZ64::toRGB but with fixed e = 1e-4.
     */
    explicit operator MColourRGB8() const;

    /**
     * ColorXYZ64 to ColorHCL16 conversion.
     *
     * Same as ColorXYZ64::toHCL but with fixed e = 1.0.
     */
    explicit operator MColourHCL16() const;

    /**
     * ColorXYZ64 to ColorRGB8 conversion. RGB colours with values outside the
     * range of [0 - e, 1 + e] will be mapped to black.
     * @param e maximum error.
     */
    MColourRGB8 toRGB(double e) const;
    /**
     * ColorXYZ64 to ColorHCL16 conversion. HCL colours with values outside the
     * range of [-360 - e, 360 + e] or [0 - e, 100 + e] will be mapped to black.
     * @param e maximum error.
     */
    MColourHCL16 toHCL(double e) const;

    double x;
    double y;
    double z;
    /**
     * Flag to indicate whether the hue value of the original HCL colour was
     * negative.
     *
     * Only used for conversions between ColourHCL16 <--> ColourXYZ64.
     */
    bool hueSgn;
};

/**
 * @brief Linear componentwise interpolation between two ColourRGB8's.
 * @param c1 The first colour.
 * @param c2 The second colour.
 * @param pos Position at which the interpolation is evaluated (Range: [0, 1]).
 * @return ColourRGB8 between c1 and c2 at position pos.
 */
MColourRGB8 lerp(const MColourRGB8& c1, const MColourRGB8& c2, float pos);

/**
 * @brief Linear componentwise interpolation between two ColourHCL16's.
 * @param c1 The first colour.
 * @param c2 The second colour.
 * @param pos Position at which the interpolation is evaluated, (Range: [0, 1]).
 * @return ColourHCL16 between c1 and c2 at position pos.
 */
MColourHCL16 lerp(const MColourHCL16& c1, const MColourHCL16& c2, float pos);

/**
 * @brief Linear componentwise interpolation between two ColourXYZ64's.
 * @param c1 The first colour.
 * @param c2 The second colour.
 * @param pos Position at which the interpolation is evaluated, (Range: [0, 1]).
 * @return ColourXYZ64 between c1 and c2 at position pos.
 */
MColourXYZ64 lerp(const MColourXYZ64& c1, const MColourXYZ64& c2, float pos);

/**
 * @brief Linear interpolation between two ColourXYZ64's, based on their
 * RGB representation.
 * @param c1 The first colour.
 * @param c2 The second colour.
 * @param pos Position at which the interpolation is evaluated, (Range: [0, 1]).
 * @return ColourXYZ64 between (ColorRGB8)c1 and (ColorRGB8)c2 at position pos.
 */
MColourXYZ64 lerpRGB(const MColourXYZ64& c1, const MColourXYZ64& c2, float pos);

/**
 * @brief Linear interpolation between two ColourXYZ64's, based on their
 * HCL representation.
 * @param c1 The first colour.
 * @param c2 The second colour.
 * @param pos Position at which the interpolation is evaluated, (Range: [0, 1]).
 * @return ColourXYZ64 between (ColorHCL16)c1 and (ColorHCL16)c2 at position pos.
 */
MColourXYZ64 lerpHCL(const MColourXYZ64& c1, const MColourXYZ64& c2, float pos);


} // namespace TFEditor
} // namespace Met3D

#endif // COLOUR_H
