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
#ifndef MET_3D_RADARBARCHART_HPP
#define MET_3D_RADARBARCHART_HPP

// standard library imports
#include <string>
#include <vector>

// related third party imports
#include <QVector2D>

// local application imports
#include "actors/transferfunction1d.h"
#include "diagrambase.h"

struct NVGcontext;
struct NVGcolor;

namespace Met3D {

class MRadarBarChart : public MDiagramBase {
public:
    explicit MRadarBarChart(GLint textureUnit, MTransferFunction1D*& diagramTransferFunction, bool equalArea = true);
    DiagramType getDiagramType() override { return DiagramType::RADAR_BAR_CHART; }
    void initialize() override;

    /**
     * Sets time independent data.
     * @param variableNames The names of the variables to be displayed.
     * @param variableValues An array with dimensions: Variable.
     */
    void setDataTimeIndependent(
            const std::vector<std::string>& variableNames,
            const std::vector<float>& variableValues);

    /**
     * Sets time dependent data.
     * @param variableNames The names of the variables to be displayed.
     * @param variableValuesTimeDependent An array with dimensions: Trajectory - Variable.
     */
    void setDataTimeDependent(
            const std::vector<std::string>& variableNames,
            const std::vector<std::vector<float>>& variableValuesTimeDependent);
    void setDataTimeDependent(
            const std::vector<std::string>& variableNames,
            const std::vector<std::vector<float>>& variableValuesTimeDependent,
            const std::vector<QColor>& highlightColors);

    inline void setUseEqualArea(bool useEqualArea) { equalArea = useEqualArea; }

    void mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;

protected:
    virtual bool hasData() override {
        return useTimeDependentData ? !variableValuesTimeDependent.empty() : !variableValues.empty();
    }
    void onWindowSizeChanged() override;
    void renderBase() override;

private:
    void drawPieSlice(const QVector2D& center, int varIdx);
    void drawEqualAreaPieSlices(const QVector2D& center, int varIdx);
    void drawEqualAreaPieSlicesWithLabels(const QVector2D& center);
    void drawPieSliceTextHorizontal(const NVGcolor& textColor, const QVector2D& center, int varIdx);
    void drawPieSliceTextRotated(const NVGcolor& textColor, const QVector2D& center, int indvarIdxex);
    void drawDashedCircle(
            const NVGcolor& circleColor, const QVector2D& center, float radius,
            int numDashes, float dashSpaceRatio, float thickness);
    /**
     * Maps the variable index to the corresponding angle.
     * @param varIdxFloat The variable index.
     */
    float mapVarIdxToAngle(float varIdxFloat);

    QVector3D transferFunction(float value);
    MTransferFunction1D*& diagramTransferFunction;

    enum class TextMode {
        HORIZONTAL, ROTATED
    };
    TextMode textMode = TextMode::ROTATED;
    bool useTimeDependentData = true;
    bool equalArea = true;
    bool timeStepColorMode = true;

    float chartRadius = 0.0f;
    float chartHoleRadius = 0.0f;

    // Color legend.
    const float colorLegendWidth = 16;
    const float colorLegendHeight = 160;
    const float textWidthMax = 26;

    std::vector<std::string> variableNames;
    std::vector<float> variableValues;
    std::vector<std::vector<float>> variableValuesTimeDependent;
    std::vector<QColor> highlightColors;
};

}

#endif //MET_3D_RADARBARCHART_HPP
