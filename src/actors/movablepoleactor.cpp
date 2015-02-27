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
#include "movablepoleactor.h"

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

MMovablePoleActor::MMovablePoleActor()
    : MActor(),
      pbot_hPa(1050.),
      ptop_hPa(100.),
      tickLength(0.8),
      lineColour(QColor(0, 104, 139, 255)),
      highlightPole(-1)
{
    enablePicking(true);

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Movable poles");

    bottomPressureProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                         "bottom pressure",
                                         actorPropertiesSupGroup);
    properties->setDDouble(bottomPressureProperty, pbot_hPa,
                           1050., 20., 1, 10., " hPa");

    topPressureProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                      "top pressure",
                                      actorPropertiesSupGroup);
    properties->setDDouble(topPressureProperty, ptop_hPa,
                           1050., 20., 1, 10., " hPa");

    tickLengthProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                     "tick length",
                                     actorPropertiesSupGroup);
    properties->setDDouble(tickLengthProperty, tickLength,
                           0.05, 20., 2, 0.05, " (world space)");

    colourProperty = addProperty(COLOR_PROPERTY, "colour",
                                 actorPropertiesSupGroup);
    properties->mColor()->setValue(colourProperty, lineColour);

    addPoleProperty = addProperty(CLICK_PROPERTY, "add pole",
                                  actorPropertiesSupGroup);

    endInitialiseQtProperties();
}


MMovablePoleActor::~MMovablePoleActor()
{
}


MMovablePoleActor::PoleSettings::PoleSettings(MActor *actor)
{
    if (actor == nullptr) return;

    groupProperty = actor->addProperty(GROUP_PROPERTY, "pole");

    positionProperty = actor->addProperty(POINTF_PROPERTY, "position",
                                          groupProperty);
    MQtProperties *properties = actor->getQtProperties();
    properties->mPointF()->setValue(positionProperty, QPointF());

    removePoleProperty = actor->addProperty(CLICK_PROPERTY, "remove",
                                            groupProperty);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE 0

void MMovablePoleActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);
    simpleGeometryEffect->compileFromFile("src/glsl/simple_coloured_geometry.fx.glsl");
    positionSpheresShader->compileFromFile("src/glsl/trajectory_positions.fx.glsl");
}


int MMovablePoleActor::checkIntersectionWithHandle(MSceneViewGLWidget *sceneView,
                                         float clipX, float clipY,
                                         float clipRadius)
{
    // MLOG << "checkIntersection(" << clipX << ", " << clipY << " / "
    //      << clipRadius << ")\n" << flush;

    // Default: No pole has been touched by the mouse. Note: This instance
    // variable is used in renderToCurrentContext; if it is >= 0 the pole
    // with the corresponding index is highlighted.
    highlightPole = -1;

    float clipRadiusSq = clipRadius*clipRadius;

    // Loop over all poles and check whether the mouse cursor is inside a
    // circle with radius "clipRadius" around the bottom pole point (in clip
    // space).
    for (int i = 0; i < poleVertices.size(); i+=2)
    {
        // Transform the bottom pole coordinate to clip space.
        QVector3D pClip = sceneView->lonLatPToClipSpace(poleVertices.at(i));

        // cout << "\tWP " << i << ": (" << pClip.x() << ", " << pClip.y()
        //      << ", " << pClip.z() << ")\n" << flush;

        float dx = pClip.x() - clipX;
        float dy = pClip.y() - clipY;

        // Compute the distance between point and mouse in clip space. If it
        // is less than clipRadius, store this waypoint as the "matched" one
        // and return from this function.
        if ( (dx*dx + dy*dy) < clipRadiusSq )
        {
            // cout << "Match with WP" << i << "\n" << flush;
            highlightPole = i;
            break;
        }
    } // for (poles)

    return highlightPole;
}


