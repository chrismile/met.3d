/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2016 Marc Rautenhaus
**  Copyright 2016      Bianca Tost
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
#include "waypointstablemodel.h"

// standard library imports
#include <iostream>

// related third party imports
#include <QtGui>
#include <QFileInfo>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MWaypointsTableModel::MWaypointsTableModel(QString id, QObject *parent)
    : QAbstractTableModel(parent),
      wpModelID(id)
{
    flightTrackName = "Met.3D vertical section path";
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

int MWaypointsTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return waypoints.length();
}


int MWaypointsTableModel::size()
{
    return waypoints.length();
}


int MWaypointsTableModel::sizeIncludingMidpoints()
{
    return 2 * waypoints.length() - 1;
}


int MWaypointsTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 8;
}


QVariant MWaypointsTableModel::data(const QModelIndex &index,
                                    int role = Qt::DisplayRole) const
{
    // Check validity of requested index.
    if ( !index.isValid() ||
         index.row() < 0 ||
         !(index.row() < this->waypoints.length()) )
    {
        return QVariant();
    }

    MWaypoint *wp = waypoints.at(index.row());
    int column = index.column();

    // DisplayRole: return the actual data fields of the waypoint.
    if (role == Qt::DisplayRole)
    {
        switch (column)
        {
        case LOCATION:
            return QVariant(wp->locationName);
        case LON:
            return QVariant(wp->positionLonLat.x());
        case LAT:
            return QVariant(wp->positionLonLat.y());
        case FLIGHT_LEVEL:
            return QVariant(wp->flightLevel);
        case PRESSURE:
            return QVariant(wp->pressure);
        case LEG_DISTANCE:
            return QVariant(wp->distanceToPreviousWaypoint);
        case CUM_DISTANCE:
            return QVariant(wp->cummulativeTotalDistance);
        case COMMENTS:
            return QVariant(wp->comments);
        }
    }

    // TextAlignmentRole: at the moment the same for all columns.
    else if (role == Qt::TextAlignmentRole)
    {
        return QVariant(int(Qt::AlignLeft | Qt::AlignVCenter));
    }

    // No other role is currently handled.
    return QVariant();
}


bool MWaypointsTableModel::insertRows(int position, int rows,
                                      const QModelIndex &index,
                                      QList<MWaypoint*> *waypoints_list = 0)
{
    Q_UNUSED(index);
    beginInsertRows(QModelIndex(), position, position+rows-1);

    for (int row=0; row < rows; row++)
    {
        MWaypoint *new_waypoint;
        if (waypoints_list == 0)
        {
            new_waypoint = new MWaypoint;
            new_waypoint->pressure    = 0.;
            new_waypoint->flightLevel = 0.;
            // (distance fields are updated below and hence not initialised)
        }
        else
        {
            new_waypoint = waypoints_list->at(row);
        }

        waypoints.insert(position + row, new_waypoint);
    }

    updateDistances(position, rows);

    endInsertRows();
    return true; // Insertion was successful.
}


bool MWaypointsTableModel::removeRows(int row, int count,
                                      const QModelIndex &parent)
{
    Q_UNUSED(parent);
    beginRemoveRows(QModelIndex(), row, row + count - 1);

    // Remove "count" rows.
    for (int i = 0; i < count; i++)
        waypoints.removeAt(row);

    // Update the distances.
    updateDistances(row);

    endRemoveRows();

    return true; // Removal was successful.
}


Qt::ItemFlags MWaypointsTableModel::flags(const QModelIndex &index) const
{
    if ( !index.isValid() ) return Qt::ItemIsEnabled;

    int column = index.column();

    // The following data fields are editable in the table view.
    if ( column == LOCATION
         || column == LAT
         || column == LON
         || column == FLIGHT_LEVEL
         || column == COMMENTS )
    {
        return Qt::ItemFlags(QAbstractTableModel::flags(index)
                             | Qt::ItemIsEditable);
    }

    // All others are not.
    else
    {
        return Qt::ItemFlags(QAbstractTableModel::flags(index));
    }
}


