/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2017      Philipp Kaiser [+]
**  Copyright 2020      Marcel Meyer [*]
**
**  + Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
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
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
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


void MTrajectories::copyAuxDataPerVertex(float *auxData, int iIndexAuxData)
{
    auxDataAtVertices.resize(getVertices().size());
    for (int i = 0; i < getVertices().size(); i++)
    {        
        auxDataAtVertices[i].resize(iIndexAuxData+1);
        auxDataAtVertices[i][iIndexAuxData] = auxData[i];
    }
}


void MTrajectories::copyAuxDataPerVertex(QVector<QVector<QVector<float>>> &av)
{
    // Copy auxiliary data from one QVector array to another QVector array
    // with the format of the internal (trajectory class) auxiliary data array.
    // Array-Indexing:
    // Input: The input to this method is a 3-D array (Qvector of Qvector of Qvector
    // with dimensions: (i) trajectories, (ii) time-steps per trajectory,
    // (iii) aux data.
    // Output: The auxiliary data in the trajectory class is stored in the
    // format QVector<Qvector<float>> where the vertices of all trajectories
    // are stored in one long list in the first dimension of the QVector
    // (traj1_v1, traj1_v2,...,traj1_vN, traj2_v1,traj2_v2,...), and the
    // auxiliary data variables at each of the vertices are stored in the
    // second dimension (QVector<float>).
    int numTrajs = av.size();
    for (int i = 0; i < numTrajs; ++i)
    {
        int numVerticesPerTraj = av[i].size();
        for (int j = 0; j < numVerticesPerTraj; ++j)
        {
            if ((i == 0) && (j == 0))
            {
                auxDataAtVertices.resize(av.size()*av[0].size());
            }
            int numAuxDataVars = av[i][j].size();
            for (int k = 0; k < numAuxDataVars; k++)
            {
                auxDataAtVertices[i * numVerticesPerTraj + j].resize(
                            av[0][0].size());
                auxDataAtVertices[i * numVerticesPerTraj + j][k] =
                        av.at(i).at(j).at(k);
            }
        }
    }
}


void MTrajectories::setAuxDataVariableNames(QStringList varNames)
{
    auxDataVarNames = varNames;
}


unsigned int MTrajectories::getTimeStepLength_sec()
{
    if (times.size() < 2)
        return 0;
    else
        return times.at(0).secsTo(times.at(1));
}


GL::MVertexBuffer* MTrajectories::getVertexBuffer(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
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
    {
        newVB->upload(vertices, currentGLContext);
    }
    else
    {
        delete newVB;
    }

    return static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(getID()));
}


GL::MVertexBuffer* MTrajectories::getAuxDataVertexBuffer(
        QString requestedAuxDataVarName,
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
{
    // Initialize instance of openGL resource manager.
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if requested variable name exists as aux var.
    assert(auxDataVarNames.contains(requestedAuxDataVarName));

    QString gpuItemID = getID() + "_aux_" + requestedAuxDataVarName;

    // Check if a texture with this item's data already exists in GPU memory.
    GL::MVertexBuffer *vb = static_cast<GL::MVertexBuffer*>(
                glRM->getGPUItem(gpuItemID));
    if (vb) return vb;

    // Get the requested auxiliary data from the QVector with all auxiliary
    // data vars along trajectories.
    QVector<float> requestedAuxDataAtVertices(auxDataAtVertices.size());
    int i = auxDataVarNames.indexOf(requestedAuxDataVarName);
    for (int j = 0; j < auxDataAtVertices.size(); j++)
    {
        requestedAuxDataAtVertices[j] = auxDataAtVertices.at(j).at(i);
    }

    // If no texture with this item's data exists, then create a new one.
    GL::MFloatVertexBuffer *newVB = new GL::MFloatVertexBuffer(
                gpuItemID, requestedAuxDataAtVertices.size());

    // Upload the requested auxiliary data.
    if (glRM->tryStoreGPUItem(newVB))
    {
        newVB->upload(requestedAuxDataAtVertices, currentGLContext);
    }
    else
    {
        delete newVB;
    }

    return static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(gpuItemID));
}


void MTrajectories::releaseVertexBuffer()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(getID());
}


void MTrajectories::releaseAuxDataVertexBuffer(QString requestedAuxDataVarName)
{
    // Check if requested variable name exists as aux var.
    assert(auxDataVarNames.contains(requestedAuxDataVarName));

    QString gpuItemID = getID() + "_aux_" + requestedAuxDataVarName;
    MGLResourcesManager::getInstance()->releaseGPUItem(gpuItemID);
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
