/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Bianca Tost
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
#include "sessionmanagerdialog.h"
#include "ui_sessionmanagerdialog.h"

// standard library imports
#include <iostream>

// related third party imports
#include <QtCore>
#include <QInputDialog>
#include <QToolTip>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "mainwindow.h"
#include "msystemcontrol.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/transferfunction.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                               CONSTANTS                                 ***
*******************************************************************************/

QString MSessionManagerDialog::fileExtension = QString(".session.config");

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSessionManagerDialog::MSessionManagerDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MSessionManagerDialog),
    currentSession(""),
    path(""),
    loadOnStart(false),
    currentRevisionNumber(-1)
{
    ui->setupUi(this);

    // Setup list view.
    //=================
    sessionItemDelegate = new MSessionItemDelegate(this);

    sessionFileSystemModel = new MSessionFileSystemModel();
    sessionFileSystemModel->setFilter(QDir::Files);
    sessionFileSystemModel->setNameFilters(QStringList("*" + fileExtension));
    sessionFileSystemModel->setNameFilterDisables(false);

    ui->sessionsListView->setItemDelegate(sessionItemDelegate);
    ui->sessionsListView->setModel(sessionFileSystemModel);

    connect(ui->changeFolderButton, SIGNAL(clicked()),
            this, SLOT(changeDirectory()));
    connect(ui->newButton, SIGNAL(clicked()), this, SLOT(createNewSession()));
    connect(ui->cloneButton, SIGNAL(clicked()), this, SLOT(cloneSession()));
    connect(ui->switchToButton, SIGNAL(clicked()),
            this, SLOT(switchTo()));
    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(deleteSession()));
    connect(ui->reloadButton, SIGNAL(clicked()), this, SLOT(reloadSession()));
    connect(ui->saveButton, SIGNAL(clicked()), this, SLOT(saveSession()));
    connect(ui->saveAsButton, SIGNAL(clicked()), this, SLOT(saveSessionAs()));
    // Double click event on an item of the list widget should lead to rename of
    // the item.
    connect(ui->sessionsListView, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(renameItem(QModelIndex)));

    // directoryLoaded() is triggered everytime when the root directory or its
    // content change.
    connect(sessionFileSystemModel, SIGNAL(directoryLoaded(QString)),
            this, SLOT(fillSessionsList()));
    connect(sessionFileSystemModel, SIGNAL(directoryLoaded(QString)),
            this, SLOT(fillCurrentSessionHistoryList()));

    connect(ui->autoSaveSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(changeAutoSaveInterval(int)));    
    connect(ui->autoSaveCheckBox, SIGNAL(toggled(bool)),
            this, SLOT(onAutoSaveToggeled(bool)));
}

MSessionManagerDialog::~MSessionManagerDialog()
{
    delete ui;
    delete sessionItemDelegate;
    delete sessionFileSystemModel;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MSessionManagerDialog::initialize(
        QString sessionName, QString path, int autoSaveInterval,
        bool loadOnStart, bool saveOnApplicationExit,
        int maximumNumberOfSavedRevisions)
{
    // Session:
    // ========
    this->loadOnStart = loadOnStart;

    setSessionToCurrent(sessionName);

    // Max auto save interval = 86400 sec == 24 h.
    autoSaveInterval = min(autoSaveInterval, 86400);
    // Auto save:
    // ==========
    ui->saveOnAppCheckBox->setChecked(saveOnApplicationExit);
    // Use tool tip to show time interval splitted up to [h min sec].
    double autoSaveIntervalTime = double(ui->autoSaveSpinBox->value());
    QVector2D time;
    time.setX(floor(autoSaveIntervalTime / 3600.));
    autoSaveIntervalTime -= time.x() * 3600.;
    time.setY(floor(autoSaveIntervalTime / 60.));
    autoSaveIntervalTime -= time.y() * 60.;
    ui->autoSaveSpinBox->setToolTip(
                QString("[%1h %2min %3sec]")
                .arg(time.x()).arg(time.y()).arg(autoSaveIntervalTime));

    if (autoSaveInterval > 0)
    {
        ui->autoSaveCheckBox->setChecked(true);
        // Interval equals the default index therefore initialisation must be
        // initiated manually (No valueChanged signal).
        if (autoSaveInterval == ui->autoSaveSpinBox->value())
        {
            changeAutoSaveInterval(autoSaveInterval);
        }
        else
        {
            ui->autoSaveSpinBox->setValue(autoSaveInterval);
        }
    }
    else
    {
        ui->autoSaveCheckBox->setChecked(false);
    }

    // Don't save negative values for maximumNumberOfSavedRevisions.
    maximumNumberOfSavedRevisions = max(maximumNumberOfSavedRevisions, 0);
    this->maximumNumberOfSavedRevisions = maximumNumberOfSavedRevisions;

    // Directory:
    // ==========
    this->path = path;
    if (!QFileInfo(this->path).isWritable())
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle("Error");
        msgBox.setText("No write access to '" + this->path
                       + "'. \nPlease select a different directory in the"
                         " session manager to be able to load and save sessions.");
        msgBox.exec();
        // Disable auto save if Met3D has no write access to the directory.
        ui->autoSaveCheckBox->setChecked(false);
        ui->autoSaveCheckBox->setEnabled(false);
    }

    ui->folderPathLabel->setText(path);
    ui->folderPathLabel->setToolTip(path);

    sessionFileSystemModel->setRootPath(path);
    ui->sessionsListView->setRootIndex(sessionFileSystemModel->index(path));
}


void MSessionManagerDialog::loadSessionOnStart()
{
    loadSessionFromFile(currentSession);
}


void MSessionManagerDialog::loadWindowLayout()
{
    QString filename = QDir(path).absoluteFilePath(currentSession + fileExtension);
    if (QFile::exists(filename))
    {
        QSettings *settings = new QSettings(filename, QSettings::IniFormat);
        QStringList groups = settings->childGroups();
        if (groups.contains("MSession"))
        {
            settings->beginGroup("MSession");
            MSystemManagerAndControl::getInstance()->getMainWindow()
                    ->loadConfiguration(settings);
            settings->endGroup();
        }
        delete settings;
    }
}


