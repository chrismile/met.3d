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
#include "scenemanagementdialog.h"
#include "ui_scenemanagementdialog.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msystemcontrol.h"
#include "gxfw/mscenecontrol.h"
#include "gxfw/actorcreationdialog.h"
#include "mainwindow.h"


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSceneManagementDialog::MSceneManagementDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MSceneManagementDialog)
{
    ui->setupUi(this);
    ui_sceneViewComboBoxes << ui->sceneView1ComboBox
                           << ui->sceneView2ComboBox
                           << ui->sceneView3ComboBox
                           << ui->sceneView4ComboBox;

    // bind indices via signal manager to one slot changeScene
    signalMapperSceneManagement = new QSignalMapper(this);
    for (int i = 0; i < ui_sceneViewComboBoxes.size(); i++)
    {
        signalMapperSceneManagement->setMapping(
                    ui_sceneViewComboBoxes[i], i);
        connect(ui_sceneViewComboBoxes[i],
                SIGNAL(activated(int)),
                signalMapperSceneManagement,
                SLOT(map()));
    }

    connect(signalMapperSceneManagement,
            SIGNAL(mapped(int)),
            this,
            SLOT(changeSceneView(int)));

    // connect buttons to corresponding actions
    connect(ui->createSceneButton, SIGNAL(clicked()),
            this, SLOT(createScene()));
    connect(ui->deleteSceneButton, SIGNAL(clicked()),
            this, SLOT(deleteScene()));
    connect(ui->createActorButton, SIGNAL(clicked()),
            this, SLOT(createActor()));
    connect(ui->createActorFromFileButton, SIGNAL(clicked()),
            this, SLOT(createActorFromFile()));
    connect(ui->deleteActorButton, SIGNAL(clicked()),
            this, SLOT(deleteActor()));
    connect(ui->actorUpPushButton, SIGNAL(clicked()),
            this, SLOT(moveActorUpward()));
    connect(ui->actorDownPushButton, SIGNAL(clicked()),
            this, SLOT(moveActorDownward()));
    connect(ui->actorPoolListWidget, SIGNAL(currentRowChanged(int)),
            this, SLOT(sceneActorConnection(int)));
    connect(ui->scenePoolListWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            this, SLOT(renameScene(QListWidgetItem*)));
    connect(ui->scenePoolListWidget, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(showRenderQueue(QListWidgetItem*)));
    connect(ui->actorPoolListWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            this, SLOT(renameActor(QListWidgetItem*)));
    connect(ui->actorScenesListWidget, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(changeActorSceneConnection(QListWidgetItem*)));
}


