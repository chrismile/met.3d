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
#include "aabb2.h"
#include "radarchart.h"

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

MRadarChart::MRadarChart(GLint textureUnit) : MDiagramBase(textureUnit) {
}

void MRadarChart::initialize() {
    borderSizeX = 90;
    borderSizeY = textMode == TextMode::HORIZONTAL ? 30 + float(variableNames.size()) / 2.0f : 110;
    chartRadius = 200;
    chartHoleRadius = 0;
    windowWidth = (chartRadius + borderSizeX) * 2.0f;
    windowHeight = (chartRadius + borderSizeY) * 2.0f;

    MDiagramBase::initialize();
}

void MRadarChart::onWindowSizeChanged() {
    const float minChartRadius = 100.0f;
    float oldWidth = windowWidth;
    float oldHeight = windowHeight;
    windowWidth = std::max(windowWidth, (minChartRadius + borderSizeX) * 2.0f);
    windowHeight = std::max(windowHeight, (minChartRadius + borderSizeY) * 2.0f);
    if ((getResizeDirection() & ResizeDirection::LEFT) != 0) {
        setWindowOffsetX(getWindowOffsetX() + (oldWidth - windowWidth) * getScaleFactor());
    }
    if ((getResizeDirection() & ResizeDirection::BOTTOM) != 0) {
        setWindowOffsetY(getWindowOffsetY() + (oldHeight - windowHeight) * getScaleFactor());
    }
    chartRadius = std::min(windowWidth * 0.5f - borderSizeX, windowHeight * 0.5f - borderSizeY);
    chartRadius = std::max(chartRadius, 100.0f);
    chartHoleRadius = chartRadius / 4.0f;
    MDiagramBase::onWindowSizeChanged();
}

void MRadarChart::setData(
        const std::vector<std::string>& variableNames,
        const std::vector<std::vector<float>>& variableValuesPerTrajectory) {
    this->highlightColors = predefinedColors;
    this->variableNames = variableNames;
    this->variableValuesPerTrajectory = variableValuesPerTrajectory;
    numVariables = variableNames.size();
    onWindowSizeChanged();
}

void MRadarChart::setData(
        const std::vector<std::string>& variableNames,
        const std::vector<std::vector<float>>& variableValuesPerTrajectory,
        const std::vector<QColor>& highlightColors) {
    this->highlightColors = highlightColors;
    this->variableNames = variableNames;
    this->variableValuesPerTrajectory = variableValuesPerTrajectory;
    numVariables = variableNames.size();
    onWindowSizeChanged();
}

