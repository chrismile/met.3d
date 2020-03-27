/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015-2017 Bianca Tost
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
#ifndef NATURALEARTHDATALOADER_H
#define NATURALEARTHDATALOADER_H

// standard library imports


// related third party imports
#include <QtCore>

// local application imports
#include "ogrsf_frmts.h"
#include "cpl_error.h"

namespace Met3D
{

/**
  @brief Module that reads shapefile geometry from "natural earth" datasets.

  @todo Refactor to pipeline architecture (data source).
  */
class MNaturalEarthDataLoader
{
public:
    enum GeometryType {
        COASTLINES  = 0,
        BORDERLINES = 1
    };

    MNaturalEarthDataLoader();
    virtual ~MNaturalEarthDataLoader();

    void setDataSources(QString coastlinesfile, QString borderlinesfile);

    /**
      Loads line geometry of @p geometryType while considering cyclic
      repetitions in longitude direction. Results are stored in @p vertices,
      @p startIndices and @p vertexCount. @p cornerRect contains the world
      coordinates of the bounding box.

      Generates the vertices of the geometry in at most three steps by dividing
      the bounding box domain into regions which on a sphere are equal to region
      with longitudes in the range of [-180, 180] which is the domain the coast-
      and borderlines to be loaded are defined on:

      1) Determines the west-most of the regions described above and tests if
         its west-east-extend is smaller than 360:
         If yes: Compue it seperately.
         If no: It will be considered in the second step.
      2) Tells @ref MNaturalEarthDataLoader::loadLineGeometry() to load the
         complete line geometry and to make a shifted copy of if for each
         360-degree-region.
         [Considerss all of the regions described above with west-east-extend
          equal to 360.]
      3) Loads the remaining region if not all of the geometry was loaded in the
         steps before.

      The line geometry in all three steps is loaded by adapting the bounding
      box and the shift parameters of
      @ref MNaturalEarthDataLoader::loadLineGeometry() to the region[s] needed.
    */

    void loadLineGeometry(GeometryType type, QRectF bbox,
                          QVector<QVector2D> *vertices,
                          QVector<int> *startIndices,
                          QVector<int> *count,
                          bool append=true, double offset=0.,
                          int copies=0);

    void loadCyclicLineGeometry(GeometryType geometryType,
                                QRectF cornerRect,
                                QVector<QVector2D> *vertices,
                                QVector<int> *startIndices,
                                QVector<int> *vertexCount);
    /**
     @brief checkDistanceViolationInPointSpacing checks, if the distance between
     two successive points (i.e., length of a single line segment) in a group
     (collection of continuous line segments) of the @p vertices vector is greater
     than a threshold value.
    */

    void checkDistanceViolationInPointSpacing(QVector<QVector2D> *vertices,
                                  QVector<int> *startIndices,
                                  QVector<int> *vertexCount);
    /**
     @brief reorganizeGroupWithUnevenPointSpacing reogranizes the points in a
     group, if the distance between two successive points is greater than a
     threshold value.The present group is terminated at the first occurence of
     the violation of the threshold and a new group is created starting with the
     subsequent points
    */

    int reorganizeGroupWithUnevenPointSpacing(int globalMaximumDistanceGroup,
                            QVector<float> groupMaximumDistance,
                            QVector<QVector2D> *vertices,
                            QVector<int> *startIndices,
                            QVector<int> *vertexCount);

    /**
     @brief loadAndRotateLineGeometry loads the line geometry and rotates it
     according to the given rotated north pole coordinates @p poleLat and
     @p poleLon.
     */
    void loadAndRotateLineGeometry(GeometryType type, QRectF bbox,
                                   QVector<QVector2D> *vertices,
                                   QVector<int> *startIndices,
                                   QVector<int> *count, bool append=true,
                                   double poleLat=90., double poleLon=-180.);

    /**
     @brief loadAndRotateLineGeometryUsingRotatedBBox loads the line geometry
     and rotates it according to the given rotated north pole coordinates
     @p poleLat and @p poleLon and treats bounding box coordinates
     given by the user as rotated coordinates.
     */
    void loadAndRotateLineGeometryUsingRotatedBBox(
            GeometryType type, QRectF bbox,
            QVector<QVector2D> *vertices,
            QVector<int> *startIndices,
            QVector<int> *count, bool append=true,
            double poleLat=90., double poleLon=-180.);

