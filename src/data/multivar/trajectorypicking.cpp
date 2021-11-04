/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2021 Christoph Neuhauser
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
#include "trajectorypicking.h"

// standard library imports

// related third party imports
#include <QVector4D>
#include <QtMath>
#include <embree3/rtcore.h>

// local application imports
#include "../../gxfw/msceneviewglwidget.h"
#include "charts/radarchart.h"
#include "charts/radarbarchart.h"
#include "charts/horizongraph.h"
#include "util/mutil.h"
#include "helpers.h"

namespace Met3D {

MTrajectoryPicker::MTrajectoryPicker(
        GLuint textureUnit, MSceneViewGLWidget* sceneView, const QVector<QString>& varNames,
        DiagramDisplayType diagramType) : textureUnit(textureUnit)//, multiVarCharts(sceneView)
{
    device = rtcNewDevice(nullptr);
    scene = rtcNewScene(device);
    mesh = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    glRM->generateEffectProgramUncached(
            "multivar_oriented_color_bands", shaderEffectHighlighted);
    shaderEffectHighlighted->compileFromFile_Met3DHome(
            "src/glsl/multivar/trajectories_highlighted.fx.glsl");

    numVars = varNames.size();

    variableNames.reserve(varNames.size());
    for (const QString& varName : varNames) {
        variableNames.push_back(varName.toStdString());
    }

    //radarChart = new QtExtensions::MRadarChart();
    //radarChart->setVariableNames(varNames);
    //radarChart->setRenderHint(QPainter::Antialiasing);
    //multiVarCharts.addChartView(radarChart);

    setDiagramType(diagramType);
}

void MTrajectoryPicker::setDiagramType(DiagramDisplayType type)
{
    // Make sure that "MGLResourcesManager" is the currently active context,
    // otherwise glDrawArrays on the VBO generated here will fail in any other
    // context than the currently active. The "MGLResourcesManager" context is
    // shared with all visible contexts, hence modifying the VBO there works
    // fine.
    //MGLResourcesManager::getInstance()->makeCurrent();

    if (diagram)
    {
        oldDiagram = diagram;
        diagram = nullptr;
    }

    diagramDisplayType = type;
    if (diagramDisplayType == DiagramDisplayType::RADAR_CHART)
    {
        diagram = new MRadarChart(textureUnit);
    }
    else if (diagramDisplayType == DiagramDisplayType::RADAR_BAR_CHART_TIME_DEPENDENT
            || diagramDisplayType == DiagramDisplayType::RADAR_BAR_CHART_TIME_INDEPENDENT)
    {
        diagram = new MRadarBarChart(textureUnit);
    }
    else if (diagramDisplayType == DiagramDisplayType::HORIZON_GRAPH)
    {
        diagram = new MHorizonGraph(textureUnit);
        static_cast<MHorizonGraph*>(diagram)->setSelectedTimeStep(timeStep);
    }

    if (diagram)
    {
        needsInitializationBeforeRendering = true;
    }

//#ifdef USE_QOPENGLWIDGET
    //MGLResourcesManager::getInstance()->doneCurrent();
//#endif
}

MTrajectoryPicker::~MTrajectoryPicker()
{
    freeStorage();

    rtcReleaseGeometry(mesh);
    rtcReleaseScene(scene);
    rtcReleaseDevice(device);

    if (highlightedTrajectoriesRenderData.indexBufferHighlighted)
    {
        MGLResourcesManager::getInstance()->releaseGPUItem(indexBufferHighlightedID);
        MGLResourcesManager::getInstance()->releaseGPUItem(vertexPositionBufferHighlightedID);
        MGLResourcesManager::getInstance()->releaseGPUItem(vertexColorBufferHighlightedID);
    }

    if (diagram)
    {
        delete diagram;
        diagram = nullptr;
    }
    if (oldDiagram)
    {
        delete oldDiagram;
        oldDiagram = nullptr;
    }
}

void MTrajectoryPicker::freeStorage()
{
    if (loaded) {
        rtcDetachGeometry(scene, geomID);
        rtcCommitScene(scene);
        loaded = false;
    }
}

void MTrajectoryPicker::render()
{
    if (oldDiagram)
    {
        delete oldDiagram;
        oldDiagram = nullptr;
    }

    if (diagram)
    {
        if (needsInitializationBeforeRendering)
        {
            needsInitializationBeforeRendering = false;
            diagram->createNanoVgHandle();
            diagram->initialize();
            updateDiagramData();
            diagram->setSelectedVariables(selectedVariables);
        }
        diagram->render();
    }
}

void MTrajectoryPicker::setTrajectoryData(
        const QVector<QVector<QVector3D>>& trajectories,
        const QVector<QVector<float>>& trajectoryPointTimeSteps,
        const QVector<uint32_t>& selectedTrajectoryIndices)
{
    this->trajectories = trajectories;
    this->trajectoryPointTimeSteps = trajectoryPointTimeSteps;
    this->selectedTrajectoryIndices = selectedTrajectoryIndices;
    this->highlightedTrajectories.clear();
    this->colorUsesCountMap.clear();
    //radarChart->clearRadars();

    if (lineRadius > 0.0f) {
        recreateTubeTriangleData();
    } else {
        this->triangleIndices.clear();
        this->vertexPositions.clear();
        this->vertexTrajectoryIndices.clear();
        this->vertexTimeSteps.clear();
    }
    highlightDataDirty = true;
}

void MTrajectoryPicker::updateTrajectoryRadius(float lineRadius)
{
    this->lineRadius = lineRadius;
    if (!this->trajectories.empty()) {
        recreateTubeTriangleData();
    }
    highlightDataDirty = true;
}

void MTrajectoryPicker::setBaseTrajectories(const MFilteredTrajectories& filteredTrajectories)
{
    baseTrajectories = filteredTrajectories;

    minMaxAttributes.clear();
    minMaxAttributes.resize(int(numVars));
    for (size_t i = 0; i < numVars; i++)
    {
        minMaxAttributes[int(i)] = QVector2D(
                std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest());
    }
    for (const MFilteredTrajectory& trajectory : filteredTrajectories)
    {
        for (size_t i = 0; i < numVars; i++)
        {
            const QVector<float>& attributes = trajectory.attributes.at(int(i));
            QVector2D& minMaxVector = minMaxAttributes[int(i)];
            for (float v : attributes)
            {
                minMaxVector.setX(std::min(minMaxVector.x(), v));
                minMaxVector.setY(std::max(minMaxVector.y(), v));
            }
        }
    }
    for (size_t i = 0; i < numVars; i++)
    {
        QVector2D& minMax = minMaxAttributes[int(i)];
        if (std::isinf(minMax[1])) {
            minMax[1] = std::numeric_limits<float>::max();
        }
    }
}

void MTrajectoryPicker::recreateTubeTriangleData()
{
    this->triangleIndices.clear();
    this->vertexPositions.clear();
    this->vertexTrajectoryIndices.clear();
    this->vertexTimeSteps.clear();

    std::vector<QVector3D> circleVertexPositions;

    const float theta = 2.0f * float(M_PI) / float(numCircleSubdivisions);
    const float tangentialFactor = std::tan(theta); // opposite / adjacent
    const float radialFactor = std::cos(theta); // adjacent / hypotenuse
    QVector3D position(lineRadius, 0, 0);

    for (int i = 0; i < numCircleSubdivisions; i++) {
        circleVertexPositions.push_back(position);

        // Add the tangent vector and correct the position using the radial factor.
        QVector3D tangent(-position.y(), position.x(), 0);
        position += tangentialFactor * tangent;
        position *= radialFactor;
    }

    for (int lineId = 0; lineId < trajectories.size(); lineId++) {
        const QVector<QVector3D>& lineCenters = trajectories.at(int(lineId));
        const QVector<float>& pointTimeSteps = trajectoryPointTimeSteps.at(int(lineId));
        int n = lineCenters.size();
        size_t indexOffset = vertexPositions.size();

        if (n < 2) {
            continue;
        }

        uint32_t selectedTrajectoryIndex = selectedTrajectoryIndices.at(int(lineId));

        QVector3D lastLineNormal(1.0f, 0.0f, 0.0f);
        int numValidLinePoints = 0;
        for (int i = 0; i < n; i++) {
            QVector3D tangent;
            if (i == 0) {
                tangent = lineCenters[i + 1] - lineCenters[i];
            } else if (i == n - 1) {
                tangent = lineCenters[i] - lineCenters[i - 1];
            } else {
                tangent = lineCenters[i + 1] - lineCenters[i - 1];
            }
            float lineSegmentLength = tangent.length();

            if (lineSegmentLength < 0.0001f) {
                // In case the two vertices are almost identical, just skip this path line segment.
                continue;
            }
            tangent.normalize();

            QVector3D center = lineCenters.at(i);
            float timeStep = pointTimeSteps.at(i);

            QVector3D helperAxis = lastLineNormal;
            if ((QVector3D::crossProduct(helperAxis, tangent).length()) < 0.01f) {
                // If tangent == lastLineNormal
                helperAxis = QVector3D(0.0f, 1.0f, 0.0f);
                if ((QVector3D::crossProduct(helperAxis, tangent).length()) < 0.01f) {
                    // If tangent == helperAxis
                    helperAxis = QVector3D(0.0f, 0.0f, 1.0f);
                }
            }
            QVector3D normal = (helperAxis - QVector3D::dotProduct(helperAxis, tangent) * tangent).normalized(); // Gram-Schmidt
            lastLineNormal = normal;
            QVector3D binormal = QVector3D::crossProduct(tangent, normal);

            for (size_t j = 0; j < circleVertexPositions.size(); j++) {
                QVector3D pt = circleVertexPositions.at(j);
                QVector3D transformedPoint(
                        pt.x() * normal.x() + pt.y() * binormal.x() + pt.z() * tangent.x() + center.x(),
                        pt.x() * normal.y() + pt.y() * binormal.y() + pt.z() * tangent.y() + center.y(),
                        pt.x() * normal.z() + pt.y() * binormal.z() + pt.z() * tangent.z() + center.z()
                );
                //QVector3D vertexNormal = (transformedPoint - center).normalized();

                vertexPositions.push_back(transformedPoint);
                vertexTrajectoryIndices.push_back(selectedTrajectoryIndex);
                vertexTimeSteps.push_back(timeStep);
            }

            numValidLinePoints++;
        }

        if (numValidLinePoints == 1) {
            // Only one vertex left -> output nothing (tube consisting only of one point).
            for (int subdivIdx = 0; subdivIdx < numCircleSubdivisions; subdivIdx++) {
                vertexPositions.pop_back();
                vertexTrajectoryIndices.pop_back();
                vertexTimeSteps.pop_back();
            }
            continue;
        }

        for (int i = 0; i < numValidLinePoints-1; i++) {
            for (int j = 0; j < numCircleSubdivisions; j++) {
                // Build two CCW triangles (one quad) for each side.
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
    }

    setMeshTriangleData(triangleIndices, vertexPositions, vertexTrajectoryIndices, vertexTimeSteps);
}

void MTrajectoryPicker::setMeshTriangleData(
        const std::vector<uint32_t>& triangleIndices, const std::vector<QVector3D>& vertexPositions,
        const std::vector<uint32_t>& vertexTrajectoryIndices, const std::vector<float>& vertexTimeSteps)
{
    freeStorage();
    if (triangleIndices.empty() || vertexPositions.empty() || vertexTrajectoryIndices.empty()
            || vertexTimeSteps.empty()) {
        return;
    }

    if (this->triangleIndices.data() != triangleIndices.data()) {
        this->triangleIndices = triangleIndices;
        this->vertexPositions = vertexPositions;
        this->vertexTrajectoryIndices = vertexTrajectoryIndices;
        this->vertexTimeSteps = vertexTimeSteps;
    }

    const size_t numVertices = vertexPositions.size();
    const size_t numIndices = triangleIndices.size();
    const size_t numTriangles = numIndices / 3;

    QVector4D* vertexPointer = (QVector4D*)rtcSetNewGeometryBuffer(
            mesh, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(QVector4D), numVertices);
    uint32_t* indexPointer = (uint32_t*)rtcSetNewGeometryBuffer(
            mesh, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
            sizeof(uint32_t) * 3, numTriangles);
    for (size_t i = 0; i < numVertices; i++) {
        const QVector3D& vertex = vertexPositions.at(i);
        vertexPointer[i] = QVector4D(vertex.x(), vertex.y(), vertex.z(), 1.0f);
    }
    for (size_t i = 0; i < numIndices; i++) {
        indexPointer[i] = triangleIndices[i];
    }

    rtcCommitGeometry(mesh);
    geomID = rtcAttachGeometry(scene, mesh);

    rtcCommitScene(scene);
    loaded = true;
}

bool MTrajectoryPicker::pickPointScreen(
        MSceneViewGLWidget* sceneView, int x, int y,
        QVector3D &firstHitPoint, uint32_t &trajectoryIndex, float &timeAtHit) {
    sceneView->getCamera()->getOrigin();

    int viewportWidth = sceneView->getViewPortWidth();
    int viewportHeight = sceneView->getViewPortHeight();
    float aspectRatio = float(viewportWidth) / float(viewportHeight);

    QMatrix4x4 inverseViewMatrix = sceneView->getCamera()->getViewMatrix().inverted();
    float scale = std::tan(qDegreesToRadians(sceneView->getVerticalAngle()) * 0.5f);
    QVector2D rayDirCameraSpace;
    rayDirCameraSpace.setX((2.0f * (float(x) + 0.5f) / float(viewportWidth) - 1.0f) * aspectRatio * scale);
    rayDirCameraSpace.setY((2.0f * (float(viewportHeight - y - 1) + 0.5f) / float(viewportHeight) - 1.0f) * scale);
    QVector4D rayDirectionVec4 = inverseViewMatrix * QVector4D(rayDirCameraSpace, -1.0, 0.0);
    QVector3D rayDirection = QVector3D(rayDirectionVec4.x(), rayDirectionVec4.y(), rayDirectionVec4.z());
    rayDirection.normalize();

    return pickPointWorld(
            sceneView->getCamera()->getOrigin(), rayDirection, firstHitPoint, trajectoryIndex, timeAtHit);
}

bool MTrajectoryPicker::checkVirtualWindowBelowMouse(
        MSceneViewGLWidget* sceneView, int mousePositionX, int mousePositionY)
{
    if (!diagram)
    {
        return false;
    }

    int viewportHeight = sceneView->getViewPortHeight();
    QVector2D mousePosition(float(mousePositionX), float(viewportHeight - mousePositionY - 1));
    return diagram->isMouseOverDiagram(mousePosition) && diagram->hasData();
}

void MTrajectoryPicker::mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (!diagram)
    {
        return;
    }
    diagram->mouseMoveEvent(sceneView, event);
}

void MTrajectoryPicker::mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (!diagram)
    {
        return;
    }
    diagram->mousePressEvent(sceneView, event);
}

void MTrajectoryPicker::mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (!diagram)
    {
        return;
    }
    diagram->mouseReleaseEvent(sceneView, event);
}

