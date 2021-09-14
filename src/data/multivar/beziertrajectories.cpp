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
        MDataRequest requestToReferTo, const MFilteredTrajectories& filteredTrajectories,
        const QVector<int>& trajIndicesToFilteredIndicesMap,
        unsigned int numVariables)
        : MSupplementalTrajectoryData(requestToReferTo, filteredTrajectories.size()),
          baseTrajectories(filteredTrajectories), bezierTrajectories(filteredTrajectories.size()),
          trajIndicesToFilteredIndicesMap(trajIndicesToFilteredIndicesMap),
          numTrajectories(filteredTrajectories.size())
{
    for (unsigned int i = 0; i < numVariables; i++)
    {
        varSelected.push_back(true);
    }
    for (unsigned int i = 0; i < numVariables; i++)
    {
        varDiverging.push_back(false);
    }

    this->numTrajectories = static_cast<int>(numTrajectories);
    trajectorySelectionCount = new GLsizei[numTrajectories];
    trajectorySelectionIndices = new ptrdiff_t[numTrajectories];
}


MBezierTrajectories::~MBezierTrajectories()
{
    if (trajectorySelectionCount)
    {
        delete trajectorySelectionCount;
        trajectorySelectionCount = nullptr;
    }
    if (trajectorySelectionIndices)
    {
        delete trajectorySelectionIndices;
        trajectorySelectionIndices = nullptr;
    }

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
        QVector<uint32_t>& lineIndexOffsets,
        QVector<uint32_t>& numIndicesPerLine,
        QVector<uint32_t>& lineIndices,
        QVector<QVector3D>& vertexPositions,
        QVector<QVector3D>& vertexNormals,
        QVector<QVector3D>& vertexTangents,
        QVector<T>& vertexAttributes)
{
    assert(lineCentersList.size() == lineAttributesList.size());
    lineIndexOffsets.reserve(lineCentersList.size());
    numIndicesPerLine.reserve(lineCentersList.size());
    for (int lineId = 0; lineId < lineCentersList.size(); lineId++)
    {
        const QVector<QVector3D> &lineCenters = lineCentersList.at(lineId);
        const QVector<T> &lineAttributes = lineAttributesList.at(lineId);
        assert(lineCenters.size() == lineAttributes.size());
        size_t n = lineCenters.size();
        size_t indexOffset = vertexPositions.size();
        lineIndexOffsets.push_back(lineIndices.size());

        if (n < 2)
        {
            //sgl::Logfile::get()->writeError(
            //        "ERROR in createLineTubesRenderDataCPU: Line must consist of at least two points.");
            numIndicesPerLine.push_back(0);
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
            numIndicesPerLine.push_back(0);
            continue;
        }

        // Create indices
        numIndicesPerLine.push_back((numValidLinePoints - 1) * 2);
        for (int i = 0; i < numValidLinePoints - 1; i++)
        {
            lineIndices.push_back(indexOffset + i);
            lineIndices.push_back(indexOffset + i + 1);
        }
    }
}


