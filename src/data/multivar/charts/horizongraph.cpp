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
#include "horizongraph.h"

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

MHorizonGraph::MHorizonGraph(GLint textureUnit) : MDiagramBase(textureUnit) {
}

float MHorizonGraph::computeWindowHeight() {
    return
            borderSizeY * 2.0f + legendTopHeight + horizonBarMargin
            + horizonBarHeight * float(variableNames.size()) + horizonBarMargin * (float(variableNames.size()) - 1.0f);
}

void MHorizonGraph::recomputeWindowHeight() {
    windowHeight = computeWindowHeight();
}

void MHorizonGraph::recomputeFullWindowHeight() {
    fullWindowHeight = computeWindowHeight();
}

void MHorizonGraph::initialize() {
    borderSizeX = 10;
    borderSizeY = 10;

    horizonBarWidth = 400;
    horizonBarHeight = 12;
    horizonBarHeightBase = horizonBarHeight;
    horizonBarMargin = 4.0f;
    horizonBarMarginBase = horizonBarMargin;
    textSize = std::max(horizonBarHeight - horizonBarMargin, 4.0f);
    textSizeLegendTop = textSize;
    legendLeftWidth = 0.0f; //< Computed below.
    legendTopHeight = textSize * 2.0f;

    MDiagramBase::initialize();
}

void MHorizonGraph::setData(
        const std::vector<std::string>& variableNames, float timeMax, float timeMin,
        const std::vector<std::vector<std::vector<float>>>& variableValuesArray) {
    this->variableNames = variableNames;
    this->variableValuesArray = variableValuesArray;

    nvgFontSize(vg, textSize);
    nvgFontFace(vg, "sans");
    for (const std::string& variableName : variableNames) {
        QVector2D bounds[2];
        nvgTextBounds(vg, 0, 0, variableName.c_str(), nullptr, &bounds[0][0]);
        legendLeftWidth = std::max(legendLeftWidth, bounds[1].x() - bounds[0].x());
    }

    offsetHorizonBarsX = borderSizeX + legendLeftWidth + horizonBarMargin;
    offsetHorizonBarsY = borderSizeY + legendTopHeight + horizonBarMargin;

    windowWidth =
            borderSizeX * 3.0f + legendLeftWidth + horizonBarMargin + horizonBarWidth
            + colorLegendWidth + textWidthMax;
    recomputeWindowHeight();
    recomputeFullWindowHeight();
    if (windowHeight > maxWindowHeight) {
        useScrollBar = true;
        windowHeight = maxWindowHeight;
        windowWidth += scrollBarWidth;
    }

    // Compute metadata.
    this->timeMin = timeMin;
    this->timeMax = timeMax;
    if (variableValuesArray.empty()) {
        numTimeSteps = int(std::round(timeMax - timeMin)) + 1;
    } else {
        numTimeSteps = variableValuesArray.front().size();
    }
    timeStepLegendIncrement = 5;
    numTrajectories = variableValuesArray.size();
    numVariables = variableNames.size();

    // Compute ensemble mean and standard deviation values.
    ensembleMeanValues.resize(numTimeSteps);
    ensembleStdDevValues.resize(numTimeSteps);
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        std::vector<float>& meanValuesAtTime = ensembleMeanValues.at(timeStepIdx);
        std::vector<float>& stdDevsAtTime = ensembleStdDevValues.at(timeStepIdx);
        meanValuesAtTime.resize(numVariables);
        stdDevsAtTime.resize(numVariables);
        for (size_t varIdx = 0; varIdx < numVariables; varIdx++) {
            float& mean = meanValuesAtTime.at(varIdx);
            float& stdDev = stdDevsAtTime.at(varIdx);
            for (size_t trajectoryIdx = 0; trajectoryIdx < numTrajectories; trajectoryIdx++) {
                float value = variableValuesArray.at(trajectoryIdx).at(timeStepIdx).at(varIdx);
                mean += value / float(numTrajectories);
            }
            float variance = 0.0f;
            for (size_t trajectoryIdx = 0; trajectoryIdx < numTrajectories; trajectoryIdx++) {
                float value = variableValuesArray.at(trajectoryIdx).at(timeStepIdx).at(varIdx);
                float diff = value - mean;
                variance += diff * diff / float(numTrajectories - 1);
            }
            stdDev = std::sqrt(variance);
        }
    }

    onWindowSizeChanged();
}

