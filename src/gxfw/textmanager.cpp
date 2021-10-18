/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
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

#include "textmanager.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "util/mexception.h"
#include "gxfw/msceneviewglwidget.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTextManager::MTextManager()
    : MActor(),
      fontfile(""),
      fontsize(-1)
{
    setName("Labels");

    // FreeType2 needs to be initialised.
    if (FT_Init_FreeType(&ft))
        throw MInitialisationError("cannot initialise FreeType",
                                   __FILE__, __LINE__);

    // characterInfo should store information about ASCII characters up to
    // code 128.
    characterInfo = new MTextureAtlasCharacterInfo[128];

    // The vertex buffer is initialised in "initialize()". Assign 0 here so
    // that glDeleteBuffers() in the destructor works in case the buffer is
    // never allocated.
    directRenderingTextVBO = 0;
    directRenderingBBoxVBO = 0;
}


MTextManager::~MTextManager()
{
    delete[] characterInfo;

    // If the buffer has never been initialised, the value of
    // vertexBufferObject is 0 (see constructor). According to
    // http://www.opengl.org/sdk/docs/man4/, "glDeleteBuffers silently ignores
    // 0's and names that do not correspond to existing buffer objects.".
    glDeleteBuffers(1, &directRenderingTextVBO); CHECK_GL_ERROR;
    glDeleteBuffers(1, &directRenderingBBoxVBO); CHECK_GL_ERROR;

    // Free label resources (GPU and CPU memory).
    foreach (MLabel *label, labelPool)
    {
        glDeleteBuffers(1, &(label->vbo)); CHECK_GL_ERROR;
        delete label;
    }
}


void MTextManager::setFont(QString fontfile, int fontsize)
{
    this->fontfile = fontfile;
    this->fontsize = fontsize;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE  0
#define SHADER_TEXTURE_ATTRIBUTE 1

void MTextManager::initializePerGLContextResources(MSceneViewGLWidget *sceneView)
{
    Q_UNUSED (sceneView);
}


void MTextManager::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);
    bboxEffect->compileFromFile_Met3DHome(
                "src/glsl/simple_coloured_geometry.fx.glsl");
    textEffect->compileFromFile_Met3DHome(
                "src/glsl/text.fx.glsl");
}


void MTextManager::renderChar(
        const char *ch, CoordinateSystem coordsys,
        float x, float y, float z, float size,
        QColor colour, MSceneViewGLWidget *sceneView,
        bool bbox, QColor bboxColour, float bboxPad)
{
    if (coordsys == WORLDSPACE)
    {
        QVector3D clip = *(sceneView->getModelViewProjectionMatrix())
                * QVector3D(x, y, z);
        x = clip.x(); y = clip.y(); z = clip.z();
    }
    else if (coordsys == LONLATP)
    {
        QVector3D clip = sceneView->lonLatPToClipSpace(QVector3D(x, y, z));
        x = clip.x(); y = clip.y(); z = clip.z();
    }

    // Shortcuts to the character and its texture and alignment information.
    unsigned char c = *ch;
    MTextureAtlasCharacterInfo *ci = &characterInfo[c];

    // Scales to convert from pixel space to [-1..1] space.
    float scale = size / textureAtlasHeight; // both in px
    float scaleX = scale * 2. / sceneView->getViewPortWidth(); // latter in px
    float scaleY = scale * 2. / sceneView->getViewPortHeight(); // latter in px

    // Coordinate x specifies the "curser position", y the position of
    // the base line of the string. To correctly place the quad that
    // accomodates the character texture, we compute the coordinates
    // of the upper left corner of the quad, as well as its width and
    // height in clip coordinates. The variables bitmapLeft and bitmapTop
    // are provided by FreeType and describe the offset of the character
    // with respect to cursor position and baseline for correct alignment.
    float xUpperLeft = x + ci->bitmapLeft * scaleX;
    float yUpperLeft = y + ci->bitmapTop * scaleY;
    float charWidth  = ci->bitmapWidth * scaleX;
    float charHeight = ci->bitmapHeight * scaleY;

    // Create the geometry for the character's quad. Structure: x1, y1, z1, s1,
    // t1, x2, y2, ... with x, y being the clip space coordinates of each
    // point ([-1..1x-1..1]) and s, t being the corresponding texture
    // coordinates ([0..1x0..1]).
    float cquad[20] = {
        // lower left
        xUpperLeft,
        yUpperLeft - charHeight,
        z,
        ci->xOffset_TexCoords,
        ci->bitmapHeight / textureAtlasHeight,
        // upper left
        xUpperLeft,
        yUpperLeft,
        z,
        ci->xOffset_TexCoords,
        0.,
        // lower right
        xUpperLeft + charWidth,
        yUpperLeft - charHeight,
        z,
        ci->xOffset_TexCoords + ci->bitmapWidth / textureAtlasWidth,
        ci->bitmapHeight / textureAtlasHeight,
        // upper right
        xUpperLeft + charWidth,
        yUpperLeft,
        z,
        ci->xOffset_TexCoords + ci->bitmapWidth / textureAtlasWidth,
        0.
    };

    if (bbox)
    {
        // If enabled, draw a bounding box around the character.

        // Convert bboxPad from pixel units to clip space [-1..1].
        float xScaledBBoxPad = bboxPad * 2. / sceneView->getViewPortWidth();
        float yScaledBBoxPad = bboxPad * 2. / sceneView->getViewPortHeight();

        float cbbox[12] = {
            // lower left
            xUpperLeft - xScaledBBoxPad,
            yUpperLeft - charHeight - yScaledBBoxPad,
            z,
            // upper left
            xUpperLeft - xScaledBBoxPad,
            yUpperLeft + yScaledBBoxPad,
            z,
            // lower right
            xUpperLeft + charWidth + xScaledBBoxPad,
            yUpperLeft - charHeight - yScaledBBoxPad,
            z,
            // upper right
            xUpperLeft + charWidth + xScaledBBoxPad,
            yUpperLeft + yScaledBBoxPad,
            z
        };

        bboxEffect->bindProgram("Simple");
        bboxEffect->setUniformValue("colour", bboxColour);

        glBindBuffer(GL_ARRAY_BUFFER, directRenderingBBoxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cbbox), cbbox, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(SHADER_VERTEX_ATTRIBUTE, 3, GL_FLOAT,
                              GL_FALSE, 0, (const GLvoid *)0);
        glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);

        glPolygonOffset(.01f, 1.0f);
        glEnable(GL_POLYGON_OFFSET_FILL);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Draw the character.
    textEffect->bindProgram("Text");
    textEffect->setUniformValue("colour", colour);

    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, textureObjectName);
    textEffect->setUniformValue("textAtlas", textureUnit);
