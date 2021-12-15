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
#include "src/data/multivar/nanovg/nanovg.h"
#include "src/data/multivar/nanovg/nanovg_gl.h"
#include "src/data/multivar/nanovg/nanovg_gl_utils.h"

// local application imports
#include "util/mutil.h"
#include "src/data/multivar/helpers.h"
#include "src/data/multivar/hidpi.h"
#include "diagrambase.h"
#include "aabb2.h"

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

MDiagramBase::MDiagramBase(GLint textureUnit) : textureUnit(textureUnit) {
}

void MDiagramBase::createNanoVgHandle() {
    if (!vg)
    {
        int flags = NVG_STENCIL_STROKES;
        if (!useMsaa)
        {
            flags |= NVG_ANTIALIAS;
        }
#ifndef NDEBUG
        flags |= NVG_DEBUG;
#endif
        vg = nvgCreateGL3(flags);

        static QString fontFilename;
        static bool fontFilenameLoaded = false;
        if (!fontFilenameLoaded)
        {
            fontFilename = getFontPath({ "Liberation Sans", "Droid Sans" });
            fontFilenameLoaded = true;
        }
        std::cout << "Used font: " << fontFilename.toStdString() << std::endl;
        int font = nvgCreateFont(vg, "sans", fontFilename.toStdString().c_str());
        if (font == -1) {
            LOG4CPLUS_ERROR(mlog, "Error in MDiagramBase::MDiagramBase: Couldn't find the font file.");
        }
    }
}

void MDiagramBase::initialize() {
    windowOffsetX = 20;
    windowOffsetY = 30;

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    glRM->generateEffectProgram("blit_shader", blitShader);
    blitShader->compileFromFile_Met3DHome("src/glsl/multivar/blit.fx.glsl");

    scaleFactor = getHighDPIScaleFactor();
}

void MDiagramBase::onWindowSizeChanged() {
    fboWidthDisplay = std::ceil(float(windowWidth) * scaleFactor);
    fboHeightDisplay = std::ceil(float(windowHeight) * scaleFactor);
    fboWidthInternal = fboWidthDisplay * supersamplingFactor;
    fboHeightInternal = fboHeightDisplay * supersamplingFactor;

    if (fbo)
    {
        delete fbo;
        fbo = nullptr;
    }
    if (colorRenderTexture)
    {
        delete colorRenderTexture;
        colorRenderTexture = nullptr;
    }
    if (depthStencilRbo)
    {
        delete depthStencilRbo;
        depthStencilRbo = nullptr;
    }
    if (blitVertexDataBuffer)
    {
        delete blitVertexDataBuffer;
        blitVertexDataBuffer = nullptr;
    }
}

void MDiagramBase::createRenderData() {
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    if (!blitVertexDataBuffer)
    {
        //QVector2D min = QVector2D(windowOffsetX, windowOffsetY);
        //QVector2D max = QVector2D(windowOffsetX + float(fboWidthDisplay), windowOffsetY + float(fboHeightDisplay));
        QVector2D min = QVector2D(0, 0);
        QVector2D max = QVector2D(float(fboWidthDisplay), float(fboHeightDisplay));

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
        //blitVertexDataBuffer = createVertexBuffer(glRM, vboID, vertexPositions);

        // No buffer with this item's data exists. Create a new one.
        GL::MTypedVertexBuffer<Vertex, GLfloat, sizeof(Vertex) / 4> *vb =
                new GL::MTypedVertexBuffer<Vertex, GLfloat, sizeof(Vertex) / 4>(vboID, vertexPositions.size());
        vb->upload(vertexPositions, glRM);
        blitVertexDataBuffer = static_cast<GL::MVertexBuffer*>(vb);
    }


    if (!fbo)
    {
        // Create the render texture.
        QString textureID = QString("radarBarChartRenderTexture_#%1").arg(getID());
        GLenum target;
        if (useMsaa)
        {
            target = GL_TEXTURE_2D_MULTISAMPLE;
            colorRenderTexture = new GL::MTexture(
                    textureID, target, GL_RGBA8, fboWidthInternal, fboHeightInternal, 1, numMsaaSamples);
        }
        else
        {
            target = GL_TEXTURE_2D;
            colorRenderTexture = new GL::MTexture(
                    textureID, target, GL_RGBA8, fboWidthInternal, fboHeightInternal);
        }
        /*if (!glRM->tryStoreGPUItem(colorRenderTexture))
        {
            delete colorRenderTexture;
            colorRenderTexture = nullptr;
        }*/
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
                glTexStorage2DMultisample(target, numMsaaSamples, GL_RGBA8, fboWidthInternal, fboHeightInternal, GL_TRUE);
            }
            else
            {
                glTexStorage2D(target, 1, GL_RGBA8, fboWidthInternal, fboHeightInternal);
            }

            glActiveTexture(GL_TEXTURE0);
        }

        QString rboID = QString("radarBarChartRbo_#%1").arg(getID());
        depthStencilRbo = new GL::MRenderbuffer(
                rboID, GL_DEPTH24_STENCIL8, fboWidthInternal, fboHeightInternal,
                useMsaa ? numMsaaSamples : 0);
        /*if (!glRM->tryStoreGPUItem(depthStencilRbo))
        {
            delete depthStencilRbo;
            depthStencilRbo = nullptr;
        }*/

        QString fboID = QString("radarBarChartFbo_#%1").arg(getID());
        fbo = new GL::MFramebuffer(fboID);
        fbo->bindTexture(colorRenderTexture, GL::COLOR_ATTACHMENT);
        fbo->bindRenderbuffer(depthStencilRbo, GL::DEPTH_STENCIL_ATTACHMENT);
        /*if (!glRM->tryStoreGPUItem(fbo))
        {
            delete fbo;
            fbo = nullptr;
        }*/

        GLint oldDrawFbo = 0, oldReadFbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFbo);
        fbo->bind();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFbo); CHECK_GL_ERROR;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFbo); CHECK_GL_ERROR;
    }
}

