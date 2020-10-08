/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2017-2018 Bianca Tost [+]
**  Copyright 2020      Andreas Beckert [*]
**
**  Regional Computing Center, Visual Data Analysis Group
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
#include "structuredgrid.h"

// standard library imports
#include <iostream>
#include <limits>

// related third party imports
#include <netcdf>
#include <log4cplus/loggingmacros.h>
#include <gsl/gsl_interp.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "util/mstopwatch.h"
#include "util/metroutines.h"
#include "gxfw/mglresourcesmanager.h"
#include "data/abstractmemorymanager.h"

using namespace std;
using namespace netCDF;
using namespace netCDF::exceptions;


namespace Met3D
{

/******************************************************************************
***                             MStructuredGrid                             ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MStructuredGrid::MStructuredGrid(MVerticalLevelType leveltype,
                                 unsigned int nlevs, unsigned int nlats,
                                 unsigned int nlons)
    : MAbstractDataItem(),
      nlevs(nlevs),
      nlats(nlats),
      nlons(nlons),
      nvalues(nlevs * nlats * nlons),
      nlatsnlons(nlats * nlons),
      levels(new double[nlevs]),
      lats(new double[nlats]),
      lons(new double[nlons]),
      data(new float[nlevs * nlats * nlons]),
      flags(nullptr),
      flagsCanBeEnabled(true),
      contributingMembers(0),
      availableMembers(0),
      horizontalGridType(REGULAR_LONLAT),
      leveltype(leveltype),
      minMaxAccel(nullptr)
{
    lonlatID = getID() + "ll";
    flagsID = getID() + "fl";
    minMaxAccelID = getID() + "accel";
    setTextureParameters();

    /*
    switch (dataType)
    {
    case FLOAT_VALUES:
        gridValues = new MFloatGridValues(nvalues);
        break;
    case UINT64_VALUES:
        gridValues = new MUInt64GridValues(nvalues);
        break;
    }
    */
}