void MTrajectoryPicker::wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event)
{
    if (!diagram)
    {
        return;
    }
    diagram->wheelEvent(sceneView, event);
}


bool MTrajectoryPicker::pickPointWorld(
        const QVector3D& cameraPosition, const QVector3D& rayDirection,
        QVector3D& firstHitPoint, uint32_t& trajectoryIndex, float& timeAtHit)
{
    if (!loaded) {
        return false;
    }

    QVector3D rayOrigin = cameraPosition;

    const float EPSILON_DEPTH = 1e-3f;
    const float INFINITY_DEPTH = 1e30f;

    RTCIntersectContext context{};
    rtcInitIntersectContext(&context);

    RTCRayHit query{};
    query.ray.org_x = rayOrigin.x();
    query.ray.org_y = rayOrigin.y();
    query.ray.org_z = rayOrigin.z();
    query.ray.dir_x = rayDirection.x();
    query.ray.dir_y = rayDirection.y();
    query.ray.dir_z = rayDirection.z();
    query.ray.tnear = EPSILON_DEPTH;
    query.ray.tfar = INFINITY_DEPTH;
    query.ray.time = 0.0f;
    query.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    query.hit.primID = RTC_INVALID_GEOMETRY_ID;

    rtcIntersect1(scene, &context, &query);
    if (query.hit.geomID == RTC_INVALID_GEOMETRY_ID) {
        return false;
    }

    firstHitPoint = rayOrigin + query.ray.tfar * rayDirection;
    uint32_t vidx0 = triangleIndices.at(query.hit.primID * 3u);
    uint32_t vidx1 = triangleIndices.at(query.hit.primID * 3u + 1u);
    uint32_t vidx2 = triangleIndices.at(query.hit.primID * 3u + 2u);
    trajectoryIndex = vertexTrajectoryIndices.at(vidx0);

    const QVector3D barycentricCoordinates(1.0f - query.hit.u - query.hit.v, query.hit.u, query.hit.v);
    timeAtHit =
            vertexTimeSteps.at(vidx0) * barycentricCoordinates.x()
            + vertexTimeSteps.at(vidx1) * barycentricCoordinates.y()
            + vertexTimeSteps.at(vidx2) * barycentricCoordinates.z();

    return true;
}