QVariant MWaypointsTableModel::headerData(int section,
                                          Qt::Orientation orientation,
                                          int role = Qt::DisplayRole) const
{
    // Text alignment of the header entries.
    if (role == Qt::TextAlignmentRole)
    {
        if (orientation == Qt::Horizontal)
            return QVariant(int(Qt::AlignLeft | Qt::AlignVCenter));

        return QVariant(int(Qt::AlignRight | Qt::AlignVCenter));
    }

    // Currently, only the DisplayRole is processed below.
    if (role != Qt::DisplayRole)
    {
        return QVariant();
    }

    // Horizontal header: Return the names of the columns.
    if (orientation == Qt::Horizontal)
    {
        switch (section) {
        case LOCATION:
            return QVariant("Location                   ");
        case LAT:
            return QVariant("Lat (+-90)");
        case LON:
            return QVariant("Lon (+-180)");
        case FLIGHT_LEVEL:
            return QVariant("Flightlevel");
        case PRESSURE:
            return QVariant("Pressure\n(hPa)");
        case LEG_DISTANCE:
            return QVariant("Leg dist.\n(km [nm])");
        case CUM_DISTANCE:
            return QVariant("Cum dist.\n(km [nm])");
        case COMMENTS:
            return QVariant("Comments");
        }
    }

    // Vertical header: Return the number of the row (i.e. the number of the
    // waypoint).
    return QVariant(section);
}


bool MWaypointsTableModel::setData(const QModelIndex &index,
                                   const QVariant &value,
                                   int role = Qt::EditRole)
{
    Q_UNUSED(role);

    if ( !index.isValid() )
        return false;
    if ( index.row() < 0 || index.row() >= waypoints.size() )
        return false;

    MWaypoint   *wp    = waypoints.at(index.row());
    QModelIndex index2 = QModelIndex(index);

    bool ok = true;

    // Try to set "value" to the corresponding table column.
    switch ( index.column() )
    {
    case (LOCATION):
        wp->locationName = value.toString();
        break;

    case (LAT):
        wp->positionLonLat.setY(value.toFloat(&ok));
        if (ok)
        {
            // Valid lat value: The old location name becomes invalid and the
            // distances need to be updated.
            wp->locationName = "";
            updateDistances(index.row());
            index2 = this->createIndex(index.row(), CUM_DISTANCE);
        }
        break;

    case (LON):
        wp->positionLonLat.setX(value.toFloat(&ok));
        if (ok)
        {
            wp->locationName = "";
            updateDistances(index.row());
            index2 = this->createIndex(index.row(), CUM_DISTANCE);
        }
        break;

    case (FLIGHT_LEVEL):
        wp->flightLevel = value.toFloat();
        break;

    case (COMMENTS):
        wp->comments = value.toString();
        break;
    }

    emit dataChanged(index, index2);

    return ok;
}


void MWaypointsTableModel::setPositionLonLat(int index, float lon, float lat)
{
    MWaypoint *wp = waypoints.at(index);
    wp->positionLonLat = QVector2D(lon, lat);
    updateDistances(index);
    emit dataChanged(createIndex(index, LON), createIndex(index, LAT));
}


void MWaypointsTableModel::setPositionLonLatIncludingMidpoints(int index,
                                                               float lon,
                                                               float lat)
{
    // Odd indices mark a midpoint: Set both waypoints around midpoint.
    if ( index & 1 )
    {
        index = (index-1) / 2;
        MWaypoint *wp0 = waypoints.at(index);
        MWaypoint *wp1 = waypoints.at(index+1);
        QVector2D mid  = wp1->midpointToPreviousWaypoint;

        // Difference vectors from midpoint to the waypoints wp0 and wp1.
        QVector2D mid_wp0 = wp0->positionLonLat - mid;
        QVector2D mid_wp1 = wp1->positionLonLat - mid;

        QVector2D new_mid = QVector2D(lon, lat);
        wp0->positionLonLat = new_mid + mid_wp0;
        wp1->positionLonLat = new_mid + mid_wp1;

        updateDistances(index, 2);
        emit dataChanged(createIndex(index, LON), createIndex(index+1, LAT));
    }

    // Even indices mark waypoints: Set the coordinates of this waypoint and
    // update the affected distances.
    else
    {
        setPositionLonLat(index/2, lon, lat);
    }
}