MStructuredGrid::~MStructuredGrid()
{
    // Make sure the corresponding data is removed from GPU memory as well.
    MGLResourcesManager::getInstance()->releaseAllGPUItemReferences(getID());
    MGLResourcesManager::getInstance()->releaseAllGPUItemReferences(lonlatID);
    MGLResourcesManager::getInstance()->releaseAllGPUItemReferences(flagsID);
    MGLResourcesManager::getInstance()->releaseAllGPUItemReferences(minMaxAccelID);

    if (minMaxAccel)
    {
        if (memoryManager)
            memoryManager->releaseData(minMaxAccel);
        else
            delete minMaxAccel;
    }

    delete[] levels;
    delete[] lats;
    delete[] lons;
    delete[] data;
    if (flags != nullptr) delete[] flags;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

unsigned int MStructuredGrid::getMemorySize_kb()
{
    // If this method was called, the flags shouldn't be enabled anymore --
    // the memory size of this object changes a lot through the additional
    // memory allocation.
    flagsCanBeEnabled = false;

    return ( sizeof(MStructuredGrid)
             + (nlevs + nlats + nlons) * sizeof(double)
             + nvalues * sizeof(float)
             + ((flags != nullptr) ? nvalues : 0) * sizeof(quint64)
             ) / 1024.;
}


QString MStructuredGrid::verticalLevelTypeToString(MVerticalLevelType type)
{
    switch (type)
    {
    case SURFACE_2D:
        return QString("Surface");
    case PRESSURE_LEVELS_3D:
        return QString("Pressure Levels");
    case HYBRID_SIGMA_PRESSURE_3D:
        return QString("Hybrid Sigma Pressure Model Levels");
    case POTENTIAL_VORTICITY_2D:
        return QString("Potential Vorticity Levels");
    case LOG_PRESSURE_LEVELS_3D:
        return QString("Log(Pressure) Levels");
    case AUXILIARY_PRESSURE_3D:
        return QString("Model Levels with Auxiliary Pressure");
    default:
        return QString("UNDEFINED");
    }
}


MVerticalLevelType MStructuredGrid::verticalLevelTypeFromString(
        const QString &str)
{
    if (str == "Surface") return SURFACE_2D;
    if (str == "Pressure Levels") return PRESSURE_LEVELS_3D;
    if (str == "Hybrid Sigma Pressure Model Levels") return HYBRID_SIGMA_PRESSURE_3D;
    if (str == "Potential Vorticity Levels") return POTENTIAL_VORTICITY_2D;
    if (str == "Log(Pressure) Levels") return LOG_PRESSURE_LEVELS_3D;
    if (str == "Model Levels with Auxiliary Pressure") return AUXILIARY_PRESSURE_3D;

    return SIZE_LEVELTYPES;
}


MVerticalLevelType MStructuredGrid::verticalLevelTypeFromConfigString(
        const QString &str)
{
    if (str == "SURFACE_2D") return SURFACE_2D;
    if (str == "PRESSURE_LEVELS_3D") return PRESSURE_LEVELS_3D;
    if (str == "HYBRID_SIGMA_PRESSURE_3D") return HYBRID_SIGMA_PRESSURE_3D;
    if (str == "POTENTIAL_VORTICITY_2D") return POTENTIAL_VORTICITY_2D;
    if (str == "LOG_PRESSURE_LEVELS_3D") return LOG_PRESSURE_LEVELS_3D;
    if (str == "AUXILIARY_PRESSURE_3D") return AUXILIARY_PRESSURE_3D;

    return SIZE_LEVELTYPES;
}


float MStructuredGrid::min()
{
    float min = numeric_limits<float>::max();
    for (unsigned int i = 0; i < nvalues; i++)
        if (data[i] != M_MISSING_VALUE && data[i] < min) min = data[i];
    return min;
}


float MStructuredGrid::max()
{
    float max = numeric_limits<float>::lowest();
    for (unsigned int i = 0; i < nvalues; i++)
        if (data[i] != M_MISSING_VALUE && data[i] > max) max = data[i];
    return max;
}


void MStructuredGrid::maskRectangularRegion(unsigned int i0,
                                            unsigned int j0,
                                            unsigned int k0,
                                            unsigned int ni,
                                            unsigned int nj,
                                            unsigned int nk)
{
    // Account for grids potentially being cyclic in longitude.
    i0 = i0 % nlons;
    unsigned int i1 = (i0 + ni) % nlons;

    unsigned int j1 = j0 + nj;
    unsigned int k1 = k0 + nk;

    if (i0 <= i1)
    {
        // Mask everything from 0..i0 and from i1..nlons.
        for (unsigned int k = 0; k < nlevs; k++)
            for (unsigned int j = 0; j < nlats; j++)
            {
                for (unsigned int i = 0; i < i0; i++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
                for (unsigned int i = i1+1; i < nlons; i++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
            }


        for (unsigned int k = 0; k < nlevs; k++)
            for (unsigned int i = i0; i <= i1; i++)
            {
                for (unsigned int j = 0; j < j0; j++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
                for (unsigned int j = j1+1; j < nlats; j++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
            }


        for (unsigned int j = j0; j <= j1; j++)
            for (unsigned int i = i0; i <= i1; i++)
            {
                for (unsigned int k = 0; k < k0; k++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
                for (unsigned int k = k1+1; k < nlevs; k++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
            }
    }
    else
    {
        // Mask everything from i1..i0.
        for (unsigned int k = 0; k < nlevs; k++)
            for (unsigned int j = 0; j < nlats; j++)
            {
                for (unsigned int i = i1+1; i < i0; i++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
            }

        for (unsigned int k = 0; k < nlevs; k++)
        {
            for (unsigned int i = 0; i < i1; i++)
            {
                for (unsigned int j = 0; j < j0; j++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
                for (unsigned int j = j1+1; j < nlats; j++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
            }
            for (unsigned int i = i0+1; i < nlons; i++)
            {
                for (unsigned int j = 0; j < j0; j++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
                for (unsigned int j = j1+1; j < nlats; j++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
            }
        }

        for (unsigned int j = j0; j <= j1; j++)
        {
            for (unsigned int i = 0; i < i1; i++)
            {
                for (unsigned int k = 0; k < k0; k++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
                for (unsigned int k = k1+1; k < nlevs; k++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
            }
            for (unsigned int i = i0+1; i < nlons; i++)
            {
                for (unsigned int k = 0; k < k0; k++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
                for (unsigned int k = k1+1; k < nlevs; k++)
                    data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = M_MISSING_VALUE;
            }
        }
    }
}


void MStructuredGrid::setToZero()
{
    for (unsigned int i = 0; i < nvalues; i++) data[i] = 0.;
}


void MStructuredGrid::setToValue(float val)
{
    for (unsigned int i = 0; i < nvalues; i++) data[i] = val;
}


void MStructuredGrid::findEnclosingHorizontalIndices(
        float lon, float lat, int *i, int *j, int *i1, int *j1,
        float *mixI, float *mixJ)
{
    // Find horizontal indices i,i+1 and j,j+1 that enclose lon, lat.
    *mixI = MMOD(lon - lons[0], 360.) / fabs(lons[1]-lons[0]);
    *mixJ = (lats[0] - lat) / fabs(lats[1]-lats[0]);
    *i = int(*mixI);
    *j = int(*mixJ);

    *i1 = (*i)+1;
    if (gridIsCyclicInLongitude()) *i1 %= nlons;
    *j1 = (*j)+1;
}


float MStructuredGrid::interpolateValue(float lon, float lat, float p_hPa)
{
    int i, j, i1, j1;
    float mixI, mixJ;
    findEnclosingHorizontalIndices(lon, lat, &i, &j, &i1, &j1, &mixI, &mixJ);

    if ((i < 0) || (j < 0) || (i1 >= int(nlons)) || (j1 >= int(nlats)))
        return M_MISSING_VALUE;

    // Get scalar values at the four surrounding grid columns, interpolated
    // to p_hPa.
    float scalar_i0j0 = interpolateGridColumnToPressure(j , i , p_hPa);
    float scalar_i1j0 = interpolateGridColumnToPressure(j , i1, p_hPa);
    float scalar_i0j1 = interpolateGridColumnToPressure(j1, i , p_hPa);
    float scalar_i1j1 = interpolateGridColumnToPressure(j1, i1, p_hPa);

    // Interpolate horizontally.
    mixJ = MFRACT(mixJ); // fract(mixJ) in GLSL
    float scalar_i0 = MMIX(scalar_i0j0, scalar_i0j1, mixJ);
    float scalar_i1 = MMIX(scalar_i1j0, scalar_i1j1, mixJ);

    mixI = MFRACT(mixI);
    float scalar = MMIX(scalar_i0, scalar_i1, mixI);

    return scalar;
}


float MStructuredGrid::interpolateValue(QVector3D vec3_lonLatP)
{
    return interpolateValue(vec3_lonLatP.x(), vec3_lonLatP.y(), vec3_lonLatP.z());
}


float MStructuredGrid::interpolateValueOnLevel(
        float lon, float lat, unsigned int k)
{
    int i, j, i1, j1;
    float mixI, mixJ;
    findEnclosingHorizontalIndices(lon, lat, &i, &j, &i1, &j1, &mixI, &mixJ);

    if ((i < 0) || (j < 0) || (i1 >= int(nlons)) || (j1 >= int(nlats)))
        return M_MISSING_VALUE;

    // Get scalar values at the four surrounding grid points of the level.
    float scalar_i0j0 = getValue(k, j , i);
    float scalar_i1j0 = getValue(k, j , i1);
    float scalar_i0j1 = getValue(k, j1, i);
    float scalar_i1j1 = getValue(k, j1, i1);

    // Interpolate horizontally.
    mixJ = MFRACT(mixJ); // fract(mixJ) in GLSL
    float scalar_i0 = MMIX(scalar_i0j0, scalar_i0j1, mixJ);
    float scalar_i1 = MMIX(scalar_i1j0, scalar_i1j1, mixJ);

    mixI = MFRACT(mixI);
    float scalar = MMIX(scalar_i0, scalar_i1, mixI);

    return scalar;
}


QVector<QVector2D> MStructuredGrid::extractVerticalProfile(float lon, float lat)
{
    QVector<QVector2D> profile;

    for (unsigned int k = 0; k < nlevs; k++)
    {
        float scalar = interpolateValueOnLevel(lon, lat, k);
        float pressure_hPa = levelPressureAtLonLat_hPa(lon, lat, k);
        profile.append(QVector2D(scalar, pressure_hPa));
    }

    return profile;
}


bool MStructuredGrid::findTopGridIndices(
        float lon, float lat, float p_hPa,
        MIndex3D *nw, MIndex3D *ne, MIndex3D *sw, MIndex3D *se)
{
    int i, j, i1, j1;
    float mixI, mixJ;
    findEnclosingHorizontalIndices(lon, lat, &i, &j, &i1, &j1, &mixI, &mixJ);

    nw->i = i;  nw->j = j;  nw->k = findLevel(nw->j, nw->i, p_hPa);
    ne->i = i1; ne->j = j;  ne->k = findLevel(ne->j, ne->i, p_hPa);
    sw->i = i;  sw->j = j1; sw->k = findLevel(sw->j, sw->i, p_hPa);
    se->i = i1; se->j = j1; se->k = findLevel(se->j, se->i, p_hPa);

    // Check if indices are inside the grid domain.
    if ((i < 0) || (i1 >= int(nlons))) return false;
    if ((j < 0) || (j1 >= int(nlats))) return false;
    if ((nw->k < 0) || (nw->k+1 >= int(nlevs))) return false;
    if ((ne->k < 0) || (ne->k+1 >= int(nlevs))) return false;
    if ((sw->k < 0) || (sw->k+1 >= int(nlevs))) return false;
    if ((se->k < 0) || (se->k+1 >= int(nlevs))) return false;

    return true;
}


bool MStructuredGrid::findTopGridIndices(
        QVector3D vec3_lonLatP,
        MIndex3D *nw, MIndex3D *ne, MIndex3D *sw, MIndex3D *se)
{
    return findTopGridIndices(vec3_lonLatP.x(), vec3_lonLatP.y(),
                              vec3_lonLatP.z(), nw, ne, sw, se);
}


int MStructuredGrid::findClosestLevel(
        unsigned int j, unsigned int i, float p_hPa)
{
    int k = findLevel(j, i, p_hPa);
    if (k == int(nlevs-1)) return k;

    float p_k = getPressure(k, j, i);
    float p_k1 = getPressure(k+1, j, i);

    return (fabs(p_k - p_hPa) < fabs(p_k1 - p_hPa)) ? k : k+1;
}


MIndex3D MStructuredGrid::maxNeighbouringGridPoint(
        float lon, float lat, float p_hPa)
{
    MIndex3D nw, ne, sw, se;
    bool insideGridVolume = findTopGridIndices(lon, lat, p_hPa,
                                               &nw, &ne, &sw, &se);

    MIndex3D maxPoint;
    // If the given position is outside the grid domain maxPoint remains an
    // invalid index.
    if (insideGridVolume)
    {
        if (getValue(nw.k+1, nw.j, nw.i) > getValue(nw.k, nw.j, nw.i)) nw.k += 1;
        if (getValue(ne.k+1, ne.j, ne.i) > getValue(ne.k, ne.j, ne.i)) ne.k += 1;
        if (getValue(sw.k+1, sw.j, sw.i) > getValue(sw.k, sw.j, sw.i)) sw.k += 1;
        if (getValue(se.k+1, se.j, se.i) > getValue(se.k, se.j, se.i)) se.k += 1;

        maxPoint = nw;
        if (getValue(ne) > getValue(maxPoint)) maxPoint = ne;
        if (getValue(sw) > getValue(maxPoint)) maxPoint = sw;
        if (getValue(se) > getValue(maxPoint)) maxPoint = se;
    }

    return maxPoint;
}


MIndex3D MStructuredGrid::maxNeighbouringGridPoint(QVector3D vec3_lonLatP)
{
    return maxNeighbouringGridPoint(vec3_lonLatP.x(), vec3_lonLatP.y(),
                                    vec3_lonLatP.z());
}


QVector3D MStructuredGrid::getNorthWestTopDataVolumeCorner_lonlatp()
{
    return QVector3D(lons[0], lats[0], getTopDataVolumePressure_hPa());
}


QVector3D MStructuredGrid::getSouthEastBottomDataVolumeCorner_lonlatp()
{
    return QVector3D(lons[nlons-1], lats[nlats-1], getBottomDataVolumePressure_hPa());
}


bool MStructuredGrid::gridIsCyclicInLongitude()
{
    double deltaLon = lons[1] - lons[0];
    double lon_west = MMOD(lons[0], 360.);
    double lon_east = MMOD(lons[nlons-1] + deltaLon, 360.);

//WORKAROUND -- Usage of M_LONLAT_RESOLUTION defined in mutil.h
    // NOTE (mr, Dec2013): Workaround to fix a float accuracy problem
    // occuring with some NetCDF data files converted from GRIB with
    // netcdf-java): For example, such longitude arrays can occur:
    // -18, -17, -16, -15, -14, -13, -12, -11, -10, -9.000004, -8.000004,
    // The latter should be equal to -9.0, -8.0 etc. The inaccuracy causes
    // wrong indices below, hence we compare to this absolute epsilon to
    // determine equality of two float values.
    // THIS WORKAROUND NEEDS TO BE REMOVED WHEN HIGHER RESOLUTIONS THAN 0.00001
    // ARE HANDLED BY MET.3D.
    // Cf. http://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
    // for potentially better solutions.

    return ( fabs(lon_west - lon_east) < M_LONLAT_RESOLUTION );
}


void MStructuredGrid::setTextureParameters(GLint  internalFormat,
                                           GLenum format,
                                           GLint  wrap,
                                           GLint  minMaxFilter)
{
    textureInternalFormat = internalFormat;
    textureFormat = format;
    textureWrap = wrap;
    textureMinMaxFilter = minMaxFilter;
}


GL::MTexture* MStructuredGrid::getTexture(QGLWidget *currentGLContext,
                                         bool nullTexture)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MTexture *t = static_cast<GL::MTexture*>(glRM->getGPUItem(getID()));
    if (t) return t;

    // No texture with this item's data exists. Create a new one.
    t = new GL::MTexture(getID(), GL_TEXTURE_3D, textureInternalFormat,
                        nlons, nlats, nlevs);

    if ( glRM->tryStoreGPUItem(t) )
    {
        // The new texture was successfully stored in the GPU memory manger.
        // We can now upload the data.
        glRM->makeCurrent();

        t->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering (RTVG p. 64).
        // If the grid is cyclic in longitude, use "GL_REPEAT" so that
        // "texture()" works correctly in the samplers (where used, e.g.
        // for pressure level data).
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S,
                        gridIsCyclicInLongitude() ? GL_REPEAT : textureWrap);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, textureWrap);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, textureWrap);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, textureMinMaxFilter);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, textureMinMaxFilter);

        if (nullTexture)
            glTexImage3D(GL_TEXTURE_3D, 0, textureInternalFormat,
                         nlons, nlats, nlevs,
                         0, textureFormat, GL_FLOAT, NULL);
        else
            glTexImage3D(GL_TEXTURE_3D, 0, textureInternalFormat,
                         nlons, nlats, nlevs,
                         0, textureFormat, GL_FLOAT, data);
        CHECK_GL_ERROR;

        if (currentGLContext) currentGLContext->makeCurrent();
    }
    else
    {
        delete t;
    }

    return static_cast<GL::MTexture*>(glRM->getGPUItem(getID()));
}


void MStructuredGrid::releaseTexture()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(getID());
}


GL::MTexture *MStructuredGrid::getLonLatLevTexture(QGLWidget *currentGLContext)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MTexture *t = static_cast<GL::MTexture*>(glRM->getGPUItem(lonlatID));
    if (t) return t;

    // No texture with this item's data exists. Create a new one.
    t = new GL::MTexture(lonlatID, GL_TEXTURE_1D, GL_ALPHA32F_ARB,
                        nlons+nlats+nlevs);

    if ( glRM->tryStoreGPUItem(t) )
    {
        // The new texture was successfully stored in the GPU memory manger.
        // We can now upload the data.
        glRM->makeCurrent();

        t->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering.
        // NOTE: GL_NEAREST is required here to avoid interpolation between
        // discrete lat/lon values.
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // Upload data array to GPU: Create texture with no data..
        glTexImage1D(GL_TEXTURE_1D,             // target
                     0,                         // level of detail
                     GL_ALPHA32F_ARB,           // internal format
                     nlons + nlats + nlevs,     // width
                     0,                         // border
                     GL_ALPHA,                  // format
                     GL_FLOAT,                  // data type of the pixel data
                     NULL); CHECK_GL_ERROR;
        // .. convert longitude to float and upload ..
        float *lons_ = new float[nlons];
        for (unsigned int i = 0; i < nlons; i++)
            lons_[i] = float(lons[i]);
        glTexSubImage1D(GL_TEXTURE_1D, 0, 0, nlons, GL_ALPHA,
                        GL_FLOAT, lons_); CHECK_GL_ERROR;
        delete[] lons_;
        // .. and latitude data ..
        float *lats_ = new float[nlats];
        for (unsigned int i = 0; i < nlats; i++)
            lats_[i] = float(lats[i]);
        glTexSubImage1D(GL_TEXTURE_1D, 0, nlons, nlats, GL_ALPHA,
                        GL_FLOAT, lats_); CHECK_GL_ERROR;
        delete[] lats_;
        // .. and vertical level data.
        float *levs_ = new float[nlevs];
        for (unsigned int i = 0; i < nlevs; i++)
            levs_[i] = float(levels[i]);
        glTexSubImage1D(GL_TEXTURE_1D, 0, nlons+nlats, nlevs, GL_ALPHA,
                        GL_FLOAT, levs_); CHECK_GL_ERROR;
        delete[] levs_;

        if (currentGLContext) currentGLContext->makeCurrent();
    }
    else
    {
        delete t;
    }

    return static_cast<GL::MTexture*>(glRM->getGPUItem(lonlatID));
}


void MStructuredGrid::releaseLonLatLevTexture()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(lonlatID);
}


void MStructuredGrid::dumpCoordinateAxes()
{
    QString sLon = "";
    for (uint i = 0; i < nlons; i++) sLon += QString("%1 ").arg(lons[i]);
    QString sLat = "";
    for (uint i = 0; i < nlats; i++) sLat += QString("%1 ").arg(lats[i]);
    QString sLev = "";
    for (uint i = 0; i < nlevs; i++) sLev += QString("%1 ").arg(levels[i]);

    QString s = QString("\nStructured Grid Coordinate Axes\n"
                        "===============================\n\n"
                        "LON:\n%1\n\nLAT:\n%2\n\nLEV:\n%3\n\n"
                        ).arg(sLon).arg(sLat).arg(sLev);
    LOG4CPLUS_INFO(mlog, s.toStdString());
}


void MStructuredGrid::dumpGridData(unsigned int maxValues)
{
    QString str = "\n\nStructured Grid Data\n====================";
    str += QString("\nVariable name: %1").arg(variableName);
    str += QString("\nInit time: %1").arg(initTime.toString(Qt::ISODate));
    str += QString("\nValid time: %1").arg(validTime.toString(Qt::ISODate));
    str += QString("\nEnsemble member: %1").arg(ensembleMember);
    str += QString("\nGenerating request: %1").arg(getGeneratingRequest());

    str += "\n\nlon: ";
    for (uint i = 0; i < nlons; i++) str += QString("%1/").arg(lons[i]);
    str += "\n\nlat: ";
    for (uint i = 0; i < nlats; i++) str += QString("%1/").arg(lats[i]);
    str += "\n\nlev: ";
    for (uint i = 0; i < nlevs; i++) str += QString("%1/").arg(levels[i]);

    unsigned int nv = std::min(nvalues, maxValues);
    str += QString("\n\ndata (first %1 values): ").arg(nv);
    for (uint i = 0; i < nv; i++) str += QString("%1/").arg(data[i]);

    str += QString("\n\ndata (column at i=0,j=0): ");
    for (uint k = 0; k < nlevs; k++) str += QString("%1/").arg(getValue(k, 0, 0));

    str += "\n\nend data\n====================\n";

    LOG4CPLUS_INFO(mlog, str.toStdString());
}


void MStructuredGrid::saveAsNetCDF(QString filename)
{
    // See netcdf-4.1.2/examples/CXX4/pres_temp_4D_wr.cpp in the NetCDF
    // sources for an example of how to write a NetCDF file.

    try
    {
        NcFile ncFile(filename.toStdString(), NcFile::replace);

        NcDim latDimension = ncFile.addDim("latitude", nlats);
        NcDim lonDimension = ncFile.addDim("longitude", nlons);
        NcDim lvlDimension = ncFile.addDim("level", nlevs);

        NcVar latVar = ncFile.addVar("latitude", ncDouble, latDimension);
        NcVar lonVar = ncFile.addVar("longitude", ncDouble, lonDimension);
        NcVar lvlVar = ncFile.addVar("level", ncDouble, lvlDimension);

        latVar.putAtt("units", "degrees_north");
        lonVar.putAtt("units", "degrees_east");

        vector<NcDim> dimVector;
        dimVector.push_back(lvlDimension);
        dimVector.push_back(latDimension);
        dimVector.push_back(lonDimension);
        NcVar gridVar = ncFile.addVar("datafield", ncFloat, dimVector);
        gridVar.setFill(true, M_MISSING_VALUE);

        latVar.putVar(lats);
        lonVar.putVar(lons);
        lvlVar.putVar(levels);

        vector<size_t> startp,countp;
        startp.push_back(0);
        startp.push_back(0);
        startp.push_back(0);
        countp.push_back(nlevs);
        countp.push_back(nlats);
        countp.push_back(nlons);
        gridVar.putVar(startp, countp, data);
    }

    catch(NcException& e)
    {
        LOG4CPLUS_ERROR(mlog, "ERROR writing to NetCDF file: ");
        e.what();
    }
}


void MStructuredGrid::enableFlags(unsigned char numBits)
{
    if (!flagsCanBeEnabled) throw MInitialisationError(
                "Flags cannot be enabled after getMemorySize_kb() has been called.",
                __FILE__, __LINE__);

    // This can be changed later to a variable size field, e.g. with
    // std::bitset (http://www.cplusplus.com/reference/bitset/bitset/) or
    // 128bit etc. datatypews from boost::multiprecision
    // (http://www.boost.org/doc/libs/1_55_0/libs/multiprecision/doc/html/boost_multiprecision/intro.html),
    // or with boost::dynamic_bitset (http://www.boost.org/doc/libs/1_36_0/libs/dynamic_bitset/dynamic_bitset.html).
    if (numBits != 64) throw MValueError(
                "MStructuredGrid currently only supports 64bit flags.",
                __FILE__, __LINE__);

    if (flags == nullptr)
    {
        flags = new quint64[nvalues];
        clearAllFlags();
    }
}


unsigned char MStructuredGrid::flagsEnabled()
{
    return ((flags == nullptr) ? 0 : 64);
}


void MStructuredGrid::clearAllFlags()
{
    if (flags == nullptr) return;
    for (unsigned int n = 0; n < nvalues; n++) flags[n] = 0;
}


GL::MTexture *MStructuredGrid::getFlagsTexture(QGLWidget *currentGLContext)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MTexture *t = static_cast<GL::MTexture*>(glRM->getGPUItem(flagsID));
    if (t) return t;

    // No texture with this item's data exists. Create a new one.
    t = new GL::MTexture(flagsID, GL_TEXTURE_3D, GL_RG32UI,
                         nlons, nlats, nlevs);

    if ( glRM->tryStoreGPUItem(t) )
    {
        // The new texture was successfully stored in the GPU memory manger.
        // We can now upload the data.
        glRM->makeCurrent();

        t->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering (RTVG p. 64).
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, textureWrap);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, textureWrap);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, textureWrap);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexImage3D(GL_TEXTURE_3D, 0, GL_RG32UI,
                     nlons, nlats, nlevs,
                     0, GL_RG_INTEGER,  GL_UNSIGNED_INT, flags);
        CHECK_GL_ERROR;

        if (currentGLContext) currentGLContext->makeCurrent();
    }
    else
    {
        delete t;
    }

    return static_cast<GL::MTexture*>(glRM->getGPUItem(flagsID));
}


void MStructuredGrid::releaseFlagsTexture()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(flagsID);
}


unsigned int MStructuredGrid::getNumContributingMembers() const
{
    unsigned int n = 0;

    for (unsigned char bit = 0; bit < 64; bit++)
        if (contributingMembers & (Q_UINT64_C(1) << bit)) n++;

    return n;
}


unsigned char MStructuredGrid::getMaxAvailableMember() const
{
    for (unsigned char bit = 63; bit > 0; bit--)
        if (contributingMembers & (Q_UINT64_C(1) << bit)) return bit;

    // Handle bit == 0 case explicitly so loop above can terminate correctly.
    unsigned char bit = 0;
    if (contributingMembers & (Q_UINT64_C(1) << bit)) return bit;

    // No bit is set? Return 255 as error.
    return 255;
}


unsigned char MStructuredGrid::getMinAvailableMember() const
{
    for (unsigned char bit = 0; bit < 64; bit++)
        if (contributingMembers & (Q_UINT64_C(1) << bit)) return bit;

    // No bit is set? Return 255 as error.
    return 255;
}


GL::MTexture *MStructuredGrid::getMinMaxAccelTexture3D(
        QGLWidget *currentGLContext)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MTexture *t = static_cast<GL::MTexture*>(glRM->getGPUItem(minMaxAccelID));
    if (t) return t;

    // Texture does not exist in GPU memory. If the acceleration structure has
    // already been computed: Upload to GPU. Else: Compute, then upload.

    if ((nlons < 2) || (nlats < 2) || (nlevs < 2))
    {
        LOG4CPLUS_DEBUG(mlog, "Cannot create min/max acceleration structure "
                        "if any grid dimension has less than two elements.");
        return nullptr;
    }

    // Size of acceleration structure.
    int nAccLon = 32;
    int nAccLat = 32;
    int nAccLnP = 32;

    if (minMaxAccel == nullptr)
    {
#ifdef ENABLE_MET3D_STOPWATCH
        MStopwatch stopwatch;
        LOG4CPLUS_DEBUG(mlog, "Creating new acceleration structure ...");
#endif

        minMaxAccel = new MMemoryManagedArray<float>(2 * nAccLon * nAccLat * nAccLnP);
        MDataRequestHelper rh(getGeneratingRequest());
        rh.insert("AUXDATA", "MINMAXACCEL");
        minMaxAccel->setGeneratingRequest(rh.request());

        // Get data volume corners in lon/lat/pressure space.
        QVector3D nwtDataCrnr = getNorthWestTopDataVolumeCorner_lonlatp();
        QVector3D sebDataCrnr = getSouthEastBottomDataVolumeCorner_lonlatp();
        // If the grid is cyclic fill the "cycle gap", e.g. between 359. and
        // 360. -- The eastern longitude of the data grid would be 359., but
        // the region we want to use to create the acceleration strucutre would
        // be 0..360.
        if (gridIsCyclicInLongitude())
            sebDataCrnr.setX(sebDataCrnr.x() + getDeltaLat());

        // Convert pressure to ln(pressure) to divide vertically in ln(p).
        nwtDataCrnr.setZ(log(nwtDataCrnr.z()));
        sebDataCrnr.setZ(log(sebDataCrnr.z()));

        float deltaAccLon = (sebDataCrnr.x() - nwtDataCrnr.x()) / nAccLon;
        float deltaAccLat = (sebDataCrnr.y() - nwtDataCrnr.y()) / nAccLat;
        float deltaAccLnP = (sebDataCrnr.z() - nwtDataCrnr.z()) / nAccLnP;

        for (int iAcc = 0; iAcc < nAccLon; iAcc++)
        {
            // Longitudinal boundaries of current brick.
            float lonWest = nwtDataCrnr.x() + iAcc * deltaAccLon;
            float lonEast = nwtDataCrnr.x() + (iAcc + 1) * deltaAccLon;

            // Find horizontal indices iWest, iEast that enclose brick.
            float mixI = (lonWest - lons[0]) / getDeltaLon();
            int iWest = int(mixI);
            mixI = (lonEast - lons[0]) / getDeltaLon();
            int iEast = int(mixI) + 1;
            if ( !gridIsCyclicInLongitude() )
            {
                // If the grid is NOT cyclic in longitude, crop iEast to the
                // range (0 .. nlons-1).
                iEast = std::min(iEast, int(nlons - 1));
            }

            for (int jAcc = 0; jAcc < nAccLon; jAcc++)
            {
                // Latitudinal boundaries.
                float latNorth = nwtDataCrnr.y() + jAcc * deltaAccLat;
                float latSouth = nwtDataCrnr.y() + (jAcc + 1) * deltaAccLat;

                // Find horizontal indices jNorth, jSouth that enclose brick.
                float mixJ = (lats[0] - latNorth) / getDeltaLat();
                int jNorth = int(mixJ);
                mixJ = (lats[0] - latSouth) / getDeltaLat();
                int jSouth = std::min(int(mixJ) + 1, int(nlats - 1));

                for (int kAcc = 0; kAcc < nAccLon; kAcc++)
                {
                    // Vertical (ln(p)) boundaries.
                    float lnPTop = nwtDataCrnr.z() + kAcc * deltaAccLnP;
                    float lnPBot = nwtDataCrnr.z() + (kAcc + 1) * deltaAccLnP;
                    float pTop = exp(lnPTop);
                    float pBot = exp(lnPBot);

                    float min = numeric_limits<float>::max();
                    float max = numeric_limits<float>::lowest();

                    for (int i = iWest; i <= iEast; i++)
                        for (int j = jNorth; j <= jSouth; j++)
                        {
                            // For grids cyclic in longitude, i may be > nlons.
                            // Map to range 0..nlons-1.
                            int imod = i % nlons;

                            // For some level types (e.g. hybrid terrain following),
                            // the current grid column i,j might be above the
                            // current brick. Skip these cases.
                            if (pTop > getPressure(nlevs-1, j, imod)) continue;

                            // Determine min/max k with findLevel().
                            int kTop = findLevel(j, imod, pTop);
                            int kBot = std::min(findLevel(j, imod, pBot) + 1,
                                                int(nlevs - 1));

                            for (int k = kTop; k <= kBot; k++)
                            {
                                float val = data[INDEX3zyx_2(k, j, imod,
                                                             nlatsnlons, nlons)];
                                if (val < min) min = val;
                                if (val > max) max = val;
                            }
                        }

                    minMaxAccel->data[INDEX4zyxc(kAcc, jAcc, iAcc, 0,
                                                 nAccLat, nAccLon, 2)] = min;
                    minMaxAccel->data[INDEX4zyxc(kAcc, jAcc, iAcc, 1,
                                                 nAccLat, nAccLon, 2)] = max;
                }
            }
        }

//        dumpCoordinateAxes();
//        for (int kAcc = 0; kAcc < nAccLon; kAcc++)
//        {
//            QString accLevel = QString("\nLEVEL %1:\n").arg(kAcc);
//            for (int jAcc = 0; jAcc < nAccLon; jAcc++)
//            {
//                for (int iAcc = 0; iAcc < nAccLon; iAcc++)
//                {
//                    accLevel += QString("%1(%2) / ")
//                            .arg(minMaxAccel->data[INDEX4zyxc(kAcc, jAcc, iAcc, 0,
//                                                              nAccLat, nAccLon, 2)])
//                            .arg(minMaxAccel->data[INDEX4zyxc(kAcc, jAcc, iAcc, 1,
//                                                              nAccLat, nAccLon, 2)]);
//                }
//                accLevel += "\n";
//            }
//            LOG4CPLUS_DEBUG(mlog, accLevel.toStdString());
//        }

        if (memoryManager)
        {
            try
            {
                // Store in memory manager and get reference to stored item.
                // storeData() increases the fields reference counter in the
                // memory manager; the field is hence released in the
                // destructor.
                if ( !memoryManager->storeData(this, minMaxAccel) )
                {
                    // In the unlikely event that another thread has stored
                    // the same field in the mean time delete this one.
                    delete minMaxAccel;
                }
                minMaxAccel = static_cast<MMemoryManagedArray<float>*>(
                            memoryManager->getData(this, rh.request())
                            );
            }
            catch (MMemoryError)
            {
            }
        }

#ifdef ENABLE_MET3D_STOPWATCH
        stopwatch.split();
        LOG4CPLUS_DEBUG(mlog, "Acceleration structure created in "
                        << stopwatch.getElapsedTime(MStopwatch::SECONDS)
                        << " seconds.\n" << flush);
#endif
    }

    // No texture with this item's data exists. Create a new one.
    t = new GL::MTexture(minMaxAccelID, GL_TEXTURE_3D, GL_RG32F,
                         nAccLon, nAccLat, nAccLnP);

    if ( glRM->tryStoreGPUItem(t) )
    {
        // The new texture was successfully stored in the GPU memory manger.
        // We can now upload the data.
        glRM->makeCurrent();

        t->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering (RTVG p. 64).
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, textureWrap);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, textureWrap);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, textureWrap);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexImage3D(GL_TEXTURE_3D, 0, GL_RG32F,
                     nAccLon, nAccLat, nAccLnP,
                     0, GL_RG, GL_FLOAT, minMaxAccel->data);
        CHECK_GL_ERROR;

        if (currentGLContext) currentGLContext->makeCurrent();
    }
    else
    {
        delete t;
    }

    return static_cast<GL::MTexture*>(glRM->getGPUItem(minMaxAccelID));
}