QVector3D MRadarChart::transferFunction(float value) {
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

void MRadarChart::drawRadarLine(const QVector2D& center, int trajectoryIdx) {
    QColor circleFillColorQt = highlightColors.at(trajectoryIdx % highlightColors.size());
    //QVector3D hsvColor = rgbToHSV(circleFillColorSgl.getFloatColorRGB());
    //hsvColor.g *= 0.5f;
    //QVector3D rgbColor = hsvToRGB(hsvColor);
    QVector3D circleColorVec;
    qreal r, g, b;
    circleFillColorQt.getRgbF(&r, &g, &b);
    QVector3D rgbColor = mix(QVector3D(1.0f, 1.0f, 1.0f), QVector3D(r, g, b), 0.7f);
    NVGcolor circleFillColor = nvgRGBAf(rgbColor.x(), rgbColor.y(), rgbColor.z(), 0.25f);
    NVGcolor circleStrokeColor = nvgRGBf(rgbColor.x(), rgbColor.y(), rgbColor.z());

    nvgBeginPath(vg);
    for (size_t varIdx = 0; varIdx < numVariables; varIdx++) {
        float varValue = variableValuesPerTrajectory.at(trajectoryIdx).at(varIdx);
        float radius = varValue * (chartRadius - chartHoleRadius) + chartHoleRadius;
        float angle = float(varIdx) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
        QVector2D point(center.x() + std::cos(angle) * radius, center.y() + std::sin(angle) * radius);
        if (varIdx == 0) {
            nvgMoveTo(vg, point.x(), point.y());
        } else {
            nvgLineTo(vg, point.x(), point.y());
        }
    }
    nvgClosePath(vg);

    if (chartHoleRadius > 0.0f) {
        nvgCircle(vg, center.x(), center.y(), chartHoleRadius);
        nvgPathWinding(vg, NVG_HOLE);
    }

    nvgFillColor(vg, circleFillColor);
    nvgFill(vg);
    nvgLineJoin(vg, NVG_ROUND);
    nvgStrokeWidth(vg, 2.0f);
    nvgStrokeColor(vg, circleStrokeColor);
    nvgStroke(vg);
}

void MRadarChart::drawPieSliceTextHorizontal(const NVGcolor& textColor, const QVector2D& center, int varIdx) {
    float radius = 1.0f * chartRadius + 10;
    float angleCenter = float(varIdx) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
    QVector2D circlePoint(center.x() + std::cos(angleCenter) * radius, center.y() + std::sin(angleCenter) * radius);

    float dirX = clamp(std::cos(angleCenter) * 2.0f, -1.0f, 1.0f);
    float dirY = clamp(std::sin(angleCenter) * 2.0f, -1.0f, 1.0f);
    //float scaleFactorText = float(variableNames.size()) / 20.0f * sign(dirY) * std::pow(std::sin(angleCenter - float(M_PI)), 10.0f);
    float scaleFactorText = 0.0f;

    const char* text = variableNames.at(varIdx).c_str();
    QVector2D bounds[2];
    nvgFontSize(vg, variableNames.size() > 50 ? 7.0f : 10.0f);
    nvgFontFace(vg, "sans");
    nvgTextBounds(vg, 0, 0, text, nullptr, &bounds[0][0]);
    QVector2D textSize = bounds[1] - bounds[0];

    QVector2D textPosition(circlePoint.x(), circlePoint.y());
    textPosition[0] += textSize.x() * (dirX - 1) * 0.5f;
    textPosition[1] += textSize.y() * ((dirY - 1) * 0.5f + scaleFactorText);

    nvgTextAlign(vg,NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    if (std::find(selectedVariableIndices.begin(), selectedVariableIndices.end(), varIdx) != selectedVariableIndices.end()) {
        nvgFontBlur(vg, 1);
        nvgFillColor(vg, nvgRGBA(255, 0, 0, 255));
        nvgText(vg, textPosition.x(), textPosition.y(), text, nullptr);
        nvgFontBlur(vg, 0);
    }
    nvgFillColor(vg, textColor);
    nvgText(vg, textPosition.x(), textPosition.y(), text, nullptr);
}

void MRadarChart::drawPieSliceTextRotated(const NVGcolor& textColor, const QVector2D& center, int varIdx) {
    nvgSave(vg);

    float radius = 1.0f * chartRadius + 10;
    float angleCenter = float(varIdx) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
    QVector2D circlePoint(center.x() + std::cos(angleCenter) * radius, center.y() + std::sin(angleCenter) * radius);

    const char* text = variableNames.at(varIdx).c_str();
    nvgFontSize(vg, variableNames.size() > 50 ? 8.0f : 10.0f);
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
        nvgTranslate(vg, (bounds[0][0] + bounds[1][0]) / 2.0f, (bounds[0][1] + bounds[1][1]) / 2.0f);
        nvgRotate(vg, M_PI);
        nvgTranslate(vg, -(bounds[0][0] + bounds[1][0]) / 2.0f, -(bounds[0][1] + bounds[1][1]) / 2.0f);
    }

    if (std::find(selectedVariableIndices.begin(), selectedVariableIndices.end(), varIdx) != selectedVariableIndices.end()) {
        nvgFontBlur(vg, 1);
        nvgFillColor(vg, nvgRGBA(255, 0, 0, 255));
        nvgText(vg, textPosition.x(), textPosition.y(), text, nullptr);
        nvgFontBlur(vg, 0);
    }
    nvgFillColor(vg, textColor);
    nvgText(vg, textPosition.x(), textPosition.y(), text, nullptr);

    nvgRestore(vg);
}

void MRadarChart::drawDashedCircle(
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

void MRadarChart::renderBase() {
    NVGcolor textColor = nvgRGBA(0, 0, 0, 255);
    NVGcolor circleFillColor = nvgRGBA(180, 180, 180, 70);
    NVGcolor circleStrokeColor = nvgRGBA(120, 120, 120, 120);
    NVGcolor dashedCircleStrokeColor = nvgRGBA(70, 70, 70, 120);

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


    const size_t numTrajectories = variableValuesPerTrajectory.size();
    for (size_t varIdx = 0; varIdx < numTrajectories; varIdx++) {
        drawRadarLine(center, int(varIdx));
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
}

void MRadarChart::mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    int viewportHeight = sceneView->getViewPortHeight();
    QVector2D mousePosition(float(event->x()), float(viewportHeight - event->y() - 1));
    mousePosition -= QVector2D(getWindowOffsetX(), getWindowOffsetY());
    mousePosition /= getScaleFactor();
    mousePosition[1] = windowHeight - mousePosition.y();

    // Let the user click on variables to select different variables to show in linked views.
    AABB2 windowAabb(
            QVector2D(borderWidth, borderWidth),
            QVector2D(windowWidth - 2.0f * borderWidth, windowHeight - 2.0f * borderWidth));
    if (windowAabb.contains(mousePosition) && event->button() == Qt::MouseButton::LeftButton) {
        QVector2D center(windowWidth / 2.0f, windowHeight / 2.0f);

        nvgFontSize(vg, variableNames.size() > 50 ? 8.0f : 10.0f);
        nvgFontFace(vg, "sans");
        if (textMode == TextMode::HORIZONTAL) {
            nvgTextAlign(vg,NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        } else {
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }

        for (size_t varIdx = 0; varIdx < numVariables; varIdx++) {
            QVector2D bounds[2];
            QVector2D transformedMousePosition;
            if (textMode == TextMode::HORIZONTAL) {
                float radius = 1.0f * chartRadius + 10;
                float angleCenter = float(varIdx) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
                QVector2D circlePoint(center.x() + std::cos(angleCenter) * radius, center.y() + std::sin(angleCenter) * radius);

                float dirX = clamp(std::cos(angleCenter) * 2.0f, -1.0f, 1.0f);
                float dirY = clamp(std::sin(angleCenter) * 2.0f, -1.0f, 1.0f);
                float scaleFactorText = 0.0f;

                const char* text = variableNames.at(varIdx).c_str();
                QVector2D boundsLocal[2];
                nvgFontSize(vg, variableNames.size() > 50 ? 7.0f : 10.0f);
                nvgFontFace(vg, "sans");
                nvgTextBounds(vg, 0, 0, text, nullptr, &boundsLocal[0][0]);
                QVector2D textSize = boundsLocal[1] - boundsLocal[0];

                QVector2D textPosition(circlePoint.x(), circlePoint.y());
                textPosition[0] += textSize.x() * (dirX - 1) * 0.5f;
                textPosition[1] += textSize.y() * ((dirY - 1) * 0.5f + scaleFactorText);

                nvgTextBounds(
                        vg, textPosition.x(), textPosition.y(),
                        variableNames.at(varIdx).c_str(), nullptr, &bounds[0][0]);
                transformedMousePosition = mousePosition;
            } else {
                float radius = 1.0f * chartRadius + 10;
                float angleCenter = float(varIdx) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
                QVector2D circlePoint(
                        center.x() + std::cos(angleCenter) * radius,
                        center.y() + std::sin(angleCenter) * radius);
                QVector2D textPosition(circlePoint.x(), circlePoint.y());

                nvgSave(vg);

                QVector2D boundsLocal[2];
                nvgTextBounds(
                        vg, textPosition.x(), textPosition.y(),
                        variableNames.at(varIdx).c_str(), nullptr, &boundsLocal[0][0]);

                nvgTranslate(vg, textPosition.x(), textPosition.y());
                nvgRotate(vg, angleCenter);
                nvgTranslate(vg, -textPosition.x(), -textPosition.y());
                if (std::cos(angleCenter) < -1e-5f) {
                    nvgTranslate(
                            vg, (boundsLocal[0].x() + boundsLocal[1].x()) / 2.0f,
                            (boundsLocal[0].y() + boundsLocal[1].y()) / 2.0f);
                    nvgRotate(vg, M_PI);
                    nvgTranslate(
                            vg, -(boundsLocal[0].x() + boundsLocal[1].x()) / 2.0f,
                            -(boundsLocal[0].y() + boundsLocal[1].y()) / 2.0f);
                }
                nvgTextBounds(
                        vg, textPosition.x(), textPosition.y(),
                        variableNames.at(varIdx).c_str(), nullptr, &bounds[0][0]);

                float xform[6] = {};
                float xformInv[6] = {};
                nvgCurrentTransform(vg, xform);
                nvgTransformInverse(xformInv, xform);
                nvgTransformPoint(
                        &transformedMousePosition[0], &transformedMousePosition[1],
                        xformInv, mousePosition[0], mousePosition[1]);
                nvgRestore(vg);

                QMatrix4x4 trafo{};
                trafo.translate(textPosition[0], textPosition[1]);
                trafo.rotate(angleCenter / float(M_PI) * 180.0f, 0.0f, 0.0f, 1.0f);
                trafo.translate(-textPosition[0], -textPosition[1]);
                if (std::cos(angleCenter) < -1e-5f) {
                    trafo.translate((bounds[0][0] + bounds[1][0]) / 2.0f, (bounds[0][1] + bounds[1][1]) / 2.0f);
                    trafo.rotate(180.0f, 0.0f, 0.0f, 1.0f);
                    trafo.translate(-(bounds[0][0] + bounds[1][0]) / 2.0f, -(bounds[0][1] + bounds[1][1]) / 2.0f);
                }
                QMatrix4x4 trafoInv = trafo.inverted();
                QVector4D trafoPt = trafoInv.map(QVector4D(
                        mousePosition.x(), mousePosition.y(), 0.0f, 1.0f));
                transformedMousePosition = QVector2D(trafoPt.x(), trafoPt.y());
            }

            AABB2 textAabb(bounds[0], bounds[1]);
            if (textAabb.contains(transformedMousePosition)) {
                if (std::find(selectedVariableIndices.begin(), selectedVariableIndices.end(), varIdx) == selectedVariableIndices.end()) {
                    selectedVariableIndices.push_back(varIdx);
                } else {
                    selectedVariableIndices.erase(std::remove(
                            selectedVariableIndices.begin(), selectedVariableIndices.end(), varIdx), selectedVariableIndices.end());
                }
                selectedVariablesChanged = true;
            }
        }
    }
}

}
