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
#include "bboxdockwidget.h"
#include "ui_bboxdockwidget.h"

// standard library imports
#include <iostream>
#include <float.h>

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QKeyEvent>

// local application imports
#include "util/mutil.h"
#include "boundingbox.h"
#include "gxfw/msystemcontrol.h"
#include "gxfw/mglresourcesmanager.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MBoundingBoxDockWidget::MBoundingBoxDockWidget(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::MBoundingBoxDockWidget),
    suppressUpdates(false)
{
    ui->setupUi(this);
    this->setWindowTitle("Bounding Boxes");

    // Setup horizontal header.
    // ========================

    QFont font = ui->tableWidget->horizontalHeader()->font();
    font.setPixelSize(11);
    ui->tableWidget->horizontalHeader()->setFont(font);
    ui->tableWidget->setHorizontalHeaderLabels(
    {"name", "western\nlongitude", "southern\nlatitude", "east-west\nextend",
     "north-south\nextend", "bottom\npressure (hPa)", "top\npressure (hPa)"});
    ui->tableWidget->horizontalHeader()->setResizeMode(
                QHeaderView::ResizeToContents);

    // Setup item delegate of name column.
    // ===================================
    MBBoxNameDelegate *bBoxNameDelegate = new MBBoxNameDelegate();
    ui->tableWidget->setItemDelegateForColumn(0, bBoxNameDelegate);

    // Setup item delegates to treat some columns as double spin boxes.
    // ================================================================
    MDoubleSpinBoxDelegate *lonLatDelegate = new MDoubleSpinBoxDelegate(this);
    ui->tableWidget->setItemDelegateForColumn(1, lonLatDelegate);
    ui->tableWidget->setItemDelegateForColumn(2, lonLatDelegate);

    MDoubleSpinBoxDelegate *extendDelegate = new MDoubleSpinBoxDelegate(this);
    extendDelegate->setMinimum(0.);
    ui->tableWidget->setItemDelegateForColumn(3, extendDelegate);
    ui->tableWidget->setItemDelegateForColumn(4, extendDelegate);
    MDoubleSpinBoxDelegate *pressureDelegate = new MDoubleSpinBoxDelegate(this);
    pressureDelegate->setRange(.01, 1050.);
    pressureDelegate->setDecimals(2);
    pressureDelegate->setSingleStep(5.);
    ui->tableWidget->setItemDelegateForColumn(5, pressureDelegate);
    ui->tableWidget->setItemDelegateForColumn(6, pressureDelegate);
    ui->tableWidget->setItemDelegateForColumn(7, pressureDelegate);

    connect(ui->tableWidget, SIGNAL(cellChanged(int, int)),
            this, SLOT(onCellChanged(int, int)));

    connect(ui->newButton, SIGNAL(clicked()), this, SLOT(onCreateBBox()));
    connect(ui->cloneButton, SIGNAL(clicked()), this, SLOT(onCloneBBox()));
    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(onDeleteBBox()));
    connect(ui->saveButton, SIGNAL(clicked()),
            this, SLOT(saveConfigurationToFile()));
    connect(ui->loadButton, SIGNAL(clicked()),
            this, SLOT(loadConfigurationFromFile()));
}