void MWaypointsTableModel::saveToFile(QString fileName)
{
    QDomDocument document("FlightTrack");

    // Appending Elements to the XMLDocument
    QDomElement ft_el = document.createElement("FlightTrack");
    document.appendChild(ft_el);

    QDomElement name_el = document.createElement("Name");
    name_el.appendChild(document.createTextNode(flightTrackName));
    ft_el.appendChild(name_el);

    QDomElement wp_el = document.createElement("ListOfWaypoints");
    ft_el.appendChild(wp_el);

    int i = 0;
    MWaypoint * wp;
    for(i = 0; i < this->waypoints.count(); i++)
    {
        wp = this->waypoints.at(i);

        QDomElement element = document.createElement("Waypoint");
        wp_el.appendChild(element);

        element.setAttribute("location", wp->locationName);
        element.setAttribute("lat", wp->positionLonLat.y());
        element.setAttribute("lon", wp->positionLonLat.x());
        element.setAttribute("flightlevel", wp->flightLevel);

        QDomElement comments = document.createElement("Comments");
        comments.appendChild(document.createTextNode(wp->comments));
        element.appendChild(comments);
    }

    // Saving the XMLDocument to the File
    QFile file(fileName);
    file.open(QIODevice::WriteOnly);
    QTextStream textStream(&file);
    document.save(textStream, 2);
    file.close();
    this->fileName = QString(fileName);
}


QString MWaypointsTableModel::getFileName() const
{
    return this->fileName;
}


void MWaypointsTableModel::loadFromFile(QString fn)
{
    // Check if file exists.
    if ( !QFile::exists(fn) )
    {
        QString msg = QString("ERROR: cannot open waypoints file %1").arg(fn);
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MInitialisationError(msg.toStdString(), __FILE__, __LINE__);
    }

    fileName = fn;

    // Read the XML document from the file "fn".
    QFile file;
    file.setFileName(fn);
    file.open(QFile::ReadOnly);

    QDomDocument document;
    document.setContent(file.readAll());

    file.close();

    // Parse the name of the flight track.
    QDomElement ft_el = document.elementsByTagName("FlightTrack").at(0).toElement();
    flightTrackName = ft_el.elementsByTagName("Name").at(0).toElement().text();

    // Get all XML nodes that represent a waypoint and parse the waypoints into
    // a temporary list of waypoints.
    QDomNodeList waypoints_nodes = document.elementsByTagName("Waypoint");
    QList<MWaypoint*> *waypoints_list = new QList<MWaypoint*>;

    for (int i = 0; i < waypoints_nodes.count(); i++)
    {
        QDomNode node = waypoints_nodes.at(i);

        MWaypoint *wp = new MWaypoint;
        float lat = node.toElement().attribute("lat").toFloat();
        float lon = node.toElement().attribute("lon").toFloat();
        wp->positionLonLat = QVector2D(lon, lat);
        wp->flightLevel    = node.toElement().attribute("flightlevel").toFloat();
//TODO: flight level to pressure conversion
        wp->pressure       = 0.;
        wp->locationName   = node.toElement().attribute("location");

        waypoints_list->append(wp);
    }

    // Clear the old list of waypoints ..
	// crashes on windows if waypointsvector is empty
	if (this->waypoints.size() > 0)
	{
		beginRemoveRows(QModelIndex(), 0, this->waypoints.length() - 1);
		this->waypoints.clear();
		endRemoveRows();
	}

    // .. and replace it with the new list.
    insertRows(0, waypoints_list->count(), QModelIndex(), waypoints_list);
}


