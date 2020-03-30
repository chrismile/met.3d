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

MGraticuleActor::MGraticuleActor(MBoundingBoxConnection *boundingBoxConnection)
    : MRotatedGridSupportingActor(),
      MBoundingBoxInterface(this, MBoundingBoxConnectionType::HORIZONTAL,
                            boundingBoxConnection),
      graticuleVertexBuffer(nullptr),
      numVerticesGraticule(0),
      coastlineVertexBuffer(nullptr),
      graticuleColour(QColor(Qt::black)),
      drawGraticule(true),
      drawCoastLines(true),
      drawBorderLines(true),
      coastLinesCountIsValid(false),
      borderLinesCountIsValid(false)
{
    naturalEarthDataLoader = MSystemManagerAndControl::getInstance()
            ->getNaturalEarthDataLoader();

    nLats.clear();
    nLats.append(0);
    nLons.clear();
    nLons.append(0);

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setActorType(staticActorType());
    setName(getActorType());

    // Only add property group if graticule is not part of a horizontal cross
    // section.
    if (boundingBoxConnection == nullptr)
    {
        // Created graticule actor as standalone actor and thus it needs its
        // own bounding box actor. (As part of e.g. 2D Horizontal Cross-Section
        // it uses the bounding box connected to the Horizontal Cross-Section.)
        insertBoundingBoxProperty(actorPropertiesSupGroup);
    }

    spacingProperty = addProperty(POINTF_LONLAT_PROPERTY, "spacing",
                                  actorPropertiesSupGroup);
    properties->setPointF(spacingProperty, QPointF(10., 5.), 2);

    colourProperty = addProperty(COLOR_PROPERTY, "colour",
                                 actorPropertiesSupGroup);
    properties->mColor()->setValue(colourProperty, graticuleColour);

    drawGraticuleProperty = addProperty(BOOL_PROPERTY, "draw graticule",
                                        actorPropertiesSupGroup);
    properties->mBool()->setValue(drawGraticuleProperty, drawGraticule);

    drawCoastLinesProperty = addProperty(BOOL_PROPERTY, "draw coast lines",
                                         actorPropertiesSupGroup);
    properties->mBool()->setValue(drawCoastLinesProperty, drawCoastLines);

    drawBorderLinesProperty = addProperty(BOOL_PROPERTY, "draw border lines",
                                          actorPropertiesSupGroup);
    properties->mBool()->setValue(drawBorderLinesProperty, drawGraticule);

    actorPropertiesSupGroup->addSubProperty(rotatedGridPropertiesSubGroup);

    // Default vertical position is at 1049 hPa.
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
    MRotatedGridSupportingActor::saveConfiguration(settings);

    settings->beginGroup(MGraticuleActor::getSettingsID());

    // Only save bounding box if the graticule is directly connected to it.
    // Otherwise graticule is part of a horizontal cross-section actor and thus
    // its bounding box is handled via the horizontal cross-section actor.
    if (bBoxConnection->getActor() == this)
    {
        MBoundingBoxInterface::saveConfiguration(settings);
    }
    settings->setValue("spacing", properties->mPointF()->value(spacingProperty));
    settings->setValue("colour", graticuleColour);
    settings->setValue("drawGraticule", drawGraticule);
    settings->setValue("drawCoastLines", drawCoastLines);
    settings->setValue("drawBorderLines", drawBorderLines);
    settings->setValue("verticalPosition", verticalPosition_hPa);

    settings->endGroup();
}


