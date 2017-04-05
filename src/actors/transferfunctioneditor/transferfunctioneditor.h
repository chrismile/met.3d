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
// SOURCE: https://github.com/gudajan/Windsim
#ifndef TRANSFERFUNCTIONEDITOR_H
#define TRANSFERFUNCTIONEDITOR_H

// standard library imports

// related third party imports
#include <QDialog>
#include <QSpinBox>
#include <QColor>
#include <QColorDialog>
#include <QPushButton>
#include <QComboBox>
#include <QLinearGradient>
#include <QMouseEvent>
#include <qcustomplot.h>

// local application imports
#include "colour.h"
#include "editortransferfunction.h"
#include "colourpicker.h"

namespace Met3D
{
namespace TFEditor
{

class MAbstractFunction;
class MColorFunction;
class MAlphaFunction;
class MFinalFunction;
class MRuler;
class MRangeRuler;
class MAlphaRuler;
class MBigAlphaRuler;
class MColorBox;
class MChannelsWidget;

/**
 * @brief Dialog to edit and create a TransferFunction in rgb and hcl
 * colorspace.
 */
class MTransferFunctionEditor : public QDialog
{
	Q_OBJECT
public:
    MTransferFunctionEditor(QWidget *parent = nullptr);
    ~MTransferFunctionEditor();

    void setRange(float min, float max, float scaleFactor,
                  int maxNumTicks, int maxNumLabels,
                  int numSteps, int decimals);

    void updateNumSteps(int numSteps);
    void resetUI();

    void setType(InterpolationType type);
    InterpolationType getType() const;

    MColorFunction* getColorFunction();
    MAlphaFunction* getAlphaFunction();
    MFinalFunction* getFinalFunction();

    MEditorTransferFunction* getTransferFunction();

signals:
    void transferFunctionChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    float normalizeValue(float value);
    float denormalizeValue(float value);

	// GUI
    MRangeRuler *rangeRuler;
    MAlphaRuler *alphaRuler;
    MBigAlphaRuler *bigAlphaRuler;

    MColorFunction *colorFunction;
    MAlphaFunction *alphaFunction;
    MFinalFunction *finalFunction;

    QPushButton *colourPrevButton;
    QPushButton *colourNextButton;
    QPushButton *colourDeleteButton;
    QPushButton *alphaPrevButton;
    QPushButton *alphaNextButton;
    QPushButton *alphaDelButton;

    QDoubleSpinBox *alphaPosBox;
    QDoubleSpinBox *alphaNormPosBox;
    QDoubleSpinBox *alphaValueBox;

    QDoubleSpinBox *colourPosBox;
    QDoubleSpinBox *colourNormPosBox;
    QComboBox *colorTypeComboBox;
    MColorBox *colourValueBox;

    MChannelsWidget *channelsWidget;
    QPushButton *openChannelsButton;

    MEditorTransferFunction transferFunction;

private slots:
    void changeTransferFunction(bool updateBoxes = true);

    void prevColourNode();
    void nextColourNode();
    void deleteColourNode();
    void changeColourPos(double pos);
    void changeColourNormPos(double pos);

    void prevAlphaNode();
    void nextAlphaNode();
    void deleteAlphaNode();
    void changeAlphaNormPos(double pos);
    void changeAlphaPos(double pos);
    void changeAlphaValue(double value);
    void changeAlphaRange(float min, float max);

    void changeColorType(int index);

    void openChannelDialog();
};


class MContentWidget : public QWidget
{
public:
    MContentWidget(QWidget *parent = nullptr);

    QRect contentRect() const;
};


class MAbstractFunction : public MContentWidget
{
    Q_OBJECT
public:
    MAbstractFunction(MEditorTransferFunction *transferFunction,
             MAbstractNodes *nodes,
             QWidget *parent = nullptr);

    void reset();

    float selectedX() const;
    float selectedY() const;

    void setSelectedX(float x);
    void setSelectedY(float y);

    void setSelectedPoint(int point);
    void deletePoint(int point);

    void selectPrev();
    void selectNext();

    MEditorTransferFunction *transferFunction;

    MAbstractNodes *abstractNodes;
    int selectedPoint;

signals:
    void functionChanged();

protected:
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;

