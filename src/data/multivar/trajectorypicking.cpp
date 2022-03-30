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
#ifdef USE_EMBREE
#include <embree3/rtcore.h>
#endif

// local application imports
#include "../../gxfw/msceneviewglwidget.h"
#include "charts/radarchart.h"
#include "charts/radarbarchart.h"
#include "charts/curveplotview.h"
#include "util/mutil.h"
#include "helpers.h"
#include "hidpi.h"

namespace Met3D {

MTrajectoryPicker::MTrajectoryPicker(
        GLuint textureUnit, MSceneViewGLWidget* sceneView, const QVector<QString>& varNames,
        DiagramDisplayType diagramType, MTransferFunction1D*& diagramTransferFunction)
        : textureUnit(textureUnit), diagramTransferFunction(diagramTransferFunction)
{
#ifdef USE_EMBREE
    device = rtcNewDevice(nullptr);
    scene = rtcNewScene(device);
    mesh = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
#endif

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

    auto it = std::find(variableNames.begin(), variableNames.end(), "time_after_ascent");
    if (it != variableNames.end()) {
        timeAfterAscentIndex = int(it - variableNames.begin());
    } else {
        timeAfterAscentIndex = -1;
    }

    diagramUpscalingFactor = getHighDPIScaleFactor();

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
        diagram = new MRadarBarChart(textureUnit, diagramTransferFunction);
    }
    else if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        diagram = new MCurvePlotView(textureUnit, diagramTransferFunction);
        auto* horizonGraph = static_cast<MCurvePlotView*>(diagram);
        horizonGraph->setSelectedTimeStep(timeStep);
        horizonGraph->setSimilarityMetric(similarityMetric);
        horizonGraph->setMeanMetricInfluence(meanMetricInfluence);
        horizonGraph->setStdDevMetricInfluence(stdDevMetricInfluence);
        horizonGraph->setNumBins(numBins);
        horizonGraph->setShowMinMaxValue(showMinMaxValue);
        horizonGraph->setUseMaxForSensitivity(useMaxForSensitivity);
        horizonGraph->setSubsequenceMatchingTechnique(subsequenceMatchingTechnique);
        horizonGraph->setSpringEpsilon(springEpsilon);
        horizonGraph->setTextSize(textSize);
    }

    if (diagram)
    {
        diagram->setBackgroundOpacity(springEpsilon);
        diagram->setUpscalingFactor(diagramUpscalingFactor);
        needsInitializationBeforeRendering = true;
    }

//#ifdef USE_QOPENGLWIDGET
    //MGLResourcesManager::getInstance()->doneCurrent();
//#endif
}

void MTrajectoryPicker::setSimilarityMetric(SimilarityMetric similarityMetric)
{
    this->similarityMetric = similarityMetric;
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setSimilarityMetric(similarityMetric);
    }
}

void MTrajectoryPicker::setMeanMetricInfluence(float meanMetricInfluence)
{
    this->meanMetricInfluence = meanMetricInfluence;
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setMeanMetricInfluence(meanMetricInfluence);
    }
}

void MTrajectoryPicker::setStdDevMetricInfluence(float stdDevMetricInfluence)
{
    this->stdDevMetricInfluence = stdDevMetricInfluence;
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setStdDevMetricInfluence(stdDevMetricInfluence);
    }
}

void MTrajectoryPicker::setNumBins(int numBins)
{
    this->numBins = numBins;
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setNumBins(numBins);
    }
}

void MTrajectoryPicker::sortByDescendingStdDev()
{
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->sortByDescendingStdDev();
    }
}

void MTrajectoryPicker::setShowMinMaxValue(bool show)
{
    this->showMinMaxValue = show;
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setShowMinMaxValue(showMinMaxValue);
    }
}

void MTrajectoryPicker::setTrimNanRegions(bool trimRegions)
{
    this->trimNanRegions = trimRegions;
    highlightDataDirty = true;
    selectedTrajectoriesChanged = true;
    if (diagram->getIsNanoVgInitialized())
    {
        updateDiagramData();
    }
}

