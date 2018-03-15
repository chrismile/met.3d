/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015      Michael Kern
**  Copyright 2017      Bianca Tost
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
#include <QInputDialog>

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "actors/trajectoryactor.h"


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

    // Update actor-wise counter when the user changes the type of the
    // actor to be created.
    connect(ui->actorTypeComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(actorTypeChanged()));

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

    if (dynamic_cast<MTrajectoryActorFactory*>(factory))
    {
        bool accepted = false;
        MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
        // ask user which sync control should be synchronized with variable
        QString syncName = QInputDialog::getItem(
                    nullptr, "Choose Sync Control",
                    "Please select a sync control to synchronize with: ",
                    sysMC->getSyncControlIdentifiers(),
                    min(1, sysMC->getSyncControlIdentifiers().size() - 1),
                    false, &accepted);
        // if user has aborted do not create actor
        if (!accepted)
        {
            return nullptr;
        }
        // Initially set synchronization control during creation of actor.
        static_cast<MTrajectoryActor *>(actor)->setSynchronizationControl(
                    sysMC->getSyncControl(syncName));

    }

    return actor;
}


void MActorCreationDialog::actorTypeChanged()
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    QVector<MActor*> actors = glRM->getActors();

    // Get the current (i.e. default) name for the new actor and find the
    // number of existing actors that already carry this name.
    QString currentActorName = ui->actorTypeComboBox->currentText();
    int numActorsWithSameName = 0;

    for (int i = 0; i < actors.size(); i++)
    {
        if (actors.at(i)->getName().contains(currentActorName))
            numActorsWithSameName++;
    }

    // If the proposed actor name already exists, add a number.
    if (numActorsWithSameName == 0)
        ui->nameLineEdit->setText(currentActorName);
    else
        ui->nameLineEdit->setText(QString("%1 %2").arg(currentActorName)
                                  .arg(numActorsWithSameName));
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

    actorTypeChanged();
}

} // namespace Met3D
