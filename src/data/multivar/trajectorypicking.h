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
#ifdef USE_EMBREE
#include <embree3/rtcore.h>
#endif

// local application imports
#include "actors/transferfunction1d.h"
#include "gxfw/tooltippicker.h"
#include "gxfw/gl/indexbuffer.h"
#include "gxfw/gl/vertexbuffer.h"
#include "gxfw/gl/shadereffect.h"
#include "data/abstractdataitem.h"
#include "qt_extensions/radarchart.h"
#include "charts/radarbarchart.h"
#include "charts/curveplotview.h"
#include "multivartrajectories.h"

namespace Met3D {

class MSceneViewGLWidget;
enum class MultiVarFocusRenderMode : unsigned int;

struct MHighlightedTrajectoriesRenderData
{
    GL::MIndexBuffer* indexBufferHighlighted = nullptr;
    GL::MVertexBuffer* vertexPositionBufferHighlighted = nullptr;
    GL::MVertexBuffer* vertexColorBufferHighlighted = nullptr;
};

enum class DiagramDisplayType {
    NONE, RADAR_BAR_CHART_TIME_DEPENDENT, RADAR_BAR_CHART_TIME_INDEPENDENT, RADAR_CHART, CURVE_PLOT_VIEW
};

class MTrajectoryPicker : public MMemoryManagementUsingObject, public MToolTipPicker {
public:
    MTrajectoryPicker(
            GLuint textureUnit, MSceneViewGLWidget* sceneView, const QVector<QString>& varNames,
            QVector<MTransferFunction1D*>& transferFunctionsMultiVar,
            DiagramDisplayType diagramType, MTransferFunction1D*& diagramTransferFunction);
    ~MTrajectoryPicker() override;

    void render();

    void setTrajectoryData(
            const QVector<QVector<QVector3D>>& trajectories,
            const QVector<QVector<float>>& trajectoryPointTimeSteps,
            const QVector<uint32_t>& selectedTrajectoryIndices,
            int _numTrajectoriesTotal);
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
     * Sets the time step sphere data.
     * @param spherePositions The center positions of the time step spheres.
     * @param entrancePoints The first entrance points of the trajectories into the time step spheres.
     * @param exitPoints The last exit points of the trajectories into the time step spheres.
     * @param lineElementIds The element ID data linking the spheres to the corresponding trajectories.
     * @param sphereRadius The sphere of the radius.
     */
    void setTimeStepSphereData(
            const QVector<QVector4D>& spherePositions,
            const QVector<QVector4D>& entrancePoints,
            const QVector<QVector4D>& exitPoints,
            const QVector<LineElementIdData>& lineElementIds,
            float sphereRadius);
    void setMultiVarFocusRenderMode(MultiVarFocusRenderMode mode) { focusRenderMode = mode; }
    void updateRenderSpheresIfNecessary(bool renderSpheres);
    void setShowTargetVariableAndSensitivity(bool show);

    void updateSectedOutputParameter(const QString& _varName, const int _selectedOutputIdx);

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

    /**
     * Picks a point on the mesh using screen coordinates (assuming origin at upper left corner of viewport)
     * and returns the depth and tool tip text for the intersection.
     * @param position The position on the screen (usually the mouse position).
     * @param depth The depth of the intersection (set if an intersection occurred).
     * @param text The tool tip text for the intersection (set if an intersection occurred).
     * @return True if a point on the mesh was hit.
     */
    bool toolTipPick(MSceneViewGLWidget* sceneView, const QPoint &position, float &depth, QString &text) override;

    /**
     * Checks whether a virtual (i.e., drawn using OpenGL) window is below
     * the mouse cursor at @p mousePositionX, @p mousePositionY.
     */
    bool checkVirtualWindowBelowMouse(MSceneViewGLWidget* sceneView, int mousePositionX, int mousePositionY);