void MTrajectoryPicker::setUseMaxForSensitivity(bool useMax)
{
    this->useMaxForSensitivity = useMax;
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setUseMaxForSensitivity(useMaxForSensitivity);
        if (diagram)
        {
            updateDiagramData();
        }
    }
}

void MTrajectoryPicker::setSubsequenceMatchingTechnique(SubsequenceMatchingTechnique technique)
{
    this->subsequenceMatchingTechnique = technique;
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setSubsequenceMatchingTechnique(subsequenceMatchingTechnique);
    }
}

void MTrajectoryPicker::setSpringEpsilon(float epsilon)
{
    this->springEpsilon = epsilon;
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setSpringEpsilon(springEpsilon);
    }
}

void MTrajectoryPicker::setBackgroundOpacity(float opacity)
{
    this->backgroundOpacity = opacity;
    diagram->setBackgroundOpacity(backgroundOpacity);
}

void MTrajectoryPicker::setDiagramNormalizationMode(DiagramNormalizationMode mode)
{
    diagramNormalizationMode = mode;
    highlightDataDirty = true;
    selectedTrajectoriesChanged = true;
    if (diagram->getIsNanoVgInitialized())
    {
        updateDiagramData();
    }
}

void MTrajectoryPicker::setTextSize(float _textSize)
{
    this->textSize = _textSize;
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setTextSize(_textSize);
    }
}

void MTrajectoryPicker::setDiagramUpscalingFactor(float factor)
{
    this->diagramUpscalingFactor = factor;
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setUpscalingFactor(diagramUpscalingFactor);
    }
}

