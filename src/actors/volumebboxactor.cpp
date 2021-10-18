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
#include "volumebboxactor.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"

using namespace std;

namespace Met3D
{


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MVolumeBoundingBoxActor::MVolumeBoundingBoxActor()
    : MActor(),
      MBoundingBoxInterface(this, MBoundingBoxConnectionType::VOLUME),
      coordinateVertexBuffer(nullptr),
      axisVertexBuffer(nullptr),
      tickLength(0.8),
      lineColour(QColor(0, 104, 139, 255))
{
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setActorType(staticActorType());
    setName(getActorType());

    // Bounding box of the actor.
    insertBoundingBoxProperty(actorPropertiesSupGroup);

    tickLengthProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                     "tick length",
                                     actorPropertiesSupGroup);
    properties->setDDouble(tickLengthProperty, tickLength,
                           0.05, 20., 2, 0.05, " (world space)");

    colourProperty = addProperty(COLOR_PROPERTY, "colour",
                                 actorPropertiesSupGroup);
    properties->mColor()->setValue(colourProperty, lineColour);

    endInitialiseQtProperties();
}


MVolumeBoundingBoxActor::~MVolumeBoundingBoxActor()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE 0

void MVolumeBoundingBoxActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs\n" << flush);
    geometryEffect->compileFromFile_Met3DHome("src/glsl/simple_coloured_geometry.fx.glsl");
}


void MVolumeBoundingBoxActor::setColour(QColor c)
{
    properties->mColor()->setValue(colourProperty, c);
}


void MVolumeBoundingBoxActor::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MVolumeBoundingBoxActor::getSettingsID());

    MBoundingBoxInterface::saveConfiguration(settings);

    settings->setValue("tickLength",
                       properties->mDDouble()->value(tickLengthProperty));

    settings->setValue("lineColour",
                       properties->mColor()->value(colourProperty));

    settings->endGroup();
}


void MVolumeBoundingBoxActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MVolumeBoundingBoxActor::getSettingsID());

    MBoundingBoxInterface::loadConfiguration(settings);

    properties->mDDouble()->setValue(
                tickLengthProperty,
                settings->value("tickLength").toDouble());

    properties->mColor()->setValue(
                colourProperty,
                settings->value("lineColour").value<QColor>());

    settings->endGroup();
}


void MVolumeBoundingBoxActor::onBoundingBoxChanged()
{
    labels.clear();
    if (suppressActorUpdates())
    {
        return;
    }
    // Switching to no bounding box only needs a redraw, but no recomputation
    // because it disables rendering of the actor.
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        emitActorChangedSignal();
        return;
    }
    generateGeometry();
    emitActorChangedSignal();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MVolumeBoundingBoxActor::initializeActorResources()
{
    generateGeometry();

    bool loadShaders = false;

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    loadShaders |= glRM->generateEffectProgram("volumebox_shader", geometryEffect);

    if (loadShaders) reloadShaderEffects();
}


void MVolumeBoundingBoxActor::onQtPropertyChanged(QtProperty *property)
{
    // The slice position has been changed.
    if ( (property == labelSizeProperty)
         || (property == labelColourProperty)
         || (property == labelBBoxProperty)
         || (property == labelBBoxColourProperty) )
    {
        if (suppressActorUpdates()) return;
        generateGeometry();
        emitActorChangedSignal();
    }

    else if (property == tickLengthProperty)
    {
        tickLength = properties->mDDouble()->value(tickLengthProperty);
        emitActorChangedSignal();
    }

    else if (property == colourProperty)
    {
        lineColour = properties->mColor()->value(colourProperty);
        emitActorChangedSignal();
    }
}


void MVolumeBoundingBoxActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }
    // A) Render volume box.
    // =====================

    // Bind shader program.
    geometryEffect->bindProgram("Pressure");

    // Set uniform and attribute values.
    geometryEffect->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
    geometryEffect->setUniformValue(
                "pToWorldZParams", sceneView->pressureToWorldZParameters());

    coordinateVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    geometryEffect->setUniformValue("colour", lineColour);
    glLineWidth(2);
    glDrawArrays(GL_LINE_STRIP, 0, coordinateSystemVertices.size());

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;


    // B) Render tick marks and adjust label positions.
    // ================================================

    // Bind shader program.
    geometryEffect->bindProgram("TickMarks");

    // Set uniform and attribute values.
    geometryEffect->setUniformValue(
                "pToWorldZParams", sceneView->pressureToWorldZParameters());
    geometryEffect->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
    geometryEffect->setUniformValue(
                "colour", lineColour);

    // Offset for the "other end" of the tick line and anchor offset for
    // the labels.
    QVector3D anchorOffset = tickLength * sceneView->getCamera()->getXAxis();

    geometryEffect->setUniformValue(
                "offsetDirection", anchorOffset);

    // Set label offset; the labels are rendered by the text manager.
    for (int i = 0; i < labels.size(); i++)
        labels[i]->anchorOffset = anchorOffset;

    axisVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
    glLineWidth(2); CHECK_GL_ERROR;
    glDrawArrays(GL_POINTS, 0, axisTicks.size()); CHECK_GL_ERROR;

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MVolumeBoundingBoxActor::generateGeometry()
{
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }
    // A) Generate geometry.
    // =====================

    // Coordinates of a unit cube that becomes the volume box.
    const unsigned int unitcubeLength = 16;
    float unitcubeCoordinates[unitcubeLength][3] = {{0, 0, 0},
                                                    {1, 0, 0},
                                                    {1, 1, 0},
                                                    {0, 1, 0},
                                                    {0, 0, 0},
                                                    {0, 0, 1},
                                                    {1, 0, 1},
                                                    {1, 0, 0},
                                                    {1, 0, 1},
                                                    {1, 1, 1},
                                                    {1, 1, 0},
                                                    {1, 1, 1},
                                                    {0, 1, 1},
                                                    {0, 1, 0},
                                                    {0, 1, 1},
                                                    {0, 0, 1}};

    float llcrnrlat = bBoxConnection->southLat();
    float llcrnrlon = bBoxConnection->westLon();
    float urcrnrlat = bBoxConnection->northLat();
    float urcrnrlon = bBoxConnection->eastLon();

    float pbot_hPa = bBoxConnection->bottomPressure_hPa();
    float ptop_hPa = bBoxConnection->topPressure_hPa();

    // Create coordinate system geometry.
    coordinateSystemVertices.clear();
    for (unsigned int i = 0; i < unitcubeLength; i++) {
        coordinateSystemVertices.append(
                    QVector3D(
                        llcrnrlon + unitcubeCoordinates[i][0]
                        * (urcrnrlon-llcrnrlon),

                        urcrnrlat - unitcubeCoordinates[i][1]
                        * (urcrnrlat-llcrnrlat),

                        (unitcubeCoordinates[i][2] == 0) ? pbot_hPa :  ptop_hPa
                        )
                    );
    }

    // Create axis ticks geometry.
    axisTicks.clear();
    int interval = pbot_hPa > 100. ? 100 : 10;
    int p = int(pbot_hPa / interval) * interval;

    while (p >= ptop_hPa)
    {
        axisTicks.append(QVector3D(llcrnrlon, llcrnrlat, p));
        axisTicks.append(QVector3D(llcrnrlon, urcrnrlat, p));
        axisTicks.append(QVector3D(urcrnrlon, urcrnrlat, p));
        axisTicks.append(QVector3D(urcrnrlon, llcrnrlat, p));

        p -= (p > 100) ? 100 : 10;
    }


    // B) Upload geometry data to VBO.
    // ===============================

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    glRM->makeCurrent();

    const QString coordinateRequestKey = "coords_vertices_actor#"
                                    + QString::number(getID());
    uploadVec3ToVertexBuffer(coordinateSystemVertices, coordinateRequestKey,
                             &coordinateVertexBuffer);

    const QString axisRequestKey = "axis_vertices_actor#"
                                    + QString::number(getID());
    uploadVec3ToVertexBuffer(axisTicks, axisRequestKey, &axisVertexBuffer);

#ifdef USE_QOPENGLWIDGET
    glRM->doneCurrent();
#endif


    // C) Generate labels.
    // ===================

    // Remove all text labels of the old geometry (MActor method).
    removeAllLabels();
    MTextManager* tm = glRM->getTextManager();

    // Get properties for label font size and colour and bounding box.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);

    int drawLabel = 0;
    for (int i = 0; i < axisTicks.size(); i++)
    {
        // Label only every 3rd tick mark (4 labelled axis of the volume box,
        // hence 4 ticks for each pressure.
        if (drawLabel++ < 0) continue;
        if (drawLabel == 4)  drawLabel = -8;

        QVector3D tickPosition = axisTicks.at(i);
        labels.append(tm->addText(
                          QString("%1").arg(tickPosition.z()),
                          MTextManager::LONLATP,
                          tickPosition.x(), tickPosition.y(), tickPosition.z(),
                          labelsize, labelColour, MTextManager::MIDDLELEFT,
                          labelbbox, labelBBoxColour)
                      );
    }
    glRM = MGLResourcesManager::getInstance();
    glRM->makeCurrent();
}

} // namespace Met3D