    /**
     * Calculates pixel position of node.
     */
    QPoint toPixelPos(QPointF) const;
    /**
     * Calculates node position from pixel coordinates.
     */
    QPointF toLogicalPos(QPoint p) const;

    int getPointClicked(QPoint click);

    void drawPoints(QPainter& painter);

    virtual float xMin() const;
    virtual float xMax() const;
    virtual float yMin() const;
    virtual float yMax() const;

private:
    virtual void selectionChanged() {}
};


class MColorFunction : public MAbstractFunction
{
    Q_OBJECT

public:
    MColorFunction(MEditorTransferFunction *transferFunction,
                   QWidget *parent = nullptr);

    void openColorPicker();
    void closeColourPicker();

protected:
    virtual void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;

    float yMin() const override;
    float yMax() const override;

private:
    void selectionChanged() override;

    QColorDialog rgbColorPicker;
    MHCLColorPicker hclColorPicker;

private slots:
    void rgbColorChanged(const QColor& color);
    void hclColorChanged(const MColorHCL16& color);
};


class MAlphaFunction : public MAbstractFunction
{
    Q_OBJECT

public:
    MAlphaFunction(MEditorTransferFunction *transferFunction,
                  MRuler *xRuler,
                  MRuler *yRuler,
                  QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;

    float yMin() const override;
    float yMax() const override;

    /**
     Sets @ref posXNeighbourLeft and @ref posXNeighbourRight to the x positions
     of the neighbouring nodes of the currently selected node to avoid passing
     them while draging the current node.
     */
    void setNeighbouringNodes();

private:
    MRuler *xRuler;
    MRuler *yRuler;
    float posXNeighbourLeft;
    float posXNeighbourRight;
};


class MFinalFunction : public MContentWidget
{
public:
    MFinalFunction(MEditorTransferFunction *transferFunction,
                   QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    MEditorTransferFunction *transferFunction;
};


class MRuler
{
public:
    MRuler(float min, float max);

    const QVector<float>& getSmallTicks() const;
    const QVector<float>& getBigTicks() const;

    float getMinValue() const;
    float getMaxValue() const;

    void setRange(float min, float max);

    virtual void updateTicks() = 0;

protected:
    float minValue;
    float maxValue;
    QVector<float> smallTicks;
    QVector<float> bigTicks;
};


class MRangeRuler : public MContentWidget, public MRuler
{
	Q_OBJECT

public:
    MRangeRuler(QWidget *parent = nullptr);
    void updateTicks() override;

    void setRange(float min, float max, float scale,
                  int maxTicks, int maxLabels, int steps, int dec);

    float scaleFactor;
    int maxNumTicks;
    int maxNumLables;
    int numSteps;
    int decimals;

protected:
    void paintEvent(QPaintEvent *event) override;
};


class MAlphaRuler : public MContentWidget, public MRuler
{
    Q_OBJECT

public:
    MAlphaRuler(QWidget *parent = nullptr);
    void updateTicks() override;

protected:
    void paintEvent(QPaintEvent *event) override;

};


class MBigAlphaRuler : public MContentWidget, public MRuler
{
    Q_OBJECT

public:
    MBigAlphaRuler(QWidget *parent = nullptr);
    void updateTicks() override;

signals:
    void rangeChanged(float min, float max);

protected:
    void drawHandle(QPainter& painter, int y1, int y2, int width,
                    bool highlight);
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    bool overMin;
    bool overMax;
};


class MColorBox : public QWidget
{
    Q_OBJECT

public:
    MColorBox(MColorFunction *colorFunction, QWidget *parent = nullptr);
    QSize minimumSizeHint() const override;

signals:
    void functionChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    MColorFunction *colorFunction;
};


class MChannelsWidget : public QWidget
{
public:
    MChannelsWidget(MEditorTransferFunction *transferFunction,
                   QWidget *parent = nullptr);
    ~MChannelsWidget();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    MEditorTransferFunction *transferFunction;

    QCustomPlot *rgbPlot;
    QCustomPlot *hclPlot;
    QCPGraph *redGraph;
    QCPGraph *greenGraph;
    QCPGraph *blueGraph;
    QCPGraph *hueGraph;
    QCPGraph *chromaGraph;
    QCPGraph *luminanceGraph;
};


} // namespace TFEditor
} // namespace Met3D

#endif // TRANSFERFUNCTIONEDITOR_H