    void mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event);
    void mouseMoveEventParent(MSceneViewGLWidget *sceneView, QMouseEvent *event);
    void mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event);
    void mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event);
    void wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event);

    void setDiagramType(DiagramDisplayType type);
    void setSimilarityMetric(SimilarityMetric similarityMetric);
    void setMeanMetricInfluence(float meanMetricInfluence);
    void setStdDevMetricInfluence(float stdDevMetricInfluence);
    void setNumBins(int numBins);
    void sortByDescendingStdDev();
    void setShowMinMaxValue(bool show);
    void setUseMaxForSensitivity(bool useMax);
    void setUseVariableToolTip(bool useToolTip);
    void setTrimNanRegions(bool trimRegions);
    void setSubsequenceMatchingTechnique(SubsequenceMatchingTechnique technique);
    void setSpringEpsilon(float epsilon);
    void setBackgroundOpacity(float opacity);
    void setDiagramNormalizationMode(DiagramNormalizationMode mode);
    void setTextSize(float _textSize);
    void setDiagramUpscalingFactor(float factor);
    void triggerSelectAllLines();
    void resetVariableSorting();

    // Used for aligning warm conveyor belt trajectories based on their ascension.
    void setSyncMode(TrajectorySyncMode syncMode);
    inline QVector<int> getAscentTimeStepIndices() const {
        QVector<int> ascentTimeStepIndicesQVector;
        ascentTimeStepIndicesQVector.reserve(int(ascentTimeStepIndices.size()));
        for (const auto& index : ascentTimeStepIndices) {
            ascentTimeStepIndicesQVector.push_back(index);
        }
        return ascentTimeStepIndicesQVector;
    }
    inline bool getHasAscentData() const {
        return timeAfterAscentIndex >= 0;
    }
    inline int getMaxAscentTimeStepIndex() const {
        return maxAscentTimeStepIndex;
    }

    // Forwards all calls to 'diagram'.
    inline float getSelectedTimeStep() const {
        if (diagram && diagram->getDiagramType() == DiagramType::CURVE_PLOT_VIEW) {
            return static_cast<MCurvePlotView*>(diagram)->getSelectedTimeStep();
        }
        return 0.0f;
    }
    inline void setSelectedTimeStep(float timeStep) {
        if (diagram && diagram->getDiagramType() == DiagramType::CURVE_PLOT_VIEW) {
            static_cast<MCurvePlotView*>(diagram)->setSelectedTimeStep(timeStep);
        }
    }
    inline bool getSelectedTimeStepChanged() const {
        if (diagram && diagram->getDiagramType() == DiagramType::CURVE_PLOT_VIEW) {
            return static_cast<MCurvePlotView*>(diagram)->getSelectedTimeStepChanged();
        }
        return false;
    }
    inline void resetSelectedTimeStepChanged() {
        if (diagram && diagram->getDiagramType() == DiagramType::CURVE_PLOT_VIEW) {
            static_cast<MCurvePlotView*>(diagram)->resetSelectedTimeStepChanged();
        }
    }

    inline const QVector<uint32_t>& getSelectedVariableIndices() {
        if (diagram) {
            this->selectedVariableIndices = diagram->getSelectedVariableIndices();
        } else {
            this->selectedVariableIndices = {};
        }
        return this->selectedVariableIndices;
    }
    inline void setSelectedVariableIndices(const QVector<uint32_t>& _selectedVariableIndices) {
        this->selectedVariableIndices = _selectedVariableIndices;
        if (diagram) {
            diagram->setSelectedVariableIndices(selectedVariableIndices);
        }
    }
    inline bool getSelectedVariablesChanged() const {
        if (diagram) {
            return diagram->getSelectedVariablesChanged();
        }
        return false;
    }
    inline void resetSelectedVariablesChanged() {
        if (diagram) {
            diagram->resetSelectedVariablesChanged();
        }
    }

    inline QVector<uint32_t> getSelectedTrajectories() const {
        QVector<uint32_t> selectedTrajectories;
        selectedTrajectories.resize(numTrajectoriesTotal);
        if (!highlightedTrajectories.empty()) {
            for (const auto& it : highlightedTrajectories) {
                uint32_t trajectoryIndex = it.first;
                selectedTrajectories[int(trajectoryIndex)] = true;
            }
        } else {
            // Show all trajectories as highlighted if no trajectory is selected.
            for (uint32_t& entry : selectedTrajectories) {
                entry = 1;
            }
        }
        return selectedTrajectories;
    }
    inline bool getSelectedTrajectoriesChanged() const {
        return selectedTrajectoriesChanged;
    }
    inline void resetSelectedTrajectoriesChanged() {
        selectedTrajectoriesChanged = false;
    }


    inline const QVector<QVector2D>& getVariableRanges() const { return minMaxAttributes; }

    void toggleTrajectoryHighlighted(uint32_t trajectoryIndex);
    void setParticlePosTimeStep(int newTimeStep);
    MHighlightedTrajectoriesRenderData getHighlightedTrajectoriesRenderData(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif
    inline std::shared_ptr<GL::MShaderEffect> getHighlightShaderEffect() { return shaderEffectHighlighted; }
    //inline QtExtensions::MMultiVarChartCollection& getMultiVarChartCollection() { return multiVarCharts; }

private:
    void freeStorage();
    void freeStorageSpheres();
    void recreateTubeTriangleData();
    void updateHighlightRenderData(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext);
#else
            QGLWidget *currentGLContext);
