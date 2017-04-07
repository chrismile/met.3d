/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Fabian Schöttl
**  Copyright 2017 Bianca Tost
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
#ifndef COLORPICKER_H
#define COLORPICKER_H

// standard library imports

// related third party imports
#include <QDialog>
#include <QDoubleSpinBox>
#include <QVector2D>

// local application imports
#include "colour.h"

namespace Met3D
{
namespace TFEditor
{

struct MColourNodes;

enum HCLType1D
{
    Hue,
    Chroma,
    Luminance
};

enum HCLType2D
{
    HueChroma,
    HueLuminance,
    ChromaLuminance
};

class MHCLColorRangeWidget;
class MHCLColorRange2DWidget;

/**
  @brief Dialog which lets the user pick a color from the hcl colorspace.
  It offers three sliders to separately adjust hue, chroma and luminance of the
  color, as well as an additional 2D-colorplot.
 */
class MHCLColorPicker : public QDialog
{
    Q_OBJECT

    friend class MHCLColorRangeWidget;
    friend class MHCLColorRange2DWidget;

public:
    typedef MColourHCL16 Color;

public:
    MHCLColorPicker(MColourNodes *colourNodes, QWidget *parent = nullptr);

    void setCurrentIndex(int index);

    const MColourHCL16& color() const;
    MColourHCL16& color();

private slots:
    void changed(bool updateBoxes = true);
    void hueBoxChanged(double value);
    void chromaBoxChanged(double value);
    void luminanceBoxChanged(double value);
    void changed(const MColourHCL16& color);
    void typeBoxChanged(int index);
    void interpolationPathsBoxChanged(bool checked);

signals:
    void colorChanged(const MColourHCL16& color);

private:
    MColourHCL16 currentColor;
    MColourNodes *colourNodes;
    int colorIndex;

    QDoubleSpinBox *hueBox;
    QDoubleSpinBox *chromaBox;
    QDoubleSpinBox *luminanceBox;
    MHCLColorRangeWidget *hueRange;
    MHCLColorRangeWidget *chromaRange;
    MHCLColorRangeWidget *luminanceRange;
    MHCLColorRange2DWidget *range2D;
};


/**
  @brief Widget for adjusting either hue, chroma or luminance of a given color.
 */
class MHCLColorRangeWidget : public QWidget
{
    Q_OBJECT

public:
    typedef MColourHCL16 Color;

public:
    MHCLColorRangeWidget(MHCLColorPicker *colorPicker,
                     HCLType1D type,
                     QWidget *parent = nullptr);

    /**
     * Generates new color by changing one component of the base
     * color.
     *
     * Which component to be changed is given by Met3D::TFEditor::Type1D.
     * @param t Normalized component value (Range: [0, 1])
     * @return generated color.
     * @see HCLColorRangeWidget::getValue()
     */
    MColourHCL16 getColor(float t) const;

    /**
     * @return normalized component value of given color
     * @see HCLColorRangeWidget::getColor()
     * @see Met3D::TFEditor::Type1D
     */
    float getValue(const MColourHCL16& color) const;
    /**
     * @return normalized component value of base color
     * @see HCLColorRangeWidget::getColor()
     * @see Met3D::TFEditor::Type1D
     */
    float getValue() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

signals:
    void changed(const MColourHCL16& color);

private:
    MHCLColorPicker *colorPicker;
    HCLType1D type;
};


/**
  *
  * @brief Widget for adjusting simultaneously either hue and chroma, hue and
  * luminance or chroma and luminance of a given color.
  *
  * It is inspired by the hcl-picker at (http://tristen.ca/hcl-picker) which is
  * based on the color conversion library 'chroma.js'.
  *
  * Issue:
  *     - Our 2D plots for HC, HL and CL which use 'colorspace.c' for color
  *         conversions differ from those in the original colorpicker
  *
  * Possible causes:
  *     - different whitepoints are used (unlikely since the D65 standard is
  *         apparently used both times)
  *     - wrong usage of colorspace.c (unlikely since our rgb plots match those
  *         from hcl-wizard which also uses colorspace.c, but still possible)
  *     - either chroma.js or colorspace.c have errors in their colorspace
  *         conversions
 */
class MHCLColorRange2DWidget : public QWidget
{
    Q_OBJECT

public:
    typedef MColourHCL16 Color;

public:
    MHCLColorRange2DWidget(MHCLColorPicker *colourPicker,
                       HCLType2D type,
                       bool showInterpolationPaths,
                       QWidget *parent = nullptr);
    void setType(HCLType2D type);
    void setShowInterpolationPaths(bool show);

    /**
     * Generates new color by changing two components of the base
     * color.
     *
     * Which components to be changed is given by Met3D::TFEditor::Type2D.
     * @param tx First normalized component value (Range: [0, 1])
     * @param ty Second normalized component value (Range: [0, 1])
     * @return generated color.
     * @see HCLColorRange2DWidget::getValueX()
     * @see HCLColorRange2DWidget::getValueY()
     * @see HCLColorRange2DWidget::getValueZ()
     */
    MColourHCL16 getColor(float tx, float ty) const;

    /**
     * @return first normalized component value of base color
     * @see HCLColorRange2DWidget::getColor()
     * @see Met3D::TFEditor::Type2D
     */
    float getValueX() const;
    /**
     * @return second normalized component value of base color
     * @see HCLColorRange2DWidget::getColor()
     * @see Met3D::TFEditor::Type2D
     */
    float getValueY() const;
    /**
     * @return third (unchanged) normalized component value of base color
     * @see HCLColorRange2DWidget::getColor()
     * @see Met3D::TFEditor::Type2D
     */
    float getValueZ() const;
    /**
     * @return first normalized component value of given color
     * @see HCLColorRange2DWidget::getColor()
     * @see Met3D::TFEditor::Type2D
     */
    float getValueX(const MColourHCL16& color) const;
    /**
     * @return second normalized component value of given color
     * @see HCLColorRange2DWidget::getColor()
     * @see Met3D::TFEditor::Type2D
     */
    float getValueY(const MColourHCL16& color) const;
    /**
     * @return third (unchanged) normalized component value of given color
     * @see HCLColorRange2DWidget::getColor()
     * @see Met3D::TFEditor::Type2D
     */
    float getValueZ(const MColourHCL16& color) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent * event);
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;

signals:
    void changed(const MColourHCL16& color);

private:
    void drawArrow(QPainter& painter, const QPoint& startPos,
                   const QPoint& endPos);

    MHCLColorPicker *colourPicker;
    HCLType2D type;

    bool showInterpolationPaths;
    QVector2D dir;
    QPoint mouseStart;

    bool moveColourNode;
};


} // namespace TFEditor
} // namespace Met3D

#endif // COLORPICKER_H
