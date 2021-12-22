/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017-2018 Marc Rautenhaus
**  Copyright 2017-2018 Bianca Tost
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
#include "transferfunction.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <QMouseEvent>

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/gl/typedvertexbuffer.h"
#include "util/mutil.h"
#include "actors/transferfunction1d.h"
#include "actors/spatial1dtransferfunction.h"
#include "data/multivar/hidpi.h"
#include "mainwindow.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTransferFunction::MTransferFunction(QObject *parent) :
    MActor(parent),
    tfTexture(nullptr),
    vertexBuffer(nullptr),
    minimumValue(0.f),
    maximumValue(100.f)
{
    scaleFactor = getHighDPIScaleFactor();
    boxMargin = boxMarginBase * scaleFactor;
    resizeMargin = resizeMarginBase * scaleFactor;

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    // Properties related to labelling the colour bar.
    // ===============================================

    maxNumTicksProperty = addProperty(INT_PROPERTY, "num. ticks",
                                      labelPropertiesSupGroup);
    properties->mInt()->setValue(maxNumTicksProperty, 11);
    properties->mInt()->setMinimum(maxNumTicksProperty, 0);

    maxNumLabelsProperty = addProperty(INT_PROPERTY, "num. labels",
                                       labelPropertiesSupGroup);
    properties->mInt()->setValue(maxNumLabelsProperty, 6);
    properties->mInt()->setMinimum(maxNumLabelsProperty, 0);

    tickWidthProperty = addProperty(DOUBLE_PROPERTY, "tick length",
                                    labelPropertiesSupGroup);
    properties->setDouble(tickWidthProperty, 0.015, 3, 0.001);

    labelSpacingProperty = addProperty(DOUBLE_PROPERTY, "space label-tick",
                                       labelPropertiesSupGroup);
    properties->setDouble(labelSpacingProperty, 0.01, 3, 0.001);

    // Properties related to data range.
    // =================================

    rangePropertiesSubGroup = addProperty(GROUP_PROPERTY, "range",
                                          actorPropertiesSupGroup);

    int significantDigits = 3;
    double step = 1.;

    minimumValueProperty = addProperty(SCIENTIFICDOUBLE_PROPERTY, "minimum value",
                                       rangePropertiesSubGroup);
    properties->setSciDouble(minimumValueProperty, minimumValue,
                           significantDigits, step);

    maximumValueProperty = addProperty(SCIENTIFICDOUBLE_PROPERTY, "maximum value",
                                       rangePropertiesSubGroup);
    properties->setSciDouble(maximumValueProperty, maximumValue,
                           significantDigits, step);

    valueOptionsPropertiesSubGroup = addProperty(
                GROUP_PROPERTY, "min/max options", rangePropertiesSubGroup);

    valueSignificantDigitsProperty = addProperty(
                INT_PROPERTY, "significant digits", valueOptionsPropertiesSubGroup);
    properties->setInt(valueSignificantDigitsProperty, significantDigits, 0, 9);

    valueStepProperty = addProperty(SCIENTIFICDOUBLE_PROPERTY, "step",
                                    valueOptionsPropertiesSubGroup);
    properties->setSciDouble(valueStepProperty, step, significantDigits, 0.1);

    // General properties.
    // ===================

    positionProperty = addProperty(RECTF_CLIP_PROPERTY, "position",
                                   actorPropertiesSupGroup);
    properties->setRectF(positionProperty, QRectF(0.9, 0.9, 0.05, 0.5), 2);

    endInitialiseQtProperties();
}


MTransferFunction::~MTransferFunction()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MTransferFunction::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MTransferFunction::getSettingsID());

    // Properties related to labelling the colour bar.
    // ===============================================
    settings->setValue("maxNumTicks",
                       properties->mInt()->value(maxNumTicksProperty));
    settings->setValue("maxNumLabels",
                       properties->mInt()->value(maxNumLabelsProperty));
    settings->setValue("tickLength",
                       properties->mDouble()->value(tickWidthProperty));
    settings->setValue("labelSpacing",
                       properties->mDouble()->value(labelSpacingProperty));

    // Properties related to data range.
    // =================================
    settings->setValue("minimumValue",
                       properties->mSciDouble()->value(minimumValueProperty));
    settings->setValue("maximumValue",
                       properties->mSciDouble()->value(maximumValueProperty));
    settings->setValue("valueSignificantDigits",
                       properties->mInt()->value(valueSignificantDigitsProperty));
    settings->setValue("valueStep",
                       properties->mInt()->value(valueSignificantDigitsProperty));

    // General properties.
    // ===================
    settings->setValue("position",
                       properties->mRectF()->value(positionProperty));

    settings->endGroup();
}