void MSessionManagerDialog::switchToSession(QString sessionName)
{
    // Only switch to session if session is not the current session.
    if (sessionName != currentSession)
    {
        // Don't ask to save session if there is no session to save.
        if (currentSession != "")
        {
            QMessageBox::StandardButton reply =
                    QMessageBox::question(
                        this, "Switch session",
                        QString("Do you want to save current session '%1' before"
                                " switching session?").arg(currentSession),
                        QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
            if (reply == QMessageBox::Yes)
            {
                saveSession();
            }
        }
        loadSessionFromFile(sessionName);
        setSessionToCurrent(sessionName);
    }
}


void MSessionManagerDialog::revertCurrentSessionToRevision(QString revisionNumber)
{
    QString filename = currentSession + MSessionManagerDialog::fileExtension;

    // The current session is not saved as a revision file thus the filename may
    // only be adapted to the revision filename patter for revisions with numbers 
    // smaller than the current revision number.
    if (revisionNumber.toInt() < this->currentRevisionNumber)
    {
        filename = "." + filename + "." + revisionNumber;
    }

    loadSessionFromFile(filename, false);
}


bool MSessionManagerDialog::getAutoSaveSession()
{
    return ui->autoSaveCheckBox->isChecked();
}

/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MSessionManagerDialog::changeDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(
                this, tr("Select directory to store sessions"), path,
                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    // Return if no directory has been chosen.
    if (dir == "")
    {
        return;
    }
    if (!QFileInfo(dir).isWritable())
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle("Error");
        msgBox.setText("No write access to this directory.\n"
                       "Failed to change directory.");
        msgBox.exec();
        return;
    }
    // Only change path if different directory was chosen.
    if (path != dir)
    {
        // Set path to chosen directory.
        path = dir;
        ui->folderPathLabel->setText(path);
        ui->folderPathLabel->setToolTip(path);
        sessionFileSystemModel->setRootPath(path);
        ui->sessionsListView->setRootIndex(sessionFileSystemModel->index(path));
        ui->autoSaveCheckBox->setEnabled(true);
    }
}


void MSessionManagerDialog::createNewSession(bool createEmptySession,
                                             QString sessionName)
{
    // Don't save session if Met.3D has no write access to directory.
    if (!QFileInfo(path).isWritable())
    {
        QMessageBox::information(
                    this, "Unable to create new session.",
                    "No write access to directory.\n"
                    "Please select a different directory.");
        return;
    }

    bool ok = false;

    // Let the name input dialog reappear until the user enters a unique,
    // non-empty name or pushes cancel.
    while (sessionName.isEmpty() || !ok)
    {
        sessionName = sessionName
                + getAppendixWithSmallestIndexForUniqueName(sessionName);

        sessionName = QInputDialog::getText(
                    this, "Create new session",
                    "Please enter a name for the session",
                    QLineEdit::Normal,
                    sessionName, &ok);

        if (!ok)
        {
            return;
        }

        ok = isValidSessionName(sessionName);
    }

    if (createEmptySession)
    {
        resetSession();
        saveSessionToFile(sessionName);
    }
    else
    {
        // Save new session file.
        saveSessionToFile(sessionName);
    }
    // Set current to newly created session.
    setSessionToCurrent(sessionName);
}


void MSessionManagerDialog::reloadSession()
{
    if (currentSession != "")
    {
        loadSessionFromFile(currentSession);
    }
}


void MSessionManagerDialog::autoSaveSession()
{
    saveSession(true);
}


void MSessionManagerDialog::saveSession(bool autoSave)
{
    // Don't save session if Met.3D has no write access to the directory.
    if (!QFileInfo(path).isWritable())
    {
        QMessageBox::warning(
                    this, "Unable to save session.",
                    "No write access to directory.\n"
                    "Please select a different directory.");
        return;
    }

    if (currentSession != "")
    {
        saveSessionToFile(currentSession, autoSave);
    }
    // If no current session is set, ask the user to choose a name to create
    // a new session.
    else
    {
        createNewSession(false);
    }
}


void MSessionManagerDialog::saveSessionAs()
{
    if (currentSession != "")
    {
        // If present, use current session name to create name-suggestion for
        // new session.
        createNewSession(false, currentSession);
    }
    else
    {
        createNewSession(false);
    }
}