MDiagramBase::~MDiagramBase() {
    if (fbo)
    {
        delete fbo;
        fbo = nullptr;
    }
    if (colorRenderTexture)
    {
        delete colorRenderTexture;
        colorRenderTexture = nullptr;
    }
    if (depthStencilRbo)
    {
        delete depthStencilRbo;
        depthStencilRbo = nullptr;
    }
    if (blitVertexDataBuffer)
    {
        delete blitVertexDataBuffer;
        blitVertexDataBuffer = nullptr;
    }

    if (vg) {
        nvgDeleteGL3(vg);
    }
}

#define SHADER_VERTEX_ATTRIBUTE  0
#define SHADER_TEXTURE_ATTRIBUTE 1

void MDiagramBase::render() {
    if (!hasData())
    {
        return;
    }
    createRenderData();

    //NVGcolor backgroundFillColor = nvgRGBA(180, 180, 180, 70);
    //NVGcolor backgroundStrokeColor = nvgRGBA(120, 120, 120, 120);
    //NVGcolor backgroundFillColor = nvgRGBA(215, 215, 215, 127);
    //NVGcolor backgroundStrokeColor = nvgRGBA(190, 190, 190, 190);
    NVGcolor backgroundFillColor = nvgRGBA(230, 230, 230, 190);
    NVGcolor backgroundStrokeColor = nvgRGBA(190, 190, 190, 190);

    GLint oldDrawFbo = 0, oldReadFbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFbo);
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    glDisable(GL_DEPTH_TEST); CHECK_GL_ERROR;
    glDepthMask(GL_FALSE); CHECK_GL_ERROR;
    fbo->bind();
    glViewport(0, 0, fboWidthInternal, fboHeightInternal);
    glStencilMask(0xffffffff);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); CHECK_GL_ERROR;
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); CHECK_GL_ERROR;
    nvgBeginFrame(vg, windowWidth, windowHeight, scaleFactor * supersamplingFactor);

    // Render the render target-filling window rectangle.
    nvgBeginPath(vg);
    nvgRoundedRect(
            vg, borderWidth, borderWidth, windowWidth - 2.0f * borderWidth, windowHeight - 2.0f * borderWidth,
            borderRoundingRadius);
    nvgFillColor(vg, backgroundFillColor);
    nvgFill(vg);
    nvgStrokeColor(vg, backgroundStrokeColor);
    nvgStroke(vg);

    renderBase();

    nvgEndFrame(vg);

    // Premultiplied alpha.
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  CHECK_GL_ERROR;
    glDisable(GL_CULL_FACE); CHECK_GL_ERROR;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFbo); CHECK_GL_ERROR;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFbo); CHECK_GL_ERROR;
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]); CHECK_GL_ERROR;

    QMatrix4x4 mvpMatrix = matrixOrthogonalProjection(
            0.0f, float(oldViewport[2]),
            0.0f, float(oldViewport[3]),
            -1.0f, 1.0f) * matrixTranslation(windowOffsetX, windowOffsetY);

    colorRenderTexture->bindToTextureUnit(textureUnit);
    if (supersamplingFactor <= 1)
    {
        if (useMsaa)
        {
            blitShader->bindProgram("Multisampled"); CHECK_GL_ERROR;
            blitShader->setUniformValue("numSamples", numMsaaSamples);
        }
        else
        {
            blitShader->bindProgram("Standard"); CHECK_GL_ERROR;
        }
    }
    else
    {
        if (useMsaa)
        {
            blitShader->bindProgram("DownscaleMultisampled"); CHECK_GL_ERROR;
            blitShader->setUniformValue("numSamples", numMsaaSamples);
        }
        else
        {
            blitShader->bindProgram("Downscale"); CHECK_GL_ERROR;
        }
        blitShader->setUniformValue("supersamplingFactor", supersamplingFactor);
    }
    blitShader->setUniformValue("blitTexture", textureUnit);
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

