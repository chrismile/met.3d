/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016 Marc Rautenhaus
**  Copyright 2016 Theresa Diefenbach
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
#include "colourmap.h"

// standard library imports
#include <iostream>

// related third party imports
#include <QtXml>
#include <QFile>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"

using namespace std;


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

#define NA_REAL M_MISSING_VALUE

#include <ctype.h>

#define DEG2RAD(x) (.01745329251994329576*(x))
#define RAD2DEG(x) (57.29577951308232087721*(x))

/* ----- CIE-XYZ <-> Device dependent RGB -----
 *
 *  Gamma Correction
 *
 *  The following two functions provide gamma correction which
 *  can be used to switch between sRGB and linearized sRGB (RGB).
 *
 *  The standard value of gamma for sRGB displays is approximately 2.2,
 *  but more accurately is a combination of a linear transform and
 *  a power transform with exponent 2.4
 *
 *  gtrans maps linearized sRGB to sRGB.
 *  ftrans provides the inverse map.
 *
 */

static double gtrans(double u, double gamma)
{
    if (u > 0.00304)
    return 1.055 * pow(u, (1 / gamma)) - 0.055;
    else
    return 12.92 * u;
}

//static double ftrans(double u, double gamma)
//{
//    if (u > 0.03928)
//    return pow((u + 0.055) / 1.055, gamma);
//    else
//    return u / 12.92;
//}


/* ----- CIE-XYZ <-> sRGB -----
 *
 *  R, G, and B give the levels of red, green and blue as values
 *  in the interval [0,1].  X, Y and Z give the CIE chromaticies.
 *  XN, YN, ZN gives the chromaticity of the white point.
 *
 */

//static void sRGB_to_XYZ(double R, double G, double B,
//                        double XN, double YN, double ZN,
//                        double *X, double *Y, double *Z)
//{
//    double r, g, b;
//    r = ftrans(R, 2.4);
//    g = ftrans(G, 2.4);
//    b = ftrans(B, 2.4);
//    *X = YN * (0.412453 * r + 0.357580 * g + 0.180423 * b);
//    *Y = YN * (0.212671 * r + 0.715160 * g + 0.072169 * b);
//    *Z = YN * (0.019334 * r + 0.119193 * g + 0.950227 * b);
//}

static void XYZ_to_sRGB(double X, double Y, double Z,
                        double XN, double YN, double ZN,
                        double *R, double *G, double *B)
{
    Q_UNUSED(XN);
    Q_UNUSED(ZN);
    *R = gtrans(( 3.240479 * X - 1.537150 * Y - 0.498535 * Z) / YN, 2.4);
    *G = gtrans((-0.969256 * X + 1.875992 * Y + 0.041556 * Z) / YN, 2.4);
    *B = gtrans(( 0.055648 * X - 0.204043 * Y + 1.057311 * Z) / YN, 2.4);
}


/* ----- CIE-XYZ <-> CIE-LUV ----- */

static void XYZ_to_uv(double X, double Y, double Z, double *u, double *v)
{
    double t, x, y;
    t = X + Y + Z;
    x = X / t;
    y = Y / t;
    *u = 2 * x / (6 * y - x + 1.5);
    *v = 4.5 * y / (6 * y - x + 1.5);
}

//static void XYZ_to_LUV(double X, double Y, double Z,
//                       double XN, double YN, double ZN,
//                       double *L, double *U, double *V)
//{
//    double u, v, uN, vN, y;
//    XYZ_to_uv(X, Y, Z, &u, &v);
//    XYZ_to_uv(XN, YN, ZN, &uN, &vN);
//    y = Y / YN;
//    *L = (y > 0.008856) ? 116 * pow(y, 1.0/3.0) - 16 : 903.3 * y;
//    *U = 13 * *L * (u - uN);
//    *V = 13 * *L * (v - vN);
//}

static void LUV_to_XYZ(double L, double U, double V,
                       double XN, double YN, double ZN,
                       double *X, double *Y, double *Z)
{
    double u, v, uN, vN;
    if (L <= 0 && U == 0 && V == 0) {
    *X = 0; *Y = 0; *Z = 0;
    }
    else {
    *Y = YN * ((L > 7.999592) ? pow((L + 16)/116, 3) : L / 903.3);
    XYZ_to_uv(XN, YN, ZN, &uN, &vN);
    u = U / (13 * L) + uN;
    v = V / (13 * L) + vN;
    *X =  9.0 * *Y * u / (4 * v);
    *Z =  - *X / 3 - 5 * *Y + 3 * *Y / v;
    }
}


/* ----- LUV <-> polarLUV ----- */

//static void LUV_to_polarLUV(double L, double U, double V,
//                            double *l, double *c, double *h)
//{
//    *l = L;
//    *c = sqrt(U * U + V * V);
//    *h = RAD2DEG(atan2(V, U));
//    while (*h > 360) *h -= 360;
//    while (*h < 0) *h += 360;
//}

static void polarLUV_to_LUV(double l, double c, double h,
                            double *L, double *U, double *V)
{
    h = DEG2RAD(h);
    *L = l;
    *U = c * cos(h);
    *V = c * sin(h);
}


/* ----- RGB <-> HSV ----- */

#define RETURN_RGB(red,green,blue) *r=red;*g=green;*b=blue;break;

//NOTE: Parts of this function have been modified in order to use it with
//      Vapor-imported transfer functions!
static void HSV_to_RGB(double h, double s, double v,
                       double *r, double *g, double *b)
{
    double m, n, f;
    int i;
    if (h == NA_REAL) {
        *r = v; *g = v; *b = v;
    }
    else {
        if (h < 0) h = 0;
        h = h * 6;		/* convert to [0, 6] , in Vapor h is in the interval from [-1 ; 1] */
        i = floor(h);
        f = h - i;
        if(!(i & 1))	/* if i is even */
            f = 1 - f;
        m = v * (1 - s);
        n = v * (1 - s * f);   
        switch (i) {
            case 6:
            case 0: RETURN_RGB(v, n, m);
            case 1: RETURN_RGB(n, v, m);
            case 2: RETURN_RGB(m, v, n);
            case 3: RETURN_RGB(m, n, v);
            case 4: RETURN_RGB(n, m, v);
            case 5: RETURN_RGB(v, m, n);
        }
   }
}


