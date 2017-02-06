/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016 Marc Rautenhaus
**  Copyright 2016 Bianca Tost
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
#ifndef MSPATIAL1DTRANSFERFUNCTION_H
#define MSPATIAL1DTRANSFERFUNCTION_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <QtProperty>
#include <QtCore>

// local application imports
#include "gxfw/transferfunction.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/texture.h"

class MGLResourcesManager;
class MSceneViewGLWidget;


namespace Met3D
{

/**
  @brief MSpatial1DTransferFunction represents a texturebar, providing both a
  2D-texture that can be used as a lookup table by actors to map scalar values
  to colours, and the geometric representation of the texturebar to be
  drawn into the scene.

  The user can control the mapping scalar value to colour value as well as
  geometric properties of the rendered texturebar (position, size, labelling).

  To allow the user more flexibility in the use of the texture, it is possible
  to choose either to use the texture as it is or to use one or more of its
  channels as an alpha map and set the colour to a constant value. Since the
  user might use a black and white texture with black representing the structure
  and white the transparent part, it is possible to invert the alpha value used.

  To simplify loading textures and since GL_TEXTURE_2D_ARRAY only allows
  textures of the same size, it is only allow to load a set of textures with
  all having the same size. In a different approach one could implement to ask
  the user to scale the textures to the same size if loaded with different
  sizes, but this could lead to unexpected behaviour and confusion.

  To achieve the best result it is advised to use a set of textures in which the
  sparser textures are part of the denser textures.
  */
class MSpatial1DTransferFunction : public MTransferFunction
{
public:
    MSpatial1DTransferFunction();
    ~MSpatial1DTransferFunction();

    void reloadShaderEffects();

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

    QString getSettingsID() override { return "TransferFunction1DSpatialTexture"; }

    /**
      Returns the texture that represents the spatial transfer function texture.
      */
    GL::MTexture* getTexture() { return tfTexture; }

    /**
      Returns the minimum scalar value of the current transfer function
      settings.
      */
    float getMinimumValue() { return minimumValue; }

    /**
      Returns the maximum scalar value of the current transfer function
      settings.
      */
    float getMaximumValue() { return maximumValue; }

    float getNumLevels() { return numLevels; }

    bool getClampMaximum() { return clampMaximum; }

    float getInterpolationRange() { return interpolationRange; }

    bool getInvertAlpha() { return invertAlpha; }

    bool getUseConstantColour() { return useConstantColour; }

    QColor getConstantColour() { return constantColour; }

    float getTextureScale() { return (float)textureScale; }

    float getTextureAspectRatio()
    { return (float)currentTextureHeight / (float)currentTextureWidth; }

    void setMinimumValue(float value);

    void setMaximumValue(float value);

    void setValueDecimals(int decimals);

    void setPosition(QRectF position);

    enum AlphaBlendingMode {
        AlphaChannel = 0,
        RedChannel,
        GreenChannel,
        BlueChannel,
        RGBAverage,
        None
    };
    AlphaBlendingMode getAlphaBlendingMode() { return alphaBlendingMode; }

    QString transferFunctionName();

protected:
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

private:
    /**
      Generates the colourbar texture with the colour mapping specified by the
      user and uploads a 1D-texture to the GPU.

      @see MColourmapPool
      @see MColourmap
      */
    void generateTransferTexture(int level, bool recreate);

    /**
      Creates geometry for a box filled with the colourbar texture and for tick
      marks, and places labels at the tick marks.
      */
    void generateTextureBarGeometry();

    void loadImagesFromPaths(QStringList pathList);

    std::shared_ptr<GL::MShaderEffect> simpleGeometryShader;
    std::shared_ptr<GL::MShaderEffect> colourbarShader;

    GL::MTexture *tfTexture;
    GLint textureUnit;

    GL::MVertexBuffer *vertexBuffer;
    uint numVertices;

    QVector<QImage> loadedImages;

    // General properties.
    QtProperty *positionProperty;

    // Properties related to ticks and labels.
    QtProperty *maxNumTicksProperty;
    uint        numTicks;
    QtProperty *maxNumLabelsProperty;
    QtProperty *tickWidthProperty;
    QtProperty *labelSpacingProperty;
    QtProperty *scaleFactorProperty;

    // Properties related to texture levels.
    QtProperty          *levelsPropertiesSubGroup;
    int                  numLevels;
    QtProperty          *loadLevelsImagesProperty;
    QtProperty          *pathToLoadedImagesProperty;
    QVector<QString>     pathsToLoadedImages;
    QString              imagePathsString;
    QtProperty          *useMirroredRepeatProperty;
    bool                 useMirroredRepeat;


    // Properties related to value range.
    QtProperty *rangePropertiesSubGroup;
    QtProperty *valueDecimalsProperty;
    QtProperty *minimumValueProperty;
    QtProperty *maximumValueProperty;
    double      minimumValue;
    double      maximumValue;
    QtProperty *clampMaximumProperty;
    bool        clampMaximum;
    QtProperty *interpolationRangeProperty;
    // Defines scalar range of interpolated texture.
    double      interpolationRange;

    // Properties related to alpha blending.
    QtProperty *alphaBlendingPropertiesSubGroup;
    QtProperty *alphaBlendingModeProperty;
    QtProperty *invertAlphaProperty;
    QtProperty *useConstantColourProperty;
    QtProperty *constantColourProperty;
    QtProperty *useWhiteBgForBarProperty;
    AlphaBlendingMode alphaBlendingMode;
    bool invertAlpha;
    bool useConstantColour;
    QColor constantColour;
    bool useWhiteBgForBar;

    // Properties related to texture scale.
    QtProperty *textureScalePropertiesSubGroup;
    QtProperty *textureScaleDecimalsProperty;
    QtProperty *textureScaleProperty;
    // Scale of texture width with respect in longitude. Texture height is
    // scaled keeping the aspect ratio.
    double      textureScale;

    int         currentTextureWidth;
    int         currentTextureHeight;
};


class MSpatial1DTransferFunctionFactory : public MAbstractActorFactory
{
public:
    MSpatial1DTransferFunctionFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override { return new MSpatial1DTransferFunction(); }
};


} // namespace Met3D

#endif // MSPATIAL1DTRANSFERFUNCTION_H