QVector3D MHorizonGraph::transferFunction(float value) {
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

void MHorizonGraph::drawHorizonLines() {
    for (size_t varIdx = 0; varIdx < variableNames.size(); varIdx++) {
        NVGcolor strokeColor = nvgRGBA(0, 0, 0, 255);

        float lowerY = offsetHorizonBarsY + float(varIdx) * (horizonBarHeight + horizonBarMargin);
        float upperY = lowerY + horizonBarHeight;

        if (mapStdDevToColor) {
            for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps - 1; timeStepIdx++) {
                float mean0 = ensembleMeanValues.at(timeStepIdx).at(varIdx);
                float stddev0 = ensembleStdDevValues.at(timeStepIdx).at(varIdx);
                float xpos0 = offsetHorizonBarsX + float(timeStepIdx) / float(numTimeSteps - 1) * horizonBarWidth;
                float ypos0 = lowerY + (upperY - lowerY) * mean0;
                QVector3D rgbColor0 = transferFunction(clamp(stddev0 * 2.0f, 0.0f, 1.0f));
                NVGcolor fillColor0 = nvgRGBf(rgbColor0.x(), rgbColor0.y(), rgbColor0.z());

                float mean1 = ensembleMeanValues.at(timeStepIdx + 1).at(varIdx);
                float stddev1 = ensembleStdDevValues.at(timeStepIdx + 1).at(varIdx);
                float xpos1 = offsetHorizonBarsX + float(timeStepIdx + 1) / float(numTimeSteps - 1) * horizonBarWidth;
                float ypos1 = lowerY + (upperY - lowerY) * mean1;
                QVector3D rgbColor1 = transferFunction(clamp(stddev1 * 2.0f, 0.0f, 1.0f));
                NVGcolor fillColor1 = nvgRGBf(rgbColor1.x(), rgbColor1.y(), rgbColor1.z());

                nvgBeginPath(vg);
                nvgMoveTo(vg, xpos0, upperY);
                nvgLineTo(vg, xpos0, ypos0);
                nvgLineTo(vg, xpos1, ypos1);
                nvgLineTo(vg, xpos1, upperY);
                nvgClosePath(vg);

                NVGpaint paint = nvgLinearGradient(vg, xpos0, upperY, xpos1, upperY, fillColor0, fillColor1);
                nvgFillPaint(vg, paint);
                nvgFill(vg);
            }
        } else {
            QColor fillColorSgl = predefinedColors.at(varIdx % predefinedColors.size());
            qreal r, g, b;
            fillColorSgl.getRgbF(&r, &g, &b);
            QVector3D rgbColor = mix(QVector3D(1.0f, 1.0f, 1.0f), QVector3D(r, g, b), 0.7f);
            NVGcolor fillColor = nvgRGBf(rgbColor.x(), rgbColor.y(), rgbColor.z());

            nvgBeginPath(vg);
            nvgMoveTo(vg, offsetHorizonBarsX, upperY);
            for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
                float mean = ensembleMeanValues.at(timeStepIdx).at(varIdx);
                float xpos = offsetHorizonBarsX + float(timeStepIdx) / float(numTimeSteps - 1) * horizonBarWidth;
                float ypos = lowerY + (upperY - lowerY) * mean;
                nvgLineTo(vg, xpos, ypos);
            }
            nvgLineTo(vg, offsetHorizonBarsX + horizonBarWidth, upperY);
            nvgFillColor(vg, fillColor);
            nvgFill(vg);
        }

        nvgBeginPath(vg);
        for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
            float mean = ensembleMeanValues.at(timeStepIdx).at(varIdx);
            float xpos = offsetHorizonBarsX + float(timeStepIdx) / float(numTimeSteps - 1) * horizonBarWidth;
            float ypos = lowerY + (upperY - lowerY) * mean;
            if (timeStepIdx == 0) {
                nvgMoveTo(vg, xpos, ypos);
            } else {
                nvgLineTo(vg, xpos, ypos);
            }
        }
        nvgStrokeColor(vg, strokeColor);
        nvgStroke(vg);
    }
}

void MHorizonGraph::drawHorizonOutline(const NVGcolor& textColor) {
    for (size_t varIdx = 0; varIdx < variableNames.size(); varIdx++) {
        float lowerY = offsetHorizonBarsY + float(varIdx) * (horizonBarHeight + horizonBarMargin);
        nvgBeginPath(vg);
        nvgRect(vg, offsetHorizonBarsX, lowerY, horizonBarWidth, horizonBarHeight);
        nvgStrokeWidth(vg, 0.25f);
        nvgStrokeColor(vg, textColor);
        nvgStroke(vg);
    }
}