MBezierTrajectoriesRenderData MBezierTrajectories::getRenderData(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
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

    trajectoryIndexOffsets.clear();
    numIndicesPerTrajectory.clear();
    createLineTubesRenderDataCPU(
            lineCentersList, lineAttributesList, trajectoryIndexOffsets, numIndicesPerTrajectory,
            lineIndices, vertexPositions, vertexNormals, vertexTangents, vertexAttributes);

    QVector<QVector4D> vertexMultiVariableArray;
    QVector<QVector4D> vertexVariableDescArray;
    QVector<float> vertexTimestepIndexArray;
    vertexMultiVariableArray.reserve(vertexAttributes.size());
    vertexVariableDescArray.reserve(vertexAttributes.size());
    vertexTimestepIndexArray.reserve(vertexAttributes.size());
    for (int vertexIdx = 0; vertexIdx < vertexAttributes.size(); vertexIdx++)
    {
        QVector<float>& attrList = vertexAttributes[vertexIdx];
        vertexMultiVariableArray.push_back(QVector4D(
                attrList.at(0), attrList.at(1), attrList.at(2), attrList.at(3)));
        vertexVariableDescArray.push_back(QVector4D(
                attrList.at(4), attrList.at(5), attrList.at(6), attrList.at(7)));
        vertexTimestepIndexArray.push_back(attrList.at(8));
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
    bezierTrajectoriesRenderData.vertexTimestepIndexBuffer = createVertexBuffer(
            currentGLContext, vertexTimestepIndexBufferID, vertexTimestepIndexArray);

    this->lineIndicesCache = lineIndices;
    this->vertexPositionsCache = vertexPositions;


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

    const size_t numVars = bezierTrajectories.empty() ? 0 : bezierTrajectories.front().multiVarDescs.size();
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
    bezierTrajectoriesRenderData.varDivergingArrayBuffer = createShaderStorageBuffer(
            currentGLContext, varDivergingArrayBufferID, varDiverging);

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
    MGLResourcesManager::getInstance()->releaseGPUItem(varDivergingArrayBufferID);
}


void MBezierTrajectories::updateSelectedVariables(const QVector<uint32_t>& varSelected)
{
    this->varSelected = varSelected;
    if (bezierTrajectoriesRenderData.varSelectedArrayBuffer)
    {
        bezierTrajectoriesRenderData.varSelectedArrayBuffer->upload(varSelected.constData(), GL_STATIC_DRAW);
    }
}


void MBezierTrajectories::updateDivergingVariables(const QVector<uint32_t>& varDiverging)
{
    this->varDiverging = varDiverging;
    if (bezierTrajectoriesRenderData.varDivergingArrayBuffer)
    {
        bezierTrajectoriesRenderData.varDivergingArrayBuffer->upload(varDiverging.constData(), GL_STATIC_DRAW);
    }
}


MTimeStepSphereRenderData* MBezierTrajectories::getTimeStepSphereRenderData(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
{
    QVector<uint32_t> triangleIndices;
    QVector<QVector3D> vertexPositions;
    QVector<QVector3D> vertexNormals;

    int numLatitudeSubdivisions = 128;
    int numLongitudeSubdivisions = 128;
    float theta; // azimuth;
    float phi; // zenith;

    for (int lat = 0; lat <= numLatitudeSubdivisions; lat++) {
        phi = float(M_PI) + float(M_PI) * (1.0f - float(lat) / float(numLatitudeSubdivisions));
        for (int lon = 0; lon < numLongitudeSubdivisions; lon++) {
            theta = -2.0f * float(M_PI) * float(lon) / float(numLongitudeSubdivisions);

            QVector3D pt(
                    std::cos(theta) * std::sin(phi),
                    std::sin(theta) * std::sin(phi),
                    std::cos(phi)
            );
            vertexNormals.push_back(pt);
            vertexPositions.push_back(pt);
        }
    }
    for (int lat = 0; lat < numLatitudeSubdivisions; lat++) {
        for (int lon = 0; lon < numLongitudeSubdivisions; lon++) {
            triangleIndices.push_back((lon)%numLongitudeSubdivisions
                                      + (lat)*numLongitudeSubdivisions);
            triangleIndices.push_back((lon+1)%numLongitudeSubdivisions
                                      + (lat)*numLongitudeSubdivisions);
            triangleIndices.push_back((lon)%numLongitudeSubdivisions
                                      + (lat+1)*numLongitudeSubdivisions);
            triangleIndices.push_back((lon+1)%numLongitudeSubdivisions
                                      + (lat)*numLongitudeSubdivisions);
            triangleIndices.push_back((lon+1)%numLongitudeSubdivisions
                                      + (lat+1)*numLongitudeSubdivisions);
            triangleIndices.push_back((lon)%numLongitudeSubdivisions
                                      + (lat+1)*numLongitudeSubdivisions);
        }
    }

    // Add the index buffer.
    timeStepSphereRenderData.indexBuffer = createIndexBuffer(
            currentGLContext, timeStepSphereIndexBufferID, triangleIndices);

    // Add the vertex position buffer.
    timeStepSphereRenderData.vertexPositionBuffer = createVertexBuffer(
            currentGLContext, timeStepSphereVertexPositionBufferID, vertexPositions);

    // Add the vertex normal buffer.
    timeStepSphereRenderData.vertexNormalBuffer = createVertexBuffer(
            currentGLContext, timeStepSphereVertexNormalBufferID, vertexNormals);

    return &timeStepSphereRenderData;
}

#define SQR(x) ((x)*(x))

float squareVec(QVector3D v) {
    return SQR(v.x()) + SQR(v.y()) + SQR(v.z());
}

/**
 * Implementation of ray-sphere intersection (idea from A. Glassner et al., "An Introduction to Ray Tracing").
 * For more details see: https://www.siggraph.org//education/materials/HyperGraph/raytrace/rtinter1.htm
 */
bool lineSegmentSphereIntersection(
        const QVector3D& p0, const QVector3D& p1, const QVector3D& sphereCenter, float sphereRadius, float& hitT) {
    const QVector3D& rayOrigin = p0;
    const QVector3D rayDirection = (p1 - p0).normalized();
    const float rayLength = (p1 - p0).length();

    float A = SQR(rayDirection.x()) + SQR(rayDirection.y()) + SQR(rayDirection.z());
    float B = 2.0f * (
            rayDirection.x() * (rayOrigin.x() - sphereCenter.x())
            + rayDirection.y() * (rayOrigin.y() - sphereCenter.y())
            + rayDirection.z() * (rayOrigin.z() - sphereCenter.z())
    );
    float C =
            SQR(rayOrigin.x() - sphereCenter.x())
            + SQR(rayOrigin.y() - sphereCenter.y())
            + SQR(rayOrigin.z() - sphereCenter.z())
            - SQR(sphereRadius);

    float discriminant = SQR(B) - 4.0f * A * C;
    if (discriminant < 0.0f) {
        return false; // No intersection
    }

    float discriminantSqrt = std::sqrt(discriminant);
    float t0 = (-B - discriminantSqrt) / (2.0f * A);
    float t1 = (-B + discriminantSqrt) / (2.0f * A);

    // Intersection(s) behind the ray origin?
    if (t0 >= 0.0f && t0 <= rayLength) {
        hitT = t0 / rayLength;
    } else if (t1 >= 0.0f && t1 <= rayLength) {
        hitT = t1 / rayLength;
    } else {
        return false;
    }

    return true;
}


struct LineElementIdData {
    float entranceIdx;
    float exitIdx;
    int lineId;
    int padding;
};


void MBezierTrajectories::updateTimeStepSphereRenderDataIfNecessary(
        int timeStep, float sphereRadius,
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
{
    if (timeStep == lastTimeStep && sphereRadius == lastSphereRadius) {
        return;
    }
    lastTimeStep = timeStep;
    lastSphereRadius = sphereRadius;

    if (timeStepSphereRenderData.entrancePointsBuffer) {
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.spherePositionsBuffer);
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.entrancePointsBuffer);
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.exitPointsBuffer);
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.lineElementIdsBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(timeStepSphereRenderData.spherePositionsBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(timeStepSphereRenderData.entrancePointsBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(timeStepSphereRenderData.exitPointsBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(timeStepSphereRenderData.lineElementIdsBuffer);
    }

    QVector<QVector4D> spherePositions;
    QVector<QVector4D> entrancePoints;
    QVector<QVector4D> exitPoints;
    QVector<LineElementIdData> lineElementIds;

    int trajectoryIndex = 0;
    for (MFilteredTrajectory& trajectory : baseTrajectories) {
        int timeStepClamped = clamp(timeStep, 0, trajectory.positions.size() - 1);
        const QVector3D& sphereCenter = trajectory.positions.at(timeStepClamped);
        spherePositions.push_back(sphereCenter);

        float entranceIdx = 0.0f;
        bool foundEntrancePoint = false;
        for (int i = timeStepClamped; i > 0; i--) {
            const QVector3D& p0 = trajectory.positions.at(i - 1);
            const QVector3D& p1 = trajectory.positions.at(i);
            float hitT;
            if (lineSegmentSphereIntersection(p0, p1, sphereCenter, sphereRadius, hitT)) {
                entrancePoints.push_back((1.0f - hitT) * p0 + hitT * p1);
                entranceIdx = float(i - 1) + hitT;
                foundEntrancePoint = true;
                break;
            }
        }
        if (!foundEntrancePoint) {
            entrancePoints.push_back(trajectory.positions.at(0));
        }

        float exitIdx = float(trajectory.positions.size() - 1);
        bool foundExitPoint = false;
        for (int i = timeStepClamped; i < trajectory.positions.size() - 1; i++) {
            const QVector3D& p0 = trajectory.positions.at(i);
            const QVector3D& p1 = trajectory.positions.at(i + 1);
            float hitT;
            if (lineSegmentSphereIntersection(p0, p1, sphereCenter, sphereRadius, hitT)) {
                exitPoints.push_back((1.0f - hitT) * p0 + hitT * p1);
                exitIdx = float(i) + hitT;
                foundExitPoint = true;
                break;
            }
        }
        if (!foundExitPoint) {
            exitPoints.push_back(trajectory.positions.at(trajectory.positions.size() - 1));
        }

        LineElementIdData lineElementIdData{};
        lineElementIdData.entranceIdx = entranceIdx;
        lineElementIdData.exitIdx = exitIdx;
        lineElementIdData.lineId = trajectoryIndex;
        lineElementIdData.padding = 0;
        lineElementIds.push_back(lineElementIdData);
        trajectoryIndex++;
    }

    timeStepSphereRenderData.numSpheres = spherePositions.size();

    timeStepSphereRenderData.spherePositionsBuffer = createShaderStorageBuffer(
            currentGLContext, timeStepSpherePositionsBufferID, spherePositions);
    timeStepSphereRenderData.entrancePointsBuffer = createShaderStorageBuffer(
            currentGLContext, timeStepSphereEntrancePointsBufferID, entrancePoints);
    timeStepSphereRenderData.exitPointsBuffer = createShaderStorageBuffer(
            currentGLContext, timeStepSphereExitPointsBufferID, exitPoints);
    timeStepSphereRenderData.lineElementIdsBuffer = createShaderStorageBuffer(
            currentGLContext, timeStepSphereLineElementIdsBufferID, lineElementIds);
}

void MBezierTrajectories::releaseTimeStepSphereRenderData()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.indexBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.vertexPositionBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.vertexNormalBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.spherePositionsBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.entrancePointsBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.exitPointsBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepSphereRenderData.lineElementIdsBuffer);
}


inline int iceil(int x, int y) { return (x - 1) / y + 1; }

void MBezierTrajectories::updateTrajectorySelection(
        const GLint* startIndices, const GLsizei* indexCount,
        int numTimeStepsPerTrajectory, int numSelectedTrajectories)
{
    if (!isDirty) {
        return;
    }

    useFiltering = false;
    int filteredTrajectoryIdx = 0;
    for (int trajectorySelectionIdx = 0; trajectorySelectionIdx < numSelectedTrajectories; trajectorySelectionIdx++)
    {
        GLint startSelection = startIndices[trajectorySelectionIdx];
        GLsizei countSelection = indexCount[trajectorySelectionIdx];
        int offsetSelection = startSelection % numTimeStepsPerTrajectory;
        //int trajectoryIdx = iceil(startSelection, numTimeStepsPerTrajectory);
        int trajectoryIdx = startSelection / numTimeStepsPerTrajectory;
        int bezierTrajectoryIdx = trajIndicesToFilteredIndicesMap[trajectoryIdx];
        if (bezierTrajectoryIdx == -1 || offsetSelection >= countSelection) {
            continue;
        }

        uint32_t trajectoryIndexOffset = trajectoryIndexOffsets.at(bezierTrajectoryIdx);
        uint32_t numTrajectoryIndices = numIndicesPerTrajectory.at(bezierTrajectoryIdx);
        if (numTrajectoryIndices == 0) {
            continue;
        }

        //MBezierTrajectory& bezierTrajectory = bezierTrajectories[trajectoryIdx];

        GLsizei selectionCount;
        if (countSelection == numTimeStepsPerTrajectory) {
            selectionCount = numTrajectoryIndices;
        } else {
            useFiltering = true;
            selectionCount = 2 * (numTrajectoryIndices / 2 * countSelection / numTimeStepsPerTrajectory);
        }

        ptrdiff_t selectionIndex;
        if (offsetSelection == 0) {
            selectionIndex = 0;
        } else {
            useFiltering = true;
            selectionIndex = 2 * (numTrajectoryIndices / 2 * offsetSelection / numTimeStepsPerTrajectory);
        }
        selectionIndex += trajectoryIndexOffset;
        selectionIndex = selectionIndex * sizeof(uint32_t);

        trajectorySelectionCount[filteredTrajectoryIdx] = selectionCount;
        trajectorySelectionIndices[filteredTrajectoryIdx] = selectionIndex;
        filteredTrajectoryIdx++;
    }

    numFilteredTrajectories = filteredTrajectoryIdx;
    if (numFilteredTrajectories != numTrajectories) {
        useFiltering = true;
    }
}

bool MBezierTrajectories::getUseFiltering()
{
    return useFiltering;
}

int MBezierTrajectories::getNumFilteredTrajectories()
{
    return numFilteredTrajectories;
}

GLsizei* MBezierTrajectories::getTrajectorySelectionCount()
{
    return trajectorySelectionCount;
}

const void* const* MBezierTrajectories::getTrajectorySelectionIndices()
{
    return reinterpret_cast<const void* const*>(trajectorySelectionIndices);
}

bool MBezierTrajectories::getFilteredTrajectories(
        const GLint* startIndices, const GLsizei* indexCount,
        int numTimeStepsPerTrajectory, int numSelectedTrajectories,
        QVector<QVector<QVector3D>>& trajectories,
        QVector<uint32_t>& selectedTrajectoryIndices)
{
    if (!isDirty) {
        return false;
    }

    for (int trajectorySelectionIdx = 0; trajectorySelectionIdx < numSelectedTrajectories; trajectorySelectionIdx++)
    {
        GLint startSelection = startIndices[trajectorySelectionIdx];
        GLsizei countSelection = indexCount[trajectorySelectionIdx];
        int offsetSelection = startSelection % numTimeStepsPerTrajectory;
        int trajectoryIdx = startSelection / numTimeStepsPerTrajectory;
        int bezierTrajectoryIdx = trajIndicesToFilteredIndicesMap[trajectoryIdx];
        if (bezierTrajectoryIdx == -1 || offsetSelection >= countSelection)
        {
            continue;
        }

        MBezierTrajectory& bezierTrajectory = bezierTrajectories[trajectoryIdx];

        int numTrajectoryPoints = bezierTrajectory.positions.size();
        if (numTrajectoryPoints <= 1)
        {
            continue;
        }

        GLsizei selectionCount;
        if (countSelection == numTimeStepsPerTrajectory) {
            selectionCount = numTrajectoryPoints;
        } else {
            selectionCount = numTrajectoryPoints * countSelection / numTimeStepsPerTrajectory;
        }

        int selectionIndex;
        if (offsetSelection == 0) {
            selectionIndex = 0;
        } else {
            selectionIndex = numTrajectoryPoints * offsetSelection / numTimeStepsPerTrajectory;
        }

        QVector<QVector3D> trajectory;
        for (int i = selectionIndex; i < selectionCount; i++)
        {
            trajectory.push_back(bezierTrajectory.positions.at(i));
        }

        trajectories.push_back(trajectory);
        selectedTrajectoryIndices.push_back(uint32_t(trajectoryIdx));
    }

    return true;
}


}
