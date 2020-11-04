/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2020 Kameswarro Modali [*]
**  Copyright 2017 Bianca Tost [+]
**
**  + Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  * Regional Computing Center, Visual Data Analysis Group
**  Universitaet Hamburg, Hamburg, Germany
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
    : MMapProjectionSupportingActor(QList<MapProjectionType>()
                                    << MAPPROJECTION_CYLINDRICAL
                                    << MAPPROJECTION_ROTATEDLATLON
                                    << MAPPROJECTION_PROJ_LIBRARY),
      MBoundingBoxInterface(this, MBoundingBoxConnectionType::HORIZONTAL,
                            boundingBoxConnection),
      graticuleVertexBuffer(nullptr),
      coastlineVertexBuffer(nullptr),
      borderlineVertexBuffer(nullptr),
      defaultGraticuleLongitudesString("[-180.,180.,10.]"),
      defaultGraticuleLatitudesString("[-90.,90.,5.]"),
      defaultLongitudeLabelsString("[-180.,180.,20.]"),
      defaultLatitudeLabelsString("[-90.,90.,10.]"),
      graticuleColour(QColor(Qt::black)),
      drawGraticule(true),
      drawCoastLines(true),
      drawBorderLines(true)
{
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

    graticuleLongitudesProperty = addProperty(STRING_PROPERTY,
                                              "graticule longitudes",
                                              actorPropertiesSupGroup);
    properties->mString()->setValue(graticuleLongitudesProperty,
                                    defaultGraticuleLongitudesString);
    graticuleLongitudesProperty->setToolTip(
                "Format can be '[from,to,step]' or 'v1,v2,v3,...'.");

    graticuleLatitudesProperty = addProperty(STRING_PROPERTY,
                                             "graticule latitudes",
                                             actorPropertiesSupGroup);
    properties->mString()->setValue(graticuleLatitudesProperty,
                                    defaultGraticuleLatitudesString);
    graticuleLatitudesProperty->setToolTip(
                "Format can be '[from,to,step]' or 'v1,v2,v3,...'.");

    longitudeLabelsProperty = addProperty(STRING_PROPERTY,
                                          "longitude labels",
                                          actorPropertiesSupGroup);
    properties->mString()->setValue(longitudeLabelsProperty,
                                    defaultLongitudeLabelsString);
    longitudeLabelsProperty->setToolTip(
                "Format can be '[from,to,step]' or 'v1,v2,v3,...'.");

    latitudeLabelsProperty = addProperty(STRING_PROPERTY,
                                         "latitude labels",
                                         actorPropertiesSupGroup);
    properties->mString()->setValue(latitudeLabelsProperty,
                                    defaultLatitudeLabelsString);
    latitudeLabelsProperty->setToolTip(
                "Format can be '[from,to,step]' or 'v1,v2,v3,...'.");

    vertexSpacingProperty = addProperty(POINTF_LONLAT_PROPERTY,
                                        "vertex spacing",
                                        actorPropertiesSupGroup);
    properties->setPointF(vertexSpacingProperty, QPointF(1., 1.), 2);

    computeGraticuleProperty = addProperty(CLICK_PROPERTY,
                                           "re-compute graticule",
                                           actorPropertiesSupGroup);

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

    actorPropertiesSupGroup->addSubProperty(mapProjectionPropertiesSubGroup);

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
    MMapProjectionSupportingActor::saveConfiguration(settings);

    settings->beginGroup(MGraticuleActor::getSettingsID());

    // Only save bounding box if the graticule is directly connected to it.
    // Otherwise graticule is part of a horizontal cross-section actor and thus
    // its bounding box is handled via the horizontal cross-section actor.
    if (bBoxConnection->getActor() == this)
    {
        MBoundingBoxInterface::saveConfiguration(settings);
    }
    settings->setValue("graticuleLongitudes",
                       properties->mString()->value(graticuleLongitudesProperty));
    settings->setValue("graticuleLatitudes",
                       properties->mString()->value(graticuleLatitudesProperty));
    settings->setValue("graticuleLongitudeLabels",
                       properties->mString()->value(longitudeLabelsProperty));
    settings->setValue("graticuleLatitudeLabels",
                       properties->mString()->value(latitudeLabelsProperty));
    settings->setValue("vertexSpacing",
                       properties->mPointF()->value(vertexSpacingProperty));
    settings->setValue("colour", graticuleColour);
    settings->setValue("drawGraticule", drawGraticule);
    settings->setValue("drawCoastLines", drawCoastLines);
    settings->setValue("drawBorderLines", drawBorderLines);
    settings->setValue("verticalPosition", verticalPosition_hPa);

    settings->endGroup();
}