void MTrajectoryPicker::toggleTrajectoryHighlighted(uint32_t trajectoryIndex)
{
    highlightDataDirty = true;

    auto it = highlightedTrajectories.find(trajectoryIndex);
    if (it != highlightedTrajectories.end())
    {
        //radarChart->removeRadar(trajectoryIndex);
        colorUsesCountMap[it->second] -= 1;
        highlightedTrajectories.erase(trajectoryIndex);
        updateDiagramData();
        return;
    }

    const std::vector<QColor> predefinedColors =
    {
            // RED
            QColor(228, 26, 28),
            // BLUE
            QColor(55, 126, 184),
            // GREEN
            QColor(5, 139, 69),
            // PURPLE
            QColor(129, 15, 124),
            // ORANGE
            QColor(217, 72, 1),
            // PINK
            QColor(231, 41, 138),
            // GOLD
            QColor(254, 178, 76),
            // DARK BLUE
            QColor(0, 7, 255)
    };
    if (colorUsesCountMap.empty())
    {
        for (const QColor& color : predefinedColors)
        {
            colorUsesCountMap[color] = 0;
        }
    }

    uint32_t minNumUses = std::numeric_limits<uint32_t>::max();
    QColor highlightColor;
    for (const QColor& color : predefinedColors)
    {
        if (colorUsesCountMap[color] < minNumUses)
        {
            minNumUses = colorUsesCountMap[color];
            highlightColor = color;
        }
    }
    colorUsesCountMap[highlightColor] += 1;

    highlightedTrajectories.insert(std::make_pair(trajectoryIndex, highlightColor));
    const MFilteredTrajectory& trajectory = baseTrajectories.at(int(trajectoryIndex));
    QVector<float> values;
    for (size_t i = 0; i < numVars; i++)
    {
        int time = clamp(timeStep, 0, int(trajectory.attributes.at(int(i)).size()) - 1);
        float value = trajectory.attributes.at(int(i)).at(time);
        QVector2D minMaxVector = minMaxAttributes.at(int(i));
        float denominator = std::max(minMaxVector.y() - minMaxVector.x(), 1e-10f);
        value = (value - minMaxVector.x()) / denominator;
        values.push_back(value);
    }
    //radarChart->addRadar(
    //        trajectoryIndex, QString("Trajectory #%1").arg(trajectoryIndex), highlightColor, values);
    updateDiagramData();
}

