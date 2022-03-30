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
#ifndef MET_3D_CURVEPLOTVIEW_H
#define MET_3D_CURVEPLOTVIEW_H

// standard library imports
#include <string>
#include <vector>

// related third party imports
#include <QVector2D>

// local application imports
#include "actors/transferfunction1d.h"
#include "../similarity/spring.h"
#include "diagrambase.h"

struct NVGcontext;
struct NVGcolor;

namespace Met3D {

enum class SimilarityMetric {
    L1_NORM, // L1 norm, also called "sum of absolute differences" (SAD).
    L2_NORM, // L2 norm, also called "sum of squared differences" (SSD) or "mean squared error" (MSE).
    NCC, // Normalized cross correlation (NCC), sometimes also called zero-normalized cross correlation (ZNCC).
    ABSOLUTE_NCC, // Absolute normalized cross correlation (NCC).
    MI, // Mutual information (MI).
    SSIM, // Structural similarity index measure (SSIM).
};
const char* const SIMILARITY_METRIC_NAMES[] = {
        "L1 norm", "L2 norm", "Normalized Cross Correlation", "Absolute Normalized Cross Correlation",
        "Mutual Information", "SSIM"
};

class MCurvePlotView : public MDiagramBase {
public:
    MCurvePlotView(GLint textureUnit, MTransferFunction1D*& diagramTransferFunction);
    DiagramType getDiagramType() override { return DiagramType::CURVE_PLOT_VIEW; }
    void initialize() override;

    /**
     * @param variableNames The names of the variables to be displayed.
     * @param variableValuesArray An array with dimensions: Trajectory - Time - Variable.
     */
    void setData(
            const std::vector<std::string>& variableNames, float timeMin, float timeMax,
            const std::vector<std::vector<std::vector<float>>>& variableValuesArray,
            bool normalizeBands);

    QVector<uint32_t> getSelectedVariableIndices() const override {
        if (targetVarIdx == std::numeric_limits<uint32_t>::max()) {
            return MDiagramBase::getSelectedVariableIndices();
        } else {
            QVector<uint32_t> _selectedVariableIndices;
            _selectedVariableIndices.reserve(int(selectedVariableIndices.size()));
            for (size_t varIdx : selectedVariableIndices) {
                if (varIdx > targetVarIdx) {
                    varIdx--;
                }
                if ((varIdx == targetVarIdx || varIdx == targetVarIdx + 1) && std::find(
                        _selectedVariableIndices.begin(), _selectedVariableIndices.end(),
                        targetVarIdx) != _selectedVariableIndices.end()) {
                    continue;
                }
                _selectedVariableIndices.push_back(varIdx);
            }
            return _selectedVariableIndices;
        }
    }
    void setSelectedVariableIndices(const QVector<uint32_t>& _selectedVariableIndices) override {
        if (targetVarIdx == std::numeric_limits<uint32_t>::max()) {
            MDiagramBase::setSelectedVariableIndices(_selectedVariableIndices);
        } else {
            selectedVariableIndices.clear();
            selectedVariableIndices.reserve(_selectedVariableIndices.size());
            for (uint32_t varIdx : _selectedVariableIndices) {
                if (varIdx > targetVarIdx) {
                    varIdx++;
                }
                selectedVariableIndices.push_back(varIdx);
            }
            selectedVariablesChanged = false;
            updateSelectedVariables();
        }
    }

    inline float getSelectedTimeStep() const { return selectedTimeStep; }
    inline void setSelectedTimeStep(float timeStep) { selectedTimeStep = timeStep; selectedTimeStepChanged = false; }
    inline bool getSelectedTimeStepChanged() const { return selectedTimeStepChanged; }
    inline void resetSelectedTimeStepChanged() { selectedTimeStepChanged = false; }

    void setSimilarityMetric(SimilarityMetric similarityMetric);
    void setMeanMetricInfluence(float meanMetricInfluence);
    void setStdDevMetricInfluence(float stdDevMetricInfluence);
    void setNumBins(int numBins);
    void sortByDescendingStdDev();
    void setShowMinMaxValue(bool show);
    void setUseMaxForSensitivity(bool useMax);
    void setSubsequenceMatchingTechnique(SubsequenceMatchingTechnique technique);
    void setSpringEpsilon(float epsilon);
    void setTextSize(float _textSize);
    void setShowSelectedVariablesFirst(bool showFirst);
    void resetVariableSorting();

    void mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;
    void mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;
    void mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;
    void wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event) override;

protected:
    bool hasData() override { return !variableValuesArray.empty(); }
    void onWindowSizeChanged() override;
    void renderBase() override;
    void updateSelectedVariables() override { resetFinalSelectedVariableIndices(); }

private:
    void drawHorizonBackground();
    void drawHorizonMatchSelections();
    void drawHorizonLines();
    void drawHorizonLinesSparse();
    void drawHorizonLinesLttb(); // Using Largest Triangle Three Buckets (LTTB) algorithm.
    void drawHorizonOutline(const NVGcolor& textColor);
    void drawSelectedTimeStepLine(const NVGcolor& textColor);
    void drawLegendLeft(const NVGcolor& textColor);
    void drawLegendTop(const NVGcolor& textColor);
    void drawTicks(const NVGcolor& textColor);
    void drawScrollBar(const NVGcolor& textColor);