#ifdef USE_QOPENGLWIDGET
    glActiveTexture(GL_TEXTURE0);
#endif

    glBindBuffer(GL_ARRAY_BUFFER, directRenderingTextVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cquad), cquad, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(SHADER_VERTEX_ATTRIBUTE, 3, GL_FLOAT,
                          GL_FALSE, 5 * sizeof(float),
                          (const GLvoid *)0); // offset for vertex positions
    glVertexAttribPointer(SHADER_TEXTURE_ATTRIBUTE, 2, GL_FLOAT,
                          GL_FALSE, 5 * sizeof(float),
                          (const GLvoid *)(3 * sizeof(float))); // offset textures

    glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);
    glEnableVertexAttribArray(SHADER_TEXTURE_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void MTextManager::renderText_2DClip_i(QString text, float x, float y, float size,
                                       QColor colour, MSceneViewGLWidget *sceneView)
{
    // Scales to convert from pixel space to [-1..1] space.
    float scale = size / textureAtlasHeight; // both in px
    float scaleX = scale * 2. / sceneView->getViewPortWidth(); // latter in px
    float scaleY = scale * 2. / sceneView->getViewPortHeight(); // latter in px

    // Render each character in the string "text" individually.
    for (int i = 0; i < text.size(); i++)
    {
        // Shortcuts to the current character and its texture and alignment
        // information.
        unsigned char c = text.at(i).toAscii();
        MTextureAtlasCharacterInfo *ci = &characterInfo[c];

        // Coordinate x specifies the "curser position", y the position of
        // the base line of the string. To correctly place the quad that
        // accomodates the character texture, we compute the coordinates
        // of the upper left corner of the quad, as well as its width and
        // height in clip coordinates. The variables bitmapLeft and bitmapTop
        // are provided by FreeType and describe the offset of the character
        // with respect to cursor position and baseline for correct alignment.
        float xUpperLeft = x + ci->bitmapLeft * scaleX;
        float yUpperLeft = y + ci->bitmapTop * scaleY;
        float charWidth  = ci->bitmapWidth * scaleX;
        float charHeight = ci->bitmapHeight * scaleY;

        // Create the geometry for the character's quad. Structure: x1, y1, z1,
        // s1, t1, x2, y2, ... with x, y being the clip space coordinates of
        // each point ([-1..1x-1..1]) and s, t being the corresponding texture
        // coordinates ([0..1x0..1]).
        float cquad[20] = {
            // lower left
            xUpperLeft,
            yUpperLeft - charHeight,
            0.,
            ci->xOffset_TexCoords,
            ci->bitmapHeight / textureAtlasHeight,
            // upper left
            xUpperLeft,
            yUpperLeft,
            0.,
            ci->xOffset_TexCoords,
            0.,
            // lower right
            xUpperLeft + charWidth,
            yUpperLeft - charHeight,
            0.,
            ci->xOffset_TexCoords + ci->bitmapWidth / textureAtlasWidth,
            ci->bitmapHeight / textureAtlasHeight,
            // upper right
            xUpperLeft + charWidth,
            yUpperLeft,
            0.,
            ci->xOffset_TexCoords + ci->bitmapWidth / textureAtlasWidth,
            0.};

        // Draw the character.
        textEffect->bindProgram("Text");
        textEffect->setUniformValue("colour", colour);

        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, textureObjectName);
        textEffect->setUniformValue("textAtlas", textureUnit);
#ifdef USE_QOPENGLWIDGET
        glActiveTexture(GL_TEXTURE0);
#endif

        glBindBuffer(GL_ARRAY_BUFFER, directRenderingTextVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cquad), cquad, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(SHADER_VERTEX_ATTRIBUTE, 3, GL_FLOAT,
                              GL_FALSE, 5 * sizeof(float),
                              (const GLvoid *)0); // offset for vertex positions
        glVertexAttribPointer(SHADER_TEXTURE_ATTRIBUTE, 2, GL_FLOAT,
                              GL_FALSE, 5 * sizeof(float),
                              (const GLvoid *)(3 * sizeof(float))); // offset textures

        glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);
        glEnableVertexAttribArray(SHADER_TEXTURE_ATTRIBUTE);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Advance the cursor position.
        x += characterInfo[c].advanceX * scaleX;
        y += characterInfo[c].advanceY * scaleY;
    }
}


