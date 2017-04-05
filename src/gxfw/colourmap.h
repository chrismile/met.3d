/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016-2017 Marc Rautenhaus
**  Copyright 2016-2017 Theresa Diefenbach
**  Copyright 2016-2017 Bianca Tost
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
#ifndef COLOURMAP_H
#define COLOURMAP_H

// standard library imports
#include <vector>



// related third party imports
#include <QtGui>
#include <gsl/gsl_interp.h>

// local application imports


/******************************************************************************
***                 COLOUR TRANSFORMATION ROUTINES                          ***
*******************************************************************************/

// The following code has been taken from colorspace.c, part of the
// R "colorspace" package by Ross Ihaka. Parts of the code have been modified.
// NOTE: colorspace.c contains further colorspace transformations that might
// become useful for Met.3D.
// =================================================================

// This file incorporates work covered by the following copyright and
// permission notice:

/* Copyright 2005, Ross Ihaka. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. The name of the Ross Ihaka may not be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ROSS IHAKA BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
namespace Met3D
{
namespace Colourspace
{
double gtrans(double u, double gamma);
double ftrans(double u, double gamma);

void sRGB_to_XYZ(double R, double G, double B,
                        double XN, double YN, double ZN,
                        double *X, double *Y, double *Z);

void XYZ_to_sRGB(double X, double Y, double Z,
                        double XN, double YN, double ZN,
                        double *R, double *G, double *B);


void XYZ_to_uv(double X, double Y, double Z, double *u, double *v);

void XYZ_to_LUV(double X, double Y, double Z,
                       double XN, double YN, double ZN,
                       double *L, double *U, double *V);


void LUV_to_XYZ(double L, double U, double V,
                       double XN, double YN, double ZN,
                       double *X, double *Y, double *Z);


void LUV_to_polarLUV(double L, double U, double V,
                            double *l, double *c, double *h);

void polarLUV_to_LUV(double l, double c, double h,
                            double *L, double *U, double *V);
}
}



namespace Met3D
{
/**
  @brief MColourmap is the abstract base class for all colourmap classes,
  classes that implement the mapping of a scalar in the range [0..1] to an
  RGBA colour.

  MColourmap defines the virtual function @ref scalarToColour(), which needs to
  be implemented in derived classes. It maps a double scalar to a colour,
  represented by a QColor value.
  */
class MColourmap
{
public:
    MColourmap();
    virtual ~MColourmap();

    /**
      @abstract Maps the scalar value @p scalar (in the range 0..1) to a colour
      value. Needs to be implemented in derived classes.
      */
    virtual QColor scalarToColour(double scalar) = 0;
};


/**
  @brief MRainbowColourmap implements an analytic rainbow colourmap. @ref
  scalarToColour() converts the given scalar analytically to an RGBA value.
  */
class MRainbowColourmap : public MColourmap
{
public:
    MRainbowColourmap();
    QColor scalarToColour(double scalar);
};


/**
  @brief MHCLColourmap implements an HCL based colourmap. The code ports parts
  of the R "colorspace" package by Ross Ihaka. In particular, the "heat_hcl()"
  and "diverge_hcl()" methods are implemented in this class.

  References:
  ===========
  Zeileis, Hornik, Murrell (2007): Escaping RGBland: Selecting Colors for
  Statistical Graphics. Research Report Series / Department of Statistics and
  Mathematics, 61. WU Vienna.

  Also see their "R companion vignette": Zeileis, Hornik, Murrell. HCL-based
  Color Palettes in R.

  Stauffer, Mayr, Dabernig, Zeileis (2013): Somewhere over the Rainbow: How to
  Make Effective Use of Colors in Meteorological Visualizations. Working Papers
  from Faculty of Economics and Statistics, University of Innsbruck.

  @see http://hclwizard.org for further information on HCL colourmaps.
  */
class MHCLColourmap : public MColourmap
{
public:
    /**
      Construct a new HCL colourmap. For meaning of the parameters, @see
      http://hclwizard.org.

      @p diverging controls whether a sequential or a divergent colourmap
      is created.
     */
    MHCLColourmap(
            float hue1, float hue2, float chroma1, float chroma2,
            float luminance1, float luminance2, float power1, float power2,
            float alpha1, float alpha2, float poweralpha,
            bool diverging=false);

    QColor scalarToColour(double scalar);

protected:
    float hue1;
    float hue2;
    float chroma1;
    float chroma2;
    float luminance1;
    float luminance2;
    float power1;
    float power2;
    float alpha1;
    float alpha2;
    float poweralpha;
    bool  diverging;
};


