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
#include <unordered_set>

// related third party imports
#include <QVector4D>
#include <QStringList>
#include <QFontDatabase>
#include <GL/glew.h>
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/nanovg.h"
#include "nanovg/nanovg_gl.h"
#include "nanovg/nanovg_gl_utils.h"

// local application imports
#include "util/mutil.h"
#include "helpers.h"
#include "hidpi.h"
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

QString getFontPath(const std::set<QString>& preferredFontNames) {
    QStringList fontLocations = QStandardPaths::standardLocations(QStandardPaths::FontsLocation);

#ifdef __linux__
    // For some reason, on an Ubuntu 20.04 test system, Qt misses some of the paths specified on
    // https://doc.qt.io/qt-5/qstandardpaths.html.
    if (!fontLocations.contains("/usr/local/share/fonts")) {
        fontLocations.push_back("/usr/local/share/fonts");
    }
    if (!fontLocations.contains("/usr/share/fonts")) {
        fontLocations.push_back("/usr/share/fonts");
    }
#endif

    std::cout << fontLocations.size() << std::endl;
    for (const QString& fontLocation : fontLocations) {
        std::cout << fontLocation.toStdString() << std::endl;
    }

    QString matchingFontPath;
    QFontDatabase fontDatabase = QFontDatabase();
    for (const QString& fontLocation : fontLocations) {
        QDirIterator dirIterator(
                fontLocation, QStringList() << "*.ttf" << "*.TTF", QDir::Files, QDirIterator::Subdirectories);
        while (dirIterator.hasNext()) {
            QString fontPath = dirIterator.next();
            QString fontPathLower = fontPath.toLower();
            std::cout << fontPath.toStdString() << std::endl;

            int idx = fontDatabase.addApplicationFont(fontPath);
            if (idx >= 0) {
                QStringList names = fontDatabase.applicationFontFamilies(idx);
                for (const QString& name : names) {
                    if (preferredFontNames.find(name) != preferredFontNames.end()
                            && !fontPathLower.contains("bold")
                            && !fontPathLower.contains("italic")
                            && !fontPathLower.contains("oblique")) {
                        matchingFontPath = fontPath;
                    }
                    std::cout << name.toStdString() << std::endl;
                }
            }
        }
    }

    return matchingFontPath;
}

