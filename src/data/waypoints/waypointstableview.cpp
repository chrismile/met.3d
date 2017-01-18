/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2016 Marc Rautenhaus
**  Copyright 2016 Bianca Tost
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
    {
        saveAsTrack();
    }
    else
    {
        checkExistanceAndSave(this->waypointsModel->getFileName());
    }
}


void MWaypointsView::saveAsTrack()
{
    QString fileName = QFileDialog::getSaveFileName(
                this, "Save Flight Track", "", "Flight Track XML (*.ftml)");
    if (fileName.length() > 0)
        checkExistanceAndSave(fileName);//this->waypointsModel->saveToFile(fileName);
}


void MWaypointsView::checkExistanceAndSave(QString filename)
{
    // Check whether the filename already exists.
    QRegExp filterlist(".*\\.(ftml)");
    if (!filename.isEmpty())
    {
        QString filetype = QString(".ftml");
        QFileInfo checkFile(filename);

        if (checkFile.exists())
        {
// TODO (bt, 17Oct2016) Use operating system dependend filename splitting.
            QMessageBox::StandardButton reply = QMessageBox::question(
                        this,
                        "Save Flight Track",
                        filename.split("/").last()
                        + " already exits.\n"
                        + "Do you want to replace it?",
                        QMessageBox::Yes|QMessageBox::No,
                        QMessageBox::No);

            if (reply == QMessageBox::No)
            {
                // Re-open FileDialog if file already exists and the user
                // chooses not to overwrite it or closes the question box.
                filename = QFileDialog::getSaveFileName(
                            this, "Save Flight Track", filename,
                            "Flight Track XML (*.ftml)");

                // Quit if user closes file dialog.
                if(filename.isEmpty()) return;
            }
        }

        // Check if filename doesn't end with .ftml
        // Repeat the test since the user could have changed the name in the
        // second FileDialog.
        while (!filterlist.exactMatch(filename))
        {
            // Append the selected file extension
            filename += filetype;
// NOTE (bt, 17Oct2016): Can be removed if Qt-bug is fixed.
            // Need to check if file already exists since QFileDialog
            // doesn't provide this functionality under Linux compared to
            // https://bugreports.qt.io/browse/QTBUG-11352 .
            QFileInfo checkFile(filename);
            if (checkFile.exists())
            {
// TODO (bt, 17Oct2016) Use operating system dependend filename splitting.
                QMessageBox::StandardButton reply = QMessageBox::question(
                            this,
                            "Save Flight Track",
                            filename.split("/").last()
                            + " already exits.\n"
                            + "Do you want to replace it?",
                            QMessageBox::Yes|QMessageBox::No,
                            QMessageBox::No);

                if (reply == QMessageBox::No)
                {
                    // Re-open FileDialog if file already exists and the user
                    // chooses not to overwrite it or closes the question box.
                    filename = QFileDialog::getSaveFileName(
                                this, "Save Flight Track", filename,
                                "Flight Track XML (*.ftml)");
                    // Quit if user closes file dialog.
                    if(filename.isEmpty()) return;
                }
            }
        }

        this->waypointsModel->saveToFile(filename);
    }

    return;
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