void MTransferFunction::loadConfiguration(QSettings *settings)
{
    // For compatibilty with versions < 1.2.
    QStringList versionString = readConfigVersionID(settings);
    if (versionString[0].toInt() <= 1  && versionString[1].toInt() < 2)
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(this))
        {
            settings->beginGroup(tf->getSettingsID());
        }
        else if(MSpatial1DTransferFunction *tf =
                dynamic_cast<MSpatial1DTransferFunction*>(this))
        {
            settings->beginGroup(tf->getSettingsID());
        }
    }
    else
    {
        settings->beginGroup(MTransferFunction::getSettingsID());
    }

    // Properties related to labelling the colour bar.
    // ===============================================
    properties->mInt()->setValue(maxNumTicksProperty,
                                 settings->value("maxNumTicks", 11).toInt());
    properties->mInt()->setValue(maxNumLabelsProperty,
                                 settings->value("maxNumLabels", 6).toInt());

    properties->mDouble()->setValue(
                tickWidthProperty,
                settings->value("tickLength", 0.015).toDouble());
    properties->mDouble()->setValue(
                labelSpacingProperty,
                settings->value("labelSpacing", 0.010).toDouble());

    // Properties related to data range.
    // =================================
    int significantDigits = 3;

    // Support of old configuration files.
    if (settings->contains("valueDecimals"))
    {
        significantDigits = settings->value("valueDecimals", 3).toInt();
    }
    else
    {
        significantDigits = settings->value("valueSignificantDigits", 3).toInt();
    }

    setValueSignificantDigits(significantDigits);
    setValueStep(settings->value("valueStep", 1.).toDouble());
    setMinimumValue(settings->value("minimumValue", 0.).toDouble());
    setMaximumValue(settings->value("maximumValue", 100.).toDouble());

    // General properties.
    // ===================
    setPosition(settings->value("position",
                                QRectF(0.9, 0.9, 0.05, 0.5)).toRectF());

    settings->endGroup();
}


void MTransferFunction::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);

    beginCompileShaders(2);

    compileShadersFromFileWithProgressDialog(
                simpleGeometryShader,
                "src/glsl/simple_coloured_geometry.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                colourbarShader,
                "src/glsl/colourbar.fx.glsl");

    endCompileShaders();
}


void MTransferFunction::setMinimumValue(float value)
{
    properties->mSciDouble()->setValue(minimumValueProperty, value);
}


void MTransferFunction::setMaximumValue(float value)
{
    properties->mSciDouble()->setValue(maximumValueProperty, value);
}


void MTransferFunction::setValueSignificantDigits(int significantDigits)
{
    properties->mInt()->setValue(valueSignificantDigitsProperty,
                                 significantDigits);
    properties->mSciDouble()->setSignificantDigits(minimumValueProperty,
                                                 significantDigits);
    properties->mSciDouble()->setSignificantDigits(maximumValueProperty,
                                                 significantDigits);
    properties->mSciDouble()->setSignificantDigits(valueStepProperty,
                                                 significantDigits);
}


void MTransferFunction::setValueStep(double step)
{
    properties->mSciDouble()->setValue(valueSignificantDigitsProperty, step);
    properties->mSciDouble()->setSingleStep(minimumValueProperty, step);
    properties->mSciDouble()->setSingleStep(maximumValueProperty, step);
}


void MTransferFunction::setPosition(QRectF position)
{
    properties->mRectF()->setValue(positionProperty, position);
}


void MTransferFunction::setNumTicks(int num)
{
    properties->mInt()->setValue(maxNumTicksProperty, num);
}