void MTextManager::renderText(
        QString text, CoordinateSystem coordsys,
        float x, float y, float z, float size,
        QColor colour, MSceneViewGLWidget *sceneView,
        TextAnchor anchor, bool bbox, QColor bboxColour, float bboxPad)
{
    if (coordsys == WORLDSPACE)
    {
        QVector3D clip = *(sceneView->getModelViewProjectionMatrix())
                * QVector3D(x, y, z);
        x = clip.x(); y = clip.y(); z = clip.z();
    }
    else if (coordsys == LONLATP)
    {
        QVector3D clip = sceneView->lonLatPToClipSpace(QVector3D(x, y, z));
        x = clip.x(); y = clip.y(); z = clip.z();
    }

    // Scales to convert from pixel space to [-1..1] space.
    float scale  = size / textureAtlasHeight; // both in px
    float scaleX = scale * 2. / sceneView->getViewPortWidth(); // latter in px
    float scaleY = scale * 2. / sceneView->getViewPortHeight(); // latter in px

    // ctriangles stores vertex and texture coordinates of the triangles that
    // will represent the string. n is a counter used to index ctriangles.
    // float ctriangles[30 * text.size()]; VLAs not supported in msvc
	float* ctriangles = new float[30 * text.size()];
    int n = 0;

    // Min/max Y coordinate (clip space) of bounding box, updated in loop.
    float maxYOfBBox = -1;
    float minYOfBBox =  1;

    // Compute geometry and texture coordinates for each character in the string
    // "text".
    for (int i = 0; i < text.size(); i++)
    {
        // Shortcuts to the current character and its texture and alignment
        // information.
        unsigned char c = text.at(i).toAscii();
        MTextureAtlasCharacterInfo *ci = &characterInfo[c];

        // Coordinate x specifies the "curser position", y the position of
        // the base line of the string. To correctly place the quad that
        // accomodates the character texture, we compute the coordinates
        // of the upper left corner of the quad, as well as its width and
        // height in clip coordinates. The variables bitmapLeft and bitmapTop
        // are provided by FreeType and describe the offset of the character
        // with respect to cursor position and baseline for correct alignment.
        float xUpperLeft = x + ci->bitmapLeft * scaleX;
        float yUpperLeft = y + ci->bitmapTop * scaleY;
        float charWidth  = ci->bitmapWidth * scaleX;
        float charHeight = ci->bitmapHeight * scaleY;

        // Update min/max Y coordinate (clip space) of bounding box.
        maxYOfBBox = max(maxYOfBBox, yUpperLeft);
        minYOfBBox = min(minYOfBBox, yUpperLeft - charHeight);

        // Create the geometry to render the character in two triangles.
        // Unfortunately, due to cursor advancement, there usually is a space
        // between two adjacent characters. Hence, adjacent characters cannot
        // be rendered with adjacent triangles in a GL_TRIANGLE_STRIP. We hence
        // have to use GL_TRIANGLES, which means that for each character, we
        // need to duplicate two vertices and describe the character quad with
        // six vertices instead of four required for a triangle strip. The
        // alternative would be to issue a separate glDrawArrays() call for
        // each character (not better)..

        // Structure of ctriangles: x1, y1, s1, t1, x2, y2, ... with x, y being
        // the clip space coordinates of each point ([-1..1x-1..1]) and s, t
        // being the corresponding texture coordinates ([0..1x0..1]).

        // FIRST TRIANGLE (lower left)
        // lower left corner of the quad
        ctriangles[n++] = xUpperLeft;
        ctriangles[n++] = yUpperLeft - charHeight;
        ctriangles[n++] = z;
        ctriangles[n++] = ci->xOffset_TexCoords;
        ctriangles[n++] = ci->bitmapHeight / textureAtlasHeight;
        // upper left corner of the quad
        ctriangles[n++] = xUpperLeft;
        ctriangles[n++] = yUpperLeft;
        ctriangles[n++] = z;
        ctriangles[n++] = ci->xOffset_TexCoords;
        ctriangles[n++] = 0;
        // lower right corner of the quad
        ctriangles[n++] = xUpperLeft + charWidth;
        ctriangles[n++] = yUpperLeft - charHeight;
        ctriangles[n++] = z;
        ctriangles[n++] = ci->xOffset_TexCoords + ci->bitmapWidth / textureAtlasWidth;
        ctriangles[n++] = ci->bitmapHeight / textureAtlasHeight;

        // SECOND TRIANGLE (upper right)
        // upper left corner of the quad
        ctriangles[n++] = xUpperLeft;
        ctriangles[n++] = yUpperLeft;
        ctriangles[n++] = z;
        ctriangles[n++] = ci->xOffset_TexCoords;
        ctriangles[n++] = 0;
        // lower right corner of the quad
        ctriangles[n++] = xUpperLeft + charWidth;
        ctriangles[n++] = yUpperLeft - charHeight;
        ctriangles[n++] = z;
        ctriangles[n++] = ci->xOffset_TexCoords + ci->bitmapWidth / textureAtlasWidth;
        ctriangles[n++] = ci->bitmapHeight / textureAtlasHeight;
        // upper right corner of the quad
        ctriangles[n++] = xUpperLeft + charWidth;
        ctriangles[n++] = yUpperLeft;
        ctriangles[n++] = z;
        ctriangles[n++] = ci->xOffset_TexCoords + ci->bitmapWidth / textureAtlasWidth;
        ctriangles[n++] = 0;

        // Advance the cursor position for the next character.
        x += ci->advanceX * scaleX;
    }

    QVector2D offset = QVector2D(0, 0); // (anchor == BASELINELEFT)
    if (anchor == BASELINERIGHT)
        offset = QVector2D(ctriangles[0]-ctriangles[n-5], 0);
    if (anchor == BASELINECENTRE)
        offset = QVector2D((ctriangles[0]-ctriangles[n-5])/2., 0);
    else if (anchor == UPPERLEFT)
        offset = QVector2D(0, y-maxYOfBBox);
    else if (anchor == UPPERRIGHT)
        offset = QVector2D(ctriangles[0]-ctriangles[n-5], y-maxYOfBBox);
    else if (anchor == UPPERCENTRE)
        offset = QVector2D((ctriangles[0]-ctriangles[n-5])/2., y-maxYOfBBox);
    else if (anchor == LOWERLEFT)
        offset = QVector2D(0, y-minYOfBBox);
    else if (anchor == LOWERRIGHT)
        offset = QVector2D(ctriangles[0]-ctriangles[n-5], y-minYOfBBox);
    else if (anchor == LOWERCENTRE)
        offset = QVector2D((ctriangles[0]-ctriangles[n-5])/2., y-minYOfBBox);
    else if (anchor == MIDDLELEFT)
        offset = QVector2D(0, y-minYOfBBox - (maxYOfBBox-minYOfBBox)/2.);
    else if (anchor == MIDDLERIGHT)
        offset = QVector2D(ctriangles[0]-ctriangles[n-5],
                           y-minYOfBBox - (maxYOfBBox-minYOfBBox)/2.);
    else if (anchor == MIDDLECENTRE)
        offset = QVector2D((ctriangles[0]-ctriangles[n-5])/2.,
                           y-minYOfBBox - (maxYOfBBox-minYOfBBox)/2.);

    if (bbox)
    {
        // If enabled, draw a bounding box around the character.

        // Convert bboxPad from pixel units to clip space [-1..1].
        float xScaledBBoxPad = bboxPad * 2. / sceneView->getViewPortWidth();
        float yScaledBBoxPad = bboxPad * 2. / sceneView->getViewPortHeight();

        float cbbox[12] = {
            // lower left (lower left x, y of first triangle from ctriangles).
            ctriangles[0  ] - xScaledBBoxPad + float(offset.x()),
            minYOfBBox      - yScaledBBoxPad + float(offset.y()),
            z,
            // upper left
            ctriangles[0  ] - xScaledBBoxPad + float(offset.x()),
            maxYOfBBox      + yScaledBBoxPad + float(offset.y()),
            z,
            // lower right
            ctriangles[n-5] + xScaledBBoxPad + float(offset.x()),
            minYOfBBox      - yScaledBBoxPad + float(offset.y()),
            z,
            // upper right (upper right x, y of the last triangle).
            ctriangles[n-5] + xScaledBBoxPad + float(offset.x()),
            maxYOfBBox      + yScaledBBoxPad + float(offset.y()),
            z
        };

        bboxEffect->bindProgram("Simple");
        bboxEffect->setUniformValue("colour", bboxColour);

        glBindBuffer(GL_ARRAY_BUFFER, directRenderingBBoxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cbbox), cbbox, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(SHADER_VERTEX_ATTRIBUTE, 3, GL_FLOAT,
                              GL_FALSE, 0, (const GLvoid *)0);
        glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);

        // Make sure the bounding box doesn't obscure the characters.
        glPolygonOffset(.01f, 1.0f);
        glEnable(GL_POLYGON_OFFSET_FILL);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Pass colour variable to shader, upload vertex attributes to VBO and
    // render the text string.
    textEffect->bindProgram("Text");
    textEffect->setUniformValue("colour", colour);
    textEffect->setUniformValue("offset", offset);

    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, textureObjectName);
    textEffect->setUniformValue("textAtlas", textureUnit);
