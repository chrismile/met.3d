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
#ifndef WAYPOINTSTABLEMODEL_H
#define WAYPOINTSTABLEMODEL_H

// standard library imports

// related third party imports
#include <QAbstractTableModel>
#include <Qt/qmessagebox.h>
#include <QTableView>
#include <QtXml/QDomDocument>
#include <QtXml>
#include <QtCore>
#include <QtGui>

// local application imports


namespace Met3D
{

/**
 @brief MWaypoint represents a waypoint in a vertical cross section path or
 flight track.
 */
struct MWaypoint
{
    // Position of the waypoint with coordinates (lon, lat, pressure, flight
    // level).
    QVector2D positionLonLat;
    float     pressure;
    float     flightLevel;

    float distanceToPreviousWaypoint;
    float cummulativeTotalDistance;

    QString locationName;
    QString comments;

    QVector2D midpointToPreviousWaypoint;
};


/**
 @brief MWaypointsTableModel implements QAbstractTableModel to provide a data
 structure that accomodates a list of waypoints that describe the path of a
 vertical section or of a flight track.
 */
class MWaypointsTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Columns {
        LOCATION,
        LON,
        LAT,
        FLIGHT_LEVEL,
        PRESSURE,
        LEG_DISTANCE,
        CUM_DISTANCE,
        COMMENTS
    };

    explicit MWaypointsTableModel(QString id, QObject *parent = 0);

    int rowCount(const QModelIndex &parent) const;

    /**
     Returns the number of waypoints stored in this model.
     */
    int size();

    /**
     Returns the total number of waypoints and midpoints (one midpoints between
     each two waypoints.)
     */
    int sizeIncludingMidpoints();

    int columnCount(const QModelIndex &parent) const;

    bool insertRows(int position, int rows, const QModelIndex &index,
                    QList<MWaypoint *> *);

    bool removeRows(int row, int count, const QModelIndex &parent);


    QVariant data(const QModelIndex &index, int role) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);

    /**
     Sets the horizontal position of the i-th waypoint in the list.
     */
    void setPositionLonLat(int index, float lon, float lat);

    /**
     Sets the horizontal poisiton of the i-th waypoint or midpoint in the total
     list of waypoints and midpoints. If @p index denotes a midpoint, both
     adjactent waypoints are changed.
     */
    void setPositionLonLatIncludingMidpoints(int index, float lon, float lat);

    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    const QList<MWaypoint*>& getWaypointsList();

    QString getID() { return wpModelID; }

    void setFlightTrackName(QString name);

    QString getFlightTrackName() { return flightTrackName; }

    void saveToFile(QString fileName);

    QString getFileName() const;

    void loadFromFile(QString fileName);

    void saveToSettings(QSettings *settings);

    void loadFromSettings(QSettings *settings);

    /**
     Returns the horizontal position (lon/lat) of the waypoint at @p index.
     */
    QVector2D positionLonLat(int index);

    /**
     Returns the horizontal position (lon/lat) of the waypoint or midpoint at
     @p index. Note that @p index indexes the total list of waypoints and
     midpoints.
     */
    QVector2D positionLonLatIncludingMidpoints(int index);

    QVector3D positionLonLatP(int index);

protected:
    /**
     Computes the great circle distance in km between two waypoints.
     */
    float distanceBetweenWaypoints(MWaypoint *wp1, MWaypoint *wp2);

    /**
     Update the distances for the @p count waypoints that follow the waypoint
     at @p index.
     */
    void updateDistances(int index, int count=1);

private:
    QList<MWaypoint*> waypoints;

    QString wpModelID;       // id for identification within Met.3D
    QString flightTrackName; // name that is stored in the file
    QString fileName;
};

} // namespace Met3D

#endif // WAYPOINTSTABLEMODEL_H
