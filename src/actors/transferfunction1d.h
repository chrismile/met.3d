/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2017      Bianca Tost
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

// related third party imports

// local application imports
#include "gxfw/transferfunction.h"
#include "gxfw/colourmap.h"

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
    MTransferFunction1D(QObject *parent = 0);
    ~MTransferFunction1D();

    static QString staticActorType()
    { return "Transfer function scalar to colour (colour map)"; }

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

    void setSteps(int steps);

    QString getSettingsID() override { return "TransferFunction1D"; }

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

    /**
      Returns the color value (rgba) of the corresponding scalar value based
      on the user-defined colour mapping and range boundaries.
      @param scalar
      @return interpolated color value as QColor
     */
    QColor getColorValue(const float scalar) const;

    inline const std::vector<unsigned char> &getColorValuesByteArray() const { return colorValues; }

protected:
    /**
      Generates the colourbar texture with the colour mapping specified by the
      user and uploads a 1D-texture to the GPU.

      @see MColourmapPool
      @see MColourmap
      */
    void generateTransferTexture() override;

    /**
      Creates geometry for a box filled with the colourbar texture and for tick
      marks, and places labels at the tick marks.
      */
    void generateBarGeometry() override;

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

public slots:
    void onEditorTransferFunctionChanged();

private:

    void updateHCLProperties();

    void updateHSVProperties();

    std::vector<unsigned char> colorValues;

    // Type of colourmap.
    enum MColourmapType { INVALID = -1, HCL = 0, EDITOR = 1, PREDEFINED = 2,
                          HSV = 3 };

    /** Returns the name of the given colour map type as QString. */
    QString colourMapTypeToString(MColourmapType colourMapType);

    /** Returns enum associated with the given name. Returns Invalid if no mode
        exists with the given name. */
    MColourmapType stringToColourMapType(QString colourMapTypeName);

    MColourmapPool colourmapPool;

    TFEditor::MTransferFunctionEditor *editor;

    // General properties.
    bool        enableAlpha;
    QtProperty *enableAlphaInTFProperty;
    QtProperty *reverseTFRangeProperty;

    // Properties related to ticks and labels.
    QtProperty *scaleFactorProperty;

    // Properties related to value range.
    QtProperty *numStepsProperty;

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
