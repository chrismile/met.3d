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
#ifndef MET_3D_RADARCHART_H
#define MET_3D_RADARCHART_H

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

class MRadarChart : public MDiagramBase {
public:
    explicit MRadarChart(GLint textureUnit);
    DiagramType getDiagramType() override { return DiagramType::RADAR_CHART; }
    void initialize() override;

    /**
     * @param variableNames The names of the variables to be displayed.
     * @param variableValuesPerTrajectory An array with dimensions: Trajectory - Variable.
     */
    void setData(
            const std::vector<std::string>& variableNames,
            const std::vector<std::vector<float>>& variableValuesPerTrajectory);
    void setData(
            const std::vector<std::string>& variableNames,
            const std::vector<std::vector<float>>& variableValuesPerTrajectory,
            const std::vector<QColor>& highlightColors);

    void mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) override;

protected:
    virtual bool hasData() override { return !variableValuesPerTrajectory.empty(); }
    void renderBase() override;

private:
    QVector3D transferFunction(float value);
    void drawRadarLine(const QVector2D& center, int trajectoryIdx);
    void drawPieSliceTextHorizontal(const NVGcolor& textColor, const QVector2D& center, int varIdx);
    void drawPieSliceTextRotated(const NVGcolor& textColor, const QVector2D& center, int indvarIdxex);
    void drawDashedCircle(
            const NVGcolor& circleColor, const QVector2D& center, float radius,
            int numDashes, float dashSpaceRatio, float thickness);

    enum class TextMode {
        HORIZONTAL, ROTATED
    };
    TextMode textMode = TextMode::ROTATED;

    float chartRadius;
    float chartHoleRadius;

    std::vector<std::string> variableNames;
    std::vector<std::vector<float>> variableValuesPerTrajectory;
    std::vector<QColor> highlightColors;
};

}

#endif //MET_3D_RADARCHART_H