void MHorizonGraph::drawLegendLeft(const NVGcolor& textColor) {
    nvgFontSize(vg, textSize);
    nvgFontFace(vg, "sans");
    for (size_t varIdx = 0; varIdx < variableNames.size(); varIdx++) {
        float lowerY = offsetHorizonBarsY + float(varIdx) * (horizonBarHeight + horizonBarMargin);
        float upperY = lowerY + horizonBarHeight;
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, textColor);
        nvgText(
                vg, borderSizeX, lowerY + horizonBarHeight / 2.0f,
                variableNames.at(varIdx).c_str(), nullptr);
    }
}

void MHorizonGraph::drawLegendTop(const NVGcolor& textColor) {
    nvgFontSize(vg, textSizeLegendTop);
    nvgFontFace(vg, "sans");
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx += timeStepLegendIncrement) {
        float timeStep = timeMin + (timeMax - timeMin) * float(timeStepIdx) / float(numTimeSteps - 1);
        std::string timeStepName = std::to_string(int(timeStep));
        float centerX = offsetHorizonBarsX + float(timeStepIdx) / float(numTimeSteps - 1) * horizonBarWidth;
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, textColor);
        nvgText(vg, centerX, borderSizeY, timeStepName.c_str(), nullptr);
    }
}

void MHorizonGraph::drawTicks(const NVGcolor& textColor) {
    const float tickWidthBase = 0.5f;
    const float tickHeightBase = 2.0f;
    nvgBeginPath(vg);
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float thicknessFactor = 1.0f;
        if (timeStepIdx % timeStepLegendIncrement == 0) {
            thicknessFactor = 2.0f;
        }
        const float tickWidth = thicknessFactor * tickWidthBase;
        const float tickHeight = thicknessFactor * tickHeightBase;
        float centerX = offsetHorizonBarsX + float(timeStepIdx) / float(numTimeSteps - 1) * horizonBarWidth;
        nvgRect(vg, centerX - tickWidth / 2.0f, offsetHorizonBarsY - tickHeight, tickWidth, tickHeight);
    }
    nvgFillColor(vg, textColor);
    nvgFill(vg);
}

static float remap(float x, float srcStart, float srcStop, float dstStart, float dstStop) {
    float t = (x - srcStart) / (srcStop - srcStart);
    return dstStart + t * (dstStop - dstStart);
}

void MHorizonGraph::drawScrollBar(const NVGcolor& textColor) {
    NVGcolor scrollThumbColor;
    if (scrollThumbHover) {
        scrollThumbColor = nvgRGBA(90, 90, 90, 120);
    } else {
        scrollThumbColor = nvgRGBA(160, 160, 160, 120);
    }
    nvgBeginPath(vg);
    nvgRoundedRectVarying(
            vg, windowWidth - scrollBarWidth, scrollThumbPosition, scrollBarWidth - borderWidth, scrollThumbHeight,
            0.0f, borderRoundingRadius, borderRoundingRadius, 0.0f);
    nvgFillColor(vg, scrollThumbColor);
    nvgFill(vg);

    NVGcolor scrollBarColor = nvgRGBA(120, 120, 120, 120);
    nvgBeginPath(vg);
    nvgRect(vg, windowWidth - scrollBarWidth, borderWidth, 1.0f, windowHeight - 2.0f * borderWidth);
    nvgFillColor(vg, scrollBarColor);
    nvgFill(vg);
}



void MHorizonGraph::renderBase() {
    NVGcolor textColor = nvgRGBA(0, 0, 0, 255);

    drawLegendTop(textColor);
    drawTicks(textColor);

    nvgSave(vg);
    nvgScissor(
            vg, borderWidth, offsetHorizonBarsY,
            windowWidth - 2.0f * borderWidth, windowHeight - borderWidth - offsetHorizonBarsY);
    //nvgScissor(vg, borderWidth, borderWidth, windowWidth - 2.0f * borderWidth, windowHeight - 2.0f * borderWidth);
    nvgTranslate(vg, 0.0f, -scrollTranslationY);

    drawHorizonLines();
    drawHorizonOutline(textColor);
    drawLegendLeft(textColor);

    nvgRestore(vg);

    // Draw color legend.
    auto labelMap = [](float t) {
        return getNiceNumberString(t * 0.5f, 4);
    };
    auto colorMap = [this](float t) {
        QVector3D color = transferFunction(t);
        return nvgRGBf(color.x(), color.y(), color.z());
    };
    drawColorLegend(
            textColor,
            windowWidth - colorLegendWidth - textWidthMax - 10 - (useScrollBar ? scrollBarWidth : 0),
            windowHeight - colorLegendHeight - 10,
            colorLegendWidth, colorLegendHeight, 2, 5, labelMap, colorMap,
            "\u03C3");

    if (useScrollBar) {
        drawScrollBar(textColor);
    }
}

