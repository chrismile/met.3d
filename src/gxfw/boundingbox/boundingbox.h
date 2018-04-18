/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Bianca Tost
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
#ifndef MBOUNDINGBOX_H
#define MBOUNDINGBOX_H

// standard library imports

// related third party imports
#include <QtCore>
#include <QtProperty>

// local application imports
#include "system/qtproperties.h"

namespace Met3D
{

/**
  @brief MBoundingBox represents a bounding box defining a domain used by
  actors inheriting from @ref MBoundingBoxInterface as render region.
 */
class MBoundingBox : public QObject
{
    Q_OBJECT

public:
    MBoundingBox(QString name, double westLon, double southLat,
                 double eastWestExtent, double northSouthExtent,
                 double bottomPressure_hPa, double topPressure_hPa);

    QString getID() { return name; }

    double getWestLon() { return horizontal2DCoords.x(); }
    double getSouthLat() { return horizontal2DCoords.y(); }
    double getEastWestExtent() { return horizontal2DCoords.width(); }
    double getNorthSouthExtent() { return horizontal2DCoords.height(); }
    QRectF getHorizontal2DCoords() { return horizontal2DCoords; }
    double getEastLon() { return eastLon; }
    double getNorthLat() { return northLat; }

    double getBottomPressure_hPa() { return bottomPressure_hPa; }
    double getTopPressure_hPa() { return topPressure_hPa; }

    void setID(QString name) { this->name = name; }

    /**
      setWestLon checks if @p westLon differs from x coordinate of
      @ref MBoundingBox::horizontal2DCoords and triggers changed-signal only for
      actual updates.
     */
    void setWestLon(double westLon);
    /**
      setSouthLat checks if @p southLat differs from y coordinate of
      @ref MBoundingBox::horizontal2DCoords and triggers changed-signal only for
      actual updates.
     */
    void setSouthLat(double southLat);
    /**
      setEastWestExtent checks if @p eastWestExtent differs from width of
      @ref MBoundingBox::horizontal2DCoords and triggers changed-signal only for
      actual updates.
     */
    void setEastWestExtent(double eastWestExtent);
    /**
      setNorthSouthExtent checks if @p northSouthExtent differs from height of
      @ref MBoundingBox::horizontal2DCoords and triggers changed-signal only for
      actual updates.
     */
    void setNorthSouthExtent(double northSouthExtent);
    /**
      setBottomPressure_hPa checks if @p bottomPressure_hPa differs from
      @ref MBoundingBox::bottomPressure_hPa and triggers changed-signal only for
      actual updates.
     */
    void setBottomPressure_hPa(double bottomPressure_hPa);
    /**
      setTopPressure_hPa checks if @p topPressure_hPa differs from
      @ref MBoundingBox::topPressure_hPa and triggers changed-signal only for
      actual updates.
     */
    void setTopPressure_hPa(double topPressure_hPa);

    /**
      Enables or disables emission of change signals.
     */
    void enableEmitChangeSignals(bool enable);

    /**
      Inform all actor types (horizontal, vertical, 3D) about changes of the
      bounding box coordinates.
     */
    void emitChangeSignal();

    /**
      Emit signals @ref horizontal2DCoordsChanged() and @ref coords3DChanged()
      if signal emitting is enabled.
     */
    void emitHorizontal2DCoordsChanged();
    /**
      Emit signals @ref pressureLevelChanged() and @ref coords3DChanged()
      if signal emitting is enabled.
     */
    void emitPressureLevelChanged();

signals:
    // Use separated signals for 2D and 3D to avoid duplicated invocation of
    // computation for 3D actors (emitChangeSignal()).
    void horizontal2DCoordsChanged();
    void pressureLevelChanged();
    void coords3DChanged();

protected:
    QString name;

    QRectF horizontal2DCoords;
    double eastLon;
    double northLat;
    double bottomPressure_hPa;
    double topPressure_hPa;

