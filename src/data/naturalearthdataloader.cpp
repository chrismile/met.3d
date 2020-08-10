/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2015-2017 Bianca Tost [+]
**  Copyright 2020-     Kameswarrao Modali [*]
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
**
**  + Computer Graphics and Visualization Group
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
#include "naturalearthdataloader.h"

#define _USE_MATH_DEFINES

// standard library imports
#include <iostream>
#include <math.h>

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <QVector>
#include <QVector2D>

#ifdef ACCEPT_USE_OF_DEPRECATED_PROJ_API_H
#include <proj_api.h>
#endif

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "util/metroutines.h"

using namespace std;


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNaturalEarthDataLoader::MNaturalEarthDataLoader()
{
    GDALAllRegister();
}


MNaturalEarthDataLoader::~MNaturalEarthDataLoader()
{
    for (int i = 0; i < gdalDataSet.size(); i++) GDALClose(gdalDataSet[i]);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MNaturalEarthDataLoader::setDataSources(
        QString coastlinesfile, QString borderlinesfile)
{
    // Remove existing datasources.
    for (int i = 0; i < gdalDataSet.size(); i++) GDALClose(gdalDataSet[i]);

    // We currently have 2 data sources.
    gdalDataSet.resize(2);

    // Open the coastlines shapefile.
    gdalDataSet[COASTLINES] = (GDALDataset*) GDALOpenEx(
                coastlinesfile.toStdString().c_str(),
                GDAL_OF_VECTOR, NULL, NULL, NULL);
    if (gdalDataSet[COASTLINES] == NULL)
    {
        QString msg = QString("ERROR: cannot open coastlines file %1")
                .arg(coastlinesfile);
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }

    gdalDataSet[BORDERLINES] = (GDALDataset*) GDALOpenEx(
                borderlinesfile.toStdString().c_str(),
                GDAL_OF_VECTOR, NULL, NULL, NULL);
    if (gdalDataSet[BORDERLINES] == NULL)
    {
        QString msg = QString("ERROR: cannot open borderlines file %1")
                .arg(borderlinesfile);
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }
}


void MNaturalEarthDataLoader::loadLineGeometry(GeometryType        type,
                                               QRectF              bbox,
                                               QVector<QVector2D> *vertices,
                                               QVector<int>       *startIndices,
                                               QVector<int>       *count,
                                               bool                append,
                                               double              offset,
                                               int                 shiftedCopies)
{
    if (gdalDataSet.size() < 2)
    {
        QString msg = QString("ERROR: NaturalEarthDataLoader not yet "
                              "initilised.");
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }

    string typeStr = type == COASTLINES ? "COASTLINES": "BORDERLINES";
    LOG4CPLUS_DEBUG(mlog, "loading " << typeStr << " geometry..");

    if ( !append )
    {
        vertices->clear();
        startIndices->clear();;
        count->clear();
    }

    // Starting points of the arrays which might be copied later. (Since the
    // startIndices array is not copied but filled with new values, we don't
    // need its starting point.)
    int verticesStart = vertices->size();
    int countStart = count->size();

    // NaturalEarth shapefiles only contain one layer. (Do shapefiles in
    // general contain only one layer?)
    OGRLayer *layer;
    layer = gdalDataSet[type]->GetLayer(0);

    OGRPolygon *bboxPolygon = getBBoxPolygon(&bbox);

    // Filter the layer on-load: Only load those geometries that intersect
    // with the bounding box.
    layer->SetSpatialFilter(bboxPolygon);

    // Loop over all features contained in the layer.
    layer->ResetReading();
    OGRFeature *feature;
    while ((feature = layer->GetNextFeature()) != NULL)
    {
        startIndices->append(vertices->size());

        QList<OGRLineString*> lineStrings;
        // Get the geometry associated with the current feature.
        getLineStringFeatures(&lineStrings, feature->GetGeometryRef());

        // Loop over the list and intersect the contained line strings with
        // the bounding box.
        for (int l = 0; l < lineStrings.size(); l++)
        {
            OGRLineString *lineString = lineStrings.at(l);

            // Compute intersection with bbox.
            OGRGeometry *iGeometry = lineString->Intersection(bboxPolygon);

            // The intersection can be either a single line string, or a
            // collection of line strings.

            if (iGeometry->getGeometryType() == wkbLineString)
            {
                // Get all points from the intersected line string and append
                // them to the "vertices" vector.
                OGRLineString *iLine = (OGRLineString *) iGeometry;
                int numLinePoints = iLine->getNumPoints();
                OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                iLine->getPoints(v);
                for (int i = 0; i < numLinePoints; i++)
                {
                    vertices->append(QVector2D(v[i].x + offset, v[i].y));
                }
                delete[] v;
            }
            else if (iGeometry->getGeometryType() == wkbMultiLineString)
            {
                // Loop over all line strings in the collection, appending
                // their points to "vertices" as above.
                OGRGeometryCollection *geomCollection =
                        (OGRGeometryCollection *) iGeometry;

                for (int g = 0; g < geomCollection->getNumGeometries(); g++)
                {
                    OGRLineString *iLine =
                            (OGRLineString *) geomCollection->getGeometryRef(g);
                    int numLinePoints = iLine->getNumPoints();
                    OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                    iLine->getPoints(v);
                    for (int i = 0; i < numLinePoints; i++)
                    {
                        vertices->append(QVector2D(v[i].x + offset, v[i].y));
                    }
                    delete[] v;
                }
            }

        } // for (lineStrings)

        OGRFeature::DestroyFeature(feature);
        count->append(vertices->size() - startIndices->last());
    }

    int countSize = count->size();
    int bound = count->size();
    // Offset of the copy with regard to the geometry loaded in the first step.
    double localOffset = 360.;
    for (int i = 0; i < shiftedCopies; ++i)
    {
        // Reset vertex index to the starting position for each copy.
        int v = verticesStart;
        for (int c = countStart; c < countSize; ++c)
        {
            count->append(count->at(c));
            bound = v + count->at(c);
            startIndices->append(vertices->size());
            for (; v < bound; ++v)
            {
                vertices->append(QVector2D(vertices->at(v).x() + localOffset,
                                           vertices->at(v).y()));
            }
        }
        localOffset += 360.;
    }
    // Clean up.
	OGRGeometryFactory::destroyGeometry(bboxPolygon);
}


void MNaturalEarthDataLoader::loadCyclicLineGeometry(
        MNaturalEarthDataLoader::GeometryType geometryType,
        QRectF cornerRect,
        QVector<QVector2D> *vertices,
        QVector<int> *startIndices,
        QVector<int> *vertexCount)
{
    // Region parameters.
    float westernLon = cornerRect.x();
    float easternLon = cornerRect.x() + cornerRect.width();
    float width = cornerRect.width();
    width = min(360.0f - MMOD(westernLon + 180.f, 360.f), width);
    // Offset which needs to be added to place the [westmost] region correctly.
    double offset = static_cast<double>(floor((westernLon + 180.f) / 360.f)
                                        * 360.f);
    // Load geometry of westmost region separately only if its width is smaller
    // than 360 degrees (i.e. "not complete") otherwise skip this first step.
    bool firstStep = width < 360.f;
    if (firstStep)
    {
        cornerRect.setX(MMOD(westernLon + 180.f, 360.f) - 180.f);
        cornerRect.setWidth(width);
        loadLineGeometry(geometryType, cornerRect, vertices, startIndices,
                         vertexCount, false,  // clear vectors
                         offset);     // shift
        // Increment offset to suit the next region.
        offset += 360.;
        // "Shift" westernLon to western border of the bounding box domain not
        // treated yet.
        westernLon += width;
    }

    // Amount of regions with a width of 360 degrees.
    int completeRegionsCount =
            static_cast<int>((easternLon - westernLon) / 360.f);
    // Load "complete" regions only if we have at least one. If the first step
    // was skipped, we need to clear the vectors before loading the line
    // geometry otherwise we need to append the computed vertices.
    if (completeRegionsCount > 0)
    {
        cornerRect.setX(-180.f);
        cornerRect.setWidth(360.f);
        loadLineGeometry(geometryType, cornerRect, vertices, startIndices,
                    vertexCount, firstStep,//clear vectors if it wasn't done before
                    offset, // shift
                    completeRegionsCount - 1);
        // "Shift" westernLon to western border of the bounding box domain not
        // treated yet.
        westernLon += static_cast<float>(completeRegionsCount) * 360.f;
        // Increment offset to suit the last region if one is left.
        offset += static_cast<double>(completeRegionsCount) * 360.;
    }

    // Load geometry of eastmost region separately only if it isn't the same as
    // the westmost region and its width is smaller than 360 degrees and thus it
    // wasn't loaded in one of the steps before.
    if (westernLon < easternLon)
    {
        cornerRect.setX(-180.f);
        cornerRect.setWidth(easternLon - westernLon);
        loadLineGeometry(geometryType, cornerRect, vertices, startIndices,
                         vertexCount, true,    // append to vectors
                         offset); // shift
    }

//WORKAROUND/CHECKME (km&mr, 30Mar2020) --
    // When loading natural earth coast and borderline data, some incorrect
    // (long) lines appear. These are charcterized by large jumps in the
    // coordinates between successive points in a given group of vertices. To
    // avoid these jumps, as long as there exists a group with maximum distance
    // between two successive points greater than an (arbitrarily defined)
    // "lon-lat-distance" of 10 deg, keep subdividing the group into smaller
    // vertex groups. This workaround eliminates the large jumps (greater than
    // graticule deltalatlon) and thereby also the incorrect lines.
    restrictDistanceBetweenSubsequentVertices(vertices, startIndices,
                                              vertexCount, 10.);
}


void MNaturalEarthDataLoader::restrictDistanceBetweenSubsequentVertices(
        QVector<QVector2D> *vertices,
        QVector<int> *startIndices,
        QVector<int> *vertexCount,
        float maxAllowedDistance_deg)
{
    // Loop over all groups (number of groups = entries in e.g. vertexCount;
    // if new groups are added that size is automatically updated).
    for (int iGroup = 0; iGroup < vertexCount->count(); iGroup++)
    {
        int vertexStartIndex = startIndices->at(iGroup);
        int vertexEndIndex = startIndices->at(iGroup) + vertexCount->at(iGroup) - 1;

        for (int iVertex = vertexStartIndex; iVertex < vertexEndIndex; iVertex++)
        {
            float distance = vertices->at(iVertex).distanceToPoint(vertices->at(iVertex + 1));

            if (distance > maxAllowedDistance_deg)
            {
                // Split the group.

                LOG4CPLUS_WARN(mlog, "WARNING: While loading coastline and borderline "
                               << "geometry, connected vertices with a spacing "
                               << "of " << distance << " (max. allowed is "
                               << maxAllowedDistance_deg << " deg) were discovered. "
                               << "The connection is classified as incorrect and eliminated.");

                // Update the current, now shortened, group's 'vertexCount'.
                int shortenedGroupVertexCount = iVertex - vertexStartIndex + 1;
                int remainingVertexCount = vertexCount->at(iGroup)
                        - shortenedGroupVertexCount;
                vertexCount->replace(iGroup, shortenedGroupVertexCount);

                // Inserting a new group at group index 'i+1' into the two
                // relevant arrays.
                int iGroupNew = iGroup + 1;
                startIndices->insert(iGroupNew, iVertex + 1);
                vertexCount->insert(iGroupNew, remainingVertexCount);

                // Break inner loop over vertices and continue with loop over
                // groups -- this will pick up the new group as the next group
                // and continue checking.
                break;
            }
        }
    }

    return;
}


void MNaturalEarthDataLoader::loadAndRotateLineGeometry(
        GeometryType type, QRectF bbox, QVector<QVector2D> *vertices,
        QVector<int> *startIndices, QVector<int> *count, bool append,
        double poleLat, double poleLon)
{
    if (gdalDataSet.size() < 2)
    {
        QString msg = QString("ERROR: NaturalEarthDataLoader not yet "
                              "initilised.");
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }

    string typeStr = type == COASTLINES ? "COASTLINES": "BORDERLINES";
    LOG4CPLUS_DEBUG(mlog, "loading " << typeStr << " geometry..");

    if ( !append )
    {
        vertices->clear();
        startIndices->clear();;
        count->clear();
    }

    // NaturalEarth shapefiles only contain one layer. (Do shapefiles in
    // general contain only one layer?)
    OGRLayer *layer;
    layer = gdalDataSet[type]->GetLayer(0);

    // Create bounding box with coordinates mapped to the range [-180, 180] in
    // longitude without "wrapping" the bounding box around (i.e. the east
    // border must not be smaller than the west border). We apply this mapping
    // since the rotation maps all values to this range and the line geometries
    // are only defined on this range.
    QRectF bboxTransformed = bbox;
    bboxTransformed.setX(MMOD(bbox.x() + 180., 360.) - 180.);
    bboxTransformed.setWidth(min(bbox.width(), 180. - bboxTransformed.x()));
    OGRPolygon *bboxPolygon = getBBoxPolygon(&bboxTransformed);

    // Variables used to get rid of lines crossing the whole domain.
    // (Conntection of the right most and the left most vertex)
    QVector2D prevPosition(0., 0.);
    QVector2D currPosition(0., 0.);
    QVector2D centreLons(0., 0.);

    OGRPoint *point = new OGRPoint();

    getCentreLons(&centreLons, poleLat, poleLon);

    // Filter the layer on-load: Only load those geometries that intersect
    // with the bounding box.
    layer->SetSpatialFilter(bboxPolygon);

    // Loop over all features contained in the layer.
    layer->ResetReading();
    OGRFeature *feature;

    while ((feature = layer->GetNextFeature()) != NULL)
    {
        startIndices->append(vertices->size());
        prevPosition.setX(0.);

        QList<OGRLineString*> lineStrings;
        // Get the geometry associated with the current feature.
        getLineStringFeatures(&lineStrings, feature->GetGeometryRef());

        // Loop over the list and intersect the contained line strings with
        // the bounding box.
        for (int l = 0; l < lineStrings.size(); l++)
        {
            OGRLineString *lineString = lineStrings.at(l);

            // Compute intersection with bbox.
            OGRGeometry *iGeometry = lineString->Intersection(bboxPolygon);

            // The intersection can be either a single line string, or a
            // collection of line strings.

            if (iGeometry->getGeometryType() == wkbLineString)
            {
                // Get all points from the intersected line string and append
                // them to the "vertices" vector.
                OGRLineString *iLine = (OGRLineString *) iGeometry;
                int numLinePoints = iLine->getNumPoints();
                OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                iLine->getPoints(v);

                for (int i = 0; i < numLinePoints; i++)
                {
                    // Rotate the current point.
                    point->setX(v[i].x);
                    point->setY(v[i].y);
                    if (!validConnectionBetweenPositions(
                                &prevPosition, &currPosition, point,
                                poleLat, poleLon, &centreLons))
                    {
                        // Start new line.
                        count->append(vertices->size() - startIndices->last());
                        startIndices->append(vertices->size());
                    }
                    vertices->append(currPosition);
                }
                delete[] v;
            }

            else if (iGeometry->getGeometryType() == wkbMultiLineString)
            {
                // Loop over all line strings in the collection, appending
                // their points to "vertices" as above.
                OGRGeometryCollection *geomCollection =
                        (OGRGeometryCollection *) iGeometry;

                for (int g = 0; g < geomCollection->getNumGeometries(); g++)
                {
                    OGRLineString *iLine =
                            (OGRLineString *) geomCollection->getGeometryRef(g);
                    int numLinePoints = iLine->getNumPoints();
                    OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                    iLine->getPoints(v);

                    for (int i = 0; i < numLinePoints; i++)
                    {
                        point->setX(v[i].x);
                        point->setY(v[i].y);
                        if (!validConnectionBetweenPositions(
                                    &prevPosition, &currPosition, point,
                                    poleLat, poleLon, &centreLons))
                        {
                            // Start new line.
                            count->append(vertices->size() - startIndices->last());
                            startIndices->append(vertices->size());
                        }
                        vertices->append(currPosition);
                    }
                    delete[] v;
                }
            }
        } // while (lineStrings)

        OGRFeature::DestroyFeature(feature);
        count->append(vertices->size() - startIndices->last());
    }

    // Clean up.
    OGRGeometryFactory::destroyGeometry(bboxPolygon);
    delete point;

    // If we have loaded only a part of the line geometry, load the missing
    // geometry by calling the method again with an adapted bounding box.
    // This might happen if the bounding box "falls appart" into two segments
    // when mapped to the range [-180, 180] in longitude with regard to sphere
    // coordinates.
    if (bbox.width() > bboxTransformed.width())
    {
        double width = bboxTransformed.width();
        bboxTransformed.setX(-180.);
        bboxTransformed.setWidth(min(bbox.width(), 360.) - width);
        loadAndRotateLineGeometry(
                type, bboxTransformed, vertices, startIndices, count, true,
                poleLat, poleLon);

    }
}


void MNaturalEarthDataLoader::loadAndRotateLineGeometryUsingRotatedBBox(
        GeometryType type, QRectF bbox, QVector<QVector2D> *vertices,
        QVector<int> *startIndices, QVector<int> *count, bool append,
        double poleLat, double poleLon)
{
    if (gdalDataSet.size() < 2)
    {
        QString msg = QString("ERROR: NaturalEarthDataLoader not yet "
                              "initilised.");
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }

    string typeStr = type == COASTLINES ? "COASTLINES": "BORDERLINES";
    LOG4CPLUS_DEBUG(mlog, "loading " << typeStr << " geometry..");

    if ( !append )
    {
        vertices->clear();
        startIndices->clear();;
        count->clear();
    }

    // NaturalEarth shapefiles only contain one layer. (Do shapefiles in
    // general contain only one layer?)
    OGRLayer *layer;
    layer = gdalDataSet[type]->GetLayer(0);

    OGRPolygon *bboxPolygonRot = getBBoxPolygon(&bbox);

    QRectF bbox2(-180., -90., 360., 180);
    OGRPolygon *bboxPolygon = getBBoxPolygon(&bbox2);

    // Filter the layer on-load: Only load those geometries that intersect
    // with the bounding box. (Somehow it does not load the whole geometry if
    // we don't set the filter, thus it is necessary to set the filter to a
    // polygon covering the whole region.)
    layer->SetSpatialFilter(bboxPolygon);

    OGRPoint *point = new OGRPoint;

    QList<OGRLineString*> lineStringList;
    OGRLineString * lineString;

    // Variables used to get rid of lines crossing the whole domain.
    // (Conntection of the right most and the left most vertex)
    QVector2D prevPosition(0., 0.);
    QVector2D currPosition(0., 0.);
    QVector2D centreLons(0., 0.);

    getCentreLons(&centreLons, poleLat, poleLon);

    // Loop over all features contained in the layer.
    layer->ResetReading();
    OGRFeature *feature;
    while ((feature = layer->GetNextFeature()) != NULL)
    {
        startIndices->append(vertices->size());

        QList<OGRLineString*> lineStrings;
        // Get the geometry associated with the current feature.
        getLineStringFeatures(&lineStrings, feature->GetGeometryRef());

        // Loop over the list, rotate each vertex of the current line and check
        // for connections from the right domain side to the left. Separate the
        // line to two lines at these connections. Afterwards intersect the set
        // of lines gotten with the bounding box.
        for (int l = 0; l < lineStrings.size(); l++)
        {
            OGRLineString *originalLineString = lineStrings.at(l);
            // Use string list to distinguish between different lines since
            // rotation of the coast- and borderlines can lead to lines
            // crossing the whole domain (connection between right side and
            // left side).
            lineStringList.append(new OGRLineString());
            lineString = lineStringList.at(0);
            originalLineString->getPoint(0, point);
            geographicalToRotatedCoords(point, poleLat, poleLon);
            prevPosition.setX(point->getX());
            // For rotation loop over all vertices of the current lineString,
            // apply the rotation and store the point in a new line string.
            for (int i = 0; i < originalLineString->getNumPoints(); i++)
            {
                originalLineString->getPoint(i, point);
                if (!validConnectionBetweenPositions(
                            &prevPosition, &currPosition, point,
                            poleLat, poleLon, &centreLons))
                {
                    // Start new line.
                    lineStringList.append(new OGRLineString());
                    lineString = lineStringList.last();
                }
                lineString->addPoint(currPosition.x(), currPosition.y());
            }

            // Loop over all separated lines and intersect each with the
            // bounding box.
            foreach (lineString, lineStringList)
            {
                // Only use valid lines with more than one vertex.
                if (lineString->getNumPoints() <= 1 || !lineString->IsValid())
                {
                    count->append(vertices->size() - startIndices->last());
                    startIndices->append(vertices->size());
                    delete lineString;
                    continue;
                }

                // Compute intersection with bbox.
                OGRGeometry *iGeometry = lineString->Intersection(bboxPolygonRot);

                // The intersection can be either a single line string, or a
                // collection of line strings.

                if (iGeometry->getGeometryType() == wkbLineString)
                {
                    // Get all points from the intersected line string and append
                    // them to the "vertices" vector.
                    OGRLineString *iLine = (OGRLineString *) iGeometry;
                    int numLinePoints = iLine->getNumPoints();
                    OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                    iLine->getPoints(v);
                    for (int i = 0; i < numLinePoints; i++)
                    {
                        vertices->append(QVector2D(v[i].x, v[i].y));
                    }
                    delete[] v;
                }
                else if (iGeometry->getGeometryType() == wkbMultiLineString)
                {
                    // Loop over all line strings in the collection, appending
                    // their points to "vertices" as above.
                    OGRGeometryCollection *geomCollection =
                            (OGRGeometryCollection *) iGeometry;

                    for (int g = 0; g < geomCollection->getNumGeometries(); g++)
                    {
                        OGRLineString *iLine =
                                (OGRLineString *) geomCollection->getGeometryRef(g);
                        int numLinePoints = iLine->getNumPoints();
                        OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                        iLine->getPoints(v);
                        for (int i = 0; i < numLinePoints; i++)
                        {
                            vertices->append(QVector2D(v[i].x, v[i].y));
                        }
                        delete[] v;
                        // Restart after each line segment to avoid connections
                        // between line segements separated by intersection
                        // with bounding box.
                        count->append(vertices->size() - startIndices->last());
                        startIndices->append(vertices->size());
                    }
                }
                count->append(vertices->size() - startIndices->last());
                startIndices->append(vertices->size());
                delete lineString;
            }
            lineStringList.clear();
        } // while (lineStrings)

        OGRFeature::DestroyFeature(feature);
        count->append(vertices->size() - startIndices->last());
    }

    // Clean up.
    OGRGeometryFactory::destroyGeometry(bboxPolygon);
    OGRGeometryFactory::destroyGeometry(bboxPolygonRot);
    delete point;
}


void MNaturalEarthDataLoader::loadAndTransformProjectedLineGeometryAndCutUsingBBox(
        GeometryType type, QRectF bbox, QVector<QVector2D> *vertices,
        QVector<int> *startIndices, QVector<int> *count, bool append,
        QString proj4_string)
{
    if (gdalDataSet.size() < 2)
    {
        QString msg = QString("ERROR: NaturalEarthDataLoader not yet "
                              "initilised.");
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }

    string typeStr = type == COASTLINES ? "COASTLINES": "BORDERLINES";
    LOG4CPLUS_DEBUG(mlog, "loading " << typeStr << " geometry..");

    if ( !append )
    {
        vertices->clear();
        startIndices->clear();;
        count->clear();
    }

    // NaturalEarth shapefiles only contain one layer. (Do shapefiles in
    // general contain only one layer?)
    OGRLayer *layer;
    layer = gdalDataSet[type]->GetLayer(0);

    OGRPolygon *bboxPolygonRot = getBBoxPolygon(&bbox);

    // Filter the layer on-load: Only load those geometries that intersect
    // with the bounding box. (Somehow it does not load the whole geometry if
    // we don't set the filter, thus it is necessary to set the filter to a
    // polygon covering the whole region.)
    QRectF bboxGlobal(-180., -90., 360., 180.);
    OGRPolygon *bboxPolygon = getBBoxPolygon(&bboxGlobal);
    layer->SetSpatialFilter(bboxPolygon);

    OGRPoint *point = new OGRPoint;

    QList<OGRLineString*> lineStringList;
    OGRLineString * lineString;

    // Variables used to get rid of lines crossing the whole domain.
    // (Conntection of the right most and the left most vertex)

    // Loop over all features contained in the layer.
    layer->ResetReading();
    OGRFeature *feature;
    while ((feature = layer->GetNextFeature()) != NULL)
    {
        startIndices->append(vertices->size());

        QList<OGRLineString*> lineStrings;
        // Get the geometry associated with the current feature.
        getLineStringFeatures(&lineStrings, feature->GetGeometryRef());

        // Loop over the list, rotate each vertex of the current line and check
        // for connections from the right domain side to the left. Separate the
        // line to two lines at these connections. Afterwards intersect the set
        // of lines gotten with the bounding box.
        for (int l = 0; l < lineStrings.size(); l++)
        {
            OGRLineString *originalLineString = lineStrings.at(l);
            // Use string list to distinguish between different lines since
            // rotation of the coast- and borderlines can lead to lines
            // crossing the whole domain (connection between right side and
            // left side).
            lineStringList.append(new OGRLineString());
            lineString = lineStringList.at(0);

            originalLineString->getPoint(0, point);

            QVector<QVector2D> lonLatPoint;
            QVector<QVector2D> projectedPoint;
            lonLatPoint.append(QVector2D(point->getX(),point->getY()));

            projectedPoint = projectGeographicalLatLonCoords(
                        lonLatPoint, proj4_string);

            // For rotation loop over all vertices of the current lineString,
            // apply the rotation and store the point in a new line string.
            for (int i = 0; i < originalLineString->getNumPoints(); i++)
            {
                originalLineString->getPoint(i, point);
                QVector<QVector2D> lonLatpoint;
                QVector<QVector2D> projectedPoint;
                lonLatpoint.append(QVector2D(point->getX(),point->getY()));

                projectedPoint = projectGeographicalLatLonCoords(
                            lonLatpoint, proj4_string);

                // Start new line.
                lineStringList.append(new OGRLineString());
                lineString = lineStringList.last();
                lineString->addPoint(projectedPoint[0].x(), projectedPoint[0].y());

                //MKM  addthe point also to the previous line string as second point.
                lineString = lineStringList.at(lineStringList.count()-2);
                lineString->addPoint(projectedPoint[0].x(), projectedPoint[0].y());
            }

            // Loop over all separated lines and intersect each with the
            // bounding box.
            foreach (lineString, lineStringList)
            {
                // Only use valid lines with more than one vertex.
                if (lineString->getNumPoints() <= 1 || !lineString->IsValid())
                {
                    count->append(vertices->size() - startIndices->last());
                    startIndices->append(vertices->size());
                    delete lineString;
                    continue;
                }

                // Compute intersection with bbox.
                OGRGeometry *iGeometry = lineString->Intersection(bboxPolygonRot);

                // The intersection can be either a single line string, or a
                // collection of line strings.

                if (iGeometry->getGeometryType() == wkbLineString)
                {
                    // Get all points from the intersected line string and append
                    // them to the "vertices" vector.
                    OGRLineString *iLine = (OGRLineString *) iGeometry;
                    int numLinePoints = iLine->getNumPoints();
                    OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                    iLine->getPoints(v);
                    for (int i = 0; i < numLinePoints; i++)
                    {
                        vertices->append(QVector2D(v[i].x, v[i].y));
                    }
                    delete[] v;
                }
                else if (iGeometry->getGeometryType() == wkbMultiLineString)
                {
                    // Loop over all line strings in the collection, appending
                    // their points to "vertices" as above.
                    OGRGeometryCollection *geomCollection =
                            (OGRGeometryCollection *) iGeometry;

                    for (int g = 0; g < geomCollection->getNumGeometries(); g++)
                    {
                        OGRLineString *iLine =
                                (OGRLineString *) geomCollection->getGeometryRef(g);
                        int numLinePoints = iLine->getNumPoints();
                        OGRRawPoint *v = new OGRRawPoint[numLinePoints];
                        iLine->getPoints(v);
                        for (int i = 0; i < numLinePoints; i++)
                        {
                            vertices->append(QVector2D(v[i].x, v[i].y));
                        }
                        delete[] v;
                        // Restart after each line segment to avoid connections
                        // between line segements separated by intersection
                        // with bounding box.
                        count->append(vertices->size() - startIndices->last());
                        startIndices->append(vertices->size());
                    }
                }
                count->append(vertices->size() - startIndices->last());
                startIndices->append(vertices->size());
                delete lineString;
            }
            lineStringList.clear();
        } // while (lineStrings)

        OGRFeature::DestroyFeature(feature);
        count->append(vertices->size() - startIndices->last());
    }

    // Clean up.
    //OGRGeometryFactory::destroyGeometry(bboxPolygon);
    OGRGeometryFactory::destroyGeometry(bboxPolygonRot);
    delete point;
}


static const double DEG2RAD = M_PI / 180.0;
static const double RAD2DEG = 180.0 / M_PI;

// Parts of the following method have been ported from the C implementation of
// the methods 'lam_to_lamrot' and 'phi_to_phirot'. The original code has been
// published under GNU GENERAL PUBLIC LICENSE Version 2, June 1991.
// source: https://code.zmaw.de/projects/cdo/files  [Version 1.8.1]

// Original code:

// static
// double lam_to_lamrot(double phi, double rla, double polphi, double pollam)
// {
//   /*
//     Umrechnung von rla (geo. System) auf rlas (rot. System)

//     phi    : Breite im geographischen System (N>0)
//     rla    : Laenge im geographischen System (E>0)
//     polphi : Geographische Breite des Nordpols des rot. Systems
//     pollam : Geographische Laenge des Nordpols des rot. Systems

//     result : Rotierte Laenge
//   */
//   double zsinpol = sin(DEG2RAD*polphi);
//   double zcospol = cos(DEG2RAD*polphi);
//   double zlampol =     DEG2RAD*pollam;

//   if ( rla > 180.0 ) rla -= 360.0;

//   double zrla = DEG2RAD*rla;
//   double zphi = DEG2RAD*phi;

//   double zarg1  = - sin(zrla-zlampol)*cos(zphi);
//   double zarg2  = - zsinpol*cos(zphi)*cos(zrla-zlampol)+zcospol*sin(zphi);

//   if ( fabs(zarg2) < 1.0e-20 ) zarg2 = 1.0e-20;

//   return RAD2DEG*atan2(zarg1,zarg2);
// }

// static
// double phi_to_phirot(double phi, double rla, double polphi, double pollam)
// {
//   /*
//     Umrechnung von phi (geo. System) auf phis (rot. System)

//     phi    : Breite im geographischen System (N>0)
//     rla    : Laenge im geographischen System (E>0)
//     polphi : Geographische Breite des Nordpols des rot. Systems
//     pollam : Geographische Laenge des Nordpols des rot. Systems

//     result : Rotierte Breite
//   */
//   double zsinpol = sin(DEG2RAD*polphi);
//   double zcospol = cos(DEG2RAD*polphi);
//   double zlampol =     DEG2RAD*pollam;

//   double zphi = DEG2RAD*phi;
//   if ( rla > 180.0 ) rla -= 360.0;
//   double zrla = DEG2RAD*rla;

//   double zarg = zcospol*cos(zphi)*cos(zrla-zlampol) + zsinpol*sin(zphi);

//   return RAD2DEG*asin(zarg);
// }

bool MNaturalEarthDataLoader::geographicalToRotatedCoords(
        OGRPoint *point, double poleLat, double poleLon)
{
    // Early break for rotation values with no effect.
    if ((poleLon == -180. || poleLon == 180.) && poleLat == 90.)
    {
        return false;
    }

    // Get longitude and latitude from point.
    double lon = point->getX();
    double lat = point->getY();

    if ( lon > 180.0 )
    {
        lon -= 360.0;
    }

    // Convert degrees to radians.
    double poleLatRad = DEG2RAD * poleLat;
    double poleLonRad = DEG2RAD * poleLon;
    double lonRad = DEG2RAD * lon;
    double latRad = DEG2RAD * lat;

    // Compute sinus and cosinus of some coordinates since they are needed more
    // often later on.
    double sinPoleLat = sin(poleLatRad);
    double cosPoleLat = cos(poleLatRad);

    // Apply the transformation (conversation to Cartesian coordinates and  two
    // rotations; difference to original code: no use of pollam).

    double x = ((-sinPoleLat) * cos(latRad) * cos(lonRad - poleLonRad))
            + (cosPoleLat * sin(latRad));
    double y = (-sin(lonRad - poleLonRad)) * cos(latRad);
    double z = (cosPoleLat * cos(latRad) * cos(lonRad - poleLonRad))
            + (sinPoleLat * sin(latRad));

    // Avoid invalid values for z (Might occure due to inaccuracies in
    // computations).
    z = max(-1., min(1., z));

    // Too small values can lead to numerical problems in method atans2.
    if ( std::abs(x) < 1.0e-20 )
    {
        x = 1.0e-20;
    }

    // Compute spherical coordinates from Cartesian coordinates and convert
    // radians to degrees.

    point->setX(RAD2DEG * (atan2(y, x)));
    point->setY(RAD2DEG * (asin(z)));

    return true;
}


// Parts of the following method have been ported from the C implementation of
// the methods 'lamrot_to_lam' and 'phirot_to_phi'. The original code has been
// published under GNU GENERAL PUBLIC LICENSE Version 2, June 1991.
// source: https://code.zmaw.de/projects/cdo/files  [Version 1.8.1]
// Necessary code duplicate in basemap.fx.glsl .

// Original code:

// double lamrot_to_lam(double phirot, double lamrot, double polphi, double pollam, double polgam)
// {
//   /*
//     This function converts lambda from one rotated system to lambda in another system.
//     If the optional argument polgam is present, the other system can also be a rotated one,
//     where polgam is the angle between the two north poles.
//     If polgam is not present, the other system is the real geographical system.

//     phirot : latitude in the rotated system
//     lamrot : longitude in the rotated system (E>0)
//     polphi : latitude of the rotated north pole
//     pollam : longitude of the rotated north pole

//     result : longitude in the geographical system
//   */
//   double zarg1, zarg2;
//   double zgam;
//   double result = 0;

//   double zsinpol = sin(DEG2RAD*polphi);
//   double zcospol = cos(DEG2RAD*polphi);

//   double zlampol = DEG2RAD*pollam;
//   double zphirot = DEG2RAD*phirot;
//   if ( lamrot > 180.0 ) lamrot -= 360.0;
//   double zlamrot = DEG2RAD*lamrot;

//   if ( fabs(polgam) > 0 )
//     {
//       zgam  = -DEG2RAD*polgam;
//       zarg1 = sin(zlampol) *
//  	     (- zsinpol*cos(zphirot) * (cos(zlamrot)*cos(zgam) - sin(zlamrot)*sin(zgam))
//  	      + zcospol*sin(zphirot))
// 	 - cos(zlampol)*cos(zphirot) * (sin(zlamrot)*cos(zgam) + cos(zlamrot)*sin(zgam));

//       zarg2 = cos(zlampol) *
//  	     (- zsinpol*cos(zphirot) * (cos(zlamrot)*cos(zgam) - sin(zlamrot)*sin(zgam))
//	      + zcospol*sin(zphirot))
//	 + sin(zlampol)*cos(zphirot) * (sin(zlamrot)*cos(zgam) + cos(zlamrot)*sin(zgam));
//      }
//   else
//     {
//       zarg1 = sin(zlampol)*(- zsinpol*cos(zlamrot)*cos(zphirot)  +
//      		               zcospol*             sin(zphirot)) -
//	       cos(zlampol)*           sin(zlamrot)*cos(zphirot);
//       zarg2 = cos(zlampol)*(- zsinpol*cos(zlamrot)*cos(zphirot)  +
//                               zcospol*             sin(zphirot)) +
//               sin(zlampol)*           sin(zlamrot)*cos(zphirot);
//     }

//   if ( fabs(zarg2) > 0 ) result = RAD2DEG*atan2(zarg1, zarg2);
//   if ( fabs(result) < 9.e-14 ) result = 0;

//   return result;
// }

// double phirot_to_phi(double phirot, double lamrot, double polphi, double polgam)
// {
//   /*
//     This function converts phi from one rotated system to phi in another
//     system. If the optional argument polgam is present, the other system
//     can also be a rotated one, where polgam is the angle between the two
//     north poles.
//     If polgam is not present, the other system is the real geographical
//     system.

//     phirot : latitude in the rotated system
//     lamrot : longitude in the rotated system (E>0)
//     polphi : latitude of the rotated north pole
//     polgam : angle between the north poles of the systems

//     result : latitude in the geographical system
//   */
//   double zarg;
//   double zgam;

//   double zsinpol = sin(DEG2RAD*polphi);
//   double zcospol = cos(DEG2RAD*polphi);

//   double zphirot   = DEG2RAD*phirot;
//   if ( lamrot > 180.0 ) lamrot -= 360.0;
//   double zlamrot   = DEG2RAD*lamrot;

//   if ( fabs(polgam) > 0 )
//     {
//       zgam = -DEG2RAD*polgam;
//       zarg = zsinpol*sin(zphirot) +
//              zcospol*cos(zphirot)*(cos(zlamrot)*cos(zgam) - sin(zgam)*sin(zlamrot));
//     }
//   else
//     zarg   = zcospol*cos(zphirot)*cos(zlamrot) + zsinpol*sin(zphirot);

//   return RAD2DEG*asin(zarg);
// }


bool MNaturalEarthDataLoader::rotatedToGeograhpicalCoords(OGRPoint *point,
                                                          double poleLat,
                                                          double poleLon)
{
    // Early break for rotation values with no effect.
    if ((poleLon == -180. || poleLon == 180.) && poleLat == 90.)
    {
        return false;
    }

    double result = 0;

    // Get longitude and latitude from point.
    double rotLon = point->getX();
    double rotLat = point->getY();

    if ( rotLon > 180.0 )
    {
        rotLon -= 360.0;
    }

    // Convert degrees to radians.
    double poleLatRad = DEG2RAD * poleLat;
    double poleLonRad = DEG2RAD * poleLon;
    double rotLonRad = DEG2RAD * rotLon;

    // Compute sinus and cosinus of some coordinates since they are needed more
    // often later on.
    double sinPoleLat = sin(poleLatRad);
    double cosPoleLat = cos(poleLatRad);
    double sinRotLatRad = sin(DEG2RAD * rotLat);
    double cosRotLatRad = cos(DEG2RAD * rotLat);
    double cosRotLonRad = cos(DEG2RAD * rotLon);

    // Apply the transformation (conversation to Cartesian coordinates and  two
    // rotations; difference to original code: no use of polgam).

    double x =
            (cos(poleLonRad) * (((-sinPoleLat) * cosRotLonRad * cosRotLatRad)
                                + (cosPoleLat * sinRotLatRad)))
            + (sin(poleLonRad) * sin(rotLonRad) * cosRotLatRad);
    double y =
            (sin(poleLonRad) * (((-sinPoleLat) * cosRotLonRad * cosRotLatRad)
                                + (cosPoleLat * sinRotLatRad)))
            - (cos(poleLonRad) * sin(rotLonRad) * cosRotLatRad);
    double z = cosPoleLat * cosRotLatRad * cosRotLonRad
            + sinPoleLat * sinRotLatRad;

    // Avoid invalid values for z (Might occure due to inaccuracies in
    // computations).
    z = max(-1., min(1., z));

    // Compute spherical coordinates from Cartesian coordinates and convert
    // radians to degrees.

    if ( std::abs(x) > 0 )
    {
        result = RAD2DEG * atan2(y, x);
    }
    if ( std::abs(result) < 9.e-14 )
    {
        result = 0;
    }

    point->setX(result);
    point->setY(RAD2DEG * (asin(z)));
    return true;
}


double MNaturalEarthDataLoader::getNearestLon(
        double toLon1, double toLon2, double lon1, double lon2)
{
    double dist1 = abs(lon2 - toLon1);
    double dist2 = abs(lon2 + 360. - toLon1);
    double dist3 = abs(lon1 - toLon1);
    double dist4 = abs(lon1 + 360. - toLon1);

    double dist5 = abs(lon2 - toLon2);
    double dist6 = abs(lon2 + 360. - toLon2);
    double dist7 = abs(lon1 - toLon2);
    double dist8 = abs(lon1 + 360. - toLon2);

    double distMin = min(min(dist1, dist2), min(dist3, dist4));
    distMin = min(distMin, min(min(dist5, dist6), min(dist7, dist8)));
    if (distMin == dist1 || distMin == dist2 || distMin == dist5
            || distMin == dist6)
    {
        return lon2;
    }
    return lon1;
}


void MNaturalEarthDataLoader::getCentreLons(
        QVector2D *centreLons, double poleLat, double poleLon)
{
    OGRPoint *point = new OGRPoint;
    // Get pair of longitudes consiting of the only two longitudes projected to
    // the longitude coordinates 0, -180 and 180 by rotated north pole
    // projection.
    if (int(poleLat - 90) % 180 == 0)
    {
        // Special case: Applying the revert projection to (0., 90.) and
        // (0., -90.) would result in a projection to the poles and this
        // does not define the both longitudes correctly.
        point->setX(0.);
        point->setY(0.);
        MNaturalEarthDataLoader::rotatedToGeograhpicalCoords(point, poleLat,
                                                         poleLon);
        centreLons->setX(point->getX());
        if (centreLons->x() < 0)
        {
            centreLons->setY(point->getX() + 180.);
        }
        else
        {
            centreLons->setY(point->getX() - 180.);
        }
    }
    else
    {
        point->setX(0.);
        point->setY(90.);
        MNaturalEarthDataLoader::rotatedToGeograhpicalCoords(point, poleLat,
                                                         poleLon);
        centreLons->setX(point->getX());
        point->setX(0.);
        point->setY(-90.);
        MNaturalEarthDataLoader::rotatedToGeograhpicalCoords(point, poleLat,
                                                         poleLon);
        centreLons->setY(point->getX());
    }
    delete point;
}


QVector<QVector2D> MNaturalEarthDataLoader::projectGeographicalLatLonCoords(
        QVector<QVector2D> verticesVector, QString projString)
{
    // define output array
    QVector<QVector2D> projectedVerticesVector;
    double currentLat, currentLon;
    float projectionCoordsScaleFactor =
            MetConstants::scaleFactorToFitProjectedCoordsTo360Range;
    int errorCodeProjLibrary;

    // Proj4 variables.
    projPJ pjProj, pjLatlon;
    pjProj = pj_init_plus(projString.toStdString().c_str());
    pjLatlon = pj_init_plus("+proj=latlong");

    for (int i = 0; i <verticesVector.size() ; i++)
    {
        QVector2D projectedPoint;

        currentLat = verticesVector[i].y();
        currentLat = degreesToRadians(currentLat);

        currentLon = verticesVector[i].x();
        currentLon = degreesToRadians(currentLon);

        errorCodeProjLibrary = pj_transform(pjLatlon, pjProj, 1, 1, &currentLon, &currentLat, NULL);

        projectedPoint.setX(currentLon/
                            (projectionCoordsScaleFactor * projectionCoordsScaleFactor));
        projectedPoint.setY(currentLat/
                            (projectionCoordsScaleFactor * projectionCoordsScaleFactor));
        projectedVerticesVector.append(projectedPoint);

        if (errorCodeProjLibrary)
        {
            // In later version of proj, const char* proj_errno_string(int err)
            // is available to get the text corresponding to the error code to.
            LOG4CPLUS_ERROR(mlog, "Error " << errorCodeProjLibrary <<
                            "encountered during transformation using Proj library\n");
        }
    }
    return projectedVerticesVector;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

OGRPolygon *MNaturalEarthDataLoader::getBBoxPolygon(QRectF *bbox)
{
    // Create a bounding box geometry: NOTE that this needs to be polygon -- if
    // a line string or ring is used, the Intersection() method used below will
    // only return the points that actually intersect the line, i.e. that are
    // on the line.
    float leftlon = bbox->x();
    float lowerlat = bbox->y();
    float rightlon = bbox->x() + bbox->width();
    float upperlat = bbox->y() + bbox->height();

    OGRLinearRing bboxRing;
    bboxRing.addPoint(leftlon, lowerlat);
    bboxRing.addPoint(rightlon, lowerlat);
    bboxRing.addPoint(rightlon, upperlat);
    bboxRing.addPoint(leftlon, upperlat);
    bboxRing.addPoint(leftlon, lowerlat);
    // OGRPolygon *bboxPolygon = new OGRPolygon(); causes problems on windows
    OGRPolygon *bboxPolygon = dynamic_cast<OGRPolygon*>
        (OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbPolygon));
    bboxPolygon->addRing(&bboxRing);
    return bboxPolygon;
}


void MNaturalEarthDataLoader::getLineStringFeatures(
        QList<OGRLineString*> *lineStrings, OGRGeometry *geometry)
{
    // For coastlines, borderlines etc. we are only interested in line
    // string features. Note that all lines that intersect with the
    // bounding box are returned here. This includes lines that lie only
    // partially within the bounding box. Hence we have to compute the
    // intersection of each line with the bounding box.

    // If the geometry is of type wkbLineString or wkbMultiLineString,
    // place the contained line strings in a list with data to be
    // processed.
    if (geometry != NULL)
    {
        OGRwkbGeometryType gType = wkbFlatten(geometry->getGeometryType());

        if (gType == wkbLineString)
        {
            lineStrings->append((OGRLineString *) geometry);
        }

        else if (gType == wkbMultiLineString)
        {
            OGRGeometryCollection *gc = (OGRGeometryCollection *) geometry;
            for (int g = 0; g < gc->getNumGeometries(); g++)
                lineStrings->append((OGRLineString *) gc->getGeometryRef(g));
        }

    } // geometry != NULL
}


bool MNaturalEarthDataLoader::validConnectionBetweenPositions(
        QVector2D *prevPosition, QVector2D *currPosition, OGRPoint *point,
        double poleLat, double poleLon, QVector2D *centreLons)
{
    bool result = true;

    geographicalToRotatedCoords(point, poleLat, poleLon);
    currPosition->setX(point->getX());
    currPosition->setY(point->getY());
    // Check if connection between previous and current vertex crosses 0
    // (middle) since this might be a connection from left to right crossing
    // (nearly) the whole domain.
    if (((currPosition->x() >= 0. && prevPosition->x() <= 0.)
    || (currPosition->x() <= 0. && prevPosition->x() >= 0.)))
    {
        // "Normalise" coordinates of current vertex by reverting the projection
        // since it makes it easier to compare the coordinates. (Projection and
        // revert projection map to domain [-180,180].)
        rotatedToGeograhpicalCoords(point, poleLat, poleLon);
        double lonNorm = point->getX();
        double latNorm = point->getY();
        // "Normalise" coordinates of previous vertex by reverting the
        // projection since it makes it easier to compare the coordinates.
        point->setX(prevPosition->x());
        point->setY(prevPosition->y());
        rotatedToGeograhpicalCoords(point, poleLat, poleLon);
        double prevLon = point->getX();
        // Get centre longitude with overall shortest distance in longitudes to
        // either the current or previous vertex.
        double centreLon = getNearestLon(lonNorm, prevLon, centreLons->x(),
                                         centreLons->y());

        // Rotate vertex on the nearest centre longitude and the latitude of the
        // current node.

        point->setX(centreLon);
        point->setY(latNorm);
        geographicalToRotatedCoords(point, poleLat, poleLon);

        // If the rotated vertex maps to the centre (= 0) than the connection
        // between the current vertex and the previous is legitimate.
        if (int(point->getX()) != 0)
        {
            result = false;
        }
    }
    prevPosition->setX(currPosition->x());
    prevPosition->setY(currPosition->y());
    return result;
}

} // namespace Met3D
