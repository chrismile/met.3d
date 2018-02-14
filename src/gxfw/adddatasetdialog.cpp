/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016-2018 Marc Rautenhaus
**  Copyright 2016      Christoph Heidelmann
**  Copyright 2018      Bianca Tost
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
#include <QFileDialog>

// local application imports
#include "mglresourcesmanager.h"
#include "msystemcontrol.h"
#include "data/weatherpredictiondatasource.h"


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

    connect(ui->nameEdit                , SIGNAL(textChanged(QString)),
            this, SLOT(inputFieldChanged()));

    connect(ui->nwpBrowseButton           , SIGNAL(clicked(bool))       ,
            this, SLOT(browsePath()));
    connect(ui->trajectoriesBrowseButton  , SIGNAL(clicked(bool))       ,
            this, SLOT(browsePath()));

    connect(ui->nwpPathEdit               , SIGNAL(textChanged(QString)),
            this, SLOT(inputFieldChanged()));
    connect(ui->nwpFileFilterEdit         , SIGNAL(textChanged(QString)),
            this, SLOT(inputFieldChanged()));
    connect(ui->trajectoriesPathEdit      , SIGNAL(textChanged(QString)),
            this, SLOT(inputFieldChanged()));
    connect(ui->pipelineTypeTabWidget     , SIGNAL(currentChanged(int)) ,
            this, SLOT(inputFieldChanged()));
    connect(ui->trajectoriesTypeTabWidget , SIGNAL(currentChanged(int)) ,
            this, SLOT(inputFieldChanged()));

    connect(ui->pipelineTypeTabWidget , SIGNAL(currentChanged(int)) ,
            this, SLOT(setDefaultMemoryManager()));

    connect(ui->trajectoriesNWPDatasetCombo, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(selectedNWPDatasetChanged(QString)));
}


