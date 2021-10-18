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
#ifndef MET_3D_TRAJECTORYPICKING_H
#define MET_3D_TRAJECTORYPICKING_H

// standard library imports
#include <set>

// related third party imports
#include <QVector>
#include <QVector3D>
#include <embree3/rtcore.h>

// local application imports
#include "gxfw/gl/indexbuffer.h"
#include "gxfw/gl/vertexbuffer.h"
#include "gxfw/gl/shadereffect.h"
#include "data/abstractdataitem.h"
#include "qt_extensions/radarchart.h"
#include "beziertrajectories.h"
#include "radarbarchart.h"

namespace Met3D {

class MSceneViewGLWidget;

struct MHighlightedTrajectoriesRenderData
{
    GL::MIndexBuffer* indexBufferHighlighted = nullptr;
    GL::MVertexBuffer* vertexPositionBufferHighlighted = nullptr;
    GL::MVertexBuffer* vertexColorBufferHighlighted = nullptr;
};

class MTrajectoryPicker : public MMemoryManagementUsingObject {
public:
    MTrajectoryPicker(GLuint textureUnit, MSceneViewGLWidget* sceneView, const QVector<QString>& varNames);
    ~MTrajectoryPicker();

    void render();

    void setTrajectoryData(
            const QVector<QVector<QVector3D>>& trajectories,
            const QVector<QVector<float>>& trajectoryPointTimeSteps,
            const QVector<uint32_t>& selectedTrajectoryIndices);
    void updateTrajectoryRadius(float lineRadius);
    void setBaseTrajectories(const MFilteredTrajectories& filteredTrajectories);

    /**
     * Sets the triangle mesh data.
     * @param triangleIndices The triangle indices.
     * @param vertexPositions The vertex points.
     * @param vertexTrajectoryIndices The trajectory indices of the vertices.
     * @param vertexTimeSteps The time step at each vertex.
     */
    void setMeshTriangleData(
            const std::vector<uint32_t>& triangleIndices, const std::vector<QVector3D>& vertexPositions,
            const std::vector<uint32_t>& vertexTrajectoryIndices, const std::vector<float>& vertexTimeSteps);

    /**
     * Picks a point on the mesh using screen coordinates (assuming origin at upper left corner of viewport).
     * @param sceneView The scene view storing the viewport size and view/projection matrix.
     * @param x The x position on the screen (usually the mouse position).
     * @param y The y position on the screen (usually the mouse position).
     * @param firstHitPoint The first hit point on the mesh (closest to the camera) is stored in this variable.
     * @param lastHit The last hit point on the mesh (furthest away from the camera) is stored in this variable.
     * @return True if a point on the mesh was hit.
     */
    bool pickPointScreen(
            MSceneViewGLWidget* sceneView, int x, int y,
            QVector3D& firstHitPoint, uint32_t& trajectoryIndex, float& timeAtHit);

    /**
     * Picks a point on the mesh using a ray in world coordinates.
     * @param cameraPosition The origin of the ray (usually the camera position, but can be somewhere different, too).
     * @param rayDirection The direction of the ray (assumed to be normalized).
     * @param firstHitPoint The first hit point on the mesh (closest to the camera) is stored in this variable.
     * @param trajectoryIndex The index of the trajectory associated with the first hit.
     * @param timeAtHit The integration time at the first hit point.
     * @return True if a point on the mesh was hit.
     */
    bool pickPointWorld(
            const QVector3D& cameraPosition, const QVector3D& rayDirection,
            QVector3D& firstHitPoint, uint32_t& trajectoryIndex, float& timeAtHit);

    void toggleTrajectoryHighlighted(uint32_t trajectoryIndex);
    void setParticlePosTimeStep(int newTimeStep);
    MHighlightedTrajectoriesRenderData getHighlightedTrajectoriesRenderData(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif
    inline std::shared_ptr<GL::MShaderEffect> getHighlightShaderEffect() { return shaderEffectHighlighted; }
    inline QtExtensions::MMultiVarChartCollection& getMultiVarChartCollection() { return multiVarCharts; }

private:
    void freeStorage();
    void recreateTubeTriangleData();
    void updateHighlightRenderData(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext);
#else
            QGLWidget *currentGLContext);
#endif

    MFilteredTrajectories baseTrajectories;
    QVector<QVector2D> minMaxAttributes;
    int timeStep = 0;

    float lineRadius = 0.0f;
    int numCircleSubdivisions = 8;
    QVector<QVector<QVector3D>> trajectories;
    QVector<QVector<float>> trajectoryPointTimeSteps;
    QVector<uint32_t> selectedTrajectoryIndices;

    std::vector<uint32_t> triangleIndices;
    std::vector<QVector3D> vertexPositions;
    std::vector<uint32_t> vertexTrajectoryIndices;
    std::vector<float> vertexTimeSteps;

    RTCDevice device;
    RTCScene scene;
    RTCGeometry mesh;
    unsigned int geomID = 0;
    bool loaded = false;

    // Highlighted trajectories.
    std::map<uint32_t, QColor> highlightedTrajectories;
    struct ColorComparator {
        bool operator()(const QColor& c0, const QColor& c1) {
            if (c0.red() != c1.red()) {
                return c0.red() < c1.red();
            }
            if (c0.green() != c1.green()) {
                return c0.green() < c1.green();
            }
            if (c0.blue() != c1.blue()) {
                return c0.blue() < c1.blue();
            }
            if (c0.alpha() != c1.alpha()) {
                return c0.alpha() < c1.alpha();
            }
            return false;
        }
    };
    std::map<QColor, uint32_t, ColorComparator> colorUsesCountMap;
    bool highlightDataDirty = true;
    std::shared_ptr<GL::MShaderEffect> shaderEffectHighlighted;
    const QString indexBufferHighlightedID =
            QString("beziertrajectories_index_buffer_highlighted_#%1").arg(getID());
    const QString vertexPositionBufferHighlightedID =
            QString("beziertrajectories_vertex_position_buffer_highlighted_#%1").arg(getID());
    const QString vertexColorBufferHighlightedID =
            QString("beziertrajectories_vertex_color_buffer_highlighted_#%1").arg(getID());
    MHighlightedTrajectoriesRenderData highlightedTrajectoriesRenderData;
    MRadarBarChart* radarBarChart;
    QtExtensions::MMultiVarChartCollection multiVarCharts;
    QtExtensions::MRadarChart* radarChart;
    size_t numVars = 0;
};

}

#endif //MET_3D_TRAJECTORYPICKING_H