void MTransferFunction::setNumLabels(int num)
{
    properties->mInt()->setValue(maxNumLabelsProperty, num);
}


QString MTransferFunction::transferFunctionName()
{
    return getName();
}


bool MTransferFunction::loadMissingTransferFunction(
        QString tfName, QString tfActorType, QString callerDescriptor,
        QString callerName, QSettings *settings)
{
    // Append space to caller descriptor if missing.
    if (!callerDescriptor.isEmpty() && !callerDescriptor.endsWith(" "))
    {
        callerDescriptor.append(" ");
    }

    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(callerName);
    msgBox.setText(QString("%1'%2' requires a transfer function "
                           "'%3' that does not exist.\n"
                           "Would you like to load the transfer function "
                           "from file?")
                   .arg(callerDescriptor).arg(callerName).arg(tfName));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.button(QMessageBox::Yes)->setText("Load transfer function");
    msgBox.button(QMessageBox::No)->setText("Discard dependency");
    msgBox.exec();
    if (msgBox.clickedButton() == msgBox.button(QMessageBox::Yes))
    {
        MSystemManagerAndControl *sysMC =
                MSystemManagerAndControl::getInstance();
        // Create default actor to get name of actor factory.
        sysMC->getMainWindow()->getSceneManagementDialog()
                ->loadRequiredActorFromFile(tfActorType,
                                            tfName,
                                            settings->fileName());
    }
    else
    {
        return false;
    }
    return true;
}


bool MTransferFunction::checkVirtualWindowBelowMouse(
        MSceneViewGLWidget *sceneView, int mousePositionX, int mousePositionY) {
    mousePositionY = sceneView->getViewPortHeight() - mousePositionY - 1;
    QRectF screenSpaceRect = getScreenSpaceRect(sceneView);
    return screenSpaceRect.contains(mousePositionX, mousePositionY);
}


