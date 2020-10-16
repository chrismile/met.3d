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
#include "geometryhandling.h"

// standard library imports
#include <iostream>
#include <math.h>

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <QVector>
#include <QVector2D>
#include <QPolygonF>
#include <QPainterPath>
#include <QPainterPathStroker>

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

MGeometryHandling::MGeometryHandling()
    : pjDstProjection(nullptr),
      pjSrcProjection(nullptr)
{

}


MGeometryHandling::~MGeometryHandling()
{
    destroyProjProjection();
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QVector<QPolygonF> MGeometryHandling::generate2DGraticuleGeometry(
        QVector2D lonLatStart, QVector2D lonLatEnd,
        QVector2D lonLatLineSpacing, QVector2D lonLatVertexSpacing)
{
    QVector<QPolygonF> graticuleGeometry;

    // Generate meridians (lines of constant longitude).
    for (float lon = lonLatStart.x(); lon <= lonLatEnd.x();
         lon += lonLatLineSpacing.x())
    {
        QPolygonF meridian;
        for (float lat = lonLatStart.y(); lat <= lonLatEnd.y();
             lat += lonLatVertexSpacing.y())
        {
            meridian << QPointF(lon, lat);
        }
        graticuleGeometry << meridian;
    }

    // Generate parallels (lines of constant latitude).
    for (float lat = lonLatStart.y(); lat <= lonLatEnd.y();
         lat += lonLatLineSpacing.y())
    {
        QPolygonF parallel;
        for (float lon = lonLatStart.x(); lon <= lonLatEnd.x();
             lon += lonLatVertexSpacing.x())
        {
            parallel << QPointF(lon, lat);
        }
        graticuleGeometry << parallel;
    }

    return graticuleGeometry;
}


void MGeometryHandling::initProjProjection(QString projString)
{
    // Initialize proj-Projection.
    // See https://proj.org/development/reference/deprecated.html
//TODO (mr, 16Oct2020) -- replace by new proj API.
    destroyProjProjection();
    pjDstProjection = pj_init_plus(projString.toStdString().c_str());
    pjSrcProjection = pj_init_plus("+proj=latlong");
}


void MGeometryHandling::destroyProjProjection()
{
    if (pjDstProjection) pj_free(pjDstProjection);
    if (pjSrcProjection) pj_free(pjSrcProjection);
}


QPointF MGeometryHandling::geographicalToProjectedCoordinates(QPointF point)
{
    int errorCode;
    double lon = degreesToRadians(point.x());
    double lat = degreesToRadians(point.y());

    errorCode = pj_transform(pjSrcProjection, pjDstProjection,
                             1, 1, &lon, &lat, NULL);

    if (errorCode)
    {
        // In later version of proj, const char* proj_errno_string(int err)
        // is available to get the text corresponding to the error code to.
        LOG4CPLUS_ERROR(mlog, "Error " << errorCode <<
                        "encountered during transformation using Proj library\n");
    }

    return QPointF(lon / MetConstants::scaleFactorToFitProjectedCoordsTo360Range,
                   lat / MetConstants::scaleFactorToFitProjectedCoordsTo360Range);
}


QVector<QPolygonF> MGeometryHandling::geographicalToProjectedCoordinates(
        QVector<QPolygonF> polygons)
{
    QVector<QPolygonF> projectedPolygons;

    for (QPolygonF polygon : polygons)
    {
        QPolygonF projectedPolygon;
        for (QPointF vertex : polygon)
        {
            projectedPolygon << geographicalToProjectedCoordinates(vertex);
        }
        projectedPolygons << projectedPolygon;
    }

    return projectedPolygons;
}


QVector<QPolygonF> MGeometryHandling::clipPolygons(
        QVector<QPolygonF> polygons, QRectF bbox)
{
    QVector<QPolygonF> clippedPolygons;

    for (QPolygonF polygon : polygons)
    {
        QPolygonF clippedPolygon;

        // Loop over each line segment in current polygon.
        for (int i = 0; i < polygon.size()-1; i++)
        {
            // Obtain the two points that make up the segment.
            QPointF p1 = polygon.at(i);
            QPointF p2 = polygon.at(i+1);

            // Clip segment against bbox. If (at least a part of the) segment
            // is maintained, add to the clipped polygon.
            if (cohenSutherlandClip(&p1, &p2, bbox))
            {
                if (clippedPolygon.isEmpty())
                {
                    // If this is the first segment in the clipped polygon,
                    // add both points, unless they're equal.
                    if (p1 == p2)
                    {
                        clippedPolygon << p2;
                    }
                    else
                    {
                        clippedPolygon << p1 << p2;
                    }
                }
                else if (p1 == clippedPolygon.last())
                {
                    // Last point in clippedPolygon equal to first point in
                    // current line segment? If yes append only last point
                    // current segment. If not, start a new polygon.
                    if (p2 != clippedPolygon.last())
                    {
                        clippedPolygon << p2;
                    }
                }
                else
                {
                    clippedPolygons << clippedPolygon;
                    clippedPolygon = QPolygonF();
                    clippedPolygon << p1 << p2;
                }
            }
        }

        if (!clippedPolygon.isEmpty())
        {
            clippedPolygons << clippedPolygon;
        }
    }

    return clippedPolygons;
}


void MGeometryHandling::flattenPolygonsToVertexList(
        QVector<QPolygonF> polygons, QVector<QVector2D> *vertexList,
        QVector<int> *polygonStartIndices, QVector<int> *polygonVertexCount)
{
    for (QPolygonF polygon : polygons)
    {
        if (polygon.isEmpty()) continue;

        *polygonStartIndices << vertexList->size();
        *polygonVertexCount << polygon.size();

        for (QPointF point : polygon)
        {
            *vertexList << QVector2D(point);
        }
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

// The following implementation of the Cohen-Sutherland Clipping algorithm
// is based on code from this website:
// https://www.geeksforgeeks.org/line-clipping-set-1-cohen-sutherland-algorithm/

// Region codes for Cohen-Sutherland clipping.
const int INSIDE = 0; // 0000
const int LEFT = 1; // 0001
const int RIGHT = 2; // 0010
const int BOTTOM = 4; // 0100
const int TOP = 8; // 1000

int MGeometryHandling::cohenSutherlandCode(QPointF &point, QRectF &bbox) const
{
    // Initialize as being inside.
    int code = INSIDE;

    if (point.x() < bbox.x()) // to the left of rectangle
    {
        code |= LEFT;
    }
    else if (point.x() > bbox.x()+bbox.width()) // to the right of rectangle
    {
        code |= RIGHT;
    }
    if (point.y() < bbox.y()) // below the rectangle
    {
        code |= BOTTOM;
    }
    else if (point.y() > bbox.y()+bbox.height()) // above the rectangle
    {
        code |= TOP;
    }

    return code;
}


bool MGeometryHandling::cohenSutherlandClip(
        QPointF *p1, QPointF *p2, QRectF &bbox)
{
    // Compute region codes for P1, P2
    int code1 = cohenSutherlandCode(*p1, bbox);
    int code2 = cohenSutherlandCode(*p2, bbox);

    // Initialize line as outside the rectangular window
    bool accept = false;

    while (true)
    {
        if ((code1 == 0) && (code2 == 0))
        {
            // If both endpoints lie within rectangle
            accept = true;
            break;
        }
        else if (code1 & code2)
        {
            // If both endpoints are outside rectangle,
            // in same region
            break;
        }
        else
        {
            // Some segment of line lies within the
            // rectangle
            int codeOut;
            double x, y;

            // At least one endpoint is outside the
            // rectangle, pick it.
            if (code1 != 0)
            {
                codeOut = code1;
            }
            else
            {
                codeOut = code2;
            }

            // Find intersection point;
            // using formulas y = y1 + slope * (x - x1),
            // x = x1 + (1 / slope) * (y - y1)
            if (codeOut & TOP)
            {
                // point is above the clip rectangle
                // NOTE: QRectF has reversed y-axis, hence top here is
                // bbox.bottom()..
                x = p1->x() + (p2->x() - p1->x()) * (bbox.bottom() - p1->y()) /
                        (p2->y() - p1->y());
                y = bbox.bottom();
            }
            else if (codeOut & BOTTOM)
            {
                // point is below the rectangle
                // NOTE: QRectF has reversed y-axis, hence bottom here is
                // bbox.top()..
                x = p1->x() + (p2->x() - p1->x()) * (bbox.top() - p1->y()) /
                        (p2->y() - p1->y());
                y = bbox.top();
            }
            else if (codeOut & RIGHT)
            {
                // point is to the right of rectangle
                y = p1->y() + (p2->y() - p1->y()) * (bbox.right() - p1->x()) /
                        (p2->x() - p1->x());
                x = bbox.right();
            }
            else if (codeOut & LEFT)
            {
                // point is to the left of rectangle
                y = p1->y() + (p2->y() - p1->y()) * (bbox.left() - p1->x()) /
                        (p2->x() - p1->x());
                x = bbox.left();
            }

            // Now intersection point x, y is found
            // We replace point outside rectangle
            // by intersection point
            if (codeOut == code1)
            {
                p1->setX(x); // x1 = x;
                p1->setY(y); // y1 = y;
                code1 = cohenSutherlandCode(*p1, bbox);
            }
            else
            {
                p2->setX(x); // x2 = x;
                p2->setY(y); // y2 = y;
                code2 = cohenSutherlandCode(*p2, bbox);
            }
        }
    }

    return accept;
}


} // namespace Met3D
