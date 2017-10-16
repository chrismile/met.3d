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

class MWeatherPredictionDataSource;

struct MSelectableDataSource
{
    QString            dataSourceID;
    MVerticalLevelType levelType;
    QString            variableName;
};

enum MSelectDataSourceDialogType {
    SYNC_CONTROL     = 0,
    VARIABLES        = 1,
    TRAJECTORIES     = 2
};


/**
  @brief MSelectDataSourceDialog implements a dialog from which the user can
  select either a data source and forecast variable to be added to an @ref
  MNWPMultiVarActor, data sources to restrict a synchronisation control to
  their times and ensemble members or a trajectory data source.

  Which dialog is created depends on the constructor used to construct the
  dialog.
  */
class MSelectDataSourceDialog : public QDialog
{
    Q_OBJECT
    
public:
    /**
      Constructs a new dialog. The dialog's data field table is filled with a
      list of the data source registered with @ref MGLResourcesManager.

      Which dialog should be created is defined by @p type. If this
      constructor is called to create a variable selection dialog, it uses all
      vertical level types available.
      */
    explicit MSelectDataSourceDialog(MSelectDataSourceDialogType type,
                                     QWidget *parent = 0);

    /**
      Constructs a new dialog. The dialog's data field table is filled with a
      list of the variables of the data source registered with
      @ref MGLResourcesManager. This constructor is used to call a dialog for
      selecting variables.
      */
    explicit MSelectDataSourceDialog(const QList<MVerticalLevelType>& supportedTypes,
                                     QWidget *parent = 0);

    ~MSelectDataSourceDialog();
    
    MSelectableDataSource getSelectedDataSource();

    QList<MSelectableDataSource> getSelectedDataSources();

    QString getSelectedDataSourceID();

    QList<QString> getSelectedDataSourceIDs();

    /**
      checkIfSingleDataSourceWithSingleVariableIsPresent returns the data source
      containing the only variable if just one variable of the supported level
      Types is present.

      @param ok is set to true if only one variable is present and to false
      otherwise.
     */
    MSelectableDataSource checkIfSingleDataSourceWithSingleVariableIsPresent(bool *ok);

    /**
      Checks whether the @p source contains init times, valid times and
      ensemble members informations.

      Returns @return true if it contains all necessary data and @return false
      if not.
      Also used by @ref MSyncControl.
      */
    static bool checkDataSourceForData(MWeatherPredictionDataSource *source);

    /**
      Checks whether @p dataSourceID describes a data source for
      trajectories by checking for the data sources needed (reader, normals,
      timestepFilter).

      checkForTrajectoryDataSource returns @return true if the check was
      positive, @return false otherwise.
     */
    static bool checkForTrajectoryDataSource(QString dataSourceID);

public Q_SLOTS:
    /**
      @brief Reimplemented exec() to avoid execusion of dialog if no variables
      or data sources respectively are available to select.

      Prints warning corresponding to the selection dialog (variables or data
      sources) executed.
      */
    int exec();

private:
    Ui::MSelectDataSourceDialog *ui;

    /**
      Creates table entries for variable selection dialog restricted to
      @p supportedTypes.
      */
    void createDataSourceEntries(QList<MVerticalLevelType> supportedTypes);
    /**
      Creates table entries for data source selection dialog.
      */
    void createDataSourceEntries();
    /**
      Creates table entries for data source selection dialog restricted to
      trajectory data sources.
      */
    void createTrajectoryDataSourceEntries();

    MSelectableDataSource getDataSourceFromRow(int row);
    QString getDataSourceIDFromRow(int row);

    /**
      Indicator variable used for variable selection dialog to decide if at
      least one variable is available to select. It is set to true for data
      source selection dialog.
      */
    bool variableAvailable;
    /**
      Indicator variable used for  data source selection dialog to decide if at
      least one data source is available to select. It is set to true for
      variable selection dialog.
      */
    bool dataSourceAvailable;
};

} // namespace Met3D

#endif // SELECTDATASOURCEDIALOG_H
