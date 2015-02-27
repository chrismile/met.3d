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
#ifndef COLOURMAP_H
#define COLOURMAP_H

// standard library imports
#include <vector>

// related third party imports
#include <QtGui>
#include <gsl/gsl_interp.h>

// local application imports

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

private:
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