void MGraticuleActor::loadConfiguration(QSettings *settings)
{
    MMapProjectionSupportingActor::loadConfiguration(settings);

    settings->beginGroup(MGraticuleActor::getSettingsID());

    // Only load bounding box if the graticule is directly connected to it.
    // Otherwise graticule is part of a horizontal cross-section actor and thus
    // its bounding box is handled via the horizontal cross-section actor.
    if (bBoxConnection->getActor() == this)
    {
        MBoundingBoxInterface::loadConfiguration(settings);
    }

    QString lonsStr = settings->value(
                "graticuleLongitudes", defaultGraticuleLongitudesString).toString();
    properties->mString()->setValue(graticuleLongitudesProperty, lonsStr);

    QString latsStr = settings->value(
                "graticuleLatitudes", defaultGraticuleLatitudesString).toString();
    properties->mString()->setValue(graticuleLatitudesProperty, latsStr);

    QString lonLabelsStr = settings->value(
                "graticuleLongitudeLabels", defaultLongitudeLabelsString).toString();
    properties->mString()->setValue(longitudeLabelsProperty, lonLabelsStr);

    QString latLabelssStr = settings->value(
                "graticuleLatitudeLabels", defaultLatitudeLabelsString).toString();
    properties->mString()->setValue(latitudeLabelsProperty, latLabelssStr);

    QPointF spacing = settings->value("vertexSpacing", QPointF(1., 1.)).toPointF();
    properties->mPointF()->setValue(vertexSpacingProperty, spacing);

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

    // Update geometry with loaded configuration.
    generateGeometry();
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
    if ( (property == computeGraticuleProperty)
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
    else if (property == mapProjectionTypesProperty)
    {
        updateMapProjectionProperties();
        if (suppressActorUpdates()) return;
        generateGeometry();
        emitActorChangedSignal();
    }
    else if (property == rotatedNorthPoleProperty)
    {
        rotatedNorthPole =
                properties->mPointF()->value(rotatedNorthPoleProperty);
        if (suppressActorUpdates()) return;
        if (mapProjection == MAPPROJECTION_ROTATEDLATLON)
        {
            generateGeometry();
            emitActorChangedSignal();
        }
    }
    else if (property == projLibraryApplyProperty)
    {
        projLibraryString =
                properties->mString()->value(projLibraryStringProperty);
        if (suppressActorUpdates()) return;
        if (mapProjection == MAPPROJECTION_PROJ_LIBRARY)
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

        glMultiDrawArrays(GL_LINE_STRIP,
                          graticuleStartIndices.constData(),
                          graticuleVertexCount.constData(),
                          graticuleStartIndices.size()); CHECK_GL_ERROR;
    }

    if (drawCoastLines)
    {
        // Draw coastlines.
        coastlineVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);
        CHECK_GL_ERROR;

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
        glLineWidth(2); CHECK_GL_ERROR;

        glMultiDrawArrays(GL_LINE_STRIP,
                          coastlineStartIndices.constData(),
                          coastlineVertexCount.constData(),
                          coastlineStartIndices.size()); CHECK_GL_ERROR;
    }

    if (drawBorderLines)
    {
        // Draw borderlines.
        borderlineVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);
        CHECK_GL_ERROR;

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
        glLineWidth(1); CHECK_GL_ERROR;

        glMultiDrawArrays(GL_LINE_STRIP,
                          borderlineStartIndices.constData(),
                          borderlineVertexCount.constData(),
                          borderlineStartIndices.size()); CHECK_GL_ERROR;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MGraticuleActor::generateGeometry()
{
    // The method requires a bounding box to correctly generate geometry.
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return;
    }

    LOG4CPLUS_DEBUG(mlog, "Generating graticule and coast-/borderline "
                          "geometry..." << flush);

    // Generate graticule geometry.
    // ============================
    QVector<float> graticuleLongitudes = parseFloatRangeString(
                properties->mString()->value(graticuleLongitudesProperty));
    QVector<float> graticuleLatitudes = parseFloatRangeString(
                properties->mString()->value(graticuleLatitudesProperty));
    QVector2D graticuleSpacing(
                properties->mPointF()->value(vertexSpacingProperty));

    // Instantiate utility class for geometry handling.
    MGeometryHandling geo;
    geo.initProjProjection(projLibraryString);
    geo.initRotatedLonLatProjection(rotatedNorthPole);

    // Generate graticule geometry.
    QVector<QPolygonF> graticule = geo.generate2DGraticuleGeometry(
                graticuleLongitudes, graticuleLatitudes, graticuleSpacing);

    // Heuristic value to eliminate line segments that after projection cross
    // the map domain due to a connection that after projection is invalid.
    // This happens when a line segment that connects two closeby vertices after
    // projection leaves e.g. the eastern side of the map and re-enters on the
    // western side (or vice versa).
    // A value of "20deg" seems to work well with global data from NaturalEarth.
    // NOTE (mr, 28Oct2020): This is "quick&dirty" workaround. The correct
    // approach to this would be to perform some sort of test that checks if
    // the correct connection of the two vertices after projection should cross
    // map boundaries, in such as case the segment needs to be broken up.
    // Another approach to this was previously implemented in BT's code, see
    // Met.3D version 1.6 or earlier.
    double rotatedGridMaxSegmentLength_deg = 20.;

    // Get bounding box in which the graticule will be displayed.
    QRectF bbox = bBoxConnection->horizontal2DCoords();

    // Project and clip the generated graticule geometry.
    graticule = projectAndClipGeometry(
                &geo, graticule, bbox, rotatedGridMaxSegmentLength_deg);

    // Convert list of polygons to vertex list for OpenGL rendering.
    QVector<QVector2D> verticesGraticule;
    graticuleStartIndices.clear();
    graticuleVertexCount.clear();
    geo.flattenPolygonsToVertexList(graticule,
                                    &verticesGraticule,
                                    &graticuleStartIndices,
                                    &graticuleVertexCount);

    // Make sure that "glResourcesManager" is the currently active context,
    // otherwise glDrawArrays on the VBO generated here will fail in any other
    // context than the currently active. The "glResourcesManager" context is
    // shared with all visible contexts, hence modifying the VBO there works
    // fine.
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    glRM->makeCurrent();

    // Upload vertex list to vertex buffer.
    const QString graticuleRequestKey = QString("graticule_vertices_actor#")
                                        + QString::number(getID());
    uploadVec2ToVertexBuffer(verticesGraticule, graticuleRequestKey,
                             &graticuleVertexBuffer);


    // Generate graticule labels.
    // ==========================
    MTextManager* tm = glRM->getTextManager();

    // Remove all text labels of the old geometry (MActor method).
    removeAllLabels();

    // Get properties for label font size and colour and bounding box.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);

    QVector<float> longitudeLabels = parseFloatRangeString(
                properties->mString()->value(longitudeLabelsProperty));
    QVector<float> latitudeLabels = parseFloatRangeString(
                properties->mString()->value(latitudeLabelsProperty));

    // Label positions are found by creating a "proxy graticule" for each
    // meridian, then after projection and clipping placing the label
    // at the first polygon vertex (i.e. boundary of the bbox).
    for (float lonLabel : longitudeLabels)
    {
        QVector<float> singleLongitudeLabel;
        singleLongitudeLabel << lonLabel;

        QVector<QPolygonF> proxyGraticule =
                geo.generate2DGraticuleGeometry(
                    singleLongitudeLabel, latitudeLabels, graticuleSpacing);

        proxyGraticule = projectAndClipGeometry(
                    &geo, proxyGraticule, bbox, rotatedGridMaxSegmentLength_deg);

        for (QPolygonF proxyPolygon : proxyGraticule)
        {
            QPointF labelPosition = proxyPolygon.first();
            labels.append(tm->addText(
                              QString("%1%2").arg(lonLabel).arg(
                                  lonLabel >= 0 ? "E" : "W"),
                              MTextManager::LONLATP,
                              labelPosition.x(), labelPosition.y(),
                              verticalPosition_hPa,
                              labelsize, labelColour,
                              MTextManager::BASELINECENTRE,
                              labelbbox, labelBBoxColour)
                          );
        }
    }

    // Parallels' labels are positioned at the last vertex of each projected
    // clipped polygon.
    for (float latLabel : latitudeLabels)
    {
        QVector<float> singleLatitudeLabel;
        singleLatitudeLabel << latLabel;

        QVector<QPolygonF> proxyGraticule =
                geo.generate2DGraticuleGeometry(
                    longitudeLabels, singleLatitudeLabel, graticuleSpacing);

        proxyGraticule = projectAndClipGeometry(
                    &geo, proxyGraticule, bbox, rotatedGridMaxSegmentLength_deg);

        for (QPolygonF proxyPolygon : proxyGraticule)
        {
            QPointF labelPosition = proxyPolygon.last();
            labels.append(tm->addText(
                              QString("%1%2").arg(latLabel).arg(
                                  latLabel >= 0 ? "N" : "S"),
                              MTextManager::LONLATP,
                              labelPosition.x(), labelPosition.y(),
                              verticalPosition_hPa,
                              labelsize, labelColour,
                              MTextManager::BASELINECENTRE,
                              labelbbox, labelBBoxColour)
                          );
        }
    }


    // Read coastline geometry from shapefile.
    // =======================================

    // For projection and clippling to work correctly, we load coastline and
    // borderline geometry on the entire globe, then clip to the bounding
    // box after projection. Performance seems to be accaptable (mr, 28Oct2020).
    QRectF geometryLimits = QRectF(-180., -90., 360., 180.);

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    QString coastfile = sysMC->getApplicationConfigurationValue(
                "geometry_shapefile_coastlines").toString();
    QVector<QPolygonF> coastlines = geo.read2DGeometryFromShapefile(
                expandEnvironmentVariables(coastfile), geometryLimits);

    coastlines = projectAndClipGeometry(
                &geo, coastlines, bbox, rotatedGridMaxSegmentLength_deg);

    QVector<QVector2D> verticesCoastlines;
    coastlineStartIndices.clear();
    coastlineVertexCount.clear();
    geo.flattenPolygonsToVertexList(coastlines,
                                    &verticesCoastlines,
                                    &coastlineStartIndices,
                                    &coastlineVertexCount);

    const QString coastRequestKey = "graticule_coastlines_actor#"
                                    + QString::number(getID());
    uploadVec2ToVertexBuffer(verticesCoastlines, coastRequestKey,
                             &coastlineVertexBuffer);


    // Read borderline geometry from shapefile.
    // ========================================
    QString borderfile = sysMC->getApplicationConfigurationValue(
                "geometry_shapefile_borderlines").toString();
    QVector<QPolygonF> borderlines = geo.read2DGeometryFromShapefile(
                expandEnvironmentVariables(borderfile), geometryLimits);

    borderlines = projectAndClipGeometry(
                &geo, borderlines, bbox, rotatedGridMaxSegmentLength_deg);

    QVector<QVector2D> verticesBorderlines;
    borderlineStartIndices.clear();
    borderlineVertexCount.clear();
    geo.flattenPolygonsToVertexList(borderlines,
                                    &verticesBorderlines,
                                    &borderlineStartIndices,
                                    &borderlineVertexCount);

    const QString borderRequestKey = "graticule_borderlines_actor#"
                                     + QString::number(getID());
    uploadVec2ToVertexBuffer(verticesBorderlines, borderRequestKey,
                             &borderlineVertexBuffer);

    LOG4CPLUS_DEBUG(mlog, "Graticule and coast-/borderline geometry was generated.");
}


