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
      pjSrcProjection(nullptr),
      rotatedPole(QPointF(0., 90.))
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
        QVector<float> longitudes, QVector<float> latitudes,
        QVector2D lonLatVertexSpacing)
{
    QVector<QPolygonF> graticuleGeometry;

    // Non-empty lists of lons and lats are required.
    if (longitudes.isEmpty() || latitudes.isEmpty())
    {
        return graticuleGeometry;
    }
    // Positive vertex spacing is required.
    if (lonLatVertexSpacing.x() <= 0.) lonLatVertexSpacing.setX(1.);
    if (lonLatVertexSpacing.y() <= 0.) lonLatVertexSpacing.setY(1.);

    // List of lons and lats needs to be in ascending order.
    qSort(longitudes);
    qSort(latitudes);

    // Generate meridians (lines of constant longitude).
    if (latitudes.size() > 1)
    {
        for (float lon : longitudes)
        {
            QPolygonF meridian;
            for (float lat = latitudes.first(); lat <= latitudes.last();
                 lat += lonLatVertexSpacing.y())
            {
                meridian << QPointF(lon, lat);
            }
            graticuleGeometry << meridian;
        }
    }

    // Generate parallels (lines of constant latitude).
    if (longitudes.size() > 1)
    {
        for (float lat : latitudes)
        {
            QPolygonF parallel;
            for (float lon = longitudes.first(); lon <= longitudes.last();
                 lon += lonLatVertexSpacing.x())
            {
                parallel << QPointF(lon, lat);
            }
            graticuleGeometry << parallel;
        }
    }

    return graticuleGeometry;
}


QVector<QPolygonF> MGeometryHandling::read2DGeometryFromShapefile(
        QString fname, QRectF bbox)
{
    LOG4CPLUS_DEBUG(mlog, "Loading shapefile geometry from file "
                    << fname.toStdString() << "...");

    QVector<QPolygonF> polygons;

    // Open Shapefile.
    GDALDataset* gdalDataSet = (GDALDataset*) GDALOpenEx(
                fname.toStdString().c_str(),
                GDAL_OF_VECTOR, NULL, NULL, NULL);

    if (gdalDataSet == NULL)
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: cannot open shapefile "
                        << fname.toStdString() << ".");
        return polygons;
    }

    // NaturalEarth shapefiles only contain one layer. (Do shapefiles in
    // general contain only one layer?)
    OGRLayer *layer;
    layer = gdalDataSet->GetLayer(0);

    // Filter the layer on-load: Only load those geometries that intersect
    // with the bounding box.
    OGRPolygon *bboxPolygon = convertQRectToOGRPolygon(bbox);
    layer->SetSpatialFilter(bboxPolygon);

    // Loop over all features contained in the layer, add all OGRLineStrings
    // to our list of polygons.
    layer->ResetReading();
    OGRFeature *feature;
    while ((feature = layer->GetNextFeature()) != NULL)
    {
        QList<OGRLineString*> lineStrings;
        // Get the geometry associated with the current feature.
        appendOGRLineStringsFromOGRGeometry(&lineStrings, feature->GetGeometryRef());

        // Create QPolygons from the OGRLineStrings.
        for (OGRLineString *lineString : lineStrings)
        {
            polygons << convertOGRLineStringToQPolygon(lineString);
        }

        OGRFeature::DestroyFeature(feature);
    }

    // Clean up.
    OGRGeometryFactory::destroyGeometry(bboxPolygon);
    GDALClose(gdalDataSet);

    LOG4CPLUS_DEBUG(mlog, "Geometry from shapefile "
                    << fname.toStdString() << " has been loaded.");

    return polygons;
}


void MGeometryHandling::initProjProjection(QString projString)
{
    // Initialize proj-Projection.
    // See https://proj.org/development/reference/deprecated.html
//TODO (mr, 16Oct2020) -- replace by new proj API.
    destroyProjProjection();
    pjDstProjection = pj_init_plus(projString.toStdString().c_str());
    if (!pjDstProjection)
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: cannot initialize proj with definition '"
                        << projString.toStdString() << "' (dst); error is '"
                        << pj_strerrno(pj_errno) << "'.");
    }

    // Source coordinate system for coordinates is assumed to be simple
    // lon/lat in WGS84, e.g. NaturalEarth:
    //   https://www.naturalearthdata.com/features/
    QString srcProjString = "+proj=latlong +ellps=WGS84";
    pjSrcProjection = pj_init_plus(srcProjString.toStdString().c_str());
    if (!pjSrcProjection)
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: cannot initialize proj with definition '"
                        << srcProjString.toStdString() << " (src)'; error is '"
                        << pj_strerrno(pj_errno) << "'.");
    }
}