void MTransferFunction::mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) {
    if (event->buttons() == Qt::NoButton) {
        resizeDirection = ResizeDirection::NONE;
        isDraggingWindow = false;
    }

    if (resizeDirection != ResizeDirection::NONE) {
        QRectF screenSpaceRect = getScreenSpaceRect(sceneView);
        double diffX = double(event->x() - lastResizeMouseX);
        double diffY = -double(event->y() - lastResizeMouseY);
        if ((resizeDirection & ResizeDirection::LEFT) != 0) {
            if (screenSpaceRect.left() + diffX + (boxMargin + resizeMargin) * 2 > screenSpaceRect.right()) {
                diffX = screenSpaceRect.right() - screenSpaceRect.left() - (boxMargin + resizeMargin) * 2;
            }
            screenSpaceRect.setLeft(screenSpaceRect.left() + diffX);
        }
        if ((resizeDirection & ResizeDirection::RIGHT) != 0) {
            if (screenSpaceRect.right() + diffX - (boxMargin + resizeMargin) * 2 < screenSpaceRect.left()) {
                diffX = screenSpaceRect.left() - screenSpaceRect.right() + (boxMargin + resizeMargin) * 2;
            }
            screenSpaceRect.setRight(screenSpaceRect.right() + diffX);
        }
        if ((resizeDirection & ResizeDirection::BOTTOM) != 0) {
            if (screenSpaceRect.top() + diffY + (boxMargin + resizeMargin) * 2 > screenSpaceRect.bottom()) {
                diffY = screenSpaceRect.bottom() - screenSpaceRect.top() - (boxMargin + resizeMargin) * 2;
            }
            screenSpaceRect.setTop(screenSpaceRect.top() + diffY);
        }
        if ((resizeDirection & ResizeDirection::TOP) != 0) {
            if (screenSpaceRect.bottom() + diffY - (boxMargin + resizeMargin) * 2 < screenSpaceRect.top()) {
                diffY = screenSpaceRect.top() - screenSpaceRect.bottom() + (boxMargin + resizeMargin) * 2;
            }
            screenSpaceRect.setBottom(screenSpaceRect.bottom() + diffY);
        }
        lastResizeMouseX = event->x();
        lastResizeMouseY = event->y();
        setScreenSpaceRect(sceneView, screenSpaceRect);
    } else {
        QRectF screenSpaceRect = getScreenSpaceRect(sceneView);
        int viewportHeight = sceneView->getViewPortHeight();
        QVector2D mousePosition(float(event->x()), float(viewportHeight - event->y() - 1));

        QRectF leftAabb;
        leftAabb.setRect(
                screenSpaceRect.left(), screenSpaceRect.top(),
                resizeMargin, screenSpaceRect.height());
        QRectF rightAabb;
        rightAabb.setRect(
                screenSpaceRect.right() - resizeMargin, screenSpaceRect.top(),
                resizeMargin, screenSpaceRect.height());
        QRectF bottomAabb;
        bottomAabb.setRect(
                screenSpaceRect.left(), screenSpaceRect.top(),
                screenSpaceRect.width(), resizeMargin);
        QRectF topAabb;
        topAabb.setRect(
                screenSpaceRect.left(), screenSpaceRect.bottom() - resizeMargin,
                screenSpaceRect.width(), resizeMargin);

        ResizeDirection resizeDirectionCurr = ResizeDirection::NONE;
        if (leftAabb.contains(mousePosition.x(), mousePosition.y())) {
            resizeDirectionCurr = ResizeDirection(resizeDirectionCurr | ResizeDirection::LEFT);
        }
        if (rightAabb.contains(mousePosition.x(), mousePosition.y())) {
            resizeDirectionCurr = ResizeDirection(resizeDirectionCurr | ResizeDirection::RIGHT);
        }
        if (bottomAabb.contains(mousePosition.x(), mousePosition.y())) {
            resizeDirectionCurr = ResizeDirection(resizeDirectionCurr | ResizeDirection::BOTTOM);
        }
        if (topAabb.contains(mousePosition.x(), mousePosition.y())) {
            resizeDirectionCurr = ResizeDirection(resizeDirectionCurr | ResizeDirection::TOP);
        }

        Qt::CursorShape newCursorShape = Qt::ArrowCursor;
        if (resizeDirectionCurr == ResizeDirection::LEFT
            || resizeDirectionCurr == ResizeDirection::RIGHT) {
            newCursorShape = Qt::SizeHorCursor;
        } else if (resizeDirectionCurr == ResizeDirection::BOTTOM
                   || resizeDirectionCurr == ResizeDirection::TOP) {
            newCursorShape = Qt::SizeVerCursor;
        } else if (resizeDirectionCurr == ResizeDirection::BOTTOM_LEFT
                   || resizeDirectionCurr == ResizeDirection::TOP_RIGHT) {
            newCursorShape = Qt::SizeBDiagCursor;
        } else if (resizeDirectionCurr == ResizeDirection::TOP_LEFT
                   || resizeDirectionCurr == ResizeDirection::BOTTOM_RIGHT) {
            newCursorShape = Qt::SizeFDiagCursor;
        } else {
            newCursorShape = Qt::ArrowCursor;
        }

        if (newCursorShape != cursorShape) {
            cursorShape = newCursorShape;
            if (cursorShape == Qt::ArrowCursor) {
                sceneView->unsetCursor();
            } else {
                sceneView->setCursor(cursorShape);
            }
        }

        if (resizeDirectionCurr != ResizeDirection::NONE) {
            setScreenSpaceRect(sceneView, screenSpaceRect);
        }
    }

    if (isDraggingWindow) {
        QRectF screenSpaceRect = getScreenSpaceRect(sceneView);
        screenSpaceRect.setRect(
                windowOffsetXBase + qreal(event->x() - mouseDragStartPosX),
                windowOffsetYBase - qreal(event->y() - mouseDragStartPosY),
                screenSpaceRect.width(), screenSpaceRect.height());
        setScreenSpaceRect(sceneView, screenSpaceRect);
    }
}