MBoundingBoxDockWidget::~MBoundingBoxDockWidget()
{
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QStringList MBoundingBoxDockWidget::getSortedBoundingBoxesList()
{
    QStringList nameList;
    nameList.clear();
    // Set 'None' as first entry representing the possibility to select no
    // bounding box.
    nameList.prepend("None");
    for (int i = 0; i < ui->tableWidget->rowCount(); i++)
    {
        nameList << ui->tableWidget->item(i, 0)->data(Qt::DisplayRole).toString();
    }
    return nameList;
}


QString MBoundingBoxDockWidget::addBoundingBox(
        const QRectF *horizontalCoords2D, double bottomPressure_hPa,
        double topPressure_hPa)
{
    return createBoundingBox(
                "", horizontalCoords2D->x(), horizontalCoords2D->y(),
                horizontalCoords2D->width(), horizontalCoords2D->height(),
                bottomPressure_hPa, topPressure_hPa);
}


QString MBoundingBoxDockWidget::addBoundingBox(
        QString name, const QRectF *horizontalCoords2D,
        double bottomPressure_hPa, double topPressure_hPa)
{
    return createBoundingBox(
                name, horizontalCoords2D->x(), horizontalCoords2D->y(),
                horizontalCoords2D->width(), horizontalCoords2D->height(),
                bottomPressure_hPa, topPressure_hPa);
}


void MBoundingBoxDockWidget::saveConfiguration(QSettings *settings)
{
    settings->beginGroup("BoundingBoxes");
    settings->beginWriteArray("boundingBox");

    for (int row = 0; row < ui->tableWidget->rowCount(); row++)
    {
        MBoundingBox *bBox = MSystemManagerAndControl::getInstance()
                ->getBoundingBox(ui->tableWidget->item(row, 0)
                                 ->data(Qt::DisplayRole).toString());
        settings->setArrayIndex(row);
        settings->setValue("name", bBox->getID());
        settings->setValue("horizontal2DCoords", bBox->getHorizontal2DCoords());
        settings->setValue("bottomPressure_hPa", bBox->getBottomPressure_hPa());
        settings->setValue("topPressure_hPa", bBox->getTopPressure_hPa());
    }

    settings->endArray();
    settings->endGroup();
}


void MBoundingBoxDockWidget::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(getSettingsID());
    int numBBoxes = settings->beginReadArray("boundingBox");

    QStringList oldBBoxes =
            MSystemManagerAndControl::getInstance()
            ->getBoundingBoxesIdentifiers();
    oldBBoxes.removeOne("None");

    QStringList selectableBBoxes("None");

    for (int i = 0; i < numBBoxes; i++)
    {
        settings->setArrayIndex(i);
        QString name = settings->value("name", "").toString();
        QRectF horizontal2DCoords = settings->value(
                    "horizontal2DCoords",
                    QRectF(-60., 30., 100., 40.)).toRectF();
        double lon = double(horizontal2DCoords.x());
        double lat = double(horizontal2DCoords.y());
        double width = double(horizontal2DCoords.width());
        double height = double(horizontal2DCoords.height());
        double bottom = settings->value("bottomPressure_hPa", 1045.).toDouble();
        double top = settings->value("topPressure_hPa", 20.).toDouble();
        if (oldBBoxes.contains(name))
        {
            oldBBoxes.removeOne(name);
            updateRow(name, lon, lat, width, height, bottom, top);
        }
        else
        {
            createBoundingBox(name, lon, lat, width, height, bottom, top);
        }
        selectableBBoxes << name;
    }
    settings->endArray();
    settings->endGroup();


    // Return if no bounding boxes is there to be removed.
    if (!oldBBoxes.empty())
    {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setWindowTitle("Load Bounding Box Configuration");
        msgBox.setText(QString(
                           "Do you want to keep the existing bounding boxes and"
                           " add the new bounding box objects contained in the"
                           " file or remove the existing bounding boxes?"));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.button(QMessageBox::Yes)->setText("Keep");
        msgBox.button(QMessageBox::No)->setText("Remove");
        msgBox.exec();
        if (msgBox.clickedButton() == msgBox.button(QMessageBox::No))
        {
            MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
            // Remove bounding boxes that aren't part of the configuration file.
            for (int row = 0; row < ui->tableWidget->rowCount(); row++)
            {
                // Return if no bounding boxes is left to be removed.
                if (oldBBoxes.empty())
                {
                    return;
                }
                QString name = getBBoxNameInRow(row);
                if ( oldBBoxes.contains(name) )
                {
                    // If the bounding box to be removed is connected to one or
                    // more actors, offer the user to select one of the
                    // selectable bounding boxes.
                    QList<MActor*> connectedActors =
                            glRM->getActorsConnectedToBBox(name);
                    if ( !connectedActors.empty() )
                    {
                        foreach (MActor *actor, connectedActors)
                        {
                            // Since 'None' is also part of the selectable
                            // bounding boxes, we need to check if the number
                            // of selectable bounding boxes is greater one.
                            if (selectableBBoxes.size() > 1)
                            {
                                // Offer the user to select one of the
                                // selectable bounding boxes.
                                int answer = QMessageBox::question(
                                            nullptr, actor->getName(),
                                            QString(
                                                "Connected bounding box '%1''"
                                                " will be removed.\n"
                                                "Do you want to select one of"
                                                " the existing bounding boxes?\n"
                                                "[Actor: %2]")
                                            .arg(name).arg(actor->getName()),
                                            QMessageBox::Yes, QMessageBox::No);
                                if (QMessageBox::StandardButton(answer)
                                        == QMessageBox::Yes)
                                {
                                    bool ok;
                                    QString selectedName = QInputDialog::getItem(
                                                nullptr, actor->getName(),
                                                "Bounding Box: ",
                                                selectableBBoxes, 0, false, &ok);
                                    // Only set name if user didn't canceled
                                    // selection.
                                    if (!ok)
                                    {
                                        continue;
                                    }
                                    dynamic_cast<MBoundingBoxInterface*>(actor)
                                            ->switchToBoundingBox(selectedName);
                                }
                            }
                        }
                    }
                    ui->tableWidget->removeRow(row);
                    MSystemManagerAndControl::getInstance()
                            ->deleteBoundingBox(name);
                    oldBBoxes.removeOne(name);
                    // Decrement row since we deleted the current one.
                    row--;
                }
            }
        }
    }
}