bool MDiagramBase::isMouseOverDiagram(QVector2D mousePosition) const
{
    AABB2 aabb;
    aabb.min = QVector2D(windowOffsetX, windowOffsetY);
    aabb.max = QVector2D(windowOffsetX + float(fboWidthDisplay), windowOffsetY + float(fboHeightDisplay));

    return aabb.contains(mousePosition);
}

void MDiagramBase::mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (event->buttons() == Qt::NoButton) {
        isDraggingWindow = false;
    }

    if (isDraggingWindow) {
        windowOffsetX = windowOffsetXBase + float(event->x() - mouseDragStartPosX);
        windowOffsetY = windowOffsetYBase - float(event->y() - mouseDragStartPosY);
    }
}

void MDiagramBase::mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (event->button() == Qt::MouseButton::LeftButton) {
        isDraggingWindow = true;
        windowOffsetXBase = windowOffsetX;
        windowOffsetYBase = windowOffsetY;
        mouseDragStartPosX = event->x();
        mouseDragStartPosY = event->y();
    }
}

void MDiagramBase::mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
{
    if (event->button() == Qt::MouseButton::LeftButton) {
        isDraggingWindow = false;
    }
}

void MDiagramBase::wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event)
{
    ;
}


/// Removes trailing zeros and unnecessary decimal points.
std::string removeTrailingZeros(const std::string& numberString) {
    size_t lastPos = numberString.size();
    for (int i = int(numberString.size()) - 1; i > 0; i--) {
        char c = numberString.at(i);
        if (c == '.') {
            lastPos--;
            break;
        }
        if (c != '0') {
            break;
        }
        lastPos--;
    }
    return numberString.substr(0, lastPos);
}

/// Removes decimal points if more than maxDigits digits are used.
std::string MDiagramBase::getNiceNumberString(float number, int digits) {
    int maxDigits = digits + 2; // Add 2 digits for '.' and one digit afterwards.
    std::string outString = removeTrailingZeros(toString(number, digits, true));

    // Can we remove digits after the decimal point?
    size_t dotPos = outString.find('.');
    if (int(outString.size()) > maxDigits && dotPos != std::string::npos) {
        size_t substrSize = dotPos;
        if (int(dotPos) < maxDigits - 1) {
            substrSize = maxDigits;
        }
        outString = outString.substr(0, substrSize);
    }

    // Still too large?
    if (int(outString.size()) > maxDigits) {
        outString = toString(number, std::max(digits - 2, 1), false, false, true);
    }
    return outString;
}

void MDiagramBase::drawColorLegend(
        const NVGcolor& textColor, float x, float y, float w, float h, int numLabels, int numTicks,
        const std::function<std::string(float)>& labelMap, const std::function<NVGcolor(float)>& colorMap,
        const std::string& textTop) {
    const int numPoints = 9;
    const int numSubdivisions = numPoints - 1;

    // Draw color bar.
    for (int i = 0; i < numSubdivisions; i++) {
        float t0 = 1.0f - float(i) / float(numSubdivisions);
        float t1 = 1.0f - float(i + 1) / float(numSubdivisions);
        NVGcolor fillColor0 = colorMap(t0);
        NVGcolor fillColor1 = colorMap(t1);
        nvgBeginPath(vg);
        nvgRect(vg, x, y + h * float(i) / float(numSubdivisions), w, h / float(numSubdivisions));
        NVGpaint paint = nvgLinearGradient(
                vg, x, y + h * float(i) / float(numSubdivisions), x, y + h * float(i+1) / float(numSubdivisions),
                fillColor0, fillColor1);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }

    // Draw ticks.
    const float tickWidth = 4.0f;
    const float tickHeight = 1.0f;
    nvgBeginPath(vg);
    for (size_t tickIdx = 0; tickIdx < numTicks; tickIdx++) {
        //float t = 1.0f - float(tickIdx) / float(int(numTicks) - 1);
        float centerY = y + float(tickIdx) / float(int(numTicks) - 1) * h;
        nvgRect(vg, x + w, centerY - tickHeight / 2.0f, tickWidth, tickHeight);
    }
    nvgFillColor(vg, textColor);
    nvgFill(vg);

    // Draw on the right.
    nvgFontSize(vg, 12.0f);
    nvgFontFace(vg, "sans");
    for (size_t tickIdx = 0; tickIdx < numTicks; tickIdx++) {
        float t = 1.0f - float(tickIdx) / float(int(numTicks) - 1);
        float centerY = y + float(tickIdx) / float(int(numTicks) - 1) * h;
        std::string labelText = labelMap(t);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, textColor);
        nvgText(vg, x + w + 2.0f * tickWidth, centerY, labelText.c_str(), nullptr);
    }
    nvgFillColor(vg, textColor);
    nvgFill(vg);

    // Draw text on the top.
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
    nvgFillColor(vg, textColor);
    nvgText(vg, x + w / 2.0f, y - 4, textTop.c_str(), nullptr);

    // Draw box outline.
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgStrokeWidth(vg, 0.75f);
    nvgStrokeColor(vg, textColor);
    nvgStroke(vg);
}

}