MSceneManagementDialog::~MSceneManagementDialog()
{
    delete ui;
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MSceneManagementDialog::createScene()
{
    bool ok;
    QString sceneName = QInputDialog::getText(
                this, "New scene",
                "Please enter a name for the new scene:", QLineEdit::Normal,
                "New scene", &ok);

    if (ok && !sceneName.isEmpty())
    {
        MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
        MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

        // two scenes should not have the same name
        if (glRM->getScene(sceneName))
        {
            QMessageBox msg;
            msg.setWindowTitle("Error");
            msg.setText("Scene ''" + sceneName + "'' already exists!");
            msg.setIcon(QMessageBox::Warning);
            msg.exec();
            return;
        }

        MSceneControl *scene = new MSceneControl(sceneName);
        glRM->registerScene(scene);
        sysMC->getMainWindow()->dockSceneControl(scene);

        new QListWidgetItem(sceneName, ui->scenePoolListWidget);
        new QListWidgetItem(sceneName, ui->actorScenesListWidget);

        for (int j = 0; j < ui_sceneViewComboBoxes.size(); j++)
        {
            ui_sceneViewComboBoxes[j]->addItem(sceneName);
        }
    }
}


void MSceneManagementDialog::deleteScene()
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    if (glRM->getScenes().size() == 1)
    {
        QMessageBox msgBox;
        msgBox.setText("Cannot delete scene, at least one scene must exist.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
        return;
    }

    const int index = ui->scenePoolListWidget->currentRow();
    const QString name = ui->scenePoolListWidget->currentItem()->text();

    QMessageBox msgBox;
    msgBox.setText(QString("Scene \"%1\" will be deleted.").arg(name));
    msgBox.setInformativeText("Please confirm.");
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.setIcon(QMessageBox::Warning);
    int ret = msgBox.exec();

    if (ret == QMessageBox::Ok)
    {
        LOG4CPLUS_DEBUG(mlog, "deleting scene " << name.toStdString());

        // Remove scene name from UI elements.
        ui->scenePoolListWidget->takeItem(index);
        ui->actorScenesListWidget->takeItem(index);

        MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
        QVector<MSceneViewGLWidget*> glWidgets = sysMC->getMainWindow()->getGLWidgets();

        for (int j = 0; j < ui_sceneViewComboBoxes.size(); j++)
        {
            const QString oldSceneName = ui_sceneViewComboBoxes[j]->currentText();

            // Remove the scene from the views' combo boxes. If one view is
            // connected with the scene to be removed the current index of its
            // combox box will be the one that is removed. Hence, a
            // currentIndexChanged signal is triggered, which changes the
            // view's scene via changeScene().
            ui_sceneViewComboBoxes[j]->removeItem(index);

            if(name == oldSceneName)
            {
                // update the scene view gl widgets
                const QString newSceneName = ui_sceneViewComboBoxes[j]->currentText();
                MSceneControl* newScene = glRM->getScene(newSceneName);
                glWidgets[j]->setScene(newScene);
            }
        }

        // Remove dock widget.
        sysMC->getMainWindow()->removeControl(glRM->getScene(name));

        // Remove scene from pool of managed scenes.
        glRM->deleteScene(name);
    }
}


void MSceneManagementDialog::renameScene(QListWidgetItem* item)
{
    bool ok;
    QString oldName = item->text();
    QString sceneName = QInputDialog::getText(
                this, "Change scene name",
                "Please enter a new name for the scene:", QLineEdit::Normal,
                oldName, &ok);

    if (ok && !sceneName.isEmpty())
    {
        MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
        MSceneControl* scene = glRM->getScene(oldName);

        // two scenes should not have the same name
        if (glRM->getScene(sceneName))
        {
            QMessageBox msg;
            msg.setWindowTitle("Error");
            msg.setText("Scene ''" + sceneName + "'' already exists!");
            msg.setIcon(QMessageBox::Warning);
            msg.exec();
            return;
        }

        item->setText(sceneName);
        const int index = ui->scenePoolListWidget->row(item);
        ui->actorScenesListWidget->item(index)->setText(sceneName);

        scene->setName(sceneName);

        // update scene view combo boxes
        for (const auto& comboBox : ui_sceneViewComboBoxes)
        {
            for (int i = 0; i < comboBox->count(); ++i)
            {
                if (oldName == comboBox->itemText(i))
                {
                    comboBox->setItemText(i, sceneName);
                    break;
                }
            }
        }
        MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();

        // update dock widget
        sysMC->getMainWindow()->changeDockedSceneName(oldName, sceneName);

        // update labels of GL widgets
        QVector<MSceneViewGLWidget*> glWidgets =
                sysMC->getMainWindow()->getGLWidgets();

        for (auto& glWidget : glWidgets)
        {
            glWidget->updateSceneLabel();
        }
    }
}


void MSceneManagementDialog::changeActorSceneConnection(QListWidgetItem* item)
{
    ui->actorScenesListWidget->clearSelection();

    const bool isChecked = (item->checkState() == Qt::Checked);

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // get current selected actor
    QListWidgetItem* actorItem = ui->actorPoolListWidget->currentItem();

    if (!actorItem) { return; }

    const QString actorName = actorItem->text();
    MActor* actor = glRM->getActorByName(actorName);

    if(!actor) { return; }

    // get current selected scene
    const QString sceneName = item->text();
    MSceneControl* scene = glRM->getScene(sceneName);

    int index = -1;
    // sometimes order of render queue is important, especially when
    // rendering opaque and non-opaque objects
    //if (dynamic_cast<MMapActor*>(actor)) { index = 0; }
    //else if (dynamic_cast<MGraticuleActor*>(actor)) { index = 1; }

    // synchronize render queue table whenever actor is added to or removed from scene
    QListWidgetItem* curSceneItem = ui->scenePoolListWidget->currentItem();

    if (isChecked)
    {
        // only add actor to scene if actor is not connected to scene
        if (!actor->getScenes().contains(scene))
        {
            item->setBackgroundColor(QColor(255,255,175));
            scene->addActor(actor, index);

            // synchronize with render queue of scene
            if (curSceneItem && curSceneItem->text() == sceneName)
            {
                const int index = ui->renderQueueTableWidget->rowCount();
                ui->renderQueueTableWidget->insertRow(index);
                ui->renderQueueTableWidget->setItem(index, 0, new QTableWidgetItem(actorName));
            }
        }
    }
    else
    {
        // only remove actor from scene if actor is connected to scene
        if (actor->getScenes().contains(scene))
        {
            // synchronize with render queue of scene
            if (curSceneItem && curSceneItem->text() == sceneName)
            {
                const int actorRow = scene->getActorRenderID(actorName);
                ui->renderQueueTableWidget->removeRow(actorRow);
            }

            item->setBackgroundColor(QColor(255,200,200));
            scene->removeActorByName(actorName);
        }
    }
}


void MSceneManagementDialog::createActor()
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    if (actorCreationDialog.exec() == QDialog::Rejected) return;

    const QString actorName = actorCreationDialog.getActorName();

    // if there's already an actor with the same name
    // do not create a new actor
    if (glRM->getActorByName(actorName))
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("Actor ''" + actorName + "'' already exists!");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }

    MActor* actor = actorCreationDialog.createActorInstance();

    // if no actor was created then return;
    if (!actor) return;

    // initialize all shaders and graphical resources of actor
    actor->initialize();

    // register actor in resource manager
    glRM->registerActor(actor);

    // update GUI elements
    QListWidgetItem* item = new QListWidgetItem(actorName, ui->actorPoolListWidget);
    // and select it in actor pool list
    ui->actorPoolListWidget->setCurrentItem(item);
}


