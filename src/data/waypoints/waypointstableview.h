/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2016 Marc Rautenhaus
**  Copyright 2016      Bianca Tost
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
#ifndef MWAYPOINTSTABLEVIEW_H
#define MWAYPOINTSTABLEVIEW_H

// standard library imports

// related third party imports
#include <QMessageBox>
#include <QFileDialog>

// local application imports
#include <data/waypoints/waypointstablemodel.h>


namespace Ui {
    class MWaypointsView;
}

namespace Met3D
{

/**
  @brief Widget including a table that displays waypoints similar to the table
  view in the DLR Mission Support System.
  */
class MWaypointsView : public QWidget
{
    Q_OBJECT

public:
    explicit MWaypointsView(QWidget *parent = 0);

    ~MWaypointsView();

    void setWaypointsTableModel(MWaypointsTableModel *model);

private:
    Ui::MWaypointsView *ui;  
    MWaypointsTableModel *waypointsModel;

private slots:
    void addNewWaypoint();

    void deleteSelectedWaypoint();

    void saveTrack();

    void checkExistanceAndSave(QString filename);

    void saveAsTrack();

    void openTrack();
};

} // namespace Met3D

#endif // MWAYPOINTSTABLEVIEW_H