void MHorizonGraph::recomputeScrollThumbHeight() {
    scrollThumbHeight = windowHeight / fullWindowHeight * (windowHeight - 2.0f * borderWidth);
}

void MHorizonGraph::update(float dt) {
    /*QVector2D mousePosition(sgl::Mouse->getX(), sgl::Mouse->getY());
    mousePosition.y = float(sgl::AppSettings::get()->getMainWindow()->getHeight()) - mousePosition.y;
    mousePosition -= QVector2D(getWindowOffsetX(), getWindowOffsetY());
    mousePosition /= getScaleFactor();
    mousePosition.y = windowHeight - mousePosition.y;

    sgl::AABB2 windowAabb(
            QVector2D(borderWidth),
            QVector2D(windowWidth - 2.0f * borderWidth, windowHeight - 2.0f * borderWidth));
    if (windowAabb.contains(mousePosition) && (sgl::Keyboard->getModifier() & KMOD_CTRL) == 0) {
        scrollTranslationY += -2000.0f * dt * sgl::Mouse->getScrollWheel();
    }

    recomputeScrollThumbHeight();

    QVector2D scrollMin(windowWidth - scrollBarWidth, scrollThumbPosition);
    sgl::AABB2 scrollThumbAabb(
            scrollMin,
            QVector2D(scrollMin.x + scrollBarWidth - borderWidth, scrollMin.y + scrollThumbHeight));
    sgl::AABB2 scrollAreaAabb(
            QVector2D(windowWidth - scrollBarWidth + borderWidth, borderWidth),
            QVector2D(windowWidth - borderWidth, windowHeight - borderWidth));
    if (scrollThumbAabb.contains(mousePosition)) {
        scrollThumbHover = true;
        if (sgl::Mouse->buttonPressed(1)) {
            scrollThumbDrag = true;
            thumbDragDelta = scrollThumbPosition - mousePosition.y;
        }
    } else {
        scrollThumbHover = false;
        if (scrollAreaAabb.contains(mousePosition)) {
            if (sgl::Mouse->isButtonDown(1)) {
                // Jump to clicked position on scroll bar.
                scrollTranslationY = remap(
                        mousePosition.y - scrollThumbHeight / 2.0f,
                        borderWidth, windowHeight - borderWidth - scrollThumbHeight,
                        0.0f, fullWindowHeight - windowHeight);
            }
        }
    }

    if (sgl::Mouse->buttonReleased(1)) {
        scrollThumbDrag = false;
    }

    if (scrollThumbDrag) {
        scrollTranslationY = remap(
                mousePosition.y + thumbDragDelta,
                borderWidth, windowHeight - borderWidth - scrollThumbHeight,
                0.0f, fullWindowHeight - windowHeight);
    }

    if ((sgl::Keyboard->getModifier() & KMOD_CTRL) != 0 && sgl::Mouse->getScrollWheel() != 0.0f) {
        zoomFactor *= (1.0f + dt * sgl::Mouse->getScrollWheel());
        horizonBarHeight = horizonBarHeightBase * zoomFactor;
        horizonBarMargin = horizonBarMarginBase * zoomFactor;
        textSize = std::max(horizonBarHeight - horizonBarMargin, 4.0f);
        recomputeFullWindowHeight();
        if (windowHeight / fullWindowHeight > 1.0f) {
            fullWindowHeight = windowHeight;
        }
        if (windowHeight / fullWindowHeight >= 1.0f) {
            useScrollBar = false;
        } else {
            useScrollBar = true;
        }
        recomputeScrollThumbHeight();
    }*/

    scrollTranslationY = clamp(scrollTranslationY, 0.0f, fullWindowHeight - windowHeight);
    scrollThumbPosition = remap(
            scrollTranslationY,
            0.0f, fullWindowHeight - windowHeight,
            borderWidth, windowHeight - borderWidth - scrollThumbHeight);
}

}
