/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2016-2017 Bianca Tost
**  Copyright 2017      Michael Kern
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

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"

using namespace std;
using namespace Met3D;

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MMovablePoleActor::MMovablePoleActor()
    : MActor(),
      tickLength(0.8),
      lineColour(QColor(0, 104, 139, 255)),
      bottomPressure_hPa(1050.f),
      topPressure_hPa(100.f),
      renderMode(RenderModes::TUBES),
      tubeRadius(0.06f),
      individualPoleHeightsEnabled(false),
      movementEnabled(true),
      highlightPole(-1),
      offsetPickPositionToHandleCentre(QVector2D(0., 0.))
{
    enablePicking(true);

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setActorType("Movable poles");
    setName(getActorType());

    colourProperty = addProperty(COLOR_PROPERTY, "colour",
                                 actorPropertiesSupGroup);
    properties->mColor()->setValue(colourProperty, lineColour);

    renderModeProperty = addProperty(ENUM_PROPERTY, "render mode",
                                     actorPropertiesSupGroup);
    QStringList modes;
    modes << "Tubes" << "Lines";
    properties->mEnum()->setEnumNames(renderModeProperty, modes);
    properties->mEnum()->setValue(renderModeProperty, static_cast<int>(renderMode));

    tubeRadiusProperty = addProperty(DOUBLE_PROPERTY, "tube radius",
                                     actorPropertiesSupGroup);
    properties->setDouble(tubeRadiusProperty, tubeRadius, 0.01, 5, 2, 0.01);

    individualPoleHeightsProperty = addProperty(
                BOOL_PROPERTY, "specify height per pole",
                actorPropertiesSupGroup);
    properties->mBool()->setValue(individualPoleHeightsProperty,
                                  individualPoleHeightsEnabled);

    bottomPressureProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                         "bottom pressure",
                                         actorPropertiesSupGroup);
    properties->setDDouble(bottomPressureProperty, bottomPressure_hPa,
                           1050., 20., 1, 10., " hPa");

    topPressureProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                      "top pressure",
                                      actorPropertiesSupGroup);
    properties->setDDouble(topPressureProperty, topPressure_hPa,
                           1050., 20., 1, 10., " hPa");

    ticksGroupProperty = addProperty(GROUP_PROPERTY, "tick marks",
                                     actorPropertiesSupGroup);

    tickLengthProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                     "tick length",
                                     ticksGroupProperty);
    properties->setDDouble(tickLengthProperty, tickLength,
                           0.05, 20., 2, 0.05, " (world space)");

    tickPressureThresholdProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                                "tick interval threshold",
                                                ticksGroupProperty);
    properties->setDDouble(tickPressureThresholdProperty, 100.,
                           1050., 20., 1, 10., " hPa");

    tickIntervalAboveThreshold = addProperty(
                DOUBLE_PROPERTY, "tick interval above threshold",
                ticksGroupProperty);
    properties->setDouble(tickIntervalAboveThreshold, 100.,
                          10., 300., 1, 10.);

    tickIntervalBelowThreshold = addProperty(
                DOUBLE_PROPERTY, "tick interval below threshold",
                ticksGroupProperty);
    properties->setDouble(tickIntervalBelowThreshold, 10.,
                          10., 300., 1, 10.);

    labelSpacingProperty = addProperty(INT_PROPERTY, "label spacing",
                                       ticksGroupProperty);
    properties->setInt(labelSpacingProperty, 3, 1, 100, 1);

    addPoleProperty = addProperty(CLICK_PROPERTY, "add pole",
                                  actorPropertiesSupGroup);

    endInitialiseQtProperties();
}


MMovablePoleActor::~MMovablePoleActor()
{
}


