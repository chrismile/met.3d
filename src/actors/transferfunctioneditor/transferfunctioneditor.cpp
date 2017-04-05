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
#include "transferfunctioneditor.h"

// standard library imports
#include <limits>
#include <iostream>

// related third party imports
#include <QBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QPainter>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QApplication>
#include <qcustomplot.h>

// local application imports

namespace Met3D
{
namespace TFEditor
{

/******************************************************************************
***                         MTransferFunctionEditor                         ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTransferFunctionEditor::MTransferFunctionEditor(QWidget *parent) :
    QDialog(parent),
    rangeRuler(nullptr),
    alphaRuler(nullptr),
    bigAlphaRuler(nullptr),
    colorFunction(nullptr),
    alphaFunction(nullptr),
    finalFunction(nullptr),
    alphaPosBox(nullptr),
    alphaNormPosBox(nullptr),
    alphaValueBox(nullptr),
    colourPosBox(nullptr),
    colourNormPosBox(nullptr),
    colorTypeComboBox(nullptr),
    colourValueBox(nullptr),
    channelsWidget(nullptr),
    openChannelsButton(nullptr),
    transferFunction()
{
    setWindowTitle("Transferfunction Editor");

    transferFunction.setType(HCL);
    this->setMinimumWidth(700);

    QHBoxLayout *layout = new QHBoxLayout(this);
    {
        QGridLayout *functionLayout = new QGridLayout();
        {
            rangeRuler = new MRangeRuler(this);
            alphaRuler = new MAlphaRuler(this);
            bigAlphaRuler = new MBigAlphaRuler(this);

            colorFunction = new MColorFunction(&transferFunction, this);
            alphaFunction = new MAlphaFunction(&transferFunction, rangeRuler,
                                              alphaRuler, this);
            finalFunction = new MFinalFunction(&transferFunction, this);

            openChannelsButton = new QPushButton("channels", this);

            colorFunction->setMinimumHeight(40);
            alphaFunction->setMinimumHeight(70);
            finalFunction->setMinimumHeight(30);

            rangeRuler->setMinimumHeight(40);
            alphaRuler->setMinimumWidth(40);
            bigAlphaRuler->setMinimumWidth(40);

            QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            policy.setHorizontalStretch(2);
            policy.setVerticalStretch(2);
            alphaFunction->setSizePolicy(policy);

            openChannelsButton->setSizePolicy(QSizePolicy::Expanding,
                                              QSizePolicy::Expanding);

            functionLayout->setContentsMargins(0, 0, 0, 0);
            functionLayout->setSpacing(0);
            functionLayout->addWidget(alphaFunction, 0, 0);
            functionLayout->addWidget(alphaRuler, 0, 1);
            functionLayout->addWidget(bigAlphaRuler, 0, 2);
            functionLayout->addWidget(rangeRuler, 1, 0);
            functionLayout->addWidget(colorFunction, 2, 0);
            functionLayout->addWidget(finalFunction, 3, 0);
            functionLayout->addWidget(openChannelsButton, 2, 1, 2, 2);
        }

        QVBoxLayout *boxLayout = new QVBoxLayout();
        {
            QStyle *style = QApplication::style();
            QIcon prevIcon = style->standardIcon(QStyle::SP_ArrowBack);
            QIcon nextIcon = style->standardIcon(QStyle::SP_ArrowForward);
            QIcon delIcon = style->standardIcon(QStyle::SP_DialogCloseButton);

            QGroupBox *colourBox = new QGroupBox(this);
            {
                QVBoxLayout *colourBoxLayout = new QVBoxLayout();
                QHBoxLayout *colourBoxTitleLayout = new QHBoxLayout();

                QLabel *colorBoxLabel = new QLabel("selected colour node", this);
                colourPrevButton = new QPushButton(this);
                colourNextButton = new QPushButton(this);
                colourDeleteButton = new QPushButton(this);

                colourPrevButton->setMaximumSize(20, 20);
                colourNextButton->setMaximumSize(20, 20);
                colourDeleteButton->setMaximumSize(20, 20);

                colourPrevButton->setIcon(prevIcon);
                colourNextButton->setIcon(nextIcon);
                colourDeleteButton->setIcon(delIcon);

                colourPrevButton->setToolTip("Switch to previous colour node");
                colourNextButton->setToolTip("Switch to next colour node");
                colourDeleteButton->setToolTip("Delete selected colour node");

                colourBoxTitleLayout->addWidget(colourPrevButton, 1);
                colourBoxTitleLayout->addWidget(colorBoxLabel, 1,
                                               Qt::AlignCenter);
                colourBoxTitleLayout->addWidget(colourNextButton, 1);
                colourBoxTitleLayout->addWidget(colourDeleteButton, 2,
                                               Qt::AlignRight);

                QFormLayout *colorBoxFormLayout = new QFormLayout();
                colourPosBox = new QDoubleSpinBox(this);
                colourNormPosBox = new QDoubleSpinBox(this);
                colourValueBox = new MColorBox(colorFunction, this);

                colorBoxFormLayout->addRow("position:", colourPosBox);
                colorBoxFormLayout->addRow("normalized position:",
                                           colourNormPosBox);
                colorBoxFormLayout->addRow("colour:", colourValueBox);

                colourBoxLayout->addLayout(colourBoxTitleLayout);
                colourBoxLayout->addLayout(colorBoxFormLayout);

                colourBox->setLayout(colourBoxLayout);
            }

            QGroupBox *alphaBox = new QGroupBox(this);
            {
                QVBoxLayout *alphaBoxLayout = new QVBoxLayout();
                QHBoxLayout *alphaBoxTitleLayout = new QHBoxLayout();

                QLabel *alphaBoxLabel = new QLabel("selected alpha node", this);
                alphaPrevButton = new QPushButton(this);
                alphaNextButton = new QPushButton(this);
                alphaDelButton = new QPushButton(this);

                alphaPrevButton->setMaximumSize(20, 20);
                alphaNextButton->setMaximumSize(20, 20);
                alphaDelButton->setMaximumSize(20, 20);

                alphaPrevButton->setIcon(prevIcon);
                alphaNextButton->setIcon(nextIcon);
                alphaDelButton->setIcon(delIcon);

                alphaPrevButton->setToolTip("Switch to previous alpha node");
                alphaNextButton->setToolTip("Switch to next alpha node");
                alphaDelButton->setToolTip("Delete selected alpha node");

                alphaBoxTitleLayout->addWidget(alphaPrevButton, 1);
                alphaBoxTitleLayout->addWidget(alphaBoxLabel, 1,
                                               Qt::AlignCenter);
                alphaBoxTitleLayout->addWidget(alphaNextButton, 1);
                alphaBoxTitleLayout->addWidget(alphaDelButton, 2,
                                               Qt::AlignRight);

                QFormLayout *alphaBoxFormLayout = new QFormLayout();
                alphaPosBox = new QDoubleSpinBox();
                alphaNormPosBox = new QDoubleSpinBox();
                alphaValueBox = new QDoubleSpinBox();

                alphaBoxFormLayout->addRow("position:", alphaPosBox);
                alphaBoxFormLayout->addRow("normalized position:",
                                           alphaNormPosBox);
                alphaBoxFormLayout->addRow("alpha:", alphaValueBox);

                alphaBoxLayout->addLayout(alphaBoxTitleLayout);
                alphaBoxLayout->addLayout(alphaBoxFormLayout);

                alphaBox->setLayout(alphaBoxLayout);
            }

            QGroupBox *colourSpaceBox = new QGroupBox("colour space", this);
            {
                colorTypeComboBox = new QComboBox(this);

                colourSpaceBox->setLayout(new QHBoxLayout());
                colourSpaceBox->layout()->addWidget(colorTypeComboBox);
            }

            boxLayout->addWidget(alphaBox);
            boxLayout->addStretch(1);
            boxLayout->addWidget(colourSpaceBox);
            boxLayout->addWidget(colourBox);

            alphaValueBox->setRange(0, 1);
            colourNormPosBox->setRange(0, 1);
            alphaNormPosBox->setRange(0, 1);

            colourNormPosBox->setSingleStep(0.1);
            alphaValueBox->setSingleStep(0.1);
            alphaNormPosBox->setSingleStep(0.1);

            colourPosBox->setFixedWidth(80);
            colourNormPosBox->setFixedWidth(80);

            alphaPosBox->setFixedWidth(80);
            alphaNormPosBox->setFixedWidth(80);
            alphaValueBox->setFixedWidth(80);

            colorTypeComboBox->addItems({"HCL", "RGB"});
        }

        QVBoxLayout *channelsLayout = new QVBoxLayout();
        {
            channelsWidget = new MChannelsWidget(&transferFunction, this);
            channelsWidget->hide();
            channelsLayout->addWidget(channelsWidget);
        }

        layout->addLayout(boxLayout);
        layout->addLayout(functionLayout, 2);
        layout->addLayout(channelsLayout);
    }

    connect(colorFunction, SIGNAL(functionChanged()),
            this, SLOT(changeTransferFunction()));
    connect(alphaFunction, SIGNAL(functionChanged()),
            this, SLOT(changeTransferFunction()));

    connect(colourPrevButton, SIGNAL(clicked()),
            this, SLOT(prevColourNode()));
    connect(colourNextButton, SIGNAL(clicked()),
            this, SLOT(nextColourNode()));
    connect(colourDeleteButton, SIGNAL(clicked()),
            this, SLOT(deleteColourNode()));
    connect(colourPosBox, SIGNAL(valueChanged(double)),
            this, SLOT(changeColourPos(double)));
    connect(colourNormPosBox, SIGNAL(valueChanged(double)),
            this, SLOT(changeColourNormPos(double)));
    connect(colourValueBox, SIGNAL(functionChanged()),
            this, SLOT(changeTransferFunction()));

    connect(alphaPrevButton, SIGNAL(clicked()),
            this, SLOT(prevAlphaNode()));
    connect(alphaNextButton, SIGNAL(clicked()),
            this, SLOT(nextAlphaNode()));
    connect(alphaDelButton, SIGNAL(clicked()),
            this, SLOT(deleteAlphaNode()));
    connect(alphaPosBox, SIGNAL(valueChanged(double)),
            this, SLOT(changeAlphaPos(double)));
    connect(alphaNormPosBox, SIGNAL(valueChanged(double)),
            this, SLOT(changeAlphaNormPos(double)));
    connect(alphaValueBox, SIGNAL(valueChanged(double)),
            this, SLOT(changeAlphaValue(double)));

    connect(colorTypeComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(changeColorType(int)));

    connect(bigAlphaRuler, SIGNAL(rangeChanged(float, float)),
            this, SLOT(changeAlphaRange(float, float)));

    connect(openChannelsButton, SIGNAL(clicked()),
            this, SLOT(openChannelDialog()));

    changeTransferFunction();
}


MTransferFunctionEditor::~MTransferFunctionEditor()
{
    delete channelsWidget;
    delete openChannelsButton;

    delete colourValueBox;
    delete colorTypeComboBox;
    delete alphaValueBox;
    delete alphaPosBox;
    delete alphaNormPosBox;
    delete colourPosBox;
    delete colourNormPosBox;

    delete alphaDelButton;
    delete alphaNextButton;
    delete alphaPrevButton;
    delete colourDeleteButton;
    delete colourNextButton;
    delete colourPrevButton;

    delete finalFunction;
    delete alphaFunction;
    delete colorFunction;

    delete bigAlphaRuler;
    delete alphaRuler;
    delete rangeRuler;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MTransferFunctionEditor::setRange(float min, float max, float scaleFactor,
                                      int maxNumTicks, int maxNumLabels,
                                      int numSteps, int decimals)
{
    rangeRuler->setRange(min, max, scaleFactor,
                         maxNumTicks, maxNumLabels,
                         numSteps, decimals);

    colourPosBox->setRange(min, max);
    colourPosBox->setSingleStep(pow(0.1, double(decimals)));
    colourPosBox->setDecimals(decimals);

    alphaPosBox->setRange(min, max);
    alphaPosBox->setSingleStep(pow(0.1, double(decimals)));
    alphaPosBox->setDecimals(decimals);

    changeTransferFunction();
}


void MTransferFunctionEditor::updateNumSteps(int numSteps)
{
    transferFunction.update(numSteps);
    repaint();
}


void MTransferFunctionEditor::resetUI()
{
    colorFunction->reset();
    alphaFunction->reset();
}


void MTransferFunctionEditor::setType(InterpolationType type)
{
    transferFunction.setType(type);

    colorTypeComboBox->blockSignals(true);
    colorTypeComboBox->setCurrentIndex((int)type);
    colorTypeComboBox->blockSignals(false);
}


InterpolationType MTransferFunctionEditor::getType() const
{
    return transferFunction.getType();
}


MColorFunction* MTransferFunctionEditor::getColorFunction()
{
    return colorFunction;
}


MAlphaFunction* MTransferFunctionEditor::getAlphaFunction()
{
    return alphaFunction;
}


MFinalFunction* MTransferFunctionEditor::getFinalFunction()
{
    return finalFunction;
}


MEditorTransferFunction* MTransferFunctionEditor::getTransferFunction()
{
    return &transferFunction;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTransferFunctionEditor::paintEvent(QPaintEvent *event)
{
    rangeRuler->updateTicks();
    bigAlphaRuler->updateTicks();
    alphaRuler->updateTicks();

    QWidget::paintEvent(event);
}


void MTransferFunctionEditor::closeEvent(QCloseEvent *event)
{
    // Close colour picker manually since it is not closed with the transfer
    // function editor.
    colorFunction->closeColourPicker();

    QDialog::closeEvent(event);
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

float MTransferFunctionEditor::normalizeValue(float value)
{
    return (value / rangeRuler->scaleFactor - rangeRuler->getMinValue())
            / (rangeRuler->getMaxValue() - rangeRuler->getMinValue());
}


float MTransferFunctionEditor::denormalizeValue(float value)
{
    return (value * (rangeRuler->getMaxValue() - rangeRuler->getMinValue())
            + rangeRuler->getMinValue()) * rangeRuler->scaleFactor;
}


/******************************************************************************
***                            PRIVATE SLOTS                                ***
*******************************************************************************/

void MTransferFunctionEditor::changeTransferFunction(bool updateBoxes)
{
    if (updateBoxes)
    {
        colourPosBox->blockSignals(true);
        colourNormPosBox->blockSignals(true);
        alphaPosBox->blockSignals(true);
        alphaNormPosBox->blockSignals(true);
        alphaValueBox->blockSignals(true);

        colourNormPosBox->setValue(colorFunction->selectedX());
        colourPosBox->setValue(denormalizeValue(colorFunction->selectedX()));
        colourPosBox->setToolTip(QString("%1").arg(
                                     denormalizeValue(
                                         colorFunction->selectedX())));
        alphaPosBox->setValue(denormalizeValue(alphaFunction->selectedX()));
        alphaPosBox->setToolTip(QString("%1").arg(
                                    denormalizeValue(
                                        alphaFunction->selectedX())));
        alphaNormPosBox->setValue(alphaFunction->selectedX());
        alphaValueBox->setValue(alphaFunction->selectedY());

        colourPosBox->blockSignals(false);
        colourNormPosBox->blockSignals(false);
        alphaPosBox->blockSignals(false);
        alphaNormPosBox->blockSignals(false);
        alphaValueBox->blockSignals(false);
    }

    bool isFirstColor = colorFunction->selectedPoint == 0;
    bool isLastColor = colorFunction->selectedPoint == 1;

    colourPrevButton->setEnabled(!isFirstColor);
    colourNextButton->setEnabled(!isLastColor);
    colourDeleteButton->setEnabled(!isFirstColor && !isLastColor);

    colourPosBox->setEnabled(!isFirstColor && !isLastColor);
    colourNormPosBox->setEnabled(!isFirstColor && !isLastColor);

    bool isFirstAlpha = alphaFunction->selectedPoint == 0;
    bool isLastAlpha = alphaFunction->selectedPoint == 1;

    alphaPrevButton->setEnabled(!isFirstAlpha);
    alphaNextButton->setEnabled(!isLastAlpha);
    alphaDelButton->setEnabled(!isFirstAlpha && !isLastAlpha);

    alphaPosBox->setEnabled(!isFirstAlpha && !isLastAlpha);
    alphaNormPosBox->setEnabled(!isFirstAlpha && !isLastAlpha);

    channelsWidget->repaint();

    emit transferFunctionChanged();
}


void MTransferFunctionEditor::prevColourNode()
{
    colorFunction->selectPrev();
}


void MTransferFunctionEditor::nextColourNode()
{
    colorFunction->selectNext();
}


void MTransferFunctionEditor::deleteColourNode()
{
    int point = colorFunction->selectedPoint;
    colorFunction->deletePoint(point);
}


void MTransferFunctionEditor::changeColourPos(double pos)
{
    colourPosBox->setToolTip(QString("%1").arg(pos));

    double normalizedPos = double(normalizeValue(float(pos)));

    // Update normalized position.
    colourNormPosBox->blockSignals(true);
    colourNormPosBox->setValue(normalizedPos);
    colourNormPosBox->blockSignals(false);

    colorFunction->setSelectedX(normalizedPos);
    changeTransferFunction(false);
}


void MTransferFunctionEditor::changeColourNormPos(double pos)
{
    double denormalizedPos = double(denormalizeValue(float(pos)));

    // Update normalized position.
    colourPosBox->blockSignals(true);
    colourPosBox->setValue(denormalizedPos);
    colourPosBox->setToolTip(QString("%1").arg(denormalizedPos));
    colourPosBox->blockSignals(false);

    colorFunction->setSelectedX(pos);
    changeTransferFunction(false);
}


void MTransferFunctionEditor::prevAlphaNode()
{
    alphaFunction->selectPrev();
}


void MTransferFunctionEditor::nextAlphaNode()
{
    alphaFunction->selectNext();
}


void MTransferFunctionEditor::deleteAlphaNode()
{
    int point = alphaFunction->selectedPoint;
    alphaFunction->deletePoint(point);
}


void MTransferFunctionEditor::changeAlphaNormPos(double pos)
{
    double denormalizedPos = double(denormalizeValue(float(pos)));

    // Update normalized position.
    alphaPosBox->blockSignals(true);
    alphaPosBox->setValue(denormalizedPos);
    alphaPosBox->setToolTip(QString("%1").arg(denormalizedPos));
    alphaPosBox->blockSignals(false);

    alphaFunction->setSelectedX(pos);
    changeTransferFunction(false);
}


void MTransferFunctionEditor::changeAlphaPos(double pos)
{
    alphaPosBox->setToolTip(QString("%1").arg(pos));

    double normalizedPos = double(normalizeValue(float(pos)));

    // Update normalized position.
    alphaNormPosBox->blockSignals(true);
    alphaNormPosBox->setValue(normalizedPos);
    alphaNormPosBox->blockSignals(false);

    alphaFunction->setSelectedX(normalizedPos);
    changeTransferFunction(false);
}


void MTransferFunctionEditor::changeAlphaValue(double value)
{
    alphaFunction->setSelectedY(value);
    changeTransferFunction(false);
}


void MTransferFunctionEditor::changeAlphaRange(float min, float max)
{
    alphaRuler->setRange(min, max);
    changeTransferFunction();
}


void MTransferFunctionEditor::changeColorType(int index)
{
    colorFunction->closeColourPicker();
    transferFunction.setType((InterpolationType)index);
    changeTransferFunction();
}


void MTransferFunctionEditor::openChannelDialog()
{
    int channelsWidgetWidth = channelsWidget->width();
    // Shorten window to size without channels.
    if (!channelsWidget->isHidden())
    {
        this->setMinimumWidth(700);
        this->resize(width() - channelsWidgetWidth, height());
    }
    // Expand window to fit channels.
    else
    {
        this->resize(width() + channelsWidgetWidth, height());
        this->setMinimumWidth(700 + channelsWidgetWidth);
    }

    channelsWidget->setShown(channelsWidget->isHidden());
}


/******************************************************************************
***                             MContentWidget                              ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MContentWidget::MContentWidget(QWidget *parent) :
    QWidget(parent)
{}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QRect MContentWidget::contentRect() const
{
    int margin = 5;
    return QRect(margin,
                 margin,
                 width() - margin * 2 - 1,
                 height() - margin * 2 - 1);
}


/******************************************************************************
***                            MAbstractFunction                            ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MAbstractFunction::MAbstractFunction(MEditorTransferFunction *transferFunction,
                   MAbstractNodes *nodes,
                   QWidget *parent)
    : MContentWidget(parent)
    , transferFunction(transferFunction)
    , abstractNodes(nodes)
{
    reset();
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MAbstractFunction::reset()
{
    selectedPoint = 0;
}


float MAbstractFunction::selectedX() const
{
    return abstractNodes->xAt(selectedPoint);
}


float MAbstractFunction::selectedY() const
{
    return abstractNodes->yAt(selectedPoint);
}


void MAbstractFunction::setSelectedX(float x)
{
    abstractNodes->setXAt(selectedPoint, x);
}


void MAbstractFunction::setSelectedY(float y)
{
    abstractNodes->setYAt(selectedPoint, y);
}


void MAbstractFunction::setSelectedPoint(int point)
{
    // Select point only if it is a point and is not already selected.
    if (selectedPoint != point &&
            point >= 0 &&
            point < abstractNodes->getNumNodes())
    {
        selectedPoint = point;

        selectionChanged();
        emit functionChanged();
    }
}


void MAbstractFunction::deletePoint(int point)
{
    // Only delete point if it is not the first or last one (zeroth and first
    // position in vector storing nodes).
    if (point > 1)
    {
        int m = QMessageBox::warning(
                    this, "Remove point",
                    "Are you sure you want to remove this point?",
                    QMessageBox::Yes, QMessageBox::Cancel);

        if (m == QMessageBox::Yes)
        {
            if (selectedPoint == point)
            {
                setSelectedPoint(0);
            }

            abstractNodes->removeNode(point);

            if (selectedPoint > point)
            {
                setSelectedPoint(selectedPoint - 1);
            }

            emit functionChanged();
        }
    }
}


void MAbstractFunction::selectPrev()
{
    float x = selectedX();
    float d = std::numeric_limits<float>::max();
    int point = -1;

    // Searching for the previous node. Since vector of nodes is not sorted, we
    // need to find the node in the list of all nodes by searching for the
    // node nearest to the current node with a smaller position value then
    // the current node.
    for (int i = 0; i < abstractNodes->getNumNodes(); i++)
    {
        float tempX = abstractNodes->xAt(i);
        float tempD = x - tempX;
        if (i != selectedPoint && tempD >= 0 && tempD < d)
        {
            point = i;
            d = tempD;
        }
    }

    setSelectedPoint(point);
}


void MAbstractFunction::selectNext()
{
    float x = selectedX();
    float d = std::numeric_limits<float>::max();
    int point = -1;

    // Searching for the next node. Since vector of nodes is not sorted, we
    // need to find the node in the list of all nodes by searching for the
    // node nearest to the current node with a greater position value then
    // the current node.
    for (int i = 0; i < abstractNodes->getNumNodes(); i++)
    {
        float tempX = abstractNodes->xAt(i);
        float tempD = tempX - x;
        if (i != selectedPoint && tempD >= 0 && tempD < d)
        {
            point = i;
            d = tempD;
        }
    }

    setSelectedPoint(point);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MAbstractFunction::mousePressEvent(QMouseEvent *event)
{
    // Adding or selecting node.
    if (event->button() == Qt::LeftButton)
    {
        int point = getPointClicked(event->pos());

        // If user clicked on a point, select it.
        if (point != -1)
        {
            setSelectedPoint(point);
        }
        // If user did not click on a point, create new one.
        else if (point == -1)
        {
            // aCodeCopy 242 transferFunctionEditor.cpp
            qreal t = toLogicalPos(event->pos()).x();

            point = abstractNodes->addNode(t);
            setSelectedPoint(point);
        }
    }
    // Deleting nodes.
    else if (event->button() == Qt::RightButton)
    {
        int point = getPointClicked(event->pos());
        deletePoint(point);
    }
}


void MAbstractFunction::mouseMoveEvent(QMouseEvent * event)
{
    if (event->buttons() == Qt::LeftButton)
    {
        QPointF pos = toLogicalPos(event->pos());

        // Allow horizontal movement only for control points different from the
        // start and end point (first two points in the vector storing nodes).
        if (selectedPoint > 1)
        {
            setSelectedX(std::max(xMin(), std::min((float)pos.x(), xMax())));
        }

        setSelectedY(std::max(0.f, std::min((float)pos.y(), 1.f)));
        emit functionChanged();
    }
}


QPoint MAbstractFunction::toPixelPos(QPointF p) const
{
    float x = xMin();
    float y = yMin();

    float xRange = xMax() - xMin();
    float yRange = yMax() - yMin();

    if (xRange != 0.f)
    {
        x = (p.x() - xMin()) / xRange;
    }
    if (yRange != 0.f)
    {
        y = (p.y() - yMin()) / yRange;
    }

    x = std::min(std::max(0.f, x), 1.f);
    y = std::min(std::max(0.f, y), 1.f);

    x = x * contentRect().width() + contentRect().x();
    y = (1.f - y) * contentRect().height() + contentRect().y();

    return QPoint(x, y);
}


QPointF MAbstractFunction::toLogicalPos(QPoint p) const
{
    float x = xMin();
    float y = yMin();

    float xRange = xMax() - xMin();
    float yRange = yMax() - yMin();

    if (xRange != 0.f)
    {
        x = (p.x() - contentRect().x()) / float(contentRect().width());
        x = x * (xRange) + xMin();
    }

    if (yRange != 0.f)
    {
        y = 1.f - (p.y() - contentRect().y()) / float(contentRect().height());
        y = y * (yRange) + yMin();
    }

    x = std::min(std::max(xMin(), x), xMax());
    y = std::min(std::max(yMin(), y), yMax());

    return QPointF(x, y);
}


int MAbstractFunction::getPointClicked(QPoint click)
{
    int distMin = std::numeric_limits<int>::max();
    int clicked = -1;

    QPoint pointSize(10, 10);
    // Loop over all given points and search for the point nearest to the click
    // position with the click position inside its representing rectangle.
    for (int point = 0; point != abstractNodes->getNumNodes(); point++)
    {
        QPoint pos = toPixelPos(QPointF(abstractNodes->xAt(point),
                                        abstractNodes->yAt(point)));
        // Bounds of the rectangle representing the current point.
        QRect bounds(pos - pointSize * 0.5, pos + pointSize * 0.5);

        int dist = (click - pos).manhattanLength();

        if (bounds.contains(click) && dist < distMin)
        {
            distMin = dist;
            clicked = point;
        }
    }

    return clicked;
}


void MAbstractFunction::drawPoints(QPainter& painter)
{
    // aCodeCopy 184 transferFunctionEditor.cpp
    QBrush pointBrush(QColor(255, 255, 255, 255));
    QPen pointPen(QColor(0, 0, 0, 255), 1);
    painter.setPen(pointPen);

    QPoint pointSize(8, 8);
    for (int i = 0; i < abstractNodes->getNumNodes(); i++)
    {
        QPoint pos = toPixelPos(QPointF(abstractNodes->xAt(i),
                                        abstractNodes->yAt(i)));

        QRect bounds(pos - pointSize * 0.5, pos + pointSize * 0.5);

        if (i == selectedPoint)
        {
            painter.setBrush(QBrush(QColor(150, 150, 255, 255)));
        }
        else
        {
            painter.setBrush(pointBrush);
        }
        painter.drawRect(bounds);
    }
}


float MAbstractFunction::xMin() const
{
    return 0;
}


float MAbstractFunction::xMax() const
{
    return 1;
}


float MAbstractFunction::yMin() const
{
    return 0;
}


float MAbstractFunction::yMax() const
{
    return 1;
}


/******************************************************************************
***                             MColorFunction                              ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MColorFunction::MColorFunction(
        MEditorTransferFunction *transferFunction,
        QWidget *parent) :
    MAbstractFunction(transferFunction, transferFunction->getColourNodes(), parent),
    hclColorPicker(transferFunction->getColourNodes(), parent)
{
    rgbColorPicker.setOption(QColorDialog::NoButtons);

    connect(&rgbColorPicker, SIGNAL(currentColorChanged(QColor)),
            this, SLOT(rgbColorChanged(QColor)));

    connect(&hclColorPicker, SIGNAL(colorChanged(MColorHCL16)),
            this, SLOT(hclColorChanged(MColorHCL16)));
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MColorFunction::openColorPicker()
{
    if (transferFunction->getType() == HCL)
    {
        hclColorPicker.setCurrentIndex(selectedPoint);
        hclColorPicker.show();
    }
    else
    {
        MColorRGB8 rgb = (MColorRGB8)
                transferFunction->getColourNodes()->colourAt(selectedPoint);
        rgbColorPicker.setCurrentColor(QColor(rgb.r, rgb.g, rgb.b));
        rgbColorPicker.show();
    }
}


void MColorFunction::closeColourPicker()
{
    rgbColorPicker.close();
    hclColorPicker.close();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MColorFunction::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QVector<QRgb>* sampledBuffer = transferFunction->getSampledBuffer();
    QImage image = QImage(
                (unsigned char*)sampledBuffer->data(), sampledBuffer->size(),
                1, QImage::Format_ARGB32);

    image = image.convertToFormat(QImage::Format_RGB888);

    painter.setBrush(QBrush());
    painter.drawImage(contentRect(), image, image.rect());

    drawPoints(painter);

    // aCodeCopy 209 transferFunctionEditor.cpp
	if (!isEnabled())
    {
        painter.fillRect(contentRect(), QColor(255, 255, 255, 128));
    }
}


void MColorFunction::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        openColorPicker();
    }
}


void MColorFunction::mousePressEvent(QMouseEvent *event)
{
    // Adding or selecting node.
    if (event->button() == Qt::LeftButton)
    {
        int point = getPointClicked(event->pos());

        // If user clicked on a point, select it.
        if (point != -1)
        {
            setSelectedPoint(point);
        }
        // If user did not click on a point, create new one.
        else if (point == -1)
        {
            // aCodeCopy 242 transferFunctionEditor.cpp
            qreal t = toLogicalPos(event->pos()).x();

            point = abstractNodes->addNode(t);
            setSelectedPoint(point);
        }
    }
    // Deleting nodes.
    else if (event->button() == Qt::RightButton)
    {
        int point = getPointClicked(event->pos());
        deletePoint(point);
    }
}


void MColorFunction::mouseMoveEvent(QMouseEvent * event)
{
    if (event->buttons() == Qt::LeftButton)
    {
        QPointF pos = toLogicalPos(event->pos());

        // Allow horizontal movement only for control points different from the
        // start and end point (first two points in the vector storing nodes).
        if (selectedPoint > 1)
        {
            setSelectedX(std::max(xMin(), std::min((float)pos.x(), xMax())));
        }

        setSelectedY(std::max(0.f, std::min((float)pos.y(), 1.f)));
        emit functionChanged();
    }
}


float MColorFunction::yMin() const
{
    return 0.5f;
}


float MColorFunction::yMax() const
{
    return 0.5f;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MColorFunction::selectionChanged()
{
    if (rgbColorPicker.isVisible())
    {
        MColorRGB8 rgb = (MColorRGB8)
                transferFunction->getColourNodes()->colourAt(selectedPoint);
        rgbColorPicker.setCurrentColor(QColor(rgb.r, rgb.g, rgb.b));
    }

    if (hclColorPicker.isVisible())
    {
        hclColorPicker.setCurrentIndex(selectedPoint);
    }
}


/******************************************************************************
***                            PRIVATE SLOTS                                ***
*******************************************************************************/

void MColorFunction::rgbColorChanged(const QColor& color)
{
    if (!rgbColorPicker.isVisible())
    {
        return;
    }

    MColorRGB8 rgb(color.redF(), color.greenF(), color.blueF());

    transferFunction->getColourNodes()->colourAt(selectedPoint) = (MColorXYZ64)rgb;
    emit functionChanged();
}


void MColorFunction::hclColorChanged(const MColorHCL16&)
{
    if (!hclColorPicker.isVisible())
    {
        return;
    }

    MColorHCL16 hcl = hclColorPicker.color();
    hcl.c = std::max(hcl.c, (unsigned short)1);
    hcl.l = std::max(hcl.l, (unsigned short)1);

    transferFunction->getColourNodes()->colourAt(selectedPoint) = (MColorXYZ64)hcl;
    emit functionChanged();
}


/******************************************************************************
***                             MAlphaFunction                              ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MAlphaFunction::MAlphaFunction(
        MEditorTransferFunction *transferFunction,
        MRuler *xRuler,
        MRuler *yRuler,
        QWidget *parent) :
    MAbstractFunction(transferFunction, transferFunction->getAlphaNodes(), parent),
    xRuler(xRuler),
    yRuler(yRuler)
{}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MAlphaFunction::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background.
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    painter.drawRect(contentRect());

    painter.setBrush(Qt::white);

    const QVector<float>& xTicks = xRuler->getSmallTicks();
    int nXTicks = xTicks.size();

    float minX = xRuler->getMinValue();
    float maxX = xRuler->getMaxValue();
    for (int i = 0; i < (nXTicks - 1); i += 2)
    {
        float x1 = (xTicks[i] - minX) / (maxX - minX);
        float x2 = (xTicks[i + 1] - minX) / (maxX - minX);
        x1 = toPixelPos(QPointF(x1, 0)).x();
        x2 = toPixelPos(QPointF(x2, 0)).x();
        painter.drawRect(QRect(x1, contentRect().y(),
                         x2 - x1, contentRect().height()));
    }

    painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination);

    const QVector<float>& yTicks = yRuler->getSmallTicks();
    int nYTicks = yTicks.size();

    for (int i = 0; i < (nYTicks - 1); i += 2)
    {
        float y1 = toPixelPos(QPointF(0, yTicks[i])).y();
        float y2 = toPixelPos(QPointF(0, yTicks[i + 1])).y();
        painter.drawRect(QRect(contentRect().x(), y2,
                         contentRect().width(), y1 - y2));
    }

    if ((nYTicks % 2) != 0)
    {
        float y1 = toPixelPos(QPointF(0, yTicks.back())).y();
        float y2 = toPixelPos(QPointF(0, 1)).y();
        painter.drawRect(QRect(contentRect().x(), y2,
                         contentRect().width(), y1 - y2));
    }

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setBrush(QColor(180, 180, 180, 220));
    painter.drawRect(contentRect());

    // Draw lines.
    // aCodeCopy 363 transferFunctionEditor.cpp
	std::vector<QPoint> points;
    points.reserve(abstractNodes->getNumNodes());
    for (int i = 0; i < abstractNodes->getNumNodes(); i++)
    {
        points.push_back(toPixelPos(QPointF(abstractNodes->xAt(i),
                                            abstractNodes->yAt(i))));
    }

    std::sort(points.begin(), points.end(),
              [](const QPoint& a, const QPoint& b) { return a.x() < b.x(); });
    painter.setPen(QPen(QColor(0, 0, 0, 255), 2));
    painter.drawPolyline(points.data(), points.size());

    // Draw points.
    drawPoints(painter);

	if (!isEnabled())
    {
        painter.fillRect(contentRect(), QColor(255, 255, 255, 128));
    }

}


void MAlphaFunction::mousePressEvent(QMouseEvent *event)
{
    // Adding or selecting node.
    if (event->button() == Qt::LeftButton)
    {
        int point = getPointClicked(event->pos());

        // If user clicked on a point, select it.
        if (point != -1)
        {
            setSelectedPoint(point);
        }
        // If user did not click on a point, create new one.
        else if (point == -1)
        {
            // aCodeCopy 242 transferFunctionEditor.cpp
            qreal t = toLogicalPos(event->pos()).x();

            point = abstractNodes->addNode(t);
            setSelectedPoint(point);
        }

        setNeighbouringNodes();
    }
    // Deleting nodes.
    else if (event->button() == Qt::RightButton)
    {
        int point = getPointClicked(event->pos());
        deletePoint(point);
    }
}


void MAlphaFunction::mouseMoveEvent(QMouseEvent * event)
{
    if (event->buttons() == Qt::LeftButton)
    {
        QPointF pos = toLogicalPos(event->pos());

        // Allow horizontal movement only for control points different from the
        // start and end point (first two points in the vector storing nodes).
        if (selectedPoint > 1)
        {
            // Avoid crossing neighbouring nodes.
            float xMinimum = posXNeighbourLeft;
            float xMaximum = posXNeighbourRight;
            setSelectedX(std::max(xMinimum, std::min((float)pos.x(), xMaximum)));
        }

        setSelectedY(std::max(0.f, std::min((float)pos.y(), 1.f)));
        emit functionChanged();
    }
}


float MAlphaFunction::yMin() const
{
    return yRuler->getMinValue();
}


float MAlphaFunction::yMax() const
{
    return yRuler->getMaxValue();
}


void MAlphaFunction::setNeighbouringNodes()
{
    float posXCurrentNode = abstractNodes->xAt(selectedPoint);
    // Initialise with border nodes.
    posXNeighbourLeft = abstractNodes->xAt(0);
    posXNeighbourRight = abstractNodes->xAt(1);

    float distLeft = posXCurrentNode - posXNeighbourLeft;
    float distRight = posXNeighbourRight - posXCurrentNode;

    // Loop over all given points and search for the point nearest to the click
    // position with the click position inside its representing rectangle.
    for (int i = 2; i < abstractNodes->getNumNodes(); i++)
    {
        // Skip current node.
        if (i == selectedPoint)
        {
            continue;
        }

        float posXNode = abstractNodes->xAt(i);

        // Node is to the left of current node.
        if (posXNode < posXCurrentNode)
        {
            if ( (posXCurrentNode - posXNode) <= distLeft )
            {
                distLeft = posXCurrentNode - posXNode;
                posXNeighbourLeft = abstractNodes->xAt(i);
            }
        }
        // Node is to the right of current node.
        else if (posXNode > posXCurrentNode)
        {
            if ( (posXNode - posXCurrentNode) < distRight )
            {
                distRight = posXNode - posXCurrentNode;
                posXNeighbourRight = abstractNodes->xAt(i);
            }
        }
        // Node is at the same level in x direction as the current node.
        else
        {
            if (i < selectedPoint)
            {
                distLeft = posXCurrentNode - posXNode;
                posXNeighbourLeft = abstractNodes->xAt(i);
            }
            else
            {
                distRight = posXNode - posXCurrentNode;
                posXNeighbourRight = abstractNodes->xAt(i);
            }
        }
    }
    distLeft = distRight + 1;
}


/******************************************************************************
***                             MFinalFunction                              ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MFinalFunction::MFinalFunction(MEditorTransferFunction *transferFunction,
                               QWidget *parent) :
    MContentWidget(parent),
    transferFunction(transferFunction)
{}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MFinalFunction::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    // Draw underlying checkerboard pattern.
    // aCodeCopy 337 transferFunctionEditor.cpp
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect rect = QRect(contentRect().x(), 0, contentRect().width(), height());

    int checkerSize = 5;
    QPixmap pixelMap(checkerSize * 2, checkerSize * 2);
    QPainter pixelMapPainter(&pixelMap);
    pixelMapPainter.fillRect(0, 0, checkerSize, checkerSize,
                 Qt::lightGray);
    pixelMapPainter.fillRect(checkerSize, checkerSize, checkerSize, checkerSize,
                 Qt::lightGray);
    pixelMapPainter.fillRect(0, checkerSize, checkerSize, checkerSize,
                 Qt::darkGray);
    pixelMapPainter.fillRect(checkerSize, 0, checkerSize, checkerSize,
                 Qt::darkGray);
    pixelMapPainter.end();

    painter.setBrush(QBrush(pixelMap));
    painter.setPen(Qt::NoPen);
    painter.drawRect(rect);

    // Draw transfer function.
    const QVector<QRgb>* sampledBuffer =
            transferFunction->getSampledBuffer();
    QImage image = QImage(
                (unsigned char*)sampledBuffer->data(), sampledBuffer->size(),
                1, QImage::Format_ARGB32);

    painter.setBrush(QBrush());
    painter.drawImage(rect, image, image.rect());

    if (!isEnabled())
    {
        painter.fillRect(rect, QColor(255, 255, 255, 128));
    }
}


/******************************************************************************
***                                 MRuler                                  ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MRuler::MRuler(float min, float max) :
    minValue(min),
    maxValue(max)
{}

/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

const QVector<float>& MRuler::getSmallTicks() const
{
    return smallTicks;
}


const QVector<float>& MRuler::getBigTicks() const
{
    return bigTicks;
}


float MRuler::getMinValue() const
{
    return minValue;
}


float MRuler::getMaxValue() const
{
    return maxValue;
}


void MRuler::setRange(float min, float max)
{
    minValue = min;
    maxValue = max;
}


/******************************************************************************
***                               MRangeRuler                               ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MRangeRuler::MRangeRuler(QWidget *parent)
    : MContentWidget(parent)
    , MRuler(0 , 1000.0)
    , scaleFactor(1)
    , maxNumTicks(0)
    , maxNumLables(0)
    , numSteps(1)
    , decimals(2)
{}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MRangeRuler::updateTicks()
{
    // Mimic tick behaviour from transferfunction colour bar.
    smallTicks.clear();
    bigTicks.clear();

    int numTicks = std::min(numSteps + 1, maxNumTicks);
    int tickStep = ceil(double(numTicks - 1) / double(maxNumLables - 1));

    for (int i = 0; i < numTicks; i++)
    {
        float value = ((i / double(numTicks - 1)) * (maxValue - minValue))
                + minValue;

        smallTicks.push_back(value);

        // Start with big ticks at greatest value like in the tf colour bar.
        if ((numTicks - i - 1) % tickStep == 0)
        {
            bigTicks.push_back(value);
        }
    }
}


void MRangeRuler::setRange(float min, float max, float scale,
              int maxTicks, int maxLabels, int steps, int dec)
{
    minValue = min;
    maxValue = max;
    scaleFactor = scale;
    maxNumTicks = maxTicks;
    maxNumLables = maxLabels;
    numSteps = steps;
    decimals = dec;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MRangeRuler::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    QRect rect = QRect(contentRect().x(), 0, contentRect().width(), height());

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawRect(rect);

    QFontMetrics fontMetrics(font());
    int fontHeight = fontMetrics.height();

    painter.setPen(QPen());

    foreach (float v, smallTicks)
    {
        float x = (v - minValue) / (maxValue - minValue);
        x = x * contentRect().width() + contentRect().x();

        int y0 = 0;
        int y1 = (height() - fontHeight) / 2.f + y0;
        int y2 = fontHeight + y1;
        int y3 = height();

        painter.drawLine(QLine(QPoint(x, y0),
                         QPoint(x, y1)));
        painter.drawLine(QLine(QPoint(x, y2),
                         QPoint(x, y3)));

    }

    foreach (float v, bigTicks)
    {
        QString text = QString::number(v * scaleFactor, 'f', decimals);

        int fontWidth = fontMetrics.width(text);

        float x = (v - minValue) / (maxValue - minValue);
        x = x * contentRect().width() + contentRect().x();
        int x1 = x + 3;
        int x2 = x1 + fontWidth;

        if (x1 < 0)
        {
            x1 = 3;
        }

        if (x2 >= contentRect().width() - 3)
        {
            x1 -= x2 - contentRect().width() + 3;
        }

        float y = (height() - fontHeight) * 0.5f;
        QRect rect = QRect(x1, y, fontWidth, fontHeight);

        painter.drawText(rect, text);

        painter.drawLine(QLine(QPoint(x, 0), QPoint(x, height())));
    }
}


/******************************************************************************
***                               MAlphaRuler                               ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MAlphaRuler::MAlphaRuler(QWidget *parent) :
    MContentWidget(parent),
    MRuler(0, 1)
{}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MAlphaRuler::updateTicks()
{
    smallTicks.clear();
    bigTicks.clear();

    QFontMetrics fontMetric(font());
    int fontHeight = fontMetric.height();

    float stepSize = 1;
    QVector<float> stepSizes = {0.001, 0.0025, 0.005, 0.01, 0.025, 0.05, 0.1,
                                0.25, 0.5};

    foreach (float step, stepSizes)
    {
        int h = std::floor(contentRect().height() *
                           (step / (maxValue - minValue)));
        if (h >= fontHeight * 2.2f)
        {
            stepSize = step;
            break;
        }
    }

    int i = std::floor(minValue / stepSize);
    if (i % 2 == 1)
    {
        i--;
    }

    float offset = i * stepSize;

    for (float v = offset;
         std::floor(v / stepSize) <= std::floor(maxValue / stepSize);
         v += stepSize)
    {
        smallTicks.push_back(v);
        bigTicks.push_back(v);
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MAlphaRuler::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    QRect rect = QRect(0, contentRect().y(), width(), contentRect().height());

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawRect(rect);

    QFontMetrics fontMetrics(font());
    int fontHeight = fontMetrics.height();

    painter.setPen(QPen());

    for (float v : bigTicks)
    {
        float y1 = (1 - (v - minValue) / (maxValue - minValue));
        y1 = y1 * contentRect().height() + contentRect().y();

        int x1 = 0;
        int x2 = width();

        painter.drawLine(QLine(x1, y1, x2, y1));

        QString text = QString::number(v, 'f', 2);

        int y2 = y1 - fontHeight;

        if (y2 < 0)
        {
            y2 = 0;
        }

        int fontWidth = fontMetrics.width(text);
        QRect rect = QRect(0, y2, fontWidth, fontHeight);

        painter.drawText(rect, text);
    }
}


/******************************************************************************
***                             MBigAlphaRuler                              ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MBigAlphaRuler::MBigAlphaRuler(QWidget *parent) :
    MContentWidget(parent),
    MRuler(0, 1),
    overMin(false),
    overMax(false)
{
    setMouseTracking(true);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MBigAlphaRuler::updateTicks()
{
    smallTicks.clear();
    bigTicks.clear();

    QFontMetrics fontMetrics(font());
    int fontHeight = fontMetrics.height();

    float stepSize = 1;
    QVector<float> stepSizes = {0.001, 0.0025, 0.005, 0.01, 0.025, 0.05, 0.1,
                                0.25, 0.5};

    for (float step : stepSizes)
    {
        int h = std::floor(contentRect().height() * step);
        if (h >= fontHeight * 2.2f)
        {
            stepSize = step;
            break;
        }
    }

    for (float v = 0;
         std::floor(v / stepSize) <= std::floor(1 / stepSize);
         v += stepSize)
    {
        smallTicks.push_back(v);
        bigTicks.push_back(v);
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MBigAlphaRuler::drawHandle(QPainter& painter, int y1, int y2, int width,
                                bool highlight)
{
    QPalette palette;
    QColor colour;
    if (highlight)
    {
        colour = palette.color(QPalette::Active, QPalette::Highlight);
    }
    else
    {
        colour = palette.color(QPalette::Inactive, QPalette::Highlight);
    }

    QRectF rect = QRectF(0, y1, width, y2 - y1);

    painter.setPen(Qt::NoPen);
    painter.setBrush(colour);

    painter.drawRect(rect);
}


void MBigAlphaRuler::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    QRect outerRect =
            QRect(0, contentRect().y(), width(), contentRect().height());

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawRect(outerRect);

    int y1 = (1 - maxValue) * contentRect().height() + contentRect().y();
    int y2 = (1 - minValue) * contentRect().height() + contentRect().y();
    QRectF innerRect = QRectF(0, y1 + 2, width(), y2 - y1 - 4);

    painter.setBrush(Qt::gray);
    painter.drawRect(innerRect);

    QFontMetrics fontMetric(font());
    int fontHeight = fontMetric.height();

    painter.setPen(Qt::black);
    foreach (float v, bigTicks)
    {
        int y1 = contentRect().height() * (1 - v) + contentRect().y();

        int x1 = 0;
        int x2 = width();

        painter.drawLine(QLine(x1, y1, x2, y1));

        QString text = QString::number(v, 'f', 2);

        int y2 = y1 - fontHeight;

        if (y2 < 0)
        {
            y2 = 0;
        }

        int fontWidth = fontMetric.width(text);
        QRect labelRect = QRect(0, y2, fontWidth, fontHeight);

        painter.drawText(labelRect, text);
    }
    painter.setRenderHint(QPainter::Antialiasing, true);

    drawHandle(painter, y1, y1 + 5, width(), overMax);
    drawHandle(painter, y2 - 5, y2, width(), overMin);
}


void MBigAlphaRuler::mouseMoveEvent(QMouseEvent *event)
{
    int maxA = (1 - maxValue) * contentRect().height() + contentRect().y();
    int minA = (1 - minValue) * contentRect().height() + contentRect().y();

    // Handle mouse movement.
    if (event->buttons() == Qt::LeftButton)
    {
        if (overMax)
        {
            maxA = std::min(minA - 10, event->pos().y());
            maxValue = std::min(1.f, (1 - (maxA - contentRect().y()) /
                                      float(contentRect().height())));
            emit rangeChanged(minValue, maxValue);

        }

        if (overMin)
        {
            minA = std::max(maxA + 10, event->pos().y() + 1);
            minValue = std::max(0.f, (1 - (minA - contentRect().y()) /
                                      float(contentRect().height())));
            emit rangeChanged(minValue, maxValue);
        }
    }

    // Change mouse cursor if over max.
    if (event->pos().y() >= maxA && event->pos().y() < maxA + 5)
    {
        if (!overMax)
        {
            QApplication::setOverrideCursor(QCursor(Qt::SizeVerCursor));
            overMax = true;
            repaint();
        }
    }
    else
    {
        if (overMax)
        {
            QApplication::restoreOverrideCursor();
            overMax = false;
            repaint();
        }
    }

    // Change mouse cursor if over min.
    if (event->pos().y() >= minA - 5 && event->pos().y() < minA)
    {
        if (!overMin)
        {
            QApplication::setOverrideCursor(QCursor(Qt::SizeVerCursor));
            overMin = true;
            repaint();
        }
    }
    else
    {
        if (overMin)
        {
            QApplication::restoreOverrideCursor();
            overMin = false;
            repaint();
        }
    }
}


void MBigAlphaRuler::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (overMin || overMax)
    {
        QApplication::restoreOverrideCursor();
    }

    overMin = overMax = false;
    repaint();
}


/******************************************************************************
***                                MColorBox                                ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MColorBox::MColorBox(MColorFunction *colorFunction, QWidget *parent) :
    QWidget(parent),
    colorFunction(colorFunction)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QSize MColorBox::minimumSizeHint() const
{
    return QSize(32, 32);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MColorBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    int index = colorFunction->selectedPoint;
    MColourNodes *colourNodes =
            colorFunction->transferFunction->getColourNodes();
    MColorRGB8 colour = (MColorRGB8)colourNodes->colourAt(index);

    painter.setPen(Qt::gray);
    painter.setBrush(QColor(colour.r, colour.g, colour.b));
    painter.drawRoundedRect(QRect(2, 2, width() - 4, height() - 4), 2, 2);
}


void MColorBox::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    colorFunction->openColorPicker();
}


/******************************************************************************
***                             MChannelsWidget                             ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MChannelsWidget::MChannelsWidget(MEditorTransferFunction *transferFunction,
                                 QWidget *parent) :
    QWidget(parent),
    transferFunction(transferFunction)
{
    setWindowTitle("Colour Channels");

    this->setMinimumSize(500, 400);

    rgbPlot = new QCustomPlot();
    rgbPlot->setMinimumSize(500, 200);

    hclPlot = new QCustomPlot();
    hclPlot->setMinimumSize(500, 200);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(rgbPlot);
    layout->addWidget(hclPlot);

    setLayout(layout);

    rgbPlot->xAxis->setRange(0, 1);
    rgbPlot->yAxis->setRange(0, 1);
    hclPlot->xAxis->setRange(0, 1);
    hclPlot->yAxis->setRange(0, 100);

    rgbPlot->legend->setVisible(true);
    hclPlot->legend->setVisible(true);

    redGraph = rgbPlot->addGraph();
    greenGraph = rgbPlot->addGraph();
    blueGraph = rgbPlot->addGraph();

    QCPAxis *hueAxis = hclPlot->axisRect(0)->axis(QCPAxis::atRight, 0);
    hueAxis->setRange(-360, 360);
    hueAxis->setVisible(true);

    hueGraph = hclPlot->addGraph(0, hueAxis);
    chromaGraph = hclPlot->addGraph();
    luminanceGraph = hclPlot->addGraph();

    redGraph->setPen(QPen(Qt::red));
    greenGraph->setPen(QPen(Qt::green));
    blueGraph->setPen(QPen(Qt::blue));
    redGraph->setName("r");
    greenGraph->setName("g");
    blueGraph->setName("b");

    hueGraph->setPen(QPen(Qt::magenta));
    chromaGraph->setPen(QPen(Qt::cyan));
    luminanceGraph->setPen(QPen(Qt::gray));
    hueGraph->setName("h");
    chromaGraph->setName("c");
    luminanceGraph->setName("l");
}


MChannelsWidget::~MChannelsWidget()
{
    // QCPGraphs are deleted by their plots.
    delete hclPlot;
    delete rgbPlot;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MChannelsWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    MColourNodes *colourNodes = transferFunction->getColourNodes();
    QVector<double> xs(width());

    QVector<double> reds(width());
    QVector<double> greens(width());
    QVector<double> blues(width());

    QVector<double> hues(width());
    QVector<double> chromas(width());
    QVector<double> luminances(width());

    for (int x = 0; x < width(); x++)
    {
        float t = x / (float)(width() - 1);
        MColorXYZ64 color = colourNodes->interpolate(t);
        MColorRGB8 rgb = color.toRGB(100);
        MColorHCL16 hcl = color.toHCL(100);

        xs[x] = t;

        reds[x] = rgb.getR();
        greens[x] = rgb.getG();
        blues[x] = rgb.getB();

        hues[x] = hcl.getH();
        chromas[x] = hcl.getC();
        luminances[x] = hcl.getL();
    }

    redGraph->setData(xs, reds);
    greenGraph->setData(xs, greens);
    blueGraph->setData(xs, blues);

    hueGraph->setData(xs, hues);
    chromaGraph->setData(xs, chromas);
    luminanceGraph->setData(xs, luminances);

    rgbPlot->replot();
    hclPlot->replot();
}


} // namespace TFEditor
} // namespace Met3D
