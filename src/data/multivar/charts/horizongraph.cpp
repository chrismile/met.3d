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
#include "aabb2.h"

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

void MHorizonGraph::updateTimeStepTicks() {
    size_t numTimeStepsLocal = size_t(float(numTimeSteps) * (timeDisplayMax - timeDisplayMin) / (timeMax - timeMin));
    if (numTimeStepsLocal < 10) {
        timeStepLegendIncrement = 1;
        timeStepTicksIncrement = 1;
    } else if (numTimeStepsLocal < 50) {
        timeStepLegendIncrement = 5;
        timeStepTicksIncrement = 1;
    } else {
        timeStepLegendIncrement = numTimeStepsLocal / 10;
        timeStepTicksIncrement = timeStepLegendIncrement / 5;
    }
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
        const std::vector<std::string>& variableNames, float timeMin, float timeMax,
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
    recomputeScrollThumbHeight();

    // Compute metadata.
    this->timeMin = timeMin;
    this->timeMax = timeMax;
    if (this->timeMin != timeMin || this->timeMax != timeMax) {
        this->selectedTimeStep = timeMin;
        this->timeDisplayMin = timeMin;
        this->timeDisplayMax = timeMax;
        this->selectedTimeStepChanged = true;
        this->sortingIdx = -1;
    }
    this->timeDisplayMin = timeMin;
    this->timeDisplayMax = timeMax;
    if (variableValuesArray.empty()) {
        numTimeSteps = int(std::round(timeMax - timeMin)) + 1;
    } else {
        numTimeSteps = variableValuesArray.front().size();
    }
    updateTimeStepTicks();

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
            mean = 0.0f;
            for (size_t trajectoryIdx = 0; trajectoryIdx < numTrajectories; trajectoryIdx++) {
                float value = variableValuesArray.at(trajectoryIdx).at(timeStepIdx).at(varIdx);
                mean += value / float(numTrajectories);
            }
            float variance = 0.0f;
            float numTrajectoriesMinOne = std::max(float(numTrajectories - 1), 1e-8f);
            for (size_t trajectoryIdx = 0; trajectoryIdx < numTrajectories; trajectoryIdx++) {
                float value = variableValuesArray.at(trajectoryIdx).at(timeStepIdx).at(varIdx);
                float diff = value - mean;
                variance += diff * diff / numTrajectoriesMinOne;
            }
            stdDev = std::sqrt(variance);
        }
    }

    sortingIdx = -1;
    sortedVariableIndices.clear();
    sortedVariableIndices.reserve(variableNames.size());
    for (size_t varIdx = 0; varIdx < numVariables; varIdx++) {
        sortedVariableIndices.push_back(varIdx);
    }

    onWindowSizeChanged();
}