MAddDatasetDialog::~MAddDatasetDialog()
{
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

PipelineType MAddDatasetDialog::getSelectedPipelineType()
{
    if (ui->pipelineTypeTabWidget->currentWidget() == ui->nwpTab)
    {
        return PipelineType::NWP_PIPELINE;
    }
    else if (ui->pipelineTypeTabWidget->currentWidget()  == ui->trajectoriesTab)
    {
        if (ui->pipelineTypeTabWidget->currentWidget()
                == ui->trajectoriesComputationTab)
        {
            QString levelTypeU = ui->trajectoriesWindUVarCombo
                    ->currentText().split(" || Level type: ").last();
            QString levelTypeV = ui->trajectoriesWindVVarCombo
                    ->currentText().split(" || Level type: ").last();
            QString levelTypeW = ui->trajectoriesWindWVarCombo
                    ->currentText().split(" || Level type: ").last();
            if (levelTypeU != levelTypeV || levelTypeV != levelTypeW)
            {
                // todo Warnung ausgeben.
                return PipelineType::INVALID_PIPELINE_TYPE;
            }
        }
        return PipelineType::TRAJECTORIES_PIPELINE;
    }
    return PipelineType::INVALID_PIPELINE_TYPE;
}


MNWPPipelineConfigurationInfo MAddDatasetDialog::getNWPPipelineConfigurationInfo()
{
    MNWPPipelineConfigurationInfo d;

    d.name = ui->nameEdit->text();
    d.fileDir = ui->nwpPathEdit->text();
    d.fileFilter = ui->nwpFileFilterEdit->text();
    d.dataFormat = (MNWPReaderFileFormat) (ui->nwpFileFormatCombo->currentIndex() + 1);

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    QStringList memoryManagers = sysMC->getMemoryManagerIdentifiers();
    d.memoryManagerID = memoryManagers.at(ui->memoryMCombo->currentIndex());
    d.schedulerID = ui->schedulerIDCombo->currentText(); // "MultiThread";
    d.enableProbabiltyRegionFilter = ui->propRegBool->isChecked();
    d.treatRotatedGridAsRegularGrid =
            ui->treatRotatedAsRegularCheckBox->isChecked();
    d.surfacePressureFieldType = ui->surfacePressureTypeComboBox->currentText();
    d.convertGeometricHeightToPressure_ICAOStandard =
            ui->convertGeometricHeightToPressureICAOStandardCheckBox->isChecked();

    return d;
}


MTrajectoriesPipelineConfigurationInfo
MAddDatasetDialog::getTrajectoriesPipelineConfigurationInfo()
{
    MTrajectoriesPipelineConfigurationInfo d;

    d.name = ui->nameEdit->text();
    if (ui->trajectoriesTypeTabWidget->currentWidget()
            == ui->trajectoriesPrecomputedTab)
    {
        d.precomputed = true;
    }
    else
    {
        d.precomputed = false;
    }
    d.fileDir = ui->trajectoriesPathEdit->text();
    d.schedulerID = ui->schedulerIDCombo->currentText(); // "MultiThread";
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    QStringList memoryManagers = sysMC->getMemoryManagerIdentifiers();
    d.memoryManagerID = memoryManagers.at(ui->memoryMCombo->currentIndex());
    d.boundaryLayerTrajectories = ui->ablTrajectoriesCheckBox;
    d.NWPDataset = ui->trajectoriesNWPDatasetCombo->currentText();
    d.windUVariable = ui->trajectoriesWindUVarCombo
            ->currentText().split(" || Level type: ").first();
    d.windVVariable = ui->trajectoriesWindVVarCombo
            ->currentText().split(" || Level type: ").first();
    d.windWVariable = ui->trajectoriesWindWVarCombo
            ->currentText().split(" || Level type: ").first();
    d.verticalLvlType = ui->trajectoriesWindUVarCombo
            ->currentText().split(" || Level type: ").last();
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

    if (path != nullptr)
    {
        if (ui->pipelineTypeTabWidget->currentIndex() == 0)
        {
            ui->nwpPathEdit->setText(path);
        }
        else
        {
            ui->trajectoriesPathEdit->setText(path);
        }
    }
}


void MAddDatasetDialog::inputFieldChanged()
{
    if (ui->nameEdit->text()       != ""
            &&
            ((ui->pipelineTypeTabWidget->currentIndex() == 0
              && ui->nwpPathEdit->text()                != ""
              && ui->nwpFileFilterEdit->text()          != ""
                )
            || (ui->pipelineTypeTabWidget->currentIndex()        == 1
                && ui->trajectoriesTypeTabWidget->currentIndex() == 0
                && ui->trajectoriesPathEdit->text()              != "")
             || (ui->pipelineTypeTabWidget->currentIndex()         == 1
                 && ui->trajectoriesTypeTabWidget->currentIndex()  == 1
                 && ui->trajectoriesNWPDatasetCombo->currentText() != ""
                 && ui->trajectoriesWindUVarCombo->currentText()   != ""
                 && ui->trajectoriesWindVVarCombo->currentText()   != ""
                 && ui->trajectoriesWindWVarCombo->currentText()   != ""))
        )
    {
        okButton->setEnabled(true);
    }
    else
    {
        okButton->setEnabled(false);
    }
}


void MAddDatasetDialog::selectedNWPDatasetChanged(QString dataset)
{
    ui->trajectoriesWindUVarCombo->clear();
    ui->trajectoriesWindVVarCombo->clear();
    ui->trajectoriesWindWVarCombo->clear();

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();

    MWeatherPredictionDataSource* source =
            dynamic_cast<MWeatherPredictionDataSource*>
            (sysMC->getDataSource(dataset));
    QList<MVerticalLevelType> levelTypes = source->availableLevelTypes();
    foreach (MVerticalLevelType lvl, levelTypes)
    {
        QStringList variables = source->availableVariables(lvl);
        QString levelTypeString =
                MStructuredGrid::verticalLevelTypeToString(lvl);
        foreach (QString variable, variables)
        {
            QString item = variable + " || Level type: " + levelTypeString;
            ui->trajectoriesWindUVarCombo->addItem(item);
            ui->trajectoriesWindVVarCombo->addItem(item);
            ui->trajectoriesWindWVarCombo->addItem(item);
        }
    }
}


void MAddDatasetDialog::setDefaultMemoryManager()
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    if (ui->pipelineTypeTabWidget->currentWidget() == ui->nwpTab)
    {
        if (sysMC->getDefaultMemoryManagers()->value("NWP") != "")
        {
            ui->memoryMCombo->setCurrentText(
                        sysMC->getDefaultMemoryManagers()->value("NWP"));
        }
    }
    else if (ui->pipelineTypeTabWidget->currentWidget() == ui->trajectoriesTab)
    {
        if (sysMC->getDefaultMemoryManagers()->value("Trajectories") != "")
        {
            ui->memoryMCombo->setCurrentText(
                        sysMC->getDefaultMemoryManagers()->value("Trajectories"));
        }
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MAddDatasetDialog::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);

    ui->memoryMCombo->clear();
    ui->trajectoriesNWPDatasetCombo->clear();

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    foreach (QString mmID, sysMC->getMemoryManagerIdentifiers())
    {
        ui->memoryMCombo->addItem(mmID);
    }
    foreach (QString nwpDataset, sysMC->getDataSourceIdentifiers())
    {
        MWeatherPredictionDataSource *source =
                dynamic_cast<MWeatherPredictionDataSource*>(
                    sysMC->getDataSource(nwpDataset));
        if (source != nullptr)
        {
                ui->trajectoriesNWPDatasetCombo->addItem(nwpDataset);
        }
    }


    if (ui->trajectoriesNWPDatasetCombo->count() > 0)
    {
        selectedNWPDatasetChanged(ui->trajectoriesNWPDatasetCombo->currentText());
    }

    setDefaultMemoryManager();
}


} // namespace Met3D