    float computeTimeStepFromMousePosition(const QVector2D& mousePosition) const;
    void recomputeScrollThumbHeight();
    float computeWindowHeight();
    void recomputeWindowHeight();
    void recomputeFullWindowHeight();
    void updateTimeStepTicks();

    // Downsampling via Largest Triangle Three Buckets (LTTB).
    void computeLttb(int varIdx, int threshold);
    std::vector<std::vector<QVector2D>> lttbPointsArray;
    float lttbTimeDisplayMin = std::numeric_limits<float>::max();
    float lttbTimeDisplayMax = std::numeric_limits<float>::lowest();

    enum class EventType {
        MousePress, MouseRelease, MouseMove
    };
    void updateTimeScale(const QVector2D& mousePosition, EventType eventType, QMouseEvent* event);
    void updateTimeShift(const QVector2D& mousePosition, EventType eventType, QMouseEvent* event);

    QVector4D transferFunction(float value) const;
    MTransferFunction1D*& diagramTransferFunction;
    bool mapStdDevToColor = true;

    float horizonBarWidth, horizonBarHeight, horizonBarHeightBase;
    float textSize, textSizeLegendTop;
    float legendLeftWidth{};
    float legendTopHeight{};
    float offsetHorizonBarsX{}, offsetHorizonBarsY{};
    float horizonBarMargin, horizonBarMarginBase;
    int timeStepLegendIncrement{};
    int timeStepTicksIncrement{};

    // Scrolling and zooming.
    float maxWindowHeight = 500.0f;
    float fullWindowHeight = 0.0f;
    bool useScrollBar = false;
    bool scrollThumbHover = false;
    bool scrollThumbDrag = false;
    float scrollBarWidth = 10.0f;
    float scrollThumbPosition = 0.0f;
    float scrollThumbHeight = 0.0f;
    float scrollTranslationY = 0.0f;
    float thumbDragDelta = 0.0f;
    float zoomFactor = 1.0f;

    // Color legend.
    const float colorLegendWidth = 16;
    const float colorLegendHeightBase = 160;
    float colorLegendHeight = 160;
    const float textWidthMaxBase = 32;
    float textWidthMax = 32;

    float timeMin{};
    float timeMax{};
    std::vector<std::string> variableNames;
    size_t numTrajectories{};
    size_t numTimeSteps{};
    std::vector<std::vector<std::vector<float>>> variableValuesArray;
    std::vector<std::vector<float>> ensembleMeanValues;
    std::vector<std::vector<float>> ensembleStdDevValues;

    // For sensitivity analysis data.
    std::vector<bool> variableIsSensitivityArray;
    bool useMaxForSensitivity = true;

    float selectedTimeStep = 0.0f;
    bool selectedTimeStepChanged = false;
    float timeDisplayMin = 0.0f;
    float timeDisplayMax = 1.0f;
    float timeDisplayMinOld = 0.0f;
    float timeDisplayMaxOld = 1.0f;
    float topLegendClickPct{};
    bool isDraggingTopLegend = false;
    float clickTime = 0.0f;
    bool isDraggingTimeShift = false;

    // Drag & drop of variables.
    bool startedVariableDragging = false;
    int draggedVariableIndex = -1;

    float computeSimilarityMetric(
            int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const;
    float computeL1Norm(
            int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const;
    float computeL2Norm(
            int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const;
    float computeNCC(
            int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const;
    float computeAbsoluteNCC(
            int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const;
    float computeMI(
            int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const;
    float computeSSIM(
            int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const;
    void sortVariables(int newSortingIdx, bool forceRecompute = false);
    void resetFinalSelectedVariableIndices();
    SimilarityMetric similarityMetric = SimilarityMetric::ABSOLUTE_NCC;
    int numBins = 10;
    bool showMinMaxValue = true;
    float meanMetricInfluence = 0.5f;
    float stdDevMetricInfluence = 0.25f;
    int sortingIdx = -1;
    std::vector<size_t> sortedVariableIndices;
    std::vector<size_t> finalVariableIndices;
    bool showSelectedVariablesFirst = true;
    uint32_t targetVarIdx = std::numeric_limits<uint32_t>::max();

    // Selection of similar subsequences in the variable data.
    void startSelection(size_t varIdx, float timeStep);
    void updateSelection(float timeStep);
    void endSelection(float timeStep);
    void computeMatchSelections();
    bool isSelecting = false;
    size_t selectVarIdx = 0;
    float selectStart = -1.0f;
    float selectEnd = -1.0f;
    SubsequenceMatchingTechnique subsequenceMatchingTechnique = SubsequenceMatchingTechnique::SPRING;
    float springEpsilon = 10.0f;
    std::vector<std::vector<std::pair<float, float>>> matchSelectionsPerVariable;
};

}

#endif //MET_3D_CURVEPLOTVIEW_H
