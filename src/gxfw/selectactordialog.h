/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Philipp Kaiser
**  Copyright 2018 Marc Rautenhaus
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
#ifndef SELECTACTORDIALOG_H
#define SELECTACTORDIALOG_H

// standard library imports

// related third party imports
#include <QDialog>

namespace Ui {
class MSelectDataSourceDialog;
}


namespace Met3D
{

enum MSelectActorType {
    POLE_ACTOR              = 0,
    HORIZONTALSECTION_ACTOR = 1,
    VERTICALSECTION_ACTOR   = 2,
    BOX_ACTOR               = 3
};

struct MSelectableActor
{
    QString            actorName;
};

/**
  @brief MSelectActorDialog implements a dialog from which the user can
  select an existing actor.
 */
class MSelectActorDialog : public QDialog
{
    Q_OBJECT
    
public:
    /**
      Constructs a new dialog. The dialog's data field table is filled with a
      list of the actor registered with @ref MGLResourcesManager.

      Displayed actors are limited to those of the types defined in @p types.
     */
    explicit MSelectActorDialog(QList<MSelectActorType> types,
                                QWidget *parent = 0);

    ~MSelectActorDialog();

    MSelectableActor getSelectedActor();

    QList<MSelectableActor> getSelectedActors();


public slots:
    /**
      @brief Reimplemented exec() to avoid execusion of dialog if no actors
      are available to select.

      Prints warning corresponding to the selection dialog executed.
     */
    int exec();

private:
    Ui::MSelectDataSourceDialog *ui;

    /**
      Creates table entries for actor selection dialog.
     */
    void createActorEntries(QList<MSelectActorType> types);

    MSelectableActor getActorFromRow(int row);

    bool actorsAvailable;
};

} // namespace Met3D

#endif // SELECTACTORDIALOG_H