#ifdef USE_QOPENGLWIDGET
    glActiveTexture(GL_TEXTURE0);
#endif

    glBindBuffer(GL_ARRAY_BUFFER, directRenderingTextVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ctriangles), ctriangles,
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(SHADER_VERTEX_ATTRIBUTE, 3, GL_FLOAT,
                          GL_FALSE, 5 * sizeof(float),
                          (const GLvoid *)0); // offset for vertex positions
    glVertexAttribPointer(SHADER_TEXTURE_ATTRIBUTE, 2, GL_FLOAT,
                          GL_FALSE, 5 * sizeof(float),
                          (const GLvoid *)(3 * sizeof(float))); // offset textures

    glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);
    glEnableVertexAttribArray(SHADER_TEXTURE_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLES, 0, 6 * text.size());

    glBindBuffer(GL_ARRAY_BUFFER, 0);

	delete[] ctriangles;
}


MLabel* MTextManager::addText(
        QString text, CoordinateSystem coordsys,
        float x, float y, float z, float size, QColor colour,
        TextAnchor anchor, bool bbox, QColor bboxColour, float bboxPadFraction)
{
    if (!isInitialized())
        throw MInitialisationError("cannot add text labels before the OpenGL "
                                   "context has been initialised",
                                   __FILE__, __LINE__);

    // Min/max Y coordinate (character pixel space) of bounding box, updated in
    // loop below.
    float maxYOfBBox = -9999;
    float minYOfBBox =  9999;

    // Curser position in 2D character bitmap pixel space.
    float cursorX = 0;
    float cursorY = 0;

    // "coordinates" stores vertex and texture coordinates of the triangles
    // that will represent the string. The first 8 entries are used for
    // bounding box coordinates. n is a counter used to index coordinates.
	std::vector<float> coordinates(8 + 24 * text.size());
    int n = 8; // reserve first 8 entries for bbox coordinates

    // Estimate width of label in pixel size (mk: used for contour labels)
    float textWidth = 0;

    for (int i = 0; i < text.size(); i++)
    {
        // Shortcuts to the current character and its texture and alignment
        // information.
        unsigned char c = text.at(i).toAscii();
        MTextureAtlasCharacterInfo *ci = &characterInfo[c];

        // Coordinate x specifies the "curser position", y the position of
        // the base line of the string. To correctly place the quad that
        // accomodates the character texture, we compute the coordinates
        // of the upper left corner of the quad, as well as its width and
        // height in clip coordinates. The variables bitmapLeft and bitmapTop
        // are provided by FreeType and describe the offset of the character
        // with respect to cursor position and baseline for correct alignment.
        float xUpperLeft = cursorX + ci->bitmapLeft;
        float yUpperLeft = cursorY + ci->bitmapTop;
        float charWidth  = ci->bitmapWidth;
        float charHeight = ci->bitmapHeight;

        textWidth += charWidth;

        // Update min/max Y coordinate (clip space) of bounding box.
        maxYOfBBox = max(maxYOfBBox, yUpperLeft);
        minYOfBBox = min(minYOfBBox, yUpperLeft - charHeight);

        // Structure of ctriangles: x1, y1, s1, t1, x2, y2, ... with x, y being
        // the clip space coordinates of each point ([-1..1x-1..1]) and s, t
        // being the corresponding texture coordinates ([0..1x0..1]).

        // FIRST TRIANGLE (lower left)
        // lower left corner of the quad
        coordinates[n++] = xUpperLeft;
        coordinates[n++] = yUpperLeft - charHeight;
        coordinates[n++] = ci->xOffset_TexCoords;
        coordinates[n++] = ci->bitmapHeight / textureAtlasHeight;
        // upper left corner of the quad
        coordinates[n++] = xUpperLeft;
        coordinates[n++] = yUpperLeft;
        coordinates[n++] = ci->xOffset_TexCoords;
        coordinates[n++] = 0;
        // lower right corner of the quad
        coordinates[n++] = xUpperLeft + charWidth;
        coordinates[n++] = yUpperLeft - charHeight;
        coordinates[n++] = ci->xOffset_TexCoords + ci->bitmapWidth / textureAtlasWidth;
        coordinates[n++] = ci->bitmapHeight / textureAtlasHeight;

        // SECOND TRIANGLE (upper right)
        // upper left corner of the quad
        coordinates[n++] = xUpperLeft;
        coordinates[n++] = yUpperLeft;
        coordinates[n++] = ci->xOffset_TexCoords;
        coordinates[n++] = 0;
        // lower right corner of the quad
        coordinates[n++] = xUpperLeft + charWidth;
        coordinates[n++] = yUpperLeft - charHeight;
        coordinates[n++] = ci->xOffset_TexCoords + ci->bitmapWidth / textureAtlasWidth;
        coordinates[n++] = ci->bitmapHeight / textureAtlasHeight;
        // upper right corner of the quad
        coordinates[n++] = xUpperLeft + charWidth;
        coordinates[n++] = yUpperLeft;
        coordinates[n++] = ci->xOffset_TexCoords + ci->bitmapWidth / textureAtlasWidth;
        coordinates[n++] = 0;

        // Advance the cursor position for the next character.
        cursorX += ci->advanceX;
    }

    // Store bounding box coordinates:
    float pad = textureAtlasHeight * bboxPadFraction;
    // lower left (lower left x, y of first triangle from ctriangles).
    coordinates[0] = coordinates[8  ] - pad;
    coordinates[1] = minYOfBBox       - pad;
    // upper left
    coordinates[2] = coordinates[8  ] - pad;
    coordinates[3] = maxYOfBBox       + pad;
    // lower right
    coordinates[4] = coordinates[n-4] + pad;
    coordinates[5] = minYOfBBox       - pad;
    // upper right (upper right x, y of the last triangle).
    coordinates[6] = coordinates[n-4] + pad;
    coordinates[7] = maxYOfBBox       + pad;

    QVector2D offset = QVector2D(0, 0); // (anchor == BASELINELEFT)
    if (anchor == BASELINERIGHT)
        offset = QVector2D(coordinates[8]-coordinates[n-4], 0);
    if (anchor == BASELINECENTRE)
        offset = QVector2D((coordinates[8]-coordinates[n-4])/2., 0);
    else if (anchor == UPPERLEFT)
        offset = QVector2D(0, cursorY-maxYOfBBox);
    else if (anchor == UPPERRIGHT)
        offset = QVector2D(coordinates[8]-coordinates[n-4], cursorY-maxYOfBBox);
    else if (anchor == UPPERCENTRE)
        offset = QVector2D((coordinates[8]-coordinates[n-4])/2., cursorY-maxYOfBBox);
    else if (anchor == LOWERLEFT)
        offset = QVector2D(0, cursorY-minYOfBBox);
    else if (anchor == LOWERRIGHT)
        offset = QVector2D(coordinates[8]-coordinates[n-4], cursorY-minYOfBBox);
    else if (anchor == LOWERCENTRE)
        offset = QVector2D((coordinates[8]-coordinates[n-4])/2., cursorY-minYOfBBox);
    else if (anchor == MIDDLELEFT)
        offset = QVector2D(0, cursorY-minYOfBBox - (maxYOfBBox-minYOfBBox)/2.);
    else if (anchor == MIDDLERIGHT)
        offset = QVector2D(coordinates[8]-coordinates[n-4],
                           cursorY-minYOfBBox - (maxYOfBBox-minYOfBBox)/2.);
    else if (anchor == MIDDLECENTRE)
        offset = QVector2D((coordinates[8]-coordinates[n-4])/2.,
                           cursorY-minYOfBBox - (maxYOfBBox-minYOfBBox)/2.);

    // Apply offset to coordinates.
    for (int i = 0; i < 8; i += 2)
    {
        // bounding box
        coordinates[i]   += offset.x();
        coordinates[i+1] += offset.y();
    }

    for (int i = 8; i < n; i += 4)
    {
        // letter "triangles"
        coordinates[i]   += offset.x();
        coordinates[i+1] += offset.y();
    }

    // Create an MLabel object that stores the information necessary for
    // rendering the text later.
    MLabel* label = new MLabel();
    label->anchor           = QVector3D(x, y, z);
    label->anchorOffset     = QVector3D(); // can be set per-frame by "owner"
    label->coordinateSystem = coordsys;
    label->textColour       = colour;
    label->numCharacters    = text.size();
    label->size             = size;
    label->width            = textWidth + pad;
    label->drawBBox         = bbox;
    label->bboxColour       = bboxColour;

    // Make sure that "MGLResourcesManager" is the currently active context,
    // otherwise glDrawArrays on the VBO generated here will fail in any other
    // context than the currently active. The "MGLResourcesManager" context is
    // shared with all visible contexts, hence modifying the VBO there works
    // fine.
    MGLResourcesManager::getInstance()->makeCurrent();

    // Generate a vertex buffer object for the geometry/texture coordinates.
    glGenBuffers(1, &(label->vbo)); CHECK_GL_ERROR;
    LOG4CPLUS_TRACE(mlog, "uploading label \"" << text.toStdString()
                    << "\" to VBO " << label->vbo << flush);

    // Upload data to GPU.
    glBindBuffer(GL_ARRAY_BUFFER, label->vbo); CHECK_GL_ERROR;
	// glBufferData(GL_ARRAY_BUFFER, sizeof(coordinates), !! does only get size of float pointer !!
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * coordinates.size(), 
		coordinates.data(), GL_STATIC_DRAW); CHECK_GL_ERROR;
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