QVector<QPolygonF> MGraticuleActor::projectAndClipGeometry(
        MGeometryHandling *geo,
        QVector<QPolygonF> geometry, QRectF bbox,
        double rotatedGridMaxSegmentLength_deg)
{
    // Projection-dependent operations.
    if (mapProjection == MAPPROJECTION_CYLINDRICAL)
    {
        // Cylindrical projections may display bounding boxed outside the
        // -180..180 degrees range, hence enlarge the geometry if required.
        geometry = geo->enlargeGeometryToBBoxIfNecessary(geometry, bbox);
    }
    else if (mapProjection == MAPPROJECTION_PROJ_LIBRARY)
    {
        geometry = geo->geographicalToProjectedCoordinates(geometry);
        //OPENISSUE (mr, 26Oct2020) -- is this safe with all projections or
        // do specific projections also lead to erroneous projected line
        // segments as those that occur with rotated lon-lat projections
        // (fixed using splitLineSegmentsLongerThanThreshold() below)?
    }
    else if (mapProjection == MAPPROJECTION_ROTATEDLATLON)
    {
        geometry = geo->geographicalToRotatedCoordinates(geometry);
        geometry = geo->splitLineSegmentsLongerThanThreshold(
                    geometry, rotatedGridMaxSegmentLength_deg);
    }

    // Clip line geometry to the bounding box that is rendered.
    geometry = geo->clipPolygons(geometry, bbox);

    return geometry;
}

} // namespace Met3D