void MSceneManagementDialog::createActorFromFile()
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    QString configfile = QFileDialog::getOpenFileName(
                glRM,
                "Load actor configuration",
                "data/actorconfig",
                "Actor configuration files (*.actor.conf)");

    if (configfile.isEmpty()) return;

    LOG4CPLUS_DEBUG(mlog, "loading configuration file "
                    << configfile.toStdString() << " ...");

    // Find actor that can be created with the specified config file and
    // create a new instance.
    foreach (MAbstractActorFactory* factory, glRM->getActorFactories())
    {
        if (factory->acceptSettings(configfile))
        {
            LOG4CPLUS_DEBUG(mlog, "creating actor of type "
                            << factory->getName().toStdString());

            MActor *actor = factory->create(configfile);
            if (!actor) return;

            QString actorName = actor->getName();
            bool ok = false;
            while (actorName.isEmpty() || glRM->getActorByName(actorName))
            {
                actorName = QInputDialog::getText(
                            this, "Change actor name",
                            "The given actor name already exists, please enter a new one:",
                            QLineEdit::Normal,
                            actorName, &ok);

                if (!ok)
                {
                    // The user has pressed the "Cancel" button.
                    delete actor;
                    return;
                }

                actor->setName(actorName);
            }

            actor->initialize();
            glRM->registerActor(actor);

            // update GUI elements
            QListWidgetItem* item =
                    new QListWidgetItem(actorName, ui->actorPoolListWidget);
            // and select it in actor pool list
            ui->actorPoolListWidget->setCurrentItem(item);

            return;
        }
    }

    LOG4CPLUS_WARN(mlog, "could not create actor from configuration file "
                   << configfile.toStdString() << " !");
}