void MTransferFunction::mouseMoveEventParent(MSceneViewGLWidget *sceneView, QMouseEvent *event) {
    if (event->buttons() == Qt::NoButton) {
        resizeDirection = ResizeDirection::NONE;
        isDraggingWindow = false;
    }

    if (resizeDirection != ResizeDirection::NONE) {
        QRectF screenSpaceRect = getScreenSpaceRect(sceneView);
        double diffX = double(event->x() - lastResizeMouseX);
        double diffY = -double(event->y() - lastResizeMouseY);
        if ((resizeDirection & ResizeDirection::LEFT) != 0) {
            if (screenSpaceRect.left() + diffX + (boxMargin + resizeMargin) * 2 > screenSpaceRect.right()) {
                diffX = screenSpaceRect.right() - screenSpaceRect.left() - (boxMargin + resizeMargin) * 2;
            }
            screenSpaceRect.setLeft(screenSpaceRect.left() + diffX);
        }
        if ((resizeDirection & ResizeDirection::RIGHT) != 0) {
            if (screenSpaceRect.right() + diffX - (boxMargin + resizeMargin) * 2 < screenSpaceRect.left()) {
                diffX = screenSpaceRect.left() - screenSpaceRect.right() + (boxMargin + resizeMargin) * 2;
            }
            screenSpaceRect.setRight(screenSpaceRect.right() + diffX);
        }
        if ((resizeDirection & ResizeDirection::BOTTOM) != 0) {
            if (screenSpaceRect.top() + diffY + (boxMargin + resizeMargin) * 2 > screenSpaceRect.bottom()) {
                diffY = screenSpaceRect.bottom() - screenSpaceRect.top() - (boxMargin + resizeMargin) * 2;
            }
            screenSpaceRect.setTop(screenSpaceRect.top() + diffY);
        }
        if ((resizeDirection & ResizeDirection::TOP) != 0) {
            if (screenSpaceRect.bottom() + diffY - (boxMargin + resizeMargin) * 2 < screenSpaceRect.top()) {
                diffY = screenSpaceRect.top() - screenSpaceRect.bottom() + (boxMargin + resizeMargin) * 2;
            }
            screenSpaceRect.setBottom(screenSpaceRect.bottom() + diffY);
        }
        lastResizeMouseX = event->x();
        lastResizeMouseY = event->y();
        setScreenSpaceRect(sceneView, screenSpaceRect);
    } else {
        if (cursorShape != Qt::ArrowCursor) {
            cursorShape = Qt::ArrowCursor;
            sceneView->unsetCursor();
        }
    }

    if (isDraggingWindow) {
        QRectF screenSpaceRect = getScreenSpaceRect(sceneView);
        screenSpaceRect.setRect(
                screenSpaceRect.x() + qreal(event->x() - mouseDragStartPosX),
                windowOffsetYBase - qreal(event->y() - mouseDragStartPosY),
                screenSpaceRect.width(), screenSpaceRect.height());
        setScreenSpaceRect(sceneView, screenSpaceRect);
    }
}


void MTransferFunction::mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) {
    QRectF screenSpaceRect = getScreenSpaceRect(sceneView);

    if (event->button() == Qt::MouseButton::LeftButton) {
        // First, check if a resize event was started.
        int viewportHeight = sceneView->getViewPortHeight();
        QVector2D mousePosition(float(event->x()), float(viewportHeight - event->y() - 1));

        QRectF leftAabb;
        leftAabb.setRect(
                screenSpaceRect.left(), screenSpaceRect.top(),
                resizeMargin, screenSpaceRect.height());
        QRectF rightAabb;
        rightAabb.setRect(
                screenSpaceRect.right() - resizeMargin, screenSpaceRect.top(),
                resizeMargin, screenSpaceRect.height());
        QRectF bottomAabb;
        bottomAabb.setRect(
                screenSpaceRect.left(), screenSpaceRect.top(),
                screenSpaceRect.width(), resizeMargin);
        QRectF topAabb;
        topAabb.setRect(
                screenSpaceRect.left(), screenSpaceRect.bottom() - resizeMargin,
                screenSpaceRect.width(), resizeMargin);

        resizeDirection = ResizeDirection::NONE;
        if (leftAabb.contains(mousePosition.x(), mousePosition.y())) {
            resizeDirection = ResizeDirection(resizeDirection | ResizeDirection::LEFT);
        }
        if (rightAabb.contains(mousePosition.x(), mousePosition.y())) {
            resizeDirection = ResizeDirection(resizeDirection | ResizeDirection::RIGHT);
        }
        if (bottomAabb.contains(mousePosition.x(), mousePosition.y())) {
            resizeDirection = ResizeDirection(resizeDirection | ResizeDirection::BOTTOM);
        }
        if (topAabb.contains(mousePosition.x(), mousePosition.y())) {
            resizeDirection = ResizeDirection(resizeDirection | ResizeDirection::TOP);
        }

        if (resizeDirection != ResizeDirection::NONE) {
            lastResizeMouseX = event->x();
            lastResizeMouseY = event->y();
        }

        if (resizeDirection != ResizeDirection::NONE) {
            setScreenSpaceRect(sceneView, screenSpaceRect);
        }
    }

    if (resizeDirection == ResizeDirection::NONE && event->button() == Qt::MouseButton::LeftButton) {
        isDraggingWindow = true;
        windowOffsetXBase = screenSpaceRect.x();
        windowOffsetYBase = screenSpaceRect.y();
        mouseDragStartPosX = event->x();
        mouseDragStartPosY = event->y();
    }
}