void MGeometryHandling::destroyProjProjection()
{
    if (pjDstProjection) pj_free(pjDstProjection);
    if (pjSrcProjection) pj_free(pjSrcProjection);
}


QPointF MGeometryHandling::geographicalToProjectedCoordinates(
        QPointF point, bool inverse)
{
    // For tests:
    // https://mygeodata.cloud/cs2cs/

    if (!pjSrcProjection || !pjDstProjection)
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: proj library not initialized, cannot "
                              "project geographical coordinates.");
        return QPointF();
    }

    int errorCode;
    double lon_x = 0.;
    double lat_y = 0.;

    if (inverse)
    {
        // Inverse projection: projected to geographical coordinates.
        lon_x = point.x() * MetConstants::scaleFactorToFitProjectedCoordsTo360Range;
        lat_y = point.y() * MetConstants::scaleFactorToFitProjectedCoordsTo360Range;
        errorCode = pj_transform(pjDstProjection, pjSrcProjection,
                                 1, 1, &lon_x, &lat_y, NULL);
        lon_x = radiansToDegrees(lon_x);
        lat_y = radiansToDegrees(lat_y);
    }
    else
    {
        // Geographical to projected coordinates.
        lon_x = degreesToRadians(point.x());
        lat_y = degreesToRadians(point.y());
        errorCode = pj_transform(pjSrcProjection, pjDstProjection,
                                 1, 1, &lon_x, &lat_y, NULL);
        lon_x /= MetConstants::scaleFactorToFitProjectedCoordsTo360Range;
        lat_y /= MetConstants::scaleFactorToFitProjectedCoordsTo360Range;
    }

    if (errorCode)
    {
        // In later version of proj, const char* proj_errno_string(int err)
        // is available to get the text corresponding to the error code to.
        LOG4CPLUS_ERROR(mlog, "ERROR: proj transformation of point ("
                        << point.x() << ", " << point.y()
                        << ") failed with error '"
                        << pj_strerrno(errorCode)
                        << "'. Returning (0., 0.).");
        return QPointF();
    }

    return QPointF(lon_x, lat_y);
}


QVector<QPolygonF> MGeometryHandling::geographicalToProjectedCoordinates(
        QVector<QPolygonF> polygons, bool inverse)
{
    QVector<QPolygonF> projectedPolygons;

    for (QPolygonF polygon : polygons)
    {
        QPolygonF projectedPolygon;
        for (QPointF vertex : polygon)
        {
            projectedPolygon << geographicalToProjectedCoordinates(
                                    vertex, inverse);
        }
        projectedPolygons << projectedPolygon;
    }

    return projectedPolygons;
}