    // Indicator to enable or disable signal emitting.
    bool signalEmitEnabled;
};


class MBoundingBoxConnection;
class MActor;

/**
  @brief MBoundingBoxInterface is the abstract base class for all actors using
  bounding boxes.

  It holds a pointer to @ref MBoundingBoxConnection, which handles the
  connection between actor and bounding box by using signals and slots.
  @ref MBoundingBoxConnection is necessary because if MBoundingBoxInterface
  would inheritate from QObject this would lead to a diamond inheritance which
  cannot be solved due to the static_cast automatically created in Qt's
  moc-files when using signals and slots.
  (cp. http://www.drdobbs.com/cpp/multiple-inheritance-considered-useful/184402074).
 */
class MBoundingBoxInterface
{
public:
    MBoundingBoxInterface(MActor *child);
    ~MBoundingBoxInterface();

    /**
      onBoundingBoxChanged() defines how the actor should react if the bounding
      box changes.
     */
    virtual void onBoundingBoxChanged() = 0;

    void saveConfiguration(QSettings *settings);
    void loadConfiguration(QSettings *settings);

    /**
      Returns pointer to actor inheriting from this interface object.
     */
    MActor *getChild() { return child; }

    /**
      Returns name of the currently selected bounding box if present otherwise
      this method returns "None".
     */
    QString getBoundingBoxName();

    /**
      @brief Switches to bounding box with @param name.
     */
    void switchToBoundingBox(QString bBoxName);

protected:
    // Pointer to actor inheriting from the interface.
    MActor *child;
    // Object realising and handling connection between actor and bounding box.
    MBoundingBoxConnection *bBoxConnection;

};


class MSystemManagerAndControl;

/**
  @brief The MBoundingBoxConnection class handles the connection between a
  bounding box and the actor using it.
 */
class MBoundingBoxConnection : public QObject
{
    Q_OBJECT

public:    
    enum ConnectionType
    {
        HORIZONTAL = 0,
        VERTICAL   = 1,
        VOLUME     = 2
    };

    MBoundingBoxConnection(MBoundingBoxInterface *actor, ConnectionType type);
    ~MBoundingBoxConnection();

    MBoundingBox *getBoundingBox() { return boundingBox; }
    QtProperty *getProperty() { return bBoxProperty; }
    MBoundingBoxInterface *getActor() { return actor; }

    // Methods to access bounding box coordinates.

    double westLon() { return boundingBox->getWestLon(); }
    double southLat() { return boundingBox->getSouthLat(); }
    double eastLon() { return boundingBox->getEastLon(); }
    double northLat() { return boundingBox->getNorthLat(); }
    double eastWestExtent() { return boundingBox->getEastWestExtent(); }
    double northSouthExtent() { return boundingBox->getNorthSouthExtent(); }
    QRectF horizontal2DCoords() { return boundingBox->getHorizontal2DCoords(); }
    double bottomPressure_hPa() { return boundingBox->getBottomPressure_hPa(); }
    double topPressure_hPa() { return boundingBox->getTopPressure_hPa(); }

    /**
      @brief Switches to bounding box called @p name.
     */
    void switchToBoundingBox(QString name);

public slots:
    /**
      Handles change events of the properties in the property browser.
     */
    void onPropertyChanged(QtProperty *property);

    /**
      Calls onBoundingBoxChanged() of @ref bboxactor.
     */
    void onBoundingBoxChanged();

    /**
     Connects to the MSystemManagerAndControl::boundingBoxCreated() signal.
     It updates the list of @ref bBoxProperty ensuring the original bounding box
     is still selected afterwards.
     */
    void onBoundingBoxCreated();

    /**
     Connects to the MSystemManagerAndControl::boundingBoxDeleted(QString) signal.
     It updates the list of @ref bBoxProperty ensuring the original bounding box
     is still selected afterwards if it is still present. If the current
     selected bounding box was deleted, it switches to "None".
     */
    void onBoundingBoxDeleted(QString name);

    /**
     Connects to the MSystemManagerAndControl::boundingBoxRenamed() signal.
     It updates the list of @ref bBoxProperty ensuring the original bounding box
     is still selected afterwards.
     */
    void onBoundingBoxRenamed();

protected:
    void setBoundingBox(QString bBoxID);

    MSystemManagerAndControl *sysMC;
    MBoundingBoxInterface *actor;
    ConnectionType type;
    MBoundingBox *boundingBox;

    MQtProperties *properties;

    QtProperty *bBoxProperty;

    bool drawBBox;
    bool suppressUpdates;
};

} // namespace Met3D

#endif // MBOUNDINGBOX_H