const QList<MWaypoint*>& MWaypointsTableModel::getWaypointsList()
{
    return waypoints;
}


void MWaypointsTableModel::setFlightTrackName(QString name)
{
    flightTrackName = name;
}


QVector2D MWaypointsTableModel::positionLonLat(int index)
{
    MWaypoint *wp = waypoints.at(index);
    return wp->positionLonLat;
}


QVector2D MWaypointsTableModel::positionLonLatIncludingMidpoints(int index)
{
    if ( index & 1 )
    {
        // Odd indices: Return midpoint.
        MWaypoint *wp = waypoints.at((index+1) / 2.);
        return wp->midpointToPreviousWaypoint;
    }
    else
    {
        // Even indices: Return waypoint.
        MWaypoint *wp = waypoints.at(index / 2.);
        return wp->positionLonLat;
    }
}


QVector3D MWaypointsTableModel::positionLonLatP(int index)
{
    MWaypoint *wp = waypoints.at(index);
    return QVector3D(wp->positionLonLat, wp->pressure);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

float MWaypointsTableModel::distanceBetweenWaypoints(MWaypoint *wp1,
                                                     MWaypoint *wp2)
{
    // Reference (assuming the earth is a sphere):
    // http://www.codeproject.com/Articles/22488/Distance-using-Longitiude-and-latitude-using-c

    double lon1 = wp1->positionLonLat.x();
    double lon2 = wp2->positionLonLat.x();
    double lat1 = wp1->positionLonLat.y();
    double lat2 = wp2->positionLonLat.y();

    double PI = 4.0 * atan(1.0);
    double dlat1 = lat1*(PI/180);

    double dlon1 = lon1*(PI/180);
    double dlat2 = lat2*(PI/180);
    double dlon2 = lon2*(PI/180);

    double dlon = dlon1-dlon2;
    double dLat = dlat1-dlat2;

    double aHarv = pow(sin(dLat/2.0), 2.0)
            + cos(dlat1) * cos(dlat2) * pow(sin(dlon/2), 2);
    double cHarv = 2 * atan2(sqrt(aHarv), sqrt(1.0-aHarv));

    // Earth radius (IUGG value for the equatorial radius of the earth):
    // 6378.137 km.
    const double earth = 6378.137;
    double distance = earth * cHarv;
    return distance;
}


void MWaypointsTableModel::updateDistances(int index, int count)
{
    MWaypoint *wp_i;

    // 1. Update "distances to previous waypoint" for the wps from "index"
    // to (and including) "index+count" (hence if count == 1, the waypoint at
    // position index and the following waypoint will be updated).
    for(int i = index; i <= min(index+count, waypoints.size()-1); i++)
    {
        wp_i = waypoints.at(i);

        if ( i == 0 )
        {
            // The first waypoint in the list has no distance to any previous
            // waypoint.
            wp_i->distanceToPreviousWaypoint = 0.;
        }
        else
        {
            // Compute distance to previous waypoint and the midpoint between
            // the two.
            MWaypoint *wp_prev = waypoints.at(i-1);

            wp_i->distanceToPreviousWaypoint =
                    distanceBetweenWaypoints(wp_i, wp_prev);

            wp_i->midpointToPreviousWaypoint =
                    (wp_i->positionLonLat + wp_prev->positionLonLat) / 2.;
        }
    }

    // 2. Update the total cumulative distance for all waypoints starting
    // at "index".
    for(int i = index; i < waypoints.size(); i++)
    {
        wp_i = waypoints.at(i);

        if ( i == 0 )
        {
            // Again, the total distance of the first waypoint is zero.
            wp_i->cummulativeTotalDistance = 0.;
        }
        else
        {
            MWaypoint *wp_prev = waypoints.at(i-1);
            wp_i->cummulativeTotalDistance =
                    wp_prev->cummulativeTotalDistance
                    + wp_i->distanceToPreviousWaypoint;
        }
    }
}

} // namespace Met3D

