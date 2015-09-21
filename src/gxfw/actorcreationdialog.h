/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Michael Kern
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
#ifndef ACTORCREATIONDIALOG_H
#define ACTORCREATIONDIALOG_H

// standard library imports
#include <map>

// related third party imports
#include <QDialog>

// local application imports
#include <gxfw/mactor.h>


namespace Ui
{
class MActorCreationDialog;
}


namespace Met3D
{

/**
  @brief Dialog for runtime actor creation. When shown, the dialog queries the
  list of available actor factories from @ref MGLResourcesManager and displays
  the list to the user. The user can choose an actor and specify a name.
 */
class MActorCreationDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit MActorCreationDialog(QWidget *parent = 0);

    ~MActorCreationDialog();

    /**
      Create a new actor of the type selected by the user.
     */
    MActor* createActorInstance();

    /**
      Returns the name of the currently selected actor factory.
     */
    QString getActorName() const;

public slots:
    /**
      Called when the user changes the type of the actor to be created.
      Updates a counter corresponding to the current actor that is used
      to add a number to the name of the new actor (e.g. Graticule, Graticule 1,
      Graticule 2, etc.).
     */
    void actorTypeChanged();

protected:
    /**
      Fills the GUI with the available actor factories.
     */
    void showEvent(QShowEvent *event) override;
    
private:
    Ui::MActorCreationDialog *ui;

};

} // namespace Met3D

#endif // ACTORCREATIONDIALOG_H
