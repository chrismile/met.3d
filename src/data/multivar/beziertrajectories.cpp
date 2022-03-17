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
            + sizeof(uint32_t) // lineID
            + elementIDs.size() * sizeof(int32_t)
            + multiVarData.size() * sizeof(float)
            + sizeof(LineDesc)
            + multiVarDescs.size() * sizeof(VarDesc);
    return static_cast<unsigned int>(sizeBytes / 1024ull);
}


MBezierTrajectories::MBezierTrajectories(
        MDataRequest requestToReferTo, const MFilteredTrajectories& filteredTrajectories,
        const QVector<int>& trajIndicesToFilteredIndicesMap,
        unsigned int numVariables, const QStringList& auxDataVarNames)
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
    for (int i = 0; i < numTrajectories; i++)
    {
        selectedLines.push_back(true);
    }
    for (int i = 0; i < numTrajectories; i++)
    {
        ascentTimeStepIndices.push_back(0);
    }

    /*
     * TODO: This is hard-coded, as there is currently no way to know which the target variable is.
     */
    for (unsigned int i = 0; i < numVariables; i++)
    {
        targetVariableAndSensitivityIndexArray.push_back(false);
    }
    int targetVariableIndex = auxDataVarNames.indexOf("QR");
    if (targetVariableIndex < 0) {
        targetVariableIndex = std::max(1, int(numVariables) - 1);
    } else {
        targetVariableIndex = targetVariableIndex + 1;
    }
    targetVariableAndSensitivityIndexArray[targetVariableIndex] = true;
    targetVariableAndSensitivityIndexArray[int(numVariables) - 1] = true;

    this->numTrajectories = static_cast<int>(numTrajectories);
    trajectorySelectionCount = new GLsizei[numTrajectories];
    trajectorySelectionIndices = new ptrdiff_t[numTrajectories];

    minMaxAttributes.clear();
    minMaxAttributes.resize(int(numVariables));
    for (size_t i = 0; i < numVariables; i++)
    {
        minMaxAttributes[int(i)] = QVector2D(
                std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest());
    }
    for (const MFilteredTrajectory& trajectory : filteredTrajectories)
    {
        for (size_t i = 0; i < numVariables; i++)
        {
            const QVector<float>& attributes = trajectory.attributes.at(int(i));
            QVector2D& minMaxVector = minMaxAttributes[int(i)];
            for (float v : attributes)
            {
                if (std::isnan(v))
                {
                    continue;
                }
                minMaxVector.setX(std::min(minMaxVector.x(), v));
                minMaxVector.setY(std::max(minMaxVector.y(), v));
            }
        }
    }
    for (size_t i = 0; i < numVariables; i++)
    {
        QVector2D& minMax = minMaxAttributes[int(i)];
        if (std::isinf(minMax[1])) {
            minMax[1] = std::numeric_limits<float>::max();
        }
    }
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


