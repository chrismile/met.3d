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
#include "boundingbox.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "actors/nwphorizontalsectionactor.h"
#include "actors/nwpverticalsectionactor.h"
#include "actors/nwpvolumeraycasteractor.h"
#include "gxfw/mactor.h"
#include "gxfw/msystemcontrol.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                              M2DBoundingBox                             ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MBoundingBox::MBoundingBox(QString name, double westLon, double southLat,
                           double eastWestExtent, double northSouthExtent,
                           double bottomPressure_hPa, double topPressure_hPa)
    : name(name),
      horizontal2DCoords(QRectF(westLon, southLat, eastWestExtent,
                                northSouthExtent)),
      eastLon(westLon + eastWestExtent),
      northLat(southLat + northSouthExtent),
      bottomPressure_hPa(bottomPressure_hPa),
      topPressure_hPa(topPressure_hPa),
      signalEmitEnabled(true)
{}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MBoundingBox::setWestLon(double westLon)
{
    // Only apply change if there is a change.
    if (qreal(westLon) != horizontal2DCoords.x())
    {
        horizontal2DCoords.moveLeft(westLon);
        eastLon = horizontal2DCoords.x() + horizontal2DCoords.width();
        emitHorizontal2DCoordsChanged();
    }
}


void MBoundingBox::setSouthLat(double southLat)
{
    // Only apply change if there is a change.
    if (qreal(southLat) != horizontal2DCoords.y())
    {
        horizontal2DCoords.moveTop(southLat);
        northLat = horizontal2DCoords.y() + horizontal2DCoords.height();
        emitHorizontal2DCoordsChanged();
    }
}


void MBoundingBox::setEastWestExtent(double eastWestExtent)
{
    // Only apply change if there is a change.
    if (qreal(eastWestExtent) != horizontal2DCoords.width())
    {
        horizontal2DCoords.setWidth(eastWestExtent);
        eastLon = horizontal2DCoords.x() + horizontal2DCoords.width();
        emitHorizontal2DCoordsChanged();
    }
}


void MBoundingBox::setNorthSouthExtent(double northSouthExtent)
{
    // Only apply change if there is a change.
    if (qreal(northSouthExtent) != horizontal2DCoords.height())
    {
        horizontal2DCoords.setHeight(northSouthExtent);
        northLat = horizontal2DCoords.y() + horizontal2DCoords.height();
        emitHorizontal2DCoordsChanged();
    }
}


void MBoundingBox::setBottomPressure_hPa(double bottomPressure_hPa)
{
    // Only apply change if there is a change.
    if (this->bottomPressure_hPa != bottomPressure_hPa)
    {
        this->bottomPressure_hPa = bottomPressure_hPa;
        emitPressureLevelChanged();
    }
}


void MBoundingBox::setTopPressure_hPa(double topPressure_hPa)
{
    // Only apply change if there is a change.
    if (this->topPressure_hPa != topPressure_hPa)
    {
        this->topPressure_hPa = topPressure_hPa;
        emitPressureLevelChanged();
    }
}


void MBoundingBox::enableEmitChangeSignals(bool enable)
{
    signalEmitEnabled = enable;
}

void MBoundingBox::emitChangeSignal()
{
    if (signalEmitEnabled)
    {
        emit horizontal2DCoordsChanged();
        emit pressureLevelChanged();
        emit coords3DChanged();
    }
}


void MBoundingBox::emitHorizontal2DCoordsChanged()
{
    if (signalEmitEnabled)
    {
        emit horizontal2DCoordsChanged();
        emit coords3DChanged();
    }
}