void MTrajectoryPicker::triggerSelectAllLines()
{
    bool allTrajectoriesSelected = true;
    for (int lineId = 0; lineId < trajectories.size(); lineId++) {
        if (highlightedTrajectories.find(lineId) == highlightedTrajectories.end()) {
            allTrajectoriesSelected = false;
            break;
        }
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

    if (allTrajectoriesSelected) {
        // Unselect all.
        for (int lineId = 0; lineId < trajectories.size(); lineId++) {
            auto it = highlightedTrajectories.find(lineId);
            if (it != highlightedTrajectories.end())
            {
                colorUsesCountMap[it->second] -= 1;
                highlightedTrajectories.erase(lineId);
            }
        }
    } else {
        // Select all.
        for (int lineId = 0; lineId < trajectories.size(); lineId++) {
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
            highlightedTrajectories.insert(std::make_pair(lineId, highlightColor));
        }
    }

    highlightDataDirty = true;
    selectedTrajectoriesChanged = true;
    updateDiagramData();
}

void MTrajectoryPicker::resetVariableSorting()
{
    if (diagramDisplayType == DiagramDisplayType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->resetVariableSorting();
    }
}

MTrajectoryPicker::~MTrajectoryPicker()
{
    freeStorage();

#ifdef USE_EMBREE
    rtcReleaseGeometry(mesh);
    rtcReleaseScene(scene);
    rtcReleaseDevice(device);
#endif

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
#ifdef USE_EMBREE
    if (loaded) {
        rtcDetachGeometry(scene, geomID);
        rtcCommitScene(scene);
        loaded = false;
    }
#endif
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
            diagram->setSelectedVariableIndices(selectedVariableIndices);
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
    selectedTrajectoriesChanged = true;
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
                if (std::isnan(v))
                {
                    continue;
                }
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

    if (showMinMaxValue)
    {
        for (size_t i = 0; i < numVars; i++)
        {
            QVector2D& minMaxVector = minMaxAttributes[int(i)];
            LOG4CPLUS_DEBUG(
                    mlog,
                    variableNames.at(i) << ": " << std::to_string(minMaxVector.x())
                                        << ", " << std::to_string(minMaxVector.y()));
        }
    }

    if (timeAfterAscentIndex >= 0)
    {
        ascentTimeStepIndices.clear();
        ascentTimeStepIndices.reserve(filteredTrajectories.size());
        for (const MFilteredTrajectory& trajectory : filteredTrajectories)
        {
            int ascentTimeStep = 0;
            const QVector<float>& timeAfterAscentArray = trajectory.attributes.at(timeAfterAscentIndex);
            for (int i = 1; i < timeAfterAscentArray.size(); i++)
            {
                float timeAfterAscent0 = timeAfterAscentArray.at(i - 1);
                float timeAfterAscent1 = timeAfterAscentArray.at(i);
                if (timeAfterAscent0 == 0) {
                    ascentTimeStep = i - 1;
                    break;
                } else if (timeAfterAscent1 == 0 || (timeAfterAscent0 < 0.0f && timeAfterAscent1 > 0.0f)) {
                    ascentTimeStep = i;
                    break;
                }
            }
            ascentTimeStepIndices.push_back(ascentTimeStep);
        }
    }
}

void MTrajectoryPicker::setSyncMode(TrajectorySyncMode syncMode) {
    trajectorySyncMode = syncMode;
    selectedTrajectoriesChanged = true;
    updateDiagramData();
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
        int n = trajectories.at(int(lineId)).size();
        QVector<QVector3D> lineCenters;
        QVector<float> pointTimeSteps;
        lineCenters.reserve(n);
        pointTimeSteps.reserve(n);
        for (int i = 0; i < n; i++)
        {
            const QVector3D& position = trajectories.at(int(lineId)).at(i);
            if (!std::isnan(position.x()) && !std::isnan(position.y()) && !std::isnan(position.z()))
            {
                lineCenters.push_back(position);
                pointTimeSteps.push_back(trajectoryPointTimeSteps.at(int(lineId)).at(i));
            }
        }
        n = lineCenters.size();

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

#ifdef USE_EMBREE
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
#endif
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

void MTrajectoryPicker::mouseMoveEventParent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (!diagram)
    {
        return;
    }
    diagram->mouseMoveEventParent(sceneView, event);
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
#ifdef USE_EMBREE
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
    if (trajectorySyncMode == TrajectorySyncMode::TIME_OF_ASCENT)
    {
        //timeAtHit = timeAtHit + float(maxAscentTimeStepIndex - ascentTimeStepIndices.at(trajectoryIndex));
        timeAtHit = timeAtHit + float(-ascentTimeStepIndices.at(trajectoryIndex));
    }

    return true;
#else
    return false;
#endif
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
        selectedTrajectoriesChanged = true;
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
    selectedTrajectoriesChanged = true;
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
    }

    if (diagram && diagram->getDiagramType() != DiagramType::CURVE_PLOT_VIEW)
    {
        updateDiagramData();
    }

    if (diagram->getDiagramType() == DiagramType::CURVE_PLOT_VIEW)
    {
        static_cast<MCurvePlotView*>(diagram)->setSelectedTimeStep(newTimeStep);
    }
}

static inline bool startsWith(const std::string& str, const std::string& prefix) {
    return prefix.length() <= str.length() && std::equal(prefix.begin(), prefix.end(), str.begin());
}

bool computeIsAllNanAtTimeStep(
        const std::vector<std::vector<std::vector<float>>>& variableValuesArray,
        const std::vector<size_t>& validVarIndices, size_t numVars, int timeStepIdx)
{
    for (size_t trajectoryIdx = 0; trajectoryIdx < variableValuesArray.size(); trajectoryIdx++)
    {
        const std::vector<float> &values = variableValuesArray.at(trajectoryIdx).at(timeStepIdx);
        for (size_t varIdx : validVarIndices)
        {
            float value = values.at(int(varIdx));
            if (!std::isnan(value))
            {
                return false;
            }
        }
    }

    return true;
}