void MSceneManagementDialog::deleteActor()
{
    // Is an actor selected?
    if(!ui->actorPoolListWidget->currentItem())
    {
        QMessageBox msgBox;
        msgBox.setText("Please select the actor you wish to delete.");
        msgBox.exec();
        return;
    }

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    const QString actorName = ui->actorPoolListWidget->currentItem()->text();
    MActor* actor = glRM->getActorByName(actorName);

    // Is this actor allowed to be deleted?
    if (!actor->getActorIsUserDeletable())
    {
        QMessageBox msgBox;
        msgBox.setText("This actor has a special role and cannot be deleted.");
        msgBox.exec();
        return;
    }

    // Check if the user really wants to delete the actor.
    QString msg = "Do you really want to remove actor ''" + actorName + "''?";

    // If the actor to be deleted is connected to other actors (e.g. transfer
    // functions), print a warning.
    QList<MActor*> connectedActors = glRM->getActorsConnectedTo(actor);
    if ( !connectedActors.empty() )
    {
        msg += "\n\n\rWARNING: ''" + actorName
                + "'' is connected to the following actors:\n\n\r";

        foreach (MActor* a, connectedActors) msg += a->getName() + "\n\r";
    }

    QMessageBox yesNoBox;
    yesNoBox.setWindowTitle("Delete actor");
    yesNoBox.setText(msg);
    yesNoBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    yesNoBox.setDefaultButton(QMessageBox::No);

    if (yesNoBox.exec() != QMessageBox::Yes) return;

    QList<MSceneControl*> actorScenes = actor->getScenes();

    // remove actor from its current scenes
    for (auto& scene : actorScenes)
    {
        // remove actor from render queue table
        QListWidgetItem* curSceneItem = ui->scenePoolListWidget->currentItem();

        if (curSceneItem && curSceneItem->text() == scene->getName())
        {
            const int actorRow = scene->getActorRenderID(actorName);
            ui->renderQueueTableWidget->removeRow(actorRow);
        }

        // and remove actor from the corresponding scene
        scene->removeActorByName(actorName);
    }

    // remove item from actor pool list
    QListWidgetItem* item =  ui->actorPoolListWidget->takeItem(
                             ui->actorPoolListWidget->currentRow());

    delete item;

    // delete resources of actor
    glRM->deleteActor(actor);
}


void MSceneManagementDialog::changeSceneView(const int viewIndex)
{
    int sceneIndex = ui_sceneViewComboBoxes[viewIndex]->currentIndex();

    if (sceneIndex == -1) { return; }

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    QList<MSceneControl*> sceneControls = glRM->getScenes();

    MSceneViewGLWidget* glWidget = sysMC->getMainWindow()->getGLWidgets()[viewIndex];
    glWidget->setScene(sceneControls.at(sceneIndex));
}


void MSceneManagementDialog::renameActor(QListWidgetItem* item)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    bool ok = false;
    QString actorName = item->text();
    QString oldActorName = actorName;
    MActor* actor = glRM->getActorByName(actorName);
    if (!actor) return;

    while (actorName.isEmpty() || glRM->getActorByName(actorName))
    {
        actorName = QInputDialog::getText(
                    this, "Rename actor",
                    "Please enter a new name for the actor::",
                    QLineEdit::Normal,
                    actorName, &ok);

        if (!ok) return;
    }

    // Change name of actor pool list item.
    item->setText(actorName);

    // Change name of render queue table item(s).
    QList<QTableWidgetItem*> renderQueueItems =
            ui->renderQueueTableWidget->findItems(oldActorName, Qt::MatchExactly);
    foreach(QTableWidgetItem* rqitem, renderQueueItems)
        rqitem->setText(actorName);

    // Change name of actor (also changes name of property browser).
    actor->setName(actorName);
}


void MSceneManagementDialog::showRenderQueue(QListWidgetItem* item)
{
    const QString sceneName = item->text();

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    MSceneControl* scene = glRM->getScene(sceneName);

    if (!scene) { return; }

    const QVector<MActor*>& renderQueue = scene->getRenderQueue();

    ui->renderQueueTableWidget->clearContents();
    ui->renderQueueTableWidget->setRowCount(renderQueue.size());

    for (int i = 0; i < renderQueue.size(); ++i)
    {
        MActor* actor = renderQueue[i];
        QTableWidgetItem* item = new QTableWidgetItem(actor->getName());
        ui->renderQueueTableWidget->setItem(i, 0, item);
    }

    QHeaderView* hv = ui->renderQueueTableWidget->verticalHeader();
    hv->setResizeMode(QHeaderView::Fixed);
    hv->setDefaultSectionSize(18);
    hv->setMovable(false);
}


