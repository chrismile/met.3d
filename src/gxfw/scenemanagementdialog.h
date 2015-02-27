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
#ifndef SCENEMANAGEMENTDIALOG_H
#define SCENEMANAGEMENTDIALOG_H

// standard library imports

// related third party imports
#include <QDialog>
#include <QtGui>
#include <QtCore>

#include "gxfw/actorcreationdialog.h"

// local application imports

namespace Ui
{
class MSceneManagementDialog;
}


namespace Met3D
{

/**
 @brief Dialog for scene and actor management by the user (create/delete scenes
 and actors, configure which scene is displayed in which OpenGL widget and
 which actor appears in which scene).
*/
class MSceneManagementDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit MSceneManagementDialog(QWidget *parent = 0);

    ~MSceneManagementDialog();

public slots:
    void createScene();

    void deleteScene();

    void createActor();

    void createActorFromFile();

    void deleteActor();

    void renameScene(QListWidgetItem* item);

    void renameActor(QListWidgetItem* item);

    void changeSceneView(const int viewIndex);

    void changeActorSceneConnection(QListWidgetItem* item);

    void showRenderQueue(QListWidgetItem* item);

    void moveActorUpward();

    void moveActorDownward();

protected:
    /**
      Reimplemented from QDialog::showEvent(). Synchronizes dialog GUI elements
      with glResourcesManager.
     */
    void showEvent(QShowEvent *event) override;

private slots:
    /**
      Update scene list widget w.r.t. the currently selected actor.
     */
    void sceneActorConnection(const int actorIndex);

private:
    Ui::MSceneManagementDialog *ui;

    MActorCreationDialog actorCreationDialog;

    QSignalMapper* signalMapperSceneManagement;

    // Reference to the four scene view combo boxes, to facilitate loops.
    QList<QComboBox*> ui_sceneViewComboBoxes;
};

} // namespace Met3D

#endif // SCENEMANAGEMENTDIALOG_H