void MBoundingBoxDockWidget::removeAllBoundingBoxes()
{
    // Remove all bounding boxes.
    for (int row = 0; row < ui->tableWidget->rowCount(); row++)
    {
        QString name = getBBoxNameInRow(row);
        ui->tableWidget->removeRow(row);
        MSystemManagerAndControl::getInstance()
                ->deleteBoundingBox(name);
        // Decrement row since we deleted the current row.
        row--;
    }
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MBoundingBoxDockWidget::onCellChanged(int row, int col)
{
    if (suppressUpdates)
    {
        return;
    }

    QString id = getBBoxNameInRow(row);

    double value =
            ui->tableWidget->item(row, col)->data(Qt::DisplayRole).toDouble();

    MBoundingBox *bBox =
            MSystemManagerAndControl::getInstance()->getBoundingBox(id);

    switch (col)
    {
    case 0:
        ui->tableWidget->sortItems(0);
        break;
    case 1:
        bBox->setWestLon(value);
        break;
    case 2:
        bBox->setSouthLat(value);
        break;
    case 3:
        bBox->setEastWestExtent(value);
        break;
    case 4:
        bBox->setNorthSouthExtent(value);
        break;
    case 5:
        bBox->setBottomPressure_hPa(value);
        break;
    case 6:
        bBox->setTopPressure_hPa(value);
        break;
    default:
        break;
    }
}



void MBoundingBoxDockWidget::onSpinBoxUpdate(double value)
{
    if (suppressUpdates)
    {
        return;
    }

    int row = ui->tableWidget->currentRow();
    int col = ui->tableWidget->currentColumn();

    QString id = getBBoxNameInRow(row);

    MBoundingBox *bBox =
            MSystemManagerAndControl::getInstance()->getBoundingBox(id);

    switch (col)
    {
    case 1:
        bBox->setWestLon(value);
        return;
    case 2:
        bBox->setSouthLat(value);
        return;
    case 3:
        bBox->setEastWestExtent(value);
        return;
    case 4:
        bBox->setNorthSouthExtent(value);
        return;
    case 5:
        bBox->setBottomPressure_hPa(value);
        return;
    case 6:
        bBox->setTopPressure_hPa(value);
        return;
    default:
        return;
    }
}


void MBoundingBoxDockWidget::onCreateBBox()
{
    // Create new bounding box.
    createBoundingBox("");
}


void MBoundingBoxDockWidget::onCloneBBox()
{
    int row = ui->tableWidget->currentRow();
    // No bounding box is selected to clone thus display error and do nothing.
    if (row == -1)
    {
        QMessageBox::warning(this, "Error",
                             "Please select a bounding box to be cloned.");
        return;
    }

    QString nameOriginBBox = getBBoxNameInRow(row);
    MBoundingBox *originBBox = MSystemManagerAndControl::getInstance()
            ->getBoundingBox(getBBoxNameInRow(row));

    bool ok = false;
    QString name = nameOriginBBox;

    QStringList bBoxes = MSystemManagerAndControl::getInstance()
            ->getBoundingBoxesIdentifiers();

    while (!ok)
    {
        // Get the name for the new bounding box to be created.
        name = QInputDialog::getText(
                    this, tr("Clone bounding box '%1'").arg(nameOriginBBox),
                    tr("Name: "), QLineEdit::Normal, nameOriginBBox, &ok);

        if (!ok)
        {
            return;
        }

        // Check if name is a valid bbox name (not empty and unique).
        ok = isValidBoundingBoxName(name, bBoxes);
    }

    // Create a copy of the selected bounding box.
    insertRow(name, originBBox->getWestLon(), originBBox->getSouthLat(),
              originBBox->getEastWestExtent(), originBBox->getNorthSouthExtent(),
              originBBox->getBottomPressure_hPa(),
              originBBox->getTopPressure_hPa());
}


void MBoundingBoxDockWidget::onDeleteBBox()
{
    int row = ui->tableWidget->currentRow();
    // No bounding box is selected to delete thus display error and do nothing.
    if (row == -1)
    {
        QMessageBox::warning(this, "Error",
                             "Please select a bounding box to delete.");
        return;
    }

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    QString name = getBBoxNameInRow(row);

    QString message = "Are you sure you want to delete '" + name +"'?";

    // If the bounding box to be deleted is connected to one or more actors,
    // print a warning.
    QList<MActor*> connectedActors = glRM->getActorsConnectedToBBox(name);
    if ( !connectedActors.empty() )
    {
        message += "\n\n\rWARNING: ''" + name
                + "'' is used by the following actors:\n\n\r";

        foreach (MActor *a, connectedActors)
        {
            message += a->getName() + "\n\r";
        }
    }

    int answer = QMessageBox::question(
                this, "Delete bounding box",
                message,
                QMessageBox::Yes, QMessageBox::No);
    if (QMessageBox::StandardButton(answer) == QMessageBox::Yes)
    {
        ui->tableWidget->removeRow(row);
        MSystemManagerAndControl::getInstance()->deleteBoundingBox(name);
    }
}


void MBoundingBoxDockWidget::saveConfigurationToFile(QString filename)
{

    if (filename.isEmpty())
    {
        QString directory =
                MSystemManagerAndControl::getInstance()
                ->getMet3DWorkingDirectory().absoluteFilePath("config/bboxes");
        QDir().mkpath(directory);
        filename = QFileDialog::getSaveFileName(
                    this, "Save bounding boxes configuration",
                    QDir(directory).absoluteFilePath("default.bbox.conf"),
                    "Bounding boxes configuration files (*.bbox.conf)");

        if (filename.isEmpty())
        {
            return;
        }
    }

    // Overwrite if the file exists.
    if (QFile::exists(filename))
    {
        QFile::remove(filename);
    }

    LOG4CPLUS_DEBUG(mlog, "Saving configuration to " << filename.toStdString());

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);

    settings->beginGroup("FileFormat");
    // Save version id of Met.3D.
    settings->setValue("met3dVersion", met3dVersionString);
    settings->endGroup();

    saveConfiguration(settings);

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been saved.");
}