void MSessionManagerDialog::cloneSession()
{
    if (ui->sessionsListView->selectionModel()->selectedIndexes().size() == 0)
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("Please select session to clone.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }
    QString sessionToClone =
            ui->sessionsListView->currentIndex().data().toString();

    QString sessionName = sessionToClone;

    bool ok = false;

    while (sessionName.isEmpty() || !ok)
    {
        sessionName = sessionName
                + getAppendixWithSmallestIndexForUniqueName(sessionName);

        sessionName = QInputDialog::getText(
                    this, "Clone session " + sessionToClone,
                    "Please enter a name for the new session",
                    QLineEdit::Normal,
                    sessionName, &ok);

        if (!ok)
        {
            return;
        }

        ok = isValidSessionName(sessionName);
    }

    if (currentSession == sessionToClone)
    {
        QMessageBox::StandardButton reply =
                QMessageBox::question(
                    this, "Clone session " + sessionToClone,
                    QString("You are cloning the current session."
                            "\nIts save file might contain an old version of"
                            " the current session."
                            "\nDo you want to save the current session '%1'"
                            " before cloning it?").arg(currentSession),
                    QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,
                    QMessageBox::No);
        if (reply == QMessageBox::Yes)
        {
            saveSessionToFile(sessionName);
        }
        else if (reply == QMessageBox::Cancel)
        {
            return;
        }
    }
    QFile file(QDir(path).absoluteFilePath(sessionToClone + fileExtension));
    file.copy(QDir(path).absoluteFilePath(sessionName + fileExtension));
}


void MSessionManagerDialog::switchTo()
{
    // Only switch to session if user selected one.
    if (ui->sessionsListView->selectionModel()->selectedIndexes().size() == 0)
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("Please select session to switch to.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }

    switchToSession(ui->sessionsListView->currentIndex().data().toString());
}


void MSessionManagerDialog::renameItem(QModelIndex item)
{
    bool ok = false;
    QString currentName = item.data(Qt::DisplayRole).toString();
    QString newName = currentName;

    while (newName.isEmpty() || newName == currentName || !ok)
    {
        newName = QInputDialog::getText(
                    this, "Rename session " + currentName,
                    "Please enter a new name for the session",
                    QLineEdit::Normal,
                    currentName, &ok);

        if (!ok)
        {
            return;
        }

        // If user doesn't change the name but clicks the ok-button, do nothing
        // and return.
        if (newName == currentName)
        {
            return;
        }

        ok = isValidSessionName(newName);
    }

    if (currentName == currentSession)
    {
        setSessionToCurrent(newName);
    }

    // Rename file.
    QFile file(QDir(path).absoluteFilePath(currentName + fileExtension));
    file.rename(QDir(path).absoluteFilePath(newName + fileExtension));

    // Rename revision files.
    QStringList revisionSplit;
    QStringList revertSessionFileNameList;

    getCurrentSessionFileHistoryFileNameList(revertSessionFileNameList,
                                             currentName);

    foreach (QString sessionRevision, revertSessionFileNameList)
    {
        QFile file(QDir(path).absoluteFilePath(sessionRevision));
        revisionSplit = sessionRevision.split(".");
        QString newRevisionName = "." + newName + fileExtension + "."
                + revisionSplit.last();
        file.rename(QDir(path).absoluteFilePath(newRevisionName));
    }
}


void MSessionManagerDialog::fillSessionsList()
{
    QStringList sessionsList;
    QModelIndex rootIndex = sessionFileSystemModel->index(path);
    int rowCount = sessionFileSystemModel->rowCount(rootIndex);
    for (int row = 0; row < rowCount; row++)
    {
        QModelIndex rowIndex =
                sessionFileSystemModel->index(row, 0, rootIndex);
        sessionsList << sessionFileSystemModel->fileName(rowIndex);
    }

    // Sort list case insensitive (using qSort is case sensitive).
    std::sort(sessionsList.begin(), sessionsList.end(), [](QString a, QString b)
    {
        return a.toLower() <= b.toLower();
    });

    MSystemManagerAndControl::getInstance()->getMainWindow()
            ->onSessionsListChanged(&sessionsList, currentSession);
}


void MSessionManagerDialog::fillCurrentSessionHistoryList()
{
    // Reset session list and currentRevisionNumber.
    QStringList sessionsList;
    currentRevisionNumber = -1;

    QStringList fileNameList;
    getCurrentSessionFileHistoryFileNameList(fileNameList, currentSession);

    foreach (QString sessionRevision, fileNameList)
    {
        int revisionNumber = sessionRevision
                .split(".", QString::SkipEmptyParts).last().toInt();
        currentRevisionNumber = max(currentRevisionNumber, revisionNumber);

        // Extract index of the file name and start current session history
        // entry with it.
        QString entry = "Rev. " + QString::number(revisionNumber);

        QSettings *settings = new QSettings(
                    QDir(path).absoluteFilePath(sessionRevision),
                    QSettings::IniFormat);

        QStringList groups = settings->childGroups();
        if ( groups.contains("MSession")  )
        {
            settings->beginGroup("MSession");
            groups = settings->childGroups();
            if ( groups.contains("SessionDetails")  )
            {
                settings->beginGroup("SessionDetails");

                entry += ": " + settings->value("dateTime", "").toDateTime()
                        .toString(Qt::ISODate);
                settings->endGroup();
                settings->endGroup();
            }
            sessionsList << entry;
        }
        delete settings;
    }
    // Increase to get the number the current session would get if it was a
    // revision file. (1 greater than the maximum revision number present.)
    currentRevisionNumber++;
    // Prepend current session to list.
    QSettings *settings = new QSettings(
                QDir(path).absoluteFilePath(currentSession + fileExtension),
                QSettings::IniFormat);
    QStringList groups = settings->childGroups();
    if ( groups.contains("MSession")  )
    {
        QString entry = "Rev. " + QString::number(currentRevisionNumber);
        settings->beginGroup("MSession");
        groups = settings->childGroups();
        if ( groups.contains("SessionDetails")  )
        {
            settings->beginGroup("SessionDetails");

            entry += ": " + settings->value("dateTime", "").toDateTime()
                    .toString(Qt::ISODate);
            settings->endGroup();
            settings->endGroup();
        }
        sessionsList.prepend(entry);
        delete settings;
    }

    // Sort list from greatest revision number to smallest.
    std::sort(sessionsList.begin(), sessionsList.end(), [](QString a, QString b)
    {
        return a.split(" ").at(1).split(":").first().toInt()
                >= b.split(" ").at(1).split(":").first().toInt();
    });

    MSystemManagerAndControl::getInstance()->getMainWindow()
            ->onCurrentSessionHistoryChanged(&sessionsList, currentSession);
}


void MSessionManagerDialog::changeAutoSaveInterval(int autoSaveInterval)
{
    // Use tool tip to show time interval splitted up to [h min sec].
    double autoSaveIntervalTime = double(autoSaveInterval);
    QVector2D time;
    time.setX(floor(autoSaveIntervalTime / 3600.));
    autoSaveIntervalTime -= time.x() * 3600.;
    time.setY(floor(autoSaveIntervalTime / 60.));
    autoSaveIntervalTime -= time.y() * 60.;
    ui->autoSaveSpinBox->setToolTip(
                QString("[%1h %2min %3sec]")
                .arg(time.x()).arg(time.y()).arg(autoSaveIntervalTime));
    QToolTip::showText(ui->autoSaveSpinBox->mapToGlobal(QPoint(0, 0)),
                       ui->autoSaveSpinBox->toolTip());

    // Adapt timer to new interval.
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    sysMC->getMainWindow()->updateSessionTimerInterval(autoSaveInterval);
}


void MSessionManagerDialog::onAutoSaveToggeled(bool checked)
{
    if (checked && currentSession == "")
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("No session specified to save automatically to."
                       "\nPlease create a new session before activating auto"
                       " save."
                       "\n(Auto save is deactivated now.)");
        msgBox.exec();
        ui->autoSaveCheckBox->setChecked(false);
    }
}


void MSessionManagerDialog::deleteSession()
{
    // Only delete session if user selected one.
    if (ui->sessionsListView->selectionModel()->selectedIndexes().size() == 0)
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("Please select session to delete.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }

    QString sessionName = ui->sessionsListView->currentIndex().data().toString();

    int result =
            QMessageBox::question(
                this, "Delete session '" + sessionName + "'",
                "Are you sure you want to delete '" + sessionName
                + "'?\n(Deletion cannot be undone!)", QMessageBox::Yes,
                QMessageBox::No);
    // Only execute deletion if user confirms.
    if (result == QMessageBox::Yes)
    {
        // Ask for another confirmation if user wants to delete current session.
        if (sessionName == currentSession)
        {
            result = QMessageBox::question(
                        this, "Delete session '" + sessionName + "'",
                        "'" + sessionName
                        + "' is the current session.\n"
                          "Do you really want to delete it?",
                        QMessageBox::Yes, QMessageBox::No);
        }

        if (result == QMessageBox::Yes)
        {
            QFile file(QDir(path).absoluteFilePath(sessionName + fileExtension));
            file.remove();

            // Delete revision files.
            QStringList revertSessionFileList;
            getCurrentSessionFileHistoryFileNameList(revertSessionFileList,
                                                     sessionName);

            foreach (QString sessionRevision, revertSessionFileList)
            {
                QFile file(QDir(path).absoluteFilePath(sessionRevision));
                file.remove();
            }

            // User deleted current session thus currentSession needs to be
            // cleared.
            if (sessionName == currentSession)
            {
                setSessionToCurrent("");
            }
        }
    }
}


bool MSessionManagerDialog::saveSessionOnAppExit()
{
    // If no auto save on exit is active, quit Met.3D without asking.
    if (!ui->saveOnAppCheckBox->isChecked())
    {
        return true;
    }

    int result = QMessageBox::question(
                this, "Exiting Met.3D",
                "Do you want to save the session before exiting the"
                " application?",
                QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);

    if (result == QMessageBox::No)
    {
        return true;
    }
    else if (result != QMessageBox::Yes)
    {
        return false;
    }

    QString path = this->path;
    QString sessionName = currentSession;

    // Don't save session if no session given to save to.
    if (currentSession == "")
    {
        QMessageBox::warning(
                    this, "Exiting Met.3D",
                    "No file name given to save session to."
                    "\nPlease select a name for the session file.");

        bool ok = true;
        while (sessionName == "" || !ok)
        {
            QString fileName = QFileDialog::getSaveFileName(
                        MSystemManagerAndControl::getInstance()->getMainWindow(),
                        "Save session", path, "*" + fileExtension);
            if (fileName == "")
            {
                return false;
            }
            path = QFileInfo(fileName).canonicalPath();
            sessionName = QFileInfo(fileName).fileName();
            if (sessionName.endsWith(fileExtension))
            {
                sessionName.chop(fileExtension.size());
                // The file dialog checked if the file already exists, so no
                // need to ask the user twice.
                continue;
            }
            // The file dialog didn't check whether the file already exists
            // thus we need to do it manually.

            fileName = QDir(path).absoluteFilePath(sessionName + fileExtension);
            // Overwrite if the file exists.
            if (QFile::exists(fileName))
            {
                int result = QMessageBox::question(
                            this, "Saving session",
                            "'" + fileName
                            + "' already exists."
                              "Do you want to replace it?",
                            QMessageBox::Yes, QMessageBox::No,
                            QMessageBox::Cancel);
                if (result == QMessageBox::Yes)
                {
                    break;
                }
                else if (result == QMessageBox::No)
                {
                    ok = false;
                    continue;
                }
                else
                {
                    return false;
                }
            }
        }
    }
    // Don't save session if Met.3D has no write access to the directory.
    while (!QFileInfo(path).isWritable())
    {
        QMessageBox::warning(
                    this, "Unable to save session.",
                    "No write access to directory.\n"
                    "Please select a different directory.");
        path = QFileDialog::getExistingDirectory(
                    this, tr("Select directory to store sessions"), path,
                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        // Return if no directory has been chosen.
        if (path == "")
        {
            return false;
        }
    }

    this->currentSession = sessionName;
    this->path = path;
    saveSession();
    return true;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MSessionManagerDialog::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event);

    // Without the implementation of this method, pressing the enter key while
    // changing the auto save interval spin box would also trigger a button
    // press event of the button pressed lastly.
}


/******************************************************************************
***                            PRIVATE METHODS                              ***
*******************************************************************************/

void MSessionManagerDialog::updateSessionLabel()
{
    ui->currentSessionLabel->setText("[" + currentSession + "]");
    ui->currentSessionLabel->setToolTip(currentSession);
    QString labelText = ui->currentSessionLabel->text();
    int textWidth = ui->currentSessionLabel->fontMetrics().width(labelText);
    if (textWidth > ui->currentSessionLabel->width())
    {
        int dotsWidth =
                ui->currentSessionLabel->fontMetrics().width("...");
        while (textWidth + dotsWidth > ui->currentSessionLabel->width())
        {
            labelText.chop(1);
            textWidth =
                    ui->currentSessionLabel->fontMetrics().width(labelText);
        }
        ui->currentSessionLabel->setText(labelText + "...");
    }
}


void MSessionManagerDialog::setSessionToCurrent(QString session)
{
    currentSession = session;
    if (currentSession != "")
    {
        ui->reloadButton->setEnabled(true);
        ui->saveButton->setEnabled(true);
    }
    updateSessionLabel();
    sessionItemDelegate->setCurrentSessionName(currentSession);

    ui->sessionsListView->viewport()->update();

    MSystemManagerAndControl::getInstance()->getMainWindow()->onSessionSwitch(
                currentSession);

    fillCurrentSessionHistoryList();
}


void MSessionManagerDialog::resetSession()
{
    LOG4CPLUS_DEBUG(mlog, "Resetting session...");

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    QSettings *settings = new QSettings();

    // Reset session to default values by loading empty settings object.

    // General.
    // ========
    sysMC->setHandleSize(.5);

    // Sync controls.
    // ==============
    QStringList syncNames = sysMC->getSyncControlIdentifiers();
    // Remove "None" from list of sync control ids.
    syncNames.removeFirst();
    foreach (QString syncName, syncNames)
    {
        MSyncControl *syncControl = sysMC->getSyncControl(syncName);
        syncControl->loadConfiguration(settings);
    }

    // Remove Actors.
    // ==============
    // List actor names which are not part of the session.
    foreach (MActor *actor, glRM->getActors())
    {
        // Skip actors not deletable by ths user. (e.g. Labels Actor)
        if (!actor->getActorIsUserDeletable())
        {
            continue;
        }
        foreach (MSceneControl *scene, actor->getScenes())
        {
            if (glRM->getScenes().contains(scene))
            {
                scene->removeActorByName(actor->getName());
            }
        }
        actor->clearScenes();
        // Delete actor.
        glRM->deleteActor(actor);
    }

    // Scene views.
    // ============
    QList<MSceneViewGLWidget*> sceneViews = sysMC->getRegisteredViews();
    QList<MSceneControl *> sceneList = glRM->getScenes();
    int numSceneViews = sceneViews.size();
    int numScenes = sceneList.size();
    for (int i = 0; i < numSceneViews; i++)
    {
        sceneViews[i]->loadConfiguration(settings);
        QString sceneName = sceneList[min(i, numScenes - 1)]->getName();
        sceneViews[i]->setScene(glRM->getScene(sceneName));
    }

    // Window Layout.
    // ==============
    sysMC->getMainWindow()->loadConfiguration(settings);

    delete settings;
}


void MSessionManagerDialog::saveSessionToFile(QString sessionName, bool autoSave)
{
    QString filename = QDir(path).absoluteFilePath(sessionName + fileExtension);

    if (autoSave)
    {
        LOG4CPLUS_DEBUG(mlog,
                        "Auto-saving session [auto-save interval "
                        << ui->autoSaveSpinBox->value() <<
                        " sec] to " << filename.toStdString());
    }
    else
    {
        LOG4CPLUS_DEBUG(mlog, "Saving session to " << filename.toStdString());
    }

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);
    // Overwrite if the file exists.
    if (QFile::exists(filename))
    {
        updateSessionFileHistory(filename);
        QFile::remove(filename);
    }
    else
    {
        currentRevisionNumber = 0;
    }

    // File Format.
    // ==========================================
    settings->beginGroup("FileFormat");
    // Save version id of Met.3D.
    settings->setValue("met3dVersion", met3dVersionString);
    settings->endGroup();
    // ==========================================

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();

    // Session.
    // ==========================================
    settings->beginGroup("MSession");

    // Session Details.
    // ==========================================
    settings->beginGroup("SessionDetails");
    settings->setValue("name", currentSession);
    settings->setValue("dateTime", QDateTime::currentDateTime());
    settings->endGroup();
    // ==========================================


    // General.
    // ==========================================
    settings->beginGroup("AllSceneViews");
    settings->setValue("handleSize", sysMC->getHandleSize());
    settings->endGroup();
    // ==========================================

    QList<MSceneViewGLWidget*> sceneViews = sysMC->getRegisteredViews();

    // Sync controls.
    // ==========================================
    QStringList syncControls = sysMC->getSyncControlIdentifiers();
    // Remove the "None" synchronisation control.
    syncControls.removeFirst();
    settings->beginGroup("MSyncControls");
    settings->setValue("syncControls", syncControls);
    foreach (QString syncControl, syncControls)
    {
        settings->beginGroup(QString("MSyncControl_" + syncControl));
        sysMC->getSyncControl(syncControl)->saveConfiguration(settings);
        settings->endGroup();
    }
    settings->endGroup();
    // ==========================================

    // Waypoints.
    // ==========================================
    QStringList waypointsModelIDList = sysMC->getWaypointsModelsIdentifiers();
    waypointsModelIDList.removeAll("None");
    settings->beginGroup("WaypointsModels");
    settings->beginWriteArray("model");
    int waypointModelCount = 0;
    foreach (QString waypointsModelID, waypointsModelIDList)
    {
        settings->setArrayIndex(waypointModelCount);
        settings->setValue("name", waypointsModelID);
        sysMC->getWaypointsModel(waypointsModelID)->saveToSettings(settings);
        waypointModelCount++;
    }
    settings->endArray();
    settings->endGroup();
    // ==========================================

    // Bounding Boxes.
    // ==========================================
    sysMC->getBoundingBoxDock()->saveConfiguration(settings);
    // ==========================================

    // Actors.
    // ==========================================
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    settings->beginGroup("MActors");
    QVector<MActor*> actors = glRM->getActors();
    QStringList sceneNamesList;
    settings->setValue("numActors", actors.size());
    for (int i = 0; i < actors.size(); i++)
    {
        settings->beginGroup(QString("MActor_%1").arg(i));
        settings->setValue("actorName", actors.at(i)->getName());
        settings->setValue("actorType", actors.at(i)->getActorType());
        actors[i]->saveActorConfiguration(settings);
        sceneNamesList.clear();
        foreach (MSceneControl *scene, actors[i]->getScenes())
        {
            sceneNamesList << scene->getName();
        }
        settings->setValue("scenes", sceneNamesList);
        settings->endGroup();
    }
    settings->endGroup();
    // ==========================================

    // Scenes.
    // ==========================================
    QStringList sceneNames;
    QList<QStringList> renderQueues;
    QStringList renderQueue;
    sceneNames.clear();
    renderQueues.clear();
    foreach (MSceneControl *scene, glRM->getScenes())
    {
        renderQueue.clear();
        foreach (MActor *actor, scene->getRenderQueue())
        {
            renderQueue << actor->getName();
        }
        sceneNames << scene->getName();
        renderQueues.append(renderQueue);
    }

    settings->beginGroup("MScenes");
    settings->beginWriteArray("Scene");
    for (int i = 0; i < sceneNames.size(); i++)
    {
        settings->setArrayIndex(i);
        settings->setValue("name", sceneNames[i]);
        settings->setValue("renderQueue", renderQueues[i]);
    }
    settings->endArray();
    settings->endGroup();
    // ==========================================

    // Scene views.
    // ==========================================
    settings->beginGroup("MSceneViews");
    settings->setValue("numSceneViews", sceneViews.size());
    for (int i = 0; i < sceneViews.size(); i++)
    {
        settings->beginGroup(QString("MSceneView_%1").arg(i));
        sceneViews[i]->saveConfiguration(settings);
        settings->setValue("scene", sceneViews[i]->getScene()->getName());
        settings->endGroup();
    }
    settings->endGroup();
    // ==========================================

    // Window Layout.
    // ==========================================
    sysMC->getMainWindow()->saveConfiguration(settings);
    // ==========================================

    settings->endGroup(); // end session group
    // ==========================================

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... session has been saved.");
    LOG4CPLUS_DEBUG(mlog, "Created session revision number "
                    << currentRevisionNumber);
}


void MSessionManagerDialog::loadSessionFromFile(QString sessionName,
                                                bool appendFileExtension)
{
    blockGUIElements();
    QString filename = "";
    if (appendFileExtension)
    {
        filename = QDir(path).absoluteFilePath(sessionName + fileExtension);
    }
    else
    {
        filename = QDir(path).absoluteFilePath(sessionName);
    }

    // File has been removed. Display warning and refuse to load session.
    // Special case: Hidden files are marked as not existing so test for
    // hidden flag as well.
    if (!QFileInfo(filename).isHidden() && !QFileInfo(filename).exists())
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("Session '" + sessionName + "' does not exist.\n"
                                                "Unable to load session.\n");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        ui->sessionsListView->viewport()->update();
        unblockGUIElements();
        return;
    }

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);

    QStringList groups = settings->childGroups();
    if ( !groups.contains("MSession") )
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("File does not contain session data...\n"
                    "Failed to load session.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        delete settings;
        unblockGUIElements();
        return;
    }

    // Indicator showing if something went wrong during session loading.
    bool corruptFile = false;

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();

    LOG4CPLUS_DEBUG(mlog, "Loading session from " << filename.toStdString());

    // Session.
    // ==========================================
    settings->beginGroup("MSession");

    // Create and initialise progress bar.
    QProgressDialog *progressDialog = new QProgressDialog(
                "Loading session...", "",
                0, settings->childGroups().size() * 2);
    // Hide cancel button.
    progressDialog->setCancelButton(0);
    // Hide close, resize and minimize buttons.
    progressDialog->setWindowFlags(
                Qt::Dialog | Qt::CustomizeWindowHint);
    progressDialog->setMinimumDuration(0);
    // Needs to be called when used from a different thread.
    //progressDialog->setAttribute(Qt::WA_DeleteOnClose, true);

    // Block access to active widget while progress dialog is active.
    // NOTE: This only works if the main window has already been shown; hence
    // only after the application has been fully initialized.
    if (sysMC->applicationIsInitialized())
    {
        progressDialog->setWindowModality(Qt::ApplicationModal);
    }

    // Always show progress dialog right away.
    progressDialog->show();
    progressDialog->update();
    progressDialog->repaint();

    int loadingProgress = 0;
    progressDialog->setValue(0);
    progressDialog->update();
    qApp->processEvents();

    // General.
    // ==========================================
    settings->beginGroup("AllSceneViews");
    sysMC->setHandleSize(settings->value("handleSize", .5).toDouble());
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    settings->endGroup();
    // ==========================================

    // Sync controls.
    // ==========================================
    LOG4CPLUS_DEBUG(mlog, "Session load: Initializing synchronization controls.");
    settings->beginGroup("MSyncControls");
    QStringList syncControlsToDelete = sysMC->getSyncControlIdentifiers();
    // Remove the "None" synchronisation control from the list of sync controls
    // to be deleted.
    syncControlsToDelete.removeOne("None");
    QStringList syncNames = settings->value("syncControls",
                                            QStringList()).toStringList();
    // Get sync controls which are present at the moment but not part of the
    // session to load.
    syncControlsToDelete =
            syncControlsToDelete.toSet().subtract(syncNames.toSet()).toList();
    // Remove sync controls which are not part of the session to load.
    foreach (QString syncToDelete, syncControlsToDelete)
    {
        MSyncControl *syncControl = sysMC->getSyncControl(syncToDelete);
        sysMC->getMainWindow()->removeSyncControl(syncControl);
    }

    progressDialog->setValue(++loadingProgress);
    progressDialog->update();

    foreach (QString syncName, syncNames)
    {
        LOG4CPLUS_DEBUG(mlog, "Checking if sync control "
                        << syncName.toStdString() << " already exists..");

        // Do not create sync controls with invalid object names.
        if (!isValidObjectName(syncName))
        {
            LOG4CPLUS_WARN(mlog, "'" << syncName.toStdString()
                           << "' is an invalid sync control name; skipping.");
            continue;
        }

        MSyncControl *syncControl = sysMC->getSyncControl(syncName);
        // Create new sync control if none with this names exists.
        if (syncControl == nullptr)
        {
            LOG4CPLUS_DEBUG(mlog, ".. sync control "
                            << syncName.toStdString()
                            << " does not exist, creating new instance.");
            syncControl = new MSyncControl(syncName, sysMC->getMainWindow());
            sysMC->registerSyncControl(syncControl);
            sysMC->getMainWindow()->dockSyncControl(syncControl);
        }
        else
        {
            // Disconnect synchronized objects to avoid scene views turning
            // black if loading invoces synchronisation event.
            syncControl->disconnectSynchronizedObjects();
        }
        settings->beginGroup(QString("MSyncControl_" + syncName));

        syncControl->loadConfiguration(settings);

        settings->endGroup();
    }
    settings->endGroup();
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    // ==========================================

    // TODO (bt, 26Oct2017): Implement replacing way points model as soon as it
    // is possible to have more than one way point model in Met.3D.

    // Waypoints.
    // ==========================================
    settings->beginGroup("WaypointsModels");
    int numWaypointModels = settings->beginReadArray("model");
    for (int i = 0; i < numWaypointModels; ++i)
    {
        settings->setArrayIndex(i);
        QString waypointsModelID = settings->value("name", "").toString();
        if (waypointsModelID != "" && isValidObjectName(waypointsModelID))
        {
            sysMC->getWaypointsModel(waypointsModelID)->loadFromSettings(
                        settings);
        }
    }
    settings->endArray();
    settings->endGroup();
    // ==========================================

    // Bounding Boxes.
    // ==========================================
    sysMC->getBoundingBoxDock()->removeAllBoundingBoxes();
    sysMC->getBoundingBoxDock()->loadConfiguration(settings);
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    // ==========================================

    // Actors.
    // ==========================================
    LOG4CPLUS_DEBUG(mlog, "Session load: Initializing actors.");

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    settings->beginGroup("MActors");
    // List actor names which are not part of the session.
    QStringList actorsToDelete;
    QMap<MActor*, int> actorsToConfigure;
    actorsToDelete.clear();
    actorsToConfigure.clear();
    foreach (MActor *actor, glRM->getActors())
    {
        // Skip actors not deletable by ths user. (e.g. Labels Actor)
        if (!actor->getActorIsUserDeletable())
        {
            continue;
        }
        foreach (MSceneControl *scene, actor->getScenes())
        {
            if (glRM->getScenes().contains(scene))
            {
                scene->removeActorByName(actor->getName());
            }
        }
        actor->clearScenes();
        actorsToDelete << actor->getName();
    }

    progressDialog->setValue(++loadingProgress);
    progressDialog->update();

    QStringList factoryNames = glRM->getActorFactoryNames();
    int numActors = settings->value("numActors", 0).toInt();
    // Create actors.
    for (int i = 0; i < numActors; i++)
    {
        settings->beginGroup(QString("MActor_%1").arg(i));
        QString actorName = settings->value("actorName", "").toString();
        QString actorType = settings->value("actorType", "").toString();
        // Skip actor if it has no name, its type does not fit any present
        // actor type or its name is invalid.
        // NOTE: Don't check if the actor name already exists since actors
        // won't be deleted thereby when e.g. reloading a session actors don't
        // need to be deleted and recreated but only their configurtation needs
        // to be loaded.
        if (actorName == "" || !factoryNames.contains(actorType)
                || !isValidObjectName(actorName))
        {
            LOG4CPLUS_WARN(mlog, "'" << actorName.toStdString()
                           << "': encountered invalid actor name or type;"
                              " skipping.");
            settings->endGroup();
            continue;
        }
        MActor *actor = glRM->getActorByName(actorName);
        // Actor does not exist yet. Create it!
        if (actor == nullptr)
        {
            actor = createActor(actorType);
            // If no actor was created, then continue.
            if (!actor)
            {
                settings->endGroup();
                continue;
            }
        }
        // Actor with same name exists, but it is not the same type.
        if (actor->getActorType() != actorType)
        {
            // Delete actor.
            glRM->deleteActor(actorName);
            // Create new actor with the right type.
            actor = createActor(actorType);
            // If no actor was created, then continue.
            if (!actor)
            {
                settings->endGroup();
                continue;
            }
        }
        // Set name of actor so that during loading of configuration it is
        // possible to identify connected actors (e.g. transfer functions
        // connected to horizontal cross section actor).
        actor->setName(actorName);
        actorsToConfigure.insert(actor, i);
        actorsToDelete.removeOne(actorName);
        settings->endGroup();
    }
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    // Load configuration of actors. It is necessary to load the configurations
    // AFTER all actors of the session have been created because e.g. a transfer
    // function actor might be loaded after an trajectory actor it is connected
    // to and this would lead to a warning.
    foreach (MActor *actor, actorsToConfigure.keys())
    {
        settings->beginGroup(
                    QString("MActor_%1").arg(actorsToConfigure.value(actor)));
        actor->loadActorConfiguration(settings);
        settings->endGroup();
    }
    settings->endGroup();
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    // Delete actors which are not saved in this session.
    foreach (QString actorName, actorsToDelete)
    {
        // Delete actor.
        glRM->deleteActor(actorName);
    }

    if (sysMC->applicationIsInitialized())
    {
        // Initialize all shaders and graphical resources of each registered
        // actor.
        foreach(MActor *actor, glRM->getActors())
        {
            actor->initialize();
        }
    }
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    // ==========================================

    // Scenes.
    // ==========================================
    LOG4CPLUS_DEBUG(mlog, "Session load: Initializing scenes.");

    // Remove scene view - scene connection.
    foreach (MSceneViewGLWidget *sceneView,  sysMC->getRegisteredViews())
    {
        sceneView->removeCurrentScene();
    }

    // Unregister scene views.
    foreach (MSceneControl *scene, glRM->getScenes())
    {
        foreach (MSceneViewGLWidget *sceneView, scene->getRegisteredSceneViews())
        {
            scene->unregisterSceneView(sceneView);
        }
    }
    QList<MSceneControl*> scenesToDelete = glRM->getScenes();
    settings->beginGroup("MScenes");
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();

    int size = settings->beginReadArray("Scene");
    for (int i = 0; i < size; i++)
    {
        settings->setArrayIndex(i);
        QString name = settings->value("name", "").toString();
        // Check parameter validity.
        if ( name.isEmpty() )
        {
            LOG4CPLUS_WARN(mlog, "invalid parameters encountered; skipping.");
            continue;
        }

        MSceneControl *scene = glRM->getScene(name);
        // Create new scene.
        if (scene == nullptr)
        {
            scene = new MSceneControl(name, sysMC->getMainWindow());
            glRM->registerScene(scene);
            sysMC->getMainWindow()->dockSceneControl(scene);
        }
        else
        {
            scenesToDelete.removeOne(scene);
        }
        // Add actors.
        if (scene != nullptr)
        {
            QStringList renderQueue =
                    settings->value("renderQueue", QStringList()).toStringList();
            foreach (QString actorName, renderQueue)
            {
                MActor *actor = glRM->getActorByName(actorName);
                if (actor != nullptr)
                {
                    scene->addActor(actor);
                }
            }
        }
    }
    settings->endArray();
    settings->endGroup();
    // Delete scenes present at the moment but not specified by the session
    // configuration.
    foreach (MSceneControl *scene, scenesToDelete)
    {
        sysMC->getMainWindow()->removeSceneControl(scene);
    }

    // Create default scene if no scene could be loaded from session file.
    if (glRM->getScenes().empty())
    {
        MSceneControl *scene = new MSceneControl("Scene 1",
                                                 sysMC->getMainWindow());
        glRM->registerScene(scene);
        sysMC->getMainWindow()->dockSceneControl(scene);
        corruptFile = true;
    }
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    // ==========================================

    // Scene views.
    // ==========================================
    LOG4CPLUS_DEBUG(mlog, "Session load: Initializing scene views.");
    QList<MSceneViewGLWidget*> sceneViews = sysMC->getRegisteredViews();
    int numSceneViews = 0;
    settings->beginGroup("MSceneViews");
    // Only load values for the number of scene views saved, but also don't
    // exceed the number of registered scene views.
    numSceneViews = min(settings->value("numSceneViews", 0).toInt(),
                        sceneViews.size());

    QList<MSceneControl *> sceneList = glRM->getScenes();
    int numScenes = glRM->getScenes().size();
    for (int i = 0; i < numSceneViews; i++)
    {
        settings->beginGroup(QString("MSceneView_%1").arg(i));
        sceneViews[i]->loadConfiguration(settings);
        QString defaultName = sceneList[min(i, numScenes - 1)]->getName();
        QString sceneName = settings->value("scene", defaultName).toString();
        // Set scene name to default if no scene exists called sceneName.
        if (glRM->getScene(sceneName) == nullptr)
        {
            sceneName = defaultName;
            corruptFile = true;
        }
        sceneViews[i]->setScene(glRM->getScene(sceneName));
        settings->endGroup();
    }
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    settings->endGroup();
    // If settings don't exist for all scene views present, assign default scene
    // to remaining scene views to prevent program crash.
    for (int i = numSceneViews; i < sceneViews.size(); i++)
    {
        QString defaultName = sceneList[min(i, numScenes - 1)]->getName();
        sceneViews[i]->setScene(glRM->getScene(defaultName));
        corruptFile = true;
    }
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    // ==========================================

    // Window Layout.
    // ==========================================
    sysMC->getMainWindow()->loadConfiguration(settings);
    progressDialog->setValue(++loadingProgress);
    progressDialog->update();
    // ==========================================

    settings->endGroup(); // end session group
    // ==========================================

    if (corruptFile)
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setIcon(QMessageBox::Warning);
        msg.setText("File seems to be corrupt.\n"
                    "One or more crucial settings were not saved correctly and"
                    " needed to be replaced by default values.");
        msg.exec();
    }

    delete settings;
    progressDialog->close();
    delete progressDialog;

    LOG4CPLUS_DEBUG(mlog, "... session has been loaded.");

    unblockGUIElements();
    return;
}