void MGraticuleActor::loadConfiguration(QSettings *settings)
{
    MRotatedGridSupportingActor::loadConfiguration(settings);

    settings->beginGroup(MGraticuleActor::getSettingsID());

    // Only load bounding box if the graticule is directly connected to it.
    // Otherwise graticule is part of a horizontal cross-section actor and thus
    // its bounding box is handled via the horizontal cross-section actor.
    if (bBoxConnection->getActor() == this)
    {
        MBoundingBoxInterface::loadConfiguration(settings);
    }

    QPointF spacing = settings->value("spacing", QPointF(10., 5.)).toPointF();
    properties->mPointF()->setValue(spacingProperty, spacing);

    QColor color = settings->value("colour").value<QColor>();
    properties->mColor()->setValue(colourProperty, color);

    drawGraticule = settings->value("drawGraticule", true).toBool();
    properties->mBool()->setValue(drawGraticuleProperty, drawGraticule);

    drawCoastLines = settings->value("drawCoastLines", true).toBool();
    properties->mBool()->setValue(drawCoastLinesProperty, drawCoastLines);

    drawBorderLines = settings->value("drawBorderLines", true).toBool();
    properties->mBool()->setValue(drawBorderLinesProperty, drawBorderLines);

    verticalPosition_hPa = settings->value("verticalPosition", 1049.).toFloat();

    settings->endGroup();
}


#define SHADER_VERTEX_ATTRIBUTE 0

void MGraticuleActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);
    shaderProgram->compileFromFile_Met3DHome("src/glsl/simple_coloured_geometry.fx.glsl");
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