void MTrajectoryPicker::setParticlePosTimeStep(int newTimeStep)
{
    if (timeStep == newTimeStep)
    {
        return;
    }
    timeStep = newTimeStep;

    //radarChart->clearRadars();

    for (const auto& it : highlightedTrajectories)
    {
        uint32_t trajectoryIndex = it.first;
        const QColor& highlightColor = it.second;
        const MFilteredTrajectory& trajectory = baseTrajectories.at(int(trajectoryIndex));
        QVector<float> values;
        for (size_t i = 0; i < numVars; i++)
        {
            int time = clamp(timeStep, 0, int(trajectory.attributes.at(int(i)).size()) - 1);
            float value = trajectory.attributes.at(int(i)).at(time);
            QVector2D minMaxVector = minMaxAttributes.at(int(i));
            float denominator = std::max(minMaxVector.y() - minMaxVector.x(), 1e-10f);
            value = (value - minMaxVector.x()) / denominator;
            values.push_back(value);
        }
        //radarChart->addRadar(
        //        trajectoryIndex, QString("Trajectory #%1").arg(trajectoryIndex), highlightColor, values);
    }

    if (diagram && diagram->getDiagramType() != DiagramType::HORIZON_GRAPH)
    {
        updateDiagramData();
    }

    if (diagram->getDiagramType() == DiagramType::HORIZON_GRAPH)
    {
        static_cast<MHorizonGraph*>(diagram)->setSelectedTimeStep(newTimeStep);
    }
}