MRadarBarChart::MRadarBarChart(GLint textureUnit) : textureUnit(textureUnit) {
    borderSizeX = 90;
    borderSizeY = textMode == TextMode::HORIZONTAL ? 30 + float(variableNames.size()) / 2.0f : 110;
    chartRadius = 200;
    chartHoleRadius = 50;
    windowOffsetX = 10;
    windowOffsetY = 30;
    windowWidth = (chartRadius + borderSizeX) * 2.0f;
    windowHeight = (chartRadius + borderSizeY) * 2.0f;

    scaleFactor = getHighDPIScaleFactor();
    fboWidth = std::ceil(float(windowWidth) * scaleFactor);
    fboHeight = std::ceil(float(windowHeight) * scaleFactor);


    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    glRM->generateEffectProgram("blit_shader", blitShader);
    blitShader->compileFromFile_Met3DHome("src/glsl/multivar/blit.fx.glsl");


    int testCaseIdx = 2;

    if (testCaseIdx == 0) {
        variableNames = {"Variable 1", "Variable 2", "Variable 3", "Variable 4", "Variable 5"};
        variableValues = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
    } else if (testCaseIdx == 1) {
        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());
        std::uniform_real_distribution<> dis(0, 1);

        int numVariables = 91;
        for (int varIdx = 1; varIdx <= numVariables; varIdx++) {
            variableNames.push_back("Var " + std::to_string(varIdx));
            variableValues.push_back(dis(generator));
            //variableValues.push_back(float(varIdx) / float(numVariables));
        }
    } else if (testCaseIdx == 2) {
        variableNames = {
                "Pressure", "NI_OUT", "NR_OUT", "NS_OUT", "QC", "QG", "QG_OUT", "QH", "QH_OUT", "QI", "QI_OUT",
                "NCCLOUD", "QR", "QR_OUT", "QS", "QS_OUT", "QV", "S", "T", "artificial", "artificial (threshold)",
                "conv_400", "NCGRAUPEL", "conv_600", "dD_rainfrz_gh", "dD_rainfrz_ig", "dT_mult_max", "dT_mult_min",
                "da_HET", "da_ccn_1", "da_ccn_4", "db_HET", "db_ccn_1", "NCHAIL", "db_ccn_3", "db_ccn_4", "dc_ccn_1",
                "dc_ccn_4", "dcloud_c_z", "dd_ccn_1", "dd_ccn_2", "dd_ccn_3", "dd_ccn_4", "dgraupel_a_vel", "NCICE",
                "dgraupel_b_geo", "dgraupel_b_vel", "dgraupel_vsedi_max", "dhail_vsedi_max", "dice_a_f", "dice_a_geo",
                "dice_b_geo", "dice_b_vel", "dice_c_s", "dice_vsedi_max", "NCRAIN", "dinv_z", "dk_r",
                "dp_sat_ice_const_b", "dp_sat_melt", "drain_a_geo", "drain_a_vel", "drain_alpha", "drain_b_geo",
                "drain_b_vel", "drain_beta", "NCSNOW", "drain_c_z", "drain_g1", "drain_g2", "drain_gamma",
                "drain_min_x", "drain_min_x_freezing", "drain_mu", "drain_nu", "drho_vel", "dsnow_a_geo", "NG_OUT",
                "dsnow_b_geo", "dsnow_b_vel", "dsnow_vsedi_max", "mean of artificial", "mean of artificial (threshold)",
                "mean of physical", "mean of physical (high variability)", "physical", "physical (high variability)",
                "time_after_ascent", "NH_OUT", "w", "z"
        };

        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());
        std::uniform_real_distribution<> dis(0, 1);

        int numVariables = int(variableNames.size());
        for (int varIdx = 1; varIdx <= numVariables; varIdx++) {
            variableValues.push_back(dis(generator));
        }
    }
}

