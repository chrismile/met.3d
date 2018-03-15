/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016-2018 Marc Rautenhaus
**  Copyright 2016-2018 Bianca Tost
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

// related third party imports
#include <QtCore>

// local application imports
#include "gxfw/transferfunction.h"

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
    MSpatial1DTransferFunction(QObject *parent = 0);
    ~MSpatial1DTransferFunction();

    static QString staticActorType()
    { return "Transfer function scalar to texture"; }

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

    void setValueSignificantDigits(int significantDigits);

    void setValueStep(double step);

    QString getSettingsID() override { return "TransferFunction1DSpatialTexture"; }

    float getNumLevels() { return numLevels; }

    bool getClampMaximum() { return clampMaximum; }

    float getInterpolationRange() { return interpolationRange; }

    bool getInvertAlpha() { return invertAlpha; }

    bool getUseConstantColour() { return useConstantColour; }

    QColor getConstantColour() { return constantColour; }

    float getTextureScale() { return (float)textureScale; }

    float getTextureAspectRatio()
    { return (float)currentTextureHeight / (float)currentTextureWidth; }

    enum AlphaBlendingMode {
        AlphaChannel = 0,
        RedChannel,
        GreenChannel,
        BlueChannel,
        RGBAverage,
        None
    };
    AlphaBlendingMode getAlphaBlendingMode() { return alphaBlendingMode; }

protected:
    void generateTransferTexture() override;

    /**
      Creates geometry for a box filled with the texture and for tick
      marks, and places labels at the tick marks.
      */
    void generateBarGeometry() override;

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

private:
    /**
      @brief Method to create the 2D texture array with mip-mapping containing
      the texture images and uploads the 3D-texture to the GPU and/or store the
      @p level-th image at @p level in the texture.

      Creates a new 2D texture array if @ref tfTexture is nullptr or
      @p recreate is true. Takes the image stored at @p level in
      @ref loadedImages and stores its content in the 2D texture array.
      */
    void generateTransferTexture(int level, bool recreate);

    void loadImagesFromPaths(QStringList pathList);

    QVector<QImage> loadedImages;

    // General properties.

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