MActor* MSessionManagerDialog::createActor(QString actorType)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    MAbstractActorFactory *factory = glRM->getActorFactory(actorType);
    MActor *actor = factory->create();
    actor->setEnabled(true);

    if (!actor)
    {
        return nullptr;
    }

    // Register actor in resource manager.
    glRM->registerActor(actor);

    return actor;
}


void MSessionManagerDialog::blockGUIElements()
{
    // Don't block GUI Elements if the GUI is invisible since processEvents()
    // when unblocking the GUI-Element might cause the application to crash when
    // session is loaded on start.
    if (this->isVisible())
    {
        ui->changeFolderButton->setEnabled(false);
        ui->newButton->setEnabled(false);
        ui->cloneButton->setEnabled(false);
        ui->switchToButton->setEnabled(false);
        ui->deleteButton->setEnabled(false);
        ui->reloadButton->setEnabled(false);
        ui->saveButton->setEnabled(false);
        ui->sessionsListView->setEnabled(false);
        ui->autoSaveCheckBox->setEnabled(false);
        ui->buttonBox->setEnabled(false);
    }
}


void MSessionManagerDialog::unblockGUIElements()
{
    // Don't block GUI Elements if the GUI is invisible since processEvents()
    // might cause the application to crash when session is loaded on start.
    if (this->isVisible())
    {
        // Get rid of waiting events before reseting the attribute since
        // otherwise the button click events will be handled after loading has
        // finished. (Waiting time is mandatory since otherwise the events will
        // be processed to the buttons nevertheless.)
        qApp->processEvents(QEventLoop::AllEvents, 1000);

        ui->changeFolderButton->setEnabled((true));
        ui->newButton->setEnabled((true));
        ui->cloneButton->setEnabled((true));
        ui->switchToButton->setEnabled((true));
        ui->deleteButton->setEnabled(true);
        ui->reloadButton->setEnabled(true);
        ui->saveButton->setEnabled((true));
        ui->sessionsListView->setEnabled((true));
        ui->autoSaveCheckBox->setEnabled((true));
        ui->buttonBox->setEnabled((true));
    }
}


