/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2017      Philipp Kaiser
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
#include "trajectories.h"

// standard library imports
#include <iostream>
#include <limits>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"

using namespace std;


namespace Met3D
{

#define MISSING_VALUE -999.E9f

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSupplementalTrajectoryData::MSupplementalTrajectoryData(
        MDataRequest requestToReferTo, unsigned int numTrajectories)
    : MAbstractDataItem(),
      numTrajectories(numTrajectories),
      requestToReferTo(requestToReferTo)
{
}


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectorySelection::MTrajectorySelection(MDataRequest requestToReferTo,
                                           unsigned int numTrajectories,
                                           QVector<QDateTime> timeValues,
                                           QVector3D startGridStride)
    : MSupplementalTrajectoryData(requestToReferTo, numTrajectories),
      startIndices(new GLint[numTrajectories]),
      indexCount(new GLsizei[numTrajectories]),
      maxNumTrajectories(numTrajectories),
      times(timeValues),
      startGridStride(startGridStride)
{
}


MTrajectorySelection::~MTrajectorySelection()
{
    delete[] startIndices;
    delete[] indexCount;
}


unsigned int MTrajectorySelection::getMemorySize_kb()
{
    return ( sizeof(MTrajectorySelection)
             + times.size() * sizeof(QDateTime)
             + maxNumTrajectories * (sizeof(GLint) + sizeof(GLsizei))
             ) / 1024.;
}


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MWritableTrajectorySelection::MWritableTrajectorySelection(
        MDataRequest requestToReferTo, unsigned int numTrajectories,
        QVector<QDateTime> timeValues, QVector3D startGridStride)
    : MTrajectorySelection(requestToReferTo, numTrajectories, timeValues,
                           startGridStride)
{
}


void MWritableTrajectorySelection::decreaseNumSelectedTrajectories(int n)
{
    if (n <= numTrajectories)
        numTrajectories = n;
    else
        throw MValueError("number of selected trajectories cannot be increased",
                          __FILE__, __LINE__);
}


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MFloatPerTrajectorySupplement::MFloatPerTrajectorySupplement(
        MDataRequest requestToReferTo, unsigned int numTrajectories)
    : MSupplementalTrajectoryData(requestToReferTo, numTrajectories)
{
    values.resize(numTrajectories);
}


unsigned int MFloatPerTrajectorySupplement::getMemorySize_kb()
{
    return ( sizeof(MFloatPerTrajectorySupplement)
             + values.size() * sizeof(float)
             ) / 1024.;
}


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryNormals::MTrajectoryNormals(MDataRequest requestToReferTo,
                                       unsigned int numTrajectories,
                                       unsigned int numTimeStepsPerTrajectory)
    : MSupplementalTrajectoryData(requestToReferTo, numTrajectories)
{
    normals.resize(numTrajectories*numTimeStepsPerTrajectory);
}


MTrajectoryNormals::~MTrajectoryNormals()
{
    // Make sure the corresponding data is removed from GPU memory as well.
    MGLResourcesManager::getInstance()->releaseAllGPUItemReferences(getID());
}


unsigned int MTrajectoryNormals::getMemorySize_kb()
{
    return ( sizeof(MTrajectoryNormals)
             + normals.size() * sizeof(QVector3D)
             ) / 1024.;
}


GL::MVertexBuffer* MTrajectoryNormals::getVertexBuffer(
        QGLWidget *currentGLContext)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MVertexBuffer *vb = static_cast<GL::MVertexBuffer*>(
                glRM->getGPUItem(getID()));
    if (vb) return vb;

    // No texture with this item's data exists. Create a new one.
    GL::MVector3DVertexBuffer *newVB = new GL::MVector3DVertexBuffer(
                getID(), normals.size());

    if (glRM->tryStoreGPUItem(newVB))
        newVB->upload(normals, currentGLContext);
    else
        delete newVB;

    return static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(getID()));
}


void MTrajectoryNormals::releaseVertexBuffer()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(getID());
}


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectories::MTrajectories(
        unsigned int numTrajectories, QVector<QDateTime> timeValues)
    : MTrajectorySelection(MDataRequest(), numTrajectories, timeValues),
      MWeatherPredictionMetaData()
{
    int numTimeStepsPerTrajectory = times.size();
    // Allocate memory for each time step of each trajectory (lon/lat/p).
    vertices.resize(numTrajectories*numTimeStepsPerTrajectory);

    // Assign arrays with trajectory start indices and index counts; required
    // for calls to glMultiDrawArrays().
    for (unsigned int i = 0; i < numTrajectories; i++) {
        startIndices[i] = i * numTimeStepsPerTrajectory;
        indexCount[i]   = numTimeStepsPerTrajectory;
    }
}


MTrajectories::~MTrajectories()
{
    // Make sure the corresponding data is removed from GPU memory as well.
    MGLResourcesManager::getInstance()->releaseAllGPUItemReferences(getID());
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

unsigned int MTrajectories::getMemorySize_kb()
{
    return ( MTrajectorySelection::getMemorySize_kb()
             + sizeof(MTrajectories)
             + vertices.size() * sizeof(QVector3D)
             + (startGrid ? startGrid->getMemorySize_kb() : 0)
             ) / 1024.;
}


void MTrajectories::copyVertexDataFrom(
        float *lons, float *lats, float *pres)
{
    for (int i = 0; i < getVertices().size(); i++)
    {
        vertices[i] = QVector3D(lons[i], lats[i], pres[i]);
    }
}


void MTrajectories::copyVertexDataFrom(QVector<QVector<QVector3D>> &v)
{
    for (int i = 0; i < v.size(); ++i)
    {
        int viSize = v[i].size();
        for (int j = 0; j < viSize; ++j)
        {
            vertices[i * viSize + j] = v[i][j];
        }
    }
}


unsigned int MTrajectories::getTimeStepLength_sec()
{
    if (times.size() < 2)
        return 0;
    else
        return times.at(0).secsTo(times.at(1));
}


GL::MVertexBuffer* MTrajectories::getVertexBuffer(QGLWidget *currentGLContext)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MVertexBuffer *vb = static_cast<GL::MVertexBuffer*>(
                glRM->getGPUItem(getID()));
    if (vb) return vb;

    // No texture with this item's data exists. Create a new one.
    GL::MVector3DVertexBuffer *newVB = new GL::MVector3DVertexBuffer(
                getID(), vertices.size());

    if (glRM->tryStoreGPUItem(newVB))
        newVB->upload(vertices, currentGLContext);
    else
        delete newVB;

    return static_cast<GL::MVertexBuffer*>(
                glRM->getGPUItem(getID()));
}


void MTrajectories::releaseVertexBuffer()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(getID());
}


void MTrajectories::dumpStartVerticesToLog(int num,
                                           MTrajectorySelection *selection)
{
    if (selection == nullptr) selection = this;
    int tindex = times.indexOf(validTime);
    for (int i = 0; i < min(num, selection->getNumTrajectories()); i++)
    {
        QVector3D v = vertices.at(selection->getStartIndices()[i] + tindex);
        LOG4CPLUS_DEBUG_FMT(mlog, "Trajectory %i: (%.2f/%.2f/%.2f)", i,
                            v.x(), v.y(), v.z());

    }
}


} // namespace Met3D
