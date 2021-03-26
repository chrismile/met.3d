/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020-2021 Christoph Neuhauser
**  Copyright 2020      Michael Kern
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
#include "beziertrajectories.h"

// standard library imports

// related third party imports

// local application imports
#include "helpers.h"

namespace Met3D {

unsigned int MBezierTrajectory::getMemorySize_kb() const {
    size_t sizeBytes =
            sizeof(MBezierTrajectory)
            + positions.size() * sizeof(QVector3D)
            + attributes.size() * attributes.front().size() * sizeof(float)
            + multiVarData.size() * sizeof(float)
            + sizeof(LineDesc)
            + multiVarDescs.size() * sizeof(VarDesc);
    return static_cast<unsigned int>(sizeBytes / 1024ull);
}


MBezierTrajectories::MBezierTrajectories(
        MDataRequest requestToReferTo, unsigned int numTrajectories, unsigned int numVariables)
        : MSupplementalTrajectoryData(requestToReferTo, numTrajectories), bezierTrajectories(numTrajectories)
{
    for (unsigned int i = 0; i < numVariables; i++)
    {
        varSelected.push_back(true);
    }
}


MBezierTrajectories::~MBezierTrajectories()
{
    // Make sure the corresponding data is removed from GPU memory as well.
    MGLResourcesManager::getInstance()->releaseAllGPUItemReferences(getID());
}


unsigned int MBezierTrajectories::getMemorySize_kb()
{
    unsigned int size = sizeof(MBezierTrajectories) / 1024;
    foreach(const MBezierTrajectory& bezierTrajectory, bezierTrajectories)
    {
        size += bezierTrajectory.getMemorySize_kb();
    }
    return size;
}


template<typename T>
void createLineTubesRenderDataCPU(
        const QVector<QVector<QVector3D>>& lineCentersList,
        const QVector<QVector<T>>& lineAttributesList,
        QVector<uint32_t>& lineIndices,
        QVector<QVector3D>& vertexPositions,
        QVector<QVector3D>& vertexNormals,
        QVector<QVector3D>& vertexTangents,
        QVector<T>& vertexAttributes)
{
    assert(lineCentersList.size() == lineAttributesList.size());
    for (int lineId = 0; lineId < lineCentersList.size(); lineId++)
    {
        const QVector<QVector3D> &lineCenters = lineCentersList.at(lineId);
        const QVector<T> &lineAttributes = lineAttributesList.at(lineId);
        assert(lineCenters.size() == lineAttributes.size());
        size_t n = lineCenters.size();
        size_t indexOffset = vertexPositions.size();

        if (n < 2)
        {
            //sgl::Logfile::get()->writeError(
            //        "ERROR in createLineTubesRenderDataCPU: Line must consist of at least two points.");
            continue;
        }

        QVector3D lastLineNormal(1.0f, 0.0f, 0.0f);
        int numValidLinePoints = 0;
        for (size_t i = 0; i < n; i++)
        {
            QVector3D tangent, normal;
            if (i == 0)
            {
                tangent = lineCenters[i+1] - lineCenters[i];
            } else if (i == n - 1)
            {
                tangent = lineCenters[i] - lineCenters[i-1];
            } else {
                tangent = (lineCenters[i+1] - lineCenters[i-1]);
            }
            float lineSegmentLength = tangent.length();

            if (lineSegmentLength < 0.0001f)
            {
                // In case the two vertices are almost identical, just skip this path line segment
                continue;
            }
            tangent.normalize();

            QVector3D helperAxis = lastLineNormal;
            if (QVector3D::crossProduct(helperAxis, tangent).length() < 0.01f)
            {
                // If tangent == lastNormal
                helperAxis = QVector3D(0.0f, 1.0f, 0.0f);
                if (QVector3D::crossProduct(helperAxis, normal).length() < 0.01f)
                {
                    // If tangent == helperAxis
                    helperAxis = QVector3D(0.0f, 0.0f, 1.0f);
                }
            }
            normal = (helperAxis - tangent * QVector3D::dotProduct(helperAxis, tangent)).normalized(); // Gram-Schmidt
            lastLineNormal = normal;

            vertexPositions.push_back(lineCenters.at(i));
            vertexNormals.push_back(normal);
            vertexTangents.push_back(tangent);
            vertexAttributes.push_back(lineAttributes.at(i));
            numValidLinePoints++;
        }

        if (numValidLinePoints == 1)
        {
            // Only one vertex left -> Output nothing (tube consisting only of one point).
            vertexPositions.pop_back();
            vertexNormals.pop_back();
            vertexTangents.pop_back();
            vertexAttributes.pop_back();
            continue;
        }

        // Create indices
        for (int i = 0; i < numValidLinePoints-1; i++)
        {
            lineIndices.push_back(indexOffset + i);
            lineIndices.push_back(indexOffset + i + 1);
        }
    }
}


MBezierTrajectoriesRenderData MBezierTrajectories::getRenderData(QGLWidget *currentGLContext)
{
    QVector<QVector<QVector3D>> lineCentersList;
    QVector<QVector<QVector<float>>> lineAttributesList;
    QVector<uint32_t> lineIndices;
    QVector<QVector3D> vertexPositions;
    QVector<QVector3D> vertexNormals;
    QVector<QVector3D> vertexTangents;
    QVector<QVector<float>> vertexAttributes;

    lineCentersList.resize(bezierTrajectories.size());
    lineAttributesList.resize(bezierTrajectories.size());
    for (int trajectoryIdx = 0; trajectoryIdx < bezierTrajectories.size(); trajectoryIdx++)
    {
        const MBezierTrajectory& trajectory = bezierTrajectories.at(trajectoryIdx);
        const QVector<QVector<float>>& attributes = trajectory.attributes;
        QVector<QVector3D>& lineCenters = lineCentersList[trajectoryIdx];
        QVector<QVector<float>>& lineAttributes = lineAttributesList[trajectoryIdx];
        for (int i = 0; i < trajectory.positions.size(); i++)
        {
            lineCenters.push_back(trajectory.positions.at(i));

            QVector<float> attrs;
            attrs.reserve(attributes.size());
            for (int attrIdx = 0; attrIdx < attributes.size(); attrIdx++)
            {
                assert(trajectory.positions.size() == attributes.at(attrIdx).size());
                attrs.push_back(attributes.at(attrIdx).at(i));
            }
            lineAttributes.push_back(attrs);
        }
    }

    createLineTubesRenderDataCPU(
            lineCentersList, lineAttributesList,
            lineIndices, vertexPositions, vertexNormals, vertexTangents, vertexAttributes);

    QVector<QVector4D> vertexMultiVariableArray;
    QVector<QVector4D> vertexVariableDescArray;
    vertexMultiVariableArray.reserve(vertexAttributes.size());
    vertexVariableDescArray.reserve(vertexAttributes.size());
    for (int vertexIdx = 0; vertexIdx < vertexAttributes.size(); vertexIdx++)
    {
        QVector<float>& attrList = vertexAttributes[vertexIdx];
        vertexMultiVariableArray.push_back(QVector4D(
                attrList.at(0), attrList.at(1), attrList.at(2), attrList.at(3)));
        vertexVariableDescArray.push_back(QVector4D(
                attrList.at(4), attrList.at(5), attrList.at(6), attrList.at(7)));
    }



    MBezierTrajectoriesRenderData bezierTrajectoriesRenderData;

    // Add the index buffer.
    bezierTrajectoriesRenderData.indexBuffer = createIndexBuffer(
            currentGLContext, indexBufferID, lineIndices);

    // Add the position buffer.
    bezierTrajectoriesRenderData.vertexPositionBuffer = createVertexBuffer(
            currentGLContext, vertexPositionBufferID, vertexPositions);

    // Add the normal buffer.
    bezierTrajectoriesRenderData.vertexNormalBuffer = createVertexBuffer(
            currentGLContext, vertexNormalBufferID, vertexNormals);

    // Add the tangent buffer.
    bezierTrajectoriesRenderData.vertexTangentBuffer = createVertexBuffer(
            currentGLContext, vertexTangentBufferID, vertexTangents);

    // Add the attribute buffers.
    bezierTrajectoriesRenderData.vertexMultiVariableBuffer = createVertexBuffer(
            currentGLContext, vertexMultiVariableBufferID, vertexMultiVariableArray);
    bezierTrajectoriesRenderData.vertexVariableDescBuffer = createVertexBuffer(
            currentGLContext, vertexVariableDescBufferID, vertexVariableDescArray);



    // ------------------------------------------ Create SSBOs. ------------------------------------------
    size_t numLines = bezierTrajectories.size();
    QVector<float> varData;
    QVector<float> lineDescData(numLines);
    QVector<QVector4D> varDescData;
    QVector<QVector2D> lineVarDescData;

    for (MBezierTrajectory& bezierTrajectory : bezierTrajectories)
    {
        for (float attrVal : bezierTrajectory.multiVarData)
        {
            varData.push_back(attrVal);
        }
    }

    for (MBezierTrajectory& bezierTrajectory : bezierTrajectories)
    {
        for (float attrVal : bezierTrajectory.multiVarData)
        {
            varData.push_back(attrVal);
        }
    }
    for (size_t lineIdx = 0; lineIdx < numLines; ++lineIdx)
    {
        lineDescData[lineIdx] = bezierTrajectories.at(lineIdx).lineDesc.startIndex;
    }

    const size_t numVars = bezierTrajectories.front().multiVarDescs.size();
    QVector<float> attributesMinValues(numVars, 0.0f);
    QVector<float> attributesMaxValues(numVars, 0.0f);
    for (size_t lineIdx = 0; lineIdx < numLines; ++lineIdx)
    {
        MBezierTrajectory& bezierTrajectory = bezierTrajectories[lineIdx];
        for (size_t varIdx = 0; varIdx < numVars; ++varIdx)
        {
            attributesMinValues[varIdx] = std::min(
                    attributesMinValues.at(varIdx), bezierTrajectory.multiVarDescs.at(varIdx).minMax.x());
            attributesMaxValues[varIdx] = std::max(
                    attributesMaxValues.at(varIdx), bezierTrajectory.multiVarDescs.at(varIdx).minMax.y());
        }
    }

    for (size_t lineIdx = 0; lineIdx < numLines; ++lineIdx)
    {
        for (size_t varIdx = 0; varIdx < numVars; ++varIdx)
        {
            MBezierTrajectory& bezierTrajectory = bezierTrajectories[lineIdx];

            QVector4D descData(
                    bezierTrajectory.multiVarDescs.at(varIdx).startIndex,
                    attributesMinValues.at(varIdx), attributesMaxValues.at(varIdx), 0.0);
            varDescData.push_back(descData);

            QVector2D lineVarDesc(
                    bezierTrajectory.multiVarDescs.at(varIdx).minMax.x(),
                    bezierTrajectory.multiVarDescs.at(varIdx).minMax.y());
            lineVarDescData.push_back(lineVarDesc);
        }
    }

    bezierTrajectoriesRenderData.variableArrayBuffer = createShaderStorageBuffer(
            currentGLContext, variableArrayBufferID, varData);
    bezierTrajectoriesRenderData.lineDescArrayBuffer = createShaderStorageBuffer(
            currentGLContext, lineDescArrayBufferID, lineDescData);
    bezierTrajectoriesRenderData.varDescArrayBuffer = createShaderStorageBuffer(
            currentGLContext, varDescArrayBufferID, varDescData);
    bezierTrajectoriesRenderData.lineVarDescArrayBuffer = createShaderStorageBuffer(
            currentGLContext, lineVarDescArrayBufferID, lineVarDescData);
    bezierTrajectoriesRenderData.varSelectedArrayBuffer = createShaderStorageBuffer(
            currentGLContext, varSelectedArrayBufferID, varSelected);

    this->bezierTrajectoriesRenderData = bezierTrajectoriesRenderData;

    return bezierTrajectoriesRenderData;
}


void MBezierTrajectories::releaseRenderData()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(indexBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(vertexNormalBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(vertexTangentBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(vertexMultiVariableBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(vertexVariableDescBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(variableArrayBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(lineDescArrayBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(varDescArrayBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(lineVarDescArrayBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(varSelectedArrayBufferID);
}


void MBezierTrajectories::updateSelectedVariables(const QVector<uint32_t>& varSelected)
{
    this->varSelected = varSelected;
    if (bezierTrajectoriesRenderData.varSelectedArrayBuffer)
    {
        bezierTrajectoriesRenderData.varSelectedArrayBuffer->upload(varSelected.constData(), GL_STATIC_DRAW);
    }
}


}
