/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Michael Kern
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
#include "actorcreationdialog.h"
#include "ui_actorcreationdialog.h"

// standard library imports

// related third party imports

// local application imports
#include "gxfw/mglresourcesmanager.h"


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MActorCreationDialog::MActorCreationDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MActorCreationDialog)
{
    ui->setupUi(this);
}


MActorCreationDialog::~MActorCreationDialog()
{
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QString MActorCreationDialog::getActorName() const
{
    return ui->nameLineEdit->text();
}


MActor* MActorCreationDialog::createActorInstance()
{
    const QString actorName = ui->nameLineEdit->text();
    const QString actorType = ui->actorTypeComboBox->currentText();

    MAbstractActorFactory *factory =
            MGLResourcesManager::getInstance()->getActorFactory(actorType);

    MActor* actor = factory->create();
    actor->setName(actorName);
    actor->setEnabled(true);

    return actor;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MActorCreationDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    // List all available factories in combo box.
    ui->actorTypeComboBox->clear();
    const QList<QString> factoryNames =
            MGLResourcesManager::getInstance()->getActorFactoryNames();
    for (int i = 0; i < factoryNames.size(); i++)
        ui->actorTypeComboBox->addItem(factoryNames[i]);
}

} // namespace Met3D