void MTrajectoryPicker::updateDiagramData()
{
    if (!diagram)
    {
        return;
    }

    if (diagram->getDiagramType() == DiagramType::RADAR_BAR_CHART)
    {
        MRadarBarChart* radarBarChart = static_cast<MRadarBarChart*>(diagram);
        if (diagramDisplayType == DiagramDisplayType::RADAR_BAR_CHART_TIME_DEPENDENT)
        {
            std::vector<std::vector<float>> variableValuesTimeDependent;
            variableValuesTimeDependent.reserve(highlightedTrajectories.size());
            for (const auto& it : highlightedTrajectories)
            {
                uint32_t trajectoryIndex = it.first;
                const QColor& highlightColor = it.second;
                const MFilteredTrajectory& trajectory = baseTrajectories.at(int(trajectoryIndex));
                std::vector<float> values;
                for (size_t i = 0; i < numVars; i++)
                {
                    int time = clamp(timeStep, 0, int(trajectory.attributes.at(int(i)).size()) - 1);
                    float value = trajectory.attributes.at(int(i)).at(time);
                    QVector2D minMaxVector = minMaxAttributes.at(int(i));
                    float denominator = std::max(minMaxVector.y() - minMaxVector.x(), 1e-10f);
                    value = (value - minMaxVector.x()) / denominator;
                    values.push_back(value);
                }
                variableValuesTimeDependent.push_back(values);
            }
            radarBarChart->setDataTimeDependent(variableNames, variableValuesTimeDependent);
        }
        else if (!highlightedTrajectories.empty())
        {
            uint32_t trajectoryIndex = highlightedTrajectories.begin()->first;
            const MFilteredTrajectory& trajectory = baseTrajectories.at(int(trajectoryIndex));
            std::vector<float> variableValues;
            for (size_t i = 0; i < numVars; i++)
            {
                int time = clamp(timeStep, 0, int(trajectory.attributes.at(int(i)).size()) - 1);
                float value = trajectory.attributes.at(int(i)).at(time);
                QVector2D minMaxVector = minMaxAttributes.at(int(i));
                float denominator = std::max(minMaxVector.y() - minMaxVector.x(), 1e-10f);
                value = (value - minMaxVector.x()) / denominator;
                variableValues.push_back(value);
            }
            radarBarChart->setDataTimeIndependent(variableNames, variableValues);
        }
    }
    else if (diagram->getDiagramType() == DiagramType::RADAR_CHART)
    {
        MRadarChart* radarChart = static_cast<MRadarChart*>(diagram);
        std::vector<std::vector<float>> variableValuesPerTrajectory;
        std::vector<QColor> highlightColors;
        variableValuesPerTrajectory.reserve(highlightedTrajectories.size());
        highlightColors.reserve(highlightedTrajectories.size());
        for (const auto& it : highlightedTrajectories)
        {
            uint32_t trajectoryIndex = it.first;
            const QColor& highlightColor = it.second;
            const MFilteredTrajectory& trajectory = baseTrajectories.at(int(trajectoryIndex));
            std::vector<float> values;
            for (size_t i = 0; i < numVars; i++)
            {
                int time = clamp(timeStep, 0, int(trajectory.attributes.at(int(i)).size()) - 1);
                float value = trajectory.attributes.at(int(i)).at(time);
                QVector2D minMaxVector = minMaxAttributes.at(int(i));
                float denominator = std::max(minMaxVector.y() - minMaxVector.x(), 1e-10f);
                value = (value - minMaxVector.x()) / denominator;
                if (std::isinf(value)) {
                    value = 1.0f;
                }
                if (std::isnan(value)) {
                    value = 0.0f;
                }
                values.push_back(value);
            }
            variableValuesPerTrajectory.push_back(values);
            highlightColors.push_back(highlightColor);
        }
        radarChart->setData(variableNames, variableValuesPerTrajectory, highlightColors);
    }
    else if (diagram->getDiagramType() == DiagramType::HORIZON_GRAPH)
    {
        const size_t numTimeSteps = baseTrajectories.empty() ? 1 : baseTrajectories.front().positions.size();

        MHorizonGraph* horizonGraph = static_cast<MHorizonGraph*>(diagram);
        std::vector<std::vector<std::vector<float>>> variableValuesArray;
        variableValuesArray.resize(highlightedTrajectories.size());
        int i = 0;
        for (const auto& it : highlightedTrajectories)
        {
            uint32_t trajectoryIndex = it.first;
            const MFilteredTrajectory& trajectory = baseTrajectories.at(int(trajectoryIndex));
            std::vector<std::vector<float>>& variableValuesPerTrajectory = variableValuesArray.at(i);
            variableValuesPerTrajectory.resize(numTimeSteps);
            for (size_t timeIdx = 0; timeIdx < numTimeSteps; timeIdx++)
            {
                std::vector<float>& values = variableValuesPerTrajectory.at(timeIdx);
                values.resize(numVars);
                for (size_t varIdx = 0; varIdx < numVars; varIdx++)
                {
                    float value = trajectory.attributes.at(int(varIdx)).at(int(timeIdx));
                    QVector2D minMaxVector = minMaxAttributes.at(int(varIdx));
                    float denominator = std::max(minMaxVector.y() - minMaxVector.x(), 1e-10f);
                    value = (value - minMaxVector.x()) / denominator;
                    values.at(varIdx) = value;
                }
            }
            i++;
        }

        horizonGraph->setData(variableNames, 0.0f, float(numTimeSteps - 1), variableValuesArray);
    }
}

