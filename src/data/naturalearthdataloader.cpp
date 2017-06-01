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
#include "naturalearthdataloader.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <QVector>
#include <QVector2D>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"

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
                                               bool                append)
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

    // Create a bounding box geometry: NOTE that this needs to be polygon -- if
    // a line string or ring is used, the Intersection() method used below will
    // only return the points that actually intersect the line, i.e. that are
    // on the line.
    float llcrnrlon = bbox.x();
    float llcrnrlat = bbox.y();
    float urcrnrlon = bbox.x() + bbox.width();
    float urcrnrlat = bbox.y() + bbox.height();

    OGRLinearRing bboxRing;
    bboxRing.addPoint(llcrnrlon, llcrnrlat);
    bboxRing.addPoint(urcrnrlon, llcrnrlat);
    bboxRing.addPoint(urcrnrlon, urcrnrlat);
    bboxRing.addPoint(llcrnrlon, urcrnrlat);
    bboxRing.addPoint(llcrnrlon, llcrnrlat);
    // OGRPolygon *bboxPolygon = new OGRPolygon(); causes problems on windows
	OGRPolygon *bboxPolygon = dynamic_cast<OGRPolygon*>
		(OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbPolygon));
    bboxPolygon->addRing(&bboxRing);

    // Filter the layer on-load: Only load those geometries that intersect
    // with the bounding box.
    layer->SetSpatialFilter(bboxPolygon);

    // Loop over all features contained in the layer.
    layer->ResetReading();
    OGRFeature *feature;
    while ((feature = layer->GetNextFeature()) != NULL)
    {
        startIndices->append(vertices->size());

        // Get the geometry associated with the current feature.
        OGRGeometry *geometry;
        geometry = feature->GetGeometryRef();

        // For coastlines, borderlines etc. we are only interested in line
        // string features. Note that all lines that intersect with the
        // bounding box are returned here. This includes lines that lie only
        // partially within the bounding box. Hence we have to compute the
        // intersection of each line with the bounding box.

        // If the geometry is of type wkbLineString or wkbMultiLineString,
        // place the contained line strings in a list with data to be
        // processed.
        QList<OGRLineString*> lineStrings;
        if (geometry != NULL)
        {
            OGRwkbGeometryType gType = wkbFlatten(geometry->getGeometryType());

            if (gType == wkbLineString)
            {
                lineStrings.append((OGRLineString *) geometry);
            }

            else if (gType == wkbMultiLineString)
            {
                OGRGeometryCollection *gc = (OGRGeometryCollection *) geometry;
                for (int g = 0; g < gc->getNumGeometries(); g++)
                    lineStrings.append((OGRLineString *) gc->getGeometryRef(g));
            }

        } // geometry != NULL

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
                    vertices->append(QVector2D(v[i].x, v[i].y));
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
                        vertices->append(QVector2D(v[i].x, v[i].y));
                    delete[] v;
                }
            }

        } // for (lineStrings)

        OGRFeature::DestroyFeature(feature);
        count->append(vertices->size() - startIndices->last());
    }

    // Clean up.
	OGRGeometryFactory::destroyGeometry(bboxPolygon);
}

} // namespace Met3D
