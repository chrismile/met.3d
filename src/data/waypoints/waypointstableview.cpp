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
#include "waypointstableview.h"
#include "ui_waypointstableview.h"

// standard library imports
#include <iostream>

// related third party imports

// local application imports

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MWaypointsView::MWaypointsView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MWaypointsView)
{
    ui->setupUi(this);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
}


MWaypointsView::~MWaypointsView()
{
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MWaypointsView::addNewWaypoint()
{
    QModelIndex currentIndex = this->ui->tableView->currentIndex();
    int row = currentIndex.isValid() ? currentIndex.row() : 0;

    this->waypointsModel->insertRows(row, 1, currentIndex, 0);
}


void MWaypointsView::deleteSelectedWaypoint()
{
    // A minimum of two waypoints need to be retained in the model.
    if (waypointsModel->size() <= 2) return;

    QModelIndex currentIndex = this->ui->tableView->currentIndex();
    if (! currentIndex.isValid()) return;
    int row = currentIndex.row();

    this->waypointsModel->removeRows(row, 1, currentIndex);
}


void MWaypointsView::saveTrack()
{
    if (this->waypointsModel->getFileName().length() == 0)
        saveAsTrack();
    else
        this->waypointsModel->saveToFile(this->waypointsModel->getFileName());
}


void MWaypointsView::saveAsTrack()
{
    QString fileName = QFileDialog::getSaveFileName(
                this, "Save Flight Track", "", "Flight Track XML (*.ftml)");
    if (fileName.length() > 0)
        this->waypointsModel->saveToFile(fileName);
}


void MWaypointsView::openTrack()
{
    QString fileName = QFileDialog::getOpenFileName(
                this, "Open Flight Track", "", "Flight Track XML (*.ftml)");
    if ( !fileName.isEmpty() ) waypointsModel->loadFromFile(fileName);
}


void MWaypointsView::setWaypointsTableModel(MWaypointsTableModel *model)
{
    waypointsModel = model;
    ui->tableView->setModel(waypointsModel);
    ui->tableView->resizeColumnsToContents();
}

} // namespace Met3D