void MBoundingBox::emitPressureLevelChanged()
{
    if (signalEmitEnabled)
    {
        emit pressureLevelChanged();
        emit coords3DChanged();
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/


/******************************************************************************
***                          MBoundingBoxInterface                          ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MBoundingBoxInterface::MBoundingBoxInterface(MActor *child)
    : child(child)
{}


MBoundingBoxInterface::~MBoundingBoxInterface()
{
    delete bBoxConnection;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MBoundingBoxInterface::saveConfiguration(QSettings *settings)
{
    settings->setValue("useBoundingBox", getBoundingBoxName());
    if (bBoxConnection->getBoundingBox() != nullptr)
    {
        QRectF coord2D(bBoxConnection->westLon(), bBoxConnection->southLat(),
                       bBoxConnection->eastWestExtent(),
                       bBoxConnection->northSouthExtent());
        settings->setValue("horizontal2DCoords", coord2D);
        settings->setValue("bottomPressure_hPa",
                           bBoxConnection->bottomPressure_hPa());
        settings->setValue("topPressure_hPa", bBoxConnection->topPressure_hPa());
    }
}


void MBoundingBoxInterface::loadConfiguration(QSettings *settings)
{
    QStringList versionID = readConfigVersionID(settings);
    QString name = "None";    
    QRectF horziontal2DCoords = QRectF();
    double bottomPressure_hPa = 1045.;
    double topPressure_hPa = 20.;
    QStringList bBoxIdentifieres = MSystemManagerAndControl::getInstance()
            ->getBoundingBoxesIdentifiers();

    // Offer user to load bounding box data and use it to create a bounding box.
    if (versionID[0].toInt() <= 1 && versionID[1].toInt() < 2)
    {
        QMessageBox::warning(
                    nullptr, child->getName(),
                    QString(
                        "You are loading an actor configuration file"
                        " (Actor: %1) that has been written with a previous"
                        " version of Met.3D.\nNote that bounding box handling"
                        " has been changed since that version.\n").arg(
                        child->getName()));

        // For earlier versions stick to currently selected bounding box if
        // user decides not to create new boudning box from coordinates.
        if (bBoxConnection->getBoundingBox() != nullptr)
        {
            name = bBoxConnection->getBoundingBox()->getID();
        }
        if (dynamic_cast<MNWPVolumeRaycasterActor*>(child))
        {
            settings->beginGroup("BoundingBox");
            float llcrnLon = settings->value("llcrnLon", -60.f).toFloat();
            float llcrnLat = settings->value("llcrnLat", 30.f).toFloat();
            float urcrnLon = settings->value("urcrnLon", 160.f).toFloat();
            float urcrnLat = settings->value("urcrnLat", 10.f).toFloat();
            horziontal2DCoords = QRectF( llcrnLon, llcrnLat,
                                  urcrnLon - llcrnLon, urcrnLat - llcrnLat );
            settings->endGroup();
            bottomPressure_hPa =
                    settings->value("bottomPressure_hPa", 1045.).toDouble();
            topPressure_hPa =
                    settings->value("topPressure_hPa", 20.).toDouble();
        }
        else if (dynamic_cast<MNWPVerticalSectionActor*>(child))
        {
            // Setup default horizontal coordinates.
            horziontal2DCoords = QRectF( -60.f, 30.f, 100.f, 40.f );
            bottomPressure_hPa =
                    double(settings->value("p_bot_hPa", 1045.).toDouble());
            topPressure_hPa =
                    double(settings->value("p_top_hPa", 20.).toDouble());
        }
        else
        {
            // Since the identifier for bounding boxes in the config file
            // differs from one actor to the other, store all in a list, loop
            // over the list and try to get the 2D coordinates. Stop if coords
            // were found.
            QStringList bboxIdentifiers = QStringList();
            bboxIdentifiers << "boundingBox" << "boxCorners" << "bbox";
            foreach (QString bboxIdentifier, bboxIdentifiers)
            {
                horziontal2DCoords = settings->value(bboxIdentifier,
                                              QRectF()).toRectF();
                if (!horziontal2DCoords.isEmpty())
                {
                    break;
                }
            }
            bottomPressure_hPa =
                    double(settings->value("p_bot_hPa", 1045.).toDouble());
            topPressure_hPa =
                    double(settings->value("p_top_hPa", 20.).toDouble());
        }


        if (!horziontal2DCoords.isEmpty())
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Question);
            msgBox.setWindowTitle(child->getName());
            msgBox.setText(QString(
                               "The loaded configuration file [Actor: %1]"
                               " contains actor-specific bounding box data"
                               " stored by a previous Met.3D version.\n\nDo you"
                               " want to transfer the bounding box data to a"
                               " new-style bounding box accessible by all"
                               " Met.3D actors or discard the data?")
                           .arg(child->getName()));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.button(QMessageBox::Yes)->setText("Transfer");
            msgBox.button(QMessageBox::No)->setText("Discard");
            msgBox.exec();
            if (msgBox.clickedButton() == msgBox.button(QMessageBox::Yes))
            {
                name = MSystemManagerAndControl::getInstance()
                        ->getBoundingBoxDock()->addBoundingBox(
                            &horziontal2DCoords, bottomPressure_hPa,
                            topPressure_hPa);
            }
            else
            {
                name = "None";
                // Since 'None' is also part of the bounding box identifiers,
                // we need to check if the number of identifieres is greater
                // one.
                if (bBoxIdentifieres.size() > 1)
                {
                    // Offer the user to select one of the existing bounding
                    // boxes.
                    int answer = QMessageBox::question(
                                nullptr, child->getName(),
                                "Do you want to select one of the existing"
                                " bounding boxes?\n"
                                "[Actor: " + child->getName() + "]",
                                QMessageBox::Yes, QMessageBox::No);
                    if (QMessageBox::StandardButton(answer) == QMessageBox::Yes)
                    {
                        bool ok;
                        bBoxIdentifieres.removeOne("None");
                        name = QInputDialog::getItem(
                                    nullptr, child->getName(),
                                    "Bounding Box: ", bBoxIdentifieres, 0,
                                    false, &ok);
                        // Reset name to "None" if user canceled selection.
                        if (!ok)
                        {
                            name = "None";
                        }
                    }
                }
                // If user chose to not create or select a bounding box,
                // inform him/her that the bounding box will be set to "None".
                if (name == "None")
                {
                    QMessageBox::information(
                                nullptr, child->getName(),
                                "Setting bounding box to 'None'.");
                }
            }
        }
    }
    else
    {
        // For later versions load name of bounding box.
        name = settings->value("useBoundingBox", "None").toString();
        if ( !(bBoxIdentifieres.contains(name)))
        {
            horziontal2DCoords =
                    settings->value("horizontal2DCoords", QRectF()).toRectF();
            if (!(horziontal2DCoords.isEmpty()))
            {
                // Offer the user to use the saved coordinates to create new
                // bounding box if the bounding box is missing and configuration
                // contains coordinates.
                int answer = QMessageBox::question(
                            nullptr, child->getName(),
                            "Could not find bounding box '" + name
                            + "'.\nDo you want to create a new bounding box"
                              " object from the coordinates specified in the"
                              " configuration file?\n"
                              "[Actor: " + child->getName() + "]",
                            QMessageBox::Yes, QMessageBox::No);
                if (QMessageBox::StandardButton(answer) == QMessageBox::Yes)
                {
                    bottomPressure_hPa = settings->value("bottomPressure_hPa",
                                             1045.).toDouble();
                    topPressure_hPa = settings->value("topPressure_hPa",
                                                      20.).toDouble();;
                    MSystemManagerAndControl::getInstance()
                            ->getBoundingBoxDock()->addBoundingBox(
                                name, &horziontal2DCoords, bottomPressure_hPa,
                                topPressure_hPa);
                }
                else
                {
                    name = "None";
                }
            }
            else
            {
                QMessageBox::warning(
                            nullptr, "Warning",
                            "Could find neither bounding box '" + name
                            + "' nor bounding box coordinates to set up new"
                              " bounding box.");
                name = "None";
            }


            if (name == "None")
            {
                // Since 'None' is also part of the bounding box identifiers,
                // we need to check if the number of identifieres is greater
                // one.
                if (bBoxIdentifieres.size() > 1)
                {
                    // Offer the user to select one of the existing bounding
                    // boxes.
                    int answer = QMessageBox::question(
                                nullptr, child->getName(),
                                "Do you want to select one of the existing"
                                " bounding boxes?\n"
                                "[Actor: " + child->getName() + "]",
                                QMessageBox::Yes, QMessageBox::No);
                    if (QMessageBox::StandardButton(answer) == QMessageBox::Yes)
                    {
                        bool ok;
                        bBoxIdentifieres.removeOne("None");
                        name = QInputDialog::getItem(
                                    nullptr, child->getName(),
                                    "Bounding Box: ", bBoxIdentifieres, 0,
                                    false, &ok);
                        // Reset name to "None" if user canceled selection.
                        if (!ok)
                        {
                            name = "None";
                        }
                    }
                }
            }

            // If user chose to not create or select a bounding box, inform
            // him/her that the bounding box will be set to "None".
            if (name == "None")
            {
                QMessageBox::information(
                            nullptr, child->getName(),
                            "Setting bounding box to 'None'.");
            }
        }
    }

    bBoxConnection->switchToBoundingBox(name);
}


QString MBoundingBoxInterface::getBoundingBoxName()
{
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        return "None";
    }
    else
    {
        return bBoxConnection->getBoundingBox()->getID();
    }
}


void MBoundingBoxInterface::switchToBoundingBox(QString bBoxName)
{
    if (!(MSystemManagerAndControl::getInstance()
          ->getBoundingBoxesIdentifiers().contains(bBoxName)))
    {
        QMessageBox::warning(
                    nullptr, "Warning",
                    "Could not find bounding box '" + bBoxName
                    + "'.\nSetting bounding box to 'None'.");
        bBoxConnection->switchToBoundingBox("None");
    }
    else
    {
        bBoxConnection->switchToBoundingBox(bBoxName);
    }
}


/******************************************************************************
***                          MBoundingBoxConnection                         ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MBoundingBoxConnection::MBoundingBoxConnection(MBoundingBoxInterface *actor,
                                               ConnectionType type)
    : QObject(),
      actor(actor),
      type(type),
      boundingBox(nullptr),
      drawBBox(true),
      suppressUpdates(false)
{
    sysMC = MSystemManagerAndControl::getInstance();
    MActor *mactor = actor->getChild();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    mactor->beginInitialiseQtProperties();

    properties = mactor->getQtProperties();

    bBoxProperty = properties->mEnum()->addProperty("bounding box");
    QStringList bBoxList = sysMC->getBoundingBoxDock()
            ->getSortedBoundingBoxesList();
    properties->mEnum()->setEnumNames(bBoxProperty, bBoxList);

    // Set default bounding box to second entry if there exists a second entry.
    if (bBoxList.length() > 1)
    {
        suppressUpdates = true;
        properties->setEnumItem(bBoxProperty, bBoxList.at(1));
        setBoundingBox(bBoxList.at(1));
        suppressUpdates = false;
    }

    mactor->endInitialiseQtProperties();

    connect(properties->mEnum(), SIGNAL(propertyChanged(QtProperty*)),
            this, SLOT(onPropertyChanged(QtProperty*)));

    connect(sysMC, SIGNAL(boundingBoxCreated()),
            this, SLOT(onBoundingBoxCreated()));
    connect(sysMC, SIGNAL(boundingBoxDeleted(QString)),
            this, SLOT(onBoundingBoxDeleted(QString)));
    connect(sysMC, SIGNAL(boundingBoxRenamed()),
            this, SLOT(onBoundingBoxRenamed()));
}


MBoundingBoxConnection::~MBoundingBoxConnection()
{
    // Disconnect signals otherwise program might crash if signal is sent,
    // but the actor was deleted previously. (This applies especially to
    // signals send by sysMC.)

    if (boundingBox != nullptr)
    {
        switch (type)
        {
        case HORIZONTAL:
            disconnect(boundingBox, SIGNAL(horizontal2DCoordsChanged()),
                       this, SLOT(onBoundingBoxChanged()));
            break;
        case VERTICAL:
            disconnect(boundingBox, SIGNAL(pressureLevelChanged()),
                       this, SLOT(onBoundingBoxChanged()));
            break;
        case VOLUME:
            disconnect(boundingBox, SIGNAL(coords3DChanged()),
                       this, SLOT(onBoundingBoxChanged()));
            break;
        default:
            break;
        }
    }

    disconnect(properties->mBool(), SIGNAL(propertyChanged(QtProperty*)),
               this, SLOT(onPropertyChanged(QtProperty*)));
    disconnect(properties->mEnum(), SIGNAL(propertyChanged(QtProperty*)),
               this, SLOT(onPropertyChanged(QtProperty*)));

    disconnect(sysMC, SIGNAL(boundingBoxCreated()),
               this, SLOT(onBoundingBoxCreated()));
    disconnect(sysMC, SIGNAL(boundingBoxDeleted(QString)),
               this, SLOT(onBoundingBoxDeleted(QString)));
    disconnect(sysMC, SIGNAL(boundingBoxRenamed()),
               this, SLOT(onBoundingBoxRenamed()));
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MBoundingBoxConnection::switchToBoundingBox(QString name)
{
    properties->setEnumItem(bBoxProperty, name);
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MBoundingBoxConnection::onPropertyChanged(QtProperty *property)
{
    if (property == bBoxProperty)
    {
        if (suppressUpdates)
        {
            return;
        }
        setBoundingBox(properties->getEnumItem(bBoxProperty));
        actor->onBoundingBoxChanged();
    }
}


void MBoundingBoxConnection::onBoundingBoxChanged()
{
    // The bounding box has changed (extend, position or selected). Inform actor
    // about it.
    actor->onBoundingBoxChanged();
}


void MBoundingBoxConnection::onBoundingBoxCreated()
{
    // Replace bounding box list by updated list.
    QStringList bBoxList = sysMC->getBoundingBoxDock()
            ->getSortedBoundingBoxesList();
    // Suppress updates since bounding box doesn't change.
    suppressUpdates = true;
    properties->mEnum()->setEnumNames(bBoxProperty, bBoxList);
    // Since 'None' is always first in the list, there is no need to change the
    // position if no bounding box is set.
    if (boundingBox != nullptr)
    {
        // Since the bounding box names are sorted alphanumerically the new name
        // may result in a new position in the list. Thus it is necessary to set
        // the enum item to the new name.
        properties->setEnumItem(bBoxProperty, boundingBox->getID());
    }
    suppressUpdates = false;
}


void MBoundingBoxConnection::onBoundingBoxDeleted(QString name)
{
    // Get current bounding box name to check whether it was deleted or not.
    QString currentBBoxName = properties->getEnumItem(bBoxProperty);

    // Replace bounding box list by updated list.
    QStringList bBoxList = sysMC->getBoundingBoxDock()
            ->getSortedBoundingBoxesList();
    // Suppress updates since bounding box only changes if current bounding box
    // was deleted.
    suppressUpdates = true;
    properties->mEnum()->setEnumNames(bBoxProperty, bBoxList);
    // Since 'None' is always first in the list, there is no need to change the
    // position if no bounding box is set.
    if (boundingBox != nullptr)
    {
        // Current bounding box was deleted thus set use bbox to "None" and tell
        // the actor the bounding box has changed. (No need to change the enum
        // item since replacing the list automatically switches to the first
        // item.)
        if (name == currentBBoxName)
        {
            // Bounding box was deleted thus pointer should be set to nullptr.
            boundingBox = nullptr;
            actor->onBoundingBoxChanged();
        }
        else
        {
            // Since the bounding box names are sorted alphanumerically the new
            // name may result in a new position in the list. Thus it is
            // necessary to set the enum item to the new name.
            properties->setEnumItem(bBoxProperty, boundingBox->getID());
        }
    }
    suppressUpdates = false;
}


void MBoundingBoxConnection::onBoundingBoxRenamed()
{
    // Replace bounding box list by updated list.
    QStringList bBoxList = sysMC->getBoundingBoxDock()
            ->getSortedBoundingBoxesList();
    // Suppress updates since bounding box doesn't change.
    suppressUpdates = true;
    properties->mEnum()->setEnumNames(bBoxProperty, bBoxList);
    // Since 'None' is always first in the list, there is no need to change the
    // position if no bounding box is set.
    if (boundingBox != nullptr)
    {
        // Since the bounding box names are sorted alphanumerically the new name
        // may result in a new position in the list. Thus it is necessary to set
        // the enum item to the new name.
        properties->setEnumItem(bBoxProperty, boundingBox->getID());
    }
    suppressUpdates = false;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MBoundingBoxConnection::setBoundingBox(QString bBoxID)
{
    if (boundingBox != nullptr)
    {
        switch (type)
        {
        case HORIZONTAL:
            disconnect(boundingBox, SIGNAL(horizontal2DCoordsChanged()),
                       this, SLOT(onBoundingBoxChanged()));
            break;
        case VERTICAL:
            disconnect(boundingBox, SIGNAL(pressureLevelChanged()),
                       this, SLOT(onBoundingBoxChanged()));
            break;
        case VOLUME:
            disconnect(boundingBox, SIGNAL(coords3DChanged()),
                       this, SLOT(onBoundingBoxChanged()));
            break;
        default:
            break;
        }
    }

    boundingBox = sysMC->getBoundingBox(bBoxID);

    if (boundingBox != nullptr)
    {
        switch (type)
        {
        case HORIZONTAL:
            connect(boundingBox, SIGNAL(horizontal2DCoordsChanged()),
                    this, SLOT(onBoundingBoxChanged()));
            break;
        case VERTICAL:
            connect(boundingBox, SIGNAL(pressureLevelChanged()),
                    this, SLOT(onBoundingBoxChanged()));
            break;
        case VOLUME:
            connect(boundingBox, SIGNAL(coords3DChanged()),
                    this, SLOT(onBoundingBoxChanged()));
            break;
        default:
            break;
        }
    }
}

} // namespace Met3D

