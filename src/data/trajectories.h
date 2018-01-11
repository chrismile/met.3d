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
#ifndef TRAJECTORIES_H
#define TRAJECTORIES_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include <QGLWidget>

// local application imports
#include "data/abstractdataitem.h"
#include "gxfw/gl/typedvertexbuffer.h"
#include "data/structuredgrid.h"


namespace Met3D
{

/**
  @brief Base class for all objects that store supplemental data along with
  trajectory data.
 */
class MSupplementalTrajectoryData : public MAbstractDataItem
{
public:
    MSupplementalTrajectoryData(MDataRequest requestToReferTo,
                                unsigned int numTrajectories);

    virtual MDataRequest refersTo() { return requestToReferTo; }

    int getNumTrajectories() const { return numTrajectories; }

protected:
    int numTrajectories;

private:
    MDataRequest requestToReferTo;

};


/**
  @brief Defines a selection of a trajectory dataset.
 */
class MTrajectorySelection : public MSupplementalTrajectoryData
{
public:
    MTrajectorySelection(MDataRequest requestToReferTo,
                         unsigned int numTrajectories,
                         QVector<QDateTime> timeValues,
                         QVector3D startGridStride=QVector3D(1,1,1));
    ~MTrajectorySelection();

    unsigned int getMemorySize_kb();

    /**
      Index [i_filtered] stores the start index of filtered trajectory
      i_filtered in the @ref MTrajectories vertex field (@see
      MTrajectories::getVertices()).

     @note Index i_filtered is not equal to trajectory i_full in the "full",
     unfiltered field of trajectories returned by @ref
     MTrajectories::getVertices(). Use i_full =
     ceil(float(startIndex)/numTimes) to get this index.
     */
    inline const GLint* getStartIndices() const { return startIndices; }

    /**
      Index [i] stores the number of vertices of filtered trajectory i in the
      @ref MTrajectories vertex field (@see MTrajectories::getVertices()).
     */
    inline const GLsizei* getIndexCount() const { return indexCount; }

    /**
      Total number of timesteps of each trajectory. This number need not be
      equal to a trajectory's index count!
     */
    inline int getNumTimeStepsPerTrajectory() const { return times.size(); }

    inline const QVector<QDateTime>& getTimes() const { return times; }

    inline const QVector3D getStartGridStride() const { return startGridStride; }

protected:
    GLint   *startIndices;
    GLsizei *indexCount;
    int      maxNumTrajectories;

    QVector<QDateTime> times;

    QVector3D startGridStride; // this is 1 for each coordinate unless
                               // trajectories have been thinned out
};


/**
  @brief As @ref MTrajectorySelection, but can be written.
 */
class MWritableTrajectorySelection : public MTrajectorySelection
{
public:
    MWritableTrajectorySelection(MDataRequest requestToReferTo,
                                 unsigned int numTrajectories,
                                 QVector<QDateTime> timeValues,
                                 QVector3D startGridStride);

    inline void setStartIndex(unsigned int i, int value)
    { startIndices[i] = value; }

    inline void setIndexCount(unsigned int i, int value)
    { indexCount[i] = value; }

    /**
      Only modify start grid stride if trajectories have been thinned out!
     */
    inline void setStartGridStride(QVector3D stride)
    { startGridStride = stride; }

    /**
      Decrease the number of selected trajectories to @p n. @p n needs to be
      smaller than the number of trajectories specified in the constructor.
     */
    void decreaseNumSelectedTrajectories(int n);

};


/**
  Supplements each trajectory of an @ref MTrajectories item with a float
  argument (only one value per trajectory, not one value per vertex!).
 */
class MFloatPerTrajectorySupplement : public MSupplementalTrajectoryData
{
public:
    MFloatPerTrajectorySupplement(MDataRequest requestToReferTo,
                                  unsigned int numTrajectories);

    unsigned int getMemorySize_kb();

    const QVector<float>& getValues() const { return values; }

    inline void setValue(unsigned int i, float value) { values[i] = value; }

protected:
    friend class MTrajectoryReader;

    QVector<float> values;
};


/**
  @brief Normals associated with a trajectory dataset and a specific view
  (normals depend on the view's z-scaling).
 */
class MTrajectoryNormals : public MSupplementalTrajectoryData
{
public:
    MTrajectoryNormals(MDataRequest requestToReferTo,
                       unsigned int numTrajectories,
                       unsigned int numTimeStepsPerTrajectory);

    ~MTrajectoryNormals();

    unsigned int getMemorySize_kb();

    const QVector<QVector3D>& getWorldSpaceNormals() const
    { return normals; }

    inline void setNormal(unsigned int i, QVector3D normal)
    { normals[i] = normal; }

    /**
     */
    GL::MVertexBuffer *getVertexBuffer(QGLWidget *currentGLContext = 0);

    void releaseVertexBuffer();

private:
    QVector<QVector3D> normals;

};


/**
 @brief Stores the trajectories of a single forecast member at a single
 timestep. The smallest entitiy that can be read from disk.
 */
class MTrajectories :
        public MTrajectorySelection, public MWeatherPredictionMetaData
{
public:
    /**
      Constructor requires data size for memory allocation.

      Call @ref MWeatherPredictionMetaData::setMetaData() to set init, valid
      time, name and ensemble member.
     */
    MTrajectories(unsigned int numTrajectories,
                  QVector<QDateTime> timeValues);

    /** Destructor frees data in verticesLonLatP. */
    ~MTrajectories();

    unsigned int getMemorySize_kb();

    MDataRequest refersTo() { return getGeneratingRequest(); }

    /**
      Copies data from the given float arrays (longitude in degrees, latitude
      in degrees, pressure in hPa) to the internal QVector-based vertex array.
      All three arrays must have the size (numTrajectories *
      numTimeStepsPerTrajectory).
     */
    void copyVertexDataFrom(float *lons, float *lats, float *pres);

    const QVector<QVector3D>& getVertices() { return vertices; }

    /**
      Returns the length of a single time step in seconds.
     */
    unsigned int getTimeStepLength_sec();

    /**
      Pass an MStructuredGrid instance that contains the geometry of the grid
      on which the trajectories were started.
     */
    void setStartGrid(std::shared_ptr<MStructuredGrid> sg) { startGrid = sg; }

    const std::shared_ptr<MStructuredGrid> getStartGrid() const { return startGrid; }

    /**
      Return a vertex buffer object that contains the trajectory data. The
      vertex buffer is created (and data uploaded) on the first call to this
      method.

      The @p currentGLContext argument is necessary as a GPU upload can switch
      the currently active OpenGL context. As this method is usually called
      from a render method, it should switch back to the current render context
      (given by @p currentGLContext).
     */
    GL::MVertexBuffer *getVertexBuffer(QGLWidget *currentGLContext = 0);

    void releaseVertexBuffer();

    /**
      Debug method to dump the start positions if the first @p num trajectories
      to the debug log. If @p selection is specified, dump the first @p num
      trajectories of the selection.
     */
    void dumpStartVerticesToLog(int num, MTrajectorySelection *selection=nullptr);

private:
    QVector<QVector3D> vertices;
    std::shared_ptr<MStructuredGrid> startGrid;

};


} // namespace Met3D

#endif // TRAJECTORIES_H