void MStructuredGrid::releaseMinMaxAccelTexture3D()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(minMaxAccelID);
}


float MStructuredGrid::getDeltaLon_km(const int iLat) const
{
    float phi = abs(getLats()[iLat]) * M_PI / 180.;
    float circumferenceLatitudeCircle_km = cos(phi) * 2. * M_PI
            * MetConstants::EARTH_RADIUS_km;
    float deltaGridpoints_km = circumferenceLatitudeCircle_km
            * (getDeltaLon() / 360.);
    return deltaGridpoints_km;
}


float MStructuredGrid::getDeltaLat_km() const
{
    float circumferenceLongitudeCircle_km = 2. * M_PI
            * MetConstants::EARTH_RADIUS_km;
    float deltaGridpoints_km = circumferenceLongitudeCircle_km
            * (getDeltaLat() / 360.);
    return deltaGridpoints_km;
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                          MRegularLonLatLnPGrid                          ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MRegularLonLatLnPGrid::MRegularLonLatLnPGrid(
        unsigned int nlevs, unsigned int nlats, unsigned int nlons)
    : MStructuredGrid(LOG_PRESSURE_LEVELS_3D, nlevs, nlats, nlons)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

float MRegularLonLatLnPGrid::interpolateGridColumnToPressure(
        unsigned int j, unsigned int i, float p_hPa)
{
    float ln_p = log(p_hPa);
    float deltaLnP = (levels[0] - levels[nlevs-1]) / (nlevs-1);

    float mixK = (levels[0] - ln_p) / deltaLnP;
    int k = int(mixK);

    if ((k < 0) || (k+1 >= int(nlevs))) return M_MISSING_VALUE;

    float scalar_k  = getValue(k  , j, i);
    float scalar_k1 = getValue(k+1, j, i);

    mixK = MFRACT(mixK);
    return MMIX(scalar_k, scalar_k1, mixK);
}


float MRegularLonLatLnPGrid::levelPressureAtLonLat_hPa(
        float lon, float lat, unsigned int k)
{
    Q_UNUSED(lon);
    Q_UNUSED(lat);
    return exp(levels[k]);
}


int MRegularLonLatLnPGrid::findLevel(
        unsigned int j, unsigned int i, float p_hPa)
{
    Q_UNUSED(j);
    Q_UNUSED(i);

    float ln_p = log(p_hPa);
    float deltaLnP = (levels[0] - levels[nlevs-1]) / (nlevs-1);

    float mixK = (levels[0] - ln_p) / deltaLnP;
    int k = int(mixK);

    return k;
}


float MRegularLonLatLnPGrid::getPressure(
        unsigned int k, unsigned int j, unsigned int i)
{
    Q_UNUSED(j);
    Q_UNUSED(i);
    return exp(levels[k]);
}


float MRegularLonLatLnPGrid::getBottomInterfacePressure(
        unsigned int k, unsigned int j, unsigned int i)
{
    Q_UNUSED(j);
    Q_UNUSED(i);

    if (k == nlevs-1) return exp(levels[k]);

    return exp((levels[k] + levels[k+1]) / 2.);
}


float MRegularLonLatLnPGrid::getTopInterfacePressure(
        unsigned int k, unsigned int j, unsigned int i)
{
    Q_UNUSED(j);
    Q_UNUSED(i);

    if (k == 0) return exp(levels[k]);

    return exp((levels[k] + levels[k-1]) / 2.);
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                   MRegularLonLatStructuredPressureGrid                  ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MRegularLonLatStructuredPressureGrid::MRegularLonLatStructuredPressureGrid(
        unsigned int nlevs, unsigned int nlats, unsigned int nlons)
    : MStructuredGrid(PRESSURE_LEVELS_3D, nlevs, nlats, nlons)
{
    pressureTableID = getID() + "ptbl";
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

GL::MTexture *MRegularLonLatStructuredPressureGrid::getPressureTexCoordTexture1D(
        QGLWidget *currentGLContext)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MTexture *t = static_cast<GL::MTexture*>(glRM->getGPUItem(pressureTableID));
    if (t) return t;

    // No texture with this item's data exists. Create a new table and texture.

    // 1) Compute texture coordinates for each p-level. For e.g. 6 levels,
    // texture coordinates will be 1/12, 3/12, 5/12, ...

    // double lnPLevels[nlevs], texCoordsPLevels[nlevs]; VLAs not supported in msvc
    double* lnPLevels = new double[nlevs];
    double* texCoordsPLevels = new double[nlevs];
    for (uint i = 0; i < nlevs; ++i)
    {
        lnPLevels[i] = log(levels[i]);
        texCoordsPLevels[i] = (2.*i+1.) / (2.*nlevs);
//        cout << i << " " << lnPLevels[i] << " " << levels[i] << " "
//             << texCoordsPLevels[i] << endl;
    }

    // Set up interpolation from ln(p) to texture coordinate.
    gsl_interp_accel *gslLnPToTexCoordAcc = gsl_interp_accel_alloc();
    gsl_interp       *gslLnPToTexCoord = gsl_interp_alloc(gsl_interp_linear,
                                                          nlevs);
    gsl_interp_init(gslLnPToTexCoord, lnPLevels, texCoordsPLevels, nlevs);

    // 2) Create regular ln(p) table with nTable levels. For each level in
    // ln(p) table, compute texture coordinate via linear interpolation in
    // ln(p).

    uint   nTable = 2048;
    double lnPbot = log(levels[nlevs-1]);
    double lnPtop = log(levels[0]);
    double dlnp   = (lnPbot - lnPtop) / (nTable-1);

    // float texCoordsTable[nTable]; VLAs not supported in msvc
    float* texCoordsTable = new float[nTable];

    for (uint i = 0; i< nTable; i++)
    {
        double lnP = lnPtop + i*dlnp;
        texCoordsTable[i] = gsl_interp_eval(gslLnPToTexCoord, lnPLevels,
                                            texCoordsPLevels, lnP,
                                            gslLnPToTexCoordAcc);
//        cout << i << " " << lnP << " " << exp(lnP) << " "
//             << texCoordsTable[i] << endl;
    }

    gsl_interp_free(gslLnPToTexCoord);
    gsl_interp_accel_free(gslLnPToTexCoordAcc);

    // Create and upload texture.
    t = new GL::MTexture(pressureTableID, GL_TEXTURE_1D, GL_ALPHA32F_ARB,
                        nTable);

    if ( glRM->tryStoreGPUItem(t) )
    {
        // The new texture was successfully stored in the GPU memory manger.
        // We can now upload the data.
        glRM->makeCurrent();

        t->bindToLastTextureUnit();

        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glTexImage1D(GL_TEXTURE_1D,             // target
                     0,                         // level of detail
                     GL_ALPHA32F_ARB,           // internal format
                     nTable,         // width
                     0,                         // border
                     GL_ALPHA,                  // format
                     GL_FLOAT,                  // data type of the pixel data
                     texCoordsTable); CHECK_GL_ERROR;

        if (currentGLContext) currentGLContext->makeCurrent();
    }
    else
    {
        delete t;
    }

    delete[] lnPLevels;
    delete[] texCoordsPLevels;
    delete[] texCoordsTable;

    return static_cast<GL::MTexture*>(glRM->getGPUItem(pressureTableID));
}


void MRegularLonLatStructuredPressureGrid::releasePressureTexCoordTexture1D()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(pressureTableID);
}


float MRegularLonLatStructuredPressureGrid::interpolateGridColumnToPressure(
        unsigned int j, unsigned int i, float p_hPa)
{
    // Binary search to find model levels k, k1 that enclose pressure level p.
    int k = 0;
    int k1 = nlevs - 1;

    while (abs(k1 - k) > 1)
    {
        int kMid = (k + k1) / 2;
        float pMid = levels[kMid];
        if (p_hPa >= pMid) k = kMid; else k1 = kMid;
    }

    float lnPk  = log(levels[k]);
    float lnPk1 = log(levels[k1]);
    float lnP   = log(p_hPa);

    // Interpolate linearly in ln(p).
    float mixK = (lnP - lnPk) / (lnPk1 - lnPk);

    if ((mixK < 0) || (mixK > 1.)) return M_MISSING_VALUE;

    float scalar_k  = getValue(k , j, i);
    float scalar_k1 = getValue(k1, j, i);

    return MMIX(scalar_k, scalar_k1, mixK);
}


float MRegularLonLatStructuredPressureGrid::levelPressureAtLonLat_hPa(
        float lon, float lat, unsigned int k)
{
    Q_UNUSED(lon);
    Q_UNUSED(lat);
    return levels[k];
}


int MRegularLonLatStructuredPressureGrid::findLevel(
        unsigned int j, unsigned int i, float p_hPa)
{
    Q_UNUSED(j);
    Q_UNUSED(i);

    // Binary search to find model levels k, k1 that enclose pressure level p.
    int k = 0;
    int k1 = nlevs - 1;

    while (abs(k1 - k) > 1)
    {
        int kMid = (k + k1) / 2;
        float pMid = levels[kMid];
        if (p_hPa >= pMid) k = kMid; else k1 = kMid;
    }

    return k;
}


float MRegularLonLatStructuredPressureGrid::getPressure(
        unsigned int k, unsigned int j, unsigned int i)
{
    Q_UNUSED(j);
    Q_UNUSED(i);
    return levels[k];
}


float MRegularLonLatStructuredPressureGrid::getBottomInterfacePressure(
        unsigned int k, unsigned int j, unsigned int i)
{
    Q_UNUSED(j);
    Q_UNUSED(i);

    if (k == nlevs-1) return levels[k];

    return (levels[k] + levels[k+1]) / 2.;
}


float MRegularLonLatStructuredPressureGrid::getTopInterfacePressure(
        unsigned int k, unsigned int j, unsigned int i)
{
    Q_UNUSED(j);
    Q_UNUSED(i);

    if (k == 0) return levels[k];

    return (levels[k] + levels[k-1]) / 2.;
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                            MRegularLonLatGrid                           ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MRegularLonLatGrid::MRegularLonLatGrid(
        unsigned int nlats, unsigned int nlons)
    : MStructuredGrid(SURFACE_2D, 1, nlats, nlons)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

GL::MTexture *MRegularLonLatGrid::getTexture(QGLWidget *currentGLContext,
                                            bool nullTexture)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MTexture *t = static_cast<GL::MTexture*>(glRM->getGPUItem(getID()));
    if (t) return t;

    // No texture with this item's data exists. Create a new one.
    t = new GL::MTexture(getID(), GL_TEXTURE_2D, textureInternalFormat,
                        nlons, nlats);

    if ( glRM->tryStoreGPUItem(t) )
    {
        // The new texture was successfully stored in the GPU memory manger.
        // We can now upload the data.
        glRM->makeCurrent();

        t->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering (RTVG p. 64).
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, textureWrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, textureWrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureMinMaxFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureMinMaxFilter);

        if (nullTexture)
            glTexImage2D(GL_TEXTURE_2D, 0, textureInternalFormat,
                         nlons, nlats, 0, textureFormat, GL_FLOAT, NULL);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, textureInternalFormat,
                         nlons, nlats, 0, textureFormat, GL_FLOAT, data);
        CHECK_GL_ERROR;

        if (currentGLContext) currentGLContext->makeCurrent();
    }
    else
    {
        delete t;
    }

    return static_cast<GL::MTexture*>(glRM->getGPUItem(getID()));
}