void MGeometryHandling::initRotatedLonLatProjection(QPointF rotatedPoleLonLat)
{
    rotatedPole = rotatedPoleLonLat;
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


QPointF MGeometryHandling::geographicalToRotatedCoordinates(QPointF point)
{
    // Early break for rotation values with no effect.
    double poleLon = rotatedPole.x();
    double poleLat = rotatedPole.y();
    if ((poleLon == -180. || poleLon == 180.) && poleLat == 90.)
    {
        return QPointF();
    }

    // Get longitude and latitude from point.
    double lon = point.x();
    double lat = point.y();

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
    return QPointF(RAD2DEG * (atan2(y, x)), RAD2DEG * (asin(z)));
}


QVector<QPolygonF> MGeometryHandling::geographicalToRotatedCoordinates(
        QVector<QPolygonF> polygons)
{
    QVector<QPolygonF> projectedPolygons;

    for (QPolygonF polygon : polygons)
    {
        QPolygonF projectedPolygon;
        for (QPointF vertex : polygon)
        {
            projectedPolygon << geographicalToRotatedCoordinates(vertex);
        }
        projectedPolygons << projectedPolygon;
    }

    return projectedPolygons;
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


QPointF MGeometryHandling::rotatedToGeographicalCoordinates(QPointF point)
{
    // Early break for rotation values with no effect.
    double poleLon = rotatedPole.x();
    double poleLat = rotatedPole.y();
    if ((poleLon == -180. || poleLon == 180.) && poleLat == 90.)
    {
        return QPointF();
    }

    double resultLon = 0.;

    // Get longitude and latitude from point.
    double rotLon = point.x();
    double rotLat = point.y();

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
        resultLon = RAD2DEG * atan2(y, x);
    }
    if ( std::abs(resultLon) < 9.e-14 )
    {
        resultLon = 0.;
    }

    return QPointF(resultLon, RAD2DEG * (asin(z)));
}


QVector<QPolygonF> MGeometryHandling::splitLineSegmentsLongerThanThreshold(
        QVector<QPolygonF> polygons, double thresholdDistance)
{
    QVector<QPolygonF> revisedPolygons;

    for (QPolygonF polygon : polygons)
    {
        QPolygonF revisedPolygon;
        for (QPointF vertex : polygon)
        {
            // The revised polygon is empty? Add this vertex.
            if (revisedPolygon.isEmpty())
            {
                revisedPolygon << vertex;
            }
            // Otherwise, check if the distance between this vertex and the
            // previous one is smaller than the threshold. If yes, add this
            // vertex.
            else if (QVector2D(revisedPolygon.last() - vertex).length()
                     < thresholdDistance)
            {
                revisedPolygon << vertex;
            }
            // If the distance is larger than the threshold split into two
            // polygons.
            else
            {
                revisedPolygons << revisedPolygon;
                revisedPolygon = QPolygonF();
                revisedPolygon << vertex;
            }
        }
        revisedPolygons << revisedPolygon;
    }

    return revisedPolygons;
}


QVector<QPolygonF> MGeometryHandling::enlargeGeometryToBBoxIfNecessary(
        QVector<QPolygonF> polygons, QRectF bbox)
{
    // The geometry in "polygons" is assumed to be located in the range
    // -180..180 degrees.
    // First, determine how many times the geometry needs to be repeated to
    // fill the provdied bounding box "bbox".
    // E.g., if the western edge of the bbox is at -270. degrees, repeat
    // one time to the west; if the eastern edge is 600. degrees, repeat twice
    // to the east.
    int globeRepetitionsWestward = 0;
    if (bbox.left() < -180.)
    {
        globeRepetitionsWestward = int(floor((bbox.left() + 180.) / 360.));
        LOG4CPLUS_DEBUG(mlog, "Western edge of BBOX is west of -180. degrees, "
                        << "copying global geometry "
                        << globeRepetitionsWestward << " times westward.");
    }

    int globeRepetitionsEastward = 0;
    if (bbox.right() > 180.)
    {
        globeRepetitionsEastward = int(ceil((bbox.right() - 180.) / 360.));
        LOG4CPLUS_DEBUG(mlog, "Eastern edge of BBOX is east of 180. degrees, "
                        << "copying global geometry "
                        << globeRepetitionsEastward << " times eastward.");
    }

    if (globeRepetitionsWestward == 0 && globeRepetitionsEastward == 0)
    {
        // Bounding box is in range -180..180 degrees, polygons can remain
        // as they are.
        return polygons;
    }

    QVector<QPolygonF> enlargedPolygonGeometry;

    for (int globeOffset = globeRepetitionsWestward;
         globeOffset <= globeRepetitionsEastward; globeOffset++)
    {
        qreal lonOffset = globeOffset * 360.;
        for (QPolygonF polygon : polygons)
        {
            if (globeOffset == 0)
            {
                enlargedPolygonGeometry << polygon;
            }
            else
            {
                enlargedPolygonGeometry << polygon.translated(lonOffset, 0.);
            }
        }
    }

    return enlargedPolygonGeometry;
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


OGRPolygon *MGeometryHandling::convertQRectToOGRPolygon(QRectF &rect)
{
    float leftlon = rect.x();
    float lowerlat = rect.y();
    float rightlon = rect.x() + rect.width();
    float upperlat = rect.y() + rect.height();

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


QPolygonF MGeometryHandling::convertOGRLineStringToQPolygon(
        OGRLineString *lineString)
{
    int numLinePoints = lineString->getNumPoints();
    OGRRawPoint *v = new OGRRawPoint[numLinePoints];
    lineString->getPoints(v);
    QPolygonF polygon;
    for (int i = 0; i < numLinePoints; i++)
    {
        polygon << QPointF(v[i].x, v[i].y);
    }
    delete[] v;
    return polygon;
}


void MGeometryHandling::appendOGRLineStringsFromOGRGeometry(
        QList<OGRLineString *> *lineStrings, OGRGeometry *geometry)
{
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
            {
                lineStrings->append((OGRLineString *) gc->getGeometryRef(g));
            }
        }
    }
}


} // namespace Met3D