MovablePole::MovablePole(MActor *actor)
{
    if (actor == nullptr) return;

    groupProperty = actor->addProperty(GROUP_PROPERTY, "pole");

    positionProperty = actor->addProperty(POINTF_LONLAT_PROPERTY, "position",
                                          groupProperty);
    MQtProperties *properties = actor->getQtProperties();
    properties->mPointF()->setValue(positionProperty, QPointF());

    bottomPressureProperty = actor->addProperty(DECORATEDDOUBLE_PROPERTY,
                                                "bottom pressure", groupProperty);
    properties->setDDouble(bottomPressureProperty, 1050.,
                           1050., 20., 1, 10., " hPa");

    topPressureProperty = actor->addProperty(DECORATEDDOUBLE_PROPERTY,
                                             "top pressure", groupProperty);
    properties->setDDouble(topPressureProperty, 100,
                           1050., 20., 1, 10., " hPa");

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

    beginCompileShaders(2);

    compileShadersFromFileWithProgressDialog(
                simpleGeometryEffect,
                "src/glsl/simple_geometry_generation.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                positionSpheresShader,
                "src/glsl/trajectory_positions.fx.glsl");

    endCompileShaders();
}


void MMovablePoleActor::removeAllPoles()
{
    foreach (const std::shared_ptr<MovablePole> pole, poles)
        actorPropertiesSupGroup->removeSubProperty(pole->groupProperty);

    poles.clear();

    if (isInitialized())
    {
        generateGeometry();
        emitActorChangedSignal();
    }
}


void MMovablePoleActor::addPole(QPointF pos)
{
    std::shared_ptr<MovablePole> pole = std::make_shared<MovablePole>(this);
    properties->mPointF()->setValue(pole->positionProperty, pos);
    poles.append(pole);
    actorPropertiesSupGroup->addSubProperty(pole->groupProperty);

    if (isInitialized())
    {
        generateGeometry();
        emitActorChangedSignal();
    }
}


void MMovablePoleActor::addPole(const QVector3D& lonlatP)
{
    std::shared_ptr<MovablePole> pole = std::make_shared<MovablePole>(this);
    properties->mPointF()->setValue(pole->positionProperty, lonlatP.toPointF());
    properties->mDDouble()->setValue(pole->topPressureProperty, lonlatP.z());
    properties->mDDouble()->setValue(pole->bottomPressureProperty, 1050.0);
    poles.append(pole);
    actorPropertiesSupGroup->addSubProperty(pole->groupProperty);

    if (isInitialized())
    {
        generateGeometry();
        emitActorChangedSignal();
    }
}


void MMovablePoleActor::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MMovablePoleActor::getSettingsID());

    settings->setValue("tickLength",
                       properties->mDDouble()->value(tickLengthProperty));

    settings->setValue("lineColour",
                       properties->mColor()->value(colourProperty));

    settings->setValue("renderMode", static_cast<int>(renderMode));

    settings->setValue("tubeRadius", tubeRadius);

    settings->setValue("individualPoleHeightsEnabled", individualPoleHeightsEnabled);

    settings->setValue("bottomPressure",
                       properties->mDDouble()->value(bottomPressureProperty));

    settings->setValue("topPressure",
                       properties->mDDouble()->value(topPressureProperty));

    settings->setValue("numPoles", poles.size());

    settings->setValue("tickIntervalAboveThreshold",
                       properties->mDouble()->value(tickIntervalAboveThreshold));

    settings->setValue("tickIntervalBelowThreshold",
                       properties->mDouble()->value(tickIntervalBelowThreshold));

    settings->setValue("tickIntervalThreshold",
                       properties->mDDouble()->value(tickPressureThresholdProperty));

    settings->setValue("labelSpacing",
                       properties->mInt()->value(labelSpacingProperty));

    for (int i = 0; i < poles.size(); i++)
    {
        std::shared_ptr<MovablePole> pole = poles.at(i);

        QString poleID = QString("polePosition_%1").arg(i);

        settings->setValue(poleID,
                           properties->mPointF()->value(pole->positionProperty));
        settings->setValue(QString("poleBottomPressure_%1").arg(i),
                           properties->mDDouble()->value(pole->topPressureProperty));
        settings->setValue(QString("poleTopPressure_%1").arg(i),
                           properties->mDDouble()->value(pole->bottomPressureProperty));
    }

    settings->endGroup();
}


void MMovablePoleActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MMovablePoleActor::getSettingsID());

    properties->mDDouble()->setValue(tickLengthProperty,
                settings->value("tickLength", 0.8).toDouble());

    properties->mColor()->setValue(
                colourProperty,
                settings->value("lineColour", QColor(Qt::black)).value<QColor>());

    properties->mEnum()->setValue(renderModeProperty,
                settings->value("renderMode", int(RenderModes::TUBES)).toInt());

    properties->mDouble()->setValue(tubeRadiusProperty,
                settings->value("tubeRadius", 0.1).toDouble());

    individualPoleHeightsEnabled = settings->value("individualPoleHeightsEnabled").toBool();
    properties->mBool()->setValue(individualPoleHeightsProperty, individualPoleHeightsEnabled);

    properties->mDDouble()->setValue(
                bottomPressureProperty,
                settings->value("bottomPressure", 1050.).toDouble());

    properties->mDDouble()->setValue(
                topPressureProperty,
                settings->value("topPressure", 100.).toDouble());

    properties->mDouble()->setValue(
                tickIntervalAboveThreshold,
                settings->value("tickIntervalAboveThreshold", 100.).toDouble());

    properties->mDouble()->setValue(
                tickIntervalBelowThreshold,
                settings->value("tickIntervalBelowThreshold", 10.).toDouble());

    properties->mDDouble()->setValue(
                tickPressureThresholdProperty,
                settings->value("tickIntervalThreshold", 100.).toDouble());

    properties->mInt()->setValue(
                labelSpacingProperty,
                settings->value("labelSpacing", 3).toInt());

    int numPoles = settings->value("numPoles", 0).toInt();

    // Clear current poles.
    foreach (const std::shared_ptr<MovablePole> ps, poles)
    {
        actorPropertiesSupGroup->removeSubProperty(ps->groupProperty);
    }
    poles.clear();

    // Read saved poles.
    for (int i = 0; i < numPoles; i++)
    {
        QString poleID = QString("polePosition_%1").arg(i);
        QPointF pos = settings->value(poleID).toPointF();
        float pBot = settings->value(QString("poleBottomPressure_%1").arg(i), 1050.).toFloat();
        float pTop= settings->value(QString("poleTopPressure_%1").arg(i), 100.).toFloat();

        std::shared_ptr<MovablePole> pole = std::make_shared<MovablePole>(this);

        properties->mPointF()->setValue(pole->positionProperty, pos);
        pole->bottomPressureProperty->setEnabled(individualPoleHeightsEnabled);
        pole->topPressureProperty->setEnabled(individualPoleHeightsEnabled);

        properties->mDouble()->setValue(pole->bottomPressureProperty, pBot);
        properties->mDouble()->setValue(pole->bottomPressureProperty, pTop);
        poles.append(pole);
        actorPropertiesSupGroup->addSubProperty(pole->groupProperty);
    }

    settings->endGroup();

    if (isInitialized()) generateGeometry();
}


const QVector<QVector3D>& MMovablePoleActor::getPoleVertices() const
{
    return poleVertices;
}


void MMovablePoleActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    // A) Render vertical axes.
    // ========================

    // Bind shader program.
    switch (renderMode)
    {
        case RenderModes::LINES:
            tubeRadius = 0;
            simpleGeometryEffect->bindProgram("LonLatPLines");
            break;
        case RenderModes::TUBES:
            simpleGeometryEffect->bindProgram("LonLatPTubes");
            break;
    }

    const float tubeRadiusTick = tubeRadius * 0.5f;

    // Set uniform and attribute values.
    simpleGeometryEffect->setUniformValue(
            "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
    simpleGeometryEffect->setUniformValue(
            "pToWorldZParams", sceneView->pressureToWorldZParameters());
    simpleGeometryEffect->setUniformValue("tubeRadius", tubeRadius);
    simpleGeometryEffect->setUniformValue(
            "lightDirection", sceneView->getLightDirection());
    simpleGeometryEffect->setUniformValue(
            "cameraPosition", sceneView->getCamera()->getOrigin());
    simpleGeometryEffect->setUniformValue("endSegmentOffset", tubeRadius);
    simpleGeometryEffect->setUniformValue("geometryColor", lineColour);

    poleVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glLineWidth(2);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); CHECK_GL_ERROR;
    glDrawArrays(GL_LINES, 0, poleVertices.size()); CHECK_GL_ERROR;

    // B) Render tick marks and adjust label positions.
    // ================================================

    // Bind shader program.
    switch (renderMode)
    {
        case RenderModes::LINES:
            simpleGeometryEffect->bindProgram("TickLines");
            break;
        case RenderModes::TUBES:
            simpleGeometryEffect->bindProgram("TickTubes");
            break;
    }

    // Set uniform and attribute values.
    simpleGeometryEffect->setUniformValue(
            "pToWorldZParams", sceneView->pressureToWorldZParameters());
    simpleGeometryEffect->setUniformValue(
            "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));
    simpleGeometryEffect->setUniformValue("geometryColor", lineColour);
    simpleGeometryEffect->setUniformValue(
            "pToWorldZParams", sceneView->pressureToWorldZParameters());
    simpleGeometryEffect->setUniformValue("tubeRadius", tubeRadiusTick);
    simpleGeometryEffect->setUniformValue(
            "lightDirection", sceneView->getLightDirection());
    simpleGeometryEffect->setUniformValue(
            "cameraPosition", sceneView->getCamera()->getOrigin());
    simpleGeometryEffect->setUniformValue("endSegmentOffset", 0.1f);


    // Offset for the "other end" of the tick line and anchor offset for
    // the labels.
    QVector3D anchorOffset = tickLength * sceneView->getCamera()->getXAxis();

    simpleGeometryEffect->setUniformValue(
            "offsetDirection", anchorOffset);

    // Set label offset; the labels are rendered by the text manager.
    for (int i = 0; i < labels.size(); i++)
    {
        labels[i]->anchorOffset = anchorOffset
                + tubeRadius * sceneView->getCamera()->getXAxis();
    }

    // Render tick marks.

    axisVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glLineWidth(2);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); CHECK_GL_ERROR;
    glDrawArrays(GL_POINTS, 0, axisTicks.size()); CHECK_GL_ERROR;

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

    // C) Highlight pole if one is currently dragged.
    // ================================================

    // If the index "highlightPole" is < 0, no waypoint should be highlighted.
    if (sceneView->interactionModeEnabled() && movementEnabled)
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
                "radius", GLfloat(MSystemManagerAndControl::getInstance()
                                  ->getHandleSize()));
        positionSpheresShader->setUniformValue(
                "scaleRadius",
                GLboolean(true));


        // Texture bindings for transfer function for data scalar (1D texture from
        // transfer function class). The data scalar is stored in the vertex.w
        // component passed to the vertex shader.
        positionSpheresShader->setUniformValue(
                "useTransferFunction", GLboolean(false));

        // Bind vertex buffer object.

        poleVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

        glPolygonMode(GL_FRONT_AND_BACK,
                      renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
        glLineWidth(1); CHECK_GL_ERROR;

        if (highlightPole >= 0)
        {
            positionSpheresShader->setUniformValue(
                        "constColour", QColor(Qt::red));
            glDrawArrays(GL_POINTS, highlightPole, 1); CHECK_GL_ERROR;
        }

        positionSpheresShader->setUniformValue(
                "constColour", QColor(Qt::white));
        glDrawArrays(GL_POINTS, 0, poleVertices.size()); CHECK_GL_ERROR;


        // Unbind VBO.
        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    }
}