void MTrajectoryPicker::updateDiagramData()
{
    if (!diagram)
    {
        return;
    }

    QVector<QVector2D> minMaxAttributesLocal;
    if (diagramNormalizationMode != DiagramNormalizationMode::SELECTION_MIN_MAX)
    {
        minMaxAttributesLocal = minMaxAttributes;
    }
    else
    {
        minMaxAttributesLocal.resize(int(numVars));
        for (size_t i = 0; i < numVars; i++)
        {
            minMaxAttributesLocal[int(i)] = QVector2D(
                    std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest());
        }
        for (const auto& it : highlightedTrajectories)
        {
            uint32_t trajectoryIndex = it.first;
            const MFilteredTrajectory& trajectory = baseTrajectories.at(int(trajectoryIndex));

            for (size_t i = 0; i < numVars; i++)
            {
                const QVector<float>& attributes = trajectory.attributes.at(int(i));
                QVector2D& minMaxVector = minMaxAttributesLocal[int(i)];
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
        for (size_t i = 0; i < numVars; i++)
        {
            QVector2D& minMax = minMaxAttributesLocal[int(i)];
            if (std::isinf(minMax[1])) {
                minMax[1] = std::numeric_limits<float>::max();
            }
        }
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
                const MFilteredTrajectory& trajectory = baseTrajectories.at(int(trajectoryIndex));
                std::vector<float> values;
                for (size_t i = 0; i < numVars; i++)
                {
                    int timeStepLocal;
                    if (trajectorySyncMode == TrajectorySyncMode::TIME_OF_ASCENT)
                    {
                        //timeStepLocal = timeStep - maxAscentTimeStepIndex + ascentTimeStepIndices.at(trajectoryIndex);
                        timeStepLocal = timeStep + ascentTimeStepIndices.at(trajectoryIndex);
                    }
                    else
                    {
                        timeStepLocal = timeStep;
                    }
                    int time = clamp(timeStepLocal, 0, int(trajectory.attributes.at(int(i)).size()) - 1);
                    float value = trajectory.attributes.at(int(i)).at(time);
                    QVector2D minMaxVector = minMaxAttributesLocal.at(int(i));
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
                int timeStepLocal;
                if (trajectorySyncMode == TrajectorySyncMode::TIME_OF_ASCENT)
                {
                    //timeStepLocal = timeStep - maxAscentTimeStepIndex + ascentTimeStepIndices.at(trajectoryIndex);
                    timeStepLocal = timeStep + ascentTimeStepIndices.at(trajectoryIndex);
                }
                else
                {
                    timeStepLocal = timeStep;
                }
                int time = clamp(timeStepLocal, 0, int(trajectory.attributes.at(int(i)).size()) - 1);
                float value = trajectory.attributes.at(int(i)).at(time);
                QVector2D minMaxVector = minMaxAttributesLocal.at(int(i));
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
                int timeStepLocal;
                if (trajectorySyncMode == TrajectorySyncMode::TIME_OF_ASCENT)
                {
                    //timeStepLocal = timeStep - maxAscentTimeStepIndex + ascentTimeStepIndices.at(trajectoryIndex);
                    timeStepLocal = timeStep + ascentTimeStepIndices.at(trajectoryIndex);
                }
                else
                {
                    timeStepLocal = timeStep;
                }
                int time = clamp(timeStepLocal, 0, int(trajectory.attributes.at(int(i)).size()) - 1);
                float value = trajectory.attributes.at(int(i)).at(time);
                QVector2D minMaxVector = minMaxAttributesLocal.at(int(i));
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
    else if (diagram->getDiagramType() == DiagramType::CURVE_PLOT_VIEW)
    {
        const size_t numTimeSteps = baseTrajectories.empty() ? 1 : baseTrajectories.front().positions.size();

        MCurvePlotView* horizonGraph = static_cast<MCurvePlotView*>(diagram);
        std::vector<std::vector<std::vector<float>>> variableValuesArray;
        variableValuesArray.resize(highlightedTrajectories.size());

        float timeMin;
        float timeMax;
        if (timeAfterAscentIndex >= 0 && selectedTrajectoriesChanged)
        {
            minAscentTimeStepIndex = int(numTimeSteps);
            maxAscentTimeStepIndex = 0;
            if (highlightedTrajectories.empty())
            {
                for (int trajectoryIndex = 0; trajectoryIndex < baseTrajectories.size(); trajectoryIndex++)
                {
                    int ascentTimeStepIndex = ascentTimeStepIndices.at(trajectoryIndex);
                    minAscentTimeStepIndex = std::min(minAscentTimeStepIndex, ascentTimeStepIndex);
                    maxAscentTimeStepIndex = std::max(maxAscentTimeStepIndex, ascentTimeStepIndex);
                }
            }
            else
            {
                for (const auto& it : highlightedTrajectories)
                {
                    uint32_t trajectoryIndex = it.first;
                    int ascentTimeStepIndex = ascentTimeStepIndices.at(trajectoryIndex);
                    minAscentTimeStepIndex = std::min(minAscentTimeStepIndex, ascentTimeStepIndex);
                    maxAscentTimeStepIndex = std::max(maxAscentTimeStepIndex, ascentTimeStepIndex);
                }
            }
        }

        std::vector<bool> isVarSensitivityArray;
        isVarSensitivityArray.reserve(numVars);
        for (size_t varIdx = 0; varIdx < numVars; varIdx++)
        {
            const std::string& varName = variableNames.at(varIdx);
            bool isSensitivity =
                    (varName.at(0) == 'd' && varName != "deposition") || varName == "sensitivity_max";
            isVarSensitivityArray.push_back(isSensitivity);
        }

        size_t numTimeStepsTotal;
        if (trajectorySyncMode == TrajectorySyncMode::TIME_OF_ASCENT)
        {
            int delta = maxAscentTimeStepIndex - minAscentTimeStepIndex;
            int timeIdxMin = -maxAscentTimeStepIndex;
            int timeIdxMax = int(numTimeSteps) - 1 - minAscentTimeStepIndex;
            numTimeStepsTotal = numTimeSteps + size_t(delta);
            //timeMin = float(0.0f);
            //timeMax = float(numTimeStepsTotal);
            timeMin = float(timeIdxMin);
            timeMax = float(timeIdxMax);

            int i = 0;
            for (const auto& it : highlightedTrajectories)
            {
                uint32_t trajectoryIndex = it.first;
                const MFilteredTrajectory& trajectory = baseTrajectories.at(int(trajectoryIndex));
                std::vector<std::vector<float>>& variableValuesPerTrajectory = variableValuesArray.at(i);
                variableValuesPerTrajectory.resize(numTimeStepsTotal);
                for (size_t timeIdx = 0; timeIdx < numTimeStepsTotal; timeIdx++)
                {
                    std::vector<float>& values = variableValuesPerTrajectory.at(timeIdx);
                    values.resize(numVars, std::numeric_limits<float>::quiet_NaN());

                    int realTimeIdx =
                            int(timeIdx) - maxAscentTimeStepIndex + ascentTimeStepIndices.at(trajectoryIndex);
                    if (realTimeIdx >= 0 && realTimeIdx < int(numTimeSteps))
                    {
                        for (size_t varIdx = 0; varIdx < numVars; varIdx++)
                        {
                            bool isSensitivity = isVarSensitivityArray.at(varIdx);
                            float value = trajectory.attributes.at(int(varIdx)).at(realTimeIdx);
                            if (!std::isnan(value))
                            {
                                QVector2D minMaxVector = minMaxAttributesLocal.at(int(varIdx));
                                if (isSensitivity)
                                {
                                    float maxVal = std::max(std::abs(minMaxVector.x()), std::abs(minMaxVector.y()));
                                    float denominator = std::max(maxVal, 1e-10f);
                                    value = std::abs(value) / denominator;
                                }
                                else
                                {
                                    float denominator = std::max(minMaxVector.y() - minMaxVector.x(), 1e-10f);
                                    value = (value - minMaxVector.x()) / denominator;
                                }
                            }
                            values.at(varIdx) = value;
                        }
                    }
                    else
                    {
                        for (size_t varIdx = 0; varIdx < numVars; varIdx++)
                        {
                            values.at(varIdx) = std::numeric_limits<float>::quiet_NaN();
                        }
                    }
                }
                i++;
            }
        }
        else
        {
            numTimeStepsTotal = numTimeSteps;
            timeMin = 0.0f;
            timeMax = float(numTimeSteps - 1);

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
                        bool isSensitivity = isVarSensitivityArray.at(varIdx);
                        float value = trajectory.attributes.at(int(varIdx)).at(int(timeIdx));
                        if (!std::isnan(value)) {
                            QVector2D minMaxVector = minMaxAttributesLocal.at(int(varIdx));
                            if (isSensitivity)
                            {
                                float maxVal = std::max(std::abs(minMaxVector.x()), std::abs(minMaxVector.y()));
                                float denominator = std::max(maxVal, 1e-10f);
                                value = std::abs(value) / denominator;
                            }
                            else
                            {
                                float denominator = std::max(minMaxVector.y() - minMaxVector.x(), 1e-10f);
                                value = (value - minMaxVector.x()) / denominator;
                            }
                        }
                        values.at(varIdx) = value;
                    }
                }
                i++;
            }
        }

        if (trimNanRegions && !variableValuesArray.empty())
        {
            // Ignore conv_[...] and slan_[...] variables, as they do not use NaN to mark invalid areas.
            std::vector<size_t> validVarIndices;
            for (size_t varIdx = 0; varIdx < numVars; varIdx++)
            {
                std::string varName = variableNames.at(varIdx);
                if (!startsWith(varName, "conv_") && !startsWith(varName, "slan_"))
                {
                    validVarIndices.push_back(varIdx);
                }
            }

            int minTimeStepIdxNotNan = int(numTimeStepsTotal);
            int maxTimeStepIdxNotNan = -1;
            for (int timeStepIdx = 0; timeStepIdx < int(numTimeStepsTotal); timeStepIdx++)
            {
                bool isAllNan = computeIsAllNanAtTimeStep(variableValuesArray, validVarIndices, numVars, timeStepIdx);
                if (!isAllNan)
                {
                    minTimeStepIdxNotNan = int(timeStepIdx);
                    break;
                }
            }
            for (int timeStepIdx = int(numTimeStepsTotal) - 1; timeStepIdx >= 0; timeStepIdx--)
            {
                bool isAllNan = computeIsAllNanAtTimeStep(variableValuesArray, validVarIndices, numVars, timeStepIdx);
                if (!isAllNan)
                {
                    maxTimeStepIdxNotNan = int(timeStepIdx);
                    break;
                }
            }

            if ((minTimeStepIdxNotNan != 0 || maxTimeStepIdxNotNan != int(numTimeStepsTotal) - 1)
                    && minTimeStepIdxNotNan <= maxTimeStepIdxNotNan)
            {
                timeMin = timeMin + float(minTimeStepIdxNotNan);
                timeMax = timeMax - float(numTimeStepsTotal - 1 - maxTimeStepIdxNotNan);

                for (size_t trajectoryIdx = 0; trajectoryIdx < variableValuesArray.size(); trajectoryIdx++)
                {
                    std::vector<std::vector<float>>& variableValuesPerTrajectory =
                            variableValuesArray.at(trajectoryIdx);
                    variableValuesPerTrajectory = std::vector<std::vector<float>>(
                            variableValuesPerTrajectory.begin() + minTimeStepIdxNotNan,
                            variableValuesPerTrajectory.begin() + maxTimeStepIdxNotNan + 1);
                }
            }
        }

        horizonGraph->setData(
                variableNames, timeMin, timeMax, variableValuesArray,
                diagramNormalizationMode == DiagramNormalizationMode::BAND_MIN_MAX);
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
