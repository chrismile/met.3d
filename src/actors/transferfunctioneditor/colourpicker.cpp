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
#include "colourpicker.h"

// standard library imports

// related third party imports
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QMouseEvent>
#include <QPainter>

// local application imports
#include "editortransferfunction.h"

namespace Met3D
{
namespace TFEditor
{

/******************************************************************************
***                             MHCLColorPicker                             ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MHCLColorPicker::MHCLColorPicker(MColourNodes *cs, QWidget *parent) :
    QDialog(parent),
    colourNodes(cs),
    colorIndex(0)
{
    setWindowTitle("HCL Colour Picker");
    setMinimumWidth(500);

    QLabel *hueLabel = new QLabel("Hue:");
    QLabel *chromaLabel = new QLabel("Chroma:");
    QLabel *luminanceLabel = new QLabel("Luminance:");

    hueBox = new QDoubleSpinBox(this);
    chromaBox = new QDoubleSpinBox(this);
    luminanceBox = new QDoubleSpinBox(this);

    hueBox->setRange(-360, 360);
    chromaBox->setRange(0, 100);
    luminanceBox->setRange(0, 100);

    hueRange = new MHCLColorRangeWidget(this, Hue, this);
    chromaRange = new MHCLColorRangeWidget(this, Chroma, this);
    luminanceRange = new MHCLColorRangeWidget(this, Luminance, this);

    QGridLayout *gridlayout = new QGridLayout();
    gridlayout->addWidget(hueLabel, 0, 0);
    gridlayout->addWidget(hueBox, 0, 1);
    gridlayout->addWidget(hueRange, 0, 2);

    gridlayout->addWidget(chromaLabel, 1, 0);
    gridlayout->addWidget(chromaBox, 1, 1);
    gridlayout->addWidget(chromaRange, 1, 2);

    gridlayout->addWidget(luminanceLabel, 2, 0);
    gridlayout->addWidget(luminanceBox, 2, 1);
    gridlayout->addWidget(luminanceRange, 2, 2);

    QComboBox *typeBox = new QComboBox(this);
    typeBox->addItems({"Hue-Chroma", "Hue-Luminance", "Chroma-Luminance"});

    QCheckBox *interpolationPathsBox = new QCheckBox(this);
    interpolationPathsBox->setText("show interpolation paths");
    interpolationPathsBox->setChecked(false);

    range2D = new MHCLColorRange2DWidget(this, HueChroma,
                                     interpolationPathsBox->isChecked(), this);
    range2D->setMinimumHeight(200);
    gridlayout->addWidget(typeBox, 3, 0, 1, 2);
    gridlayout->addWidget(interpolationPathsBox, 4, 0, 1, 2);
    gridlayout->addWidget(range2D, 3, 2, 3, 2);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addLayout(gridlayout);

    setLayout(layout);

    connect(hueBox, SIGNAL(valueChanged(double)),
            this, SLOT(hueBoxChanged(double)));
    connect(chromaBox, SIGNAL(valueChanged(double)),
            this, SLOT(chromaBoxChanged(double)));
    connect(luminanceBox, SIGNAL(valueChanged(double)),
            this, SLOT(luminanceBoxChanged(double)));

    connect(hueRange, SIGNAL(changed(MColourHCL16)),
            this, SLOT(changed(MColourHCL16)));
    connect(chromaRange, SIGNAL(changed(MColourHCL16)),
            this, SLOT(changed(MColourHCL16)));
    connect(luminanceRange, SIGNAL(changed(MColourHCL16)),
            this, SLOT(changed(MColourHCL16)));

    connect(range2D, SIGNAL(changed(MColourHCL16)),
            this, SLOT(changed(MColourHCL16)));

    connect(typeBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(typeBoxChanged(int)));

    connect(interpolationPathsBox, SIGNAL(clicked(bool)),
            this, SLOT(interpolationPathsBoxChanged(bool)));

    changed();
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MHCLColorPicker::setCurrentIndex(int index)
{
    colorIndex = index;
    currentColor = (MColourHCL16)colourNodes->colourAt(colorIndex);
    changed();
}


const MColourHCL16& MHCLColorPicker::color() const
{
    return currentColor;
}


MColourHCL16& MHCLColorPicker::color()
{
    return currentColor;
}


/******************************************************************************
***                            PRIVATE SLOTS                                ***
*******************************************************************************/

void MHCLColorPicker::changed(bool updateBoxes)
{
    if (updateBoxes)
    {
        hueBox->blockSignals(true);
        chromaBox->blockSignals(true);
        luminanceBox->blockSignals(true);

        hueBox->setValue(color().getH());
        chromaBox->setValue(color().getC());
        luminanceBox->setValue(color().getL());

        hueBox->blockSignals(false);
        chromaBox->blockSignals(false);
        luminanceBox->blockSignals(false);
    }

    hueRange->repaint();
    chromaRange->repaint();
    luminanceRange->repaint();

    range2D->repaint();

    emit colorChanged(color());
}


void MHCLColorPicker::hueBoxChanged(double value)
{
    color().setH(value);
    changed(false);
}


void MHCLColorPicker::chromaBoxChanged(double value)
{
    color().setC(value);
    changed(false);
}


void MHCLColorPicker::luminanceBoxChanged(double value)
{
    color().setL(value);
    changed(false);
}


void MHCLColorPicker::changed(const MColourHCL16& c)
{
    color() = c;
    changed();
}


void MHCLColorPicker::typeBoxChanged(int index)
{
    range2D->setType((HCLType2D)index);
    range2D->repaint();
}


void MHCLColorPicker::interpolationPathsBoxChanged(bool checked)
{
    range2D->setShowInterpolationPaths(checked);
    range2D->repaint();
}


/******************************************************************************
***                          MHCLColorRangeWidget                           ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MHCLColorRangeWidget::MHCLColorRangeWidget(
        MHCLColorPicker *colorPicker,
        HCLType1D type,
        QWidget *parent) :
    QWidget(parent),
    colorPicker(colorPicker),
    type(type)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MColourHCL16 MHCLColorRangeWidget::getColor(float t) const
{
    MColourHCL16 color = colorPicker->color();
    switch(type)
    {
    case Hue:
        color.setH((t - 0.5f) * 720);
        return color;
    case Chroma:
        color.setC(t * 100);
        return color;
    case Luminance:
        color.setL(t * 100);
        return color;
    default:
        return color;
    }
}


float MHCLColorRangeWidget::getValue(const MColourHCL16& color) const
{
    switch(type)
    {
    case Hue:
        return color.getH() / 720 + 0.5f;
    case Chroma:
        return color.getC() / 100;
    case Luminance:
        return color.getL() / 100;
    default:
        return 0;
    }
}


float MHCLColorRangeWidget::getValue() const
{
    return getValue(colorPicker->color());
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MHCLColorRangeWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    QVector<QRgb> function(width());
    for (int i = 0; i < width(); i++)
    {
        float t = i / float(width() + 1);

        MColourXYZ64 xyz = (MColourXYZ64)getColor(t);
        MColourRGB8 rgb = (MColourRGB8)xyz;

        function[i] = qRgb(rgb.r, rgb.g, rgb.b);
    }

    int b = 5;
    int h = height() - b;
    QImage image((uchar*)function.data(), width(), 1, QImage::Format_ARGB32);
    painter.drawImage(QRect(0, 0, width(), h), image, image.rect());


    float t = getValue();
    QPoint point(t * width(), h + 1);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(Qt::black, 1.2));
    painter.drawLine(point, point + QPoint(-b, b));
    painter.drawLine(point, point + QPoint(b, b));
}


void MHCLColorRangeWidget::mousePressEvent(QMouseEvent *event)
{
    float t = event->pos().x() / float(width());
    t = std::max(0.f, std::min(t, 1.f));
    emit changed(getColor(t));
}


void MHCLColorRangeWidget::mouseMoveEvent(QMouseEvent *event)
{
    float t = event->pos().x() / float(width());
    t = std::max(0.f, std::min(t, 1.f));
     emit changed(getColor(t));
}


/******************************************************************************
***                         MHCLColorRange2DWidget                          ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MHCLColorRange2DWidget::MHCLColorRange2DWidget(
        MHCLColorPicker *colorPicker,
        HCLType2D type,
        bool showInterpolationPaths,
        QWidget *parent) :
    QWidget(parent),
    colourPicker(colorPicker),
    type(type),
    showInterpolationPaths(showInterpolationPaths),
    moveColourNode(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    dir = QVector2D(-1, 1);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MHCLColorRange2DWidget::setType(HCLType2D type)
{
    this->type = type;
}


void MHCLColorRange2DWidget::setShowInterpolationPaths(bool show)
{
    showInterpolationPaths = show;
}


MColourHCL16 MHCLColorRange2DWidget::getColor(float tx, float ty) const
{
    MColourHCL16 color = colourPicker->color();
    switch(type)
    {
    case HueChroma:
        color.setH((tx - 0.5f) * 720);
        color.setC(ty * 100);
        break;
    case HueLuminance:
         color.setH((tx - 0.5f) * 720);
         color.setL(ty * 100);
        break;
    case ChromaLuminance:
        color.setC(tx * 100);
        color.setL(ty * 100);
        break;
    default:
        break;
    }

    return color;
}


float MHCLColorRange2DWidget::getValueX() const
{
    return getValueX(colourPicker->color());
}


float MHCLColorRange2DWidget::getValueX(const MColourHCL16& color) const
{
    switch(type)
    {
    case HueChroma:
        return color.getH() / 720 + 0.5f;
    case HueLuminance:
        return color.getH() / 720 + 0.5f;
    case ChromaLuminance:
        return color.getC() / 100;
    default:
        return 0;
    }
}


float MHCLColorRange2DWidget::getValueY() const
{
    return getValueY(colourPicker->color());
}


float MHCLColorRange2DWidget::getValueY(const MColourHCL16& color) const
{
    switch(type)
    {
    case HueChroma:
        return color.getC() / 100;
    case HueLuminance:
        return color.getL() / 100;
    case ChromaLuminance:
        return color.getL() / 100;
    default:
        return 0;
    }
}


float MHCLColorRange2DWidget::getValueZ() const
{
    return getValueZ(colourPicker->color());
}


float MHCLColorRange2DWidget::getValueZ(const MColourHCL16& color) const
{
    switch(type)
    {
    case HueChroma:
        return color.getL() / 100;
    case HueLuminance:
        return color.getC() / 100;
    case ChromaLuminance:
        return color.getH() / 720 + 0.5f;
    default:
        return 0;
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MHCLColorRange2DWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    // Draw HCL Image.
    int nX = width();
    int nY = height();

    // Write colour values into array to generate image from, which represents
    // the colour range to draw.
    QVector<QRgb> function(nX * nY);
    for (int x = 0; x < nX; x++)
    {
        float tx = x / float(nX + 1);

        for (int y = 0; y < nY; y++)
        {
            float ty = y / float(nY + 1);

            MColourXYZ64 xyz = (MColourXYZ64)getColor(tx, ty);
            MColourRGB8 rgb = (MColourRGB8)xyz;

            function[x + y * nX] = qRgb(rgb.r, rgb.g, rgb.b);
        }
    }

    // Generate image from array filled beforehand.
    QImage image((uchar*)function.data(), nX, nY, QImage::Format_ARGB32);
    painter.drawImage(rect(), image, image.rect());

    painter.setRenderHint(QPainter::Antialiasing);

    // Draw colour nodes.
    if (showInterpolationPaths &&
            colourPicker->colourNodes != nullptr &&
            colourPicker->colorIndex >= 0)
    {
        QVector<ColourNode> nodes = colourPicker->colourNodes->nodes;
        float currentPos = nodes[colourPicker->colorIndex].first;

        std::sort(nodes.begin(), nodes.end(),
                  [](const ColourNode& a, const ColourNode& b)
        { return a.first < b.first; });

        QVector<QPoint> points1, points2;

        for (int i = 0; i < nodes.size(); i++)
        {
            MColourHCL16 color;

            if (nodes[i].first == currentPos)
            {
                color = colourPicker->color();
            }
            else
            {
                color = (MColourHCL16)nodes[i].second;
            }

            float tx = getValueX(color);
            float ty = getValueY(color);

            QPoint point = QPoint(tx * width(), ty * height());
            points1.push_back(point);

            float delta = getValueZ() - getValueZ(color);
            point += (dir * delta * 100).toPoint();
            points2.push_back(point);
        }

        painter.setPen(QPen(Qt::lightGray, 1.5f));
        painter.drawPolyline(points2.data(), points2.size());

        painter.setPen(QPen(Qt::white, 1.5f, Qt::DashLine));
        painter.drawPolyline(points1.data(), points1.size());

        for (int i = 0; i < nodes.size(); i++)
        {
            QPoint point1 = points1[i];
            QPoint point2 = points2[i];

            if (nodes[i].first == currentPos)
            {
                continue;
            }
            else
            {
                if ((point2.x() < point1.x()) == (dir.x() < 0))
                {
                    painter.setPen(QPen(Qt::lightGray, 1.5f, Qt::DashLine));
                }
                else
                {
                    painter.setPen(QPen(Qt::lightGray, 1.5f, Qt::DotLine));
                }
                painter.drawLine(point1, point2);

                painter.setBrush(Qt::lightGray);
                painter.setPen(QPen(Qt::lightGray, 1.5f));
                painter.drawEllipse(point2, 3, 3);
            }
        }

        int size = 15;

        QPoint offset(5, height() - size * 2 - 5);
        painter.setPen(QPen(Qt::lightGray, 1.5, Qt::SolidLine));

        drawArrow(painter,
                  offset + QPoint(size, size * 2),
                  offset + QPoint(size, 0));
        drawArrow(painter,
                  offset + QPoint(0, size),
                  offset + QPoint(size * 2, size));

        painter.setPen(QPen(Qt::lightGray, 1.5f, Qt::DashLine));
        painter.drawLine(offset + QPoint(size, size),
                   offset + QPoint(size, size) + (dir * size / 1.4).toPoint());

        painter.setPen(QPen(Qt::lightGray, 1.5f, Qt::DotLine));
        drawArrow(painter,
                  offset + QPoint(size, size),
                  offset + QPoint(size, size) - (dir * size / 1.4).toPoint());
    }

    // Draw selection
    MColourHCL16 color = colourPicker->color();

    float tx = getValueX(color);
    float ty = getValueY(color);

    QPoint point = QPoint(tx * width(), ty * height());

    painter.setBrush(Qt::transparent);
    painter.setPen(QPen(Qt::white, 2));
    painter.drawEllipse(point, 5, 5);

}


void MHCLColorRange2DWidget::mousePressEvent(QMouseEvent * event)
{
    // Control of current colour value.
    if (event->buttons() & Qt::LeftButton)
    {
        float mousePosX = event->pos().x() / float(width());
        mousePosX = std::max(0.f, std::min(mousePosX, 1.f));

        float mousePosY = event->pos().y() / float(height());
        mousePosY = std::max(0.f, std::min(mousePosY, 1.f));

        MColourHCL16 color = colourPicker->color();

        float nodePosX = getValueX(color);
        float nodePosY = getValueY(color);

        float distX = (mousePosX - nodePosX) * width();
        float distY = (mousePosY - nodePosY) * height();

        moveColourNode = ( (distX * distX + distY * distY) <= (7.f * 7.f));

        // Only move node if user "picked" it. (Don't "jump" from one position
        // to the other)
        if (moveColourNode)
        {
            emit changed(getColor(mousePosX, mousePosY));
        }
    }
    // Control of indicators.
    else if (event->buttons() & Qt::RightButton)
    {
        mouseStart = event->pos();
    }
}


void MHCLColorRange2DWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    // Control of current colour value.
    if (event->buttons() & Qt::LeftButton)
    {
        float mousePosX = event->pos().x() / float(width());
        mousePosX = std::max(0.f, std::min(mousePosX, 1.f));

        float mousePosY = event->pos().y() / float(height());
        mousePosY = std::max(0.f, std::min(mousePosY, 1.f));

        moveColourNode = true;

        emit changed(getColor(mousePosX, mousePosY));
    }
}


void MHCLColorRange2DWidget::mouseMoveEvent(QMouseEvent * event)
{
    // Control of current colour value.
    if (event->buttons() & Qt::LeftButton)
    {
        // Only move node if user "picked" it. (Don't "jump" from one position
        // to the other)
        if (moveColourNode)
        {
            float mousePosX = event->pos().x() / float(width());
            mousePosX = std::max(0.f, std::min(mousePosX, 1.f));

            float mousePosY = event->pos().y() / float(height());
            mousePosY = std::max(0.f, std::min(mousePosY, 1.f));

            emit changed(getColor(mousePosX, mousePosY));
        }
    }
    // Control of indicators.
    else if (event->buttons() & Qt::RightButton)
    {
        QPoint delta = event->pos() - mouseStart;
        dir = QVector2D(delta);
        dir /= 100;
        repaint();
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MHCLColorRange2DWidget::drawArrow(QPainter& painter, const QPoint& startPos,
                                       const QPoint& endPos)
{
    painter.drawLine(startPos, endPos);


    QVector2D direction(endPos - startPos);
    direction.normalize();

    QVector2D normal(direction.y(), -direction.x());
    float width = painter.pen().widthF() * 2;

    QVector2D height(endPos);

    QPolygon poly;
    poly.append((height - direction * width + normal * width).toPoint());
    poly.append(height.toPoint());
    poly.append((height - direction * width - normal * width).toPoint());

    QPen pen = painter.pen();
    QBrush brush = painter.brush();

    painter.setPen(QPen(pen.color(), 1));
    painter.setBrush(pen.color());
    painter.drawPolygon(poly);

    painter.setPen(pen);
    painter.setBrush(brush);
}


} // namespace TFEditor
} // namespace Met3D