void MSceneManagementDialog::moveActorUpward()
{
    const int curActorRow = ui->renderQueueTableWidget->currentRow();
    if (curActorRow <= 0) { return; }
    const int curSceneRow = ui->scenePoolListWidget->currentRow();
    if (curSceneRow == -1) { return; }
    const QString sceneName = ui->scenePoolListWidget->item(curSceneRow)->text();

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    MSceneControl* scene = glRM->getScene(sceneName);

    QVector<MActor*>& renderQueue = scene->getRenderQueue();

    // swap elements in render queue
    MActor* oldActor = renderQueue[curActorRow - 1];
    MActor* actor = renderQueue[curActorRow];
    renderQueue[curActorRow - 1] = actor;
    renderQueue[curActorRow] = oldActor;

    // swap entries in table
    QTableWidgetItem* newItem = ui->renderQueueTableWidget->takeItem(curActorRow, 0);
    QTableWidgetItem* oldItem = ui->renderQueueTableWidget->takeItem(curActorRow - 1, 0);
    ui->renderQueueTableWidget->setItem(curActorRow - 1, 0, newItem);
    ui->renderQueueTableWidget->setItem(curActorRow, 0, oldItem);

    // reorder properties so that they correspond to current render queue
    QtProperty* oldActorProp = oldActor->getPropertyGroup();
    QtProperty* actorProp = actor->getPropertyGroup();

    QtTreePropertyBrowser* prop = scene->getActorPropertyBrowser();
    prop->removeProperty(oldActorProp); // remove old actor property from the list
    // and add it after the current one
    QtBrowserItem* item = prop->insertProperty(oldActorProp,actorProp);
    // set background color to light yellow
    prop->setBackgroundColor(item, QColor(255, 255, 191));
    // and collapse the tree of the old actor
    scene->collapseActorPropertyTree(oldActor);

    // select the row where the actor was moved to
    ui->renderQueueTableWidget->selectRow(curActorRow - 1);
}


