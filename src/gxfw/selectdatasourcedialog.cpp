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

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSelectDataSourceDialog::MSelectDataSourceDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MSelectDataSourceDialog)
{
    ui->setupUi(this);

    QList<MVerticalLevelType> types;
    types << AUXILIARY_PRESSURE_3D << HYBRID_SIGMA_PRESSURE_3D
          << LOG_PRESSURE_LEVELS_3D << PRESSURE_LEVELS_3D
          << SURFACE_2D << POTENTIAL_VORTICITY_2D;

    createDataSourceEntries(types);
}


MSelectDataSourceDialog::MSelectDataSourceDialog(
        const QList<MVerticalLevelType>& supportList, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MSelectDataSourceDialog)
{
    ui->setupUi(this);
    createDataSourceEntries(supportList);
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

    for (const auto& item : items)
    {
        const int row = item->row();
        if (visitedRows.contains(row)) { continue; }
        visitedRows.push_back(row);
        dataSources.push_back(getDataSourceFromRow(row));
    }

    return dataSources;
}


/******************************************************************************
***                            PRIVATE METHODS                              ***
*******************************************************************************/

void MSelectDataSourceDialog::createDataSourceEntries(
        QList<MVerticalLevelType> supportedTypes)
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
            if (!supportedTypes.contains(lvl)) continue;

            // .. and all variables for the current level type ..
            QStringList variables = source->availableVariables(lvl);
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

} // namespace Met3D
