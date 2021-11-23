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
#ifndef MET_3D_HORIZONGRAPH_H
#define MET_3D_HORIZONGRAPH_H

// standard library imports
#include <string>
#include <vector>

// related third party imports
#include <QVector2D>

// local application imports
#include "diagrambase.h"

struct NVGcontext;
struct NVGcolor;

namespace Met3D {

class MHorizonGraph : public MDiagramBase {
public:
    explicit MHorizonGraph(GLint textureUnit);
    DiagramType getDiagramType() override { return DiagramType::HORIZON_GRAPH; }
    void initialize() override;

    /**
     * @param variableNames The names of the variables to be displayed.
     * @param variableValuesArray An array with dimensions: Trajectory - Time - Variable.
     */
    void setData(
            const std::vector<std::string>& variableNames, float timeMin, float timeMax,
            const std::vector<std::vector<std::vector<float>>>& variableValuesArray);

    inline float getSelectedTimeStep() const { return selectedTimeStep; }
    inline void setSelectedTimeStep(float timeStep) { selectedTimeStep = timeStep; selectedTimeStepChanged = false; }
    inline bool getSelectedTimeStepChanged() const { return selectedTimeStepChanged; }
    inline void resetSelectedTimeStepChanged() { selectedTimeStepChanged = false; }

    void mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;
    void mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;
    void mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;
    void wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event) override;

protected:
    bool hasData() override { return !variableValuesArray.empty(); }
    void renderBase() override;

private:
    QVector3D transferFunction(float value);
    void drawHorizonBackground();
    void drawHorizonLines();
    void drawHorizonLinesSparse();
    void drawHorizonOutline(const NVGcolor& textColor);
    void drawSelectedTimeStepLine(const NVGcolor& textColor);
    void drawLegendLeft(const NVGcolor& textColor);
    void drawLegendTop(const NVGcolor& textColor);
    void drawTicks(const NVGcolor& textColor);
    void drawScrollBar(const NVGcolor& textColor);

    void recomputeScrollThumbHeight();
    float computeWindowHeight();
    void recomputeWindowHeight();
    void recomputeFullWindowHeight();
    void updateTimeStepTicks();

    enum class EventType {
        MousePress, MouseRelease, MouseMove
    };
    void updateTimeScale(const QVector2D& mousePosition, EventType eventType, QMouseEvent* event);
    void updateTimeShift(const QVector2D& mousePosition, EventType eventType, QMouseEvent* event);

    bool mapStdDevToColor = true;

    float horizonBarWidth, horizonBarHeight, horizonBarHeightBase;
    float textSize, textSizeLegendTop;
    float legendLeftWidth;
    float legendTopHeight;
    float offsetHorizonBarsX, offsetHorizonBarsY;
    float horizonBarMargin, horizonBarMarginBase;
    int timeStepLegendIncrement;
    int timeStepTicksIncrement;

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
    const float colorLegendHeight = 160;
    const float textWidthMax = 32;

    float timeMin;
    float timeMax;
    std::vector<std::string> variableNames;
    size_t numTrajectories;
    size_t numTimeSteps;
    std::vector<std::vector<std::vector<float>>> variableValuesArray;
    std::vector<std::vector<float>> ensembleMeanValues;
    std::vector<std::vector<float>> ensembleStdDevValues;

    float selectedTimeStep = 0.0f;
    bool selectedTimeStepChanged = false;
    float timeDisplayMin = 0.0f;
    float timeDisplayMax = 1.0f;
    float timeDisplayMinOld = 0.0f;
    float timeDisplayMaxOld = 1.0f;
    float topLegendClickPct;
    bool isDraggingTopLegend = false;
    float clickTime = 0.0f;
    bool isDraggingTimeShift = false;

    void sortVariables(int varIdx);
    int sortingIdx = -1;
    std::vector<size_t> sortedVariableIndices;
};

}

#endif //MET_3D_HORIZONGRAPH_H