GL::MTexture *MRegularLonLatGrid::getFlagsTexture(QGLWidget *currentGLContext)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MTexture *t = static_cast<GL::MTexture*>(glRM->getGPUItem(flagsID));
    if (t) return t;

    // No texture with this item's data exists. Create a new one.
    t = new GL::MTexture(flagsID, GL_TEXTURE_2D, GL_RG32UI,
                        nlons, nlats);

    if ( glRM->tryStoreGPUItem(t) )
    {
        // The new texture was successfully stored in the GPU memory manger.
        // We can now upload the data.
        glRM->makeCurrent();

        t->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering (RTVG p. 64).
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, textureWrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, textureWrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexImage3D(GL_TEXTURE_2D, 0, GL_RG32UI,
                     nlons, nlats, nlevs,
                     0, GL_RG_INTEGER,  GL_UNSIGNED_INT, flags);
        CHECK_GL_ERROR;

        if (currentGLContext) currentGLContext->makeCurrent();
    }
    else
    {
        delete t;
    }

    return static_cast<GL::MTexture*>(glRM->getGPUItem(flagsID));
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                      MLonLatHybridSigmaPressureGrid                     ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MLonLatHybridSigmaPressureGrid::MLonLatHybridSigmaPressureGrid(
        unsigned int nlevs, unsigned int nlats, unsigned int nlons)
    : MStructuredGrid(HYBRID_SIGMA_PRESSURE_3D, nlevs, nlats, nlons),
      ak_hPa(new double[nlevs]),
      bk(new double[nlevs]),
      aki_hPa(nullptr),
      bki(nullptr),
      surfacePressure(nullptr),
      cachedTopDataVolumePressure_hPa(M_MISSING_VALUE),
      cachedBottomDataVolumePressure_hPa(M_MISSING_VALUE)
{
    akbkID = getID() + "hyb";
}