void MRadarBarChart::createRenderData() {
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    int flags = NVG_STENCIL_STROKES;
    if (!useMsaa) {
        flags |= NVG_ANTIALIAS;
    }
#ifndef NDEBUG
    flags |= NVG_DEBUG;
#endif
    vg = nvgCreateGL3(flags);

    QString fontFilename = getFontPath({ "Liberation Sans", "Droid Sans" });
    std::cout << "Used font: " << fontFilename.toStdString() << std::endl;
    int font = nvgCreateFont(vg, "sans", fontFilename.toStdString().c_str());
    if (font == -1) {
        LOG4CPLUS_ERROR(mlog, "Error in MRadarBarChart::MRadarBarChart: Couldn't find the font file.");
    }


    QVector2D min = QVector2D(windowOffsetX, windowOffsetY);
    QVector2D max = QVector2D(windowOffsetX + float(fboWidth), windowOffsetY + float(fboHeight));

    struct Vertex {
        QVector3D position;
        QVector2D texCoord;
    };
    QVector<Vertex> vertexPositions = {
            { { max.x(), max.y(), 0 }, { 1, 1 } },
            { { min.x(), min.y(), 0 }, { 0, 0 } },
            { { max.x(), min.y(), 0 }, { 1, 0 } },
            { { min.x(), min.y(), 0 }, { 0, 0 } },
            { { max.x(), max.y(), 0 }, { 1, 1 } },
            { { min.x(), max.y(), 0 }, { 0, 1 } }};
    QString vboID = QString("radarBarChartVbo_%1").arg(getID());
    blitVertexDataBuffer = createVertexBuffer(glRM, vboID, vertexPositions);


    // Create the render texture.
    QString textureID = QString("radarBarChartRenderTexture_#%1").arg(getID());
    GLenum target;
    if (useMsaa)
    {
        target = GL_TEXTURE_2D_MULTISAMPLE;
        colorRenderTexture = new GL::MTexture(
                textureID, target, GL_RGBA8, fboWidth, fboHeight, 1, numMsaaSamples);
    }
    else
    {
        target = GL_TEXTURE_2D;
        colorRenderTexture = new GL::MTexture(textureID, target, GL_RGBA8, fboWidth, fboHeight);
    }
    if (!glRM->tryStoreGPUItem(colorRenderTexture))
    {
        delete colorRenderTexture;
        colorRenderTexture = nullptr;
    }
    if (colorRenderTexture)
    {
        colorRenderTexture->bindToLastTextureUnit();

        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        // Upload data array to GPU.
        if (useMsaa)
        {
            glTexStorage2DMultisample(target, numMsaaSamples, GL_RGBA8, fboWidth, fboHeight, GL_TRUE);
        }
        else
        {
            glTexStorage2D(target, 1, GL_RGBA8, fboWidth, fboHeight);
        }

        glActiveTexture(GL_TEXTURE0);
    }

    QString rboID = QString("radarBarChartRbo_#%1").arg(getID());
    depthStencilRbo = new GL::MRenderbuffer(
            rboID, GL_DEPTH24_STENCIL8, fboWidth, fboHeight, useMsaa ? numMsaaSamples : 0);
    if (!glRM->tryStoreGPUItem(depthStencilRbo))
    {
        delete depthStencilRbo;
        depthStencilRbo = nullptr;
    }

    QString fboID = QString("radarBarChartFbo_#%1").arg(getID());
    fbo = new GL::MFramebuffer(fboID);
    fbo->bindTexture(colorRenderTexture, GL::COLOR_ATTACHMENT);
    fbo->bindRenderbuffer(depthStencilRbo, GL::DEPTH_STENCIL_ATTACHMENT);
    if (!glRM->tryStoreGPUItem(fbo))
    {
        delete fbo;
        fbo = nullptr;
    }


    GLint oldDrawFbo = 0, oldReadFbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFbo);
    fbo->bind();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFbo); CHECK_GL_ERROR;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFbo); CHECK_GL_ERROR;
}

MRadarBarChart::~MRadarBarChart() {
    if (vg) {
        nvgDeleteGL3(vg);
    }
}

inline int sign(float x) {
    return x > 0.0f ? 1 : (x < 0.0f ? -1 : 0);
}

inline QVector3D mix(const QVector3D &v0, const QVector3D &v1, float t) {
    return (1.0f - t) * v0 + t * v1;
}