QVector3D MHorizonGraph::transferFunction(float value) const {
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

void MHorizonGraph::drawHorizonBackground() {
    NVGcolor backgroundFillColor = nvgRGBA(255, 255, 255, 100);
    for (size_t heightIdx = 0; heightIdx < numVariables; heightIdx++) {
        float lowerY = offsetHorizonBarsY + float(heightIdx) * (horizonBarHeight + horizonBarMargin);
        nvgBeginPath(vg);
        nvgRect(vg, offsetHorizonBarsX, lowerY, horizonBarWidth, horizonBarHeight);
        nvgFillColor(vg, backgroundFillColor);
        nvgFill(vg);
    }
}

void MHorizonGraph::drawHorizonLines() {
    AABB2 scissorAabb(
            QVector2D(borderWidth, offsetHorizonBarsY + scrollTranslationY),
            QVector2D(windowWidth - borderWidth, windowHeight - borderWidth + scrollTranslationY));

    size_t heightIdx = 0;
    for (size_t varIdx : sortedVariableIndices) {
        NVGcolor strokeColor = nvgRGBA(0, 0, 0, 255);

        float lowerY = offsetHorizonBarsY + float(heightIdx) * (horizonBarHeight + horizonBarMargin);
        float upperY = lowerY + horizonBarHeight;

        AABB2 boxAabb(
                QVector2D(offsetHorizonBarsX, lowerY),
                QVector2D(offsetHorizonBarsX + horizonBarWidth, lowerY + horizonBarHeight));
        if (!scissorAabb.intersects(boxAabb)) {
            heightIdx++;
            continue;
        }

        float timeStepIdxStartFlt = (timeDisplayMin - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1);
        float timeStepIdxStopFlt = (timeDisplayMax - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1);
        int timeStepIdxStart = int(std::floor(timeStepIdxStartFlt));
        int timeStepIdxStop = int(std::ceil(timeStepIdxStopFlt));

        if (mapStdDevToColor) {
            for (int timeStepIdx = timeStepIdxStart; timeStepIdx < timeStepIdxStop; timeStepIdx++) {
                float mean0 = ensembleMeanValues.at(timeStepIdx).at(varIdx);
                float stddev0 = ensembleStdDevValues.at(timeStepIdx).at(varIdx);
                float timeStep0 = timeMin + (timeMax - timeMin) * float(timeStepIdx) / float(numTimeSteps - 1);
                float xpos0 = offsetHorizonBarsX + (timeStep0 - timeDisplayMin) / (timeDisplayMax - timeDisplayMin) * horizonBarWidth;

                float mean1 = ensembleMeanValues.at(timeStepIdx + 1).at(varIdx);
                float stddev1 = ensembleStdDevValues.at(timeStepIdx + 1).at(varIdx);
                float timeStep1 = timeMin + (timeMax - timeMin) * float(timeStepIdx + 1) / float(numTimeSteps - 1);
                float xpos1 = offsetHorizonBarsX + (timeStep1 - timeDisplayMin) / (timeDisplayMax - timeDisplayMin) * horizonBarWidth;

                if (timeStepIdx == timeStepIdxStart && fract(timeStepIdxStartFlt) != 0.0f) {
                    mean0 = mix(mean0, mean1, fract(timeStepIdxStartFlt));
                    xpos0 = offsetHorizonBarsX;
                }
                if (timeStepIdx == timeStepIdxStop - 1 && fract(timeStepIdxStopFlt) != 0.0f) {
                    mean1 = mix(mean0, mean1, fract(timeStepIdxStartFlt));
                    xpos1 = offsetHorizonBarsX + horizonBarWidth;
                }
                float ypos0 = lowerY + (upperY - lowerY) * mean0;
                float ypos1 = lowerY + (upperY - lowerY) * mean1;

                QVector3D rgbColor0 = transferFunction(clamp(stddev0 * 2.0f, 0.0f, 1.0f));
                NVGcolor fillColor0 = nvgRGBf(rgbColor0.x(), rgbColor0.y(), rgbColor0.z());
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
            for (size_t timeStepIdx = 0; timeStepIdx <= numTimeSteps; timeStepIdx++) {
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
        for (int timeStepIdx = timeStepIdxStart; timeStepIdx <= timeStepIdxStop; timeStepIdx++) {
            float mean = ensembleMeanValues.at(timeStepIdx).at(varIdx);
            float timeStep = timeMin + (timeMax - timeMin) * float(timeStepIdx) / float(numTimeSteps - 1);
            float xpos = offsetHorizonBarsX + (timeStep - timeDisplayMin) / (timeDisplayMax - timeDisplayMin) * horizonBarWidth;
            if (timeStepIdx == timeStepIdxStart && fract(timeStepIdxStartFlt) != 0.0f) {
                float meanNext = ensembleMeanValues.at(timeStepIdx + 1).at(varIdx);
                mean = mix(mean, meanNext, fract(timeStepIdxStartFlt));
                xpos = offsetHorizonBarsX;
            }
            if (timeStepIdx == timeStepIdxStop && fract(timeStepIdxStopFlt) != 0.0f) {
                float meanLast = ensembleMeanValues.at(timeStepIdx - 1).at(varIdx);
                mean = mix(meanLast, mean, fract(timeStepIdxStartFlt));
                xpos = offsetHorizonBarsX + horizonBarWidth;
            }
            float ypos = lowerY + (upperY - lowerY) * mean;

            if (timeStepIdx == timeStepIdxStart) {
                nvgMoveTo(vg, xpos, ypos);
            } else {
                nvgLineTo(vg, xpos, ypos);
            }
        }
        nvgStrokeColor(vg, strokeColor);
        nvgStroke(vg);

        heightIdx++;
    }
}

void MHorizonGraph::drawHorizonLinesSparse() {
    AABB2 scissorAabb(
            QVector2D(borderWidth, offsetHorizonBarsY + scrollTranslationY),
            QVector2D(windowWidth - borderWidth, windowHeight - borderWidth + scrollTranslationY));

    float timeStepIdxStartFlt = (timeDisplayMin - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1);
    float timeStepIdxStopFlt = (timeDisplayMax - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1);
    int timeStepIdxStart = int(std::floor(timeStepIdxStartFlt));
    int timeStepIdxStop = int(std::ceil(timeStepIdxStopFlt));

    float density = horizonBarWidth * getScaleFactor() / float(timeStepIdxStop - timeStepIdxStart);
    const int horizonBarWidthInt = int(horizonBarWidth);

    size_t heightIdx = 0;
    for (size_t varIdx : sortedVariableIndices) {
        NVGcolor strokeColor = nvgRGBA(0, 0, 0, 255);

        float lowerY = offsetHorizonBarsY + float(heightIdx) * (horizonBarHeight + horizonBarMargin);
        float upperY = lowerY + horizonBarHeight;

        AABB2 boxAabb(
                QVector2D(offsetHorizonBarsX, lowerY),
                QVector2D(offsetHorizonBarsX + horizonBarWidth, lowerY + horizonBarHeight));
        if (!scissorAabb.intersects(boxAabb)) {
            heightIdx++;
            continue;
        }

        if (mapStdDevToColor) {
            if (density >= 1.0f) {
                for (int timeStepIdx = timeStepIdxStart; timeStepIdx < timeStepIdxStop; timeStepIdx++) {
                    float mean0 = ensembleMeanValues.at(timeStepIdx).at(varIdx);
                    float stddev0 = ensembleStdDevValues.at(timeStepIdx).at(varIdx);
                    float timeStep0 = timeMin + (timeMax - timeMin) * float(timeStepIdx) / float(numTimeSteps - 1);
                    float xpos0 =
                            offsetHorizonBarsX
                            + (timeStep0 - timeDisplayMin) / (timeDisplayMax - timeDisplayMin) * horizonBarWidth;

                    float mean1 = ensembleMeanValues.at(timeStepIdx + 1).at(varIdx);
                    float stddev1 = ensembleStdDevValues.at(timeStepIdx + 1).at(varIdx);
                    float timeStep1 = timeMin + (timeMax - timeMin) * float(timeStepIdx + 1) / float(numTimeSteps - 1);
                    float xpos1 =
                            offsetHorizonBarsX
                            + (timeStep1 - timeDisplayMin) / (timeDisplayMax - timeDisplayMin) * horizonBarWidth;

                    if (timeStepIdx == timeStepIdxStart && fract(timeStepIdxStartFlt) != 0.0f) {
                        mean0 = mix(mean0, mean1, fract(timeStepIdxStartFlt));
                        xpos0 = offsetHorizonBarsX;
                    }
                    if (timeStepIdx == timeStepIdxStop - 1 && fract(timeStepIdxStopFlt) != 0.0f) {
                        mean1 = mix(mean0, mean1, fract(timeStepIdxStartFlt));
                        xpos1 = offsetHorizonBarsX + horizonBarWidth;
                    }
                    float ypos0 = lowerY + (upperY - lowerY) * mean0;
                    float ypos1 = lowerY + (upperY - lowerY) * mean1;

                    QVector3D rgbColor0 = transferFunction(clamp(stddev0 * 2.0f, 0.0f, 1.0f));
                    NVGcolor fillColor0 = nvgRGBf(rgbColor0.x(), rgbColor0.y(), rgbColor0.z());
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
                for (int x = 0; x < horizonBarWidthInt; x++) {
                    float timeStep0 = timeDisplayMin + float(x) / horizonBarWidth * (timeDisplayMax - timeDisplayMin);
                    float timeStepIdxFlt0 = (timeStep0 - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1);
                    int timeStepIdx0a = std::floor(timeStepIdxFlt0);
                    int timeStepIdx0b = std::ceil(timeStepIdxFlt0);
                    float interp0 = fract(timeStepIdxFlt0);
                    float mean0a = ensembleMeanValues.at(timeStepIdx0a).at(varIdx);
                    float mean0b = ensembleMeanValues.at(timeStepIdx0b).at(varIdx);
                    float mean0 = mix(mean0a, mean0b, interp0);
                    float stddev0a = ensembleStdDevValues.at(timeStepIdx0a).at(varIdx);
                    float stddev0b = ensembleStdDevValues.at(timeStepIdx0b).at(varIdx);
                    float stddev0 = mix(stddev0a, stddev0b, interp0);
                    float xpos0 = offsetHorizonBarsX + float(x);

                    float timeStep1 =
                            timeDisplayMin + float(x + 1) / horizonBarWidth * (timeDisplayMax - timeDisplayMin);
                    float timeStepIdxFlt1 = (timeStep1 - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1);
                    int timeStepIdx1a = std::floor(timeStepIdxFlt1);
                    int timeStepIdx1b = std::ceil(timeStepIdxFlt1);
                    float interp1 = fract(timeStepIdxFlt1);
                    float mean1a = ensembleMeanValues.at(timeStepIdx1a).at(varIdx);
                    float mean1b = ensembleMeanValues.at(timeStepIdx1b).at(varIdx);
                    float mean1 = mix(mean1a, mean1b, interp1);
                    float stddev1a = ensembleStdDevValues.at(timeStepIdx1a).at(varIdx);
                    float stddev1b = ensembleStdDevValues.at(timeStepIdx1b).at(varIdx);
                    float stddev1 = mix(stddev1a, stddev1b, interp1);
                    float xpos1 = offsetHorizonBarsX + float(x + 1);

                    float ypos0 = lowerY + (upperY - lowerY) * mean0;
                    float ypos1 = lowerY + (upperY - lowerY) * mean1;

                    QVector3D rgbColor0 = transferFunction(clamp(stddev0 * 2.0f, 0.0f, 1.0f));
                    NVGcolor fillColor0 = nvgRGBf(rgbColor0.x(), rgbColor0.y(), rgbColor0.z());
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
            }
        } else {
            QColor fillColorSgl = predefinedColors.at(varIdx % predefinedColors.size());
            qreal r, g, b;
            fillColorSgl.getRgbF(&r, &g, &b);
            QVector3D rgbColor = mix(QVector3D(1.0f, 1.0f, 1.0f), QVector3D(r, g, b), 0.7f);
            NVGcolor fillColor = nvgRGBf(rgbColor.x(), rgbColor.y(), rgbColor.z());

            nvgBeginPath(vg);
            nvgMoveTo(vg, offsetHorizonBarsX, upperY);
            for (size_t timeStepIdx = 0; timeStepIdx <= numTimeSteps; timeStepIdx++) {
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
        if (density >= 1.0f) {
            for (int timeStepIdx = timeStepIdxStart; timeStepIdx <= timeStepIdxStop; timeStepIdx++) {
                float mean = ensembleMeanValues.at(timeStepIdx).at(varIdx);
                float timeStep = timeMin + (timeMax - timeMin) * float(timeStepIdx) / float(numTimeSteps - 1);
                float xpos =
                        offsetHorizonBarsX
                        + (timeStep - timeDisplayMin) / (timeDisplayMax - timeDisplayMin) * horizonBarWidth;
                if (timeStepIdx == timeStepIdxStart && fract(timeStepIdxStartFlt) != 0.0f) {
                    float meanNext = ensembleMeanValues.at(timeStepIdx + 1).at(varIdx);
                    mean = mix(mean, meanNext, fract(timeStepIdxStartFlt));
                    xpos = offsetHorizonBarsX;
                }
                if (timeStepIdx == timeStepIdxStop && fract(timeStepIdxStopFlt) != 0.0f) {
                    float meanLast = ensembleMeanValues.at(timeStepIdx - 1).at(varIdx);
                    mean = mix(meanLast, mean, fract(timeStepIdxStartFlt));
                    xpos = offsetHorizonBarsX + horizonBarWidth;
                }
                float ypos = lowerY + (upperY - lowerY) * mean;

                if (timeStepIdx == timeStepIdxStart) {
                    nvgMoveTo(vg, xpos, ypos);
                } else {
                    nvgLineTo(vg, xpos, ypos);
                }
            }
        } else {
            for (int x = 0; x <= horizonBarWidthInt; x++) {
                float timeStep = timeDisplayMin + float(x) / horizonBarWidth * (timeDisplayMax - timeDisplayMin);
                float timeStepIdxFlt = (timeStep - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1);
                int timeStepIdxA = std::floor(timeStepIdxFlt);
                int timeStepIdxB = std::ceil(timeStepIdxFlt);
                float interp = fract(timeStepIdxFlt);
                float meanB = ensembleMeanValues.at(timeStepIdxA).at(varIdx);
                float meanA = ensembleMeanValues.at(timeStepIdxB).at(varIdx);
                float mean = mix(meanA, meanB, interp);

                float xpos = offsetHorizonBarsX + float(x);
                float ypos = lowerY + (upperY - lowerY) * mean;

                if (x == 0) {
                    nvgMoveTo(vg, xpos, ypos);
                } else {
                    nvgLineTo(vg, xpos, ypos);
                }
            }
        }
        nvgStrokeColor(vg, strokeColor);
        nvgStroke(vg);

        heightIdx++;
    }
}

void MHorizonGraph::drawHorizonOutline(const NVGcolor& textColor) {
    for (size_t heightIdx = 0; heightIdx < numVariables; heightIdx++) {
        float lowerY = offsetHorizonBarsY + float(heightIdx) * (horizonBarHeight + horizonBarMargin);
        nvgBeginPath(vg);
        nvgRect(vg, offsetHorizonBarsX, lowerY, horizonBarWidth, horizonBarHeight);
        nvgStrokeWidth(vg, 0.25f);
        nvgStrokeColor(vg, textColor);
        nvgStroke(vg);
    }
}

void MHorizonGraph::drawSelectedTimeStepLine(const NVGcolor& textColor) {
    if (selectedTimeStep < timeDisplayMin || selectedTimeStep > timeDisplayMax) {
        return;
    }

    const float timeStepLineWidth = 1.0f;
    NVGcolor lineColor = textColor;
    lineColor.a = 0.1f;

    nvgBeginPath(vg);
    float xpos = remap(
            selectedTimeStep, timeDisplayMin, timeDisplayMax,
            offsetHorizonBarsX, offsetHorizonBarsX + horizonBarWidth);
    nvgRect(
            vg, xpos, offsetHorizonBarsY, timeStepLineWidth,
            horizonBarHeight * float(variableNames.size()) + horizonBarMargin * (float(variableNames.size()) - 1.0f));
    nvgFillColor(vg, lineColor);
    nvgFill(vg);
}

void MHorizonGraph::drawLegendLeft(const NVGcolor& textColor) {
    nvgFontSize(vg, textSize);
    nvgFontFace(vg, "sans");
    size_t heightIdx = 0;
    for (size_t varIdx : sortedVariableIndices) {
        float lowerY = offsetHorizonBarsY + float(heightIdx) * (horizonBarHeight + horizonBarMargin);
        //float upperY = lowerY + horizonBarHeight;
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        if (selectedVariableIndices.find(varIdx) != selectedVariableIndices.end()) {
            nvgFontBlur(vg, 1);
            nvgFillColor(vg, nvgRGBA(255, 0, 0, 255));
            nvgText(
                    vg, borderSizeX, lowerY + horizonBarHeight / 2.0f,
                    variableNames.at(varIdx).c_str(), nullptr);
            nvgFontBlur(vg, 0);
        }
        nvgFillColor(vg, textColor);
        nvgText(
                vg, borderSizeX, lowerY + horizonBarHeight / 2.0f,
                variableNames.at(varIdx).c_str(), nullptr);
        heightIdx++;
    }
}

void MHorizonGraph::drawLegendTop(const NVGcolor& textColor) {
    updateTimeStepTicks();

    nvgFontSize(vg, textSizeLegendTop);
    nvgFontFace(vg, "sans");
    int timeStepIdxStart = int(std::ceil((timeDisplayMin - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1)));
    int timeStepIdxStop = int(std::floor((timeDisplayMax - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1)));
    for (int timeStepIdx = timeStepIdxStart; timeStepIdx <= timeStepIdxStop; timeStepIdx++) {
        if (timeStepIdx % timeStepLegendIncrement != 0) {
            continue;
        }
        float timeStep = timeMin + (timeMax - timeMin) * float(timeStepIdx) / float(numTimeSteps - 1);
        std::string timeStepName = std::to_string(int(timeStep));
        float centerX = offsetHorizonBarsX + (timeStep - timeDisplayMin) / (timeDisplayMax - timeDisplayMin) * horizonBarWidth;
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, textColor);
        nvgText(vg, centerX, borderSizeY, timeStepName.c_str(), nullptr);
    }
}

void MHorizonGraph::drawTicks(const NVGcolor& textColor) {
    const float tickWidthBase = 0.5f;
    const float tickHeightBase = 2.0f;
    nvgBeginPath(vg);

    int timeStepIdxStart = int(std::ceil((timeDisplayMin - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1)));
    int timeStepIdxStop = int(std::floor((timeDisplayMax - timeMin) / (timeMax - timeMin) * float(numTimeSteps - 1)));
    for (int timeStepIdx = timeStepIdxStart; timeStepIdx <= timeStepIdxStop; timeStepIdx++) {
        if (timeStepIdx % timeStepTicksIncrement != 0) {
            continue;
        }

        float thicknessFactor = 1.0f;
        if (timeStepIdx % timeStepLegendIncrement == 0) {
            thicknessFactor = 2.0f;
        }
        const float tickWidth = thicknessFactor * tickWidthBase;
        const float tickHeight = thicknessFactor * tickHeightBase;
        float timeStep = timeMin + (timeMax - timeMin) * float(timeStepIdx) / float(numTimeSteps - 1);
        float centerX = offsetHorizonBarsX + (timeStep - timeDisplayMin) / (timeDisplayMax - timeDisplayMin) * horizonBarWidth;
        nvgRect(vg, centerX - tickWidth / 2.0f, offsetHorizonBarsY - tickHeight, tickWidth, tickHeight);
    }
    nvgFillColor(vg, textColor);
    nvgFill(vg);

    // Arrow indicating selected time step.
    NVGcolor lineColor = nvgRGB(50, 50, 50);
    float xpos = remap(
            selectedTimeStep, timeDisplayMin, timeDisplayMax,
            offsetHorizonBarsX, offsetHorizonBarsX + horizonBarWidth);
    nvgBeginPath(vg);
    nvgMoveTo(vg, xpos, offsetHorizonBarsY);
    nvgLineTo(vg, xpos + 4, offsetHorizonBarsY - 4);
    nvgLineTo(vg, xpos - 4, offsetHorizonBarsY - 4);
    nvgClosePath(vg);
    nvgFillColor(vg, lineColor);
    nvgFill(vg);
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

    drawHorizonBackground();
    drawHorizonLinesSparse();
    drawHorizonOutline(textColor);
    drawSelectedTimeStepLine(textColor);
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

void MHorizonGraph::mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    int viewportHeight = sceneView->getViewPortHeight();
    QVector2D mousePosition(float(event->x()), float(viewportHeight - event->y() - 1));
    mousePosition -= QVector2D(getWindowOffsetX(), getWindowOffsetY());
    mousePosition /= getScaleFactor();
    mousePosition[1] = windowHeight - mousePosition.y();

    if (event->buttons() == Qt::NoButton) {
        scrollThumbDrag = false;
    }

    if (scrollThumbDrag) {
        scrollTranslationY = remap(
                mousePosition.y() + thumbDragDelta,
                borderWidth, windowHeight - borderWidth - scrollThumbHeight,
                0.0f, fullWindowHeight - windowHeight);
    }

    QVector2D scrollMin(windowWidth - scrollBarWidth, scrollThumbPosition);
    AABB2 scrollThumbAabb(
            scrollMin,
            QVector2D(scrollMin.x() + scrollBarWidth - borderWidth, scrollMin.y() + scrollThumbHeight));
    if (scrollThumbAabb.contains(mousePosition)) {
        scrollThumbHover = true;
    } else {
        scrollThumbHover = false;
    }

    scrollTranslationY = clamp(scrollTranslationY, 0.0f, fullWindowHeight - windowHeight);
    scrollThumbPosition = remap(
            scrollTranslationY,
            0.0f, fullWindowHeight - windowHeight,
            borderWidth, windowHeight - borderWidth - scrollThumbHeight);

    // Click on the top legend and move the mouse to change the timescale.
    updateTimeScale(mousePosition, EventType::MouseMove, event);
    // Translation in the time axis.
    updateTimeShift(mousePosition, EventType::MouseMove, event);
}

void MHorizonGraph::mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    int viewportHeight = sceneView->getViewPortHeight();
    QVector2D mousePosition(float(event->x()), float(viewportHeight - event->y() - 1));
    mousePosition -= QVector2D(getWindowOffsetX(), getWindowOffsetY());
    mousePosition /= getScaleFactor();
    mousePosition[1] = windowHeight - mousePosition.y();

    QVector2D scrollMin(windowWidth - scrollBarWidth, scrollThumbPosition);
    AABB2 scrollThumbAabb(
            scrollMin,
            QVector2D(scrollMin.x() + scrollBarWidth - borderWidth, scrollMin.y() + scrollThumbHeight));
    AABB2 scrollAreaAabb(
            QVector2D(windowWidth - scrollBarWidth + borderWidth, borderWidth),
            QVector2D(windowWidth - borderWidth, windowHeight - borderWidth));
    if (scrollThumbAabb.contains(mousePosition)) {
        scrollThumbHover = true;
        if (event->button() == Qt::MouseButton::LeftButton) {
            scrollThumbDrag = true;
            thumbDragDelta = scrollThumbPosition - mousePosition.y();
        }
    } else {
        scrollThumbHover = false;
        if (scrollAreaAabb.contains(mousePosition) && event->button() == Qt::MouseButton::LeftButton) {
            // Jump to clicked position on scroll bar.
            scrollTranslationY = remap(
                    mousePosition.y() - scrollThumbHeight / 2.0f,
                    borderWidth, windowHeight - borderWidth - scrollThumbHeight,
                    0.0f, fullWindowHeight - windowHeight);
        }
    }

    scrollTranslationY = clamp(scrollTranslationY, 0.0f, fullWindowHeight - windowHeight);
    scrollThumbPosition = remap(
            scrollTranslationY,
            0.0f, fullWindowHeight - windowHeight,
            borderWidth, windowHeight - borderWidth - scrollThumbHeight);


    // Check whether the user clicked on one of the bars.
    AABB2 windowAabb(
            QVector2D(borderWidth, borderWidth),
            QVector2D(windowWidth - 2.0f * borderWidth, windowHeight - 2.0f * borderWidth));
    if (windowAabb.contains(mousePosition) && !event->modifiers().testFlag(Qt::ControlModifier)
            && !event->modifiers().testFlag(Qt::ShiftModifier) && event->button() == Qt::MouseButton::LeftButton) {
        size_t heightIdx = 0;
        for (size_t varIdx : sortedVariableIndices) {
            float lowerY = offsetHorizonBarsY + float(heightIdx) * (horizonBarHeight + horizonBarMargin);
            AABB2 boxAabb(
                    QVector2D(offsetHorizonBarsX, lowerY - scrollTranslationY),
                    QVector2D(
                            offsetHorizonBarsX + horizonBarWidth,
                            lowerY + horizonBarHeight - scrollTranslationY));
            if (boxAabb.contains(mousePosition)) {
                sortVariables(int(varIdx));
                break;
            }
            heightIdx++;
        }
    }

    // Click on the top legend and move the mouse to change the timescale.
    updateTimeScale(mousePosition, EventType::MousePress, event);
    // Translation in the time axis.
    updateTimeShift(mousePosition, EventType::MousePress, event);
}

void MHorizonGraph::mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    int viewportHeight = sceneView->getViewPortHeight();
    QVector2D mousePosition(float(event->x()), float(viewportHeight - event->y() - 1));
    mousePosition -= QVector2D(getWindowOffsetX(), getWindowOffsetY());
    mousePosition /= getScaleFactor();
    mousePosition[1] = windowHeight - mousePosition.y();

    if (event->button() == Qt::MouseButton::LeftButton) {
        scrollThumbDrag = false;
    }

    // Check whether the user right-clicked on the main graph area.
    AABB2 graphAreaAabb(
            QVector2D(offsetHorizonBarsX, offsetHorizonBarsY),
            QVector2D(offsetHorizonBarsX + horizonBarWidth, windowHeight - borderWidth));
    if (graphAreaAabb.contains(mousePosition) && event->button() == Qt::MouseButton::RightButton) {
        selectedTimeStep = remap(
                mousePosition.x(), offsetHorizonBarsX, offsetHorizonBarsX + horizonBarWidth,
                timeDisplayMin, timeDisplayMax);
        selectedTimeStep = clamp(selectedTimeStep, timeDisplayMin, timeDisplayMax);
        selectedTimeStepChanged = true;
    }

    scrollTranslationY = clamp(scrollTranslationY, 0.0f, fullWindowHeight - windowHeight);
    scrollThumbPosition = remap(
            scrollTranslationY,
            0.0f, fullWindowHeight - windowHeight,
            borderWidth, windowHeight - borderWidth - scrollThumbHeight);


    // Let the user click on variables to select different variables to show in linked views.
    AABB2 windowAabb(
            QVector2D(borderWidth, borderWidth),
            QVector2D(windowWidth - 2.0f * borderWidth, windowHeight - 2.0f * borderWidth));
    if (windowAabb.contains(mousePosition) && event->button() == Qt::MouseButton::LeftButton) {
        size_t heightIdx = 0;
        for (size_t varIdx : sortedVariableIndices) {
            float lowerY = offsetHorizonBarsY + float(heightIdx) * (horizonBarHeight + horizonBarMargin);
            //float upperY = lowerY + horizonBarHeight;
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            QVector2D bounds[2];
            nvgTextBounds(
                    vg, borderSizeX, lowerY + horizonBarHeight / 2.0f - scrollTranslationY,
                    variableNames.at(varIdx).c_str(), nullptr, &bounds[0][0]);
            AABB2 textAabb(bounds[0], bounds[1]);
            if (textAabb.contains(mousePosition)) {
                if (selectedVariableIndices.find(varIdx) == selectedVariableIndices.end()) {
                    selectedVariableIndices.insert(varIdx);
                } else {
                    selectedVariableIndices.erase(varIdx);
                }
                selectedVariablesChanged = true;
            }
            heightIdx++;
        }
    }

    // Click on the top legend and move the mouse to change the timescale.
    updateTimeScale(mousePosition, EventType::MouseRelease, event);
    // Translation in the time axis.
    updateTimeShift(mousePosition, EventType::MouseRelease, event);
}

void MHorizonGraph::wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event)
{
    const float dt = 1.0f / 60.0f / 120.0f;
    //std::cout << "wheel: " << event->delta() << std::endl;

    int viewportHeight = sceneView->getViewPortHeight();
    QVector2D mousePosition(float(event->x()), float(viewportHeight - event->y() - 1));
    mousePosition -= QVector2D(getWindowOffsetX(), getWindowOffsetY());
    mousePosition /= getScaleFactor();
    mousePosition[1] = windowHeight - mousePosition.y();

    AABB2 windowAabb(
            QVector2D(borderWidth, borderWidth),
            QVector2D(windowWidth - 2.0f * borderWidth, windowHeight - 2.0f * borderWidth));
    if (windowAabb.contains(mousePosition) && !event->modifiers().testFlag(Qt::ControlModifier)
            && !event->modifiers().testFlag(Qt::ShiftModifier)) {
        scrollTranslationY += -2000.0f * dt * float(event->delta());
    }

    if (event->modifiers().testFlag(Qt::ControlModifier) && event->delta() != 0) {
        zoomFactor *= (1.0f + dt * float(event->delta()));
        horizonBarHeight = horizonBarHeightBase * zoomFactor;
        horizonBarMargin = horizonBarMarginBase * zoomFactor;
        textSize = clamp(horizonBarHeight - horizonBarMargin, 4.0f, horizonBarHeightBase - horizonBarMarginBase);
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
    }

    scrollTranslationY = clamp(scrollTranslationY, 0.0f, fullWindowHeight - windowHeight);
    scrollThumbPosition = remap(
            scrollTranslationY,
            0.0f, fullWindowHeight - windowHeight,
            borderWidth, windowHeight - borderWidth - scrollThumbHeight);

    // Zoom in the time axis.
    if (event->modifiers().testFlag(Qt::ShiftModifier) && event->delta() != 0) {
        float timeZoomFactor = dt * float(event->delta());

        float timeDisplayMinOld = timeDisplayMin;
        float timeDisplayMaxOld = timeDisplayMax;

        float x0 = (selectedTimeStep - timeDisplayMinOld) / (timeDisplayMaxOld - timeDisplayMinOld);
        float xa = x0 + sign(event->delta()) * 0.1f;
        float xb = xa + sign(event->delta()) * timeZoomFactor;

        if (sign(xa - x0) == sign(xb - x0)) {
            float pa = (xa - x0) * (1.0f - x0) / (xb - x0);
            float na = (xa - x0) * x0 / (xb - x0);

            timeDisplayMin = selectedTimeStep - na * (timeDisplayMaxOld - timeDisplayMinOld);
            timeDisplayMax = selectedTimeStep + pa * (timeDisplayMaxOld - timeDisplayMinOld);

            if (timeDisplayMin < timeMin) {
                float timeDisplayMinNew = timeDisplayMin;
                float timeDisplayMaxNew = timeDisplayMax;

                timeDisplayMin = timeMin;
                timeDisplayMax = std::min(timeMax, timeDisplayMin + (timeDisplayMaxNew - timeDisplayMinNew));
            }
            if (timeDisplayMax > timeMax) {
                float timeDisplayMinNew = timeDisplayMin;
                float timeDisplayMaxNew = timeDisplayMax;

                timeDisplayMax = timeMax;
                timeDisplayMin = std::max(timeMin, timeDisplayMax - (timeDisplayMaxNew - timeDisplayMinNew));
            }
        }
    }
}

void MHorizonGraph::updateTimeScale(const QVector2D& mousePosition, EventType eventType, QMouseEvent* event) {
    // Click on the top legend and move the mouse to change the timescale.
    AABB2 legendTopAabb(
            QVector2D(offsetHorizonBarsX, borderSizeY),
            QVector2D(offsetHorizonBarsX + horizonBarWidth, offsetHorizonBarsY));
    if (legendTopAabb.contains(mousePosition)) {
        if (eventType == EventType::MousePress && event->button() == Qt::LeftButton
                && event->modifiers().testFlag(Qt::ShiftModifier)) {
            topLegendClickPct = (mousePosition.x() - offsetHorizonBarsX) / horizonBarWidth;
            timeDisplayMinOld = timeDisplayMin;
            timeDisplayMaxOld = timeDisplayMax;
            isDraggingTopLegend = true;
        }
        if (eventType == EventType::MouseRelease && event->button() == Qt::LeftButton) {
            isDraggingTopLegend = false;
        }
        if (isDraggingTopLegend && event->buttons() == Qt::LeftButton && eventType == EventType::MouseMove) {
            float x0 = (selectedTimeStep - timeDisplayMinOld) / (timeDisplayMaxOld - timeDisplayMinOld);
            float xa = topLegendClickPct;
            float xb = (mousePosition.x() - offsetHorizonBarsX) / horizonBarWidth;

            if (sign(xa - x0) == sign(xb - x0)) {
                float pa = (xa - x0) * (1.0f - x0) / (xb - x0);
                float na = (xa - x0) * x0 / (xb - x0);

                timeDisplayMin = selectedTimeStep - na * (timeDisplayMaxOld - timeDisplayMinOld);
                timeDisplayMax = selectedTimeStep + pa * (timeDisplayMaxOld - timeDisplayMinOld);

                if (timeDisplayMin < timeMin) {
                    float timeDisplayMinNew = timeDisplayMin;
                    float timeDisplayMaxNew = timeDisplayMax;

                    timeDisplayMin = timeMin;
                    timeDisplayMax = std::min(timeMax, timeDisplayMin + (timeDisplayMaxNew - timeDisplayMinNew));
                }
                if (timeDisplayMax > timeMax) {
                    float timeDisplayMinNew = timeDisplayMin;
                    float timeDisplayMaxNew = timeDisplayMax;

                    timeDisplayMax = timeMax;
                    timeDisplayMin = std::max(timeMin, timeDisplayMax - (timeDisplayMaxNew - timeDisplayMinNew));
                }
            }
        }
    }
}

void MHorizonGraph::updateTimeShift(const QVector2D& mousePosition, EventType eventType, QMouseEvent* event) {
    // Translation in the time axis.
    AABB2 graphAreaAabb(
            QVector2D(offsetHorizonBarsX, offsetHorizonBarsY),
            QVector2D(offsetHorizonBarsX + horizonBarWidth, windowHeight - borderWidth));
    if (graphAreaAabb.contains(mousePosition)) {
        if (eventType == EventType::MousePress && event->button() == Qt::LeftButton
                && event->modifiers().testFlag(Qt::ShiftModifier)) {
            clickTime = remap(
                    mousePosition.x(), offsetHorizonBarsX, offsetHorizonBarsX + horizonBarWidth,
                    timeDisplayMin, timeDisplayMax);
            timeDisplayMinOld = timeDisplayMin;
            timeDisplayMaxOld = timeDisplayMax;
            isDraggingTimeShift = true;
        }
        if (eventType == EventType::MouseRelease && event->button() == Qt::LeftButton) {
            isDraggingTimeShift = false;
        }
        if (isDraggingTimeShift && event->buttons() == Qt::LeftButton && eventType == EventType::MouseMove) {
            float timeDiff = clickTime - remap(
                    mousePosition.x(), offsetHorizonBarsX, offsetHorizonBarsX + horizonBarWidth,
                    timeDisplayMinOld, timeDisplayMaxOld);
            if (timeDisplayMinOld + timeDiff < timeMin) {
                timeDiff = timeMin - timeDisplayMinOld;
            }
            if (timeDisplayMaxOld + timeDiff > timeMax) {
                timeDiff = timeMax - timeDisplayMaxOld;
            }
            timeDisplayMin = timeDisplayMinOld + timeDiff;
            timeDisplayMax = timeDisplayMaxOld + timeDiff;
        }
    }
}

float MHorizonGraph::computeSimilarityMetric(
        int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const {
    if (varIdx0 == varIdx1) {
        return std::numeric_limits<float>::lowest();
    }

    if (similarityMetric == SimilarityMetric::L1_NORM) {
        return computeL1Norm(varIdx0, varIdx1, valueArray, factor);
    } else if (similarityMetric == SimilarityMetric::L2_NORM) {
        return computeL2Norm(varIdx0, varIdx1, valueArray, factor);
    } else if (similarityMetric == SimilarityMetric::NCC) {
        return computeNCC(varIdx0, varIdx1, valueArray, factor);
    } else if (similarityMetric == SimilarityMetric::MI) {
        return computeMI(varIdx0, varIdx1, valueArray, factor);
    } else if (similarityMetric == SimilarityMetric::SSIM) {
        return computeSSIM(varIdx0, varIdx1, valueArray, factor);
    } else {
        throw std::runtime_error("Error in HorizonGraph::computeSimilarityMetric: Unknown similarity metric.");
        return 0.0f;
    }
}

float MHorizonGraph::computeL1Norm(
        int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const {
    float difference = 0.0f;
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float val0 = factor * valueArray.at(timeStepIdx).at(varIdx0);
        float val1 = factor * valueArray.at(timeStepIdx).at(varIdx1);
        float diffMean = val1 - val0;
        difference = std::abs(diffMean);
    }
    difference /= float(numTimeSteps);

    return difference;
}

float MHorizonGraph::computeL2Norm(
        int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const {
    float difference = 0.0f;
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float val0 = factor * valueArray.at(timeStepIdx).at(varIdx0);
        float val1 = factor * valueArray.at(timeStepIdx).at(varIdx1);
        float diffMean = val1 - val0;
        difference += diffMean * diffMean;
    }
    difference /= float(numTimeSteps);

    return difference;
}

float MHorizonGraph::computeNCC(
        int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const {
    float mean0 = 0.0f;
    float mean1 = 0.0f;
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float val0 = factor * valueArray.at(timeStepIdx).at(varIdx0);
        float val1 = factor * valueArray.at(timeStepIdx).at(varIdx1);
        mean0 += val0;
        mean1 += val1;
    }
    mean0 /= float(numTimeSteps);
    mean1 /= float(numTimeSteps);

    float var0 = 0.0f;
    float var1 = 0.0f;
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float val0 = factor * valueArray.at(timeStepIdx).at(varIdx0);
        float val1 = factor * valueArray.at(timeStepIdx).at(varIdx1);
        float diff0 = val0 - mean0;
        float diff1 = val1 - mean1;
        var0 += diff0 * diff0;
        var1 += diff1 * diff1;
    }
    var0 /= float(numTimeSteps - 1);
    var1 /= float(numTimeSteps - 1);

    float stdDev0 = std::sqrt(var0);
    float stdDev1 = std::sqrt(var1);

    float ncc = 0.0f;
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float val0 = factor * valueArray.at(timeStepIdx).at(varIdx0);
        float val1 = factor * valueArray.at(timeStepIdx).at(varIdx1);
        ncc += (val0 - mean0) * (val1 - mean1) / (stdDev0 * stdDev1);
    }
    ncc /= float(numTimeSteps);

    return -ncc;
}

float MHorizonGraph::computeMI(
        int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const {
    float* histogram0 = new float[numBins];
    float* histogram1 = new float[numBins];
    float* histogram2d = new float[numBins * numBins];

    // Initialize the histograms with zeros.
    for (int binIdx = 0; binIdx < numBins; binIdx++) {
        histogram0[binIdx] = 0.0f;
        histogram1[binIdx] = 0;
    }
    for (int binIdx0 = 0; binIdx0 < numBins; binIdx0++) {
        for (int binIdx1 = 0; binIdx1 < numBins; binIdx1++) {
            histogram2d[binIdx0 * numBins + binIdx1] = 0.0f;
        }
    }

    // Compute the two 1D histograms and the 2D joint histogram.
    float entryWeight1d = 1.0f / float(numTimeSteps);
    float entryWeight2d = 1.0f / float(numTimeSteps * numTimeSteps);
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float val = factor * valueArray.at(timeStepIdx).at(varIdx0);
        int binIdx = clamp(int(val * float(numBins)), 0, numBins - 1);
        histogram0[binIdx] += entryWeight1d;
    }
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float val = factor * valueArray.at(timeStepIdx).at(varIdx1);
        int binIdx = clamp(int(val * float(numBins)), 0, numBins - 1);
        histogram1[binIdx] += entryWeight1d;
    }
    for (size_t timeStepIdx0 = 0; timeStepIdx0 < numTimeSteps; timeStepIdx0++) {
        for (size_t timeStepIdx1 = 0; timeStepIdx1 < numTimeSteps; timeStepIdx1++) {
            float val0 = factor * valueArray.at(timeStepIdx0).at(varIdx0);
            float val1 = factor * valueArray.at(timeStepIdx1).at(varIdx1);
            int binIdx0 = clamp(int(val0 * float(numBins)), 0, numBins - 1);
            int binIdx1 = clamp(int(val1 * float(numBins)), 0, numBins - 1);
            histogram2d[binIdx0 * numBins + binIdx1] += entryWeight2d;
        }
    }

    /*
     * Compute the mutual information metric. Two possible ways of calculation:
     * a) $MI = H(x) + H(y) - H(x, y)$
     * with the Shannon entropy $H(x) = -\sum_i p_x(i) \log p_x(i)$
     * and the joint entropy $H(x, y) = -\sum_i \sum_j p_{xy}(i, j) \log p_{xy}(i, j)$
     * b) $MI = \sum_i \sum_j p_{xy}(i, j) \log \frac{p_{xy}(i, j)}{p_x(i) p_y(j)}$
     */
    const float EPSILON = 1e-7;
    float mi = 0.0f;
    for (int binIdx0 = 0; binIdx0 < numBins; binIdx0++) {
        for (int binIdx1 = 0; binIdx1 < numBins; binIdx1++) {
            float p_xy = std::max(histogram2d[binIdx0 * numBins + binIdx1], EPSILON);
            float p_x = std::max(histogram0[binIdx0], EPSILON);
            float p_y = std::max(histogram1[binIdx1], EPSILON);
            mi += p_xy * std::log(std::max(p_xy, EPSILON) / std::max(p_x * p_y, EPSILON));
        }
    }

    delete[] histogram0;
    delete[] histogram1;
    delete[] histogram2d;

    return -mi;
}

float MHorizonGraph::computeSSIM(
        int varIdx0, int varIdx1, const std::vector<std::vector<float>>& valueArray, float factor) const {
    float mean0 = 0.0f;
    float mean1 = 0.0f;
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float val0 = factor * valueArray.at(timeStepIdx).at(varIdx0);
        float val1 = factor * valueArray.at(timeStepIdx).at(varIdx1);
        mean0 += val0;
        mean1 += val1;
    }
    mean0 /= float(numTimeSteps);
    mean1 /= float(numTimeSteps);

    float var0 = 0.0f;
    float var1 = 0.0f;
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float val0 = factor * valueArray.at(timeStepIdx).at(varIdx0);
        float val1 = factor * valueArray.at(timeStepIdx).at(varIdx1);
        float diff0 = val0 - mean0;
        float diff1 = val1 - mean1;
        var0 += diff0 * diff0;
        var1 += diff1 * diff1;
    }
    var0 /= float(numTimeSteps - 1);
    var1 /= float(numTimeSteps - 1);

    float stdDev0 = std::sqrt(var0);
    float stdDev1 = std::sqrt(var1);

    float cov = 0.0f;
    for (size_t timeStepIdx = 0; timeStepIdx < numTimeSteps; timeStepIdx++) {
        float val0 = factor * valueArray.at(timeStepIdx).at(varIdx0);
        float val1 = factor * valueArray.at(timeStepIdx).at(varIdx1);
        float diff0 = val0 - mean0;
        float diff1 = val1 - mean1;
        cov += diff0 * diff1;
    }
    cov /= float(numTimeSteps - 1);

    const float k1 = 0.01;
    const float k2 = 0.03;
    const float c1 = k1 * k1;
    const float c2 = k2 * k2;

    float ssim =
            (2.0f * mean0 * mean1 + c1) * (2.0f * cov + c2)
            / ((mean0 * mean0 + mean1 * mean1 + c1) * (stdDev0 * stdDev0 + stdDev1 * stdDev1 + c2));

    return -ssim;
}