int MMovablePoleActor::checkIntersectionWithHandle(MSceneViewGLWidget *sceneView,
                                                   float clipX, float clipY)
{
    if (!movementEnabled)
    {
        return -1;
    }

    // Default: No pole has been touched by the mouse. Note: This instance
    // variable is used in renderToCurrentContext; if it is >= 0 the pole
    // with the corresponding index is highlighted.
    highlightPole = -1;

    //! TODO: HACK for pole actor. Should be changed for all movable actors!
    float clipRadius = MSystemManagerAndControl::getInstance()->getHandleSize();

    // Loop over all poles and check whether the mouse cursor is inside a
    // circle with radius "clipRadius" around the bottom pole point (in clip
    // space).
    for (int i = 0; i < poleVertices.size(); i++)
    {
        // Compute the world position of the current pole
        QVector3D posPole = poleVertices.at(i);
        posPole.setZ(sceneView->worldZfromPressure(posPole.z()));

        // Obtain the camera position and the view direction
        const QVector3D& cameraPos = sceneView->getCamera()->getOrigin();
        QVector3D viewDir = posPole - cameraPos;

        // Scale the radius (in world space) with respect to the viewer distance
        float radius = static_cast<float>(clipRadius * viewDir.length() / 100.0);

        QMatrix4x4 *mvpMatrix = sceneView->getModelViewProjectionMatrix();
        // Compute the world position of the current mouse position
        QVector3D mouseWorldPos = mvpMatrix->inverted() * QVector3D(clipX, clipY, 1);

        // Get the ray direction from the camera to the mouse position
        QVector3D l = mouseWorldPos - cameraPos;
        l.normalize();

        // Compute (o - c) // ray origin (o) - sphere center (c)
        QVector3D oc = cameraPos - posPole;
        // Length of (o - c) = || o - c ||
        float lenOC = static_cast<float>(oc.length());
        // Compute l * (o - c)
        float loc = static_cast<float>(QVector3D::dotProduct(l, oc));

        // Solve equation:
        // d = - (l * (o - c) +- sqrt( (l * (o - c))² - || o - c ||² + r² )
        // Since the equation can be solved only if root discriminant is >= 0
        // just compute the discriminant
        float root = loc * loc - lenOC * lenOC + radius * radius;

        // If root discriminant is positive or zero, there's an intersection
        if (root >= 0)
        {
            highlightPole = i;
            mvpMatrix = sceneView->getModelViewProjectionMatrix();
            QVector3D posPoleClip = *mvpMatrix * posPole;
            offsetPickPositionToHandleCentre = QVector2D(posPoleClip.x() - clipX,
                                     posPoleClip.y() - clipY);
            break;
        }
    } // for (poles)

    return highlightPole;
}


void MMovablePoleActor::addPositionLabel(MSceneViewGLWidget *sceneView,
                                         int handleID, float clipX, float clipY)
{
    // Get properties for label font size and colour and bounding box.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);
    double lon = properties->mPointF()->value(poles[handleID / 2]
            ->positionProperty).x();
    double lat = properties->mPointF()->value(poles[handleID / 2]
            ->positionProperty).y();

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    MTextManager* tm = glRM->getTextManager();
    positionLabel = tm->addText(
                QString("lon:%1, lat:%2").arg(lon, 0, 'f', 2).arg(lat, 0, 'f', 2),
                MTextManager::LONLATP, poleVertices[handleID].x(),
                poleVertices[handleID].y(), poleVertices[handleID].z(),
                labelsize, labelColour, MTextManager::LOWERRIGHT,
                labelbbox, labelBBoxColour);

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
    float d = static_cast<float>(QVector3D::dotProduct(p0 - l0, n)
                                 / QVector3D::dotProduct(l, n));
    QVector3D mousePosWorldSpace = l0 + d * l;

    double weight = computePositionLabelDistanceWeight(sceneView->getCamera(),
                                                       mousePosWorldSpace);
    positionLabel->anchorOffset = -((weight + tubeRadius)
            * sceneView->getCamera()->getXAxis());

    emitActorChangedSignal();
}