bool MSessionManagerDialog::isValidSessionName(QString sessionName)
{
    if (sessionName == "")
    {
        // The user entered an empty string as name. Display a warning
        // and ask the user to enter another name.
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("Please enter a name.");
        msgBox.exec();

        return false;
    }

    // Reject name if it already exists.
    if (QFile::exists(QDir(path).absoluteFilePath(sessionName
                                                  + fileExtension)))
    {
        // The user entered a name that already exists. Display a warning
        // and ask the user to enter another name.
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("'" + sessionName + "' already exists.\nPlease"
                                           " enter a different name.");
        msgBox.exec();

        return false;
    }

    return true;
}


QString MSessionManagerDialog::getAppendixWithSmallestIndexForUniqueName(
        QString sessionName)
{
    QDir directory(path);
    // Get all files with the right file extension.
    QStringList sessions =
            directory.entryList(QStringList("*" + fileExtension),
                                QDir::Files);

    if (!sessions.contains(sessionName + fileExtension))
    {
        return QString();
    }

    // Get regular expression which matches the file extension by escaping
    // special regular expression characters.
    QString regExpFileExt = QRegExp::escape(fileExtension);
    // Get all sessions with a name matching the form "sessionName (x)" with
    // x number of length greater or equal 1 without leading zeros.
    sessions = sessions.filter(QRegExp(QRegExp::escape(sessionName)
                                       + "\\s\\((\\d|([1-9]\\d+))\\)"
                                       + regExpFileExt));

    QList<int> numbers;
    numbers.clear();
    QRegExp regExpNumbers("\\d+");
    QRegExp regExpBracket("\\(");
    // Extract the numbers of the session names.
    foreach (QString session, sessions)
    {
        // Get position of bracket introducing the number of the cloned session.
        int offset = regExpBracket.lastIndexIn(session);
        // Use index of the bracket as an offset to get starting position from
        // which on to search for the number of the cloned session.
        regExpNumbers.indexIn(session, offset);
        numbers.append(regExpNumbers.capturedTexts()[0].toInt());
    }

    int index = 0;
    // If we found numbers, search for the smallest index not assigned yet.
    if (!numbers.empty())
    {
        // Sort list of numbers.
        qSort(numbers);
        // If the first number is not zero, zero is the smallest index available.
        if (numbers[0] == 0)
        {
            // Loop over all numbers and search for the first gap >= 2.
            // Leave out the last one since it has no following number to
            // compare with (segmentation fault).
            for (int i = 0; i < numbers.size() - 1; i++)
            {
                if ((numbers[i + 1] - numbers[i]) >= 2)
                {
                    index = numbers[i] + 1;
                    break;
                }
            }
            // Since no gap was found, we need to increment the last number.
            if (index == 0)
            {
                index = numbers.last() + 1;
            }
        }
    }

    return QString(" (%1)").arg(index);
}