#endif

    MSceneViewGLWidget* parentSceneView = nullptr;
    MActor* actor = nullptr;
    MQtProperties *properties = nullptr;
    QtProperty *multiVarGroupProperty = nullptr;
    QVector<MTransferFunction1D*>& transferFunctionsMultiVar;

    SimilarityMetric similarityMetric = SimilarityMetric::ABSOLUTE_NCC;
    float meanMetricInfluence = 0.5f;
    float stdDevMetricInfluence = 0.25f;
    int numBins = 10;
    bool showMinMaxValue = true;
    bool useMaxForSensitivity = true;
    bool trimNanRegions = true;
    SubsequenceMatchingTechnique subsequenceMatchingTechnique = SubsequenceMatchingTechnique::SPRING;
    float springEpsilon = 10.0f;
    float backgroundOpacity = 0.85f;
    DiagramNormalizationMode diagramNormalizationMode = DiagramNormalizationMode::GLOBAL_MIN_MAX;
    float textSize = 8.0f;
    float diagramUpscalingFactor = 1.0f;

    GLuint textureUnit;
    MFilteredTrajectories baseTrajectories;
    QVector<QVector2D> minMaxAttributes;
    int timeStep = 0;
    bool useVariableToolTip = true;

    QString varName;
    uint32_t selectedOutputIdx = 0;

    float lineRadius = 0.0f;
    int numCircleSubdivisions = 8;
    QVector<QVector<QVector3D>> trajectories;
    QVector<QVector<float>> trajectoryPointTimeSteps;
    QVector<uint32_t> selectedTrajectoryIndices;
    int numTrajectoriesTotal = 0;

    std::vector<uint32_t> triangleIndices;
    std::vector<QVector3D> vertexPositions;
    std::vector<uint32_t> vertexTrajectoryIndices;
    std::vector<float> vertexTimeSteps;

    MultiVarFocusRenderMode focusRenderMode;
    bool renderSpheres = false;
    bool targetVariableAndSensitivity = false;

#ifdef USE_EMBREE
    RTCDevice device = nullptr;

    RTCScene scene = nullptr;
    RTCGeometry tubeMeshGeometry = nullptr;
    unsigned int tubeMeshGeometryId = 0;

    RTCScene sceneSpheres = nullptr;
    size_t cachedNumSpheres = 0;
    QVector<QVector4D> cachedSpherePositions;
    QVector<QVector4D> cachedEntrancePoints;
    QVector<QVector4D> cachedExitPoints;
    QVector<LineElementIdData> cachedLineElementIds;
    float cachedSphereRadius = 0.0f;
    RTCGeometry spheresGeometry = nullptr;
    unsigned int spheresGeometryId = 0;
    QVector4D* spherePointPointer = nullptr;

    bool loaded = false;
    bool loadedSpheres = false;
#endif

    // Highlighted trajectories.
    void updateDiagramData();
    std::map<uint32_t, QColor> highlightedTrajectories;
    bool selectedTrajectoriesChanged = true;
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
            QString("multivartrajectories_index_buffer_highlighted_#%1").arg(getID());
    const QString vertexPositionBufferHighlightedID =
            QString("multivartrajectories_vertex_position_buffer_highlighted_#%1").arg(getID());
    const QString vertexColorBufferHighlightedID =
            QString("multivartrajectories_vertex_color_buffer_highlighted_#%1").arg(getID());
    MHighlightedTrajectoriesRenderData highlightedTrajectoriesRenderData;
    MDiagramBase* diagram = nullptr;
    MDiagramBase* oldDiagram = nullptr;
    bool needsInitializationBeforeRendering = false;
    DiagramDisplayType diagramDisplayType = DiagramDisplayType::CURVE_PLOT_VIEW;
    //QtExtensions::MMultiVarChartCollection multiVarCharts;
    //QtExtensions::MRadarChart* radarChart;
    std::vector<std::string> variableNames;
    size_t numVars = 0;
    QVector<uint32_t> selectedVariableIndices;
    MTransferFunction1D*& diagramTransferFunction;

    // Used for aligning warm conveyor belt trajectories based on their ascension or height.
    TrajectorySyncMode trajectorySyncMode = TrajectorySyncMode::TIMESTEP;
    int timeAfterAscentIndex = -1;
    std::vector<int> ascentTimeStepIndices;
    int minAscentTimeStepIndex = 0;
    int maxAscentTimeStepIndex = 0;
};

}

#endif //MET_3D_TRAJECTORYPICKING_H