#ifdef USE_QOPENGLWIDGET
    MGLResourcesManager::getInstance()->doneCurrent();
#endif

    // Store label information in "labels" list.
    labelPool.insert(label);

    return label;
}


void MTextManager::removeText(MLabel *label)
{
    LOG4CPLUS_TRACE(mlog, "removing label on VBO " << label->vbo);
    glDeleteBuffers(1, &(label->vbo)); CHECK_GL_ERROR;
    labelPool.remove(label);
    delete label;
    label = 0;
}


void MTextManager::renderLabelList(MSceneViewGLWidget *sceneView,
                                   const QList<MLabel*>& labelList)
{
    // First draw the bounding boxes of all labels in the pool.
    bboxEffect->bindProgram("SimpleAnchor");

    // Make sure the bounding box doesn't obscure the characters.
    glPolygonOffset(.01f, 1.0f);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);

    for (int i = 0; i < labelList.size(); i++)
    {
        MLabel *label = labelList.at(i);
        if (!label->drawBBox) continue;

        // Scales to convert from pixel space to [-1..1] clip space.
        float scale  = label->size / textureAtlasHeight; // both in px
        float scaleX = scale * 2. / sceneView->getViewPortWidth(); // latter in px
        float scaleY = scale * 2. / sceneView->getViewPortHeight(); // latter in px

        QVector3D anchorInClipSpace;
        QVector3D anchorPlusOffset = label->anchor + label->anchorOffset;
        if (label->coordinateSystem == WORLDSPACE)
        {
            // Convert from world space to clip space.
            anchorInClipSpace = *(sceneView->getModelViewProjectionMatrix())
                    * anchorPlusOffset;
        }
        else if (label->coordinateSystem == LONLATP)
        {
            anchorInClipSpace = sceneView->lonLatPToClipSpace(anchorPlusOffset);
        }
        else
        {
            // labels[i]->coordinateSystem == CLIPSPACE
            anchorInClipSpace = anchorPlusOffset;
        }

        bboxEffect->setUniformValue("anchor", anchorInClipSpace);
        bboxEffect->setUniformValue("scale", QVector2D(scaleX, scaleY));
        bboxEffect->setUniformValue("colour", label->bboxColour);

        glBindBuffer(GL_ARRAY_BUFFER, label->vbo); CHECK_GL_ERROR;
        glVertexAttribPointer(SHADER_VERTEX_ATTRIBUTE, 2, GL_FLOAT,
                              GL_FALSE, 0, (const GLvoid *)0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glDisable(GL_POLYGON_OFFSET_FILL);

    textEffect->bindProgram("TextPool");

    glActiveTexture(GL_TEXTURE0 + textureUnit); CHECK_GL_ERROR;
    glBindTexture(GL_TEXTURE_2D, textureObjectName); CHECK_GL_ERROR;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); CHECK_GL_ERROR;
#ifdef USE_QOPENGLWIDGET
    glActiveTexture(GL_TEXTURE0);
#endif

    glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE); CHECK_GL_ERROR;
    glEnableVertexAttribArray(SHADER_TEXTURE_ATTRIBUTE); CHECK_GL_ERROR;

    for (int i = 0; i < labelList.size(); i++)
    {
        MLabel *label = labelList.at(i);

        // Scales to convert from pixel space to [-1..1] clip space.
        float scale  = label->size / textureAtlasHeight; // both in px
        float scaleX = scale * 2. / sceneView->getViewPortWidth(); // latter in px
        float scaleY = scale * 2. / sceneView->getViewPortHeight(); // latter in px

        QVector3D anchorInClipSpace;
        QVector3D anchorPlusOffset = label->anchor + label->anchorOffset;
        if (label->coordinateSystem == WORLDSPACE)
        {
            // Convert from world space to clip space.
            anchorInClipSpace = *(sceneView->getModelViewProjectionMatrix())
                    * anchorPlusOffset;
        }
        else if (label->coordinateSystem == LONLATP)
        {
            anchorInClipSpace = sceneView->lonLatPToClipSpace(anchorPlusOffset);
        }
        else
        {
            // labels[i]->coordinateSystem == CLIPSPACE
            anchorInClipSpace = anchorPlusOffset;
        }

        textEffect->setUniformValue("anchor", anchorInClipSpace);
        textEffect->setUniformValue("scale", QVector2D(scaleX, scaleY));
        textEffect->setUniformValue("colour", label->textColour);
        textEffect->setUniformValue("textAtlas", textureUnit);

        glBindBuffer(GL_ARRAY_BUFFER, label->vbo); CHECK_GL_ERROR;
        glVertexAttribPointer(SHADER_VERTEX_ATTRIBUTE, 2, GL_FLOAT,
                              GL_FALSE, 4 * sizeof(float),
                              (const GLvoid *)(8 * sizeof(float))); // offset for vertex positions
        glVertexAttribPointer(SHADER_TEXTURE_ATTRIBUTE, 2, GL_FLOAT,
                              GL_FALSE, 4 * sizeof(float),
                              (const GLvoid *)(10 * sizeof(float))); // offset textures
        glDrawArrays(GL_TRIANGLES, 0, 6 * label->numCharacters);
    }

    glDisableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);
    glDisableVertexAttribArray(SHADER_TEXTURE_ATTRIBUTE);

    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTextManager::initializeActorResources()
{
    // Bind the texture object (for the texture atlas) to unit 0.
    textureUnit = 0;

    // Font file and size must be specified before this method is called.
    if (fontfile.isEmpty() || (fontsize < 0))
    {
        throw MInitialisationError("Font file and font size must be specified "
                                   "before OpenGL intialisation of "
                                   "MTextManager.", __FILE__, __LINE__);
    }

    generateTextureAtlas(fontfile, fontsize);

    // Generate vertex buffer objects.
    glGenBuffers(1, &directRenderingTextVBO); CHECK_GL_ERROR;
    glGenBuffers(1, &directRenderingBBoxVBO); CHECK_GL_ERROR;

    // Load shader.
    bool loadShaders = false;
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    loadShaders |= glRM->generateEffectProgram("text_bbox", bboxEffect);
    loadShaders |= glRM->generateEffectProgram("text_shader", textEffect);

    if (loadShaders) reloadShaderEffects();
}