/**
  MColourNode stores an interpolation node for a single colour component. It is
  used in conjunction with @ref MColourmapInterpolationNodes and @ref
  MLinearSegmentedColourmap.
  */
struct MColourNode
{
    double scalar;
    double intensity;
};


/**
  MColourmapInterpolationNodes stores vector arrays of interpolation nodes for
  the four colour components RGBA.

  @see MLinearSegmentedColourmap
  */
typedef std::vector<MColourNode> MColourmapInterpolationNodes[4];


/**
  @brief MLinearSegmentedColourmap implements a colourmap that derives its
  colour values through linear interpolation. The intensities of the RGBA
  colour components are defined at a number of interpolation nodes (which are
  passed to the constructor). The GSL library is used to linearly interpolate
  any scalar in the range 0..1 to a colour value.

  The design of the class has been inspired by Matplotlib's
  LinearSegmentedColormap class.
  @see http://matplotlib.sourceforge.net/api/colors_api.html#matplotlib.colors.LinearSegmentedColormap
  */
class MLinearSegmentedColourmap : public MColourmap
{
public:
    enum ColourIndex { RED = 0, GREEN = 1, BLUE = 2, ALPHA = 3 };

    /**
      Constructs a new colourmap from the sepcified interpolation nodes. The
      GSL interpolation objects are intialized here.
      */
    MLinearSegmentedColourmap(
            const MColourmapInterpolationNodes& interpolationNodes);

    /**
      Frees the GSL interpolation objects.
      */
    ~MLinearSegmentedColourmap();

    /**
      Performs a linear interpolation with GSL to map @p scalar to a colour.
      */
    QColor scalarToColour(double scalar);

protected:
    gsl_interp       *interp[4];
    gsl_interp_accel *interpAcc[4];
    double *scalars[4];
    double *colourValues[4];
};


/**
  @brief MHSVColourmap implements an HSV based colourmap. The code ports parts
  of the R "colorspace" package by Ross Ihaka.

  Colourmap definitions are read from Vapor-exported transfer functions.
 */
class MHSVColourmap : public MColourmap
{
public:
    enum ColourIndex { HUE = 0, SATURATION = 1, VALUE = 2, ALPHA = 3 };

    MHSVColourmap(QString& vaporFileName);

    /**
      Frees the GSL interpolation objects.
      */
    ~MHSVColourmap();

    QColor scalarToColour(double scalar);

    void readFromVaporFile(QString fileName);

protected:
    MColourmapInterpolationNodes vaporNodes;
    gsl_interp       *interp[4];
    gsl_interp_accel *interpAcc[4];
    double *scalars[4];
    double *colourValues[4];
};


/**
  @brief MColourmapPool manages a "pool" of colourmaps.

  A number of pre-defined colourmaps (in part taken from Matplotlib) are
  created upon construction, but further colourmaps can added at runtime with
  the @ref addColourmap() method.
  */
class MColourmapPool
{
public:
    /**
      Constructs a new colourmap pool and creates a number of predefined
      colourmaps by calling @ref initializePredefinedColourmaps(). Some
      colourmap definitions are taken from Matplotlib.

      @see http://matplotlib.sourceforge.net/examples/pylab_examples/show_colormaps.html
      */
    MColourmapPool();
    ~MColourmapPool();

    /**
      Returns a list of the names of the available colourmaps.
      */
    QStringList availableColourmaps() { return colourmaps.keys(); }

    /**
      Adds the colourmap @p colourmap to the pool and register it under the
      name @p key. The name can be used to access the colourmap with @ref
      colourmap().

      NOTE: The colourmap is destroyed by the constructor of this class, there
      is no need to delete the object in the user code.
      */
    void addColourmap(QString key,
                      MColourmap* colourmap) { colourmaps[key] = colourmap; }

    /**
      Returns the colourmap registered with the name @p key.
      */
    MColourmap* colourmap(QString key) { return colourmaps[key]; }

private:
    /**
      Creates a number of predefined colourmaps, data in parts taken from
      Matplotlib. Compare to the Matplotlib "_cm.py" file.

      @see http://matplotlib.sourceforge.net/examples/pylab_examples/show_colormaps.html
      */
    void initializePredefinedColourmaps();

    QMap<QString, MColourmap*> colourmaps;
};

} // namespace Met3D

#endif // COLOURMAP_H
