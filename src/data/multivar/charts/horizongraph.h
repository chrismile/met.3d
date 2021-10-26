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
    void update(float dt) override;

    /**
     * @param variableNames The names of the variables to be displayed.
     * @param variableValuesArray An array with dimensions: Trajectory - Time - Variable.
     */
    void setData(
            const std::vector<std::string>& variableNames, float timeMax, float timeMin,
            const std::vector<std::vector<std::vector<float>>>& variableValuesArray);

protected:
    virtual bool hasData() override { return !variableValuesArray.empty(); }
    void renderBase() override;

private:
    QVector3D transferFunction(float value);
    void drawHorizonLines();
    void drawHorizonOutline(const NVGcolor& textColor);
    void drawLegendLeft(const NVGcolor& textColor);
    void drawLegendTop(const NVGcolor& textColor);
    void drawTicks(const NVGcolor& textColor);
    void drawScrollBar(const NVGcolor& textColor);

    void recomputeScrollThumbHeight();
    float computeWindowHeight();
    void recomputeWindowHeight();
    void recomputeFullWindowHeight();

    bool mapStdDevToColor = true;

    float horizonBarWidth, horizonBarHeight, horizonBarHeightBase;
    float textSize, textSizeLegendTop;
    float legendLeftWidth;
    float legendTopHeight;
    float offsetHorizonBarsX, offsetHorizonBarsY;
    float horizonBarMargin, horizonBarMarginBase;
    int timeStepLegendIncrement;

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
    size_t numVariables;
    std::vector<std::vector<std::vector<float>>> variableValuesArray;
    std::vector<std::vector<float>> ensembleMeanValues;
    std::vector<std::vector<float>> ensembleStdDevValues;
};

}

#endif //MET_3D_HORIZONGRAPH_H