void MMovablePoleActor::dragEvent(MSceneViewGLWidget *sceneView,
                                  int handleID, float clipX, float clipY)
{
    // Select an arbitrary z-value to construct a point in clip space that,
    // transformed to world space, lies on the ray passing through the camera
    // and the location on the worldZ==0 plane "picked" by the mouse.
    // (See notes 22-23Feb2012).
    QVector3D mousePosClipSpace = QVector3D(clipX, clipY, 0.);

    // The point p at which the ray intersects the worldZ==0 plane is found by
    // computing the value d in p=d*l+l0, where l0 is a point on the ray and l
    // is a vector in the direction of the ray. d can be found with
    //        (p0 - l0) * n
    //   d = ----------------
    //            l * n
    // where p0 is a point on the worldZ==p2worldZ(pbot) plane and n is the
    // normal vector of the plane.
    //       http://en.wikipedia.org/wiki/Line-plane_intersection

    // To compute l0, the MVP matrix has to be inverted.
    QMatrix4x4 *mvpMatrix = sceneView->getModelViewProjectionMatrix();
    QVector3D l0 = mvpMatrix->inverted() * mousePosClipSpace;

    // Compute l as the vector from l0 to the camera origin.
    QVector3D cameraPosWorldSpace = sceneView->getCamera()->getOrigin();
    QVector3D l = (l0 - cameraPosWorldSpace);

    // The plane's normal vector simply points upward, the origin in world
    // space is lcoated on the plane.
    QVector3D n = QVector3D(0, 0, 1);
    QVector3D p0 = QVector3D(0, 0, sceneView->worldZfromPressure(
                                 poleVertices[handleID].z()));

    // Compute the mouse position in world space.
    float d = QVector3D::dotProduct(p0 - l0, n) / QVector3D::dotProduct(l, n);
    QVector3D mousePosWorldSpace = l0 + d * l;

    // DEBUG output.
//    MLOG << "dragging handle " << handleID << endl << flush;
//    cout << "\tmouse position clip space = (" << mousePosClipSpace.x() << "," << mousePosClipSpace.y() << "," << mousePosClipSpace.z() << ")\n" << flush;
//    cout << "\tl0 = (" << l0.x() << "," << l0.y() << "," << l0.z() << ")\n" << flush;
//    QVector3D l0Clip = *(mvpMatrix) * l0;
//    cout << "\tcheck: l0, transformed back to clip space = (" << l0Clip.x() << "," << l0Clip.y() << "," << l0Clip.z() << ")\n" << flush;
//    cout << "\tl = (" << l.x() << "," << l.y() << "," << l.z() << ")\n" << flush;
//    cout << "\tmouse position, world space, on worldZ==0 plane = (" << mousePosWorldSpace.x() << "," << mousePosWorldSpace.y()
//         << "," << mousePosWorldSpace.z()
//         << "); d = " << d << "\n" << flush;

    // Update the coordinates of pole, axis tick marks and labels. Upload new
    // positions to vertex buffers and redraw the scene.

    poleVertices[handleID  ].setX(mousePosWorldSpace.x());
    poleVertices[handleID  ].setY(mousePosWorldSpace.y());
    poleVertices[handleID+1].setX(mousePosWorldSpace.x());
    poleVertices[handleID+1].setY(mousePosWorldSpace.y());

    // Update tick mark positions.
    int numPoles = poleVertices.size() / 2; // two vertices per pole
    int pole = handleID / 2;
    for (int i = pole; i < axisTicks.size(); i+=numPoles)
    {
        axisTicks[i].setX(mousePosWorldSpace.x());
        axisTicks[i].setY(mousePosWorldSpace.y());
    }

    const QString poleRequestKey = "pole_vertices_actor#"
                                    + QString::number(getID());
    uploadVec3ToVertexBuffer(poleVertices, poleRequestKey, &poleVertexBuffer, sceneView);

    const QString axisRequestKey = "axis_vertices_actor#"
                                    + QString::number(getID());
    uploadVec3ToVertexBuffer(axisTicks, axisRequestKey, &axisVertexBuffer, sceneView);

    // Update label positions.
    for (int i = pole; i < labels.size(); i+=numPoles)
    {
        labels[i]->anchor.setX(mousePosWorldSpace.x());
        labels[i]->anchor.setY(mousePosWorldSpace.y());
    }

    // Update GUI properties.
    properties->mPointF()->setValue(
                poleProperties[pole].positionProperty,
                QPointF(mousePosWorldSpace.x(), mousePosWorldSpace.y()));

    emitActorChangedSignal();
}


void MMovablePoleActor::addPole(QPointF pos)
{
    PoleSettings poleSettings(this);
    properties->mPointF()->setValue(poleSettings.positionProperty, pos);
    poleProperties.append(poleSettings);
    actorPropertiesSupGroup->addSubProperty(poleSettings.groupProperty);

    if (isInitialized())
    {
        generateGeometry();
        emitActorChangedSignal();
    }
}


void MMovablePoleActor::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MMovablePoleActor::getSettingsID());

    settings->setValue("bottomPressure",
                       properties->mDDouble()->value(bottomPressureProperty));

    settings->setValue("topPressure",
                       properties->mDDouble()->value(topPressureProperty));

    settings->setValue("tickLength",
                       properties->mDDouble()->value(tickLengthProperty));

    settings->setValue("lineColour",
                       properties->mColor()->value(colourProperty));

    settings->setValue("numPoles", poleProperties.size());

    for (int i = 0; i < poleProperties.size(); i++)
    {
        const PoleSettings& ps = poleProperties.at(i);
        QString poleID = QString("polePosition_%1").arg(i);
        settings->setValue(
                    poleID, properties->mPointF()->value(ps.positionProperty));
    }

    settings->endGroup();
}


void MMovablePoleActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MMovablePoleActor::getSettingsID());

    properties->mDDouble()->setValue(
                bottomPressureProperty,
                settings->value("bottomPressure").toDouble());

    properties->mDDouble()->setValue(
                topPressureProperty,
                settings->value("topPressure").toDouble());

    properties->mDDouble()->setValue(
                tickLengthProperty,
                settings->value("tickLength").toDouble());

    properties->mColor()->setValue(
                colourProperty,
                settings->value("lineColour").value<QColor>());

    int numPoles = settings->value("numPoles").toInt();

    // Clear current poles.
    foreach(const PoleSettings& ps, poleProperties)
        actorPropertiesSupGroup->removeSubProperty(ps.groupProperty);
    poleProperties.clear();

    // Read saved poles.
    for (int i = 0; i < numPoles; i++)
    {
        QString poleID = QString("polePosition_%1").arg(i);
        QPointF pos = settings->value(poleID).toPointF();
        PoleSettings poleSettings(this);
        properties->mPointF()->setValue(poleSettings.positionProperty, pos);
        poleProperties.append(poleSettings);
        actorPropertiesSupGroup->addSubProperty(poleSettings.groupProperty);
    }

    settings->endGroup();

    if (isInitialized()) generateGeometry();
}



/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MMovablePoleActor::onQtPropertyChanged(QtProperty *property)
{
    // The slice position has been changed.
    if ( property == bottomPressureProperty
         || (property == topPressureProperty)
         || (property == labelSizeProperty)
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

    else if (property == addPoleProperty)
    {
        PoleSettings poleSettings(this);
        poleProperties.append(poleSettings);
        actorPropertiesSupGroup->addSubProperty(poleSettings.groupProperty);

        if (suppressActorUpdates()) return;
        generateGeometry();
        emitActorChangedSignal();
    }

    else
    {
        for (int i = 0; i < poleProperties.size(); i++)
        {
            const PoleSettings& ps = poleProperties.at(i);

            if (property == ps.positionProperty)
            {
                if (suppressActorUpdates()) return;
                generateGeometry();
                emitActorChangedSignal();
                break;
            }

            else if (property == ps.removePoleProperty)
            {
                actorPropertiesSupGroup->removeSubProperty(ps.groupProperty);
                poleProperties.remove(i);
                if (suppressActorUpdates()) return;
                generateGeometry();
                emitActorChangedSignal();
                break;
            }
        }
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MMovablePoleActor::initializeActorResources()
{
    generateGeometry();

    bool loadShaders = false;
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    loadShaders |= glRM->generateEffectProgram("ppole_geometry",
                                               simpleGeometryEffect);
    loadShaders |= glRM->generateEffectProgram("ppole_spheres",
                                               positionSpheresShader);

    if (loadShaders) reloadShaderEffects();
}


void MMovablePoleActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    // A) Render vertical axes.
    // ========================

    // Bind shader program.
    simpleGeometryEffect->bindProgram("Pressure");

    // Set uniform and attribute values.
    simpleGeometryEffect->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
    simpleGeometryEffect->setUniformValue(
                "pToWorldZParams", sceneView->pressureToWorldZParameters());

    poleVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
    simpleGeometryEffect->setUniformValue("colour", lineColour);
    glLineWidth(2); CHECK_GL_ERROR;
    glDrawArrays(GL_LINES, 0, poleVertices.size()); CHECK_GL_ERROR;

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;


    // B) Render tick marks and adjust label positions.
    // ================================================

    // Bind shader program.
    simpleGeometryEffect->bindProgram("TickMarks");

    // Set uniform and attribute values.
    simpleGeometryEffect->setUniformValue(
                "pToWorldZParams", sceneView->pressureToWorldZParameters());
    simpleGeometryEffect->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
    simpleGeometryEffect->setUniformValue(
                "colour", lineColour);

    // Offset for the "other end" of the tick line and anchor offset for
    // the labels.
    QVector3D anchorOffset = tickLength * sceneView->getCamera()->getXAxis();

    simpleGeometryEffect->setUniformValue(
                "offsetDirection", anchorOffset);

    // Set label offset; the labels are rendered by the text manager.
    for (int i = 0; i < labels.size(); i++)
        labels[i]->anchorOffset = anchorOffset;

    // Render tick marks.

    axisVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
    glLineWidth(2); CHECK_GL_ERROR;
    glDrawArrays(GL_POINTS, 0, axisTicks.size()); CHECK_GL_ERROR;

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;


    // C) Highlight pole if one is currently dragged.
    // ================================================

    // If the index "highlightPole" is < 0, no waypoint should be highlighted.
    if (sceneView->interactionModeEnabled())
    {

        // Bind shader program.
        positionSpheresShader->bind();

        // Set MVP-matrix and parameters to map pressure to world space in the
        // vertex shader.
        positionSpheresShader->setUniformValue(
                    "mvpMatrix",
                    *(sceneView->getModelViewProjectionMatrix()));
        positionSpheresShader->setUniformValue(
                    "pToWorldZParams",
                    sceneView->pressureToWorldZParameters());
        positionSpheresShader->setUniformValue(
                    "lightDirection",
                    sceneView->getLightDirection());
        positionSpheresShader->setUniformValue(
                    "cameraPosition",
                    sceneView->getCamera()->getOrigin());
        positionSpheresShader->setUniformValue(
                    "cameraUpDir",
                    sceneView->getCamera()->getYAxis());
        positionSpheresShader->setUniformValue(
                    "radius",
                    GLfloat(0.5));
        positionSpheresShader->setUniformValue(
                    "scaleRadius",
                    GLboolean(true));


        // Texture bindings for transfer function for data scalar (1D texture from
        // transfer function class). The data scalar is stored in the vertex.w
        // component passed to the vertex shader.
        positionSpheresShader->setUniformValue(
                    "useTransferFunction", GLboolean(false));
        positionSpheresShader->setUniformValue(
                    "constColour", QColor(Qt::white));

        // Bind vertex buffer object.

        poleVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

        glPolygonMode(GL_FRONT_AND_BACK,
                      renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
        glLineWidth(1); CHECK_GL_ERROR;

        glDrawArrays(GL_POINTS, 0, poleVertices.size()); CHECK_GL_ERROR;

        if (highlightPole >= 0)
        {
            positionSpheresShader->setUniformValue(
                        "radius",
                        GLfloat(0.51));
            positionSpheresShader->setUniformValue(
                        "constColour", QColor(Qt::red));
            glDrawArrays(GL_POINTS, highlightPole, 2); CHECK_GL_ERROR;
        }

        // Unbind VBO.
        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MMovablePoleActor::generateGeometry()
{
    // A) Update/generate geometry.
    // =====================

    poleVertices.clear();
    foreach(const PoleSettings& ps, poleProperties)
    {
        QPointF pos = properties->mPointF()->value(ps.positionProperty);
        poleVertices.append(QVector3D(pos)); // for pbot
        poleVertices.append(QVector3D(pos)); // for ptop
    }

    pbot_hPa = properties->mDDouble()->value(bottomPressureProperty);
    ptop_hPa = properties->mDDouble()->value(topPressureProperty);

    // Update pressure limits of the poles.
    for (int i = 0; i < poleVertices.size(); i+=2)
    {
        poleVertices[i  ].setZ(pbot_hPa);
        poleVertices[i+1].setZ(ptop_hPa);
    }

    // Create new axis ticks geometry, according to the pressure limits.
    axisTicks.clear();
    int interval = pbot_hPa > 100. ? 100 : 10;
    int p = int(pbot_hPa / interval) * interval;

    while (p >= ptop_hPa)
    {
        for (int i = 0; i < poleVertices.size(); i+=2) // two vertices per pole
        {
            QVector3D pos = poleVertices.at(i);
            axisTicks.append(QVector3D(pos.x(), pos.y(), p));
        }
        p -= (p > 100) ? 100 : 10;
    }


    // B) Upload geometry data to VBO.
    // ===============================

    const QString poleRequestKey = "pole_vertices_actor#"
                                    + QString::number(getID());
    uploadVec3ToVertexBuffer(poleVertices, poleRequestKey, &poleVertexBuffer);

    const QString axisRequestKey = "axis_vertices_actor#"
                                    + QString::number(getID());
    uploadVec3ToVertexBuffer(axisTicks, axisRequestKey, &axisVertexBuffer);

    // (MActor method).


    // C) Generate labels.
    // ===================

    // Remove all text labels of the old geometry (MActor method).
    removeAllLabels();
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    MTextManager* tm = glRM->getTextManager();

    // Get properties for label font size and colour and bounding box.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);

    int drawLabel = 0;
    for (int i = 0; i < axisTicks.size(); i++)
    {
        // Label only every 3rd tick mark (numPoles labelled axes, hence
        // numPoles ticks for each pressure).
        if (drawLabel++ < 0) continue;
        int numPoles = poleVertices.size() / 2; // two vertices per pole
        if (drawLabel == numPoles) drawLabel = -2 * numPoles;

        QVector3D tickPosition = axisTicks.at(i);
        labels.append(tm->addText(
                          QString("%1").arg(tickPosition.z()),
                          MTextManager::LONLATP,
                          tickPosition.x(), tickPosition.y(), tickPosition.z(),
                          labelsize, labelColour, MTextManager::MIDDLELEFT,
                          labelbbox, labelBBoxColour)
                      );
    }
}

} // namespace Met3D