void MMovablePoleActor::dragEvent(MSceneViewGLWidget *sceneView,
                                  int handleID, float clipX, float clipY)
{
    if (!movementEnabled)
    {
        return;
    }

    // Select an arbitrary z-value to construct a point in clip space that,
    // transformed to world space, lies on the ray passing through the camera
    // and the location on the worldZ==0 plane "picked" by the mouse.
    // (See notes 22-23Feb2012).
    QVector3D mousePosClipSpace = QVector3D(clipX + offsetPickPositionToHandleCentre.x(),
                                            clipY + offsetPickPositionToHandleCentre.y(),
                                            0.);

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
    float d = static_cast<float>(QVector3D::dotProduct(p0 - l0, n) / QVector3D::dotProduct(l, n));
    QVector3D mousePosWorldSpace = l0 + d * l;

    // Update the coordinates of pole, axis tick marks and labels. Upload new
    // positions to vertex buffers and redraw the scene.

    // We only have one pole vertex in a pole actor. So simply use handleID = 0
    int pole = handleID / 2;

    poleVertices[pole  ].setX(mousePosWorldSpace.x());
    poleVertices[pole  ].setY(mousePosWorldSpace.y());
    poleVertices[pole + 1].setX(mousePosWorldSpace.x());
    poleVertices[pole + 1].setY(mousePosWorldSpace.y());

    // Update tick mark positions.
    int numPoles = poleVertices.size() / 2; // two vertices per pole

    for (int i = pole; i < axisTicks.size(); i += numPoles)
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

    // Only change label if there is one present.
    if (positionLabel != nullptr)
    {
        MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
        MTextManager* tm = glRM->getTextManager();
        tm->removeText(positionLabel);

        // Get properties for label font size and colour and bounding box.
        int labelsize = properties->mInt()->value(labelSizeProperty);
        QColor labelColour = properties->mColor()->value(labelColourProperty);
        bool labelbbox = properties->mBool()->value(labelBBoxProperty);
        QColor labelBBoxColour =
                properties->mColor()->value(labelBBoxColourProperty);

        positionLabel = tm->addText(
                    QString("lon:%1, lat:%2")
                    .arg(poleVertices[handleID].x(), 0, 'f', 2)
                    .arg(poleVertices[handleID].y(), 0, 'f', 2),
                    MTextManager::LONLATP, poleVertices[handleID].x(),
                    poleVertices[handleID].y(), poleVertices[handleID].z(),
                    labelsize, labelColour, MTextManager::LOWERRIGHT,
                    labelbbox, labelBBoxColour);

        double weight = computePositionLabelDistanceWeight(sceneView->getCamera(),
                                                           mousePosWorldSpace);
        positionLabel->anchorOffset = -((weight + tubeRadius)
                * sceneView->getCamera()->getXAxis());
    }

    // Update GUI properties.
    properties->mPointF()->setValue(
            poles[pole]->positionProperty,
            QPointF(mousePosWorldSpace.x(), mousePosWorldSpace.y()));

    emitActorChangedSignal();
}


void MMovablePoleActor::setMovement(bool enabled)
{
    movementEnabled = enabled;
}


void MMovablePoleActor::setIndividualPoleHeightsEnabled(bool enabled)
{
    individualPoleHeightsEnabled = enabled;
    properties->mBool()->setValue(individualPoleHeightsProperty, enabled);
}

