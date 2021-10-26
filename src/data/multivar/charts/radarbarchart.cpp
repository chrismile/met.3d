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
// standard library imports
#include <set>

// related third party imports
#include "src/data/multivar/nanovg/nanovg.h"

// local application imports
#include "../helpers.h"
#include "radarbarchart.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Met3D {

static const std::vector<QColor> predefinedColors = {
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

MRadarBarChart::MRadarBarChart(GLint textureUnit, bool equalArea)
        : MDiagramBase(textureUnit), equalArea(equalArea) {
}

void MRadarBarChart::initialize() {
    borderSizeX = 90;
    borderSizeY = textMode == TextMode::HORIZONTAL ? 30 + float(variableNames.size()) / 2.0f : 110;
    chartRadius = 200;
    chartHoleRadius = 50;
    windowWidth = (chartRadius + borderSizeX) * 2.0f;
    windowHeight = (chartRadius + borderSizeY) * 2.0f;

    MDiagramBase::initialize();
}

void MRadarBarChart::setDataTimeIndependent(
        const std::vector<std::string>& variableNames,
        const std::vector<float>& variableValues) {
    useTimeDependentData = false;
    this->variableNames = variableNames;
    this->variableValues = variableValues;
    onWindowSizeChanged();
}

void MRadarBarChart::setDataTimeDependent(
        const std::vector<std::string>& variableNames,
        const std::vector<std::vector<float>>& variableValuesTimeDependent) {
    useTimeDependentData = true;
    this->variableNames = variableNames;
    this->variableValuesTimeDependent = variableValuesTimeDependent;
    onWindowSizeChanged();
}

QVector3D MRadarBarChart::transferFunction(float value) {
    std::vector<QColor> colorPoints = {
            QColor(59, 76, 192),
            QColor(144, 178, 254),
            QColor(220, 220, 220),
            QColor(245, 156, 125),
            QColor(180, 4, 38)
    };
    int stepLast = clamp(int(std::floor(value / 0.25f)), 0, 4);
    int stepNext = clamp(int(std::ceil(value / 0.25f)), 0, 4);
    float t = fract(value / 0.25f);
    qreal r, g, b;
    colorPoints.at(stepLast).getRgbF(&r, &g, &b);
    QVector3D colorLast{float(r), float(g), float(b)};
    colorPoints.at(stepNext).getRgbF(&r, &g, &b);
    QVector3D colorNext{float(r), float(g), float(b)};
    return mix(colorLast, colorNext, t);
}

void MRadarBarChart::drawPieSlice(const QVector2D& center, int varIdx) {
    float varValue = variableValues.at(varIdx);
    if (varValue <= std::numeric_limits<float>::epsilon()) {
        return;
    }
    float radius = varValue * (chartRadius - chartHoleRadius) + chartHoleRadius;

    QColor circleFillColorQt = predefinedColors.at(varIdx % predefinedColors.size());
    //QVector3D hsvColor = rgbToHSV(circleFillColorSgl.getFloatColorRGB());
    //hsvColor.g *= 0.5f;
    //QVector3D rgbColor = hsvToRGB(hsvColor);
    QVector3D circleColorVec;
    qreal r, g, b;
    circleFillColorQt.getRgbF(&r, &g, &b);
    QVector3D rgbColor = mix(QVector3D(1.0f, 1.0f, 1.0f), QVector3D(r, g, b), 0.7f);
    NVGcolor circleFillColor = nvgRGBf(rgbColor.x(), rgbColor.y(), rgbColor.z());
    NVGcolor circleStrokeColor = nvgRGBA(0, 0, 0, 255);

    nvgBeginPath(vg);
    if (variableNames.size() == 1) {
        nvgCircle(vg, center.x(), center.y(), radius);
    } else {
        float angleStart = float(varIdx) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
        float angleEnd = float(varIdx + 1) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;

        if (chartHoleRadius > 0.0f) {
            nvgArc(vg, center.x(), center.y(), chartHoleRadius, angleEnd, angleStart, NVG_CCW);
            nvgLineTo(vg, center.x() + std::cos(angleStart) * radius, center.y() + std::sin(angleStart) * radius);
            nvgArc(vg, center.x(), center.y(), radius, angleStart, angleEnd, NVG_CW);
            nvgLineTo(
                    vg, center.x() + std::cos(angleEnd) * chartHoleRadius,
                    center.y() + std::sin(angleEnd) * chartHoleRadius);
        } else {
            nvgMoveTo(vg, center.x(), center.y());
            nvgLineTo(vg, center.x() + std::cos(angleStart) * radius, center.y() + std::sin(angleStart) * radius);
            nvgArc(vg, center.x(), center.y(), radius, angleStart, angleEnd, NVG_CW);
            nvgLineTo(vg, center.x(), center.y());
        }
    }

    nvgFillColor(vg, circleFillColor);
    nvgFill(vg);
    nvgStrokeWidth(vg, 0.75f);
    nvgStrokeColor(vg, circleStrokeColor);
    nvgStroke(vg);
}

void MRadarBarChart::drawEqualAreaPieSlices(const QVector2D& center, int varIdx) {
    const size_t numTimesteps = variableValuesTimeDependent.size();
    float radiusInner = chartHoleRadius;
    for (int timeStepIdx = 0; timeStepIdx < numTimesteps; timeStepIdx++) {
        float varValue = variableValuesTimeDependent.at(timeStepIdx).at(varIdx);
        float radiusOuter;
        if (equalArea) {
            radiusOuter = std::sqrt((chartRadius*chartRadius - chartHoleRadius*chartHoleRadius)
                                    / float(numTimesteps) + radiusInner*radiusInner);
        } else {
            radiusOuter =
                    chartHoleRadius + (chartRadius - chartHoleRadius) * float(timeStepIdx + 1) / float(numTimesteps);
        }

        QVector3D rgbColor = transferFunction(varValue);
        NVGcolor circleFillColor = nvgRGBf(rgbColor.x(), rgbColor.y(), rgbColor.z());
        NVGcolor circleStrokeColor = nvgRGBA(0, 0, 0, 255);

        nvgBeginPath(vg);
        if (variableNames.size() == 1) {
            // TODO
            //nvgCircle(vg, center.x(), center.y(), radius);
        } else {
            float angleStart = float(varIdx) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
            float angleEnd = float(varIdx + 1) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;

            if (radiusInner > 0.0f) {
                nvgArc(vg, center.x(), center.y(), radiusInner, angleEnd, angleStart, NVG_CCW);
                nvgLineTo(vg, center.x() + std::cos(angleStart) * radiusOuter, center.y() + std::sin(angleStart) * radiusOuter);
                nvgArc(vg, center.x(), center.y(), radiusOuter, angleStart, angleEnd, NVG_CW);
                nvgLineTo(
                        vg, center.x() + std::cos(angleEnd) * radiusInner,
                        center.y() + std::sin(angleEnd) * radiusInner);
            } else {
                nvgMoveTo(vg, center.x(), center.y());
                nvgLineTo(vg, center.x() + std::cos(angleStart) * radiusOuter, center.y() + std::sin(angleStart) * radiusOuter);
                nvgArc(vg, center.x(), center.y(), radiusOuter, angleStart, angleEnd, NVG_CW);
                nvgLineTo(vg, center.x(), center.y());
            }
        }

        nvgFillColor(vg, circleFillColor);
        nvgFill(vg);
        nvgStrokeWidth(vg, 0.75f);
        nvgStrokeColor(vg, circleStrokeColor);
        nvgStroke(vg);

        radiusInner = radiusOuter;
    }
}

void MRadarBarChart::drawPieSliceTextHorizontal(const NVGcolor& textColor, const QVector2D& center, int varIdx) {
    float radius = 1.0f * chartRadius + 10;
    float angleCenter = (float(varIdx) + 0.5f) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
    QVector2D circlePoint(center.x() + std::cos(angleCenter) * radius, center.y() + std::sin(angleCenter) * radius);

    float dirX = clamp(std::cos(angleCenter) * 2.0f, -1.0f, 1.0f);
    float dirY = clamp(std::sin(angleCenter) * 2.0f, -1.0f, 1.0f);
    float scaleFactorText = 0.0f;

    const char* text = variableNames.at(varIdx).c_str();
    QVector2D bounds[2];
    nvgFontSize(vg, variableNames.size() > 50 ? 7.0f : 12.0f);
    nvgFontFace(vg, "sans");
    nvgTextBounds(vg, 0, 0, text, nullptr, &bounds[0][0]);
    QVector2D textSize = bounds[1] - bounds[0];

    QVector2D textPosition(circlePoint.x(), circlePoint.y());
    textPosition[0] += textSize.x() * (dirX - 1) * 0.5f;
    textPosition[1] += textSize.y() * ((dirY - 1) * 0.5f + scaleFactorText);

    nvgTextAlign(vg,NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, textColor);
    nvgText(vg, textPosition.x(), textPosition.y(), text, nullptr);
}

void MRadarBarChart::drawPieSliceTextRotated(const NVGcolor& textColor, const QVector2D& center, int varIdx) {
    nvgSave(vg);

    float radius = 1.0f * chartRadius + 10;
    float angleCenter = (float(varIdx) + 0.5f) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
    QVector2D circlePoint(center.x() + std::cos(angleCenter) * radius, center.y() + std::sin(angleCenter) * radius);

    const char* text = variableNames.at(varIdx).c_str();
    nvgFontSize(vg, variableNames.size() > 50 ? 8.0f : 12.0f);
    nvgFontFace(vg, "sans");

    QVector2D textPosition(circlePoint.x(), circlePoint.y());

    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    QVector2D bounds[2];
    nvgTextBounds(vg, textPosition.x(), textPosition.y(), text, nullptr, &bounds[0][0]);
    QVector2D textSize = bounds[1] - bounds[0];

    nvgTranslate(vg, textPosition.x(), textPosition.y());
    nvgRotate(vg, angleCenter);
    nvgTranslate(vg, -textPosition.x(), -textPosition.y());
    nvgFillColor(vg, textColor);
    if (std::cos(angleCenter) < -1e-5f) {
        nvgTranslate(vg, (bounds[0].x() + bounds[1].x()) / 2.0f, (bounds[0].y() + bounds[1].y()) / 2.0f);
        nvgRotate(vg, M_PI);
        nvgTranslate(vg, -(bounds[0].x() + bounds[1].x()) / 2.0f, -(bounds[0].y() + bounds[1].y()) / 2.0f);
    }
    nvgText(vg, textPosition.x(), textPosition.y(), text, nullptr);

    nvgRestore(vg);
}

void MRadarBarChart::drawDashedCircle(
        const NVGcolor& circleColor, const QVector2D& center, float radius,
        int numDashes, float dashSpaceRatio, float thickness) {
    const float radiusLower = radius - thickness / 2.0f;
    const float radiusUpper = radius + thickness / 2.0f;
    const float dashSize = 2.0f * float(M_PI) * dashSpaceRatio / float(numDashes);

    nvgBeginPath(vg);
    for (int i = 0; i < numDashes; i++) {
        float angleStart = 2.0f * float(M_PI) * float(i) / float(numDashes);
        float angleEnd = angleStart + dashSize;
        QVector2D startPointLower(
                center.x() + std::cos(angleStart) * radiusLower,
                center.y() + std::sin(angleStart) * radiusLower);
        QVector2D endPointUpper(
                center.x() + std::cos(angleEnd) * radiusUpper,
                center.y() + std::sin(angleEnd) * radiusUpper);
        nvgMoveTo(vg, startPointLower.x(), startPointLower.y());
        nvgArc(vg, center.x(), center.y(), radiusLower, angleStart, angleEnd, NVG_CW);
        nvgLineTo(vg, endPointUpper.x(), endPointUpper.y());
        nvgArc(vg, center.x(), center.y(), radiusUpper, angleEnd, angleStart, NVG_CCW);
        nvgLineTo(vg, startPointLower.x(), startPointLower.y());
    }
    nvgFillColor(vg, circleColor);
    nvgFill(vg);
}


void MRadarBarChart::renderBase() {
    NVGcolor textColor = nvgRGBA(0, 0, 0, 255);
    NVGcolor circleFillColor = nvgRGBA(180, 180, 180, 70);
    NVGcolor circleStrokeColor = nvgRGBA(120, 120, 120, 120);
    NVGcolor dashedCircleStrokeColor = nvgRGBA(120, 120, 120, 120);

    // Render the central radial chart area.
    QVector2D center(windowWidth / 2.0f, windowHeight / 2.0f);
    nvgBeginPath(vg);
    nvgCircle(vg, center.x(), center.y(), chartRadius);
    if (chartHoleRadius > 0.0f) {
        nvgCircle(vg, center.x(), center.y(), chartHoleRadius);
        nvgPathWinding(vg, NVG_HOLE);
    }
    nvgFillColor(vg, circleFillColor);
    nvgFill(vg);
    nvgStrokeColor(vg, circleStrokeColor);
    nvgStroke(vg);

    if (!useTimeDependentData) {
        // Dotted lines at 0.25, 0.5 and 0.75.
        drawDashedCircle(
                dashedCircleStrokeColor, center, chartHoleRadius + (chartRadius - chartHoleRadius) * 0.25f,
                75, 0.5f, 0.25f);
        drawDashedCircle(
                dashedCircleStrokeColor, center, chartHoleRadius + (chartRadius - chartHoleRadius) * 0.50f,
                75, 0.5f, 0.75f);
        drawDashedCircle(
                dashedCircleStrokeColor, center, chartHoleRadius + (chartRadius - chartHoleRadius) * 0.75f,
                75, 0.5f, 0.25f);
    }


    size_t numVariables = variableNames.size();
    if (useTimeDependentData) {
        for (size_t varIdx = 0; varIdx < numVariables; varIdx++) {
            drawEqualAreaPieSlices(center, int(varIdx));
        }
    } else {
        for (size_t varIdx = 0; varIdx < numVariables; varIdx++) {
            drawPieSlice(center, int(varIdx));
        }
    }
    if (textMode == TextMode::HORIZONTAL) {
        for (size_t varIdx = 0; varIdx < numVariables; varIdx++) {
            drawPieSliceTextHorizontal(textColor, center, int(varIdx));
        }
    } else {
        for (size_t varIdx = 0; varIdx < numVariables; varIdx++) {
            drawPieSliceTextRotated(textColor, center, int(varIdx));
        }
    }

    // Draw color legend.
    if (useTimeDependentData) {
        auto labelMap = [](float t) {
            const float EPS = 1e-5f;
            if (std::abs(t - 0.0f) < EPS) {
                return "min";
            } else if (std::abs(t - 1.0f) < EPS) {
                return "max";
            } else {
                return "";
            }
        };
        auto colorMap = [this](float t) {
            QVector3D color = transferFunction(t);
            return nvgRGBf(color.x(), color.y(), color.z());
        };
        drawColorLegend(
                textColor,
                windowWidth - colorLegendWidth - textWidthMax - 10, windowHeight - colorLegendHeight - 10,
                colorLegendWidth, colorLegendHeight, 2, 5, labelMap, colorMap);
    }
}

}