MLonLatHybridSigmaPressureGrid::~MLonLatHybridSigmaPressureGrid()
{
    removeSurfacePressureField();

    MGLResourcesManager::getInstance()->releaseAllGPUItemReferences(akbkID);

    delete[] ak_hPa;
    delete[] bk;
    if (aki_hPa) delete[] aki_hPa;
    if (bki) delete[] bki;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

unsigned int MLonLatHybridSigmaPressureGrid::getMemorySize_kb()
{
    return MStructuredGrid::getMemorySize_kb()
            + ( sizeof(MLonLatHybridSigmaPressureGrid)
                -  sizeof(MStructuredGrid)
                + (nlevs * 2) * sizeof(double) // ak, bk
                + ((nlevs+1) * 2) * sizeof(double) // aki, bki
                ) / 1024.;
}


void MLonLatHybridSigmaPressureGrid::exchangeSurfacePressureGrid(
        MRegularLonLatGrid *newSfcPressureGrid)
{
    if (newSfcPressureGrid != nullptr)
    {
        removeSurfacePressureField();
        surfacePressure = newSfcPressureGrid;
    }
}


GL::MTexture *MLonLatHybridSigmaPressureGrid::getHybridCoeffTexture(
        QGLWidget *currentGLContext)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MTexture *t = static_cast<GL::MTexture*>(glRM->getGPUItem(akbkID));
    if (t) return t;

    // No texture with this item's data exists. Create a new one.
    t = new GL::MTexture(akbkID, GL_TEXTURE_1D, GL_ALPHA32F_ARB, nlevs);

    if ( glRM->tryStoreGPUItem(t) )
    {
        // The new texture was successfully stored in the GPU memory manger.
        // We can now upload the data.
        glRM->makeCurrent();

        t->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering.
        // NOTE: GL_NEAREST is required here to avoid interpolation between
        // the discrete coefficients.
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // Upload ak and bk as float array to GPU. "nlevel" can be
        // reconstructed in the vertex shader with the "textureSize()"
        // function.
        float *akbk = new float[2 * nlevs];
        for (unsigned int i = 0; i < nlevs; i++) {
            akbk[i        ] = float(ak_hPa[i]);
            akbk[i + nlevs] = float(bk[i]);
        }
        glTexImage1D(GL_TEXTURE_1D,             // target
                     0,                         // level of detail
                     GL_ALPHA32F_ARB,           // internal format
                     2 * nlevs,                 // width
                     0,                         // border
                     GL_ALPHA,                  // format
                     GL_FLOAT,                  // data type of the pixel data
                     akbk); CHECK_GL_ERROR;
        delete[] akbk;

        if (currentGLContext) currentGLContext->makeCurrent();
    }
    else
    {
        delete t;
    }

    return static_cast<GL::MTexture*>(glRM->getGPUItem(akbkID));
}


