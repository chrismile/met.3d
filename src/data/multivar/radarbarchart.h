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
#include <set>

// related third party imports
#include <QVector2D>
#include "nanovg/nanovg.h"

// local application imports
#include "gxfw/gl/indexbuffer.h"
#include "gxfw/gl/vertexbuffer.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/renderbuffer.h"
#include "gxfw/gl/framebuffer.h"
#include "data/abstractdataitem.h"
#include "qt_extensions/radarchart.h"
#include "beziertrajectories.h"

struct NVGcontext;
struct NVGcolor;

namespace Met3D {

class MRadarBarChart : public MMemoryManagementUsingObject {
public:
    MRadarBarChart(GLint textureUnit);

    ~MRadarBarChart();

    void render();

private:
    void createRenderData();

    bool showWindow = true;

    enum class TextMode {
        HORIZONTAL, ROTATED
    };
    TextMode textMode = TextMode::ROTATED;

    float borderSizeX;
    float borderSizeY;
    float chartRadius;
    float chartHoleRadius;
    float windowOffsetX;
    float windowOffsetY;
    float windowWidth, windowHeight;
    float scaleFactor = 1.0f;
    int fboWidth = 0, fboHeight = 0;
    bool useMsaa = false;
    int numMsaaSamples = 8;
    GL::MFramebuffer *fbo = nullptr;
    GL::MTexture *colorRenderTexture = nullptr;
    GL::MRenderbuffer *depthStencilRbo = nullptr;
    GLint textureUnit;

    NVGcontext *vg;

    void drawPieSlice(const QVector2D &center, int index);

    void drawPieSliceTextHorizontal(const NVGcolor &textColor, const QVector2D &center, int index);

    void drawPieSliceTextRotated(const NVGcolor &textColor, const QVector2D &center, int index);

    void drawDashedCircle(
            const NVGcolor &circleColor, const QVector2D &center, float radius,
            int numDashes, float dashSpaceRatio, float thickness);

    std::vector<std::string> variableNames;
    std::vector<float> variableValues;

    // For drawing to the main window.
    std::shared_ptr<GL::MShaderEffect> blitShader;
    GL::MVertexBuffer *blitVertexDataBuffer = nullptr;
};

}

#endif //MET_3D_RADARBARCHART_HPP