void MHorizonGraph::sortVariables(int newSortingIdx, bool forceRecompute) {
    if (sortingIdx == newSortingIdx && !forceRecompute) {
        return;
    }
    sortingIdx = newSortingIdx;

    sortedVariableIndices.clear();
    std::vector<std::pair<float, size_t>> differenceMap;
    differenceMap.resize(variableNames.size());
#if _OPENMP >= 200805
    #pragma omp parallel for default(none) shared(variableNames, differenceMap, sortingIdx)
#endif
    for (size_t varIdx = 0; varIdx < variableNames.size(); varIdx++) {
        float metric = 0.0f;
        if (meanMetricInfluence > 0.0f) {
            metric += meanMetricInfluence * computeSimilarityMetric(
                    sortingIdx, int(varIdx), ensembleMeanValues, 1.0f);
        }
        if (stdDevMetricInfluence > 0.0f) {
            metric += stdDevMetricInfluence * computeSimilarityMetric(
                    sortingIdx, int(varIdx), ensembleStdDevValues, 2.0f);
        }
        differenceMap.at(varIdx) = std::make_pair(metric, varIdx);
    }
    std::sort(differenceMap.begin(), differenceMap.end());
    for (auto& it : differenceMap) {
        sortedVariableIndices.push_back(it.second);
    }
}

}