void MTextManager::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    Q_UNUSED(sceneView);
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MTextManager::generateTextureAtlas(QString fontFile, int size)
{
    // Reference:
    // http://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Text_Rendering_01

    // Consider all ASCII characters in the specified range when creating the
    // texture atlas.
    int minASCII = 32;
    int maxASCII = 128;

    // Insert a horizontal padding of "pad" pixels between all characters to
    // avoid rendering artefacts due to interpolation in texture space (e.g.
    // a vertical line on the right of a rendered "O" resulting from the right
    // border of the adjacent "P". One pixel should usually be enough.
    int pad = 1;

    // Load the font into an FreeType "FT_Face" object.
    FT_Face face;
    if (FT_New_Face(ft, fontFile.toStdString().c_str(), 0, &face))
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: could not load font from file "
                        << fontFile.toStdString());
        return;
    }

    // Tell FreeType about the requested font size.
    FT_Set_Pixel_Sizes(face, 0, size);
    // Obtain a pointer to the "glyph" object of the font (this is a shortcut
    // for the code below).
    FT_GlyphSlot g = face->glyph;

    // Determine the texture image size that is required to accomodate all
    // characters.
    textureAtlasWidth  = 0;
    textureAtlasHeight = 0;

    for (int i = minASCII; i < maxASCII; i++)
    {
        if (FT_Load_Char(face, i, FT_LOAD_RENDER))
        {
            LOG4CPLUS_WARN_FMT(mlog, "loading character %c failed\n", i);
            continue;
        }
        textureAtlasWidth += g->bitmap.width + pad;
        textureAtlasHeight = max(int(textureAtlasHeight), int(g->bitmap.rows));
    }

    LOG4CPLUS_DEBUG_FMT(mlog, "\ttexture atlas: width %i, height %i",
                        textureAtlasWidth, textureAtlasHeight);

    // Check if a texture of this size is supported by the graphics hardware.
    GLint maxTexSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    if ((textureAtlasWidth > maxTexSize) || (textureAtlasHeight > maxTexSize))
    {
        QString error = QString(
                    "texture atlas is too large, textures can "
                    "have a maximum size of %1 pixels in each "
                    "dimension -- try to decrease font size").arg(maxTexSize);
        throw MInitialisationError(error.toStdString(), __FILE__, __LINE__);
    }

    // Obtain a texture object name from the resources manager. If successfull,
    // create an empty texture of the determined size, fill it with zeros, then
    // write bitmaps of the individual characters to the texture.