    /**
     @brief geographicalToRotatedCoords transforms @p point according to the
     given rotated north pole coordinates @p poleLat and @p poleLon.

     @p point, @p poleLat and @p poleLon need to be given in
     spherical coordinates.
     The approach applied here is implemented following the example of
     the methods 'lamrot_to_lam' and 'phirot_to_phi' taken from the file
     'grid_rot.c' of the CDO project. The original code hase been published
     under GNU GENERAL PUBLIC LICENSE Version 2, June 1991.
     (source: https://code.zmaw.de/projects/cdo/files  [Version 1.8.1])

     It is similar to the one described here:
     https://gis.stackexchange.com/questions/10808/lon-lat-transformation

     @return true if the input variables define rotation with any effect false
     otherwise. (e.g. poleLat = 90. and poleLon = 180. results in the original
     position an therefore has no effect.)
     */
    static bool geographicalToRotatedCoords(OGRPoint *point, double poleLat,
                                            double poleLon);

    /**
     @brief rotatedToGeograhpicalCoords inverts the rotated north pole
     tramsformation and computes the geographical coordinates to given
     @p point in rotated coordinates according to the given rotated north
     pole coordinates @p poleLat and @p poleLon.

     @p point, @p poleLat and @p poleLon need to be given in
     spherical coordinates.
     The approach applied here is implemented following the example of
     the methods 'lamrot_to_lam' and 'phirot_to_phi' taken from the file
     'grid_rot.c' of the CDO project. The original code hase been published
     under GNU GENERAL PUBLIC LICENSE Version 2, June 1991.
     (source: https://code.zmaw.de/projects/cdo/files  [Version 1.8.1])

     It is similar to the one described here:
     https://gis.stackexchange.com/questions/10808/lon-lat-transformation

     @return true if the input variables define rotation with any effect false
     otherwise. (e.g. poleLat = 90 and poleLon = 180 results in the original
     position an therefore has no effect.)
     */
    static bool rotatedToGeograhpicalCoords(OGRPoint *point, double poleLat,
                                            double poleLon);

    /**
     getNearestLon determines which of @p lon1 and @p lon2 has the
     closest distance to one of the two longitudes @p toLon1 and
     @p toLon2 and returns the one of lon1 and lon2 which has the closest
     distance.

     This method is required to get rid of connections for testing which
     longitude the connection between toLon1 and toLon2 is more likely to cross.
     Using this information we can detect if this connection is wanted or not
     and by this get rid of line crossing nearly the whole domain.
     */
    static double getNearestLon(double toLon1, double toLon2, double lon1,
                                double lon2);

    /**
     getCentreLons computes pair of longitudes consisting of
     the only two longitudes projected to the longitude coordinates 0, -180 and
     180 by the rotated north pole projection.

     @p poleLat and @p poleLon must contain the rotated pole coordinates
     in degree. The results are stored in @p centreLons in degree in real
     geographical coordinates.
     */
    static void getCentreLons(QVector2D *centreLons, double poleLat,
                              double poleLon);

    /**
     validConnectionBetweenPositions tests if connection bewteen @p prevPosition and
     @p currPosition is wanted or not and returns true if it is unwanted
     false otherwise.

     When drawing a graticule for a rotated grid, coast/border lines incorrectly
     connecting an eastern and a western vertex on the map may occur. Such lines
     are incorrectly drawn across the entire map domain. To avoid such lines it
     needs to be tested whether two vertices connected by a line that crosses
     the centre of the map lie in different "halfs" of the map (i.e. whether the
     connection line crosses the centre of the map on the "front side" or on the
     "back side" of the map). For this purpose we test if the pair of longitudes
     in @p centreLons is the nearest to @p prevPosition or
     @p currPosition in longitudes. If the nearest centre longitude maps to
     0 the connection line crosses the centre on the "front side" and thus is
     wanted otherwise it is not.

     The rotated north pole projection is computed according to @p poleLat
     and @p poleLon. @p point is used as a wrapper for applying the
     projections and is changed by this method.

     At the end of this method @p prevPosition and @p currPosition
     contain the position of the current step i.e. the previous position for the
     next step in rotated coordinates.

     For this method to work correctly @p centreLons need to contain the
     pair of longitudes consisting of the only two longitudes projected to the
     longitude coordinates 0, -180 and 180 in real geugraphical coordinates.
     This pair can be determined by calling
     @ref MNaturalEarthDataLoader::getCentreLons().
     */
    static bool validConnectionBetweenPositions(
            QVector2D *prevPosition, QVector2D *currPosition, OGRPoint *point,
            double poleLat, double poleLon, QVector2D *centreLons);

    /**
     getBBoxPolygon builds and returns a OGRPolygon representing  @p bbox.
     */
    static OGRPolygon *getBBoxPolygon(QRectF *bbox);

private:
    /**
     getLineStringFeatures returns the line features of @p geometry and stores
     the result in @p lineStrings.
     */
    void getLineStringFeatures(QList<OGRLineString*> *lineStrings,
                               OGRGeometry *geometry);

    QVector<GDALDataset*> gdalDataSet;

};

} // namespace Met3D

#endif // NATURALEARTHDATALOADER_H