void MTransferFunction::mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event) {
    if (event->button() == Qt::MouseButton::LeftButton) {
        resizeDirection = ResizeDirection::NONE;
        isDraggingWindow = false;
    }
}


void MTransferFunction::wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event) {
    ;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTransferFunction::initializeActorResources()
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    textureUnit = assignTextureUnit();

    generateTransferTexture();

    // Load shader programs.
    bool loadShaders = false;

    loadShaders |= glRM->generateEffectProgram("transfer_colourbar",
                                               colourbarShader);
    loadShaders |= glRM->generateEffectProgram("transfer_geom",
                                               simpleGeometryShader);

    if (loadShaders) reloadShaderEffects();

    generateBarGeometry();
}


QRectF MTransferFunction::getScreenSpaceRect(MSceneViewGLWidget *sceneView) {
    QRectF clipSpaceRect = properties->mRectF()->value(positionProperty);
    qreal x0 = clipSpaceRect.x();
    qreal y0 = clipSpaceRect.y() - clipSpaceRect.height();
    qreal x1 = clipSpaceRect.x() + clipSpaceRect.width();
    qreal y1 = clipSpaceRect.y();
    x0 = (x0 + qreal(1.0)) * qreal(0.5) * sceneView->getViewPortWidth() - boxMargin;
    x1 = (x1 + qreal(1.0)) * qreal(0.5) * sceneView->getViewPortWidth() + boxMargin;
    y0 = (y0 + qreal(1.0)) * qreal(0.5) * sceneView->getViewPortHeight() - boxMargin;
    y1 = (y1 + qreal(1.0)) * qreal(0.5) * sceneView->getViewPortHeight() + boxMargin;
    QRectF screenSpaceRect;
    screenSpaceRect.setCoords(x0, y0, x1, y1);
    return screenSpaceRect;
}


void MTransferFunction::setScreenSpaceRect(MSceneViewGLWidget *sceneView, const QRectF &screenSpaceRect) {
    qreal x0 = screenSpaceRect.x();
    qreal y0 = screenSpaceRect.y();
    qreal x1 = screenSpaceRect.x() + screenSpaceRect.width();
    qreal y1 = screenSpaceRect.y() + screenSpaceRect.height();
    x0 = (x0 + boxMargin) * qreal(2.0) / sceneView->getViewPortWidth() - qreal(1.0);
    x1 = (x1 - boxMargin) * qreal(2.0) / sceneView->getViewPortWidth() - qreal(1.0);
    y0 = (y0 + boxMargin) * qreal(2.0) / sceneView->getViewPortHeight() - qreal(1.0);
    y1 = (y1 - boxMargin) * qreal(2.0) / sceneView->getViewPortHeight() - qreal(1.0);
    qreal width = x1 - x0;
    qreal height = y1 - y0;
    QRectF clipSpaceRect;
    clipSpaceRect.setRect(x0, y1, width, height);
    properties->mRectF()->setValue(positionProperty, clipSpaceRect);
}

} // namespace Met3D
