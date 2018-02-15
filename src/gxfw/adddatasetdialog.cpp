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
#include <log4cplus/loggingmacros.h>

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
    connect(ui->saveConfigPushButton, SIGNAL(clicked()),
            this, SLOT(saveConfigurationToFile()));

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
    connect(ui->pipelineTypeTabWidget     , SIGNAL(currentChanged(int)) ,
            this, SLOT(setDefaultMemoryManager()));
    connect(ui->trajectoriesTypeTabWidget , SIGNAL(currentChanged(int)) ,
            this, SLOT(inputFieldChanged()));

    connect(ui->trajectoriesNWPDatasetCombo, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(selectedNWPDatasetChanged(QString)));

    resetAddDatasetGUI();
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
                QMessageBox::warning(nullptr, "Add New Dataset",
                                     "wind u, v and omega variable do NOT have"
                                     " the same vertical level type. Failed to"
                                     " add new data set.");
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


void MAddDatasetDialog::resetAddDatasetGUI()
{
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


void MAddDatasetDialog::saveConfiguration(QSettings *settings)
{
    // File Format.
    // ==========================================
    settings->beginGroup("FileFormat");
    // Save version id of Met.3D.
    settings->setValue("met3dVersion", met3dVersionString);
    settings->endGroup();
    // ==========================================

    // Save configuration for NWP Pipeline.
    // ==========================================
    if (ui->pipelineTypeTabWidget->currentWidget() == ui->nwpTab)
    {
        settings->beginGroup("NWPPipeline");
        settings->setValue("name", ui->nameEdit->text());
        settings->setValue("path", ui->nwpPathEdit->text());
        settings->setValue("fileFilter", ui->nwpFileFilterEdit->text());
        settings->setValue("schedulerID", ui->schedulerIDCombo->currentText());
        settings->setValue("memoryManagerID", ui->memoryMCombo->currentText());
        settings->setValue("fileFormat", ui->nwpFileFormatCombo->currentText());
        settings->setValue("enableRegridding", ui->regriddingBool->isChecked());
        settings->setValue("enableProbabilityRegionFilter",
                           ui->propRegBool->isChecked());
        settings->setValue("treatRotatedGridAsRegularGrid",
                           ui->treatRotatedAsRegularCheckBox->isChecked());
        settings->setValue(
                    "convertGeometricHeightToPressure_ICAOStandard",
                    ui->convertGeometricHeightToPressureICAOStandardCheckBox
                    ->isChecked());
        settings->setValue("gribSurfacePressureFieldType",
                           ui->surfacePressureTypeComboBox->currentText());
        settings->endGroup();
    }
    // Save configuration for Trajectories Pipeline.
    // ==========================================
    else if (ui->pipelineTypeTabWidget->currentWidget() == ui->trajectoriesTab)
    {
        settings->beginGroup("TrajectoriesPipeline");
        settings->setValue("name", ui->nameEdit->text());
        settings->setValue("ABLTrajectories",
                           ui->ablTrajectoriesCheckBox->isChecked());
        settings->setValue("schedulerID", ui->schedulerIDCombo->currentText());
        settings->setValue("memoryManagerID", ui->memoryMCombo->currentText());

        // Precomputed trajectories.
        // ==========================================
        if (ui->trajectoriesTypeTabWidget->currentWidget()
                == ui->trajectoriesPrecomputedTab)
        {
            settings->setValue("path", ui->trajectoriesPathEdit->text());
            settings->setValue("precomputed", true);
        }
        // Computed trajectories.
        // ==========================================
        else if (ui->trajectoriesTypeTabWidget->currentWidget()
                 == ui->trajectoriesComputationTab)
        {
            settings->setValue("NWPDataset",
                               ui->trajectoriesNWPDatasetCombo->currentText());
            settings->setValue("wind_uVariable",
                               ui->trajectoriesWindUVarCombo->currentText());
            settings->setValue("wind_vVariable",
                               ui->trajectoriesWindVVarCombo->currentText());
            settings->setValue("wind_omegaVariable",
                               ui->trajectoriesWindWVarCombo->currentText());
            settings->setValue("precomputed", false);
        }
        settings->endGroup();
    }
}


void MAddDatasetDialog::loadConfiguration(QSettings *settings)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    QStringList groups = settings->childGroups();
    if ( groups.contains("NWPPipeline") )
    {
        ui->pipelineTypeTabWidget->setCurrentWidget(ui->nwpTab);

        settings->beginGroup("NWPPipeline");
        ui->nameEdit->setText(settings->value("name", "").toString());
        ui->nwpPathEdit->setText(settings->value("path", "").toString());
        ui->nwpFileFilterEdit->setText(settings->value("fileFilter", "*").toString());
        ui->schedulerIDCombo->setCurrentText(
                    settings->value("schedulerID", "MultiThread").toString());
        ui->memoryMCombo->setCurrentText(
                    settings->value(
                        "memoryManagerID",
                        sysMC->getDefaultMemoryManagers()->value("NWP"))
                    .toString());
        ui->nwpFileFormatCombo->setCurrentText(
                    settings->value("fileFormat", "CF_NETCDF").toString());
        ui->regriddingBool->setChecked(
                    settings->value("enableRegridding", false).toBool());
        ui->propRegBool->setChecked(
                    settings->value(
                        "enableProbabilityRegionFilter", false).toBool());
        ui->treatRotatedAsRegularCheckBox->setChecked(
                    settings->value(
                        "treatRotatedGridAsRegularGrid", false).toBool());
        ui->convertGeometricHeightToPressureICAOStandardCheckBox->setChecked(
                    settings->value(
                        "convertGeometricHeightToPressure_ICAOStandard",
                        false).toBool());
        ui->surfacePressureTypeComboBox->setCurrentText(
                    settings->value("gribSurfacePressureFieldType",
                                    "auto").toString());

        settings->endGroup();
    }
    else if ( groups.contains("TrajectoriesPipeline") )
    {
        ui->pipelineTypeTabWidget->setCurrentWidget(ui->trajectoriesTab);

        settings->beginGroup("TrajectoriesPipeline");
        ui->nameEdit->setText(settings->value("name", "").toString());
        ui->schedulerIDCombo->setCurrentText(
                    settings->value("schedulerID", "MultiThread").toString());
        ui->memoryMCombo->setCurrentText(
                    settings->value(
                        "memoryManagerID",
                        sysMC->getDefaultMemoryManagers()->value("Trajectories"))
                    .toString());
        bool precomputed = settings->value("precomputed", true).toBool();
        if (precomputed)
        {
            ui->trajectoriesTypeTabWidget->setCurrentWidget(
                        ui->trajectoriesPrecomputedTab);
            ui->trajectoriesPathEdit->setText(settings->value("path",
                                                              "").toString());
        }
        else
        {
            ui->trajectoriesTypeTabWidget->setCurrentWidget(
                        ui->trajectoriesComputationTab);
            ui->trajectoriesNWPDatasetCombo->setCurrentText(
                        settings->value("NWPDataset", "").toString());
            ui->trajectoriesWindUVarCombo->setCurrentText(
                        settings->value("wind_uVariable", "").toString());
            ui->trajectoriesWindVVarCombo->setCurrentText(
                        settings->value("wind_vVariable", "").toString());
            ui->trajectoriesWindWVarCombo->setCurrentText(
                        settings->value("wind_omegaVariable", "").toString());
        }
        ui->ablTrajectoriesCheckBox->setChecked(
                    settings->value("ABLTrajectories", false).toBool());

        settings->endGroup();
    }
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MAddDatasetDialog::saveConfigurationToFile(QString filename)
{
    if (filename.isEmpty())
    {
        QString directory =
                MSystemManagerAndControl::getInstance()
                ->getMet3DWorkingDirectory().absoluteFilePath("config/pipelines");
        QDir().mkpath(directory);
        filename = QFileDialog::getSaveFileName(
                    this, "Save pipeline configuration",
                    QDir(directory).absoluteFilePath("default.pipeline.conf"),
                    "Pipeline configuration files (*.pipeline.conf)");
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

    saveConfiguration(settings);

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been saved.");
}


bool MAddDatasetDialog::loadConfigurationFromFile(QString filename)
{
    if (filename.isEmpty())
    {
        filename = QFileDialog::getOpenFileName(
                    this, "Load pipeline configuration",
                    MSystemManagerAndControl::getInstance()
                    ->getMet3DWorkingDirectory().absoluteFilePath("config/pipelines"),
                    "Pipeline configuration files (*.pipeline.conf)");

        if (filename.isEmpty())
        {
            return false;
        }
    }

    QFileInfo fileInfo(filename);
    if (!fileInfo.exists())
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText(QString("Pipeline configuration file"
                            " ' %1 ' does not exist.").arg(filename));
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return false;
    }

    LOG4CPLUS_DEBUG(mlog,
                    "Loading pipeline configuration from "
                    << filename.toStdString());

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);

    QStringList groups = settings->childGroups();
    if ( !(groups.contains("NWPPipeline")
           || groups.contains("TrajectoriesPipeline")) )
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("The selected file does not contain configuration data "
                    "for nwp or trajectories pipeline.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return false;
    }

    loadConfiguration(settings);

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been loaded.");

    return true;
}


/******************************************************************************
***                            PRIVATE SLOTS                                ***
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
//    Q_UNUSED(event);

//    ui->memoryMCombo->clear();
//    ui->trajectoriesNWPDatasetCombo->clear();

//    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
//    foreach (QString mmID, sysMC->getMemoryManagerIdentifiers())
//    {
//        ui->memoryMCombo->addItem(mmID);
//    }
//    foreach (QString nwpDataset, sysMC->getDataSourceIdentifiers())
//    {
//        MWeatherPredictionDataSource *source =
//                dynamic_cast<MWeatherPredictionDataSource*>(
//                    sysMC->getDataSource(nwpDataset));
//        if (source != nullptr)
//        {
//                ui->trajectoriesNWPDatasetCombo->addItem(nwpDataset);
//        }
//    }


//    if (ui->trajectoriesNWPDatasetCombo->count() > 0)
//    {
//        selectedNWPDatasetChanged(ui->trajectoriesNWPDatasetCombo->currentText());
//    }

//    setDefaultMemoryManager();
}


} // namespace Met3D