void MSessionManagerDialog::updateSessionFileHistory(QString filename)
{
    QDir directory(path);
    QStringList fileList = directory.entryList(QDir::Files | QDir::Hidden);
    QString regExpString =
            QRegExp::escape("." + currentSession + fileExtension + ".")
            + "\\d+";
    fileList = fileList.filter(QRegExp(regExpString));

    int fileIndex = 0;
    if (!fileList.isEmpty())
    {
        // Since string sorting would lead to ".10" being sorted before ".2",
        // it is necessary to extract the index number from the filename and
        // use it for sorting.
        std::sort(fileList.begin(), fileList.end(),
                  [](QString x, QString y)
        { return x.split(".", QString::SkipEmptyParts).last().toInt()
                    < y.split(".", QString::SkipEmptyParts).last().toInt(); }
        );

        // Store at most maximumNumberOfSavedRevisions revision files for one
        // session and remove all others starting from the smallest index.
        while (!fileList.isEmpty()
               && fileList.size() >= maximumNumberOfSavedRevisions)
        {
            QFile(directory.absoluteFilePath(fileList.takeFirst())).remove();
        }

        fileIndex = fileList.last().split(
                    ".", QString::SkipEmptyParts).last().toInt() + 1;
    }

    // Only generate revision file if user wants to save revisions.
    if (maximumNumberOfSavedRevisions > 0)
    {
        QFile file(filename);
        file.copy(QDir(path).absoluteFilePath(
                      "." + currentSession + fileExtension
                      + "." + QString::number(fileIndex)));
    }

    // Revision Number of current session.
    currentRevisionNumber = fileIndex + 1;
}