void createLineTubesRenderDataCPU(
        const QVector<QVector<QVector3D>>& lineCentersList,
        const QVector<QVector<int>>& lineLineIDList,
        const QVector<QVector<int>>& lineElementIDList,
        QVector<uint32_t>& lineIndexOffsets,
        QVector<uint32_t>& numIndicesPerLine,
        QVector<uint32_t>& lineIndices,
        QVector<QVector3D>& vertexPositions,
        QVector<QVector3D>& vertexNormals,
        QVector<QVector3D>& vertexTangents,
        QVector<int>& vertexLineIDs,
        QVector<int>& vertexElementIDs)
{
    assert(lineCentersList.size() == lineLineIDList.size());
    assert(lineCentersList.size() == lineElementIDList.size());
    lineIndexOffsets.reserve(lineCentersList.size());
    numIndicesPerLine.reserve(lineCentersList.size());
    for (int lineId = 0; lineId < lineCentersList.size(); lineId++)
    {
        const QVector<QVector3D> &lineCenters = lineCentersList.at(lineId);
        const QVector<int> &lineLineIDs = lineLineIDList.at(lineId);
        const QVector<int> &lineElementIDs = lineElementIDList.at(lineId);
        assert(lineCenters.size() == lineLineIDs.size());
        assert(lineCenters.size() == lineElementIDs.size());
        size_t n = lineCenters.size();
        size_t indexOffset = vertexPositions.size();
        lineIndexOffsets.push_back(lineIndices.size());

        if (n < 2)
        {
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
            vertexLineIDs.push_back(lineLineIDs.at(i));
            vertexElementIDs.push_back(lineElementIDs.at(i));
            numValidLinePoints++;
        }

        if (numValidLinePoints == 1)
        {
            // Only one vertex left -> Output nothing (tube consisting only of one point).
            vertexPositions.pop_back();
            vertexNormals.pop_back();
            vertexTangents.pop_back();
            vertexLineIDs.pop_back();
            vertexElementIDs.pop_back();
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
    QVector<QVector<int>> lineLineIDList;
    QVector<QVector<int>> lineElementIDList;
    QVector<uint32_t> lineIndices;
    QVector<QVector3D> vertexPositions;
    QVector<QVector3D> vertexNormals;
    QVector<QVector3D> vertexTangents;
    QVector<int> vertexLineIDs;
    QVector<int> vertexElementIDs;

    lineCentersList.resize(bezierTrajectories.size());
    lineLineIDList.resize(bezierTrajectories.size());
    lineElementIDList.resize(bezierTrajectories.size());
    for (int trajectoryIdx = 0; trajectoryIdx < bezierTrajectories.size(); trajectoryIdx++)
    {
        const MBezierTrajectory& trajectory = bezierTrajectories.at(trajectoryIdx);
        QVector<QVector3D>& lineCenters = lineCentersList[trajectoryIdx];
        QVector<int>& lineLineIDs = lineLineIDList[trajectoryIdx];
        QVector<int>& lineElementIDs = lineElementIDList[trajectoryIdx];

        for (int i = 0; i < trajectory.positions.size(); i++)
        {
            const QVector3D& position = trajectory.positions.at(i);
            // Skip NaN values.
            if (!std::isnan(position.x()) && !std::isnan(position.y()) && !std::isnan(position.z()))
            {
                lineCenters.push_back(position);
                lineLineIDs.push_back(trajectory.lineID);
                lineElementIDs.push_back(trajectory.elementIDs.at(i));
            }
        }
    }

    trajectoryIndexOffsets.clear();
    numIndicesPerTrajectory.clear();
    createLineTubesRenderDataCPU(
            lineCentersList, lineLineIDList, lineElementIDList,
            trajectoryIndexOffsets, numIndicesPerTrajectory,
            lineIndices, vertexPositions, vertexNormals, vertexTangents, vertexLineIDs, vertexElementIDs);


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
    bezierTrajectoriesRenderData.vertexLineIDBuffer = createVertexBuffer(
            currentGLContext, vertexLineIDBufferID, vertexLineIDs);
    bezierTrajectoriesRenderData.vertexElementIDBuffer = createVertexBuffer(
            currentGLContext, vertexElementIDBufferID, vertexElementIDs);

    this->lineIndicesCache = lineIndices;
    this->vertexPositionsCache = vertexPositions;


    // ------------------------------------------ Create SSBOs. ------------------------------------------
    size_t numLines = bezierTrajectories.size();
    QVector<float> varData;
    QVector<float> lineDescData(numLines);
    QVector<QVector4D> varDescData;
    QVector<QVector2D> lineVarDescData;

    for (const MBezierTrajectory& bezierTrajectory : bezierTrajectories)
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
        const MBezierTrajectory& bezierTrajectory = bezierTrajectories[lineIdx];
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
            const MBezierTrajectory& bezierTrajectory = bezierTrajectories[lineIdx];

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
    bezierTrajectoriesRenderData.varSelectedTargetVariableAndSensitivityArrayBuffer = createShaderStorageBuffer(
            currentGLContext, varSelectedTargetVariableAndSensitivityArrayBufferID,
            targetVariableAndSensitivityIndexArray);
    bezierTrajectoriesRenderData.varDivergingArrayBuffer = createShaderStorageBuffer(
            currentGLContext, varDivergingArrayBufferID, varDiverging);
    bezierTrajectoriesRenderData.lineSelectedArrayBuffer = createShaderStorageBuffer(
            currentGLContext, lineSelectedArrayBufferID, selectedLines);

    this->bezierTrajectoriesRenderData = bezierTrajectoriesRenderData;

    return bezierTrajectoriesRenderData;
}


void MBezierTrajectories::releaseRenderData()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(indexBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(vertexNormalBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(vertexTangentBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(vertexLineIDBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(vertexElementIDBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(variableArrayBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(lineDescArrayBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(varDescArrayBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(lineVarDescArrayBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(varSelectedArrayBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(varSelectedTargetVariableAndSensitivityArrayBufferID);
    MGLResourcesManager::getInstance()->releaseGPUItem(varDivergingArrayBufferID);
}


void MBezierTrajectories::updateSelectedVariables(const QVector<uint32_t>& varSelected)
{
    this->varSelected = varSelected;
    if (bezierTrajectoriesRenderData.varSelectedArrayBuffer)
    {
        bezierTrajectoriesRenderData.varSelectedArrayBuffer->upload(
                varSelected.constData(), GL_STATIC_DRAW);
    }
}


void MBezierTrajectories::updateDivergingVariables(const QVector<uint32_t>& varDiverging)
{
    this->varDiverging = varDiverging;
    if (bezierTrajectoriesRenderData.varDivergingArrayBuffer)
    {
        bezierTrajectoriesRenderData.varDivergingArrayBuffer->upload(
                this->varDiverging.constData(), GL_STATIC_DRAW);
    }
}


void MBezierTrajectories::updateSelectedLines(const QVector<uint32_t>& selectedLines)
{
    if (selectedLines.empty())
    {
        // Data might not be available immediately at the first rendering pass.
        for (uint32_t& entry : this->selectedLines)
        {
            entry = true;
        }
    }
    else
    {
        this->selectedLines = selectedLines;
    }

    if (bezierTrajectoriesRenderData.lineSelectedArrayBuffer)
    {
        bezierTrajectoriesRenderData.lineSelectedArrayBuffer->upload(
                this->selectedLines.constData(), GL_STATIC_DRAW);
    }
}


void MBezierTrajectories::setSyncMode(TrajectorySyncMode syncMode)
{
    trajectorySyncMode = syncMode;
}


void MBezierTrajectories::updateLineAscentTimeStepArrayBuffer(
        const QVector<int>& _ascentTimeStepIndices, int _maxAscentTimeStepIndex)
{
    ascentTimeStepIndices = _ascentTimeStepIndices;
    maxAscentTimeStepIndex = _maxAscentTimeStepIndex;
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
 * For more details see: https://education.siggraph.org/static/HyperGraph/raytrace/rtinter1.htm
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

bool halfLineSphereIntersection(
        const QVector3D& p0, const QVector3D& p1, const QVector3D& sphereCenter, float sphereRadius,
        bool isLeftOpen, float& hitT) {
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
    if (isLeftOpen) {
        if (t0 <= rayLength) {
            hitT = t0 / rayLength;
        } else if (t1 <= rayLength) {
            hitT = t1 / rayLength;
        } else {
            return false;
        }
    } else {
        if (t0 >= 0.0f) {
            hitT = t0 / rayLength;
        } else if (t1 >= 0.0f) {
            hitT = t1 / rayLength;
        } else {
            return false;
        }
    }

    return true;
}

struct LineElementIdData {
    float centerIdx;
    float entranceIdx;
    float exitIdx;
    int lineId;
};


void MBezierTrajectories::updateTimeStepSphereRenderDataIfNecessary(
        int timeStep, uint32_t syncModeTrajectoryIndex, float sphereRadius,
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
{
    if (timeStep == lastSphereTimeStep && syncModeTrajectoryIndex == lastSphereSyncModeTrajectoryIndex
            && sphereRadius == lastSphereRadius) {
        return;
    }
    lastSphereTimeStep = timeStep;
    lastSphereSyncModeTrajectoryIndex = syncModeTrajectoryIndex;
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

    const MFilteredTrajectory& syncModeTrajectory = baseTrajectories[int(syncModeTrajectoryIndex)];

    int trajectoryIndex = 0;
    for (MFilteredTrajectory& trajectory : baseTrajectories) {
        int timeStepLocal;
        if (trajectorySyncMode == TrajectorySyncMode::TIME_OF_ASCENT)
        {
            timeStepLocal = timeStep - maxAscentTimeStepIndex + ascentTimeStepIndices.at(trajectoryIndex);
        }
        else if (trajectorySyncMode == TrajectorySyncMode::HEIGHT)
        {
            int timeStepGlobal =
                    timeStep
                    - ascentTimeStepIndices.at(int(syncModeTrajectoryIndex))
                    + ascentTimeStepIndices.at(trajectoryIndex);
            int timeStepSyncModeTrajectoryClamped = clamp(timeStep, 0, syncModeTrajectory.positions.size() - 1);
            int timeStepClamped = clamp(timeStepGlobal, 0, trajectory.positions.size() - 1);
            float height = syncModeTrajectory.positions.at(timeStepSyncModeTrajectoryClamped).z();
            float heightStart = trajectory.positions.at(timeStepClamped).z();
            float heightNext = heightStart;
            float heightLast;

            int timeStepLeft = timeStepClamped;
            int distanceLeft = std::numeric_limits<int>::max();
            for (int i = timeStepClamped - 1; i > 0; i--) {
                heightLast = heightNext;
                heightNext = trajectory.positions.at(i).z();
                float heightMin, heightMax;
                int timeMin, timeMax;
                if (heightLast < heightNext) {
                    heightMin = heightLast;
                    heightMax = heightNext;
                    timeMin = i + 1;
                    timeMax = i;
                } else {
                    heightMin = heightNext;
                    heightMax = heightLast;
                    timeMin = i;
                    timeMax = i + 1;
                }
                if (heightMin <= height && heightMax >= height) {
                    timeStepLeft = (height - heightMin) / (heightMax - heightMin) <= 0.5f ? timeMin : timeMax;
                    distanceLeft = timeStepClamped - i;
                    break;
                }
            }

            heightNext = heightStart;
            int timeStepRight = timeStepClamped;
            int distanceRight = std::numeric_limits<int>::max();
            for (int i = timeStepClamped + 1; i < trajectory.positions.size(); i++) {
                heightLast = heightNext;
                heightNext = trajectory.positions.at(i).z();
                float heightMin, heightMax;
                int timeMin, timeMax;
                if (heightLast < heightNext) {
                    heightMin = heightLast;
                    heightMax = heightNext;
                    timeMin = i - 1;
                    timeMax = i;
                } else {
                    heightMin = heightNext;
                    heightMax = heightLast;
                    timeMin = i;
                    timeMax = i - 1;
                }
                if (heightMin <= height && heightMax >= height) {
                    timeStepRight = (height - heightMin) / (heightMax - heightMin) <= 0.5f ? timeMin : timeMax;
                    distanceRight = i - timeStepClamped;
                    break;
                }
            }

            if (distanceLeft < distanceRight) {
                timeStepLocal = timeStepLeft;
            } else {
                timeStepLocal = timeStepRight;
            }
        }
        else
        {
            timeStepLocal = timeStep;
        }
        int timeStepClamped = clamp(timeStepLocal, 0, trajectory.positions.size() - 1);
        float centerIdx = float(timeStepClamped);
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
            if (trajectory.positions.size() == 1) {
                entrancePoints.push_back(trajectory.positions.at(0));
            } else {
                const QVector3D& p0 = trajectory.positions.at(0);
                const QVector3D& p1 = trajectory.positions.at(1);
                float hitT;
                if (halfLineSphereIntersection(p0, p1, sphereCenter, sphereRadius, true, hitT)) {
                    entrancePoints.push_back((1.0f - hitT) * p0 + hitT * p1);
                } else {
                    entrancePoints.push_back(trajectory.positions.at(0));
                }
            }
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
            if (trajectory.positions.size() == 1) {
                exitPoints.push_back(trajectory.positions.at(trajectory.positions.size() - 1));
            } else {
                const QVector3D& p0 = trajectory.positions.at(trajectory.positions.size() - 2);
                const QVector3D& p1 = trajectory.positions.at(trajectory.positions.size() - 1);
                float hitT;
                if (halfLineSphereIntersection(p0, p1, sphereCenter, sphereRadius, false, hitT)) {
                    exitPoints.push_back((1.0f - hitT) * p0 + hitT * p1);
                } else {
                    exitPoints.push_back(trajectory.positions.at(trajectory.positions.size() - 1));
                }
            }
        }

        LineElementIdData lineElementIdData{};
        lineElementIdData.centerIdx = centerIdx;
        lineElementIdData.entranceIdx = entranceIdx;
        lineElementIdData.exitIdx = exitIdx;
        lineElementIdData.lineId = trajectoryIndex;
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


MTimeStepRollsRenderData* MBezierTrajectories::getTimeStepRollsRenderData(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
QGLWidget *currentGLContext)
#endif
{
    return &timeStepRollsRenderData;
}


void MBezierTrajectories::updateTimeStepRollsRenderDataIfNecessary(
        int timeStep, uint32_t syncModeTrajectoryIndex, float tubeRadius,
        float rollsRadius, float rollsWidth, bool mapRollsThickness, int numLineSegments,
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
QGLWidget *currentGLContext)
#endif
{
    if (timeStep == lastRollsTimeStep && syncModeTrajectoryIndex == lastRollsSyncModeTrajectoryIndex
            && (!mapRollsThickness || tubeRadius == lastTubeRadius)
            && rollsRadius == lastRollsRadius && rollsWidth == lastRollsWidth && lastVarSelectedRolls == varSelected
            && mapRollsThickness == lastMapRollsThickness && lastNumLineSegmentsRolls == numLineSegments) {
        return;
    }
    lastRollsTimeStep = timeStep;
    lastRollsSyncModeTrajectoryIndex = syncModeTrajectoryIndex;
    lastTubeRadius = tubeRadius;
    lastRollsRadius = rollsRadius;
    lastRollsWidth = rollsWidth;
    lastVarSelectedRolls = varSelected;
    lastMapRollsThickness = mapRollsThickness;
    lastNumLineSegmentsRolls = numLineSegments;

    QVector<int> selectedVarIndices;
    int numVarsSelected = 0;
    for (int i = 0; i < varSelected.size(); i++)
    {
        if (varSelected.at(i)) {
            numVarsSelected++;
            selectedVarIndices.push_back(i);
        }
    }

    if (timeStepRollsRenderData.indexBuffer) {
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.indexBuffer);
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexPositionBuffer);
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexNormalBuffer);
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexTangentBuffer);
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexRollPositionBuffer);
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexLineIdBuffer);
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexLinePointIdxBuffer);
        MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexVariableIdAndIsCapBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(
                timeStepRollsRenderData.indexBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(
                timeStepRollsRenderData.vertexPositionBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(
                timeStepRollsRenderData.vertexNormalBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(
                timeStepRollsRenderData.vertexTangentBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(
                timeStepRollsRenderData.vertexRollPositionBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(
                timeStepRollsRenderData.vertexLineIdBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(
                timeStepRollsRenderData.vertexLinePointIdxBuffer);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(
                timeStepRollsRenderData.vertexVariableIdAndIsCapBuffer);
        timeStepRollsRenderData.indexBuffer = nullptr;
        timeStepRollsRenderData.vertexPositionBuffer = nullptr;
        timeStepRollsRenderData.vertexNormalBuffer = nullptr;
        timeStepRollsRenderData.vertexTangentBuffer = nullptr;
        timeStepRollsRenderData.vertexRollPositionBuffer = nullptr;
        timeStepRollsRenderData.vertexLineIdBuffer = nullptr;
        timeStepRollsRenderData.vertexLinePointIdxBuffer = nullptr;
        timeStepRollsRenderData.vertexVariableIdAndIsCapBuffer = nullptr;
    }

    if (numVarsSelected <= 0) {
        return;
    }

    QVector<uint32_t> triangleIndices;
    QVector<QVector3D> vertexPositions;
    QVector<QVector3D> vertexNormals;
    QVector<QVector3D> vertexTangents;
    QVector<float> vertexRollPositions;
    QVector<int32_t> vertexLineIds;
    QVector<float> vertexLinePointIndices;
    QVector<uint32_t> vertexVariableIdAndIsCapList;
    QVector<QVector3D> lineNormals;

    float radius = rollsRadius;
    const float PI = 3.1415926535897932f;
    const float TWO_PI = PI * 2.0f;
    const int numCircleSubdivisions = numLineSegments;
    std::vector<QVector3D> globalCircleVertexPositions;
    const float theta = TWO_PI / numCircleSubdivisions;
    const float tangentialFactor = std::tan(theta); // opposite / adjacent
    const float radialFactor = std::cos(theta); // adjacent / hypotenuse
    QVector3D position(radius, 0, 0);
    for (int i = 0; i < numCircleSubdivisions; i++) {
        globalCircleVertexPositions.push_back(position);
        // Add the tangent vector and correct the position using the radial factor.
        QVector3D tangent(-position.y(), position.x(), 0);
        position += tangentialFactor * tangent;
        position *= radialFactor;
    }

    // Scale with ratio of radius of circumcircle and incircle to make sure the rolls don't intersect with the tubes.
    float radiusFactor = 1.0f / std::cos(PI / float(numLineSegments));

    const MFilteredTrajectory& syncModeTrajectory = baseTrajectories[int(syncModeTrajectoryIndex)];

    int trajectoryIndex = 0;
    for (MFilteredTrajectory& trajectory : baseTrajectories) {
        int timeStepLocal;
        if (trajectorySyncMode == TrajectorySyncMode::TIME_OF_ASCENT)
        {
            timeStepLocal = timeStep - maxAscentTimeStepIndex + ascentTimeStepIndices.at(trajectoryIndex);
        }
        else if (trajectorySyncMode == TrajectorySyncMode::HEIGHT)
        {
            int timeStepGlobal =
                    timeStep
                    - ascentTimeStepIndices.at(int(syncModeTrajectoryIndex))
                    + ascentTimeStepIndices.at(trajectoryIndex);
            int timeStepSyncModeTrajectoryClamped = clamp(timeStep, 0, syncModeTrajectory.positions.size() - 1);
            int timeStepClamped = clamp(timeStepGlobal, 0, trajectory.positions.size() - 1);
            float height = syncModeTrajectory.positions.at(timeStepSyncModeTrajectoryClamped).z();
            float heightStart = trajectory.positions.at(timeStepClamped).z();
            float heightNext = heightStart;
            float heightLast;

            int timeStepLeft = timeStepClamped;
            int distanceLeft = std::numeric_limits<int>::max();
            for (int i = timeStepClamped - 1; i > 0; i--) {
                heightLast = heightNext;
                heightNext = trajectory.positions.at(i).z();
                float heightMin, heightMax;
                int timeMin, timeMax;
                if (heightLast < heightNext) {
                    heightMin = heightLast;
                    heightMax = heightNext;
                    timeMin = i + 1;
                    timeMax = i;
                } else {
                    heightMin = heightNext;
                    heightMax = heightLast;
                    timeMin = i;
                    timeMax = i + 1;
                }
                if (heightMin <= height && heightMax >= height) {
                    timeStepLeft = (height - heightMin) / (heightMax - heightMin) <= 0.5f ? timeMin : timeMax;
                    distanceLeft = timeStepClamped - i;
                    break;
                }
            }

            heightNext = heightStart;
            int timeStepRight = timeStepClamped;
            int distanceRight = std::numeric_limits<int>::max();
            for (int i = timeStepClamped + 1; i < trajectory.positions.size(); i++) {
                heightLast = heightNext;
                heightNext = trajectory.positions.at(i).z();
                float heightMin, heightMax;
                int timeMin, timeMax;
                if (heightLast < heightNext) {
                    heightMin = heightLast;
                    heightMax = heightNext;
                    timeMin = i - 1;
                    timeMax = i;
                } else {
                    heightMin = heightNext;
                    heightMax = heightLast;
                    timeMin = i;
                    timeMax = i - 1;
                }
                if (heightMin <= height && heightMax >= height) {
                    timeStepRight = (height - heightMin) / (heightMax - heightMin) <= 0.5f ? timeMin : timeMax;
                    distanceRight = i - timeStepClamped;
                    break;
                }
            }

            if (distanceLeft < distanceRight) {
                timeStepLocal = timeStepLeft;
            } else {
                timeStepLocal = timeStepRight;
            }
        }
        else
        {
            timeStepLocal = timeStep;
        }
        int timeStepClamped = clamp(timeStepLocal, 0, trajectory.positions.size() - 1);
        float centerIdx = float(timeStepClamped);

        const QVector<QVector3D>& lineCenters = trajectory.positions;
        int n = lineCenters.size();
        // Assert that we have a valid input data range
        if (n < 2) {
            continue;
        }

        bool isFirstVarHalf = numVarsSelected % 2 == 1;
        QVector<int> startTimeSteps;
        QVector<int> stopTimeSteps;
        startTimeSteps.resize(numVarsSelected);
        stopTimeSteps.resize(numVarsSelected);

        if (!isFirstVarHalf) {
            startTimeSteps[numVarsSelected / 2] = timeStepClamped;
            stopTimeSteps[(numVarsSelected - 1) / 2] = timeStepClamped;
        }

        // Backward.
        float accumulatedLength = 0.0f;
        float minAccumulatedLength = isFirstVarHalf ? rollsWidth / 2.0f : rollsWidth;
        int currentVar = (numVarsSelected - 1) / 2;
        for (int i = timeStepClamped - 1; i >= 0; i--) {
            QVector3D tangent = lineCenters[i + 1] - lineCenters[i];
            float lineSegmentLength = tangent.length();
            accumulatedLength += lineSegmentLength;
            if (accumulatedLength >= minAccumulatedLength) {
                startTimeSteps[currentVar] = i;
                currentVar--;
                if (currentVar < 0) {
                    break;
                }
                stopTimeSteps[currentVar] = i;
                minAccumulatedLength = rollsWidth;
                accumulatedLength = 0.0f;
            }
        }

        // Forward.
        accumulatedLength = 0.0f;
        minAccumulatedLength = isFirstVarHalf ? rollsWidth / 2.0f : rollsWidth;
        currentVar = numVarsSelected / 2;
        for (int i = timeStepClamped + 1; i < n; i++) {
            QVector3D tangent = lineCenters[i] - lineCenters[i - 1];
            float lineSegmentLength = tangent.length();
            accumulatedLength += lineSegmentLength;
            if (accumulatedLength >= minAccumulatedLength) {
                stopTimeSteps[currentVar] = i;
                currentVar++;
                if (currentVar >= numVarsSelected) {
                    break;
                }
                startTimeSteps[currentVar] = i;
                minAccumulatedLength = rollsWidth;
                accumulatedLength = 0.0f;
            }
        }

        for (int variableId = 0; variableId < numVarsSelected; variableId++)
        {
            if (mapRollsThickness)
            {
                globalCircleVertexPositions.clear();
                float centerAttrValue =
                        trajectory.attributes.at(selectedVarIndices.at(variableId)).at(timeStepClamped);
                QVector2D minMax = minMaxAttributes.at(selectedVarIndices.at(variableId));
                float innerRadius = std::min(tubeRadius * radiusFactor, rollsRadius);
                float t = (centerAttrValue - minMax.x()) / (minMax.y() - minMax.x());
                radius = (1.0f - t) * innerRadius + t * rollsRadius;
                position = QVector3D(radius, 0, 0);
                for (int i = 0; i < numCircleSubdivisions; i++) {
                    globalCircleVertexPositions.push_back(position);
                    // Add the tangent vector and correct the position using the radial factor.
                    QVector3D tangent(-position.y(), position.x(), 0);
                    position += tangentialFactor * tangent;
                    position *= radialFactor;
                }
            }

            uint32_t indexOffset = uint32_t(vertexPositions.size());
            uint32_t lineIndexOffset = uint32_t(lineNormals.size());

            /*int rollTimeStepRadius = 64;
            int timeStepOffset = -int(float(2 * rollTimeStepRadius * numVarsSelected) / 2.0f);
            if (timeStepClamped + timeStepOffset < 0) {
                timeStepOffset = -timeStepClamped;
            }
            if (timeStepClamped + timeStepOffset > n - 1) {
                timeStepOffset = n - 1 - timeStepClamped;
            }
            int timeStepStart = clamp(
                    timeStepClamped - rollTimeStepRadius + rollTimeStepRadius * 2 * variableId + timeStepOffset,
                    0, n - 1);
            int timeStepStop = clamp(
                    timeStepClamped + rollTimeStepRadius + rollTimeStepRadius * 2 * variableId + timeStepOffset,
                    0, n - 1);*/

            int timeStepStart = startTimeSteps[variableId];
            int timeStepStop = stopTimeSteps[variableId];

            QVector3D lastLineNormal(1.0f, 0.0f, 0.0f);
            int numValidLinePoints = 0;
            for (int i = timeStepStart; i <= timeStepStop; i++) {
                QVector3D tangent;
                if (i == 0) {
                    tangent = lineCenters[i + 1] - lineCenters[i];
                } else if (i == n - 1) {
                    tangent = lineCenters[i] - lineCenters[i - 1];
                } else {
                    tangent = (lineCenters[i + 1] - lineCenters[i - 1]);
                }
                float lineSegmentLength = tangent.length();

                if (lineSegmentLength < 0.0001f) {
                    // In case the two vertices are almost identical, just skip this path line segment
                    continue;
                }
                tangent.normalize();

                const QVector3D& center = lineCenters.at(i);
                QVector3D helperAxis = lastLineNormal;
                if (QVector3D::crossProduct(helperAxis, tangent).length() < 0.01f) {
                    // If tangent == lastNormal
                    helperAxis = QVector3D(0.0f, 1.0f, 0.0f);
                    if (QVector3D::crossProduct(helperAxis, tangent).length() < 0.01f) {
                        // If tangent == helperAxis
                        helperAxis = QVector3D(0.0f, 0.0f, 1.0f);
                    }
                }
                QVector3D normal = (helperAxis - QVector3D::dotProduct(helperAxis, tangent) * tangent).normalized(); // Gram-Schmidt
                lastLineNormal = normal;
                QVector3D binormal = QVector3D::crossProduct(tangent, normal);
                lineNormals.push_back(normal);

                for (size_t j = 0; j < globalCircleVertexPositions.size(); j++) {
                    QVector3D pt = globalCircleVertexPositions.at(j);
                    QVector3D transformedPoint(
                            pt.x() * normal.x() + pt.y() * binormal.x() + pt.z() * tangent.x() + center.x(),
                            pt.x() * normal.y() + pt.y() * binormal.y() + pt.z() * tangent.y() + center.y(),
                            pt.x() * normal.z() + pt.y() * binormal.z() + pt.z() * tangent.z() + center.z()
                    );
                    QVector3D vertexNormal = (transformedPoint - center).normalized();

                    vertexPositions.push_back(transformedPoint);
                    vertexNormals.push_back(vertexNormal);
                    vertexTangents.push_back(tangent);
                    vertexRollPositions.push_back(float(i - timeStepStart) / float(timeStepStop - timeStepStart));
                    vertexLineIds.push_back(trajectoryIndex);
                    vertexLinePointIndices.push_back(centerIdx);
                    vertexVariableIdAndIsCapList.push_back(variableId);
                }

                numValidLinePoints++;
            }

            if (numValidLinePoints == 1) {
                // Only one vertex left -> output nothing (tube consisting only of one point).
                for (int subdivIdx = 0; subdivIdx < numCircleSubdivisions; subdivIdx++) {
                    vertexPositions.pop_back();
                    vertexNormals.pop_back();
                    vertexTangents.pop_back();
                    vertexRollPositions.pop_back();
                    vertexLineIds.pop_back();
                    vertexLinePointIndices.pop_back();
                    vertexVariableIdAndIsCapList.pop_back();
                }
            }
            if (numValidLinePoints <= 1) {
                continue;
            }

            for (int i = 0; i < numValidLinePoints-1; i++) {
                for (int j = 0; j < numCircleSubdivisions; j++) {
                    // Build two CCW triangles (one quad) for each side
                    // Triangle 1
                    triangleIndices.push_back(
                            indexOffset + i*numCircleSubdivisions+j);
                    triangleIndices.push_back(
                            indexOffset + i*numCircleSubdivisions+(j+1)%numCircleSubdivisions);
                    triangleIndices.push_back(
                            indexOffset + ((i+1)%numValidLinePoints)*numCircleSubdivisions+(j+1)%numCircleSubdivisions);

                    // Triangle 2
                    triangleIndices.push_back(
                            indexOffset + i*numCircleSubdivisions+j);
                    triangleIndices.push_back(
                            indexOffset + ((i+1)%numValidLinePoints)*numCircleSubdivisions+(j+1)%numCircleSubdivisions);
                    triangleIndices.push_back(
                            indexOffset + ((i+1)%numValidLinePoints)*numCircleSubdivisions+j);
                }
            }

            /*
             * Close the tube with two flat caps at the ends.
             */
            int numLongitudeSubdivisions = numCircleSubdivisions; // azimuth
            int numLatitudeSubdivisions = int(std::ceil(numCircleSubdivisions/2)); // zenith

            // Hemisphere at the start.
            indexOffset = uint32_t(vertexPositions.size());
            QVector3D center0 = lineCenters[timeStepStart];
            QVector3D tangent0 = lineCenters[timeStepStart] - lineCenters[timeStepStart + 1];
            tangent0.normalize();
            QVector3D normal0 = lineNormals[lineIndexOffset];

            // Center point.
            vertexPositions.push_back(center0);
            vertexNormals.push_back(normal0);
            vertexTangents.push_back(tangent0);
            vertexRollPositions.push_back(0.0f);
            vertexLineIds.push_back(trajectoryIndex);
            vertexLinePointIndices.push_back(centerIdx);
            vertexVariableIdAndIsCapList.push_back(variableId | (1u << 31u));

            QVector3D binormal0 = QVector3D::crossProduct(tangent0, normal0);
            for (size_t i = 0; i < globalCircleVertexPositions.size(); i++) {
                QVector3D pt = globalCircleVertexPositions.at(i);
                QVector3D transformedPoint(
                        pt.x() * normal0.x() + pt.y() * binormal0.x() + pt.z() * tangent0.x() + center0.x(),
                        pt.x() * normal0.y() + pt.y() * binormal0.y() + pt.z() * tangent0.y() + center0.y(),
                        pt.x() * normal0.z() + pt.y() * binormal0.z() + pt.z() * tangent0.z() + center0.z()
                );
                QVector3D vertexNormal = (transformedPoint - center0).normalized();

                vertexPositions.push_back(transformedPoint);
                vertexNormals.push_back(vertexNormal);
                vertexTangents.push_back(tangent0);
                vertexRollPositions.push_back(0.0f);
                vertexLineIds.push_back(trajectoryIndex);
                vertexLinePointIndices.push_back(centerIdx);
                vertexVariableIdAndIsCapList.push_back(variableId | (1u << 31u));
            }
            for (int j = 0; j < numCircleSubdivisions; j++) {
                triangleIndices.push_back(indexOffset);
                triangleIndices.push_back(indexOffset + j + 1);
                triangleIndices.push_back(indexOffset + (j + 1) % numCircleSubdivisions + 1);
            }

            // Hemisphere at the end.
            indexOffset = uint32_t(vertexPositions.size());
            QVector3D center1 = lineCenters[timeStepStop];
            QVector3D tangent1 = lineCenters[timeStepStop] - lineCenters[timeStepStop-1];
            tangent1.normalize();
            QVector3D normal1 = lineNormals[lineIndexOffset + numValidLinePoints - 1];

            // Center point.
            vertexPositions.push_back(center1);
            vertexNormals.push_back(normal1);
            vertexTangents.push_back(tangent1);
            vertexRollPositions.push_back(1.0f);
            vertexLineIds.push_back(trajectoryIndex);
            vertexLinePointIndices.push_back(centerIdx);
            vertexVariableIdAndIsCapList.push_back(variableId | (1u << 31u));

            QVector3D binormal1 = QVector3D::crossProduct(tangent1, normal1);
            for (size_t i = 0; i < globalCircleVertexPositions.size(); i++) {
                QVector3D pt = globalCircleVertexPositions.at(i);
                QVector3D transformedPoint(
                        pt.x() * normal1.x() + pt.y() * binormal1.x() + pt.z() * tangent1.x() + center1.x(),
                        pt.x() * normal1.y() + pt.y() * binormal1.y() + pt.z() * tangent1.y() + center1.y(),
                        pt.x() * normal1.z() + pt.y() * binormal1.z() + pt.z() * tangent1.z() + center1.z()
                );
                QVector3D vertexNormal = (transformedPoint - center1).normalized();

                vertexPositions.push_back(transformedPoint);
                vertexNormals.push_back(vertexNormal);
                vertexTangents.push_back(tangent1);
                vertexRollPositions.push_back(1.0f);
                vertexLineIds.push_back(trajectoryIndex);
                vertexLinePointIndices.push_back(centerIdx);
                vertexVariableIdAndIsCapList.push_back(variableId | (1u << 31u));
            }
            for (int j = 0; j < numCircleSubdivisions; j++) {
                triangleIndices.push_back(indexOffset);
                triangleIndices.push_back(indexOffset + j + 1);
                triangleIndices.push_back(indexOffset + (j + 1) % numCircleSubdivisions + 1);
            }
        }

        trajectoryIndex++;
    }

    timeStepRollsRenderData.indexBuffer = createIndexBuffer(
            currentGLContext, timeStepRollsIndexBufferID, triangleIndices);
    timeStepRollsRenderData.vertexPositionBuffer = createVertexBuffer(
            currentGLContext, timeStepRollsVertexPositionBufferID, vertexPositions);
    timeStepRollsRenderData.vertexNormalBuffer = createVertexBuffer(
            currentGLContext, timeStepRollsVertexNormalBufferID, vertexNormals);
    timeStepRollsRenderData.vertexTangentBuffer = createVertexBuffer(
            currentGLContext, timeStepRollsVertexTangentBufferID, vertexTangents);
    timeStepRollsRenderData.vertexRollPositionBuffer = createVertexBuffer(
            currentGLContext, timeStepRollsPositionBufferID, vertexRollPositions);
    timeStepRollsRenderData.vertexLineIdBuffer = createVertexBuffer(
            currentGLContext, timeStepRollsVertexLineIdBufferID, vertexLineIds);
    timeStepRollsRenderData.vertexLinePointIdxBuffer = createVertexBuffer(
            currentGLContext, timeStepRollsVertexLinePointIdxBufferID, vertexLinePointIndices);
    timeStepRollsRenderData.vertexVariableIdAndIsCapBuffer = createVertexBuffer(
            currentGLContext, timeStepRollsVertexVariableIdAndIsCapBufferBufferID, vertexVariableIdAndIsCapList);
}

void MBezierTrajectories::releaseTimeStepRollsRenderData()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.indexBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexPositionBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexNormalBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexTangentBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexRollPositionBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexLineIdBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexLinePointIdxBuffer);
    MGLResourcesManager::getInstance()->releaseGPUItem(timeStepRollsRenderData.vertexVariableIdAndIsCapBuffer);
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
        QVector<QVector<float>>& trajectoryPointTimeSteps,
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

        const MBezierTrajectory& bezierTrajectory = bezierTrajectories[trajectoryIdx];

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
        QVector<float> pointTimeSteps;
        for (int i = selectionIndex; i < selectionCount; i++)
        {
            trajectory.push_back(bezierTrajectory.positions.at(i));
            pointTimeSteps.push_back(float(bezierTrajectory.elementIDs.at(i)));
        }

        trajectories.push_back(trajectory);
        trajectoryPointTimeSteps.push_back(pointTimeSteps);
        selectedTrajectoryIndices.push_back(uint32_t(trajectoryIdx));
    }

    return true;
}


}
