/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
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
#include "nwpmultivaractor.h"

// standard library imports
#include <iostream>
#include <limits>

// related third party imports
#include <QtCore>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/mscenecontrol.h"
#include "actors/nwpverticalsectionactor.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNWPMultiVarActor::MNWPMultiVarActor()
    : MActor(),
      analysisControl(nullptr)
{
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    // Property group for the variable properties.
    variablePropertiesGroup = addProperty(GROUP_PROPERTY, "variables",
                                          propertyGroup);

    addVariableProperty = addProperty(CLICK_PROPERTY, "add new variable",
                                      variablePropertiesGroup);

    endInitialiseQtProperties();
}


MNWPMultiVarActor::~MNWPMultiVarActor()
{
    // Delete all MNWPActorVariable instances.
    foreach (MNWPActorVariable* var, variables) delete var;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MNWPMultiVarActor::provideSynchronizationInfoToScene(
        MSceneControl *scene)
{
    foreach (MNWPActorVariable* var, variables)
    {
        if (var->getSynchronizationControl() != nullptr)
        {
            scene->variableSynchronizesWith(var->getSynchronizationControl());
            var->updateSyncPropertyColourHints(scene);
        }
    }
}


MNWPActorVariable *MNWPMultiVarActor::createActorVariable2(
        const QString &dataSourceID,
        const MVerticalLevelType levelType,
        const QString &variableName)
{
    MSelectableDataSource ds;
    ds.dataSourceID = dataSourceID;
    ds.levelType = levelType;
    ds.variableName = variableName;

    return createActorVariable(ds);
}


MNWPActorVariable* MNWPMultiVarActor::addActorVariable()
{
    // Ask the user for a data source to be connected to the new actor variable.
    MSelectDataSourceDialog dialog(this->supportedLevelTypes());
    if (dialog.exec() == QDialog::Rejected) return nullptr;

    QStringList sysControlIdentifiers = MSystemManagerAndControl::getInstance()
            ->getSyncControlIdentifiers();

    bool accepted = false;
    // ask user which sync control should be synchronized with variable
    QString syncName = QInputDialog::getItem(
                nullptr, "Choose Sync Control",
                "Please select a sync control to synchronize with: ",
                sysControlIdentifiers,
                min(1, sysControlIdentifiers.size() - 1), false,
                &accepted);
    // if user has aborted do not add any variable
    if (!accepted) return nullptr;

    MSelectableDataSource dataSource = dialog.getSelectedDataSource();

    if (dynamic_cast<MNWPVerticalSectionActor*>(this))
    {
        if (!variables.isEmpty())
        {
            QString dataSourceID = variables.at(0)->dataSourceID;
            if (dataSourceID != "" && dataSourceID != dataSource.dataSourceID)
            {
                QMessageBox::warning(
                            nullptr, getName(),
                            "Vertical cross-section actors cannot handle"
                            " multiple variables coming from different data"
                            " sources.\n"
                            "(No variable was added.)");
                return nullptr;
            }
        }
    }

    MNWPActorVariable* var = createActorVariable(dataSource);

    return addActorVariable(var, syncName);
}


MNWPActorVariable* MNWPMultiVarActor::addActorVariable(
        MNWPActorVariable *var, const QString &syncName)
{
    LOG4CPLUS_DEBUG(mlog, "Adding new variable '"
                    << var->variableName.toStdString()
                    << "' to actor '" << getName().toStdString() << "'");

    enableEmissionOfActorChangedSignal(false);

    // Add variable to list of variables.
    variables << var;

    // Add variable property group to this actor's properties.
    variablePropertiesGroup->addSubProperty(var->varPropertyGroup);

    // Initialize variable.
    if (isInitialized())
    {
        LOG4CPLUS_DEBUG(mlog, "Initializing variable:");
        var->initialize();
    }

    if (!syncName.isEmpty())
    {
        var->synchronizeWith(MSystemManagerAndControl::getInstance()
                             ->getSyncControl(syncName));
    }

    // Tell derived classes that this variable has been added.
    onAddActorVariable(var);

    // Collapse browser items of variable.
    foreach (MSceneControl *scene, scenes)
        scene->collapsePropertySubTree(var->varPropertyGroup);

    enableEmissionOfActorChangedSignal(true);

    LOG4CPLUS_DEBUG(mlog, "... variable has been added.");

    return var;
}


void MNWPMultiVarActor::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MNWPMultiVarActor::getSettingsID());
    settings->setValue("numVariables", variables.size());
    settings->endGroup();

    for (int vi = 0; vi < variables.size(); vi++)
    {
        settings->beginGroup(QString("Variable_%1").arg(vi));
        variables.at(vi)->saveConfiguration(settings);
        settings->endGroup();
    }
}


