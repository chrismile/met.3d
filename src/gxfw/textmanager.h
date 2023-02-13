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
#ifndef TEXTMANAGER_H
#define TEXTMANAGER_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <ft2build.h>  // FreeType 2 library
#include FT_FREETYPE_H
#include <QtCore>

// local application imports
#include "gxfw/mtypes.h"
#include "gxfw/mactor.h"
#include "gxfw/gl/shadereffect.h"

namespace Met3D
{

// forward declarations
class MGLResourcesManager;
class MSceneViewGLWidget;


/**
  @brief TextureAtlasCharacterInfo contains information about the bitmap of a
  specific character (glyph) in the texture atlas.
  */
struct MTextureAtlasCharacterInfo {
    // Advance cursor by these values after the character has been drawn.
    float advanceX;
    float advanceY;
    // "bitmap" refers to the character bitmap stored in the texture atlas.
    // Values are given in pixels of the character bitmap; left and top refer
    // to the indention of the bitmap relative to the cursor position.
    float bitmapWidth;
    float bitmapHeight;
    float bitmapLeft;
    float bitmapTop;
    // Texture coordinates of where the character starts in the texture.
    float xOffset_TexCoords;
};


/**
  @brief MTextManager manages all labels (i.e. text strings) that appear in a
  scene.

  Labels can be added to the render queue with the addXY() methods. These
  methods create the geometry for a label and upload it to the GPU (using
  VBOs). All registered labels are rendered when renderToCurrentContext() is
  called by a @ref MSceneViewGLWidget.

  Label rendering is implemented using a texture atlas generated with FreeType
  2. Compare this tutorial
  (http://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Text_Rendering_02)
  and this StackOverflow post
  (http://stackoverflow.com/questions/2071621/opengl-live-text-rendering) for
  more information.

  @todo Refactor to use GL::MTexture and GL::MVertexBuffer
  */
class MTextManager : public MActor
{
public:
    enum TextAnchor {
        BASELINELEFT   =  0,
        BASELINERIGHT  =  1,
        BASELINECENTRE =  2,
        UPPERLEFT      =  3,
        UPPERRIGHT     =  4,
        UPPERCENTRE    =  5,
        LOWERLEFT      =  6,
        LOWERRIGHT     =  7,
        LOWERCENTRE    =  8,
        MIDDLELEFT     =  9,
        MIDDLERIGHT    = 10,
        MIDDLECENTRE   = 11
    };

    enum CoordinateSystem {
        CLIPSPACE  = 0,
        WORLDSPACE = 1,
        LONLATP    = 2
    };

    MTextManager();

    ~MTextManager();

    /**
      Specify the font and font size used for the texture atlas. This method
      must be called before the OpenGL initialisation of this actor is called.
     */
    void setFont(QString fontfile, int fontsize);

    void initializePerGLContextResources(MSceneViewGLWidget *sceneView);

    void reloadShaderEffects();

    QString getSettingsID() { return "TextManager"; }

    /**
      Immediately renders the string @p text to the image at the point
      specified by the coordinates @p x and @p y @p z in the coordinate system
      specified by @p coordsys (e.g., if coordsys == CLIPSPACE, x, y, z are in
      [-1..1]). The @p size of the text is specified in pixels (actual pixels
      on the screen). renderText() computes the geometry for the letter boxes
      and uploads it to the GPU vertex buffer via glBufferData() (position and
      texture coordinates are stored in one interleaved VBO).

      @note only use this function for text that changes every frame, otherwise
            use @ref addText()
      @note text rendered in clip space at z = -1 will be rendered above all
            other text
      */
    void renderText(QString text, CoordinateSystem coordsys,
                    float x, float y, float z, float size,
                    QColor colour, MSceneViewGLWidget *sceneView,
                    TextAnchor anchor=BASELINELEFT,
                    bool bbox=false, QColor bboxColour=QColor(0,0,0,50),
                    float bboxPad=2);