void MLonLatHybridSigmaPressureGrid::releaseHybridCoeffTexture()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(akbkID);
}


GL::MTexture *MLonLatHybridSigmaPressureGrid::getPressureTexCoordTexture2D(
        QGLWidget *currentGLContext)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MTexture *t = static_cast<GL::MTexture*>(
                glRM->getGPUItem(getPressureTexCoordID()));
    if (t) return t;

    // No texture with this item's data exists. Create a new table and texture.

#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
    LOG4CPLUS_DEBUG(mlog, "Creating new hybrid model level LUT ...");
#endif

    // Initialise new lookup table. Example: With 1200 values of surface
    // pressure and 2046+2 LUT entries per psfc, at 4 bytes/entry the LUT
    // will occupy approximately 9 MB in GPU memory.
    // The first two entries per psfc are used to store upper table boundary
    // and vertical table range (both in ln(pressure)), required to scale
    // ln(p) to texture coordinate in the shader.

    // NOTE: The range of values for surface pressure (1050..450 in 0.5 hPa
    // steps) is HARD-CODED in the shader method for volume sampling. If
    // these value are changed here, make sure the shader code is changed
    // as well. --> see GLSL method sampleHybridSigmaColumnAtP_LUT()

    uint  nVerticalTableSize = 2048;    // LUT with 2048-2 entries per psfc ...
    uint  nVerticalTableEntries = nVerticalTableSize - 2;
    uint  nPsfcTableEntries = 1200;     // ... for 1200 values of psfc
    float psfc_hPa_bottom = 1050.;      // ... starting at 1050. hPa
    float delta_psfc_hPa = 0.5;         // ... at 0.5 hPa steps
    float *texCoordsTable = new float[nPsfcTableEntries * nVerticalTableSize];

    double *lnPLevels = new double[nlevs];
    double *texCoordsPLevels = new double[nlevs];
    for (uint ipsfc = 0; ipsfc < nPsfcTableEntries; ipsfc++)
    {
        double psfc_hPa = psfc_hPa_bottom - ipsfc * delta_psfc_hPa;

        // 1) Compute texture coordinates for each p-level. For e.g. 6 levels,
        // texture coordinates will be 1/12, 3/12, 5/12, ...
        for (uint k = 0; k < nlevs; ++k)
        {
            lnPLevels[k] = log(ak_hPa[k] + bk[k] * psfc_hPa);
            texCoordsPLevels[k] = (2.*k + 1.) / (2.*nlevs);
            // cout << k << " " << lnPLevels[k] << " " << levels[k] << " "
            //      << texCoordsPLevels[k] << endl;
        }

        // Set up interpolation from ln(p) to texture coordinate.
        gsl_interp_accel *gslLnPToTexCoordAcc = gsl_interp_accel_alloc();
        gsl_interp *gslLnPToTexCoord = gsl_interp_alloc(gsl_interp_linear, nlevs);
        gsl_interp_init(gslLnPToTexCoord, lnPLevels, texCoordsPLevels, nlevs);

        // 2) Create regular ln(p) table with nTable levels. For each level in
        // ln(p) table, compute texture coordinate via linear interpolation in
        // ln(p).

        double lnPbot = log(ak_hPa[nlevs-1] + bk[nlevs-1] * psfc_hPa);
        double lnPtop = log(ak_hPa[0] + bk[0] * psfc_hPa);
        double dlnp   = (lnPbot - lnPtop) / (nVerticalTableEntries-1);

        // Compute upper table boundary and vertical table range (both in
        // ln(pressure)), required to scale ln(p) to texture coordinate in the
        // shader. Store the two values at the first two texture indices.
        // They can be retrieved in the shader.
        double lnPtop_table = lnPtop - dlnp/2.;
        double lnp_vertExtent = fabs(lnPtop - lnPbot) + dlnp;
        texCoordsTable[INDEX2yx(0, ipsfc, nPsfcTableEntries)] = lnPtop_table;
        texCoordsTable[INDEX2yx(1, ipsfc, nPsfcTableEntries)] = lnp_vertExtent;

        for (uint ilut = 0; ilut < nVerticalTableEntries; ilut++)
        {
            double lnP = lnPtop + ilut*dlnp;

            // The following is required to avoid gsl interpolation errors
            // due to numerical inaccuracies (it can happen that ln(p) for
            // the last ilut, i.e. the bottom value, is slightly larger
            // than lnPbot (in the 11th digit or so...).
            lnP = std::min(lnP, lnPbot);

            // Offset index by 2 due to the two above values stored at the
            // beginning of the table.
            texCoordsTable[INDEX2yx(2+ilut, ipsfc, nPsfcTableEntries)] =
                    gsl_interp_eval(gslLnPToTexCoord, lnPLevels,
                                    texCoordsPLevels, lnP, gslLnPToTexCoordAcc);
            // cout << ilut << " " << lnP << " " << exp(lnP) << " "
            //      << texCoordsTable[INDEX2yx(2+ilut, ipsfc, nPsfcTableEntries)] << endl;
        }

        gsl_interp_free(gslLnPToTexCoord);
        gsl_interp_accel_free(gslLnPToTexCoordAcc);
    }
    delete[] lnPLevels;
    delete[] texCoordsPLevels;

    // Create and upload texture.
    t = new GL::MTexture(getPressureTexCoordID(), GL_TEXTURE_2D,
                         GL_ALPHA32F_ARB, nPsfcTableEntries,
                         nVerticalTableEntries);

    if ( glRM->tryStoreGPUItem(t) )
    {
        // The new texture was successfully stored in the GPU memory manger.
        // We can now upload the data.
        glRM->makeCurrent();

        t->bindToLastTextureUnit();

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D,             // target
                     0,                         // level of detail
                     GL_ALPHA32F_ARB,           // internal format
                     nPsfcTableEntries,         // width (x)
                     nVerticalTableEntries,     // height (y)
                     0,                         // border
                     GL_ALPHA,                  // format
                     GL_FLOAT,                  // data type of the pixel data
                     texCoordsTable); CHECK_GL_ERROR;

        if (currentGLContext) currentGLContext->makeCurrent();
    }
    else
    {
        delete t;
    }

    delete[] texCoordsTable;

#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "LUT created in "
                    << stopwatch.getElapsedTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif

    return static_cast<GL::MTexture*>(glRM->getGPUItem(getPressureTexCoordID()));
}


