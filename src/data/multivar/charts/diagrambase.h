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
#ifndef MET_3D_DIAGRAMBASE_H
#define MET_3D_DIAGRAMBASE_H

// standard library imports
#include <set>

// related third party imports
#include <QVector2D>
#include "src/data/multivar/nanovg/nanovg.h"

// local application imports
#include "gxfw/gl/indexbuffer.h"
#include "gxfw/gl/vertexbuffer.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/renderbuffer.h"
#include "gxfw/gl/framebuffer.h"
#include "data/abstractdataitem.h"
#include "qt_extensions/radarchart.h"
#include "src/data/multivar/beziertrajectories.h"

struct NVGcontext;
struct NVGcolor;

namespace Met3D {

enum class DiagramType {
    RADAR_CHART, RADAR_BAR_CHART, HORIZON_GRAPH
};

class MDiagramBase : public MMemoryManagementUsingObject {
public:
    explicit MDiagramBase(GLint textureUnit);
    virtual DiagramType getDiagramType()=0;
    virtual void initialize();
    ~MDiagramBase() override;
    void render();

    /// Returns whether the mouse is over the area of the diagram.
    bool isMouseOverDiagram(QVector2D mousePosition) const;
    virtual bool hasData()=0;

    inline QVector<uint32_t> getSelectedVariables() const {
        QVector<uint32_t> selectedVariables;
        selectedVariables.resize(int(numVariables));
        for (size_t varIdx : selectedVariableIndices) {
            selectedVariables[int(varIdx)] = 1u;
        }
        return selectedVariables;
    }
    inline void setSelectedVariables(const QVector<uint32_t>& selectedVariables) {
        selectedVariableIndices.clear();
        for (int varIdx = 0; varIdx < selectedVariables.size(); varIdx++) {
            if (selectedVariables.at(varIdx)) {
                selectedVariableIndices.insert(size_t(varIdx));
            }
        }
        selectedVariablesChanged = false;
    }
    inline bool getSelectedVariablesChanged() const { return selectedVariablesChanged; }
    inline void resetSelectedVariablesChanged() { selectedVariablesChanged = false; }

    virtual void mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) {}
    virtual void mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) {}
    virtual void mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) {}
    virtual void wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event) {}

protected:
    virtual void renderBase()=0;
    virtual void onWindowSizeChanged();

    // Utility functions.
    void drawColorLegend(
            const NVGcolor& textColor, float x, float y, float w, float h, int numLabels, int numTicks,
            const std::function<std::string(float)>& labelMap, const std::function<NVGcolor(float)>& colorMap,
            const std::string& textTop = "");

    /// Removes decimal points if more than maxDigits digits are used.
    static std::string getNiceNumberString(float number, int digits);
    /// Conversion to and from string
    template <class T>
    static std::string toString(
            T obj, int precision, bool fixed = true, bool noshowpoint = false, bool scientific = false) {
        std::ostringstream ostr;
        ostr.precision(precision);
        if (fixed) {
            ostr << std::fixed;
        }
        if (noshowpoint) {
            ostr << std::noshowpoint;
        }
        if (scientific) {
            ostr << std::scientific;
        }
        ostr << obj;
        return ostr.str();
    }

    NVGcontext* vg = nullptr;
    float windowWidth, windowHeight;
    float borderSizeX, borderSizeY;
    const float borderWidth = 1.0f;
    const float borderRoundingRadius = 4.0f;

    inline float getWindowOffsetX() const { return windowOffsetX; }
    inline float getWindowOffsetY() const { return windowOffsetY; }
    inline float getScaleFactor() const { return scaleFactor; }

    // Variables can be selected by clicking on them.
    size_t numVariables = 0;
    std::set<size_t> selectedVariableIndices;
    bool selectedVariablesChanged = false;

private:
    void createRenderData();

    bool showWindow = true;

    enum class TextMode {
        HORIZONTAL, ROTATED
    };
    TextMode textMode = TextMode::ROTATED;

    float windowOffsetX, windowOffsetY;
    float scaleFactor = 1.0f;
    int fboWidthInternal = 0, fboHeightInternal = 0;
    int fboWidthDisplay = 0, fboHeightDisplay = 0;
    bool useMsaa = false;
    int numMsaaSamples = 8;
    int supersamplingFactor = 4;
    GL::MFramebuffer *fbo = nullptr;
    GL::MTexture *colorRenderTexture = nullptr;
    GL::MRenderbuffer *depthStencilRbo = nullptr;
    GLint textureUnit;

    // For drawing to the main window.
    std::shared_ptr<GL::MShaderEffect> blitShader;
    GL::MVertexBuffer *blitVertexDataBuffer = nullptr;
};

}

#endif //MET_3D_DIAGRAMBASE_H
