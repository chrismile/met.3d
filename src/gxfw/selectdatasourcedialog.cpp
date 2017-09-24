/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
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
#include "selectdatasourcedialog.h"
#include "ui_selectdatasourcedialog.h"

// standard library imports
#include <iostream>

// related third party imports
#include <QtCore>
#include <QtGui>

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msystemcontrol.h"
#include "data/weatherpredictiondatasource.h"
#include "data/trajectorydatasource.h"
#include "data/trajectorynormalssource.h"
#include "data/trajectoryfilter.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSelectDataSourceDialog::MSelectDataSourceDialog(
        MSelectDataSourceDialogType type, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MSelectDataSourceDialog),
      variableAvailable(true),
      dataSourceAvailable(false)
{
    ui->setupUi(this);

    switch (type)
    {
    case MSelectDataSourceDialogType::SYNC_CONTROL:
    {
        ui->label->setText("Please select data sources and confirm with \"OK\"");
        createDataSourceEntries();
        break;
    }
    case MSelectDataSourceDialogType::VARIABLES:
    {
        ui->label->setText("Please select a variable and confirm with \"OK\"");
        QList<MVerticalLevelType> types;
        types << AUXILIARY_PRESSURE_3D << HYBRID_SIGMA_PRESSURE_3D
              << LOG_PRESSURE_LEVELS_3D << PRESSURE_LEVELS_3D
              << SURFACE_2D << POTENTIAL_VORTICITY_2D;
        createDataSourceEntries(types);
        break;
    }
    case MSelectDataSourceDialogType::TRAJECTORIES:
    {
        ui->label->setText("Please select a data source and confirm with \"OK\"");
        createTrajectoryDataSourceEntries();
        break;
    }
    default:
        break;
    }
}


MSelectDataSourceDialog::MSelectDataSourceDialog(
        const QList<MVerticalLevelType>& supportedList,
        //const QList<QString>&            supportedFilters,
        QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MSelectDataSourceDialog),
      variableAvailable(false),
      dataSourceAvailable(true)
{
    ui->setupUi(this);

    ui->label->setText("Please select a variable and confirm with \"OK\":");

    createDataSourceEntries(supportedList);
}


