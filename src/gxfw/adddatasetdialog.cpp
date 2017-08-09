/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016 Marc Rautenhaus
**  Copyright 2016 Christoph Heidelmann
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
#include "adddatasetdialog.h"
#include "ui_adddatasetdialog.h"

// standard library imports

// related third party imports

// local application imports
#include "mglresourcesmanager.h"
#include "msystemcontrol.h"


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MAddDatasetDialog::MAddDatasetDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MAddDatasetDialog)
{
    ui->setupUi(this);
    okButton = ui->buttonBox->buttons().at(0);
    okButton->setEnabled(false);
    connect(ui->browseButton  , SIGNAL(clicked(bool))       ,
            this, SLOT(browsePath()));
    connect(ui->nameEdit      , SIGNAL(textChanged(QString)),
            this, SLOT(inputFieldChanged()));
    connect(ui->pathEdit      , SIGNAL(textChanged(QString)),
            this, SLOT(inputFieldChanged()));
    connect(ui->fileFilterEdit, SIGNAL(textChanged(QString)),
            this, SLOT(inputFieldChanged()));
}


MAddDatasetDialog::~MAddDatasetDialog()
{
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MNWPPipelineConfigurationInfo MAddDatasetDialog::getNWPPipelineConfigurationInfo()
{
    MNWPPipelineConfigurationInfo d;

    d.name = ui->nameEdit->text();
    d.fileDir = ui->pathEdit->text();
    d.fileFilter = ui->fileFilterEdit->text();
    d.dataFormat = (MNWPReaderFileFormat) (ui->fileFormatCombo->currentIndex() + 1);

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    QStringList memoryManagers = sysMC->getMemoryManagerIdentifiers();
    d.memoryManagerID = memoryManagers.at(ui->memoryMCombo->currentIndex());
    d.schedulerID = "MultiThread";
    d.enableProbabiltyRegionFilter = ui->propRegBool->isChecked();
    d.treatRotatedGridAsRegularGrid =
            ui->treatRotatedAsRegularCheckBox->isChecked();

    return d;
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MAddDatasetDialog::browsePath()
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    QString path = QFileDialog::getExistingDirectory(
                this, tr("Select path to data files"),
                sysMC->getMet3DHomeDir().absolutePath(),
                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (path != nullptr) ui->pathEdit->setText(path);
}


void MAddDatasetDialog::inputFieldChanged()
{
    if (ui->nameEdit->text()       != "" &&
        ui->pathEdit->text()       != "" &&
        ui->fileFilterEdit->text() != "")
    {
        okButton->setEnabled(true);
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MAddDatasetDialog::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);

    ui->memoryMCombo->clear();
    ui->fileFormatCombo->clear();

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    foreach (QString mmID, sysMC->getMemoryManagerIdentifiers())
        ui->memoryMCombo->addItem(mmID);

    ui->fileFormatCombo->addItem("CF_NETCDF");
    ui->fileFormatCombo->addItem("ECMWF_GRIB");
}


} // namespace Met3D