MHighlightedTrajectoriesRenderData MTrajectoryPicker::getHighlightedTrajectoriesRenderData(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
{
    if (highlightDataDirty) {
        updateHighlightRenderData(currentGLContext);
    }
    return highlightedTrajectoriesRenderData;
}

void MTrajectoryPicker::updateHighlightRenderData(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
{
    QVector<uint32_t> triangleIndicesHighlighted;
    QVector<QVector3D> vertexPositionsHighlighted;
    QVector<QVector4D> vertexColorsHighlighted;

    std::vector<QVector3D> circleVertexPositions;

    const float theta = 2.0f * float(M_PI) / float(numCircleSubdivisions);
    const float tangentialFactor = std::tan(theta); // opposite / adjacent
    const float radialFactor = std::cos(theta); // adjacent / hypotenuse
    QVector3D position(lineRadius * 1.25f, 0, 0);

    for (int i = 0; i < numCircleSubdivisions; i++) {
        circleVertexPositions.push_back(position);

        // Add the tangent vector and correct the position using the radial factor.
        QVector3D tangent(-position.y(), position.x(), 0);
        position += tangentialFactor * tangent;
        position *= radialFactor;
    }

    for (int lineId = 0; lineId < trajectories.size(); lineId++) {
        const QVector<QVector3D> &lineCenters = trajectories.at(lineId);
        int n = lineCenters.size();
        size_t indexOffset = vertexPositionsHighlighted.size();

        if (n < 2) {
            continue;
        }

        uint32_t selectedTrajectoryIndex = selectedTrajectoryIndices.at(lineId);
        auto it = highlightedTrajectories.find(selectedTrajectoryIndex);
        if (it == highlightedTrajectories.end()) {
            continue;
        }

        qreal r, g, b, a;
        it->second.getRgbF(&r, &g, &b, &a);
        QVector4D lineColor{float(r), float(g), float(b), float(a)};

        QVector3D lastLineNormal(1.0f, 0.0f, 0.0f);
        int numValidLinePoints = 0;
        for (int i = 0; i < n; i++) {
            QVector3D tangent;
            if (i == 0) {
                tangent = lineCenters[i + 1] - lineCenters[i];
            } else if (i == n - 1) {
                tangent = lineCenters[i] - lineCenters[i - 1];
            } else {
                tangent = lineCenters[i + 1] - lineCenters[i - 1];
            }
            float lineSegmentLength = tangent.length();

            if (lineSegmentLength < 0.0001f) {
                // In case the two vertices are almost identical, just skip this path line segment.
                continue;
            }
            tangent.normalize();

            QVector3D center = lineCenters.at(i);

            QVector3D helperAxis = lastLineNormal;
            if ((QVector3D::crossProduct(helperAxis, tangent).length()) < 0.01f) {
                // If tangent == lastLineNormal
                helperAxis = QVector3D(0.0f, 1.0f, 0.0f);
                if ((QVector3D::crossProduct(helperAxis, tangent).length()) < 0.01f) {
                    // If tangent == helperAxis
                    helperAxis = QVector3D(0.0f, 0.0f, 1.0f);
                }
            }
            QVector3D normal = (helperAxis - QVector3D::dotProduct(helperAxis, tangent) * tangent).normalized(); // Gram-Schmidt
            lastLineNormal = normal;
            QVector3D binormal = QVector3D::crossProduct(tangent, normal);

            for (size_t j = 0; j < circleVertexPositions.size(); j++) {
                QVector3D pt = circleVertexPositions.at(j);
                QVector3D transformedPoint(
                        pt.x() * normal.x() + pt.y() * binormal.x() + pt.z() * tangent.x() + center.x(),
                        pt.x() * normal.y() + pt.y() * binormal.y() + pt.z() * tangent.y() + center.y(),
                        pt.x() * normal.z() + pt.y() * binormal.z() + pt.z() * tangent.z() + center.z()
                );
                //QVector3D vertexNormal = (transformedPoint - center).normalized();

                vertexPositionsHighlighted.push_back(transformedPoint);
                vertexColorsHighlighted.push_back(lineColor);
            }

            numValidLinePoints++;
        }

        if (numValidLinePoints == 1) {
            // Only one vertex left -> output nothing (tube consisting only of one point).
            for (int subdivIdx = 0; subdivIdx < numCircleSubdivisions; subdivIdx++) {
                vertexPositionsHighlighted.pop_back();
                vertexColorsHighlighted.pop_back();
            }
            continue;
        }

        for (int i = 0; i < numValidLinePoints-1; i++) {
            for (int j = 0; j < numCircleSubdivisions; j++) {
                // Build two CCW triangles (one quad) for each side.
                // Triangle 1
                triangleIndicesHighlighted.push_back(
                        indexOffset + i*numCircleSubdivisions+j);
                triangleIndicesHighlighted.push_back(
                        indexOffset + i*numCircleSubdivisions+(j+1)%numCircleSubdivisions);
                triangleIndicesHighlighted.push_back(
                        indexOffset + ((i+1)%numValidLinePoints)*numCircleSubdivisions+(j+1)%numCircleSubdivisions);

                // Triangle 2
                triangleIndicesHighlighted.push_back(
                        indexOffset + i*numCircleSubdivisions+j);
                triangleIndicesHighlighted.push_back(
                        indexOffset + ((i+1)%numValidLinePoints)*numCircleSubdivisions+(j+1)%numCircleSubdivisions);
                triangleIndicesHighlighted.push_back(
                        indexOffset + ((i+1)%numValidLinePoints)*numCircleSubdivisions+j);
            }
        }
    }

    if (highlightedTrajectoriesRenderData.indexBufferHighlighted) {
        MGLResourcesManager::getInstance()->releaseGPUItem(indexBufferHighlightedID);
        MGLResourcesManager::getInstance()->releaseGPUItem(vertexPositionBufferHighlightedID);
        MGLResourcesManager::getInstance()->releaseGPUItem(vertexColorBufferHighlightedID);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(indexBufferHighlightedID);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(vertexPositionBufferHighlightedID);
        MGLResourcesManager::getInstance()->deleteReleasedGPUItem(vertexColorBufferHighlightedID);
    }

    if (triangleIndicesHighlighted.empty()) {
        highlightedTrajectoriesRenderData = {};
        return;
    }

    // Add the index buffer.
    highlightedTrajectoriesRenderData.indexBufferHighlighted = createIndexBuffer(
            currentGLContext, indexBufferHighlightedID, triangleIndicesHighlighted);

    // Add the vertex position buffer.
    highlightedTrajectoriesRenderData.vertexPositionBufferHighlighted = createVertexBuffer(
            currentGLContext, vertexPositionBufferHighlightedID, vertexPositionsHighlighted);

    // Add the vertex color buffer.
    highlightedTrajectoriesRenderData.vertexColorBufferHighlighted = createVertexBuffer(
            currentGLContext, vertexColorBufferHighlightedID, vertexColorsHighlighted);

    highlightDataDirty = false;
}

}