void MSceneManagementDialog::moveActorDownward()
{
    const int curActorRow = ui->renderQueueTableWidget->currentRow();
    if (curActorRow == -1 || curActorRow >= ui->renderQueueTableWidget->rowCount() - 1) { return; }
    const int curSceneRow = ui->scenePoolListWidget->currentRow();
    if (curSceneRow == -1) { return; }
    const QString sceneName = ui->scenePoolListWidget->item(curSceneRow)->text();

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    MSceneControl* scene = glRM->getScene(sceneName);

    QVector<MActor*>& renderQueue = scene->getRenderQueue();

    // swap elements in render queue
    MActor* oldActor = renderQueue[curActorRow + 1];
    MActor* actor = renderQueue[curActorRow];
    renderQueue[curActorRow + 1] = actor;
    renderQueue[curActorRow] = oldActor;

    // swap entries in table
    QTableWidgetItem* newItem = ui->renderQueueTableWidget->takeItem(curActorRow, 0);
    QTableWidgetItem* oldItem = ui->renderQueueTableWidget->takeItem(curActorRow + 1, 0);
    ui->renderQueueTableWidget->setItem(curActorRow + 1, 0, newItem);
    ui->renderQueueTableWidget->setItem(curActorRow, 0, oldItem);

    // reorder properties so that they correspond to current render queue
    QtProperty* oldActorProp = oldActor->getPropertyGroup();
    QtProperty* actorProp = actor->getPropertyGroup();

    QtTreePropertyBrowser* prop = scene->getActorPropertyBrowser();
    prop->removeProperty(actorProp); // remove old actor property from the list
    // and add it after the current one
    QtBrowserItem* item = prop->insertProperty(actorProp,oldActorProp);
    // set background color to light yellow
    prop->setBackgroundColor(item, QColor(255, 255, 191));
    // and collapse the tree of the old actor
    scene->collapseActorPropertyTree(actor);

    // select the row where the actor was moved to
    ui->renderQueueTableWidget->selectRow(curActorRow + 1);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MSceneManagementDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();

    // Obtain available scenes.
    QList<MSceneControl*> sceneControls = glRM->getScenes();

    // Clear the content of all widgets and elements.
    ui->scenePoolListWidget->clear();
    ui->scenePoolListWidget->clearSelection();
    ui->actorPoolListWidget->clear();
    ui->actorPoolListWidget->clearSelection();
    ui->actorScenesListWidget->clear();
    ui->sceneView1ComboBox->clear();
    ui->sceneView2ComboBox->clear();
    ui->sceneView3ComboBox->clear();
    ui->sceneView4ComboBox->clear();

    for (auto& comboBox : ui_sceneViewComboBoxes)
    {
        comboBox->clear();
    }

    // Populate GUI elements depending on the current system configuration.
    for (int i = 0; i < sceneControls.size(); i++)
    {
        QString sceneName = sceneControls.at(i)->getName();

        new QListWidgetItem(sceneName, ui->scenePoolListWidget);
        QListWidgetItem* item = new QListWidgetItem(sceneName,
                                                    ui->actorScenesListWidget);
        item->setCheckState(Qt::Unchecked);

        for (int j = 0; j < ui_sceneViewComboBoxes.size(); j++)
        {
            ui_sceneViewComboBoxes[j]->addItem(sceneName);
        }
    }

    QVector<MSceneViewGLWidget*>& sceneViewGLWidgets =
                                        sysMC->getMainWindow()->getGLWidgets();

    // Set the current indices for the scene view combo boxes.
    for (int i = 0; i < sceneViewGLWidgets.size(); i++)
    {
        QString sceneName = sceneViewGLWidgets[i]->getScene()->getName();
        int     index     = ui_sceneViewComboBoxes[i]->findText(sceneName);
        ui_sceneViewComboBoxes[i]->setCurrentIndex(index);
    }

    // Create actor view list from actor pool.
    foreach (MActor* actor, glRM->getActors())
    {
        QString actorName = actor->getName();

        // Skip special actors.
        // See: MGLResourcesManager::getSceneRotationCentreSelectionPoleActor()
        if (actorName == "SelectSceneRotationCentreActor") continue;

        new QListWidgetItem(actorName, ui->actorPoolListWidget);
    }

    // Set the default settings for render queue table in a selected scene.
    ui->renderQueueTableWidget->clear();
    ui->renderQueueTableWidget->setRowCount(0);
    ui->renderQueueTableWidget->horizontalHeader()->hide();
    ui->renderQueueTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->renderQueueTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->renderQueueTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->renderQueueTableWidget->setColumnCount(1);
    ui->renderQueueTableWidget->setColumnWidth(0, 150);
}


/******************************************************************************
***                            PRIVATE METHODS                              ***
*******************************************************************************/

void MSceneManagementDialog::sceneActorConnection(const int actorIndex)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    QListWidgetItem* item = ui->actorPoolListWidget->item(actorIndex);

    // if nothing was selected -> return
    if (!item) { return; }

    MActor *actor = glRM->getActorByName(item->text());

    if (!actor) { return; }

    const QList<MSceneControl*>& actorScenes = actor->getScenes();

    ui->actorScenesListWidget->clearSelection();

    QList<MSceneControl*> sceneControls = glRM->getScenes();

    // clear check states
    for (int i = 0; i < sceneControls.size(); ++i)
    {
        ui->actorScenesListWidget->item(i)->setText(sceneControls[i]->getName());
        ui->actorScenesListWidget->item(i)->setBackgroundColor(QColor(255,255,255));
        ui->actorScenesListWidget->item(i)->setCheckState(Qt::Unchecked);
    }

    for (int i = 0; i < actorScenes.size(); i++)
    {
        for (int j = 0; j < sceneControls.size(); j++)
        {
            if (actorScenes.at(i) == sceneControls.at(j))
            {
                QListWidgetItem* item = ui->actorScenesListWidget->item(j);
                item->setBackgroundColor(QColor(200,250,200));
                item->setCheckState(Qt::Checked);

                break;
            }
        }
    }
}


} // namespace Met3D
