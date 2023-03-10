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
#ifndef STRUCTUREDGRID_H
#define STRUCTUREDGRID_H

// standard library imports

// related third party imports
#include <GL/glew.h>
#ifdef USE_QOPENGLWIDGET
#include <QOpenGLWidget>
#else
#include <QGLWidget>
#endif

// local application imports
#include "data/abstractdataitem.h"
#include "gxfw/gl/texture.h"
#include "gxfw/msceneviewglwidget.h"


namespace Met3D
{

/******************************************************************************
***                               ENUMS                                     ***
*******************************************************************************/

/**
  Vertical level types for that specialisations of @ref MStructuredGrid exist.
  */
enum MVerticalLevelType {
    SURFACE_2D               = 0,
    PRESSURE_LEVELS_3D       = 1,
    HYBRID_SIGMA_PRESSURE_3D = 2,
    POTENTIAL_VORTICITY_2D   = 3,
    LOG_PRESSURE_LEVELS_3D   = 4,
    AUXILIARY_PRESSURE_3D    = 5,   // pressure in auxiliary variable
    MISC_LEVELS_3D           = 6,   // level types that do not fit any other
    SIZE_LEVELTYPES
};

enum MHorizontalGridType
{
    REGULAR_LONLAT_GRID = 0,
    // Rotated north pole coordinates as used by COSMO.
    // (cf. http://www.cosmo-model.org/content/model/documentation/core/cosmoDyncsNumcs.pdf ,
    //  chapter 3.3)
    REGULAR_ROTATED_LONLAT_GRID = 1,
    // Proj-supported projection.
    REGULAR_PROJECTED_GRID = 2,
    // Regular grid with geometric coordinates without geographical reference.
    REGULAR_GEOMETRIC_GRID
};


/******************************************************************************
***                              MACROS                                     ***
*******************************************************************************/

// 4D (ensemble + 3D) index macro for x being the fast varying dimension.
#define INDEX4ezyx(e, z, y, x, nz, ny, nx)  ((e)*(nz)*(ny)*(nx) + (z)*(nx)*(ny) + (y)*(nx) + (x))

// The same for pre-multiplied nznynx and nynx (faster for loops).
#define INDEX4ezyx_2(e, z, y, x, nznynx, nynx, nx)  ((e)*(nznynx) + (z)*(nynx) + (y)*(nx) + (x))

// 3D index macro for x being the fast varying dimension.
#define INDEX3zyx(z, y, x, ny, nx)  ((z)*(nx)*(ny) + (y)*(nx) + (x))

// The same with pre-multiplied nynx (faster for loops).
#define INDEX3zyx_2(z, y, x, nynx, nx)  ((z)*(nynx) + (y)*(nx) + (x))

// 2D index macro for x being the fast varying dimension.
#define INDEX2yx(y, x, nx)  ((y)*(nx) + (x))

// 3D + component (i.e. r,g,b)
#define INDEX4zyxc(z, y, x, c, ny, nx, nc)  ((z)*(ny)*(nx)*(nc) + (y)*(nc)*(nx) + (x)*(nc) + (c))


/******************************************************************************
***                             CLASSES                                     ***
*******************************************************************************/

struct MIndex3D
{
    MIndex3D() : k(-1), j(-1), i(-1) { }
    MIndex3D(int k, int j, int i) : k(k), j(j), i(i) { }

    bool isValid() { return ((k >= 0) && (j >= 0) && (i >= 0)); }
    QString toString() { return QString("(k=%1, j=%2, i=%3)").arg(k).arg(j).arg(i); }

    int k, j, i;
};

typedef QList<MIndex3D> MIndexedGridRegion;


/**

 */
template <typename T>
class MMemoryManagedArray : public MAbstractDataItem
{
public:
    MMemoryManagedArray(int n)
        : data(new T[n]), nvalues(n)
    { }

    ~MMemoryManagedArray()
    { delete[] data; }

    unsigned int getMemorySize_kb()
    { return (nvalues * sizeof(T)) / 1024.; }