MSelectDataSourceDialog::~MSelectDataSourceDialog()
{
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MSelectableDataSource MSelectDataSourceDialog::getSelectedDataSource()
{
    int row = ui->dataFieldTable->currentRow();

    return getDataSourceFromRow(row);
}


QList<MSelectableDataSource> MSelectDataSourceDialog::getSelectedDataSources()
{
    QList<QTableWidgetItem*> items = ui->dataFieldTable->selectedItems();
    QList<MSelectableDataSource> dataSources;
    QList<int> visitedRows;

    foreach (QTableWidgetItem *item, items)
    {
        const int row = item->row();
        if (visitedRows.contains(row)) { continue; }
        visitedRows.push_back(row);
        dataSources.push_back(getDataSourceFromRow(row));
    }

    return dataSources;
}


QString MSelectDataSourceDialog::getSelectedDataSourceID()
{
    int row = ui->dataFieldTable->currentRow();

    return getDataSourceIDFromRow(row);
}


QList<QString> MSelectDataSourceDialog::getSelectedDataSourceIDs()
{
    QList<QTableWidgetItem*> items = ui->dataFieldTable->selectedItems();
    QList<QString> dataSources;
    QList<int> visitedRows;

    foreach (QTableWidgetItem *item, items)
    {
        const int row = item->row();
        if (visitedRows.contains(row)) { continue; }
        visitedRows.push_back(row);
        dataSources.push_back(getDataSourceIDFromRow(row));
    }

    return dataSources;
}


MSelectableDataSource MSelectDataSourceDialog::
checkIfSingleDataSourceWithSingleVariableIsPresent(bool *ok)
{
    if (ui->dataFieldTable->rowCount() != 1)
    {
        *ok = false;
        return MSelectableDataSource();
    }
    *ok = true;
    return getDataSourceFromRow(0);
}


bool MSelectDataSourceDialog::checkDataSourceForData(
        MWeatherPredictionDataSource *source)
{
    if (source == nullptr)
    {
        return false;
    }

    QStringList variables;
    QList<QDateTime> currentInitTimes;
    QList<QDateTime> currentValidTimes;
    bool validTimesMissing = true;

    // Check if data source contains init times, valid times and
    // ensemble members.
    QList<MVerticalLevelType> levelTypes = source->availableLevelTypes();
    for (int ilvl = 0; ilvl < levelTypes.size(); ilvl++)
    {
        MVerticalLevelType levelType = levelTypes.at(ilvl);

        variables = source->availableVariables(levelType);

        for (int ivar = 0; ivar < variables.size(); ivar++)
        {
            QString var = variables.at(ivar);
            currentInitTimes =
                    source->availableInitTimes(levelType, var);
            if (currentInitTimes.size() == 0)
            {
                continue;
            }

            validTimesMissing = true;
            for (int iInitTime = 0; iInitTime < currentInitTimes.size();
                 iInitTime++)
            {
                QDateTime initTime = currentInitTimes.at(iInitTime);
                currentValidTimes = source->availableValidTimes(levelType,
                                                                var,
                                                                initTime);
                if (currentValidTimes.size() == 0)
                {
                    continue;
                }
                validTimesMissing = false;
            } // initTimes

            if (validTimesMissing)
            {
                continue;
            }

            if (source->availableEnsembleMembers(levelType, var).size() == 0)
            {
                continue;
            }

            return true;
        } // variables
    } // levelTypes

    return false;
}


bool MSelectDataSourceDialog::checkForTrajectoryDataSource(QString dataSourceID)
{
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    if (!dynamic_cast<MTrajectoryDataSource*>(
                sysMC->getDataSource(dataSourceID + QString(" Reader"))))
    {
        return false;
    }

    if (!dynamic_cast<MTrajectoryNormalsSource*>(
                sysMC->getDataSource(dataSourceID  + QString(" Normals"))))
    {
        return false;
    }

    if (!dynamic_cast<MTrajectoryFilter*>(
                sysMC->getDataSource(dataSourceID + QString(" timestepFilter"))))
    {
        return false;
    }

    return true;
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/


int MSelectDataSourceDialog::exec()
{
    // Test if variables or data sources to select are available. If not, inform
    // user and return QDialog::Rejected without executing the dialog.
    if (variableAvailable && dataSourceAvailable)
    {
        return QDialog::exec();
    }
    else
    {
        // Dialog was executed for variable selection therefore
        // dataSourcesAvailable is set initially to true and only
        // variableAvailable is used as indicator.
        if (dataSourceAvailable)
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("No variables available to select.");
            msgBox.exec();
        }
        // Dialog was executed for data source selection therefore
        // variableAvailable is set initially to true and only
        // dataSourcesAvailable is used as indicator.
        else
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("No data sources available to select.");
            msgBox.exec();
        }
        return QDialog::Rejected;
    }
}


/******************************************************************************
***                            PRIVATE METHODS                              ***
*******************************************************************************/

void MSelectDataSourceDialog::createDataSourceEntries(
        const QList<MVerticalLevelType> supportedTypes)
//        const QList<QString>&           supportedFilters)
{
    // Set the data field table's header.
    QTableWidget *table = ui->dataFieldTable;
    table->setColumnCount(6);
    QStringList headerLabels;
    headerLabels << "Dataset" << "Vertical Dimension" << "Variable Name"
                 << "Standard Name" << "Long Name" << "Units";
    table->setHorizontalHeaderLabels(headerLabels);

    // Loop over all data loaders registered with the resource manager.
    MSystemManagerAndControl* sysMC = MSystemManagerAndControl::getInstance();

    variableAvailable = false;

    QStringList dataSources = sysMC->getDataSourceIdentifiers();
    for (int idl = 0; idl < dataSources.size(); idl++)
    {
        MWeatherPredictionDataSource* source =
                dynamic_cast<MWeatherPredictionDataSource*>
                (sysMC->getDataSource(dataSources[idl]));

        if (source == nullptr) continue;

        // Fill the data field table with elements that are available from the
        // current data loader.

        // Loop over all level types..
        QList<MVerticalLevelType> levelTypes = source->availableLevelTypes();
        for (int ilvl = 0; ilvl < levelTypes.size(); ilvl++)
        {
            MVerticalLevelType lvl = levelTypes.at(ilvl);

            // do not list data sources of not supported level types
            if (!supportedTypes.contains(lvl)) { continue; }

//            // do not list data sources of not supported filters
//            bool isSupported = supportedFilters.contains("all");
//            if (!isSupported)
//            {
//                foreach (const auto& filter, supportedFilters)
//                {
//                    if (dataSources[idl].contains(filter))
//                    {
//                        isSupported = true;
//                        break;
//                    }
//                }
//            }
//
//            if (!isSupported) { continue; }

            // .. and all variables for the current level type ..
            QStringList variables = source->availableVariables(lvl);
            variableAvailable = variableAvailable || !variables.empty();
            for (int ivar = 0; ivar < variables.size(); ivar++)
            {
                QString var = variables.at(ivar);
                // Add a row to the table..
                int row = table->rowCount();
                table->setRowCount(row + 1);
                // .. and insert the element.
                table->setItem(row, 0, new QTableWidgetItem(dataSources[idl]));
                table->setItem(row, 1, new QTableWidgetItem(
                                   MStructuredGrid::verticalLevelTypeToString(lvl)));
                table->setItem(row, 2, new QTableWidgetItem(var));
                table->setItem(row, 3, new QTableWidgetItem(
                                   source->variableStandardName(lvl, var)));
                table->setItem(row, 4, new QTableWidgetItem(
                                   source->variableLongName(lvl, var)));
                table->setItem(row, 5, new QTableWidgetItem(
                                   source->variableUnits(lvl, var)));
            } // for (variables)

        } // for (level types)

    } // for (data loaders)

    // Resize the table's columns.
    table->resizeColumnsToContents();
}