void MGraticuleActor::onBoundingBoxChanged()
{
    labels.clear();

    if (suppressActorUpdates())
    {
        return;
    }
    // Switching to no bounding box only needs a redraw, but no recomputation
    // because it disables rendering of the actor.
    if (bBoxConnection->getBoundingBox() != nullptr)
    {
        generateGeometry();
    }
    emitActorChangedSignal();
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
    if ( (property == spacingProperty)
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

    else if ( (property == drawGraticuleProperty)
              || (property == drawCoastLinesProperty)
              || (property == drawBorderLinesProperty) )
    {
        drawGraticule = properties->mBool()->value(drawGraticuleProperty);
        drawCoastLines = properties->mBool()->value(drawCoastLinesProperty);
        drawBorderLines = properties->mBool()->value(drawBorderLinesProperty);
        emitActorChangedSignal();
    }

    else if (property == enableGridRotationProperty)
    {
        enableGridRotation =
                properties->mBool()->value(enableGridRotationProperty);
        if (suppressActorUpdates()) return;
        generateGeometry();
        emitActorChangedSignal();
    }

    else if (property == rotateBBoxProperty)
    {
        rotateBBox = properties->mBool()->value(rotateBBoxProperty);
        if (suppressActorUpdates()) return;
        generateGeometry();
        emitActorChangedSignal();
    }

    else if (property == rotatedNorthPoleProperty)
    {
        rotatedNorthPole =
                properties->mPointF()->value(rotatedNorthPoleProperty);
        if (suppressActorUpdates()) return;
        if (enableGridRotation)
        {
            generateGeometry();
            emitActorChangedSignal();
        }
    }
}


void MGraticuleActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    // Draw nothing if no bounding box is available.
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }
    shaderProgram->bindProgram("IsoPressure");

    // Set uniform and attribute values.
    shaderProgram->setUniformValue("mvpMatrix",
                                   *(sceneView->getModelViewProjectionMatrix()));
    shaderProgram->setUniformValue("colour", graticuleColour);
    float worldZ = sceneView->worldZfromPressure(verticalPosition_hPa);
    shaderProgram->setUniformValue("worldZ", worldZ);

    if (drawGraticule)
    {
        // Draw graticule.
        graticuleVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
        glLineWidth(1); CHECK_GL_ERROR;

        if (enableGridRotation)
        {
            int startIndex = 0;
            if (rotateBBox)
            {
                // Start at index 1 since at index 0 the number of longitudes to
                // be drawn is stored.
                for (int j = 1; j < nLons.size(); j++)
                {
                    glDrawArrays(GL_LINE_STRIP, startIndex, nLons.at(j)); CHECK_GL_ERROR;
                    startIndex = startIndex + nLons.at(j);
                }
                // Use the number of longitudes stored at index 0 to draw the
                // longitudes.
                for (int i = 0; i < nLons.at(0); i++)
                {
                    glDrawArrays(GL_LINE_STRIP, startIndex, nLats.at(0)); CHECK_GL_ERROR;
                    startIndex = startIndex + nLats.at(0);
                }
            }
            else
            {
                for (int i = 0; i < nLats.size(); i++)
                {
                    glDrawArrays(GL_LINE_STRIP, startIndex, nLats.at(i)); CHECK_GL_ERROR;
                    startIndex = startIndex + nLats.at(i);
                }
                for (int i = 0; i < nLons.size(); i++)
                {
                    glDrawArrays(GL_LINE_STRIP, startIndex, nLons.at(i)); CHECK_GL_ERROR;
                    startIndex = startIndex + nLons.at(i);
                }
            }
        }
        else
        {
            glDrawArrays(GL_LINES, 0, numVerticesGraticule); CHECK_GL_ERROR;
        }
    }

    if (drawCoastLines)
    {
        if (coastLinesCountIsValid)
        {
            // Draw coastlines.
            coastlineVertexBuffer->attachToVertexAttribute(
                        SHADER_VERTEX_ATTRIBUTE);
            CHECK_GL_ERROR;

            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
            glLineWidth(2); CHECK_GL_ERROR;

            glMultiDrawArrays(GL_LINE_STRIP,
                              coastlineStartIndices.constData(),
                              coastlineVertexCount.constData(),
                              coastlineStartIndices.size()); CHECK_GL_ERROR;
        }
    }

    if (drawBorderLines)
    {
        if (borderLinesCountIsValid)
        {
            // Draw borderlines.
            borderlineVertexBuffer->attachToVertexAttribute(
                        SHADER_VERTEX_ATTRIBUTE);
            CHECK_GL_ERROR;

            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
            glLineWidth(1); CHECK_GL_ERROR;

            glMultiDrawArrays(GL_LINE_STRIP,
                              borderlineStartIndices.constData(),
                              borderlineVertexCount.constData(),
                              borderlineStartIndices.size()); CHECK_GL_ERROR;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MGraticuleActor::generateGeometry()
{
    // Generate nothing if no bounding box is available.
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }
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
    QRectF cornerRect = bBoxConnection->horizontal2DCoords();
    QPointF spacing = properties->mPointF()->value(spacingProperty);

    float llcrnrlat = float(bBoxConnection->southLat());
    float llcrnrlon = float(bBoxConnection->westLon());
    float urcrnrlat = float(bBoxConnection->northLat());
    float urcrnrlon = float(bBoxConnection->eastLon());
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

    if (!enableGridRotation)
    {
        // Generate parallels (lines of constant latitude) and their correspondig
        // labels. NOTE that parallels are started at the first latitude that is
        // dividable by "deltalat". Thus, if llcrnrlat == 28.1 and deltalat == 5,
        // parallels will be drawn at 30, 35, .. etc.
        bool label = true;
        float latstart = ceil(llcrnrlat / deltalat) * deltalat;

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
        float lonstart = ceil(llcrnrlon / deltalon) * deltalon;

        label = false;
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
    } // if !useRotation
    else
    {
        OGRPoint *point = new OGRPoint();
        nLons.clear();
        nLats.clear();

        // Variables used to get rid of lines crossing the whole domain.
        // (Conntection of the right most and the left most vertex)
        QVector2D prevPosition(0., 0.);
        QVector2D currPosition(0., 0.);
        QVector2D centreLons(0., 0.);

        MNaturalEarthDataLoader::getCentreLons(
                    &centreLons, rotatedNorthPole.y(), rotatedNorthPole.x());

        bool label = true;
        // Rotate graticule and bounding box.
        if (rotateBBox)
        {
            // Draw rotated graticule only in the latitude range [-90,90] since
            // otherwise graticule lines might overlap due to the rotation applied.
            urcrnrlat = min( 90.f, urcrnrlat);
            llcrnrlat = max(-90.f, llcrnrlat);
            // Compute starting points (see starting point computation above).
            float latstart = ceil(llcrnrlat / deltalat) * deltalat;
            float lonstart = ceil(llcrnrlon / deltalon) * deltalon;

            // Compute number of longitudes and latitudes.
            int numlons = floor((urcrnrlon - lonstart) / deltalon) + 1;
            int numlats = floor((urcrnrlat - latstart) / deltalat) + 1;

            // Compute length of a latitude/longitude line in number of
            // vertices (start, end, number of "crossing points"). This might
            // differ from the number of lons and lats to be drawn if the
            // bounding box does not start and/or end at lon/lat coordinates
            // which can be divided by the according delta.
            int lengthLat = ceil(urcrnrlon / deltalon)
                    - floor(llcrnrlon / deltalon) + 1;
            int lengthLon = ceil(urcrnrlat / deltalat)
                    - floor(llcrnrlat / deltalat) + 1;

            // Special case: The length of a latitude or longitude might be
            // computed wrong if the size of the bounding box is 0 but the
            // boundary coordinate cannot be divided by the delta of the
            // according dimension (ceil and floor differ and do not eliminate
            // each other but they should in this special case).
            if (urcrnrlon == llcrnrlon)
            {
                lengthLat = 1;
            }
            if (urcrnrlat == llcrnrlat)
            {
                lengthLon = 1;
            }
            int countLons = 0;

            // Since the length of lat/lon might differ from the number of
            // lons/lats we want to draw, we need different indices to compute
            // the position of a vertex in the vertices array (e.g. if the
            // southern latitude of the bounding box is not divisible by
            // deltalat, iLon starts with the bounding box boundary to draw the
            // longitude from boundary to boundary but iLat starts with latstart
            // since we draw latitudes (and longitudes) only at coordinates
            // divisible by the range).
            int iLon = 0;
            int jLon = 0;
            int iLat = 0;
            int jLat = 0;
            // Store number of longitudes at first position of nLons for later
            // use to draw correct amout of longitudes.
            nLons.append(numlons);
            nLats.append(lengthLon);
            int latVerticesCount = lengthLat * numlats;
            verticesGraticule.resize(latVerticesCount + lengthLon * numlons);
            bool lonLabel = false;

            for (float lat = llcrnrlat; lat <= urcrnrlat; lat += deltalat)
            {
                // Reset j indices (represents longitude coordinate in array
                // index computation).
                jLon = 0;
                jLat = 0;
                // Initialise previous longitudes with first longitude to avoid
                // discarding it.
                point->setX(MMOD(lonstart + 180., 360.) - 180.);
                point->setY(lat);
                MNaturalEarthDataLoader::geographicalToRotatedCoords(
                            point, rotatedNorthPole.y(), rotatedNorthPole.x());
                prevPosition.setX(point->getX());
                prevPosition.setY(point->getY());
                for (float lon = llcrnrlon; lon <= urcrnrlon; lon += deltalon)
                {
                    point->setX(MMOD(lon + 180., 360.) - 180.);
                    point->setY(lat);
                    if (!MNaturalEarthDataLoader::validConnectionBetweenPositions(
                                &prevPosition, &currPosition, point,
                                rotatedNorthPole.y(), rotatedNorthPole.x(),
                                &centreLons))
                    {
                        // Start new line.
                        nLons.append(countLons);
                        countLons = 0;
                    }

                    // Don't draw latitudes at latitude coordinates which cannot
                    // be divided by deltalat but start longitude lines at
                    // bounding box boundary.
                    if (fmod(lat, deltalat) == 0.f)
                    {
                        verticesGraticule.replace(iLat * lengthLat + jLat,
                                                  currPosition);
                        ++countLons;
                    }
                    // Same for longitudes.
                    if (fmod(lon, deltalon) == 0.f)
                    {
                        verticesGraticule.replace(
                                    latVerticesCount + jLon * lengthLon + iLon,
                                    currPosition);
                    }

                    ++jLat;

                    // We might need to adapt lon if the distance between
                    // llcrnrlon and the next node is not equal to deltalon
                    // (detailed explanation see below).
                    if (lon == llcrnrlon)
                    {
                        if (llcrnrlon != lonstart)
                        {
                            lon = lonstart - deltalon;
                            continue;
                        }
                    }

                    ++jLon;

                    // Add longitude label.
                    if (lat == llcrnrlat && lonLabel)
                    {
                        point->setX(MMOD(lon + 180., 360.) - 180.);
                        point->setY(llcrnrlat);
                        MNaturalEarthDataLoader::geographicalToRotatedCoords(
                                    point, rotatedNorthPole.y(),
                                    rotatedNorthPole.x());
                        labels.append(tm->addText(
                                          QString("%1").arg(lon),
                                          MTextManager::LONLATP,
                                          point->getX(), point->getY(),
                                          verticalPosition_hPa, labelsize,
                                          labelColour,
                                          MTextManager::BASELINECENTRE,
                                          labelbbox, labelBBoxColour)
                                      );
                    }
                    lonLabel = !lonLabel; // alternate labelling of lines

                    // We might need to adapt lon if the distance between
                    // the second to last node's longitude and urcrnrlon is not
                    // equal to deltalon (detailed explanation see below).
                    if (lon + deltalon > urcrnrlon)
                    {
                        if (lon != urcrnrlon)
                        {
                            lon = urcrnrlon - deltalon;
                            lonLabel = false;
                        }
                    }
                }

                nLons.append(countLons);
                countLons = 0;

                ++iLon;

                // We always start with llcrnrlat but its distance to the next
                // node of the graticule might not be deltalat (this is the
                // case if llcrnrlat is not divisble by deltalat). Thus we need
                // to manipulate lat so that after llcrnrlat the next greatest
                // latitude value divisible by deltalat (latstart) will follow.
                // We need to substract deltalat since the for loop will add it
                // for the next iteration.
                if (lat == llcrnrlat)
                {
                    if (llcrnrlat != latstart)
                    {
                        lat = latstart - deltalat;
                        continue;
                    }
                }

                ++iLat;

                if (label)
                {
                    point->setX(MMOD(urcrnrlon + 180., 360.) - 180.);
                    point->setY(lat);
                    MNaturalEarthDataLoader::geographicalToRotatedCoords(
                                point, rotatedNorthPole.y(),
                                rotatedNorthPole.x());
                    labels.append(tm->addText(
                                      QString("%1").arg(lat),
                                      MTextManager::LONLATP,
                                      point->getX(), point->getY(),
                                      verticalPosition_hPa, labelsize,
                                      labelColour, MTextManager::BASELINECENTRE,
                                      labelbbox, labelBBoxColour)
                                  );
                }
                label = !label; // alternate labelling of lines

                // If the distance betweene the second to last node's latitude
                // coordinate and urcrnrlat is smaller than deltalat, we need
                // to adapt lat after reaching the second to last latitude so
                // that the we end at urcrnrlat as last latitude coordinate.
                if (lat + deltalat > urcrnrlat)
                {
                    if (lat != urcrnrlat)
                    {
                        lat = urcrnrlat - deltalat;
                        label = false;
                    }
                }
            } // for latitudes.
        } // if rotateBBox
        // Rotate graticule but not bounding box (represents rotated coordinates).
        else
        {
            int numVertices = 0;

            float lonStart = llcrnrlon;
            float latStart = llcrnrlat;

            // Get bounding box as polygon to compute intersection with the
            // graticule.
            OGRPolygon *bboxPolygon =
                    MNaturalEarthDataLoader::getBBoxPolygon(&cornerRect);

            // List storing separated ling strings.
            QList<OGRLineString*> lineStringList;
            OGRLineString* lineString;

            // Use the whole grid to make sure to not miss some grid cells.
            for (float lat = -90.f; lat <= 90.f; lat += deltalat)
            {
                numVertices = 0;
                lineStringList.append(new OGRLineString());
                lineString = lineStringList.at(0);
                // Intial setting of previous (rotated) longitude;
                point->setX(-180.);
                point->setY(lat);
                MNaturalEarthDataLoader::geographicalToRotatedCoords(
                            point, rotatedNorthPole.y(), rotatedNorthPole.x());
                float lon;
                // Use the whole grid to make sure to not miss some grid cells.
                for (lon = -180.f; lon <= 180.f; lon += deltalon)
                {
                    point->setX(lon);
                    point->setY(lat);
                    if (!MNaturalEarthDataLoader::validConnectionBetweenPositions(
                                &prevPosition, &currPosition, point,
                                rotatedNorthPole.y(), rotatedNorthPole.x(),
                                &centreLons))
                    {
                        // Start new line.
                        lineStringList.append(new OGRLineString());
                        lineString = lineStringList.last();
                    }
                    lineString->addPoint(currPosition.x(), currPosition.y());
                }
                // Add missing connection line for spacing between first and
                // last lon smaller than deltalon.
                if (lon > 180.f)
                {
                    point->setX(-180.f);
                    point->setY(lat);
                    if (!MNaturalEarthDataLoader::validConnectionBetweenPositions(
                                &prevPosition, &currPosition, point,
                                rotatedNorthPole.y(), rotatedNorthPole.x(),
                                &centreLons))
                    {
                        // Start new line.
                        lineStringList.append(new OGRLineString());
                        lineString = lineStringList.last();
                    }
                    lineString->addPoint(currPosition.x(), currPosition.y());
                }

                foreach (lineString, lineStringList)
                {
                    // Only use valid lines with more than one vertex.
                    if (lineString->getNumPoints() <= 1)
                    {
                        numVertices = 0;
                        delete lineString;
                        continue;
                    }

                    // Compute intersection with bbox.
                    OGRGeometry *iGeometry =
                            lineString->Intersection(bboxPolygon);

                    // The intersection can be either a single line string, or a
                    // collection of line strings.

                    if (iGeometry->getGeometryType() == wkbLineString)
                    {
                        // Get all points from the intersected line string and
                        // append them to the "vertices" vector.
                        OGRLineString *iLine = (OGRLineString *) iGeometry;
                        int numLinePoints = iLine->getNumPoints();
                        OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                        iLine->getPoints(v);
                        for (int i = 0; i < numLinePoints; i++)
                        {
                            verticesGraticule.append(QVector2D(v[i].x, v[i].y));
                            numVertices++;
                        }
                        if (v[numLinePoints - 1].x < v[0].x)
                        {
                            lonStart = v[0].x;
                            latStart = v[0].y;
                        }
                        else
                        {
                            lonStart = v[numLinePoints - 1].x;
                            latStart = v[numLinePoints - 1].y;
                        }
                        delete[] v;
                        nLats.append(numVertices);
                        numVertices = 0;
                    }

                    else if (iGeometry->getGeometryType() == wkbMultiLineString)
                    {
                        // Loop over all line strings in the collection,
                        // appending their points to "vertices" as above.
                        OGRGeometryCollection *geomCollection =
                                (OGRGeometryCollection *) iGeometry;

                        for (int g = 0; g < geomCollection->getNumGeometries();
                             g++)
                        {
                            OGRLineString *iLine = (OGRLineString *)
                                    geomCollection->getGeometryRef(g);
                            int numLinePoints = iLine->getNumPoints();
                            OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                            iLine->getPoints(v);
                            for (int i = 0; i < numLinePoints; i++)
                            {
                                verticesGraticule.append(QVector2D(v[i].x,
                                                                   v[i].y));
                                numVertices++;
                            }
                            if (lonStart < v[0].x)
                            {
                                lonStart = v[0].x;
                                latStart = v[0].y;
                            }
                            if (lonStart < v[numLinePoints - 1].x)
                            {
                                lonStart = v[numLinePoints - 1].x;
                                latStart = v[numLinePoints - 1].y;
                            }
                            delete[] v;
                            // Restart after each line segment to avoid
                            // connections between line segements separated by
                            // intersection with bounding box.
                            nLats.append(numVertices);
                            numVertices = 0;
                        }
                    }
                    numVertices = 0;
                    delete lineString;
                }

                lineStringList.clear();

                if (label && llcrnrlat < latStart && latStart < urcrnrlat)
                {
                    labels.append(tm->addText(
                                      QString("%1").arg(lat),
                                      MTextManager::LONLATP, lonStart, latStart,
                                      verticalPosition_hPa, labelsize,
                                      labelColour, MTextManager::BASELINECENTRE,
                                      labelbbox, labelBBoxColour)
                                  );
                }
                label = !label; // alternate labelling of lines
                latStart = llcrnrlat;
                lonStart = llcrnrlon;
            } // for latitudes.

            label = false;
            for (float lon = -180.f; lon <= 180.f; lon += deltalon)
            {
                lineString = new OGRLineString();
                // Allways use the whole grid to make sure to not miss some grid
                // cells.
                for (float lat = -90.f; lat <= 90.f; lat += deltalat)
                {
                    point->setX(lon);
                    point->setY(lat);
                    MNaturalEarthDataLoader::geographicalToRotatedCoords(
                                point, rotatedNorthPole.y(),
                                rotatedNorthPole.x());
                    lineString->addPoint(point->getX(), point->getY());
                }

                // Compute intersection with bbox.
                OGRGeometry *iGeometry = lineString->Intersection(bboxPolygon);

                // The intersection can be either a single line string, or a
                // collection of line strings.

                if (iGeometry->getGeometryType() == wkbLineString)
                {
                    // Get all points from the intersected line string and
                    // append them to the "vertices" vector.
                    OGRLineString *iLine = (OGRLineString *) iGeometry;
                    int numLinePoints = iLine->getNumPoints();
                    OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                    iLine->getPoints(v);
                    lonStart = v[0].x;
                    latStart = v[0].y;
                    for (int i = 0; i < numLinePoints; i++)
                    {
                        verticesGraticule.append(QVector2D(v[i].x, v[i].y));
                        numVertices++;
                    }
                    delete[] v;
                    nLons.append(numVertices);
                    numVertices = 0;
                }

                else if (iGeometry->getGeometryType() == wkbMultiLineString)
                {
                    // Loop over all line strings in the collection, appending
                    // their points to "vertices" as above.
                    OGRGeometryCollection *geomCollection =
                            (OGRGeometryCollection *) iGeometry;

                    for (int g = 0; g < geomCollection->getNumGeometries(); g++)
                    {
                        OGRLineString *iLine = (OGRLineString *) geomCollection
                                ->getGeometryRef(g);
                        int numLinePoints = iLine->getNumPoints();
                        OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                        iLine->getPoints(v);
                        lonStart = v[0].x;
                        latStart = v[0].y;
                        for (int i = 0; i < numLinePoints; i++)
                        {
                            verticesGraticule.append(QVector2D(v[i].x, v[i].y));
                            numVertices++;
                        }
                        nLons.append(numVertices);
                        numVertices = 0;
                        delete[] v;
                    }
                }
                delete lineString;

                if (label && llcrnrlon < lonStart && lonStart < urcrnrlon)
                {
                    labels.append(tm->addText(
                                      QString("%1").arg(lon),
                                      MTextManager::LONLATP, lonStart, latStart,
                                      verticalPosition_hPa, labelsize,
                                      labelColour, MTextManager::BASELINECENTRE,
                                      labelbbox, labelBBoxColour)
                                  );
                }
                label = !label;
                lonStart = llcrnrlon;
            } // for longitudes.
        } // else rotateBBox
        // Clean up.
        delete point;
    } // else !useRotation

    // generate data item key for every vertex buffer object wrt the actor
    const QString graticuleRequestKey = QString("graticule_vertices_actor#")
                                        + QString::number(getID());
    uploadVec2ToVertexBuffer(verticesGraticule, graticuleRequestKey,
                             &graticuleVertexBuffer);


    // Required for the glDrawArrays() call in renderToCurrentContext().
    numVerticesGraticule = verticesGraticule.size();

    // Load coastlines and upload the vertices to a GPU vertex buffer as well.
    QVector<QVector2D> verticesCoastlines;
    if (enableGridRotation)
    {
        if (rotateBBox)
        {
            naturalEarthDataLoader->loadAndRotateLineGeometry(
                        MNaturalEarthDataLoader::COASTLINES,
                        cornerRect,
                        &verticesCoastlines,
                        &coastlineStartIndices,
                        &coastlineVertexCount,
                        false, rotatedNorthPole.y(),
                        rotatedNorthPole.x());  // clear vectors
        }
        else
        {
            naturalEarthDataLoader->loadAndRotateLineGeometryUsingRotatedBBox(
                        MNaturalEarthDataLoader::COASTLINES,
                        cornerRect,
                        &verticesCoastlines,
                        &coastlineStartIndices,
                        &coastlineVertexCount,
                        false, rotatedNorthPole.y(),
                        rotatedNorthPole.x());  // clear vectors
        }
    }
    else
    {
        naturalEarthDataLoader->loadCyclicLineGeometry(
                               MNaturalEarthDataLoader::COASTLINES,
                               cornerRect, &verticesCoastlines,
                               &coastlineStartIndices, &coastlineVertexCount);
    }
    const QString coastRequestKey = "graticule_coastlines_actor#"
                                    + QString::number(getID());
    uploadVec2ToVertexBuffer(verticesCoastlines, coastRequestKey,
                             &coastlineVertexBuffer);

    // .. and borderlines.
    QVector<QVector2D> verticesBorderlines;
    if (enableGridRotation)
    {
        if (rotateBBox)
        {
        naturalEarthDataLoader->loadAndRotateLineGeometry(
                    MNaturalEarthDataLoader::BORDERLINES,
                    cornerRect,
                    &verticesBorderlines,
                    &borderlineStartIndices,
                    &borderlineVertexCount,
                    false, rotatedNorthPole.y(),
                    rotatedNorthPole.x());  // clear vectors
        }
        else
        {
            naturalEarthDataLoader->loadAndRotateLineGeometryUsingRotatedBBox(
                        MNaturalEarthDataLoader::BORDERLINES,
                        cornerRect,
                        &verticesBorderlines,
                        &borderlineStartIndices,
                        &borderlineVertexCount,
                        false, rotatedNorthPole.y(),
                        rotatedNorthPole.x());  // clear vectors
        }
    }
    else
    {
        naturalEarthDataLoader->loadCyclicLineGeometry(
                               MNaturalEarthDataLoader::BORDERLINES,
                               cornerRect, &verticesBorderlines,
                               &borderlineStartIndices, &borderlineVertexCount);
    }


    const QString borderRequestKey = "graticule_borderlines_actor#"
                                     + QString::number(getID());
    uploadVec2ToVertexBuffer(verticesBorderlines, borderRequestKey,
                             &borderlineVertexBuffer);

    // Since a vertex count array filled with zeros leads to a program
    // crash, check if the vertex counts of coast and border lines contain at
    // least one valid values.

    coastLinesCountIsValid = false;
    foreach (int vertexCount, coastlineVertexCount)
    {
        if (vertexCount > 0)
        {
            coastLinesCountIsValid = true;
            break;
        }
    }

    borderLinesCountIsValid = false;
    foreach (int vertexCount, borderlineVertexCount)
    {
        if (vertexCount > 0)
        {
            borderLinesCountIsValid = true;
            break;
        }
    }
}

} // namespace Met3D