void MNWPMultiVarActor::loadConfiguration(QSettings *settings)
{
    // Delete current actor variables and their property groups.
    // =========================================================

    LOG4CPLUS_DEBUG(mlog, "Removing current actor variables:");

    foreach (MNWPActorVariable* var, variables)
    {
        // Tell derived classes that the variable will be deleted.
        enableEmissionOfActorChangedSignal(false);
        onDeleteActorVariable(var);
        enableEmissionOfActorChangedSignal(true);

        variablePropertiesGroup->removeSubProperty(var->varPropertyGroup);
        variables.removeOne(var);

        LOG4CPLUS_DEBUG(mlog, "Removed variable <"
                        << var->variableName.toStdString()
                        << "> from actor ''" << getName().toStdString()
                        << "''.");

        delete var;
    }

    // Read MNWPMultiVarActor specific properties.
    // ===========================================

    settings->beginGroup(MNWPMultiVarActor::getSettingsID());

    int numVariables = settings->value("numVariables").toInt();

    settings->endGroup();

    // Create new actor variables from file info.
    // ==========================================

    LOG4CPLUS_DEBUG(mlog, "Creating new actor variables:");

    // Index of variable with respect to variable list not containing variables
    // which were not loaded.
    int varIndex = 0;

    for (int vi = 0; vi < numVariables; vi++)
    {
        settings->beginGroup(QString("Variable_%1").arg(vi));

        // Forecast variable name and data source.
        const QString dataSourceID = settings->value("dataLoaderID").toString();
        const MVerticalLevelType levelType = MVerticalLevelType(
                    settings->value("levelType").toInt());
        const QString variableName = settings->value("variableName").toString();

        LOG4CPLUS_DEBUG(mlog, "  > Variable " << vi << ": data source = "
                        << dataSourceID.toStdString() << ", level type = "
                        << MStructuredGrid::verticalLevelTypeToString(levelType).toStdString()
                        << ", variable = " << variableName.toStdString());

        // Check if data source and variable are available in the current
        // configuration.

        MWeatherPredictionDataSource *source =
                dynamic_cast<MWeatherPredictionDataSource*>(
                    MSystemManagerAndControl::getInstance()
                    ->getDataSource(dataSourceID));

        bool dataSourceIsAvailable = (
                    (source != nullptr)
                    && source->availableLevelTypes().contains(levelType)
                    && source->availableVariables(levelType).contains(variableName)
                    );

        if (dataSourceIsAvailable)
        {
            // Yes, data source and variable are available.

            // Create new actor variable.
            MNWPActorVariable* var = createActorVariable2(
                        dataSourceID, levelType, variableName);

            // Add the variable to the actor.
            addActorVariable(var, QString());

            // Load the variable configuration.
            var->loadConfiguration(settings);

//TODO (mr, Feb2015) -- The call to var->initialize() in addActorVariable()
//                      already triggers a data request, but BEFORE the sync
//                      time/member is set. Hence trigger another request
//                      here -- this should be fixed (only trigger here!).
//                      Also see below.
            var->triggerAsynchronousDataRequest(false);
            ++varIndex;
        }
        else
        {
            // No, either data source or variable is not available. Ask the
            // user if she/he wants to choose an alternative variable.

            QString msg = QString(
                        "Data source and/or variable\n%1 / %2 / %3\nis not "
                        "available. Would you like to choose an alternative source?")
                    .arg(dataSourceID)
                    .arg(MStructuredGrid::verticalLevelTypeToString(levelType))
                    .arg(variableName);

            QMessageBox yesNoBox;
            yesNoBox.setWindowTitle("Load actor variable");
            yesNoBox.setText(msg);
            yesNoBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            yesNoBox.setDefaultButton(QMessageBox::Yes);

            if (yesNoBox.exec() == QMessageBox::Yes)
            {
                // Display data source dialog and create variable from
                // selected source.
                MNWPActorVariable* var = addActorVariable();

                // Load the variable configuration.
                if (var != nullptr)
                {
                    var->loadConfiguration(settings);
//TODO -- see above.
                    var->triggerAsynchronousDataRequest(false);
                    ++varIndex;
                }
                else
                {
                    onLoadActorVariableFailure(varIndex);
                }
            }
            else
            {
                onLoadActorVariableFailure(varIndex);
            }
        }

        settings->endGroup();
    }
}


void MNWPMultiVarActor::broadcastPropertyChangedEvent(
        MPropertyType::ChangeNotification ptype, void *value)
{
    foreach (MNWPActorVariable* var, variables)
        var->actorPropertyChangeEvent(ptype, value);
}


bool MNWPMultiVarActor::isConnectedTo(MActor *actor)
{
    if (MActor::isConnectedTo(actor))
    {
        return true;
    }

    // This actor is connected to the argument actor if the actor is the
    // transfer function of any variable.
    foreach (MNWPActorVariable* var, variables)
    {
        if (var->transferFunction == actor)
        {
            return true;
        }
    }

    return false;
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MNWPMultiVarActor::onQtPropertyChanged(QtProperty *property)
{
    if (property == addVariableProperty)
    {
        addActorVariable();
        emitActorChangedSignal();
    }

    // Variable specific properties.
    foreach (MNWPActorVariable* var, variables)
    {
        if (property == var->changeVariableProperty)
        {
            if ( var->onQtPropertyChanged(property) )
            {
                onChangeActorVariable(var);
                emitActorChangedSignal();
            }
            break;
        }
        else if ( var->onQtPropertyChanged(property) )
        {
            emitActorChangedSignal();
            break;
        }
        else if (property == var->removeVariableProperty)
        {
            // Ask the user if the variable should really be deleted.
            QMessageBox yesNoBox;
            yesNoBox.setWindowTitle("Delete actor variable");
            yesNoBox.setText("Do you really want to delete actor variable ''"
                             + var->variableName + "''?");
            yesNoBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            yesNoBox.setDefaultButton(QMessageBox::No);
            if (yesNoBox.exec() != QMessageBox::Yes) break;

            // Tell derived classes that the variable will be deleted.
            enableEmissionOfActorChangedSignal(false);
            onDeleteActorVariable(var);
            enableEmissionOfActorChangedSignal(true);

            variablePropertiesGroup->removeSubProperty(var->varPropertyGroup);
            variables.removeOne(var);

            LOG4CPLUS_DEBUG(mlog, "Removed variable <"
                            << var->variableName.toStdString()
                            << "> from actor ''" << getName().toStdString()
                            << "''.");

            delete var;

            emitActorChangedSignal();
            break;
        }
    } // for variables
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MNWPMultiVarActor::initializeActorResources()
{
    foreach (MNWPActorVariable* var, variables) var->initialize();

    collapseActorPropertyTree();
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


} // namespace Met3D