    T *data;
    unsigned int nvalues;
};


/**

 */
class MStructuredGrid
        : public MAbstractDataItem, public MWeatherPredictionMetaData
{
public:
    /**
      The constructor allocates the data arrays.

      @param nlevs Number of data points in the vertical (z) direction.
      @param nlats Number of data points in the latitude (y) direction.
      @param nlons Number of data points in the longitude (x) direction.
      */
    MStructuredGrid(MVerticalLevelType leveltype, unsigned int nlevs,
                    unsigned int nlats, unsigned int nlons);

    /** Destructor frees memory fields. */
    ~MStructuredGrid();

    /** Memory required for the data field in kilobytes. */
    unsigned int getMemorySize_kb();

    /** Returns the vertical level type of this grid instance. */
    MVerticalLevelType getLevelType() const { return leveltype; }

    /** Convert a numerical vertical level code to a string. */
    static QString verticalLevelTypeToString(MVerticalLevelType type);

    static MVerticalLevelType verticalLevelTypeFromString(const QString& str);

    static MVerticalLevelType verticalLevelTypeFromConfigString(const QString& str);

    /**
      Minimum value of @ref data. This method requires O(nlevs * nlats * nlons)
      time.
     */
    float min();

    /**
      Maximum value of @ref data. This method requires O(nlevs * nlats * nlons)
      time.
     */
    float max();

    /**
      Mask a rectangular region so that all grid point data values outside of
      (i0,j0,k0)-->(i0+ni,j0+nj,k0+nk) are set to MISSING_VALUE.
      */
    void maskRectangularRegion(
            unsigned int i0, unsigned int j0, unsigned int k0,
            unsigned int ni, unsigned int nj, unsigned int nk);

    /** Sets the values of all grid points to zero. */
    void setToZero();

    /** Sets the values of all grid points to @p val. */
    void setToValue(float val);

    inline const float* getData() const { return data; }

    inline float getValue(
            unsigned int k, unsigned int j, unsigned int i) const
    { return data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)]; }

    inline float getValue(unsigned int n) const { return data[n]; }

    inline float getValue(MIndex3D idx) const
    { return data[INDEX3zyx_2(idx.k, idx.j, idx.i, nlatsnlons, nlons)]; }

    inline void setValue(
            unsigned int k, unsigned int j, unsigned int i, float v)
    { data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = v; }

    inline void setValue(unsigned int n, float v) { data[n] = v; }

    inline void setValue(MIndex3D idx, float v)
    { data[INDEX3zyx_2(idx.k, idx.j, idx.i, nlatsnlons, nlons)] = v; }

    inline void addValue(
            unsigned int k, unsigned int j, unsigned int i, float v)
    { data[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] += v; }

    inline void addValue(unsigned int n, float v) { data[n] += v; }

    inline void addValue(MIndex3D idx, float v)
    { data[INDEX3zyx_2(idx.k, idx.j, idx.i, nlatsnlons, nlons)] += v; }

    inline void setLon(unsigned int i, double v) { lons[i] = v; }
    inline void setLat(unsigned int j, double v) { lats[j] = v; }
    inline void setLevel(unsigned int k, double v) { levels[k] = v; }

    inline void setHorizontalGridType(MHorizontalGridType horizontalGridType)
    { this->horizontalGridType = horizontalGridType; }

    inline unsigned int getNumLevels() const { return nlevs; }
    inline unsigned int getNumLats() const { return nlats; }
    inline unsigned int getNumLons() const { return nlons; }
    inline unsigned int getNumValues() const { return nvalues; }

    inline const double* getLevels() const { return levels; }
    inline const double* getLats() const { return lats; }
    inline const double* getLons() const { return lons; }

    inline MHorizontalGridType getHorizontalGridType()
    { return horizontalGridType; }

    /**
      Returns the pressure (hPa) of grid point at indices @p i, @p j, @p k.
     */
    virtual float getPressure(unsigned int k, unsigned int j, unsigned int i)
    { Q_UNUSED(k); Q_UNUSED(j); Q_UNUSED(i); return M_MISSING_VALUE; }

    virtual float getBottomInterfacePressure(
            unsigned int k, unsigned int j, unsigned int i)
    { Q_UNUSED(k); Q_UNUSED(j); Q_UNUSED(i); return M_MISSING_VALUE; }

    virtual float getTopInterfacePressure(
            unsigned int k, unsigned int j, unsigned int i)
    { Q_UNUSED(k); Q_UNUSED(j); Q_UNUSED(i); return M_MISSING_VALUE; }

    inline float getDeltaLon() const { return fabs(lons[1]-lons[0]); }
    inline float getDeltaLat() const { return fabs(lats[1]-lats[0]); }

    inline float getWestInterfaceLon(unsigned int i)
    { return lons[i] - getDeltaLon()/2.; }

    inline float getEastInterfaceLon(unsigned int i)
    { return lons[i] + getDeltaLon()/2.; }

    inline float getNorthInterfaceLat(unsigned int j)
    { return lats[j] + getDeltaLat()/2.; }

    inline float getSouthInterfaceLat(unsigned int j)
    { return lats[j] - getDeltaLat()/2.; }

    /**
      Determine the horizontal grid indices @p i, @p j, @p i1, @p j1 that
      enclose the position given by @p lon, @p lat.
     */
    void findEnclosingHorizontalIndices(float lon, float lat, int *i, int *j,
                                    int *i1, int *j1, float *mixI, float *mixJ);

    /**
      Sample the data grid at lon, lat and p, using trilinear interpolation.
      Uses @ref interpolateGridColumnToPressure(). For derived grid classes
      that are only two-dimensional, the @p p_hPa parameter is ignored.
     */
    float interpolateValue(float lon, float lat, float p_hPa);

    float interpolateValue(QVector3D vec3_lonLatP);

    /**
      Implement this method in derived classes that know about their vertical
      coordinate. It is used by @ref interpolateValue(). If the derived class
      is two-dimensional, the @p p_hPa parameter can be ignored.
     */
    virtual float interpolateGridColumnToPressure(
            unsigned int j, unsigned int i, float p_hPa)
    { Q_UNUSED(j); Q_UNUSED(i); Q_UNUSED(p_hPa); return M_MISSING_VALUE; }

    /**
      Computes the pressure on grid level @p k at position (@p lon, @p lat).

      Implement this method in derived classes.
     */
    virtual float levelPressureAtLonLat_hPa(float lon, float lat, unsigned int k)
    { Q_UNUSED(lon); Q_UNUSED(lat); Q_UNUSED(k); return M_MISSING_VALUE; }

    /**
      Samples the data grid on vertical level k and at position (@p lon, @p lat)
      using bi-linear interpolation.
     */
    float interpolateValueOnLevel(float lon, float lat, unsigned int k);

    /**
      Extracts a vertical profile of (scalar, p_hPa) tuples from the data
      field at position (@p lon, @p lat). Uses @ref interpolateValueOnLevel()
      and @ref levelPressureAtLonLat_hPa().
     */
    QVector<QVector2D> extractVerticalProfile(float lon, float lat);

    /**
      Determine the four grid indices that horizontally bound the grid cell
      that contains the position specified by @p lon, @p lat, @p p_hPa. In the
      vertical, the indices refer to the level above the position in each
      grid column (so that the position is between k and k+1).
     */
    bool findTopGridIndices(float lon, float lat, float p_hPa,
                            MIndex3D *nw, MIndex3D *ne,
                            MIndex3D *sw, MIndex3D *se);

    bool findTopGridIndices(QVector3D vec3_lonLatP,
                            MIndex3D *nw, MIndex3D *ne,
                            MIndex3D *sw, MIndex3D *se);

    /**
      Find model level k so that the pressure value @p p_hPa is located
      between k and k+1.
     */
    virtual int findLevel(unsigned int j, unsigned int i, float p_hPa)
    { Q_UNUSED(j); Q_UNUSED(i); Q_UNUSED(p_hPa); return -1; }

    int findClosestLevel(unsigned int j, unsigned int i, float p_hPa);

    MIndex3D maxNeighbouringGridPoint(float lon, float lat, float p_hPa);

    MIndex3D maxNeighbouringGridPoint(QVector3D vec3_lonLatP);

    QVector3D getNorthWestTopDataVolumeCorner_lonlatp();

    QVector3D getSouthEastBottomDataVolumeCorner_lonlatp();

    /**
      Returns the topmost pressure elevation of the data volume. If
      @p useCachedValue is set to @p true (default), the value is computed
      once and reused (i.e., the vertical levels are assumed to be static).
      If you change the vertical levels and need to update this value,
      set @p useCachedValue to @p false.
     */
    virtual float getTopDataVolumePressure_hPa(bool useCachedValue=true)
    { Q_UNUSED(useCachedValue); return 0.; }

    /**
      Returns the bottommost pressure elevation of the data volume.
      See @see getTopDataVolumePressure_hPa() for the meaning of the
      @p useCachedValue parameter.
     */
    virtual float getBottomDataVolumePressure_hPa(bool useCachedValue=true)
    { Q_UNUSED(useCachedValue); return 0.; }

    bool gridIsCyclicInLongitude();

    /**
      Allows a number of texture parameters to be modified. Call this function
      before you call @ref getTexture().
     */
    void setTextureParameters(GLint  internalFormat = GL_ALPHA32F_ARB,
                              GLenum format = GL_ALPHA,
                              GLint  wrap = GL_CLAMP,
                              GLint  minMaxFilter = GL_LINEAR);

    /**
      Returns the handle to a texture containing the grid data. The handle
      needs to be released with @ref releaseTexture() if not required anylonger
      (not released textures will stay in GPU memory forever). The texture is
      memory managed by @ref MGLResourcesManager.
     */
    virtual GL::MTexture* getTexture(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr,
#else
            QGLWidget *currentGLContext = nullptr,
#endif
            bool nullTexture = false);

    /** Release a texture acquired with getTexture(). */
    void releaseTexture();

    /**
      Returns the handle to a texture containing the coordinate axis data (1D
      texture). Needs to be released with @ref releaseLonLatTexture().
     */
    GL::MTexture* getLonLatLevTexture(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif

    /** Release a texture acquired with getLonLatTexture(). */
    void releaseLonLatLevTexture();

    /** Writes coordinate axis data to the LOG. */
    void dumpCoordinateAxes();

    virtual void dumpGridData(unsigned int maxValues=50);

    /** Saves the data field in text file @p filename. */
    void saveAsNetCDF(QString filename);

    /*
    // Implementation alternative should a generalized grid class become
    // necessary (see notes 04Mar2014).

    enum MValueDataType
    {
        FLOAT_VALUES  = 1,
        UINT64_VALUES = 2
    };

    struct MGridValues {};

    template<class T> struct MTypedGridValues : MGridValues
    {
        MTypedGridValues(unsigned int nvals) : data(new T[nvals]) {}

        inline void setValue(unsigned int n, T v) { data[n] = v; }
        inline T getValue(unsigned int n) const { return data[n]; }

        T *data;
    };

    typedef MTypedGridValues<float> MFloatGridValues;
    typedef MTypedGridValues<quint64> MUInt64GridValues;

    template<class T> inline MTypedGridValues<T>* values()
    { return static_cast<MTypedGridValues<T>*>(gridValues); }

    MGridValues *gridValues;
    */

    /**
     Enable flags for this grid. If enabled, each grid point stores an
     additional bitfield of width @p numBits that can be used for arbitrary
     flags.

     @note Call this function DIRECTLY AFTER THE OBJECT CONSTRUCTION, before
     any other method is called. In particular, if flags are enabled after the
     object has been added to a memory manager, memory mangement will be
     corrupted. The method throws an exception if called after @ref
     getMemorySize_kb().

     @note Currently only 64bit-Flags are supported.
     */
    void enableFlags(unsigned char numBits=64);

    /** Returns the number of enabled flags (0 if no flags are enabled). */
    unsigned char flagsEnabled();

    /** Set flag @p f of grid value @p n.*/
    inline void setFlag(unsigned int n, unsigned char f)
    { flags[n] |= (Q_UINT64_C(1) << f); }

    inline void setFlag(
            unsigned int k, unsigned int j, unsigned int i, unsigned char f)
    { flags[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] |= (Q_UINT64_C(1) << f); }

    inline void setFlag(MIndex3D idx, unsigned char f)
    { flags[INDEX3zyx_2(idx.k, idx.j, idx.i, nlatsnlons, nlons)] |=
                (Q_UINT64_C(1) << f); }

    /** Set all flags of grid value @p n.*/
    inline void setFlags(unsigned int n, quint64 fl)
    { flags[n] = fl; }

    inline void setFlags(
            unsigned int k, unsigned int j, unsigned int i,quint64 fl)
    { flags[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = fl; }

    inline void setFlags(MIndex3D idx, quint64 fl)
    { flags[INDEX3zyx_2(idx.k, idx.j, idx.i, nlatsnlons, nlons)] = fl; }

    /** Clear flag @p f of grid value @p n.*/
    inline void clearFlag(unsigned int n, unsigned char f)
    { flags[n] &= ~(Q_UINT64_C(1) << f); }

    inline void clearFlag(
            unsigned int k, unsigned int j, unsigned int i, unsigned char f)
    { flags[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] &= ~(Q_UINT64_C(1) << f); }

    inline void clearFlag(MIndex3D idx, unsigned char f)
    { flags[INDEX3zyx_2(idx.k, idx.j, idx.i, nlatsnlons, nlons)] &=
                ~(Q_UINT64_C(1) << f); }

    /** Clear all flags of grid value @p n.*/
    inline void clearFlags(unsigned int n)
    { flags[n] = 0; }

    inline void clearFlags(
            unsigned int k, unsigned int j, unsigned int i)
    { flags[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)] = 0; }

    inline void clearFlags(MIndex3D idx)
    { flags[INDEX3zyx_2(idx.k, idx.j, idx.i, nlatsnlons, nlons)] = 0; }

    void clearAllFlags();

    /** Get flag @p f of grid value @p n.*/
    inline bool getFlag(unsigned int n, unsigned char f)
    { return (flags[n] & (Q_UINT64_C(1) << f)) > 0; }

    inline bool getFlag(
            unsigned int k, unsigned int j, unsigned int i, unsigned char f)
    { return (flags[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)]
                & (Q_UINT64_C(1) << f)) > 0; }

    inline bool getFlag(MIndex3D idx, unsigned char f)
    { return (flags[INDEX3zyx_2(idx.k, idx.j, idx.i, nlatsnlons, nlons)]
                & (Q_UINT64_C(1) << f)) > 0; }

    inline quint64 getFlags(unsigned int n) { return flags[n]; }

    inline quint64 getFlags(unsigned int k, unsigned int j, unsigned int i)
    { return flags[INDEX3zyx_2(k, j, i, nlatsnlons, nlons)]; }

    inline quint64 getFlags(MIndex3D idx)
    { return flags[INDEX3zyx_2(idx.k, idx.j, idx.i, nlatsnlons, nlons)]; }

    /**
      Returns the handle to a texture containing the flag data (3D int
      texture). Needs to be released with @ref releaseFlagsTexture().
     */
    virtual GL::MTexture* getFlagsTexture(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif

    /** Release a texture acquired with getFlagsTexture(). */
    void releaseFlagsTexture();

    inline void setContributingMember(unsigned char m)
    { contributingMembers |= (Q_UINT64_C(1) << m); }

    inline void setContributingMembers(quint64 memberBitfield)
    { contributingMembers = memberBitfield; }

    inline quint64 getContributingMembers() const
    { return contributingMembers; }

    inline bool memberIsContributing(unsigned char m) const
    { return contributingMembers & (Q_UINT64_C(1) << m); }

    /** Returns the number of set bits in "contributing members". */
    unsigned int getNumContributingMembers() const;

    inline void setAvailableMember(unsigned char m)
    { availableMembers |= (Q_UINT64_C(1) << m); }

    inline void setAvailableMembers(quint64 memberBitfield)
    { availableMembers = memberBitfield; }

    inline quint64 getAvailableMembers() const
    { return availableMembers; }

    unsigned char getMaxAvailableMember() const;

    unsigned char getMinAvailableMember() const;

    /**
      Texture for empty space skipping: Creates (or returns if already created)
      a 3D grid of fixed size NIxNJxNK (specified in the method, e.g. 32x32x32)
      that subdivides the world space covered by the data volume into regular
      bricks. For each brick, the minimum/maximum values of the data points
      that overlap with the brick are stored in the red/green texture
      components. The texture can be used in the shader to skip regions in
      which an isosurface cannot be located.

      References: Krueger and Westermann (2003); Shirley, Fundamentals of
      Computer Graphics, 3rd ed. (2009), Ch. 12.2.3.
     */
    GL::MTexture* getMinMaxAccelTexture3D(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif

    void releaseMinMaxAccelTexture3D();

    /**
      Computes the longitudinal grid point spacing in km at the specified
      latitude index.
     */
    float getDeltaLon_km(const int iLat) const;

    /**
      Computes the latitudinal grid point spacing in km.
     */
    float getDeltaLat_km() const;

    MVerticalLevelType getVerticalLevelType() { return leveltype; }

protected:
    friend class MClimateForecastReader; // NetCDF can read directly into data
                                         // fields.
    friend class MTrajectoryReader;
    friend class MTrajectoryComputationSource;
    friend class MNWPHorizontalSectionActor;
    friend class MNWPVerticalSectionActor;
    friend class MNWPSurfaceTopographyActor;
    friend class MLonLatHybVolumeActor;
    friend class MNWPVolumeRaycasterActor;
    friend class MNWP2DHorizontalActorVariable;
    friend class MNWP2DVerticalActorVariable;
    friend class MStructuredGridEnsembleFilter;
    friend class MVerticalRegridder;
    friend class MGribReader;
    friend class MDifferenceDataSource;
    friend class MProcessingWeatherPredictionDataSource;
    friend class MPotentialVorticityProcessor_LAGRANTOcalvar;

    /** Sizes of the dimensions. */
    unsigned int nlevs, nlats, nlons;
    unsigned int nvalues;
    unsigned int nlatsnlons; // precomputed nlats*nlons

    /** Coordinate axes and, if required, level coefficients. */
    double *levels, *lats, *lons;

    /** The data field. */
    float   *data;
    quint64 *flags;
    bool     flagsCanBeEnabled;
    quint64  contributingMembers;
    quint64  availableMembers;
    MHorizontalGridType horizontalGridType;

    /** Texture parameters. **/
    GLint  textureInternalFormat;
    GLenum textureFormat;
    GLint  textureWrap;
    GLint  textureMinMaxFilter;

    MVerticalLevelType leveltype;

    QString lonlatID; /** Texture ID string for the coordinate axes. */
    QString flagsID;
    QString minMaxAccelID;

    MMemoryManagedArray<float>* minMaxAccel;
};


class MRegularLonLatLnPGrid : public MStructuredGrid
{
public:
    MRegularLonLatLnPGrid(unsigned int nlevs, unsigned int nlats,
                          unsigned int nlons);

    float interpolateGridColumnToPressure(unsigned int j, unsigned int i,
                                          float p_hPa);

    float levelPressureAtLonLat_hPa(float lon, float lat, unsigned int k) override;

    int findLevel(unsigned int j, unsigned int i, float p_hPa);

    float getPressure(unsigned int k, unsigned int j, unsigned int i);

    float getBottomInterfacePressure(
            unsigned int k, unsigned int j, unsigned int i);

    float getTopInterfacePressure(
            unsigned int k, unsigned int j, unsigned int i);

    float getTopDataVolumePressure_hPa(bool useCachedValue=true) override
    { Q_UNUSED(useCachedValue); return exp(levels[0]); }

    float getBottomDataVolumePressure_hPa(bool useCachedValue=true) override
    { Q_UNUSED(useCachedValue); return exp(levels[nlevs-1]); }

protected:

};


class MRegularLonLatStructuredPressureGrid : public MStructuredGrid
{
public:
    MRegularLonLatStructuredPressureGrid(unsigned int nlevs, unsigned int nlats,
                                         unsigned int nlons);

    /**
     Upload a 1D texture mapping ln(p), normalised to 0..1, to the texture
     coordinate required to sample the data volume texture.

     @note see notes 17Mar2014.
     */
    GL::MTexture* getPressureTexCoordTexture1D(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif

    void releasePressureTexCoordTexture1D();

    float interpolateGridColumnToPressure(unsigned int j, unsigned int i,
                                          float p_hPa);

    float levelPressureAtLonLat_hPa(float lon, float lat, unsigned int k) override;

    int findLevel(unsigned int j, unsigned int i, float p_hPa);

    float getPressure(unsigned int k, unsigned int j, unsigned int i);

    float getBottomInterfacePressure(
            unsigned int k, unsigned int j, unsigned int i);

    float getTopInterfacePressure(
            unsigned int k, unsigned int j, unsigned int i);

    float getTopDataVolumePressure_hPa(bool useCachedValue=true) override
    { Q_UNUSED(useCachedValue); return levels[0]; }

    float getBottomDataVolumePressure_hPa(bool useCachedValue=true) override
    { Q_UNUSED(useCachedValue); return levels[nlevs-1]; }

protected:

private:
    QString pressureTableID; /** Texture ID string for the pressure table. */

};


class MRegularLonLatGrid : public MStructuredGrid
{
public:
    MRegularLonLatGrid(unsigned int nlats, unsigned int nlons);

    inline void setValue(unsigned int j, unsigned int i, float v)
    { data[INDEX2yx(j, i, nlons)] = v; }

    inline float getValue(unsigned int j, unsigned int i) const
    { return data[INDEX2yx(j, i, nlons)]; }

    /**
      2D special case: ignore @p p_hPa parameter and simply map to getValue().
      Implementation required for @ref MStructuredGrid::interpolateValue().
     */
    float interpolateGridColumnToPressure(unsigned int j, unsigned int i,
                                          float p_hPa)
    { Q_UNUSED(p_hPa); return getValue(j, i); }

    GL::MTexture* getTexture(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr,
#else
            QGLWidget *currentGLContext = nullptr,
#endif
            bool nullTexture = false);

    GL::MTexture* getFlagsTexture(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif

protected:

};


class MLonLatHybridSigmaPressureGrid : public MStructuredGrid
{
public:
    MLonLatHybridSigmaPressureGrid(unsigned int nlevs, unsigned int nlats,
                                   unsigned int nlons);

    ~MLonLatHybridSigmaPressureGrid();

    unsigned int getMemorySize_kb();

    /**
      Returns a pointer to the surface pressure grid associated with this
      hybrid sigma-pressure levels grid object.
      @note This method does NOT increase the reference count of the surface
      pressure field in the memory manager, hence does not need to be released.
     */
    MRegularLonLatGrid* getSurfacePressureGrid() { return surfacePressure; }

    /**
      Exchanges the associated surface pressure field with
      @p newSfcPressureGrid.
      @note If the new field is memory managed (should almost always be the
      case), the reference counter needs to be increased before the field is
      passed to this method! See, e.g., use in @ref MSmoothFilter::produceData().
     */
    void exchangeSurfacePressureGrid(MRegularLonLatGrid* newSfcPressureGrid);

    GL::MTexture* getHybridCoeffTexture(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif

    void releaseHybridCoeffTexture();

    /**
     */
    GL::MTexture* getPressureTexCoordTexture2D(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif

    void releasePressureTexCoordTexture2D();

    /**
      Computes the indices of @ref levels, @ref ak, @ref bk of the two levels
      that enclose the pressure value @p p_hPa for a pressure column with
      surface pressure @p psfc_hPa. The result is written to the integers @p
      kLowerPressure and @p kUpperPressure .

      A binary search is carried out to find the levels.
     */
    void findEnclosingModelLevels(double psfc_hPa, double p_hPa,
                                  int *kLowerPressure, int *kUpperPressure);

    float interpolateGridColumnToPressure(unsigned int j, unsigned int i,
                                          float p_hPa);

    float levelPressureAtLonLat_hPa(float lon, float lat, unsigned int k) override;

    int findLevel(unsigned int j, unsigned int i, float p_hPa);

    float getPressure(unsigned int k, unsigned int j, unsigned int i);

    void computeInterfaceCoefficients();

    float getBottomInterfacePressure(
            unsigned int k, unsigned int j, unsigned int i);

    float getTopInterfacePressure(
            unsigned int k, unsigned int j, unsigned int i);

    float getTopDataVolumePressure_hPa(bool useCachedValue=true) override;

    float getBottomDataVolumePressure_hPa(bool useCachedValue=true) override;

    void dumpGridData(unsigned int maxValues=50);

protected:
    friend class MWeatherPredictionReader;
    friend class MClimateForecastReader;
    friend class MStructuredGridEnsembleFilter;
    friend class MVerticalRegridder;
    friend class MProbabilityRegionDetectorFilter;
    friend class MTrajectoryReader;
    friend class MProbDFTrajectoriesSource;
    friend class MProbABLTrajectoriesSource;
    friend class MGribReader;
    friend class MDifferenceDataSource;
    friend class MProcessingWeatherPredictionDataSource;
    friend class MPotentialVorticityProcessor_LAGRANTOcalvar;

    void allocateInterfaceCoefficients();

    /** Hybrid model level coefficients. */
    double *ak_hPa, *bk;

    /** Hybrid model level coefficients, level interfaces (NOTE: the index is
    shifted by 1 wrt ak/bk --> aki[k] is used to compute the *top* interface
    pressure of the grid box centred at the pressure computed with ak[k]). */
    double *aki_hPa, *bki;

    MRegularLonLatGrid *surfacePressure;

    QString akbkID; /** Texture ID string for the hybrid coeffs. */

    QString getPressureTexCoordID();

private:
    /**
      Releases (if memory managed) or deletes the current surface pressure
      field.
     */
    void removeSurfacePressureField();

    QString pressureTexCoordID;
    double cachedTopDataVolumePressure_hPa;
    double cachedBottomDataVolumePressure_hPa;
};


class MLonLatAuxiliaryPressureGrid : public MStructuredGrid
{
public:
    MLonLatAuxiliaryPressureGrid(unsigned int nlevs, unsigned int nlats,
                                 unsigned int nlons, bool reverseLevels);

    ~MLonLatAuxiliaryPressureGrid();

    MLonLatAuxiliaryPressureGrid* getAuxiliaryPressureFieldGrid()
    { return auxPressureField_hPa; }

    /**
      Exchanges the associated surface pressure field with
      @p newSfcPressureGrid.
      @note If the new field is memory managed (should almost always be the
      case), the reference counter needs to be increased before the field is
      passed to this method! See, e.g., use in @ref MSmoothFilter::produceData().
     */
    void exchangeAuxiliaryPressureGrid(MLonLatAuxiliaryPressureGrid* newAuxPGrid);

    float interpolateGridColumnToPressure(unsigned int j, unsigned int i,
                                          float p_hPa);

    float levelPressureAtLonLat_hPa(float lon, float lat, unsigned int k) override;

    int findLevel(unsigned int j, unsigned int i, float p_hPa);

    float getPressure(unsigned int k, unsigned int j, unsigned int i);

    float getBottomInterfacePressure(
            unsigned int k, unsigned int j, unsigned int i);

    float getTopInterfacePressure(
            unsigned int k, unsigned int j, unsigned int i);

    float getTopDataVolumePressure_hPa(bool useCachedValue=true) override;

    float getBottomDataVolumePressure_hPa(bool useCachedValue=true) override;

    /**
      Returns the reverseLevels-Flag.

      Since the structured grid is used to obtain the informations about the
      auxiliary pressure field if it is not stored in the same file, this
      method can be used to get the reverseLevels-Flag.
      */
    bool getReverseLevels() { return reverseLevels; }

    void dumpGridData(unsigned int maxValues=50);

protected:
    friend class MWeatherPredictionReader;
    friend class MClimateForecastReader;
    friend class MStructuredGridEnsembleFilter;
    friend class MProcessingWeatherPredictionDataSource;

    MLonLatAuxiliaryPressureGrid *auxPressureField_hPa;

    bool reverseLevels;

private:
    void removeAuxiliaryPressureField();

    double cachedTopDataVolumePressure_hPa;
    double cachedBottomDataVolumePressure_hPa;
};


} // namespace Met3D

#endif // STRUCTUREDGRID_H
