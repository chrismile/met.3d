/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Philipp Kaiser
**  Copyright 2017 Marc Rautenhaus
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
#include "selectactordialog.h"
#include "ui_selectdatasourcedialog.h"

// standard library imports
#include <iostream>

// related third party imports
//#include <QtCore>
//#include <QtGui>

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msystemcontrol.h"
#include "actors/movablepoleactor.h"
#include "actors/nwpverticalsectionactor.h"
#include "actors/nwphorizontalsectionactor.h"
#include "actors/volumebboxactor.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSelectActorDialog::MSelectActorDialog(
        QList<MSelectActorType> types, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MSelectDataSourceDialog),
      actorsAvailable(true)
{
    ui->setupUi(this);

    createActorEntries(types);
}


MSelectActorDialog::~MSelectActorDialog()
{
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MSelectableActor MSelectActorDialog::getSelectedActor()
{
    int row = ui->dataFieldTable->currentRow();

    return getActorFromRow(row);
}


QList<MSelectableActor> MSelectActorDialog::getSelectedActors()
{
    QList<QTableWidgetItem*> items = ui->dataFieldTable->selectedItems();
    QList<MSelectableActor> actors;
    QList<int> visitedRows;

    foreach (QTableWidgetItem *item, items)
    {
        const int row = item->row();
        if (visitedRows.contains(row))
        {
            continue;
        }
        visitedRows.push_back(row);
        actors.push_back(getActorFromRow(row));
    }

    return actors;
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/


int MSelectActorDialog::exec()
{
    if (actorsAvailable)
    {
        return QDialog::exec();
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("No actors available to select!");
        msgBox.exec();
        return QDialog::Rejected;
    }
}


/******************************************************************************
***                            PRIVATE METHODS                              ***
*******************************************************************************/


void MSelectActorDialog::createActorEntries(QList<MSelectActorType> types)
{
    // Set the data field table's header.
    QTableWidget *table = ui->dataFieldTable;
    table->setColumnCount(1);
    table->setHorizontalHeaderLabels(QStringList("Available Actors"));

    // Loop over all data loaders registered with the resource manager.
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    actorsAvailable = false;

    // iterate through all actors
    for (MActor* a : glRM->getActors())
    {
        if (a == nullptr)
            continue;

        // check if actor is of requested type
        bool addActor = false;
        for (MSelectActorType type : types)
        {
            switch (type) {
                case POLE_ACTOR:
                    addActor |= dynamic_cast<MMovablePoleActor*>(a) != nullptr;
                    break;
                case HORIZONTALSECTION_ACTOR:
                    addActor |=  (dynamic_cast<MNWPHorizontalSectionActor*>(a)
                                  != nullptr);
                    break;
                case VERTICALSECTION_ACTOR:
                    addActor |= (dynamic_cast<MNWPVerticalSectionActor*>(a)
                                 != nullptr);
                    break;
                case BOX_ACTOR:
                    addActor |= (dynamic_cast<MVolumeBoundingBoxActor*>(a)
                                 != nullptr);
            }
        }

        // only add if type is requested
        if (addActor)
        {
            // Add a row to the table..
            int row = table->rowCount();
            table->setRowCount(row + 1);
            // .. and insert the element.
            table->setItem(row, 0, new QTableWidgetItem(a->getName()));

            actorsAvailable = true;
        }

    } // for (data loaders)

    // Resize the table's columns to fit data source names.
    table->resizeColumnsToContents();
    // Set table width to always fit window size.
    table->horizontalHeader()->setStretchLastSection(true);
    // Disable resize of column by user.
    table->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeMode::Fixed);
    // Resize widget to fit table size.
    this->resize(table->width(), table->height());
}


MSelectableActor MSelectActorDialog::getActorFromRow(int row)
{
    MSelectableActor actor;
    actor.actorName = ui->dataFieldTable->item(row, 0)->text();
    return actor;
}


} // namespace Met3D
