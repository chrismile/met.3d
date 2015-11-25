/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Christoph Heidelmann
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
#ifndef MEMBERSELECTIONDIALOG_H
#define MEMBERSELECTIONDIALOG_H

// standard library imports

// related third party imports
#include <QDialog>

// local application imports


namespace Ui {
class MMemberSelectionDialog;
}

namespace Met3D
{

class MMemberSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MMemberSelectionDialog(QWidget *parent = 0);
    ~MMemberSelectionDialog();

    /**
      Set the list of available members that are displayed to the user.
     */
    void setAvailableEnsembleMembers(QSet<unsigned int> members);

    /**
      Pre-select a set of members.
     */
    void setSelectedMembers(QSet<unsigned int> members);

    QSet<unsigned int> getSelectedMembers();

private:
    Ui::MMemberSelectionDialog *ui;
};

} // namespace Met3D

#endif // MEMBERSELECTIONDIALOG_H