// END code form the R "colorspace" package.
// =================================================================


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/
MColourmap::MColourmap()
{
}


MColourmap::~MColourmap()
{
}


MRainbowColourmap::MRainbowColourmap()
    : MColourmap()
{
}


MHCLColourmap::MHCLColourmap(
        float hue1, float hue2, float chroma1, float chroma2,
        float luminance1, float luminance2, float power1, float power2,
        float alpha1, float alpha2, float poweralpha, bool diverging)
    : MColourmap(),
      hue1(hue1),
      hue2(hue2),
      chroma1(chroma1),
      chroma2(chroma2),
      luminance1(luminance1),
      luminance2(luminance2),
      power1(power1),
      power2(power2),
      alpha1(alpha1),
      alpha2(alpha2),
      poweralpha(poweralpha),
      diverging(diverging)
{
}


MLinearSegmentedColourmap::MLinearSegmentedColourmap(
        const MColourmapInterpolationNodes& interpolationNodes)
    : MColourmap()
{
    // For each colour component:
    for (int c = RED; c <= ALPHA; c++)
    {
        // Initialize a GSL interpolation object for the colour component.
        int numNodes = interpolationNodes[c].size();
        interpAcc[c] = gsl_interp_accel_alloc();
        interp[c]    = gsl_interp_alloc(gsl_interp_linear, numNodes);
        // Copy scalar (t) and colour intensity data to double arrays, as these
        // are required by gsl_interp_init().
        scalars[c] = new double[numNodes];
        colourValues[c] = new double[numNodes];
        for (int i = 0; i < numNodes; i++) {
            scalars[c][i] = interpolationNodes[c][i].scalar;
            colourValues[c][i] = interpolationNodes[c][i].intensity;
        }
        // Pass interpolation nodes to GSL.
        gsl_interp_init(interp[c], scalars[c], colourValues[c], numNodes);
    }
}


MLinearSegmentedColourmap::~MLinearSegmentedColourmap()
{
    // For each colour compontent:
    for (int c = RED; c <= ALPHA; c++)
    {
        // Free interpolation object ..
        gsl_interp_free(interp[c]);
        gsl_interp_accel_free(interpAcc[c]);
        // .. and memory.
        delete[] scalars[c];
        delete[] colourValues[c];
    }
}


MHSVColourmap::MHSVColourmap(QString& vaporFileName)
    :MColourmap()
{
    readFromVaporFile(vaporFileName);

    // For each colour component:
    for (int c = HUE; c <= ALPHA; c++)
    {
        // Initialize a GSL interpolation object for the colour component.
        int numNodes = vaporNodes[c].size();
        interpAcc[c] = gsl_interp_accel_alloc();
        interp[c]    = gsl_interp_alloc(gsl_interp_linear, numNodes);
        // Copy scalar (t) and colour intensity data to double arrays, as these
        // are required by gsl_interp_init().
        scalars[c] = new double[numNodes];
        colourValues[c] = new double[numNodes];
        for (int i = 0; i < numNodes; i++) {
            scalars[c][i] = vaporNodes[c][i].scalar;
            colourValues[c][i] = vaporNodes[c][i].intensity;
        }
        // Pass interpolation nodes to GSL.
        scalars[c][numNodes-1] = 1. ;

        gsl_interp_init(interp[c], scalars[c], colourValues[c], numNodes);
    }
}


MHSVColourmap::~MHSVColourmap()
{
    // For each colour component:
    for (int c = HUE; c <= ALPHA; c++)
    {
        // Free interpolation object ..
        gsl_interp_free(interp[c]);
        gsl_interp_accel_free(interpAcc[c]);
        // .. and memory.
        delete[] scalars[c];
        delete[] colourValues[c];
    }
}


MColourmapPool::MColourmapPool()
{
    initializePredefinedColourmaps();
}


