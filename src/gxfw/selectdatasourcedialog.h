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
#ifndef SELECTDATASOURCEDIALOG_H
#define SELECTDATASOURCEDIALOG_H

// standard library imports

// related third party imports
#include <QDialog>

// local application imports
#include "data/structuredgrid.h"


namespace Ui {
class MSelectDataSourceDialog;
}


namespace Met3D
{

struct MSelectableDataSource
{
    QString            dataSourceID;
    MVerticalLevelType levelType;
    QString            variableName;
};


/**
  @brief MSelectDataSourceDialog implements a dialog from which the user can
  select a data source and forecast variable to be added to an @ref
  MNWPMultiVarActor.
  */
class MSelectDataSourceDialog : public QDialog
{
    Q_OBJECT
    
public:
    /**
      Constructs a new dialog. The dialog's data field table is filled with a
      list of the data source registered with @ref MGLResourcesManager.
      */
    explicit MSelectDataSourceDialog(QWidget *parent = 0);

    explicit MSelectDataSourceDialog(const QList<MVerticalLevelType>& supportedTypes,
                                     QWidget *parent = 0);

    ~MSelectDataSourceDialog();
    
    MSelectableDataSource getSelectedDataSource();

    QList<MSelectableDataSource> getSelectedDataSources();

private:
    Ui::MSelectDataSourceDialog *ui;

    void createDataSourceEntries(QList<MVerticalLevelType> supportedTypes);

    MSelectableDataSource getDataSourceFromRow(int row);
};

} // namespace Met3D

#endif // SELECTDATASOURCEDIALOG_H