void MBoundingBoxDockWidget::loadConfigurationFromFile(QString filename)
{
    if (filename.isEmpty())
    {
        filename = QFileDialog::getOpenFileName(
                    this, "Load bounding boxes configuration",
                    MSystemManagerAndControl::getInstance()
                    ->getMet3DWorkingDirectory().absoluteFilePath("config/bboxes"),
                    "Bounding boxes configuration files (*.bbox.conf)");

        if (filename.isEmpty())
        {
            return;
        }
    }

    QFileInfo fileInfo(filename);
    if (!fileInfo.exists())
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText(QString("Bounding box configuration file"
                            " ' %1 ' does not exist.").arg(filename));
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }

    LOG4CPLUS_DEBUG(mlog,
                    "Loading bounding box configuration from "
                    << filename.toStdString());

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);

    QStringList groups = settings->childGroups();
    if ( !groups.contains(getSettingsID()) )
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("The selected file does not contain configuration data "
                    "for bounding boxes.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }

    // Don't ask user whether to save configuration during program start.
    if (MSystemManagerAndControl::getInstance()->applicationIsInitialized()
            && ui->tableWidget->rowCount() > 0)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
                    this,
                    "Load Bounding Box Configuration",
                    "Do you want to save current bounding box configuration"
                    " before loading new configuration?\n"
                    "(Unsaved changes might get lost otherwise.)",
                    QMessageBox::Yes|QMessageBox::No,
                    QMessageBox::Yes);

        if (reply == QMessageBox::Yes)
        {
            saveConfigurationToFile();
        }
    }

    loadConfiguration(settings);

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been loaded.");
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MBoundingBoxDockWidget::keyPressEvent(QKeyEvent *event)
{
    // Enable use of enter/return key to edit table entry.
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
    {
        ui->tableWidget->editItem(ui->tableWidget->currentItem());
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

QString MBoundingBoxDockWidget::createBoundingBox(
        QString name, double lon, double lat, double width, double height,
        double bottom, double top)
{
    QStringList bBoxes = MSystemManagerAndControl::getInstance()
            ->getBoundingBoxesIdentifiers();
    bool ok = isValidBoundingBoxName(name, bBoxes, false);

    while (!ok)
    {
        // Get the name for the new bounding box to be created.
        name = QInputDialog::getText(
                    this, tr("Create new bounding box"), tr("Name: "),
                    QLineEdit::Normal, name, &ok);

        if (!ok)
        {
            return "None";
        }

        // Check if name is a valid bbox name (not empty and unique).
        ok = isValidBoundingBoxName(name, bBoxes);
    }

    insertRow(name, lon, lat, width, height, bottom, top);

    return name;
}

void MBoundingBoxDockWidget::insertRow(QString name, double lon, double lat,
                                          double width, double height,
                                          double bottom, double top)
{
    int row = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(row);

    MBoundingBox *bBox = new MBoundingBox(
                name, lon, lat, width, height, bottom, top);

    // Fill row with values.
    // =====================

    suppressUpdates = true;

    QTableWidgetItem *item = new QTableWidgetItem(name);
    ui->tableWidget->setItem(row, 0, item);

    // Store values in a vector to be able to access them in a loop.
    QVector<double> values;
    values << lon << lat << width << height << bottom << top;

    for (int col = 1; col < ui->tableWidget->columnCount(); col++)
    {
        item = new QTableWidgetItem(QString("%1").arg(values.at(col - 1)));
        ui->tableWidget->setItem(row, col, item);
    }
    ui->tableWidget->sortItems(0);

    suppressUpdates = false;

    MSystemManagerAndControl::getInstance()->registerBoundingBox(bBox);
}


void MBoundingBoxDockWidget::updateRow(QString name, double lon, double lat,
                                       double width, double height, double bottom,
                                       double top)
{
    MBoundingBox *bBox =
            MSystemManagerAndControl::getInstance()->getBoundingBox(name);

    // Bounding Box does not exist.
    if (bBox == nullptr)
    {
        return;
    }

    // Search for the row number of the bounding box to be updated.
    int row = 0;
    for (; row < ui->tableWidget->rowCount(); row++)
    {
        if (name == (ui->tableWidget->item(row, 0)
                     ->data(Qt::DisplayRole).toString()))
        {
            break;
        }
    }
    // Could not find a bounding box called name. (Should not happen.)
    if (row == ui->tableWidget->rowCount())
    {
        return;
    }

    bool updatedHorizontal2Dcoords = false;
    bool updatedPressureLevel = false;

    // Fill row with values.
    // =====================

    // Disable emission of change signals to avoid update event for each value
    // set.
    bBox->enableEmitChangeSignals(false);

    QVector<double> values;
    values << lon << lat << width << height << bottom << top;

    QTableWidgetItem *item;

    for (int col = 1; col < ui->tableWidget->columnCount(); col++)
    {
        if (values.at(col - 1) != (ui->tableWidget->item(row, col)
                                   ->data(Qt::DisplayRole).toDouble()))
        {
            item = new QTableWidgetItem(QString("%1").arg(values.at(col - 1)));
            // setItem() will delete the old item if it exists.
            ui->tableWidget->setItem(row, col, item);
            if (col <= 4)
            {
                updatedHorizontal2Dcoords = true;
            }
            else
            {
                updatedPressureLevel = true;
            }
        }
    }

    bBox->enableEmitChangeSignals(true);

    // After all values have been changed inform the listening actors about it.

    if (updatedHorizontal2Dcoords && updatedPressureLevel)
    {
        bBox->emitChangeSignal();
        return;
    }
    if (updatedHorizontal2Dcoords)
    {
        bBox->emitHorizontal2DCoordsChanged();
    }
    if (updatedPressureLevel)
    {
        bBox->emitPressureLevelChanged();
    }

}


QString MBoundingBoxDockWidget::getBBoxNameInRow(int row)
{
    return ui->tableWidget->item(row, 0)->data(Qt::DisplayRole).toString();
}


bool MBoundingBoxDockWidget::isValidBoundingBoxName(QString boundingBoxName,
                                                    QStringList &bBoxes,
                                                    bool printMessage)
{
    // Reject empty strings as name.
    if (boundingBoxName == "")
    {
        if (printMessage)
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("Please enter a name.");
            msgBox.exec();
        }

        return false;
    }

    // Reject already existing names.
    if (bBoxes.contains(boundingBoxName))
    {
        if (printMessage)
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("'" + boundingBoxName
                           + "' already exists.\nPlease enter a different name.");
            msgBox.exec();
        }

        return false;
    }

    return true;
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                             MSpinBoxDelegate                            ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/
MDoubleSpinBoxDelegate::MDoubleSpinBoxDelegate(MBoundingBoxDockWidget *dockWidget)
    : QStyledItemDelegate(),
      minimum(-DBL_MAX),
      maximum(DBL_MAX),
      decimals(2),
      singleStep(1.),
      dockWidget(dockWidget)
{}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QWidget *MDoubleSpinBoxDelegate::createEditor(QWidget *parent,
                                              const QStyleOptionViewItem &option,
                                              const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    QDoubleSpinBox *doubleSpinBox = new QDoubleSpinBox(parent);
    doubleSpinBox->setMinimum(minimum);
    doubleSpinBox->setMaximum(maximum);
    doubleSpinBox->setDecimals(decimals);
    doubleSpinBox->setSingleStep(singleStep);

    // Connect editor and dock widget to update bounding boxes instantly if the
    // value of one of the double spin boxes changes.
    connect(doubleSpinBox, SIGNAL(valueChanged(double)),
            dockWidget, SLOT(onSpinBoxUpdate(double)));

    return doubleSpinBox;
}


void MDoubleSpinBoxDelegate::setEditorData(QWidget *editor,
                                           const QModelIndex &index) const
{
    double value = index.model()->data(index, Qt::EditRole).toDouble();
    QDoubleSpinBox *doubleSpinBox = static_cast<QDoubleSpinBox*>(editor);
    doubleSpinBox->setValue(value);
}


void MDoubleSpinBoxDelegate::setModelData(QWidget *editor,
                                          QAbstractItemModel *model,
                                          const QModelIndex &index) const
{
    QDoubleSpinBox *doubleSpinBox = static_cast<QDoubleSpinBox*>(editor);
    doubleSpinBox->interpretText();
    model->setData(index, doubleSpinBox->value(), Qt::EditRole);
}


void MDoubleSpinBoxDelegate::updateEditorGeometry(
        QWidget *editor, const QStyleOptionViewItem &option,
        const QModelIndex &index) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                            MBBoxNameDelegate                            ***
*******************************************************************************/
/******************************************************************************
***                            PUBLIC METHODS                              ***
*******************************************************************************/

void MBBoxNameDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);

    QString newName = lineEdit->text();
    QString oldName = model->data(index, Qt::EditRole).toString();

    // Nothing has changed.
    if (oldName == newName)
    {
        return;
    }

    // Reject new name if a bounding box with the same name already exists.
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    if (sysMC->getBoundingBoxesIdentifiers().contains(newName))
    {
        // Inform the user and don't update the table so it resets to the
        // previous name automatically.
        QMessageBox::warning(
                    nullptr, tr("Error"),
                    "Could not rename bounding box.\n"
                    "Bounding box '" + newName + "' already exists.");
        return;
    }
    // Change name of bounding box and entry of table.
    MBoundingBox *bBox = sysMC->getBoundingBox(oldName);
    bBox->setID(newName);
    model->setData(index, newName, Qt::EditRole);
    sysMC->renameBoundingBox(oldName, bBox);
}


} // namespace Met3D