MColourmapPool::~MColourmapPool()
{
    foreach (MColourmap* cmap, colourmaps) delete cmap;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QColor MRainbowColourmap::scalarToColour(double scalar)
{
    scalar = clamp(scalar, 0., 1.);
    QColor rgba;
    if      (scalar < (1./4.)) {
        rgba.setRgbF(0., 4. * (scalar - (0./4.)), 1., 1.);
    }
    else if (scalar >= (1./4.) && scalar < (2./4.)) {
        rgba.setRgbF(0., 1., 1. - 4. * (scalar - (1./4.)), 1.);
    }
    else if (scalar >= (2./4.) && scalar < (3./4.)) {
        rgba.setRgbF(4. * (scalar - (2./4.)), 1., 0., 1.);
    }
    else if (scalar >= (3./4.)) {
        rgba.setRgbF(1., 1. - 4. * (scalar - (3./4.)), 0., 1.);
    }
    return rgba;
}


QColor MHCLColourmap::scalarToColour(double scalar)
{
    // This functions provides an implementation similar to the "heat_hcl()"
    // and "diverge_hcl()" functions of the R "colorspace" package. See code in
    // "palettes.R".

    // Compute hue, chroma and luminance according to Zeileis et al. (2007),
    // Sect. 4.2 and 4.3. Full reference is: Zeileis, Hornik, Murrell: Escaping
    // RGBland: Selecting Colors for Statistical Graphics. Research Report
    // Series / Department of Statistics and Mathematics, 61. WU Vienna.

    double hue, chroma, luminance, alpha;
    if (!diverging)
    {
        // Equations for sequential colourmaps (single or multiple hue).

        // The equations are taken from "heat_hcl()" by Ross Ihaka
        // (cf. copyright statement above) (l,c,h,power are 2-tuples in the R
        // implementation):
        //    ...
        //    rval <- hex(polarLUV(L = l[2L] - diff(l) * rval^power[2L],
        //                         C = c[2L] - diff(c) * rval^power[1L],
        //                         H = h[2L] - diff(h) * rval),
        //                fixup = fixup, ...)
        //    ...
        hue       = hue2 - scalar * (hue2-hue1);
        chroma    = chroma2 - pow(scalar, power1) * (chroma2-chroma1);
        luminance = luminance2 - pow(scalar, power2) * (luminance2-luminance1);

        // Alpha mapping is not part of the R package.
        alpha     = alpha2 - pow(scalar, poweralpha) * (alpha2-alpha1);
    }
    else
    {
        // Equations for divergent colourmaps.

        // The equations are taken from "diverge_hcl()" by Ross
        // Ihaka (l,h are 2-tuples in the R implementation):
        //    ...
        //    rval <- hex(polarLUV(L = l[2L] - diff(l) * abs(rval)^power[2L],
        //                         C = c * abs(rval)^power[1L],
        //                         H = ifelse(rval > 0, h[1L], h[2L])),
        //                fixup = fixup, ...)
        //    ...

        // R method takes scalar from -1..1; this method from 0..1. Scale our
        // scalar to -1..1.
        double scalar_ = 2.*scalar - 1.;
        hue       = scalar_ > 0. ? hue1 : hue2;
        chroma    = chroma1 * pow(fabs(scalar_), power1);
        luminance = luminance2 - pow(fabs(scalar_), power2) * (luminance2-luminance1);

        // Alpha mapping is not part of the R package.
        alpha     = alpha2 - pow(fabs(scalar_), poweralpha) * (alpha2-alpha1);
    }

    // HCL is equivalent to polar LUV colour space. Conversion to RGB follows
    // the implementation in the "as_sRGB()" function in "colorspace.c", first
    // converting from polar LUV to LUV, then to XYZ colour space, and finally
    // to sRGB.
    double L, U, V;
    polarLUV_to_LUV(luminance, chroma, hue, &L, &U, &V);

    // Default white point used in "colorspace.c". See function "CheckWhite()".
    double Xn =  95.047; /* Use D65 by default. */
    double Yn = 100.000;
    double Zn = 108.883;

    double X, Y, Z;
    LUV_to_XYZ(L, U, V, Xn, Yn, Zn, &X, &Y, &Z);

    // The "colorspace" package uses sRGB, see "colorspace.R", method "hex()".
    // "hex()" is used in "heat_hcl()" to convert polar LUV to sRGB.
    double R, G, B;
    XYZ_to_sRGB(X, Y, Z, Xn, Yn, Zn, &R, &G, &B);

    return QColor(int(R*255.), int(G*255.), int(B*255.), int(alpha*255.));
}


QColor MLinearSegmentedColourmap::scalarToColour(double scalar)
{
    QColor colour;
    colour.setRgbF(
                gsl_interp_eval(interp[RED], scalars[RED],
                                colourValues[RED], scalar, interpAcc[RED]),
                gsl_interp_eval(interp[GREEN], scalars[GREEN],
                                colourValues[GREEN], scalar, interpAcc[GREEN]),
                gsl_interp_eval(interp[BLUE], scalars[BLUE],
                                colourValues[BLUE], scalar, interpAcc[BLUE]),
                gsl_interp_eval(interp[ALPHA], scalars[ALPHA],
                                colourValues[ALPHA], scalar, interpAcc[ALPHA])
                );
    return colour;
}


QColor MHSVColourmap::scalarToColour(double scalar)
{
    double H, S, V, alpha, R, G, B;

    H = gsl_interp_eval(interp[HUE], scalars[HUE],
                                colourValues[HUE], scalar, interpAcc[HUE]);
    S = gsl_interp_eval(interp[SATURATION], scalars[SATURATION],
                                colourValues[SATURATION], scalar, interpAcc[SATURATION]);
    V = gsl_interp_eval(interp[VALUE], scalars[VALUE],
                                colourValues[VALUE], scalar, interpAcc[VALUE]);
    alpha = gsl_interp_eval(interp[ALPHA], scalars[ALPHA],
                                colourValues[ALPHA], scalar, interpAcc[ALPHA]);

    HSV_to_RGB(H, S, V, &R, &G, &B);
    return QColor(int(R*255.), int(G*255.), int(B*255.), int(alpha*255.));
}


void MHSVColourmap::readFromVaporFile(QString fileName)
{
    QDomDocument vaporTF;
    QFile file(fileName);

    MColourNode firstNode;
    firstNode.scalar = 7.; //Random Value to initialize
    firstNode.intensity = 7.; //Random Value to initialize

    for (int p = 0; p <= 3; p++) vaporNodes[p].push_back(firstNode);

    LOG4CPLUS_TRACE(mlog, "Size of vaporNodes beginning " << vaporNodes[0].size());

    if (!file.open(QIODevice::ReadOnly))
    {
        file.close();

        QString msg = QString("ERROR: cannot open Vapor transfer function file %1")
                .arg(fileName);
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }

    vaporTF.setContent(&file);

    file.close();

    // Get OpacityMapControlPoints

    QDomElement transferFunction = vaporTF.documentElement();

    // Find Out where OpacityMap starts,
    // since default transferfunctions in vapor have other format than 'self-made' ones

    QDomElement opacityMap = transferFunction.firstChild().toElement();

    while(opacityMap.tagName() != "OpacityMap")
    {
        opacityMap = opacityMap.nextSibling().toElement();
    }

    QDomElement opacityMapControlPoint = opacityMap.firstChild().toElement();

    // Loop while there is an OpacityMapControlPoint
    int i = 0 ;

    while ( !opacityMapControlPoint.isNull() )
    {
        // Check if the child tag name is COMPONENT
        if (opacityMapControlPoint.tagName()=="OpacityMapControlPoint")
        {
            QString opacity, valueStr;
            double alpha, value;
            opacity = opacityMapControlPoint.attribute("Opacity","NaN");
            valueStr = opacityMapControlPoint.attribute("Value","NaN");
            alpha = opacity.toDouble();
            value = valueStr.toDouble();

            MColourNode newNode;
            newNode.scalar = value;
            newNode.intensity = alpha;

            vaporNodes[ALPHA].push_back(newNode);

            //std::cout << "Opacity  = " << VaporNodes[4][i].intensity  << "    \t Value  = " << value << std::endl;
            i += 1;
            opacityMapControlPoint = opacityMapControlPoint.nextSibling().toElement();
        }
    }

    vaporNodes[ALPHA].erase(vaporNodes[ALPHA].begin());
    i = 0;

    QDomElement colorMap = opacityMap.nextSibling().toElement();
    QDomElement colorMapControlPoint = colorMap.firstChild().toElement();

    while ( !colorMapControlPoint.isNull() )
    {
        // Check if the child tag name is COMPONENT
        if (colorMapControlPoint.tagName() == "ColorMapControlPoint")
        {
            QString HSV, Value ;
            QStringList HSVlist ;
            double  h,s,v, value;
            HSV = colorMapControlPoint.attribute("HSV", "NaN");
            HSVlist = HSV.split(" ");
            h = HSVlist[0].toDouble();
            s = HSVlist[1].toDouble();
            v = HSVlist[2].toDouble();

            Value = colorMapControlPoint.attribute("Value", "NaN");
            value = Value.toDouble();

            MColourNode newNode;
            newNode.scalar = value;

            newNode.intensity = h;
            vaporNodes[HUE].push_back(newNode);

            newNode.intensity = s;
            vaporNodes[SATURATION].push_back(newNode);

            newNode.intensity = v;
            vaporNodes[VALUE].push_back(newNode);

            /*
            vaporNodes[0][i].intensity = h;
            vaporNodes[1][i].scalar = value;
            vaporNodes[1][i].intensity = s;
            vaporNodes[2][i].scalar = value;
            vaporNodes[2][i].intensity = v;
            **/

            //for (unsigned int c = 0; c <= 3; c++) std::cout << vaporNodes[c][i].scalar <<std::endl;
            //std::cout << vaporNodes[0][i].scalar <<std::endl;
            //std::cout << vaporNodes[1][i].scalar <<std::endl;
            //std::cout << vaporNodes[2][i].scalar <<std::endl;
            //std::cout << vaporNodes[3][i].scalar <<std::endl;
        }
        colorMapControlPoint = colorMapControlPoint.nextSibling().toElement();
        i += 1;
    }

    for(unsigned int c=HUE; c<=VALUE; c++)
    {
        vaporNodes[c].erase(vaporNodes[c].begin());
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MColourmapPool::initializePredefinedColourmaps()
{
    // NOTE: A number of colourmap definitions for linear segmented colourmaps
    // are taken from Matplotlib. See matplotlib.colors.LinearSegmentedColormap
    // http://matplotlib.sourceforge.net/api/colors_api.html#matplotlib.colors.LinearSegmentedColormap
    // and the definitions in _cm.py.

    // ==================================================================
    // A) Analytic colourmaps:
    // ==================================================================

    addColourmap("rainbow", new MRainbowColourmap());


    // ==================================================================
    // B) Colourmaps based on linear interpolation:
    // ==================================================================
#define S 1.e-9 // small offset to make steps in the colourmap possible

    MColourmapInterpolationNodes mss_clouds_data = {
        /* red */   {{0.0  , 1.},
                     {0.2-S, 1.},
                     {0.2  , 0.},
                     {1.0  , 0.}},
        /* green */ {{0.0  , 1.},
                     {0.2-S, 1.},
                     {0.2  , 0.},
                     {1.0  , 1.}},
        /* blue */  {{0.0  , 1.},
                     {0.2-S, 1.},
                     {0.2  , 1.},
                     {1.0  , .5}},
        /* alpha */ {{0.0  , 0.5},
                     {0.2-S, 0.5},
                     {0.2  , 1.0},
                     {1.0  , 1.0}}
    };
    addColourmap("mss_clouds", new MLinearSegmentedColourmap(mss_clouds_data));

    MColourmapInterpolationNodes raycaster_clouds_data = {
        /* red */   {{0.0  , 0.},
                     {0.2-S, 0.},
                     {0.2  , 0.},
                     {1.0  , 0.}},
        /* green */ {{0.0  , 0.},
                     {0.2-S, 0.},
                     {0.2  , 0.},
                     {1.0  , 1.}},
        /* blue */  {{0.0  , 0.},
                     {0.2-S, 0.},
                     {0.2  , 1.},
                     {1.0  , .5}},
        /* alpha */ {{0.0  , 0.0},
                     {0.2-S, 0.0},
                     {0.2  , 1.0},
                     {1.0  , 1.0}}
    };
    addColourmap("raycaster_clouds", new MLinearSegmentedColourmap(raycaster_clouds_data));

    MColourmapInterpolationNodes winter_data = {
        /* red */   {{0., 0.},
                     {1., 0.}},
        /* green */ {{0., 0.},
                     {1., 1.}},
        /* blue */  {{0., 1.},
                     {1., .5}},
        /* alpha */ {{0., 1.},
                     {1., 1.}}
    };
    addColourmap("winter", new MLinearSegmentedColourmap(winter_data));

    MColourmapInterpolationNodes spring_data = {
        /* red */   {{0., 1.},
                     {1., 1.}},
        /* green */ {{0., 0.},
                     {1., 1.}},
        /* blue */  {{0., 1.},
                     {1., 0.}},
        /* alpha */ {{0., 1.},
                     {1., 1.}}
    };
    addColourmap("spring", new MLinearSegmentedColourmap(spring_data));

    MColourmapInterpolationNodes summer_data = {
        /* red */   {{0., 0.0},
                     {1., 1.0}},
        /* green */ {{0., 0.5},
                     {1., 1.0}},
        /* blue */  {{0., 0.4},
                     {1., 0.4}},
        /* alpha */ {{0., 1.0},
                     {1., 1.0}}
    };
    addColourmap("summer", new MLinearSegmentedColourmap(summer_data));

    MColourmapInterpolationNodes autumn_data = {
        /* red */   {{0., 1.0},
                     {1., 1.0}},
        /* green */ {{0., 0.0},
                     {1., 1.0}},
        /* blue */  {{0., 0.0},
                     {1., 0.0}},
        /* alpha */ {{0., 1.0},
                     {1., 1.0}}
    };
    addColourmap("autumn", new MLinearSegmentedColourmap(autumn_data));

    MColourmapInterpolationNodes grey_data = {
        /* red */   {{0., 0.0},
                     {1., 1.0}},
        /* green */ {{0., 0.0},
                     {1., 1.0}},
        /* blue */  {{0., 0.0},
                     {1., 1.0}},
        /* alpha */ {{0., 1.0},
                     {1., 1.0}}
    };
    addColourmap("grey", new MLinearSegmentedColourmap(grey_data));

    MColourmapInterpolationNodes terrain_data = {
        /* red */   {{0.00, 0.2},
                     {0.15, 0.0},
                     {0.25, 0.0},
                     {0.50, 1.0},
                     {0.75, 0.5},
                     {1.00, 1.0}},
        /* green */ {{0.00, 0.2},
                     {0.15, 0.6},
                     {0.25, 0.8},
                     {0.50, 1.0},
                     {0.75, 0.36},
                     {1.00, 1.0}},
        /* blue */  {{0.00, 0.6},
                     {0.15, 1.0},
                     {0.25, 0.4},
                     {0.50, 0.6},
                     {0.75, 0.33},
                     {1.00, 1.0}},
        /* alpha */ {{0., 1.0},
                     {1., 1.0}}
    };
    addColourmap("terrain", new MLinearSegmentedColourmap(terrain_data));

    MColourmapInterpolationNodes raycaster_hot_data = {
        /* red */   {{0.      , 0.0416},
                     {0.365079, 1.0   },
                     {1.      , 1.0   }},
        /* green */ {{0.      , 0.0   },
                     {0.365079, 0.0   },
                     {0.746032, 1.0   },
                     {1.      , 1.0   }},
        /* blue */  {{0.      , 0.0   },
                     {0.746032, 0.0   },
                     {1.      , 1.0   }},
        /* alpha */ {{0.0  , 0.0},
                     {0.2-S, 0.0},
                     {0.2  , 1.0},
                     {1.0  , 1.0}}
    };
    addColourmap("raycaster_hot", new MLinearSegmentedColourmap(raycaster_hot_data));

    MColourmapInterpolationNodes hot_data = {
        /* red */   {{0.      , 0.0416},
                     {0.365079, 1.0   },
                     {1.      , 1.0   }},
        /* green */ {{0.      , 0.0   },
                     {0.365079, 0.0   },
                     {0.746032, 1.0   },
                     {1.      , 1.0   }},
        /* blue */  {{0.      , 0.0   },
                     {0.746032, 0.0   },
                     {1.      , 1.0   }},
        /* alpha */ {{0., 1.0},
                     {1., 1.0}}
    };
    addColourmap("hot", new MLinearSegmentedColourmap(hot_data));

    MColourmapInterpolationNodes hot_wind_data = {
        /* red */   {{0.      , 0.0416},
                     {0.365079, 1.0   },
                     {1.      , 1.0   }},
        /* green */ {{0.      , 0.0   },
                     {0.365079, 0.0   },
                     {0.746032, 1.0   },
                     {1.      , 1.0   }},
        /* blue */  {{0.      , 0.0   },
                     {0.746032, 0.0   },
                     {1.      , 1.0   }},
        /* alpha */ {{0.0  , 1.0},
                     {0.9  , 1.0},
                     {1.0  , 0.85}}
    };
    addColourmap("hot_wind", new MLinearSegmentedColourmap(hot_wind_data));

    MColourmapInterpolationNodes bwr_data = {
        /* red */   {{0.0, 0.0},
                     {0.5, 1.0},
                     {1.0, 1.0}},
        /* green */ {{0.0, 0.0},
                     {0.5, 1.0},
                     {1.0, 0.0}},
        /* blue */  {{0.0, 1.0},
                     {0.5, 1.0},
                     {1.0, 0.0}},
        /* alpha */ {{0., 1.0},
                     {1., 1.0}}
    };
    addColourmap("bwr", new MLinearSegmentedColourmap(bwr_data));

    MColourmapInterpolationNodes gist_rainbow_data = {
        /* red */   {{0.000, 1.0},
                     {0.030, 1.0},
                     {0.215, 1.0},
                     {0.400, 0.0},
                     {0.586, 0.0},
                     {0.770, 0.0},
                     {0.954, 1.0},
                     {1.000, 1.0}},
        /* green */ {{0.000, 0.0},
                     {0.030, 0.0},
                     {0.215, 1.0},
                     {0.400, 1.0},
                     {0.586, 1.0},
                     {0.770, 0.0},
                     {0.954, 0.0},
                     {1.000, 0.0}},
        /* blue */  {{0.000, 0.16},
                     {0.030, 0.00},
                     {0.215, 0.00},
                     {0.400, 0.00},
                     {0.586, 1.00},
                     {0.770, 1.00},
                     {0.954, 1.00},
                     {1.000, 0.75}},
        /* alpha */ {{0., 1.0},
                     {1., 1.0}}
    };
    addColourmap("gist_rainbow", new MLinearSegmentedColourmap(gist_rainbow_data));


    // ETH PV colourmap, taken from Matlab implementation of F. Gierth.
    MColourmapInterpolationNodes pv_data = {
        /* red */   {
            {0.000000, 0.000000},
            {0.015873, 0.098000},
            {0.031746, 0.196100},
            {0.047619, 0.294100},
            {0.063492, 0.392200},
            {0.079365, 0.490200},
            {0.095238, 0.588200},
            {0.111111, 0.686300},
            {0.126984, 0.784300},
            {0.142857, 0.882400},
            {0.158730, 0.980400},
            {0.174603, 1.000000},
            {0.190476, 1.000000},
            {0.206349, 1.000000},
            {0.222222, 1.000000},
            {0.238095, 1.000000},
            {0.253968, 1.000000},
            {0.269841, 1.000000},
            {0.285714, 1.000000},
            {0.301587, 1.000000},
            {0.317460, 1.000000},
            {0.333333, 1.000000},
            {0.349206, 1.000000},
            {0.365079, 1.000000},
            {0.380952, 1.000000},
            {0.396825, 1.000000},
            {0.412698, 1.000000},
            {0.428571, 1.000000},
            {0.444444, 1.000000},
            {0.460317, 1.000000},
            {0.476190, 1.000000},
            {0.492063, 1.000000},
            {0.507937, 1.000000},
            {0.523810, 1.000000},
            {0.539683, 1.000000},
            {0.555556, 1.000000},
            {0.571429, 1.000000},
            {0.587302, 1.000000},
            {0.603175, 1.000000},
            {0.619048, 1.000000},
            {0.634921, 1.000000},
            {0.650794, 1.000000},
            {0.666667, 1.000000},
            {0.682540, 1.000000},
            {0.698413, 1.000000},
            {0.714286, 1.000000},
            {0.730159, 1.000000},
            {0.746032, 1.000000},
            {0.761905, 1.000000},
            {0.777778, 1.000000},
            {0.793651, 1.000000},
            {0.809524, 1.000000},
            {0.825397, 1.000000},
            {0.841270, 1.000000},
            {0.857143, 1.000000},
            {0.873016, 1.000000},
            {0.888889, 1.000000},
            {0.904762, 1.000000},
            {0.920635, 1.000000},
            {0.936508, 1.000000},
            {0.952381, 1.000000},
            {0.968254, 1.000000},
            {0.984127, 1.000000},
            {1.000000, 1.000000}
        },
        /* green */ {
            {0.000000, 0.600000},
            {0.015873, 0.603900},
            {0.031746, 0.615700},
            {0.047619, 0.631400},
            {0.063492, 0.658800},
            {0.079365, 0.694100},
            {0.095238, 0.737300},
            {0.111111, 0.788200},
            {0.126984, 0.843100},
            {0.142857, 0.909800},
            {0.158730, 0.984300},
            {0.174603, 1.000000},
            {0.190476, 0.988800},
            {0.206349, 0.961200},
            {0.222222, 0.918900},
            {0.238095, 0.872900},
            {0.253968, 0.832400},
            {0.269841, 0.794600},
            {0.285714, 0.756900},
            {0.301587, 0.711600},
            {0.317460, 0.640700},
            {0.333333, 0.542200},
            {0.349206, 0.432000},
            {0.365079, 0.303900},
            {0.380952, 0.199800},
            {0.396825, 0.148000},
            {0.412698, 0.114100},
            {0.428571, 0.081500},
            {0.444444, 0.051100},
            {0.460317, 0.031700},
            {0.476190, 0.033300},
            {0.492063, 0.053300},
            {0.507937, 0.081900},
            {0.523810, 0.114100},
            {0.539683, 0.146400},
            {0.555556, 0.178800},
            {0.571429, 0.211100},
            {0.587302, 0.243500},
            {0.603175, 0.275900},
            {0.619048, 0.308200},
            {0.634921, 0.340600},
            {0.650794, 0.373000},
            {0.666667, 0.405300},
            {0.682540, 0.437700},
            {0.698413, 0.470100},
            {0.714286, 0.503800},
            {0.730159, 0.531300},
            {0.746032, 0.558900},
            {0.761905, 0.586400},
            {0.777778, 0.614000},
            {0.793651, 0.641600},
            {0.809524, 0.669200},
            {0.825397, 0.696700},
            {0.841270, 0.724300},
            {0.857143, 0.751900},
            {0.873016, 0.779400},
            {0.888889, 0.807000},
            {0.904762, 0.834600},
            {0.920635, 0.862100},
            {0.936508, 0.889700},
            {0.952381, 0.917300},
            {0.968254, 0.944900},
            {0.984127, 0.972400},
            {1.000000, 1.000000}
        },
        /* blue */  {
            {0.000000, 1.000000},
            {0.015873, 1.000000},
            {0.031746, 1.000000},
            {0.047619, 1.000000},
            {0.063492, 1.000000},
            {0.079365, 1.000000},
            {0.095238, 1.000000},
            {0.111111, 1.000000},
            {0.126984, 1.000000},
            {0.142857, 1.000000},
            {0.158730, 1.000000},
            {0.174603, 0.996100},
            {0.190476, 0.977800},
            {0.206349, 0.922900},
            {0.222222, 0.839000},
            {0.238095, 0.747500},
            {0.253968, 0.666900},
            {0.269841, 0.591900},
            {0.285714, 0.516800},
            {0.301587, 0.443000},
            {0.317460, 0.373200},
            {0.333333, 0.307800},
            {0.349206, 0.244100},
            {0.365079, 0.171700},
            {0.380952, 0.112900},
            {0.396825, 0.083600},
            {0.412698, 0.064500},
            {0.428571, 0.046000},
            {0.444444, 0.028100},
            {0.460317, 0.013400},
            {0.476190, 0.004700},
            {0.492063, 0.001100},
            {0.507937, 0.000100},
            {0.523810, 0.000000},
            {0.539683, 0.000000},
            {0.555556, 0.000000},
            {0.571429, 0.000000},
            {0.587302, 0.000000},
            {0.603175, 0.000000},
            {0.619048, 0.000000},
            {0.634921, 0.000000},
            {0.650794, 0.000000},
            {0.666667, 0.000000},
            {0.682540, 0.000000},
            {0.698413, 0.000000},
            {0.714286, 0.000000},
            {0.730159, 0.000000},
            {0.746032, 0.000000},
            {0.761905, 0.000000},
            {0.777778, 0.000000},
            {0.793651, 0.000000},
            {0.809524, 0.000000},
            {0.825397, 0.000000},
            {0.841270, 0.000000},
            {0.857143, 0.000000},
            {0.873016, 0.000000},
            {0.888889, 0.000000},
            {0.904762, 0.000000},
            {0.920635, 0.000000},
            {0.936508, 0.000000},
            {0.952381, 0.000000},
            {0.968254, 0.000000},
            {0.984127, 0.000000},
            {1.000000, 0.000000}
        },
        /* alpha */ {
            {0., 1.0},
            {1., 1.0}
        }
    };
    addColourmap("pv", new MLinearSegmentedColourmap(pv_data));


    MColourmapInterpolationNodes ylgnbu_data = {
        /* red */   {{0.000, 1.0},
                     {0.125, 0.92941176891326904},
                     {0.250, 0.78039216995239258},
                     {0.375, 0.49803921580314636},
                     {0.500, 0.25490197539329529},
                     {0.625, 0.11372549086809158},
                     {0.750, 0.13333334028720856},
                     {0.875, 0.14509804546833038},
                     {1.000, 0.031372550874948502}},
        /* green */ {{0.000, 1.0},
                     {0.125, 0.97254902124404907},
                     {0.250, 0.91372549533843994},
                     {0.375, 0.80392158031463623},
                     {0.500, 0.7137255072593689},
                     {0.625, 0.56862747669219971},
                     {0.750, 0.36862745881080627},
                     {0.875, 0.20392157137393951},
                     {1.000, 0.11372549086809158}},
        /* blue */  {{0.000, 0.85098040103912354},
                     {0.125, 0.69411766529083252},
                     {0.250, 0.70588237047195435},
                     {0.375, 0.73333334922790527},
                     {0.500, 0.76862746477127075},
                     {0.625, 0.75294119119644165},
                     {0.750, 0.65882354974746704},
                     {0.875, 0.58039218187332153},
                     {1.000, 0.34509804844856262}},
        /* alpha */ {{0., 1.0},
                     {1., 1.0}}
    };
    addColourmap("YlGnBu", new MLinearSegmentedColourmap(ylgnbu_data));


#define SCALE_PVU_TO_01(x) ((x+2.)/10.)
    MColourmapInterpolationNodes pv_eth_data = {
        /* red */   {{SCALE_PVU_TO_01(-2.), 142./255.},
                     {SCALE_PVU_TO_01(0.-S), 142./255.},
                     {SCALE_PVU_TO_01(0.), 181./255.},
                     {SCALE_PVU_TO_01(0.2-S), 181./255.},
                     {SCALE_PVU_TO_01(0.2), 214./255.},
                     {SCALE_PVU_TO_01(0.5-S), 214./255.},
                     {SCALE_PVU_TO_01(0.5), 242./255.},
                     {SCALE_PVU_TO_01(0.8-S), 242./255.},
                     {SCALE_PVU_TO_01(0.8), 239./255.},
                     {SCALE_PVU_TO_01(1.-S), 239./255.},
                     {SCALE_PVU_TO_01(1.), 242./255.},
                     {SCALE_PVU_TO_01(1.5-S), 242./255.},
                     {SCALE_PVU_TO_01(1.5), 220./255.},
                     {SCALE_PVU_TO_01(2.-S), 220./255.},
                     {SCALE_PVU_TO_01(2.), 255./255.},
                     {SCALE_PVU_TO_01(3.-S), 255./255.},
                     {SCALE_PVU_TO_01(3.), 255./255.},
                     {SCALE_PVU_TO_01(4.-S), 255./255.},
                     {SCALE_PVU_TO_01(4.), 255./255.},
                     {SCALE_PVU_TO_01(6.-S), 255./255.},
                     {SCALE_PVU_TO_01(6.), 170./255.},
                     {SCALE_PVU_TO_01(8.), 170./255.}},

        /* green */ {{SCALE_PVU_TO_01(-2.), 178./255.},
                     {SCALE_PVU_TO_01(0.-S), 178./255.},
                     {SCALE_PVU_TO_01(0.), 201./255.},
                     {SCALE_PVU_TO_01(0.2-S), 201./255.},
                     {SCALE_PVU_TO_01(0.2), 226./255.},
                     {SCALE_PVU_TO_01(0.5-S), 226./255.},
                     {SCALE_PVU_TO_01(0.5), 221./255.},
                     {SCALE_PVU_TO_01(0.8-S), 221./255.},
                     {SCALE_PVU_TO_01(0.8), 193./255.},
                     {SCALE_PVU_TO_01(1.-S), 193./255.},
                     {SCALE_PVU_TO_01(1.), 132./255.},
                     {SCALE_PVU_TO_01(1.5-S), 132./255.},
                     {SCALE_PVU_TO_01(1.5), 60./255.},
                     {SCALE_PVU_TO_01(2.-S), 60./255.},
                     {SCALE_PVU_TO_01(2.), 120./255.},
                     {SCALE_PVU_TO_01(3.-S), 120./255.},
                     {SCALE_PVU_TO_01(3.), 190./255.},
                     {SCALE_PVU_TO_01(4.-S), 190./255.},
                     {SCALE_PVU_TO_01(4.), 249./255.},
                     {SCALE_PVU_TO_01(6.-S), 249./255.},
                     {SCALE_PVU_TO_01(6.), 255./255.},
                     {SCALE_PVU_TO_01(8.), 255./255.}},

        /* blue */  {{SCALE_PVU_TO_01(-2.), 255./255.},
                     {SCALE_PVU_TO_01(0.-S), 255./255.},
                     {SCALE_PVU_TO_01(0.), 255./255.},
                     {SCALE_PVU_TO_01(0.2-S), 255./255.},
                     {SCALE_PVU_TO_01(0.2), 237./255.},
                     {SCALE_PVU_TO_01(0.5-S), 237./255.},
                     {SCALE_PVU_TO_01(0.5), 160./255.},
                     {SCALE_PVU_TO_01(0.8-S), 160./255.},
                     {SCALE_PVU_TO_01(0.8), 130./255.},
                     {SCALE_PVU_TO_01(1.-S), 130./255.},
                     {SCALE_PVU_TO_01(1.), 68./255.},
                     {SCALE_PVU_TO_01(1.5-S), 68./255.},
                     {SCALE_PVU_TO_01(1.5), 30./255.},
                     {SCALE_PVU_TO_01(2.-S), 30./255.},
                     {SCALE_PVU_TO_01(2.), 20./255.},
                     {SCALE_PVU_TO_01(3.-S), 20./255.},
                     {SCALE_PVU_TO_01(3.), 20./255.},
                     {SCALE_PVU_TO_01(4.-S), 20./255.},
                     {SCALE_PVU_TO_01(4.), 20./255.},
                     {SCALE_PVU_TO_01(6.-S), 20./255.},
                     {SCALE_PVU_TO_01(6.), 60./255.},
                     {SCALE_PVU_TO_01(8.), 60./255.}},
        /* alpha */ {{0., 1.0},
                     {1., 1.0}}
    };
    addColourmap("pv_eth", new MLinearSegmentedColourmap(pv_eth_data));

#define SCALE_PVU_TO_01_b(x) ((x+6.)/12.)
    MColourmapInterpolationNodes pv_error_data = {
        /* red */   {{SCALE_PVU_TO_01_b(-6.), 175./255.},
                     {SCALE_PVU_TO_01_b(-5.-S), 175./255.},
                     {SCALE_PVU_TO_01_b(-5.), 135./255.},
                     {SCALE_PVU_TO_01_b(-4.-S), 135./255.},
                     {SCALE_PVU_TO_01_b(-4.), 70./255.},
                     {SCALE_PVU_TO_01_b(-3.-S), 70./255.},
                     {SCALE_PVU_TO_01_b(-3), 100./255.},
                     {SCALE_PVU_TO_01_b(-2.-S), 100./255.},
                     {SCALE_PVU_TO_01_b(-2.), 204./255.},
                     {SCALE_PVU_TO_01_b(-1.-S), 204./255.},
                     {SCALE_PVU_TO_01_b(-1.), 255./255.},
                     {SCALE_PVU_TO_01_b(1.-S), 255./255.},
                     {SCALE_PVU_TO_01_b(1.), 255./255.},
                     {SCALE_PVU_TO_01_b(2.-S), 255./255.},
                     {SCALE_PVU_TO_01_b(2.), 255./255.},
                     {SCALE_PVU_TO_01_b(3.-S), 255./255.},
                     {SCALE_PVU_TO_01_b(3.), 205./255.},
                     {SCALE_PVU_TO_01_b(4.-S), 205./255.},
                     {SCALE_PVU_TO_01_b(4.), 238./255.},
                     {SCALE_PVU_TO_01_b(5.-S), 238./255.},
                     {SCALE_PVU_TO_01_b(5.), 255./255.},
                     {SCALE_PVU_TO_01_b(6.), 255./255.}},

        /* green */ {{SCALE_PVU_TO_01_b(-6.), 238./255.},
                     {SCALE_PVU_TO_01_b(-5.-S), 238./255.},
                     {SCALE_PVU_TO_01_b(-5.), 206./255.},
                     {SCALE_PVU_TO_01_b(-4.-S), 206./255.},
                     {SCALE_PVU_TO_01_b(-4.), 130./255.},
                     {SCALE_PVU_TO_01_b(-3.-S), 130./255.},
                     {SCALE_PVU_TO_01_b(-3), 100./255.},
                     {SCALE_PVU_TO_01_b(-2.-S), 100./255.},
                     {SCALE_PVU_TO_01_b(-2.), 204./255.},
                     {SCALE_PVU_TO_01_b(-1.-S), 204./255.},
                     {SCALE_PVU_TO_01_b(-1.), 255./255.},
                     {SCALE_PVU_TO_01_b(1.-S), 255./255.},
                     {SCALE_PVU_TO_01_b(1.), 204./255.},
                     {SCALE_PVU_TO_01_b(2.-S), 204./255.},
                     {SCALE_PVU_TO_01_b(2.), 81./255.},
                     {SCALE_PVU_TO_01_b(3.-S), 81./255.},
                     {SCALE_PVU_TO_01_b(3.), 55./255.},
                     {SCALE_PVU_TO_01_b(4.-S), 55./255.},
                     {SCALE_PVU_TO_01_b(4.), 118./255.},
                     {SCALE_PVU_TO_01_b(5.-S), 118./255.},
                     {SCALE_PVU_TO_01_b(5.), 165./255.},
                     {SCALE_PVU_TO_01_b(6.), 165./255.}},

        /* blue */  {{SCALE_PVU_TO_01_b(-6.), 238./255.},
                     {SCALE_PVU_TO_01_b(-5.-S), 238./255.},
                     {SCALE_PVU_TO_01_b(-5.), 235./255.},
                     {SCALE_PVU_TO_01_b(-4.-S), 235./255.},
                     {SCALE_PVU_TO_01_b(-4.), 180./255.},
                     {SCALE_PVU_TO_01_b(-3.-S), 180./255.},
                     {SCALE_PVU_TO_01_b(-3), 255./255.},
                     {SCALE_PVU_TO_01_b(-2.-S), 255./255.},
                     {SCALE_PVU_TO_01_b(-2.), 255./255.},
                     {SCALE_PVU_TO_01_b(-1.-S), 255./255.},
                     {SCALE_PVU_TO_01_b(-1.), 255./255.},
                     {SCALE_PVU_TO_01_b(1.-S), 255./255.},
                     {SCALE_PVU_TO_01_b(1.), 204./255.},
                     {SCALE_PVU_TO_01_b(2.-S), 204./255.},
                     {SCALE_PVU_TO_01_b(2.), 81./255.},
                     {SCALE_PVU_TO_01_b(3.-S), 81./255.},
                     {SCALE_PVU_TO_01_b(3.), 0./255.},
                     {SCALE_PVU_TO_01_b(4.-S), 0./255.},
                     {SCALE_PVU_TO_01_b(4.), 0./255.},
                     {SCALE_PVU_TO_01_b(5.-S), 0./255.},
                     {SCALE_PVU_TO_01_b(5.), 0./255.},
                     {SCALE_PVU_TO_01_b(6.), 0./255.}},
        /* alpha */ {{0., 1.0},
                     {1., 1.0}}
    };
    addColourmap("pv_error", new MLinearSegmentedColourmap(pv_error_data));
}

} // namespace Met3D
