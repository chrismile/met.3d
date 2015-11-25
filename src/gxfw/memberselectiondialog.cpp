/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Christoph Heidelmann
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
#include "memberselectiondialog.h"
#include "ui_memberselectiondialog.h"

// standard library imports

// related third party imports

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msystemcontrol.h"
#include "gxfw/mscenecontrol.h"
#include "gxfw/actorcreationdialog.h"
#include "mainwindow.h"


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MMemberSelectionDialog::MMemberSelectionDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MMemberSelectionDialog)
{
    ui->setupUi(this);
    ui->availableMembers->setSelectionMode(QAbstractItemView::MultiSelection);
    ui->availableMembers->setSelectionBehavior(QAbstractItemView::SelectRows);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MMemberSelectionDialog::setAvailableEnsembleMembers(
        QSet<unsigned int> members)
{
    ui->availableMembers->clear();

    foreach (unsigned int m, members)
        ui->availableMembers->addItem(QString("%1").arg(m));
}


void MMemberSelectionDialog::setSelectedMembers(QSet<unsigned int> members)
{
    // For each available member in the list, check if it is contained in the
    // set "members" and set its selection accordingly.
    for (int row = 0; row < ui->availableMembers->count(); row++)
    {
        QListWidgetItem *currentItem = ui->availableMembers->item(row);
        unsigned int currentMember = currentItem->text().toUInt();
        currentItem->setSelected(members.contains(currentMember));
    }
}


QSet<unsigned int> MMemberSelectionDialog::getSelectedMembers()
{
    QSet<unsigned int> selectedMembers;

    foreach (QListWidgetItem *currentItem, ui->availableMembers->selectedItems())
    {
        unsigned int currentMember = currentItem->text().toUInt();
        selectedMembers.insert(currentMember);
    }

    return selectedMembers;
}


MMemberSelectionDialog::~MMemberSelectionDialog()
{
    delete ui;
}

} // namespace Met3D
