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
#include "graticuleactor.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mstopwatch.h"
#include "util/mutil.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/textmanager.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MGraticuleActor::MGraticuleActor()
    : MActor(),
      graticuleVertexBuffer(nullptr),
      numVerticesGraticule(0),
      coastlineVertexBuffer(nullptr),
      graticuleColour(QColor(Qt::black))
{
    naturalEarthDataLoader = MSystemManagerAndControl::getInstance()
            ->getNaturalEarthDataLoader();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Graticule");

    cornersProperty = addProperty(RECTF_PROPERTY, "corners",
                                  actorPropertiesSupGroup);
    properties->setRectF(cornersProperty, QRectF(-90., 0., 180., 90.), 2);

    spacingProperty = addProperty(POINTF_PROPERTY, "spacing",
                                  actorPropertiesSupGroup);
    properties->setPointF(spacingProperty, QPointF(10., 5.), 2);

    colourProperty = addProperty(COLOR_PROPERTY, "colour",
                                 actorPropertiesSupGroup);
    properties->mColor()->setValue(colourProperty, graticuleColour);

    // Default vertical position is at 1050 hPa.
    setVerticalPosition(1049.);

    endInitialiseQtProperties();
}


MGraticuleActor::~MGraticuleActor()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MGraticuleActor::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MGraticuleActor::getSettingsID());

    settings->setValue("bbox", properties->mRectF()->value(cornersProperty));
    settings->setValue("spacing", properties->mPointF()->value(spacingProperty));
    settings->setValue("colour", graticuleColour);
    settings->setValue("verticalPosition", verticalPosition_hPa);

    settings->endGroup();
}


void MGraticuleActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MGraticuleActor::getSettingsID());

    QRectF bbox = settings->value("bbox").toRectF();
    properties->mRectF()->setValue(cornersProperty, bbox);

    QPointF spacing = settings->value("spacing").toPointF();
    properties->mPointF()->setValue(spacingProperty, spacing);

    QColor color = settings->value("colour").value<QColor>();
    properties->mColor()->setValue(colourProperty, color);

    verticalPosition_hPa = settings->value("verticalPosition").toFloat();

    settings->endGroup();
}


#define SHADER_VERTEX_ATTRIBUTE 0

void MGraticuleActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);
    shaderProgram->compileFromFile_Met3DHome("src/glsl/simple_coloured_geometry.fx.glsl");
}


void MGraticuleActor::setBBox(QRectF bbox)
{
    properties->mRectF()->setValue(cornersProperty, bbox);
}


void MGraticuleActor::setVerticalPosition(double pressure_hPa)
{
    // NOTE that the vertical position cannot be set by the user. Hence no
    // property is set here. No redraw is triggered.
    verticalPosition_hPa = pressure_hPa;

    foreach (MLabel* label, labels) label->anchor.setZ(pressure_hPa);
}


void MGraticuleActor::setColour(QColor c)
{
    properties->mColor()->setValue(colourProperty, c);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MGraticuleActor::initializeActorResources()
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    bool loadShaders = false;

    // Load shader program if the returned program is new.
    loadShaders |= glRM->generateEffectProgram("graticule_shader", shaderProgram);

    if (loadShaders) reloadShaderEffects();

    generateGeometry();
}


void MGraticuleActor::onQtPropertyChanged(QtProperty *property)
{
    // Recompute the geometry when bounding box or spacing have been changed.
    if ((property == cornersProperty)
            || (property == spacingProperty)
            || (property == labelSizeProperty)
            || (property == labelColourProperty)
            || (property == labelBBoxProperty)
            || (property == labelBBoxColourProperty) )
    {
        if (suppressActorUpdates()) return;
        generateGeometry();
        emitActorChangedSignal();
    }

    else if (property == colourProperty)
    {
        graticuleColour = properties->mColor()->value(colourProperty);
        emitActorChangedSignal();
    }
}


void MGraticuleActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    shaderProgram->bindProgram("IsoPressure");

    // Set uniform and attribute values.
    shaderProgram->setUniformValue("mvpMatrix",
                                   *(sceneView->getModelViewProjectionMatrix()));
    shaderProgram->setUniformValue("colour", graticuleColour);
    float worldZ = sceneView->worldZfromPressure(verticalPosition_hPa);
    shaderProgram->setUniformValue("worldZ", worldZ);

    // Draw graticule.
    graticuleVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
    glLineWidth(1); CHECK_GL_ERROR;

    glDrawArrays(GL_LINES, 0, numVerticesGraticule); CHECK_GL_ERROR;

    // Draw coastlines.
    coastlineVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);
    CHECK_GL_ERROR;

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
    glLineWidth(2); CHECK_GL_ERROR;

    glMultiDrawArrays(GL_LINE_STRIP,
                      coastlineStartIndices.constData(),
                      coastlineVertexCount.constData(),
                      coastlineStartIndices.size()); CHECK_GL_ERROR;

    // Draw borderlines.
    borderlineVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);
    CHECK_GL_ERROR;

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
    glLineWidth(1); CHECK_GL_ERROR;

    glMultiDrawArrays(GL_LINE_STRIP,
                      borderlineStartIndices.constData(),
                      borderlineVertexCount.constData(),
                      borderlineStartIndices.size()); CHECK_GL_ERROR;

    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MGraticuleActor::generateGeometry()
{
    // Make sure that "glResourcesManager" is the currently active context,
    // otherwise glDrawArrays on the VBO generated here will fail in any other
    // context than the currently active. The "glResourcesManager" context is
    // shared with all visible contexts, hence modifying the VBO there works
    // fine.
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    glRM->makeCurrent();

    LOG4CPLUS_DEBUG(mlog, "generating graticule geometry" << flush);

    // Remove all text labels of the old geometry (MActor method).
    removeAllLabels();

    // Get properties for label font size and colour and bounding box.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);

    // Get (user-defined) corner coordinates from the property browser.
    QRectF cornerRect = properties->mRectF()->value(cornersProperty);
    QPointF spacing = properties->mPointF()->value(spacingProperty);

    float llcrnrlat = cornerRect.y();
    float llcrnrlon = cornerRect.x();
    float urcrnrlat = cornerRect.y() + cornerRect.height();
    float urcrnrlon = cornerRect.x() + cornerRect.width();
    float deltalat  = spacing.y();
    float deltalon  = spacing.x();

    // Check for zero or negative spacing between the graticule lines.
    if (deltalon <= 0.)
    {
        deltalon = 1.;
        properties->mPointF()->setValue(spacingProperty,
                                        QPointF(deltalon, deltalat));
    }
    if (deltalat <= 0.)
    {
        deltalat = 1.;
        properties->mPointF()->setValue(spacingProperty,
                                        QPointF(deltalon, deltalat));
    }

    // Obtain a shortcut to the application's text manager to register the
    // labels generated in the loops below.
    MTextManager* tm = glRM->getTextManager();

    // Append all graticule lines to this vector.
    QVector<QVector2D> verticesGraticule;

    // Generate parallels (lines of constant latitude) and their correspondig
    // labels. NOTE that parallels are started at the first latitude that is
    // dividable by "deltalat". Thus, if llcrnrlat == 28.1 and deltalat == 5,
    // parallels will be drawn at 30, 35, .. etc.
    bool label = true;
    float latstart = llcrnrlat - fmod(llcrnrlat, deltalat);
    for (float lat = latstart; lat <= urcrnrlat; lat += deltalat)
    {
        verticesGraticule.append(QVector2D(llcrnrlon, lat));
        verticesGraticule.append(QVector2D(urcrnrlon, lat));

        if (label)
        {
            labels.append(tm->addText(
                              QString("%1").arg(lat),
                              MTextManager::LONLATP,
                              urcrnrlon, lat, verticalPosition_hPa,
                              labelsize, labelColour, MTextManager::BASELINECENTRE,
                              labelbbox, labelBBoxColour)
                          );
        }
        label = !label; // alternate labelling of lines
    }

    // Generate meridians (lines of constant longitude) and labels. NOTE that
    // meridians are also offset so that their position is dividable by
    // deltalon (see above).
    label = false;
    float lonstart = llcrnrlon - fmod(llcrnrlon, deltalon);
    for (float lon = lonstart; lon <= urcrnrlon; lon += deltalon)
    {
        verticesGraticule.append(QVector2D(lon, llcrnrlat));
        verticesGraticule.append(QVector2D(lon, urcrnrlat));

        if (label)
        {
            labels.append(tm->addText(
                              QString("%1").arg(lon),
                              MTextManager::LONLATP,
                              lon, llcrnrlat, verticalPosition_hPa,
                              labelsize, labelColour, MTextManager::BASELINECENTRE,
                              labelbbox, labelBBoxColour)
                          );
        }
        label = !label;
    }

    // generate data item key for every vertex buffer object wrt the actor
    const QString graticuleRequestKey = QString("graticule_vertices_actor#")
                                        + QString::number(getID());
    uploadVec2ToVertexBuffer(verticesGraticule, graticuleRequestKey,
                             &graticuleVertexBuffer);


    // Required for the glDrawArrays() call in renderToCurrentContext().
    numVerticesGraticule = verticesGraticule.size();

    // Load coastlines and upload the vertices to a GPU vertex buffer as well.
    QVector<QVector2D> verticesCoastlines;
    naturalEarthDataLoader->loadLineGeometry(
                MNaturalEarthDataLoader::COASTLINES,
                cornerRect,
                &verticesCoastlines,
                &coastlineStartIndices,
                &coastlineVertexCount,
                false);  // clear vectors

    const QString coastRequestKey = "graticule_coastlines_actor#"
                                    + QString::number(getID());
    uploadVec2ToVertexBuffer(verticesCoastlines, coastRequestKey,
                             &coastlineVertexBuffer);

    // .. and borderlines.
    QVector<QVector2D> verticesBorderlines;
    naturalEarthDataLoader->loadLineGeometry(
                MNaturalEarthDataLoader::BORDERLINES,
                cornerRect,
                &verticesBorderlines,
                &borderlineStartIndices,
                &borderlineVertexCount,
                false);  // clear vectors

    const QString borderRequestKey = "graticule_borderlines_actor#"
                                     + QString::number(getID());
    uploadVec2ToVertexBuffer(verticesBorderlines, borderRequestKey,
                             &borderlineVertexBuffer);
}

} // namespace Met3D