void MLonLatHybridSigmaPressureGrid::releasePressureTexCoordTexture2D()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(getPressureTexCoordID());
}


void MLonLatHybridSigmaPressureGrid::findEnclosingModelLevels(
        double psfc_hPa, double p_hPa, int *kLowerPressure, int *kUpperPressure)
{
    // Initial position of klower and kupper.
    int klower = 0;
    int kupper = nlevs - 1;

    // Perform the binary search.
    while ((kupper-klower) > 1)
    {
        // Element midway between klower and kupper.
        int kmid = (kupper+klower) / 2;
        // Compute pressure at kmid.
        float pressureAt_kmid_hPa = ak_hPa[kmid] + bk[kmid] * psfc_hPa;

        // Cut interval in half.
        if (p_hPa >= pressureAt_kmid_hPa)
            klower = kmid;
        else
            kupper = kmid;
    }

    double plower_hPa = ak_hPa[klower] + bk[klower] * psfc_hPa;
    double pupper_hPa = ak_hPa[kupper] + bk[kupper] * psfc_hPa;

    *kLowerPressure = (plower_hPa < pupper_hPa) ? klower : kupper;
    *kUpperPressure = (plower_hPa < pupper_hPa) ? kupper : klower;
}


float MLonLatHybridSigmaPressureGrid::interpolateGridColumnToPressure(
        unsigned int j, unsigned int i, float p_hPa)
{
    float psfc_hPa = surfacePressure->getValue(j, i) / 100.;

    // Initial position of klower and kupper.
    int klower = 0;
    int kupper = nlevs - 1;

    // Perform the binary search.
    while ((kupper-klower) > 1)
    {
        // Element midway between klower and kupper.
        int kmid = (kupper+klower) / 2;
        // Compute pressure at kmid.
        float pressureAt_kmid_hPa = ak_hPa[kmid] + bk[kmid] * psfc_hPa;

        // Cut interval in half.
        if (p_hPa >= pressureAt_kmid_hPa)
            klower = kmid;
        else
            kupper = kmid;
    }

    float plower_hPa = ak_hPa[klower] + bk[klower] * psfc_hPa;
    float pupper_hPa = ak_hPa[kupper] + bk[kupper] * psfc_hPa;
    float ln_plower  = log(plower_hPa);
    float ln_pupper  = log(pupper_hPa);
    float ln_p       = log(p_hPa);

    float scalar_klower = getValue(klower, j, i);
    float scalar_kupper = getValue(kupper, j, i);

    // If the requested pressure value is below the upper pressure limit or
    // above the lower pressure limit, return M_MISSING_VALUE.
    if (ln_plower < ln_pupper)
    {
        if ((ln_p > ln_pupper) || (ln_p < ln_plower)) return M_MISSING_VALUE;
    }
    else
    {
        if ((ln_p < ln_pupper) || (ln_p > ln_plower)) return M_MISSING_VALUE;
    }

    // Linearly interpolate in ln(p) between the scalar values at level
    // kupper and level klower.
    float a = (ln_p - ln_pupper) / (ln_plower - ln_pupper);
    // return mix(scalar_kupper, scalar_klower, a);
    // GLSL mix(x,y,a) = x * (1.-a) + y*a
    return (scalar_kupper * (1.-a) + scalar_klower * a);
}


float MLonLatHybridSigmaPressureGrid::levelPressureAtLonLat_hPa(
        float lon, float lat, unsigned int k)
{
    // Interpolate surface pressure to lon/lat position (pressure value is
    // ignored by interpolateValue() since surface pressure is a 2D field), ..
    float psfc_hPa = surfacePressure->interpolateValue(lon, lat, 0.) / 100.;
    // .. then compute pressure of level.
    float p_hPa = ak_hPa[k] + bk[k] * psfc_hPa;
    return p_hPa;
}


int MLonLatHybridSigmaPressureGrid::findLevel(
        unsigned int j, unsigned int i, float p_hPa)
{
    float psfc_hPa = surfacePressure->getValue(j, i) / 100.;

    // Binary search to find model levels k, k1 that enclose pressure level p.
    int k = 0;
    int k1 = nlevs - 1;

    while (abs(k1-k) > 1)
    {
        // Element midway between k and k1.
        int kmid = (k1 + k) / 2;
        // Compute pressure at kmid.
        float pressureAt_kmid_hPa = ak_hPa[kmid] + bk[kmid] * psfc_hPa;

        // Cut interval in half.
        if (p_hPa >= pressureAt_kmid_hPa) k = kmid; else k1 = kmid;
    }

    return k;
}


float MLonLatHybridSigmaPressureGrid::getPressure(
         unsigned int k, unsigned int j, unsigned int i)
{
    float psfc_hPa = surfacePressure->getValue(j, i) / 100.;
    float p_hPa = ak_hPa[k] + bk[k] * psfc_hPa;
    return p_hPa;
}


void MLonLatHybridSigmaPressureGrid::computeInterfaceCoefficients()
{
    allocateInterfaceCoefficients();

    // Boundary coefficients, cf.
    // http://old.ecmwf.int/products/data/technical/model_levels/model_def_91.html
    aki_hPa[0] = 0.;
    bki[0] = 0.;
    aki_hPa[nlevs] = 0.;
    bki[nlevs] = 1.;

    // Compute half-level (or interface) coefficients ai_k, bi_k from
    // full-level coeffs af_k, bf_k.
    for (unsigned int k = nlevs-1; k > 0; k--)
    {
        aki_hPa[k] = 2.*ak_hPa[k] - aki_hPa[k+1];
        bki[k] = 2.*bk[k] - bki[k+1];
    }
}


float MLonLatHybridSigmaPressureGrid::getBottomInterfacePressure(
        unsigned int k, unsigned int j, unsigned int i)
{
    if (aki_hPa == nullptr) computeInterfaceCoefficients();
    float psfc_hPa = surfacePressure->getValue(j, i) / 100.;
    float p_hPa = aki_hPa[k+1] + bki[k+1] * psfc_hPa;
    return p_hPa;
}


float MLonLatHybridSigmaPressureGrid::getTopInterfacePressure(
        unsigned int k, unsigned int j, unsigned int i)
{
    if (aki_hPa == nullptr) computeInterfaceCoefficients();
    float psfc_hPa = surfacePressure->getValue(j, i) / 100.;
    float p_hPa = aki_hPa[k] + bki[k] * psfc_hPa;
    return p_hPa;
}


float MLonLatHybridSigmaPressureGrid::getTopDataVolumePressure_hPa(
        bool useCachedValue)
{
    if (cachedTopDataVolumePressure_hPa == M_MISSING_VALUE || !useCachedValue)
    {
        // Update cached value upon first call or if explicity requested by
        // caller.
        float psfc_min = surfacePressure->min() / 100.;
        cachedTopDataVolumePressure_hPa = (ak_hPa[0] + bk[0] * psfc_min);
    }

    // Return cached value.
    return cachedTopDataVolumePressure_hPa;
}


float MLonLatHybridSigmaPressureGrid::getBottomDataVolumePressure_hPa(
        bool useCachedValue)
{
    if (cachedBottomDataVolumePressure_hPa == M_MISSING_VALUE || !useCachedValue)
    {
        // Update cached value upon first call or if explicity requested by
        // caller.
        float psfc_max = surfacePressure->max() / 100.;
        cachedBottomDataVolumePressure_hPa =
                (ak_hPa[nlevs-1] + bk[nlevs-1] * psfc_max);
    }

    // Return cached value.
    return cachedBottomDataVolumePressure_hPa;
}