void MRadarBarChart::drawPieSlice(const QVector2D &center, int index) {
    float varValue = variableValues.at(index);
    if (varValue <= std::numeric_limits<float>::epsilon()) {
        return;
    }
    float radius = varValue * (chartRadius - chartHoleRadius) + chartHoleRadius;

    QColor circleFillColorSgl = predefinedColors.at(index % predefinedColors.size());
    //QVector3D hsvColor = rgbToHSV(circleFillColorSgl.getFloatColorRGB());
    //hsvColor.g *= 0.5f;
    //QVector3D rgbColor = hsvToRGB(hsvColor);
    QVector3D circleColorVec;
    qreal r, g, b;
    circleFillColorSgl.getRgbF(&r, &g, &b);
    QVector3D rgbColor = mix(QVector3D(1.0f, 1.0f, 1.0f), QVector3D(r, g, b), 0.5f);
    NVGcolor circleFillColor = nvgRGBf(rgbColor.x(), rgbColor.y(), rgbColor.z());
    NVGcolor circleStrokeColor = nvgRGBA(0, 0, 0, 255);

    nvgBeginPath(vg);
    if (variableNames.size() == 1) {
        nvgCircle(vg, center.x(), center.y(), radius);
    } else {
        float angleStart = float(index) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
        float angleEnd = float(index + 1) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;

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

    //nvgClosePath(vg);
    //if (chartHoleRadius > 0.0f) {
    //    nvgCircle(vg, center.x, center.y, chartHoleRadius);
    //    nvgPathWinding(vg, NVG_HOLE);
    //}

    nvgFillColor(vg, circleFillColor);
    nvgFill(vg);
    nvgStrokeWidth(vg, 0.75f);
    nvgStrokeColor(vg, circleStrokeColor);
    nvgStroke(vg);
}

void MRadarBarChart::drawPieSliceTextHorizontal(const NVGcolor &textColor, const QVector2D &center, int index) {
    float radius = 1.0f * chartRadius + 10;
    float angleCenter = (float(index) + 0.5f) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
    QVector2D circlePoint(
            center.x() + std::cos(angleCenter) * radius, center.y() + std::sin(angleCenter) * radius);

    float dirX = clamp(std::cos(angleCenter) * 2.0f, -1.0f, 1.0f);
    float dirY = clamp(std::sin(angleCenter) * 2.0f, -1.0f, 1.0f);
    //float scaleFactorText = float(variableNames.size()) / 20.0f * sign(dirY) * std::pow(std::sin(angleCenter - float(M_PI)), 10.0f);
    float scaleFactorText = 0.0f;

    const char *text = variableNames.at(index).c_str();
    QVector2D bounds[2];
    nvgFontSize(vg, variableNames.size() > 50 ? 7.0f : 12.0f);
    nvgFontFace(vg, "sans");
    nvgTextBounds(vg, 0, 0, text, nullptr, &bounds[0][0]);
    QVector2D textSize = bounds[1] - bounds[0];

    QVector2D textPosition(circlePoint.x(), circlePoint.y());
    textPosition.setX(textPosition.x() + textSize.x() * (dirX - 1) * 0.5f);
    textPosition.setY(textPosition.y() + textSize.y() * ((dirY - 1) * 0.5f + scaleFactorText));

    /*const float EPSILON = 0.1f;
    int directionSignX = dirX > EPSILON ? 1 : (dirX < -EPSILON ? -1 : 0);
    int directionSignY = dirY > EPSILON ? 1 : (dirY < -EPSILON ? -1 : 0);
    if (directionSignX == -1) {
        textPosition.x -= textSize.x;
    } else if (directionSignX == 0) {
        textPosition.x -= textSize.x / 2.0f;
    }
    if (directionSignY == -1) {
        textPosition.y -= textSize.y;
    } else if (directionSignY == 0) {
        textPosition.y -= textSize.y / 2.0f;
    }*/

    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, textColor);
    nvgText(vg, textPosition.x(), textPosition.y(), text, nullptr);
}

void MRadarBarChart::drawPieSliceTextRotated(const NVGcolor &textColor, const QVector2D &center, int index) {
    nvgSave(vg);

    float radius = 1.0f * chartRadius + 10;
    float angleCenter = (float(index) + 0.5f) / float(variableNames.size()) * 2.0f * float(M_PI) - float(M_PI) / 2.0f;
    QVector2D circlePoint(
            center.x() + std::cos(angleCenter) * radius, center.y() + std::sin(angleCenter) * radius);

    const char *text = variableNames.at(index).c_str();
    nvgFontSize(vg, variableNames.size() > 50 ? 8.0f : 12.0f);
    nvgFontFace(vg, "sans");

    QVector2D textPosition(circlePoint.x(), circlePoint.y());
    //textPosition.x += textSize.x * (dirX - 1) * 0.5f;
    //textPosition.y += textSize.y * ((dirY - 1) * 0.5f + scaleFactorText);

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
        nvgRotate(vg, float(M_PI));
        nvgTranslate(vg, -(bounds[0].x() + bounds[1].x()) / 2.0f, -(bounds[0].y() + bounds[1].y()) / 2.0f);
    }
    nvgText(vg, textPosition.x(), textPosition.y(), text, nullptr);

    nvgRestore(vg);
}