//TODO: Encode font name and size in texture key to allow multiple texture atlases.

    if (MGLResourcesManager::getInstance()->generateTexture(
                "fontAtlas", &textureObjectName))
    {
        LOG4CPLUS_DEBUG(mlog, "\tloading texture atlas to texture object "
                        << textureObjectName);

        // Bind texture object and set its parameters.
        glActiveTexture(GL_TEXTURE0 + textureUnit); CHECK_GL_ERROR;
        glBindTexture(GL_TEXTURE_2D, textureObjectName); CHECK_GL_ERROR;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); CHECK_GL_ERROR;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        // Allocate GPU memory and fill the texture with zeros. If this is
        // omitted the texture contains undefined regions (e.g. below a
        // vertically small character such as "-"), which result in similar
        // rendering artefacts as the "O"-"P" problem described above.

        // NOTE: The texture is stored in the alpha channel -- this way the
        // alpha component can be directly used in the shader to render the
        // text with "transparent" whitespace, e.g. in "O".

        unsigned char* nullData = 
			new unsigned char[textureAtlasWidth * textureAtlasHeight];
        for (int i = 0; i < textureAtlasWidth * textureAtlasHeight; i++)
            nullData[i] = 0;

        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA,
                     textureAtlasWidth, textureAtlasHeight,
                     0, GL_ALPHA, GL_UNSIGNED_BYTE,
                     nullData); CHECK_GL_ERROR;

        // Load a bitmap of each character (glyph) and write it to the texture.
        // Save information about the position of the glyph, its width and
        // height and alignment in the array "characterInfo".

        // Start at the left of the image and advance this "cursor" with every
        // character.
        int xCoord_TextureSpace = 0;

        for (int asciiCode = minASCII; asciiCode < maxASCII; asciiCode++)
        {
            if (FT_Load_Char(face, asciiCode, FT_LOAD_RENDER))
                continue; // returns 0 on success, otherwise an error code

            // Upload image to texture.
            glTexSubImage2D(GL_TEXTURE_2D, 0, xCoord_TextureSpace, 0,
                            g->bitmap.width, g->bitmap.rows,
                            GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);
            CHECK_GL_ERROR;

            // Store glyph information (texture coordinates, width and height)
            // of the just uploaded character.
            characterInfo[asciiCode].advanceX     = g->advance.x >> 6;
            characterInfo[asciiCode].advanceY     = g->advance.y >> 6;
            characterInfo[asciiCode].bitmapWidth  = g->bitmap.width;
            characterInfo[asciiCode].bitmapHeight = g->bitmap.rows;
            characterInfo[asciiCode].bitmapLeft   = g->bitmap_left;
            characterInfo[asciiCode].bitmapTop    = g->bitmap_top;
            characterInfo[asciiCode].xOffset_TexCoords
                    = (float) xCoord_TextureSpace / textureAtlasWidth;

            LOG4CPLUS_TRACE(
                        mlog, "\tinfo for character " << (char)asciiCode << ":"
                        << " advX=" << characterInfo[asciiCode].advanceX
                        << " advY=" << characterInfo[asciiCode].advanceY
                        << " bitW=" << characterInfo[asciiCode].bitmapWidth
                        << " bitH=" << characterInfo[asciiCode].bitmapHeight
                        << " bitL=" << characterInfo[asciiCode].bitmapLeft
                        << " bitT=" << characterInfo[asciiCode].bitmapTop
                        << " texX=" << characterInfo[asciiCode].xOffset_TexCoords);

            // Advance cursor.
            xCoord_TextureSpace += g->bitmap.width + pad;
        }

        // Generate a mipmap for the texture.
        glGenerateMipmap(GL_TEXTURE_2D); CHECK_GL_ERROR;

        GLfloat largestAnisotropyLevel;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largestAnisotropyLevel);
        LOG4CPLUS_DEBUG(mlog, "\tlargest anisotropy level: "
                        << largestAnisotropyLevel);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        largestAnisotropyLevel); CHECK_GL_ERROR;

#ifdef USE_QOPENGLWIDGET
        glActiveTexture(GL_TEXTURE0);
#endif

		delete[] nullData;
    }
}

} // namespace Met3D