/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MMovablePoleActor::onQtPropertyChanged(QtProperty *property)
{
    // The slice position has been changed.
    if (property == bottomPressureProperty
            || (property == topPressureProperty))
    {
        bottomPressure_hPa = properties->mDDouble()->value(bottomPressureProperty);
        topPressure_hPa = properties->mDDouble()->value(topPressureProperty);

        if (suppressActorUpdates()) return;
        generateGeometry();
        emitActorChangedSignal();
    }

    // The slice position has been changed.
    else if (property == labelSizeProperty
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
        tickLength = static_cast<float>(properties->mDDouble()
                                        ->value(tickLengthProperty));
        emitActorChangedSignal();
    }

    else if (property == colourProperty)
    {
        lineColour = properties->mColor()->value(colourProperty);
        emitActorChangedSignal();
    }

    else if (property == addPoleProperty)
    {
        generatePole();
        if (suppressActorUpdates()) return;
        generateGeometry();
        emitActorChangedSignal();
    }

    else if (property == tickPressureThresholdProperty
             || property == tickIntervalAboveThreshold
             || property == tickIntervalBelowThreshold
             || property == labelSpacingProperty)
    {
        if (suppressActorUpdates()) return;
        generateGeometry();
        emitActorChangedSignal();
    }

    else if (property == renderModeProperty
             || property == tubeRadiusProperty)
    {
        renderMode = static_cast<RenderModes>(properties->mEnum()
                                              ->value(renderModeProperty));

        tubeRadiusProperty->setEnabled(renderMode == RenderModes::TUBES);

        tubeRadius = static_cast<float>(
                    properties->mDouble()->value(tubeRadiusProperty));

        emitActorChangedSignal();
    }

    else
    {
        individualPoleHeightsEnabled = properties->mBool()->value(
                    individualPoleHeightsProperty);

        for (int i = 0; i < poles.size(); i++)
        {
            std::shared_ptr<MovablePole> pole = poles.at(i);

            if (property == individualPoleHeightsProperty)
            {
                pole->bottomPressureProperty->setEnabled(individualPoleHeightsEnabled);
                pole->topPressureProperty->setEnabled(individualPoleHeightsEnabled);
            }

            else if (property == pole->positionProperty
                     || (individualPoleHeightsEnabled
                         && (property == pole->bottomPressureProperty
                             || property == pole->topPressureProperty)))
            {
                if (suppressActorUpdates()) return;
                generateGeometry();
                emitActorChangedSignal();
                break;
            }

            else if (property == pole->removePoleProperty)
            {
                actorPropertiesSupGroup->removeSubProperty(pole->groupProperty);
                poles.remove(i);
                if (suppressActorUpdates()) return;
                generateGeometry();
                emitActorChangedSignal();
                break;
            }
        }

        if (property == individualPoleHeightsProperty)
        {
            if (suppressActorUpdates()) return;
            generateGeometry();
            emitActorChangedSignal();
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


void MMovablePoleActor::generatePole()
{
    std::shared_ptr<MovablePole> pole = std::make_shared<MovablePole>(this);
    pole->bottomPressureProperty->setEnabled(individualPoleHeightsEnabled);
    pole->topPressureProperty->setEnabled(individualPoleHeightsEnabled);
    poles.append(pole);
    actorPropertiesSupGroup->addSubProperty(pole->groupProperty);
}


void MMovablePoleActor::generateGeometry()
{
    // A) Update/generate geometry.
    // =====================

    // Clear all poles
    poleVertices.clear();
    // Clear all ticks
    axisTicks.clear();

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

    for (int i = 0; i < poles.size(); ++i)
    {
        std::shared_ptr<MovablePole> pole = poles.at(i);

        QPointF pos = properties->mPointF()->value(pole->positionProperty);
        QVector3D polePos(pos);

        double pBot = 0; double pTop = 0;

        if (individualPoleHeightsEnabled)
        {
            pBot = properties->mDDouble()->value(pole->bottomPressureProperty);
            pTop = properties->mDDouble()->value(pole->topPressureProperty);
        }
        else
        {
            pBot = bottomPressure_hPa; pTop = topPressure_hPa;
        }

        polePos.setZ(pBot);
        poleVertices.append(polePos); // for pbot
        polePos.setZ(pTop);
        poleVertices.append(polePos); // for ptop

        // Create new axis ticks geometry, according to the pressure limits.
        const double upperTickStep =
                properties->mDouble()->value(tickIntervalAboveThreshold);

        const double lowerTickStep =
                properties->mDouble()->value(tickIntervalBelowThreshold);

        const double pressureThreshold =
                properties->mDDouble()->value(tickPressureThresholdProperty);

        const int labelIteration =
                properties->mInt()->value(labelSpacingProperty);

        int interval = static_cast<int>((pBot > pressureThreshold) ? upperTickStep : lowerTickStep);
        int p = int(pBot / interval) * interval;
        int counter = 0;

        while (p >= pTop)
        {
            axisTicks.append(QVector3D(polePos.x(), polePos.y(), p));

            // Generate labels for every ith tick mark
            if (counter % labelIteration == 0)
            {
                labels.append(tm->addText(
                        QString("%1").arg(p),
                        MTextManager::LONLATP,
                        polePos.x(), polePos.y(), p,
                        labelsize, labelColour, MTextManager::MIDDLELEFT,
                        labelbbox, labelBBoxColour)
                );
            }

            counter++;
            p -= (p > pressureThreshold) ? upperTickStep : lowerTickStep;
        }
    }

    // B) Upload geometry data to VBO.
    // ===============================

    const QString poleRequestKey = "pole_vertices_actor#"
                                   + QString::number(getID());
    uploadVec3ToVertexBuffer(poleVertices, poleRequestKey, &poleVertexBuffer);

    const QString axisRequestKey = "axis_vertices_actor#"
                                   + QString::number(getID());
    uploadVec3ToVertexBuffer(axisTicks, axisRequestKey, &axisVertexBuffer);
}

