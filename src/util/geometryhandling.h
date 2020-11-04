/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020 Marc Rautenhaus
**
**  Regional Computing Center, Visual Data Analysis Group
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
#ifndef GEOMETRYHANDLING_H
#define GEOMETRYHANDLING_H

// standard library imports

// related third party imports
#include <QtCore>
#include "ogrsf_frmts.h"
#include "cpl_error.h"
#ifdef ACCEPT_USE_OF_DEPRECATED_PROJ_API_H
#include <proj_api.h>
#endif

// local application imports


namespace Met3D
{

/**

 */
class MGeometryHandling
{
public:
    MGeometryHandling();
    virtual ~MGeometryHandling();

    QVector<QPolygonF> generate2DGraticuleGeometry(
            QVector<float> longitudes, QVector<float> latitudes,
            QVector2D lonLatVertexSpacing);

    QVector<QPolygonF> read2DGeometryFromShapefile(QString fname, QRectF bbox);

    void initProjProjection(QString projString);

    void destroyProjProjection();

    QPointF geographicalToProjectedCoordinates(QPointF point);

    QVector<QPolygonF> geographicalToProjectedCoordinates(
            QVector<QPolygonF> polygons);

    void initRotatedLonLatProjection(QPointF rotatedPoleLonLat);

    QPointF geographicalToRotatedCoordinates(QPointF point);

    QVector<QPolygonF> geographicalToRotatedCoordinates(
            QVector<QPolygonF> polygons);

    QPointF rotatedToGeographicalCoordinates(QPointF point);

    QVector<QPolygonF> splitLineSegmentsLongerThanThreshold(
            QVector<QPolygonF> polygons, double thresholdDistance);

    QVector<QPolygonF> enlargeGeometryToBBoxIfNecessary(
            QVector<QPolygonF> polygons, QRectF bbox);

    QVector<QPolygonF> clipPolygons(QVector<QPolygonF> polygons, QRectF bbox);

    void flattenPolygonsToVertexList(
            QVector<QPolygonF> polygons,
            QVector<QVector2D> *vertexList,
            QVector<int> *polygonStartIndices,
            QVector<int> *polygonVertexCount);

private:
    int cohenSutherlandCode(QPointF &point, QRectF &bbox) const;

    bool cohenSutherlandClip(QPointF *p1, QPointF *p2, QRectF &bbox);

    OGRPolygon* convertQRectToOGRPolygon(QRectF &rect);

    QPolygonF convertOGRLineStringToQPolygon(OGRLineString *lineString);

    void appendOGRLineStringsFromOGRGeometry(QList<OGRLineString*> *lineStrings,
                                             OGRGeometry *geometry);

    projPJ pjDstProjection, pjSrcProjection;
    QPointF rotatedPole;
};

} // namespace Met3D

#endif // GEOMETRYHANDLING_H
