/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016-2018 Marc Rautenhaus
**  Copyright 2016      Christoph Heidelmann
**  Copyright 2017-2018 Bianca Tost
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
#ifndef MADDDATASETDIALOG_H
#define MADDDATASETDIALOG_H

// standard library imports

// related third party imports
#include <QDialog>
#include <QAbstractButton>
#include <QSettings>

// local application imports


namespace Ui {
class MAddDatasetDialog;
}


namespace Met3D
{

enum MNWPReaderFileFormat
{
    INVALID_FORMAT  = 0,
    CF_NETCDF       = 1,
    ECMWF_GRIB      = 2
};

enum PipelineType
{
    INVALID_PIPELINE_TYPE  = 0,
    NWP_PIPELINE           = 1,
    TRAJECTORIES_PIPELINE  = 2
};


struct MNWPPipelineConfigurationInfo
{
    QString name;
    QString fileDir;
    QString fileFilter;
    QString schedulerID;
    QString memoryManagerID;
    MNWPReaderFileFormat dataFormat;
    bool enableRegridding;
    bool enableProbabiltyRegionFilter;
    bool treatRotatedGridAsRegularGrid;
    bool treatProjectedGridAsRegularLonLatGrid;
    QString surfacePressureFieldType;
    bool convertGeometricHeightToPressure_ICAOStandard;
    QString auxiliary3DPressureField;
    bool disableGridConsistencyCheck;
    QString inputVarsForDerivedVars;
};


struct MTrajectoriesPipelineConfigurationInfo
{
    QString name;
    bool precomputed;
    QString fileDir;
    QString schedulerID;
    QString memoryManagerID;
    bool boundaryLayerTrajectories;
    QString NWPDataset;
    QString windEastwardVariable;
    QString windNorthwardVariable;
    QString windVerticalVariable;
    QString auxDataVariablesInputStr;
    QString verticalLevelType;
};


/**

 */
class MAddDatasetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MAddDatasetDialog(QWidget *parent = 0);
    ~MAddDatasetDialog();

    PipelineType getSelectedPipelineType();

    MNWPPipelineConfigurationInfo getNWPPipelineConfigurationInfo();

    MTrajectoriesPipelineConfigurationInfo
    getTrajectoriesPipelineConfigurationInfo();

    void resetAddDatasetGUI();

    void saveConfiguration(QSettings *settings);
    void loadConfiguration(QSettings *settings);

public slots:
    void saveConfigurationToFile(QString filename = "");
    bool loadConfigurationFromFile(QString filename = "");

private slots:
    void browsePath();
    void inputFieldChanged();
    void selectedNWPDatasetChanged(QString dataset);
    void setDefaultMemoryManager();

protected:

private:
    Ui::MAddDatasetDialog *ui;
    QAbstractButton *okButton;
};

} // namespace Met3D

#endif // MADDDATASETDIALOG_H
