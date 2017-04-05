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
#ifndef TRANSFERFUNCTION1D_H
#define TRANSFERFUNCTION1D_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <QtProperty>

// local application imports
#include "gxfw/transferfunction.h"
#include "gxfw/colourmap.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/texture.h"

class MGLResourcesManager;
class MSceneViewGLWidget;


namespace Met3D
{

namespace TFEditor
{
class MTransferFunctionEditor;
}

/**
  @brief TransferFunction1D represents a colourbar, providing both a 1D-texture
  that can be used as a lookup table by actors that map a scalar value to a
  colour, and the geometric representation of the colourbar to be drawn into
  the scene.

  The user can control the mapping scalar value to colour value as well as
  geometric properties of the rendered colourbar (position, size, labelling).
  The colourbar has a Matplotlib-like appearance.
  */
class MTransferFunction1D : public MTransferFunction
{
    Q_OBJECT
public:
    MTransferFunction1D();
    ~MTransferFunction1D();

    void reloadShaderEffects();

    /**
      Returns the texture that represents the transfer function texture.
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
    float getMaximimValue() { return maximumValue; }

    /**
      Select a predefined colourmap. @p name must be the name of a colourmap
      available from the colourmap pool.

      @see MColourmapPool
      @see MColourmap
      */
    void selectPredefinedColourmap(QString name, bool reversed=false,
                                   int saturation=0, int lightness=0);

    enum MHCLType { DIVERGING = 0, QUALITATIVE = 1, SEQENTIAL_SINGLE_HUE = 2,
                    SEQUENTIAL_MULTIPLE_HUE = 3 };

    /**
      Select an HCL colourmap. Parameters are explained in
      @ref MHCLColourmap::MHCLColourmap().

     @p type can be one of DIVERGING, QUALITATIVE, SEQENTIAL_SINGLE_HUE,
     SEQUENTIAL_MULTIPLE_HUE. This emulates behaviour on http://hclwizard.org.
     */
    void selectHCLColourmap(
            MHCLType type,
            float hue1, float hue2, float chroma1, float chroma2,
            float luminance1, float luminance2, float power1, float power2,
            float alpha1, float alpha2, float poweralpha,
            bool reversed=false);

    void  selectEditor();

    void selectHSVColourmap(QString vaporXMLFile, bool reversed=false);

    void setMinimumValue(float value);

    void setMaximumValue(float value);

    void setValueDecimals(int decimals);

    void setPosition(QRectF position);

    void setSteps(int steps);

    void setNumTicks(int num);

    void setNumLabels(int num);

    QString getSettingsID() override { return "TransferFunction1D"; }

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

    QString transferFunctionName();

protected:
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

public slots:
    void onEditorTransferFunctionChanged();

private:
    /**
      Generates the colourbar texture with the colour mapping specified by the
      user and uploads a 1D-texture to the GPU.

      @see MColourmapPool
      @see MColourmap
      */
    void generateTransferTexture();

    /**
      Creates geometry for a box filled with the colourbar texture and for tick
      marks, and places labels at the tick marks.
      */
    void generateColourBarGeometry();

    void updateHCLProperties();

    void updateHSVProperties();

    std::shared_ptr<GL::MShaderEffect> colourbarShader;
    std::shared_ptr<GL::MShaderEffect> simpleGeometryShader;

    GL::MTexture *tfTexture;
    GLint textureUnit;

    GL::MVertexBuffer *vertexBuffer;
    uint numVertices;

    MColourmapPool colourmapPool;

    TFEditor::MTransferFunctionEditor *editor;

    // General properties.
    QtProperty *positionProperty;
    bool        enableAlpha;
    QtProperty *enableAlphaInTFProperty;
    QtProperty *reverseTFRangeProperty;

    // Properties related to ticks and labels.
    QtProperty *maxNumTicksProperty;
    uint        numTicks;
    QtProperty *maxNumLabelsProperty;
    QtProperty *tickWidthProperty;
    QtProperty *labelSpacingProperty;
    QtProperty *scaleFactorProperty;

    // Properties related to value range.
    QtProperty *rangePropertiesSubGroup;
    QtProperty *valueDecimalsProperty;
    QtProperty *minimumValueProperty;
    QtProperty *maximumValueProperty;
    float       minimumValue;
    float       maximumValue;
    QtProperty *numStepsProperty;

    // Type of colourmap.
    enum MColourmapType { PREDEFINED = 0, HCL = 1, HSV = 2, EDITOR = 3};

    QtProperty *colourmapTypeProperty;

    QtProperty *predefCMapPropertiesSubGroup;
    QtProperty *predefColourmapProperty;
    QtProperty *predefLightnessAdjustProperty;
    QtProperty *predefSaturationAdjustProperty;

    QtProperty *hclCMapPropertiesSubGroup;
    QtProperty *hclTypeProperty;
    QtProperty *hclHue1Property;
    QtProperty *hclHue2Property;
    QtProperty *hclChroma1Property;
    QtProperty *hclChroma2Property;
    QtProperty *hclLuminance1Property;
    QtProperty *hclLuminance2Property;
    QtProperty *hclPower1Property;
    QtProperty *hclPower2Property;
    QtProperty *hclAlpha1Property;
    QtProperty *hclAlpha2Property;
    QtProperty *hclPowerAlphaProperty;
    QtProperty *hclReverseProperty;

    QtProperty *hsvCMapPropertiesSubGroup;
    QtProperty *hsvLoadFromVaporXMLProperty;
    QString     hsvVaporXMLFilename;
    QtProperty *hsvVaporXMLFilenameProperty;

    QtProperty *editorPropertiesSubGroup;
    QtProperty *editorClickProperty;
};


class MTransferFunction1DFactory : public MAbstractActorFactory
{
public:
    MTransferFunction1DFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override { return new MTransferFunction1D(); }
};


} // namespace Met3D

#endif // TRANSFERFUNCTION1D_H