void MSessionManagerDialog::getCurrentSessionFileHistoryFileNameList(
        QStringList &fileNameList, QString &sessionName)
{
    QDir directory(path);

    fileNameList = directory.entryList(QDir::Files | QDir::Hidden);

    if (!fileNameList.isEmpty())
    {
        QString regExpString =
                QRegExp::escape("." + sessionName + fileExtension + ".")
                + "\\d+";

        fileNameList = fileNameList.filter(QRegExp(regExpString));
    }
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                         MSessionFileSystemModel                         ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSessionFileSystemModel::MSessionFileSystemModel(QWidget *parent) :
    QFileSystemModel(parent)
{}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QVariant MSessionFileSystemModel::data(const QModelIndex & index, int role) const
{
    switch (role)
    {
    case Qt::DecorationRole:
    {
        return QVariant();
    }
    case Qt::DisplayRole:
    {
        QString entry = QFileSystemModel::data(index, role).toString();
        entry.chop(MSessionManagerDialog::fileExtension.size());
        return QVariant(entry);
    }
    default:
    {
        return QFileSystemModel::data(index, role);
    }
    }
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                          MSessionItemDelegate                           ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSessionItemDelegate::MSessionItemDelegate(QWidget *parent) :
    QStyledItemDelegate(parent)
{}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MSessionItemDelegate::paint(QPainter *painter,
                                 const QStyleOptionViewItem & option,
                                 const QModelIndex & index) const
{
    QStyleOptionViewItem newOption = QStyleOptionViewItem(option);
    initStyleOption(&newOption, index);
    if (currentSessionName != "")
    {
        if (index.data() == currentSessionName)
        {
            newOption.font.setBold(true);
            newOption.font.setItalic(true);
        }
    }
    QStyledItemDelegate::paint(painter, newOption, index);
}

} // namespace Met3D