void MRadarBarChart::drawDashedCircle(
        const NVGcolor &circleColor, const QVector2D &center, float radius,
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


#define SHADER_VERTEX_ATTRIBUTE  0
#define SHADER_TEXTURE_ATTRIBUTE 1

void MRadarBarChart::render() {
    NVGcolor textColor = nvgRGBA(0, 0, 0, 255);
    NVGcolor backgroundFillColor = nvgRGBA(180, 180, 180, 70);
    NVGcolor backgroundStrokeColor = nvgRGBA(120, 120, 120, 120);
    NVGcolor circleFillColor = nvgRGBA(180, 180, 180, 70);
    NVGcolor circleStrokeColor = nvgRGBA(120, 120, 120, 120);
    NVGcolor dashedCircleStrokeColor = nvgRGBA(120, 120, 120, 120);

    if (colorRenderTexture == nullptr) {
        createRenderData();
    }

    GLint oldDrawFbo = 0, oldReadFbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFbo);
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    glDisable(GL_DEPTH_TEST); CHECK_GL_ERROR;
    glDepthMask(GL_FALSE); CHECK_GL_ERROR;
    fbo->bind();
    glViewport(0, 0, fboWidth, fboHeight); CHECK_GL_ERROR;
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); CHECK_GL_ERROR;
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); CHECK_GL_ERROR;
    nvgBeginFrame(vg, windowWidth, windowHeight, scaleFactor);

    // Render the render target-filling widget rectangle.
    float borderWidth = 1.0f;
    nvgBeginPath(vg);
    nvgRoundedRect(
            vg, borderWidth, borderWidth, windowWidth - 2.0f * borderWidth, windowHeight - 2.0f * borderWidth,
            4.0f);
    nvgFillColor(vg, backgroundFillColor);
    nvgFill(vg);
    nvgStrokeColor(vg, backgroundStrokeColor);
    nvgStroke(vg);

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


    size_t numVariables = variableNames.size();
    for (size_t varIdx = 0; varIdx < numVariables; varIdx++) {
        drawPieSlice(center, int(varIdx));
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

    nvgEndFrame(vg);

    // Premultiplied alpha.
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  CHECK_GL_ERROR;
    glDisable(GL_CULL_FACE); CHECK_GL_ERROR;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFbo); CHECK_GL_ERROR;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFbo); CHECK_GL_ERROR;
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]); CHECK_GL_ERROR;

    QMatrix4x4 mvpMatrix = matrixOrthogonalProjection(
            0.0f, float(oldViewport[2]) - 1.0f,
            0.0f, float(oldViewport[3]) - 1.0f,
            -1.0f, 1.0f);

    colorRenderTexture->bindToTextureUnit(textureUnit);
    if (useMsaa)
    {
        blitShader->bindProgram("Multisampled"); CHECK_GL_ERROR;
        blitShader->setUniformValue("blitTextureMS", textureUnit);
        blitShader->setUniformValue("numSamples", numMsaaSamples);
    }
    else
    {
        blitShader->bindProgram("Standard"); CHECK_GL_ERROR;
        blitShader->setUniformValue("blitTexture", textureUnit);
    }
    blitShader->setUniformValue("mvpMatrix", mvpMatrix);

    blitVertexDataBuffer->attachToVertexAttribute(
            SHADER_VERTEX_ATTRIBUTE, 3, false,
            5 * sizeof(float),
            (const GLvoid *) (0 * sizeof(float)));
    blitVertexDataBuffer->attachToVertexAttribute(
            SHADER_TEXTURE_ATTRIBUTE, 2, false,
            5 * sizeof(float),
            (const GLvoid *) (3 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

    glEnable(GL_DEPTH_TEST); CHECK_GL_ERROR;
    glDepthMask(GL_TRUE); CHECK_GL_ERROR;
}

}
