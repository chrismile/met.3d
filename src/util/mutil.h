/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015-2017 Bianca Tost
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
#ifndef MUTIL_H
#define MUTIL_H

// standard library imports
#include <typeinfo>

// related third party imports
#include <QtCore>
#include "log4cplus/logger.h"

// local application imports


/******************************************************************************
***                      VERSION INFORMATION                                ***
*******************************************************************************/

const QString met3dVersionString = "1.2.2";
// String containing default value for missing version number in config.
const QString defaultConfigVersion = "1.0.0";
const QString met3dBuildDate = QString("built on %1 %2").arg(__DATE__).arg(__TIME__);


/******************************************************************************
***                             LOGGING                                     ***
*******************************************************************************/

// Global reference to the Met3D application logger (defined in mutil.cpp).
extern log4cplus::Logger mlog;


/******************************************************************************
***                              MACROS                                     ***
*******************************************************************************/

// Shortcut for OpenGL error checking.
#define GLErr(x) x; checkOpenGLError(__FILE__, __LINE__)
#define CHECK_GL_ERROR checkOpenGLError(__FILE__, __LINE__)

// Define a floating point modulo function that behaves like the Python
// modulo, i.e. -40.2 mod 360. == 319.8 and NOT -40.2, as the C++ fmod()
// function does. Required for cyclic grids.
#define MMOD(a, b) ((a) - floor((a) / (b)) * (b))

#define MFRACT(a) ((a) - int(a))

#define MMIX(x, y, a) (((x) * (1.-(a))) + ((y)*(a)))


/******************************************************************************
***                 DEFINES COMMON TO THE ENTIRE SYSTEM                     ***
*******************************************************************************/

// Maximum number of OpenGL contexts that can display a scene.
#define MET3D_MAX_SCENEVIEWS 4

#define M_MISSING_VALUE -999.E9f
#define M_INVALID_TRAJECTORY_POS -999.99f

//WORKAROUND
    // NOTE (mr, Dec2013): Workaround to fix a float accuracy problem
    // occuring with some NetCDF data files (e.g. converted from GRIB with
    // netcdf-java): For example, such longitude arrays can occur:
    // -18, -17, -16, -15, -14, -13, -12, -11, -10, -9.000004, -8.000004,
    // The latter should be equal to -9.0, -8.0 etc. The inaccuracy causes
    // problems in MNWP2DHorizontalActorVariable::computeRenderRegionParameters()
    // and MClimateForecastReader::readGrid(), hence we compare to this
    // absolute epsilon to determine equality of two float values.
    // THIS WORKAROUND NEEDS TO BE REMOVED WHEN HIGHER RESOLUTIONS THAN 0.00001
    // ARE HANDLED BY MET.3D.
    // Cf. http://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
    // for potentially better solutions.
#define M_LONLAT_RESOLUTION 0.00001


/******************************************************************************
***                            FUNCTIONS                                    ***
*******************************************************************************/

void checkOpenGLError(const char* file="", int line=-1);

inline float clamp(float x, float a, float b)
{
    return x < a ? a : (x > b ? b : x);
}

inline float clamp(double x, double a, double b)
{
    return x < a ? a : (x > b ? b : x);
}

inline int clamp(int x, int a, int b)
{
    return x < a ? a : (x > b ? b : x);
}

QStringList readConfigVersionID(QSettings *settings);

/**
  Expands environment variables of format $VARIABLE in the string @p path.
  Example: If the envrionment variable "MET3D_HOME" is set to "/home/user/m3d",
  the path "$MET3D_HOME/config/data" would be expaned to
  "/home/user/m3d/config/data".
 */
QString expandEnvironmentVariables(QString path);

#endif // MUTIL_H