    /**
      Same as @ref renderText() , but renders only the single character
      @p ch.

      @note only use this function for characters that change every frame
      */
    void renderChar(const char *ch, CoordinateSystem coordsys,
                    float x, float y, float z, float size,
                    QColor colour, MSceneViewGLWidget *sceneView,
                    bool bbox=false, QColor bboxColour=QColor(0,0,0,50),
                    float bboxPad=2);

    /**
      Generates the geometry for a text label, but unlike @ref renderText(),
      addText() does not immediately render the label but uploads the geometry
      to a VBO for later rendering with @ref renderToCurrentContext().

      Arguments are the same as for @ref renderText().

      @returns An integer index identifying the label. The label can be removed
      from the pool with @ref removeText() and this index.
      */
    MLabel* addText(QString text, CoordinateSystem coordsys,
                    float x, float y, float z, float size,
                    QColor colour, TextAnchor anchor=BASELINELEFT,
                    bool bbox=false, QColor bboxColour=QColor(0,0,0,50),
                    float bboxPadFraction=0.1);

    /**
      Generates the geometry for a vertical text label, but unlike @ref renderText(),
      addText() does not immediately render the label but uploads the geometry
      to a VBO for later rendering with @ref renderToCurrentContext().

      Arguments are the same as for @ref renderText().

      @returns An integer index identifying the label. The label can be removed
      from the pool with @ref removeText() and this index.
      */
    MLabel* addTextVertical(QString text, CoordinateSystem coordsys,
                    float x, float y, float z, float size,
                    QColor colour, TextAnchor anchor=BASELINELEFT,
                    bool bbox=false, QColor bboxColour=QColor(0,0,0,50),
                    float bboxPadFraction=0.1, bool orientationCcw = false);

    /**
      Removes the label with index @p id (obtained from @ref addText()) from
      the label pool.
      */
    void removeText(MLabel* label);

    /**
      */
    void renderLabelList(MSceneViewGLWidget *sceneView,
                         const QList<MLabel *> &labelList);

    /**
      @deprecated

      Same as @ref renderText() , but renders each character individually (i.e.
      one call of glDrawArrays() for each chracter instead of one call for the
      entire string). @p x and @p y have to be specified in clip space, the z
      coordinate is fixed to 0.

      @note use this function only for performance comparisons
      */
    void renderText_2DClip_i(QString text, float x, float y, float size,
                             QColor colour, MSceneViewGLWidget *sceneView);

protected:
    void initializeActorResources();

    /**
      Renders the text queue. Overrides the virtual method
      MActor::renderToCurrentContext().

      @todo Possibly displaces labels that collide and draws a line from the
      desired position to the actual label.
      */
    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

private:
    /**
      Generates a texture atlas of the font stored in @p fontFile, i.e. a
      texture containing images of all ASCII characters of the font. The
      generated texture us uploaded to GPU memory and accessed from the render
      methods of this class. @p fontFile must contain the complete path to the
      font file. The @p size of the characters in the texture is specified in
      pixels. The value should be close to the most common screen size in which
      the font will be used later.

      The method uses the FreeType2 library to access bitmap images of each
      character in the font file.
      */
    void generateTextureAtlas(QString fontFile, int size);

    // Instance of the FreeType2 library.
    FT_Library ft;

    QString fontfile;
    int fontsize;

    std::shared_ptr<GL::MShaderEffect> textEffect;
    std::shared_ptr<GL::MShaderEffect> bboxEffect;

    // The texture contains the texture atlas for the loaded font.
    GLuint textureObjectName;
    int textureUnit;

    // characterInfo provides information on which character can be found
    // where in the texture map and how it can be correctly aligned
    MTextureAtlasCharacterInfo *characterInfo;
    int textureAtlasHeight;  // in pixels of the image uploaded to GPU
    int textureAtlasWidth;

    // One vertex buffer for the geometry, can be shared across OpenGL contexts
    // (= scene views).
    GLuint directRenderingTextVBO;
    GLuint directRenderingBBoxVBO;

    // The hash set "labelPool" stores the pool of labels (addText(),
    // removeText(), renderToCurrentContext()).
    QSet<MLabel*> labelPool;
};

} // namespace Met3D

#endif // TEXTMANAGER_H