void MSelectDataSourceDialog::createDataSourceEntries()
{
    QStringList variables;
    QList<QDateTime> currentInitTimes;
    QList<QDateTime> currentValidTimes;
    variables.clear();
    currentInitTimes.clear();
    currentValidTimes.clear();

    // Set the data field table's header.
    QTableWidget *table = ui->dataFieldTable;
    table->setColumnCount(1);
    table->setHorizontalHeaderLabels(QStringList("Dataset"));

    // Loop over all data loaders registered with the resource manager.
    MSystemManagerAndControl* sysMC = MSystemManagerAndControl::getInstance();

    dataSourceAvailable = false;

    QStringList dataSources = sysMC->getDataSourceIdentifiers();
    for (int idl = 0; idl < dataSources.size(); idl++)
    {
        MWeatherPredictionDataSource* source =
                dynamic_cast<MWeatherPredictionDataSource*>
                (sysMC->getDataSource(dataSources[idl]));

        if (source == nullptr)
        {
            continue;
        }

        // Only add data source to table if it contains init times, valid times
        // and ensemble member informations.
        if (checkDataSourceForData(source))
        {
            // Add a row to the table..
            int row = table->rowCount();
            table->setRowCount(row + 1);
            // .. and insert the element.
            table->setItem(row, 0, new QTableWidgetItem(dataSources[idl]));

            dataSourceAvailable = true;
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


void MSelectDataSourceDialog::createTrajectoryDataSourceEntries()
{
    // Set the data field table's header.
    QTableWidget *table = ui->dataFieldTable;
    table->setColumnCount(1);
    table->setHorizontalHeaderLabels(QStringList("Dataset"));
    table->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);

    // Loop over all data loaders registered with the resource manager.
    MSystemManagerAndControl* sysMC = MSystemManagerAndControl::getInstance();

    dataSourceAvailable = false;

    QStringList dataSources = sysMC->getDataSourceIdentifiers();
    for (int idl = 0; idl < dataSources.size(); idl++)
    {
        QString dataSourceID = dataSources[idl];

        if (checkForTrajectoryDataSource(dataSourceID))
        {
            // Add a row to the table..
            int row = table->rowCount();
            table->setRowCount(row + 1);
            // .. and insert the element.
            table->setItem(row, 0, new QTableWidgetItem(dataSources[idl]));

            dataSourceAvailable = true;
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


MSelectableDataSource MSelectDataSourceDialog::getDataSourceFromRow(int row)
{
    MSelectableDataSource dataSource;
    dataSource.dataSourceID = ui->dataFieldTable->item(row, 0)->text();
    dataSource.levelType = MStructuredGrid::verticalLevelTypeFromString(
                ui->dataFieldTable->item(row, 1)->text());

//TODO (mr, Feb2015) -- make this function return the std.name by default.
//                      This requires changes in MWeatherPredictionDataSource->
//                      availableVariables(), which needs to return std.names as well
    // Use the CF standard name as variable name, as long as a standard name
    // exists. Otherwise use the NetCDF/Grib variable name.
//    QString varName = ui->dataFieldTable->item(row, 3)->text();
//    if (varName.isEmpty())
//        varName = ui->dataFieldTable->item(row, 2)->text();
//    dataSource.variableName = varName;

    QString varName = ui->dataFieldTable->item(row, 2)->text();
    dataSource.variableName = varName;

    return dataSource;
}


QString MSelectDataSourceDialog::getDataSourceIDFromRow(int row)
{
    return ui->dataFieldTable->item(row, 0)->text();
}

} // namespace Met3D