void MLonLatHybridSigmaPressureGrid::dumpGridData(unsigned int maxValues)
{
    QString str = "\n\nLonLatHybridSigmaPressure Grid Data\n====================";
    str += QString("\nVariable name: %1").arg(variableName);
    str += QString("\nInit time: %1").arg(initTime.toString(Qt::ISODate));
    str += QString("\nValid time: %1").arg(validTime.toString(Qt::ISODate));
    str += QString("\nEnsemble member: %1").arg(ensembleMember);
    str += QString("\nGenerating request: %1").arg(getGeneratingRequest());

    str += "\n\nlon: ";
    for (uint i = 0; i < nlons; i++) str += QString("%1/").arg(lons[i]);
    str += "\n\nlat: ";
    for (uint i = 0; i < nlats; i++) str += QString("%1/").arg(lats[i]);
    str += "\n\nlev: ";
    for (uint i = 0; i < nlevs; i++) str += QString("%1/").arg(levels[i]);

    str += "\n\nak: ";
    for (uint i = 0; i < nlevs; i++) str += QString("%1/").arg(ak_hPa[i]);
    str += "\n\nbk: ";
    for (uint i = 0; i < nlevs; i++) str += QString("%1/").arg(bk[i]);

    if (aki_hPa == nullptr) computeInterfaceCoefficients();
    str += "\n\naki: ";
    for (uint i = 0; i <= nlevs; i++) str += QString("%1/").arg(aki_hPa[i]);
    str += "\n\nbki: ";
    for (uint i = 0; i <= nlevs; i++) str += QString("%1/").arg(bki[i]);

    unsigned int nv = std::min(nvalues, maxValues);
    str += QString("\n\ndata (first %1 values): ").arg(nv);
    for (uint i = 0; i < nv; i++) str += QString("%1/").arg(data[i]);

    str += QString("\n\ndata (column at i=0,j=0): ");
    for (uint k = 0; k < nlevs; k++) str += QString("%1/").arg(getValue(k, 0, 0));

    MStructuredGrid* psfc = surfacePressure;
    nv = std::min(psfc->getNumValues(), maxValues);
    str += QString("\n\npsfc data (first %1 values): ").arg(nv);
    for (uint i = 0; i < nv; i++) str += QString("%1/").arg(psfc->getValue(i));

    str += "\n\nend data\n====================\n";

    LOG4CPLUS_INFO(mlog, str.toStdString());
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MLonLatHybridSigmaPressureGrid::allocateInterfaceCoefficients()
{
    if (aki_hPa) delete[] aki_hPa;
    if (bki) delete[] bki;
    aki_hPa = new double[nlevs+1];
    bki = new double[nlevs+1];
}


QString MLonLatHybridSigmaPressureGrid::getPressureTexCoordID()
{
    // If this is the first time the ID is requested we must construct it.
    // To keep the key short, we use an MD5 hash on the ak coefficients.
    // This way, all grids that share the same ak/bk coefficients (where the
    // bk are assumed to uniquely correspond to the ak) can also share the
    // same pressure-to-texture-coordinate-table.
    if (pressureTexCoordID == "")
    {
        QCryptographicHash md5(QCryptographicHash::Md5);

        for (uint k = 0; k < nlevs; k++)
        {
            QByteArray ba;
            ba.setNum(ak_hPa[k]);
            // pressureTexCoordID += ba + "/";
            md5.addData(ba);
        }

        pressureTexCoordID = "pTexCoord/" + md5.result().toHex();
    }

    return pressureTexCoordID;
}


void MLonLatHybridSigmaPressureGrid::removeSurfacePressureField()
{
    if (surfacePressure)
    {
        // Release surface pressure field if it has been registered to a memory
        // manager otherwise delete it.
        if (surfacePressure->getMemoryManager())
        {
            LOG4CPLUS_TRACE(mlog, "Releasing psfc of request "
                            << getGeneratingRequest().toStdString());
            surfacePressure->getMemoryManager()->releaseData(surfacePressure);
        }
        else
        {
            delete surfacePressure;
        }
    }
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                       MLonLatAuxiliaryPressureGrid                      ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MLonLatAuxiliaryPressureGrid::MLonLatAuxiliaryPressureGrid(
        unsigned int nlevs, unsigned int nlats, unsigned int nlons,
        bool reverseLevels)
    : MStructuredGrid(AUXILIARY_PRESSURE_3D, nlevs, nlats, nlons),
      auxPressureField_hPa(nullptr),
      reverseLevels(reverseLevels),
      cachedTopDataVolumePressure_hPa(M_MISSING_VALUE),
      cachedBottomDataVolumePressure_hPa(M_MISSING_VALUE)
{}


MLonLatAuxiliaryPressureGrid::~MLonLatAuxiliaryPressureGrid()
{
    removeAuxiliaryPressureField();
}


void MLonLatAuxiliaryPressureGrid::exchangeAuxiliaryPressureGrid(
        MLonLatAuxiliaryPressureGrid *newAuxPGrid)
{
    if (newAuxPGrid != nullptr)
    {
        removeAuxiliaryPressureField();
        auxPressureField_hPa = newAuxPGrid;
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

float MLonLatAuxiliaryPressureGrid::interpolateGridColumnToPressure(
        unsigned int j, unsigned int i, float p_hPa)
{
    // Initial position of klower and kupper.
    int klower = 0;
    int kupper = nlevs - 1;

    // Perform the binary search.
    while ((kupper - klower) > 1)
    {
        // Element midway between klower and kupper.
        int kmid = (kupper + klower) / 2;
        // Compute pressure at kmid.
        float pressureAt_kmid_hPa = auxPressureField_hPa->getValue(kmid, j, i);

        // Cut interval in half.
        if (p_hPa >= pressureAt_kmid_hPa)
        {
            klower = kmid;
        }
        else
        {
            kupper = kmid;
        }
    }

    float plower_hPa = auxPressureField_hPa->getValue(klower, j, i);
    float pupper_hPa = auxPressureField_hPa->getValue(kupper, j, i);

    float ln_plower  = log(plower_hPa);
    float ln_pupper  = log(pupper_hPa);
    float ln_p       = log(p_hPa);

    float scalar_klower = getValue(klower, j, i);
    float scalar_kupper = getValue(kupper, j, i);

    // If the requested pressure value is below the upper pressure limit or
    // above the lower pressure limit, return M_MISSING_VALUE.
    if (ln_plower < ln_pupper)
    {
        if ((ln_p > ln_pupper) || (ln_p < ln_plower))
        {
            return M_MISSING_VALUE;
        }
    }
    else
    {
        if ((ln_p < ln_pupper) || (ln_p > ln_plower))
        {
            return M_MISSING_VALUE;
        }
    }

    // Linearly interpolate in ln(p) between the scalar values at level
    // kupper and level klower.
    float a = (ln_p - ln_pupper) / (ln_plower - ln_pupper);
    // return mix(scalar_kupper, scalar_klower, a);
    // GLSL mix(x,y,a) = x * (1.-a) + y*a
    return (scalar_kupper * (1. - a) + scalar_klower * a);
}


float MLonLatAuxiliaryPressureGrid::levelPressureAtLonLat_hPa(
        float lon, float lat, unsigned int k)
{
    return auxPressureField_hPa->interpolateValueOnLevel(lon, lat, k);
}


int MLonLatAuxiliaryPressureGrid::findLevel(
        unsigned int j, unsigned int i, float p_hPa)
{
    // Binary search to find model levels k, k1 that enclose pressure level p.
    int k = 0;
    int k1 = nlevs - 1;

    while (abs(k1 - k) > 1)
    {
        // Element midway between k and k1.
        int kmid = (k1 + k) / 2;
        // Compute pressure at kmid.
        float pressureAt_kmid_hPa = auxPressureField_hPa->getValue(kmid, j, i);

        // Cut interval in half.
        if (p_hPa >= pressureAt_kmid_hPa)
        {
            k = kmid;
        }
        else
        {
            k1 = kmid;
        }
    }

    return k;
}


float MLonLatAuxiliaryPressureGrid::getPressure(
         unsigned int k, unsigned int j, unsigned int i)
{
        return auxPressureField_hPa->getValue(k, j, i);
}


float MLonLatAuxiliaryPressureGrid::getBottomInterfacePressure(
        unsigned int k, unsigned int j, unsigned int i)
{
    return auxPressureField_hPa->getValue(k + 1, j, i);
}


float MLonLatAuxiliaryPressureGrid::getTopInterfacePressure(
        unsigned int k, unsigned int j, unsigned int i)
{
    return auxPressureField_hPa->getValue(k, j, i);
}


float MLonLatAuxiliaryPressureGrid::getTopDataVolumePressure_hPa(
        bool useCachedValue)
{
    if (cachedTopDataVolumePressure_hPa == M_MISSING_VALUE || !useCachedValue)
    {
        // Update cached value upon first call or if explicity requested by
        // caller.
        cachedTopDataVolumePressure_hPa = auxPressureField_hPa->min();
    }

    // Return cached value.
    return cachedTopDataVolumePressure_hPa;
}


float MLonLatAuxiliaryPressureGrid::getBottomDataVolumePressure_hPa(
        bool useCachedValue)
{
    if (cachedBottomDataVolumePressure_hPa == M_MISSING_VALUE || !useCachedValue)
    {
        // Update cached value upon first call or if explicity requested by
        // caller.
        cachedBottomDataVolumePressure_hPa = auxPressureField_hPa->max();
    }

    // Return cached value.
    return cachedBottomDataVolumePressure_hPa;
}


void MLonLatAuxiliaryPressureGrid::dumpGridData(unsigned int maxValues)
{
    QString str = "\n\nLonLatAuxiliaryPressureGrid Grid Data\n====================";
    str += QString("\nVariable name: %1").arg(variableName);
    str += QString("\nInit time: %1").arg(initTime.toString(Qt::ISODate));
    str += QString("\nValid time: %1").arg(validTime.toString(Qt::ISODate));
    str += QString("\nEnsemble member: %1").arg(ensembleMember);
    str += QString("\nGenerating request: %1").arg(getGeneratingRequest());

    str += "\n\nlon: ";
    for (uint i = 0; i < nlons; i++) str += QString("%1/").arg(lons[i]);
    str += "\n\nlat: ";
    for (uint i = 0; i < nlats; i++) str += QString("%1/").arg(lats[i]);
    str += "\n\nlev: ";
    for (uint i = 0; i < nlevs; i++) str += QString("%1/").arg(levels[i]);

    unsigned int nv = std::min(nvalues, maxValues);
    str += QString("\n\ndata (first %1 values): ").arg(nv);
    for (uint i = 0; i < nv; i++) str += QString("%1/").arg(data[i]);

    str += QString("\n\ndata (column at i=0,j=0): ");
    for (uint k = 0; k < nlevs; k++) str += QString("%1/").arg(getValue(k, 0, 0));

//    str += QString("\n\nZeros at index positions (k,j,i): ");
//    for (uint k = 0; k < nlevs; k++)
//        for (uint j = 0; j < nlats; j++)
//            for (uint i = 0; i < nlons; i++)
//            {
//                if (getValue(k, j, i) == 0.)
//                {
//                    str += QString("%1,%2,%3/").arg(k).arg(j).arg(i);
//                }
//            }

    nv = std::min(auxPressureField_hPa->getNumValues(), maxValues);
    str += QString("\n\naux-p data (first %1 values): ").arg(nv);
    for (uint i = 0; i < nv; i++) str += QString("%1/").arg(auxPressureField_hPa->getValue(i));

    str += "\n\nend data\n====================\n";

    LOG4CPLUS_INFO(mlog, str.toStdString());
}


void MLonLatAuxiliaryPressureGrid::removeAuxiliaryPressureField()
{
    // If the pressure field was set by the friend class
    // MWeatherPredictionReader, the field was stored in the same memory
    // manager as this item. If this item is deleted from the memory manager,
    // release the pressure field.
    // If this grid is not registered with any memory manager simply delete
    // the pressure field grid.
    // Special case: Since the pressure field is connected to itself, don't
    // release it again.
    if (auxPressureField_hPa && auxPressureField_hPa != this)
    {
        if (auxPressureField_hPa->getMemoryManager())
        {
            LOG4CPLUS_TRACE(mlog, "Releasing aux pressure field of request "
                            << getGeneratingRequest().toStdString());
            auxPressureField_hPa->getMemoryManager()->releaseData(auxPressureField_hPa);
        }
        else
        {
            delete auxPressureField_hPa;
        }
    }
}


} // namespace Met3D
