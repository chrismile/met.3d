/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
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
#include "nwpactorvariable.h"

// standard library imports
#include <iostream>
#include <limits>
#include <float.h>

// related third party imports
#include <QtCore>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/mscenecontrol.h"
#include "gxfw/selectdatasourcedialog.h"
#include "gxfw/nwpmultivaractor.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                            NWPActorVariable                             ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNWPActorVariable::MNWPActorVariable(MNWPMultiVarActor *actor)
    : dataSource(nullptr),
      grid(nullptr),
      textureDataField(nullptr),
      textureUnitDataField(-1),
      textureLonLatLevAxes(nullptr),
      textureUnitLonLatLevAxes(-1),
      textureSurfacePressure(nullptr),
      textureUnitSurfacePressure(-1),
      textureHybridCoefficients(nullptr),
      textureUnitHybridCoefficients(-1),
      textureDataFlags(nullptr),
      textureUnitDataFlags(-1),
      texturePressureTexCoordTable(nullptr),
      textureUnitPressureTexCoordTable(-1),
      textureDummy1D(nullptr),
      textureDummy2D(nullptr),
      textureDummy3D(nullptr),
      textureUnitUnusedTextures(-1),
      transferFunction(nullptr),
      textureUnitTransferFunction(-1),
      synchronizationControl(nullptr),
      actor(actor),
      ensembleFilterOperation(""),
      numEnsembleMembers(0),
      useFlagsIfAvailable(false),
      gridTopologyMayHaveChanged(true),
      requestPropertiesFactory(new MRequestPropertiesFactory(this)),
      suppressUpdate(false)
{
    MNWPMultiVarActor *a = actor;
    MQtProperties *properties = actor->getQtProperties();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    a->beginInitialiseQtProperties();

    varPropertyGroup = a->addProperty(GROUP_PROPERTY, variableName);

    datasourceNameProperty = a->addProperty(STRING_PROPERTY, "data source",
                                            varPropertyGroup);
    datasourceNameProperty->setEnabled(false);

    changeVariableProperty = a->addProperty(CLICK_PROPERTY, "change variable",
                                            varPropertyGroup);

    removeVariableProperty = a->addProperty(CLICK_PROPERTY, "remove",
                                            varPropertyGroup);

    // Property: Synchronize time and ensemble with an MSyncControl instance?
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    synchronizationProperty = a->addProperty(ENUM_PROPERTY, "synchronization",
                                             varPropertyGroup);
    properties->mEnum()->setEnumNames(synchronizationProperty,
                                      sysMC->getSyncControlIdentifiers());

    // Properties for init and valid time.
    initTimeProperty = a->addProperty(ENUM_PROPERTY, "initialisation",
                                      varPropertyGroup);

    validTimeProperty = a->addProperty(ENUM_PROPERTY, "valid",
                                       varPropertyGroup);

    QStringList ensembleModeNames;
    ensembleModeNames << "member" << "mean" << "standard deviation"
                      << "p(> threshold)" << "p(< threshold)"
                      << "min" << "max" << "max-min";
    ensembleModeProperty = a->addProperty(ENUM_PROPERTY, "ensemble mode",
                                          varPropertyGroup);
    properties->mEnum()->setEnumNames(ensembleModeProperty, ensembleModeNames);

    ensembleMemberProperty = a->addProperty(INT_PROPERTY, "ensemble member",
                                            varPropertyGroup);

    ensembleThresholdProperty = a->addProperty(DOUBLE_PROPERTY,
                                               "ensemble threshold",
                                               varPropertyGroup);
    properties->setDouble(ensembleThresholdProperty, 0., 6, 0.1);

    // Rendering properties.
    varRenderingPropertyGroup = getPropertyGroup("rendering");

    // Scan currently available actors for transfer functions. Add TFs to
    // the list displayed in the combo box of the transferFunctionProperty.
    QStringList availableTFs;
    availableTFs << "None";
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    foreach (MActor *ma, glRM->getActors())
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(ma))
        {
            availableTFs << tf->transferFunctionName();
        }
    }
    transferFunctionProperty = a->addProperty(ENUM_PROPERTY, "transfer function",
                                              varRenderingPropertyGroup);
    properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);

    // Observe the creation of the new actors -- if these are transfer
    // functions, add to the list displayed in the transfer function property.
    connect(glRM, SIGNAL(actorCreated(MActor*)), SLOT(onActorCreated(MActor*)));

    actor->endInitialiseQtProperties();
}


MNWPActorVariable::~MNWPActorVariable()
{
    // Release data fields.
    releaseDataItems();

    // Delete synchronization links (don't update the already deleted GUI
    // properties anymore...).
    synchronizeWith(nullptr, false);

    if (textureUnitDataField >=0)
        actor->releaseTextureUnit(textureUnitDataField);
    if (textureUnitLonLatLevAxes >=0)
        actor->releaseTextureUnit(textureUnitLonLatLevAxes);
    if (textureUnitTransferFunction >=0)
        actor->releaseTextureUnit(textureUnitTransferFunction);
    if (textureUnitSurfacePressure >=0)
        actor->releaseTextureUnit(textureUnitSurfacePressure);
    if (textureUnitHybridCoefficients >=0)
        actor->releaseTextureUnit(textureUnitHybridCoefficients);
    if (textureUnitDataFlags >=0)
        actor->releaseTextureUnit(textureUnitDataFlags);
    if (textureUnitPressureTexCoordTable >=0)
        actor->releaseTextureUnit(textureUnitPressureTexCoordTable);
    if (textureUnitUnusedTextures >=0)
        actor->releaseTextureUnit(textureUnitUnusedTextures);

    delete requestPropertiesFactory;
    if (textureDummy1D) delete textureDummy1D;
    if (textureDummy2D) delete textureDummy2D;
    if (textureDummy3D) delete textureDummy3D;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MNWPActorVariable::initialize()
{
    QString groupName = QString("%1 (%2)").arg(variableName).arg(
                MStructuredGrid::verticalLevelTypeToString(levelType));
    varPropertyGroup->setPropertyName(groupName);

    // Obtain new texture units.
    if (textureUnitDataField >=0)
        actor->releaseTextureUnit(textureUnitDataField);
    if (textureUnitLonLatLevAxes >=0)
        actor->releaseTextureUnit(textureUnitLonLatLevAxes);
    if (textureUnitTransferFunction >=0)
        actor->releaseTextureUnit(textureUnitTransferFunction);
    if (textureUnitSurfacePressure >=0)
        actor->releaseTextureUnit(textureUnitSurfacePressure);
    if (textureUnitHybridCoefficients >=0)
        actor->releaseTextureUnit(textureUnitHybridCoefficients);
    if (textureUnitDataFlags >=0)
        actor->releaseTextureUnit(textureUnitDataFlags);
    if (textureUnitPressureTexCoordTable >=0)
        actor->releaseTextureUnit(textureUnitPressureTexCoordTable);
    if (textureUnitUnusedTextures >=0)
        actor->releaseTextureUnit(textureUnitUnusedTextures);

    textureUnitDataField             = actor->assignTextureUnit();
    textureUnitLonLatLevAxes         = actor->assignTextureUnit();
    textureUnitTransferFunction      = actor->assignTextureUnit();
    textureUnitSurfacePressure       = actor->assignTextureUnit();
    textureUnitHybridCoefficients    = actor->assignTextureUnit();
    textureUnitDataFlags             = actor->assignTextureUnit();
    textureUnitPressureTexCoordTable = actor->assignTextureUnit();
    textureUnitUnusedTextures        = actor->assignTextureUnit();

    // This method is called on variable creation and when the datafield it
    // represents is changed (see changeVariables()). In the latter case,
    // the old data source needs to be disconnected.
    if (dataSource != nullptr)
    {
        disconnect(dataSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousDataAvailable(MDataRequest)));
    }

    // Get a pointer to the new source and connect to its request completed
    // signal.
    MAbstractDataSource *source =
            MSystemManagerAndControl::getInstance()->getDataSource(dataSourceID);
    dataSource = dynamic_cast<MWeatherPredictionDataSource*>(source);

    if (dataSource == nullptr)
    {
        LOG4CPLUS_ERROR(mlog, "no data source with ID "
                        << dataSourceID.toStdString() << " available");
    }
    else
    {
        connect(dataSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousDataAvailable(MDataRequest)));
    }

    textureDataField = textureHybridCoefficients =
            textureLonLatLevAxes = textureSurfacePressure =
            textureDataFlags = texturePressureTexCoordTable = nullptr;

    gridTopologyMayHaveChanged = true;

    actor->getQtProperties()->mString()->setValue(datasourceNameProperty,
                                                  dataSourceID);

    requestPropertiesFactory->updateProperties(&propertiesList,
                                               dataSource->requiredKeys());

    updateInitTimeProperty();
    updateValidTimeProperty();
    initEnsembleProperties();

    // Get values from sync control, if connected to one.
    if (synchronizationControl != nullptr)
    {
        setInitDateTime(synchronizationControl->initDateTime());
        updateValidTimeProperty();
        setValidDateTime(synchronizationControl->validDateTime());
        setEnsembleMember(synchronizationControl->ensembleMember());
    }

    setTransferFunctionFromProperty();

    if (textureDummy1D == nullptr)
        textureDummy1D = new GL::MTexture(GL_TEXTURE_1D, GL_ALPHA32F_ARB, 1);
    if (textureDummy2D == nullptr)
        textureDummy2D = new GL::MTexture(GL_TEXTURE_2D, GL_ALPHA32F_ARB, 1, 1);
    if (textureDummy3D == nullptr)
        textureDummy3D = new GL::MTexture(GL_TEXTURE_3D, GL_ALPHA32F_ARB, 1, 1, 1);

    // Load data field.
    asynchronousDataRequest();
}


void MNWPActorVariable::synchronizeWith(
        MSyncControl *sync, bool updateGUIProperty)

{
    if (synchronizationControl == sync) return;

    // If the variable is currently connected to a sync control, reset the
    // background colours of the valid and init time properties (they have
    // been set to red/green from this class to indicate time sync status,
    // see setValidDateTime()) and disconnect the signals.
    if (synchronizationControl != nullptr)
    {
        for (int i = 0; i < actor->getScenes().size(); i++)
        {
            actor->getScenes().at(i)->variableDeletesSynchronizationWith(
                        synchronizationControl);
        }

#ifdef DIRECT_SYNCHRONIZATION
        synchronizationControl->deregisterSynchronizedClass(this);
#else
        disconnect(synchronizationControl, SIGNAL(initDateTimeChanged(QDateTime)),
                   this, SLOT(setInitDateTime(QDateTime)));
        disconnect(synchronizationControl, SIGNAL(validDateTimeChanged(QDateTime)),
                   this, SLOT(setValidDateTime(QDateTime)));
        disconnect(synchronizationControl, SIGNAL(ensembleMemberChanged(int)),
                   this, SLOT(setEnsembleMember(int)));
#endif
    }

    synchronizationControl = sync;

    // Update GUI property.
    if (updateGUIProperty) // true by default
    {
        MQtProperties *properties = actor->getQtProperties();
        QString displayedSyncID = properties->getEnumItem(synchronizationProperty);
        QString newSyncID = (sync == nullptr) ? "None" : synchronizationControl->getID();
        if (displayedSyncID != newSyncID)
            properties->setEnumItem(synchronizationProperty, newSyncID);
    }

    // Connect to new sync control and try to switch to its current times.
    if (sync != nullptr)
    {
        // Tell the actor's scenes that this variable synchronized with this
        // sync control.
        for (int i = 0; i < actor->getScenes().size(); i++)
            actor->getScenes().at(i)->variableSynchronizesWith(sync);

#ifdef DIRECT_SYNCHRONIZATION
        synchronizationControl->registerSynchronizedClass(this);
#else
        connect(sync, SIGNAL(initDateTimeChanged(QDateTime)),
                this, SLOT(setInitDateTime(QDateTime)));
        connect(sync, SIGNAL(validDateTimeChanged(QDateTime)),
                this, SLOT(setValidDateTime(QDateTime)));
        connect(sync, SIGNAL(ensembleMemberChanged(int)),
                this, SLOT(setEnsembleMember(int)));
#endif
        setInitDateTime(sync->initDateTime());
        setValidDateTime(sync->validDateTime());
        setEnsembleMember(sync->ensembleMember());
    }
    else
    {
        for (int i = 0; i < actor->getScenes().size(); i++)
        {
            actor->getScenes().at(i)->resetPropertyColour(initTimeProperty);
            actor->getScenes().at(i)->resetPropertyColour(validTimeProperty);
        }
    }
}


bool MNWPActorVariable::synchronizationEvent(
        MSynchronizationType syncType, QVariant data)
{
    switch (syncType)
    {
    case SYNC_INIT_TIME:
    {
        actor->enableActorUpdates(false);
        bool newInitTimeSet = setInitDateTime(data.toDateTime());
        actor->enableActorUpdates(true);
        if (newInitTimeSet) asynchronousDataRequest(true);
        return newInitTimeSet;
    }
    case SYNC_VALID_TIME:
    {
        actor->enableActorUpdates(false);
        bool newValidTimeSet = setValidDateTime(data.toDateTime());
        actor->enableActorUpdates(true);
        if (newValidTimeSet) asynchronousDataRequest(true);
        return newValidTimeSet;
        break;
    }
    case SYNC_ENSEMBLE_MEMBER:
    {
        actor->enableActorUpdates(false);
        bool newEnsembleMemberSet = setEnsembleMember(data.toInt());
        actor->enableActorUpdates(true);
        if (newEnsembleMemberSet) asynchronousDataRequest(true);
        return newEnsembleMemberSet;
    }
    default:
        break;
    }

    return false;
}


void MNWPActorVariable::updateSyncPropertyColourHints(MSceneControl *scene)
{
    if (synchronizationControl == nullptr) return;

    // ( Also see internalSetDateTime() ).

    // Init time.
    // ==========
    bool match = (getPropertyTime(initTimeProperty)
                  == synchronizationControl->initDateTime());
    QColor colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);

    if (scene == nullptr)
        // Set colour for all scenes in which the actor appears.
        for (int i = 0; i < actor->getScenes().size(); i++)
            actor->getScenes().at(i)->setPropertyColour(
                        initTimeProperty, colour);
    else
        // Set colour only for the specifed scene.
        scene->setPropertyColour(initTimeProperty, colour);

    // Valid time.
    // ===========
    match = (getPropertyTime(validTimeProperty)
             == synchronizationControl->validDateTime());
    colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);

    if (scene == nullptr)
        // Set colour for all scenes in which the actor appears.
        for (int i = 0; i < actor->getScenes().size(); i++)
            actor->getScenes().at(i)->setPropertyColour(
                        validTimeProperty, colour);
    else
        // Set colour only for the specifed scene.
        scene->setPropertyColour(validTimeProperty, colour);
}


void MNWPActorVariable::asynchronousDataRequest(bool synchronizationRequest)
{
#ifndef DIRECT_SYNCHRONIZATION
    Q_UNUSED(synchronizationRequest);
#endif
    // Request grid.
    // ===================================================================

    QDateTime initTime  = getPropertyTime(initTimeProperty);
    QDateTime validTime = getPropertyTime(validTimeProperty);
    unsigned int member = getEnsembleMember();

    MDataRequestHelper rh;
    rh.insert("LEVELTYPE", levelType);
    rh.insert("VARIABLE", variableName);
    rh.insert("INIT_TIME", initTime);
    rh.insert("VALID_TIME", validTime);

    if (ensembleFilterOperation == "")
    {
        rh.insert("MEMBER", member);
    }
    else
    {
        rh.insert("ENS_OPERATION", ensembleFilterOperation);
        rh.insert("MEMBER_RANGE", QString("0/%1").arg(numEnsembleMembers-1));
    }

    // Add request keys from the property subgroups.
    foreach (MRequestProperties* props, propertiesList)
        props->addToRequest(&rh);

    MDataRequest r = rh.request();

    LOG4CPLUS_DEBUG(mlog, "Emitting request " << r.toStdString() << " ...");

    // Place the requests into the QSet pendingRequests to decide in O(1)
    // in asynchronousDataAvailable() whether to accept an incoming request.
    pendingRequests.insert(r);
    // Place the request into the request queue to ensure correct order
    // when incoming requests are handled.
    MRequestQueueInfo rqi;
    rqi.request = r;
    rqi.available = false;
#ifdef DIRECT_SYNCHRONIZATION
    rqi.syncchronizationRequest = synchronizationRequest;
#endif
    pendingRequestsQueue.enqueue(rqi);
#ifdef MSTOPWATCH_ENABLED
    if (!stopwatches.contains(r)) stopwatches[r] = new MStopwatch();
#endif

    dataSource->requestData(r);
}


bool MNWPActorVariable::onQtPropertyChanged(QtProperty *property)
{
    // NOTE: This function returns true if the actor should be redrawn.

    MQtProperties *properties = actor->getQtProperties();

    if (property == changeVariableProperty)
    {
        if (actor->suppressActorUpdates()) return false;

        return changeVariable();
    }

    // Connect to the time signals of the selected scene.
    else if (property == synchronizationProperty)
    {
        MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
        QString syncID = properties->getEnumItem(synchronizationProperty);
        synchronizeWith(sysMC->getSyncControl(syncID), false);

        return false;
    }

    // The init time has been changed. Reload valid times.
    else if (property == initTimeProperty)
    {
        updateValidTimeProperty();

        if (actor->suppressActorUpdates()) return false;

        asynchronousDataRequest();
        return false;
    }

    else if (property == validTimeProperty)
    {
        if (suppressUpdate) return false; // ignore if init times are being updated
        if (actor->suppressActorUpdates()) return false;

        asynchronousDataRequest();
        return false;
    }

    else if ( (property == ensembleModeProperty) ||
              (property == ensembleThresholdProperty) )
    {
        updateEnsembleProperties();

        if (actor->suppressActorUpdates()) return false;

        // Reload data.
        asynchronousDataRequest();
        return false;
    }

    else if (property == ensembleMemberProperty)
    {
        if ( ensembleMemberProperty->isEnabled() )
        {
            if (actor->suppressActorUpdates()) return false;

            asynchronousDataRequest();
            return false;
        }
    }

    else if (property == transferFunctionProperty)
    {
        return setTransferFunctionFromProperty();
    }

    else
    {
        bool redrawWithoutDataRequest = false;
        for (int i = 0; i < propertiesList.size(); i++)
        {
            if (propertiesList[i]->onQtPropertyChanged(
                        property, &redrawWithoutDataRequest)) break;
        }
        return redrawWithoutDataRequest;
    }

    return false;
}


void MNWPActorVariable::saveConfiguration(QSettings *settings)
{
    settings->setValue("dataLoaderID", dataSourceID);
    settings->setValue("levelType", levelType);
    settings->setValue("variableName", variableName);

    MQtProperties *properties = actor->getQtProperties();
    QString tfName = properties->getEnumItem(transferFunctionProperty);
    settings->setValue("transferFunction", tfName);
    QString emName = properties->getEnumItem(ensembleModeProperty);
    settings->setValue("ensembleMode", emName);

    settings->setValue("synchronizationID",
                       (synchronizationControl != nullptr) ?
                           synchronizationControl->getID() : "");
}


void MNWPActorVariable::loadConfiguration(QSettings *settings)
{
    // This method is only called from MNWPMultiVarActor::loadConfiguration().
    // Data source is set in MNWPMultiVarActor::loadConfiguration() to be
    // able to handle the case in which the stored data source is not
    // available and the user is asked for an alternative source. The
    // remaining configuration should nevertheless be loaded.

//    dataSourceID = settings->value("dataLoaderID").toString();
//    levelType    = MVerticalLevelType(settings->value("levelType").toInt());
//    variableName = settings->value("variableName").toString();

    QString tfName = settings->value("transferFunction").toString();
    if ( !setTransferFunction(tfName) )
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(QString("Variable '%1':\n"
                               "Transfer function '%2' does not exist.\n"
                               "Setting transfer function to 'None'.")
                       .arg(variableName).arg(tfName));
        msgBox.exec();
    }

    QString emName = settings->value("ensembleMode").toString();
    if ( !setEnsembleMode(emName) )
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(QString("Variable '%1':\n"
                               "Ensemble Mode '%2' does not exist.\n"
                               "Setting ensemble Mode to 'Member'.")
                       .arg(variableName).arg(emName));
        msgBox.exec();
    }

    QString syncID = settings->value("synchronizationID").toString();
    if ( !syncID.isEmpty() )
    {
        MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
        if (sysMC->getSyncControlIdentifiers().contains(syncID))
        {
            synchronizeWith(sysMC->getSyncControl(syncID));
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(QString("Variable '%1':\n"
                                   "Synchronization control '%2' does not exist.\n"
                                   "Setting synchronization control to 'None'.")
                           .arg(variableName).arg(syncID));
            msgBox.exec();

            synchronizeWith(nullptr);
        }
    }
}


bool MNWPActorVariable::setEnsembleMode(QString emName)
{
    MQtProperties *properties = actor->getQtProperties();
    QStringList emNames = properties->mEnum()->enumNames(
                ensembleModeProperty);
    int emIndex = emNames.indexOf(emName);

    if (emIndex >= 0)
    {
        properties->mEnum()->setValue(ensembleModeProperty, emIndex);
        return true;
    }

    // Set ensemble mode property to "None".
    properties->mEnum()->setValue(ensembleModeProperty, 0);

    return false; // the given ensemble mode name could not be found
}


bool MNWPActorVariable::setTransferFunction(QString tfName)
{
    MQtProperties *properties = actor->getQtProperties();
    QStringList tfNames = properties->mEnum()->enumNames(
                transferFunctionProperty);
    int tfIndex = tfNames.indexOf(tfName);

    if (tfIndex >= 0)
    {
        properties->mEnum()->setValue(transferFunctionProperty, tfIndex);
        return true;
    }

    // Set transfer function property to "None".
    properties->mEnum()->setValue(transferFunctionProperty, 0);

    return false; // the given tf name could not be found
}


void MNWPActorVariable::useFlags(bool b)
{
    useFlagsIfAvailable = b;

    if (grid != nullptr)
    {
        if (useFlagsIfAvailable)
        {
            if (textureDataFlags == nullptr)
                textureDataFlags = grid->getFlagsTexture();
            // else: if textureDataFlags points to a valid texture there's
            // nothing to be done.
        }
        else
        {
            // Flags are disabled -- if there was a texture bound release it.
            if (textureDataFlags != nullptr)
            {
                grid->releaseFlagsTexture();
                textureDataFlags = nullptr;
            }
        }
    }
}


int MNWPActorVariable::getEnsembleMember()
{
    return actor->getQtProperties()->mInt()->value(ensembleMemberProperty);
}


QtProperty *MNWPActorVariable::getPropertyGroup(QString name)
{
    if (!propertySubGroups.contains(name))
    {
        propertySubGroups[name] = actor->addProperty(GROUP_PROPERTY, name,
                                                     varPropertyGroup);
    }

    return propertySubGroups[name];
}


void MNWPActorVariable::triggerAsynchronousDataRequest(
        bool gridTopologyMayChange)
{
    if ( !actor->isInitialized() ) return;

    if (gridTopologyMayChange) gridTopologyMayHaveChanged = true;
    asynchronousDataRequest();
}


void MNWPActorVariable::actorPropertyChangeEvent(
        MPropertyType::ChangeNotification ptype, void *value)
{
    for (int i = 0; i < propertiesList.size(); i++)
        propertiesList[i]->actorPropertyChangeEvent(ptype, value);
}


bool MNWPActorVariable::hasData()
{
    return (textureDataField != nullptr);
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

bool MNWPActorVariable::setValidDateTime(const QDateTime& datetime)
{
    return internalSetDateTime(availableValidTimes, datetime, validTimeProperty);
}


bool MNWPActorVariable::setInitDateTime(const QDateTime& datetime)
{
    return internalSetDateTime(availableInitTimes, datetime, initTimeProperty);
}


bool MNWPActorVariable::setEnsembleMember(int member)
{
    // Only accept set-member-operation if the connected data source provides
    // ensemble data.
    if ( numEnsembleMembers == 0 ) return false;

    // Ensemble mean: member == -1.
    if (member < 0)
    {
        // Disable all ensemble properties. Set ensemble filter operation to
        // MEAN. The corresponding mode property is NOT set: When the sync
        // mean is disabled afterwards, the variable's ensemble settings are
        // restored.

        ensembleModeProperty->setEnabled(false);
        ensembleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
#ifdef DIRECT_SYNCHRONIZATION
        if (ensembleFilterOperation == "MEAN")
        {
            // The ensemble mode was already set to MEAN? Nothing needs to
            // be done.
            return false;
        }
        else
        {
            ensembleFilterOperation = "MEAN";
            return true;
        }
#else
        ensembleFilterOperation = "MEAN";
        asynchronousDataRequest();
#endif
    }

    // Change to the specified member.
    else
    {
        // Change member via ensemble member property.

#ifdef DIRECT_SYNCHRONIZATION
        QString prevEnsembleFilterOperation = ensembleFilterOperation;
        int prevEnsembleMember = getEnsembleMember();
#endif

        // If the variable's ensemble mode property was disabled by a previous
        // syny request for the ensemble mean, enable it. Also enable the other
        // ensemble properties according to the current mode settings.
        // NOTE: If the variable's mode settings was not set to "member"
        // before it won't be set to member now -- the member sync will not
        // be considered.
        if ( !ensembleModeProperty->isEnabled() )
        {
            ensembleModeProperty->setEnabled(true);
            // updateEnsembleProperties() will update ensembleFilterOperation
            updateEnsembleProperties();
        }

        static_cast<QtIntPropertyManager*>
                (ensembleMemberProperty->propertyManager())
                ->setValue(ensembleMemberProperty, member);

#ifdef DIRECT_SYNCHRONIZATION
        // Does a new data request need to be emitted?
        if (prevEnsembleFilterOperation != ensembleFilterOperation) return true;
        if (prevEnsembleMember != member) return true;
        return false;
#endif
    }

    return false;
}


void MNWPActorVariable::onActorCreated(MActor *actor)
{
    // If the new actor is a transfer function, add it to the list of
    // available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        MQtProperties *properties = actor->getQtProperties();
        int index = properties->mEnum()->value(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                    transferFunctionProperty);
        availableTFs << tf->transferFunctionName();
        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);
    }
}


void MNWPActorVariable::asynchronousDataAvailable(MDataRequest request)
{  
    // Decide in O(1) based on the QSet whether to accept the incoming request.
    if (!pendingRequests.contains(request)) return;
    pendingRequests.remove(request);

    LOG4CPLUS_DEBUG(mlog, "Accepting received data for request <"
                    << request.toStdString() << ">.");
    LOG4CPLUS_DEBUG(mlog, "Number of pending requests: "
                    << pendingRequests.size());

#ifdef MSTOPWATCH_ENABLED
    if (stopwatches.contains(request))
    {
        stopwatches[request]->split();
        LOG4CPLUS_DEBUG(mlog, "request processed in "
                        << stopwatches[request]->getLastSplitTime(
                            MStopwatch::SECONDS)
                        << " seconds.");
        delete stopwatches[request];
        stopwatches.remove(request);
    }
#endif

    // Mark the incoming request as "available" in the request queue. Usually
    // the requests are received in correct order, i.e. this loop on average
    // will only compare the first entry.
    for (int i = 0; i < pendingRequestsQueue.size(); i++)
    {
        if (pendingRequestsQueue[i].request == request)
        {
            pendingRequestsQueue[i].available = true;
            // Don't break; the incoming request might be relevant for multiple
            // entries in the queue.
        }
    }

    // Debug: Output content of request queue.
//    for (int i = 0; i < pendingRequestsQueue.size(); i++)
//    {
//        LOG4CPLUS_DEBUG(mlog, "RQI[" << i << "]["
//                        << pendingRequestsQueue[i].available << "] <"
//                        << pendingRequestsQueue[i].request.toStdString() << ">");
//    }

    // Prepare datafields for rendering as long as they are available in
    // the order in which they were requested.
    while ( ( !pendingRequestsQueue.isEmpty() ) &&
            pendingRequestsQueue.head().available )
    {
        MRequestQueueInfo rqi = pendingRequestsQueue.dequeue();
        MDataRequest processRequest = rqi.request;
        LOG4CPLUS_DEBUG(mlog, "Preparing for rendering: request <"
                        << processRequest.toStdString() << ">.");

        // Release currently used data items.
        releaseDataItems();

        // Acquire the new ones.
        grid = dataSource->getData(processRequest);
        textureDataField  = grid->getTexture();
        textureLonLatLevAxes = grid->getLonLatLevTexture();

        if (useFlagsIfAvailable && grid->flagsEnabled())
            textureDataFlags = grid->getFlagsTexture();
        else
            textureDataFlags = nullptr;

        if (MLonLatHybridSigmaPressureGrid *hgrid =
                dynamic_cast<MLonLatHybridSigmaPressureGrid*>(grid))
        {
            textureHybridCoefficients = hgrid->getHybridCoeffTexture();
            textureSurfacePressure = hgrid->getSurfacePressureGrid()->getTexture();
#ifdef ENABLE_HYBRID_PRESSURETEXCOORDTABLE
            texturePressureTexCoordTable = hgrid->getPressureTexCoordTexture2D();
#endif
        }

        if (MRegularLonLatStructuredPressureGrid *pgrid =
                dynamic_cast<MRegularLonLatStructuredPressureGrid*>(grid))
        {
            texturePressureTexCoordTable = pgrid->getPressureTexCoordTexture1D();
        }

        asynchronousDataAvailableEvent(grid);

        dataFieldChangedEvent();
        actor->dataFieldChangedEvent();

#ifdef DIRECT_SYNCHRONIZATION
        // If this was a synchronization request signal to the sync control
        // that it has been completed.
        if (rqi.syncchronizationRequest)
            synchronizationControl->synchronizationCompleted(this);
#endif
    }
}


/******************************************************************************
***                           PROTECTED METHODS                             ***
*******************************************************************************/

void MNWPActorVariable::releaseDataItems()
{
    // Release currently used data items.
    if (grid)
    {
        if (MLonLatHybridSigmaPressureGrid *hgrid =
                dynamic_cast<MLonLatHybridSigmaPressureGrid*>(grid))
        {
            hgrid->releaseHybridCoeffTexture();
            textureHybridCoefficients = nullptr;
            hgrid->getSurfacePressureGrid()->releaseTexture();
            textureSurfacePressure = nullptr;
#ifdef ENABLE_HYBRID_PRESSURETEXCOORDTABLE
            hgrid->releasePressureTexCoordTexture2D();
            texturePressureTexCoordTable = nullptr;
#endif
        }

        if (MRegularLonLatStructuredPressureGrid *pgrid =
                dynamic_cast<MRegularLonLatStructuredPressureGrid*>(grid))
        {
            pgrid->releasePressureTexCoordTexture1D();
            texturePressureTexCoordTable = nullptr;
        }

        if (textureDataFlags != nullptr)
        {
            grid->releaseFlagsTexture();
            textureDataFlags = nullptr;
        }

        grid->releaseTexture();
        textureDataField = nullptr;
        grid->releaseLonLatLevTexture();
        textureLonLatLevAxes = nullptr;
        dataSource->releaseData(grid);
        grid = nullptr;
    }
}


QDateTime MNWPActorVariable::getPropertyTime(QtProperty *enumProperty)
{
    MQtProperties *properties = actor->getQtProperties();
    QStringList dateStrings = properties->mEnum()->enumNames(enumProperty);

    // If the list of date strings is empty return an invalid null time.
    if (dateStrings.empty()) return QDateTime();

    int index = properties->mEnum()->value(enumProperty);
    return QDateTime::fromString(dateStrings.at(index), Qt::ISODate);
}


void MNWPActorVariable::updateInitTimeProperty()
{
    suppressUpdate = true;

    // Get the current init time value.
    QDateTime initTime  = getPropertyTime(initTimeProperty);

    // Get available init times from the data loader. Convert the QDateTime
    // objects to strings for the enum manager.
    availableInitTimes = dataSource->availableInitTimes(levelType, variableName);
    QStringList timeStrings;
    for (int i = 0; i < availableInitTimes.size(); i++)
        timeStrings << availableInitTimes.at(i).toString(Qt::ISODate);

    actor->getQtProperties()->mEnum()->setEnumNames(initTimeProperty,
                                                    timeStrings);

    setInitDateTime(initTime);

    suppressUpdate = false;
}


void MNWPActorVariable::updateValidTimeProperty()
{
    suppressUpdate = true;

    // Get the current time values.
    QDateTime initTime  = getPropertyTime(initTimeProperty);
    QDateTime validTime = getPropertyTime(validTimeProperty);

    // Get a list of the available valid times for the new init time,
    // convert the QDateTime objects to strings for the enum manager.
    availableValidTimes =
            dataSource->availableValidTimes(levelType, variableName, initTime);
    QStringList validTimeStrings;
    for (int i = 0; i < availableValidTimes.size(); i++)
        validTimeStrings << availableValidTimes.at(i).toString(Qt::ISODate);

    actor->getQtProperties()->mEnum()->setEnumNames(validTimeProperty,
                                                    validTimeStrings);

    // Try to re-set the old valid time.
    setValidDateTime(validTime);

    suppressUpdate = false;
}


void MNWPActorVariable::initEnsembleProperties()
{
    // Get the number of ensemble members.
    numEnsembleMembers =
            dataSource->availableEnsembleMembers(levelType, variableName).size();

    if ( numEnsembleMembers > 0 )
    {
        ensembleModeProperty->setEnabled(true);
        ensembleMemberProperty->setEnabled(true);
        ensembleThresholdProperty->setEnabled(false);
        MQtProperties *properties = actor->getQtProperties();
        properties->mInt()->setMinimum(ensembleMemberProperty, 0);
        properties->mInt()->setMaximum(ensembleMemberProperty, numEnsembleMembers-1);
    }
    else
    {
        ensembleModeProperty->setEnabled(false);
        ensembleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "";
    }
}


void MNWPActorVariable::updateEnsembleProperties()
{
    if ( numEnsembleMembers == 0 ) return;

    MQtProperties *properties = actor->getQtProperties();
    int mode = properties->mEnum()->value(ensembleModeProperty);

    switch ( mode )
    {
    case (0):
        // ensemble member
        ensembleMemberProperty->setEnabled(true);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "";
        break;
    case (1):
        // mean
        ensembleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "MEAN";
        break;
    case (2):
        // stddev
        ensembleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "STDDEV";
        break;
    case (3):
        // > threshold
        ensembleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(true);
        ensembleFilterOperation = QString("P>%1").arg(
                    properties->mDouble()->value(ensembleThresholdProperty));
        break;
    case (4):
        // < threshold
        ensembleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(true);
        ensembleFilterOperation = QString("P<%1").arg(
                    properties->mDouble()->value(ensembleThresholdProperty));
        break;
    case (5):
        // min
        ensembleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "MIN";
        break;
    case (6):
        // max
        ensembleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "MAX";
        break;
    case (7):
        // max-min
        ensembleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "MAX-MIN";
        break;
    }

}


/******************************************************************************
***                            PRIVATE METHODS                              ***
*******************************************************************************/

bool MNWPActorVariable::internalSetDateTime(
        const QList<QDateTime>& availableTimes,
        const QDateTime&        datetime,
        QtProperty*             timeProperty)
{
    // Find the time closest to "datetime" in the list of available valid
    // times.
    int i = -1; // use of "++i" below
    bool exactMatch = false;
    while (i < availableTimes.size()-1)
    {
        // Loop as long as datetime is larger that the currently inspected
        // element (use "++i" to have the same i available for the remaining
        // statements in this block).
        if (datetime > availableTimes.at(++i)) continue;

        // We'll only get here if datetime <= availableTimes.at(i). If we
        // have an exact match, break the loop. This is our time.
        if (availableTimes.at(i) == datetime)
        {
            exactMatch = true;
            break;
        }

        // If datetime cannot be exactly matched it lies between indices i-1
        // and i in availableTimes. Determine which is closer.
        if (i == 0) break; // if there's no i-1 we're done
        if ( abs(datetime.secsTo(availableTimes.at(i-1)))
             <= abs(datetime.secsTo(availableTimes.at(i))) ) i--;
        // "i" now contains the index of the closest available valid time.
        break;
    }

    if (i > -1)
    {
        // ( Also see updateSyncPropertyColourHints() ).

        // Update background colour of the valid time property in the connected
        // scene's property browser: green if the scene's valid time is an
        // exact match with one of the available valid time, red otherwise.
        if (synchronizationControl != nullptr)
        {
            QColor colour = exactMatch ? QColor(0, 255, 0) : QColor(255, 0, 0);
            for (int i = 0; i < actor->getScenes().size(); i++)
                actor->getScenes().at(i)->setPropertyColour(timeProperty, colour);
        }

        // Get the currently selected index.
        int currentIndex = static_cast<QtEnumPropertyManager*> (
                    timeProperty->propertyManager())->value(timeProperty);

        if (i == currentIndex)
        {
            // Index i is already the current one. Nothing needs to be done.
            return false;
        }
        else
        {
            // Set the new valid time.
            static_cast<QtEnumPropertyManager*> (timeProperty->propertyManager())
                    ->setValue(timeProperty, i);
            // A new index was set. Return true.
            return true;
        }
    }

    return false;
}


bool MNWPActorVariable::changeVariable()
{
    // Open an MSelectDataSourceDialog and re-initialize the variable with
    // information returned from the dialog.
    MSelectDataSourceDialog dialog;

    if (dialog.exec() == QDialog::Rejected) return false;

    MSelectableDataSource dsrc = dialog.getSelectedDataSource();

    LOG4CPLUS_DEBUG(mlog, "New variable has been selected: "
                    << dsrc.variableName.toStdString());

    dataSourceID = dsrc.dataSourceID;
    levelType    = dsrc.levelType;
    variableName = dsrc.variableName;

    releaseDataItems();
    actor->enableActorUpdates(false);
    initialize();
    actor->enableActorUpdates(true);

    return true;
}


bool MNWPActorVariable::setTransferFunctionFromProperty()
{
    MQtProperties *properties = actor->getQtProperties();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    QString tfName = properties->getEnumItem(transferFunctionProperty);

    if (tfName == "None")
    {
        transferFunction = nullptr;

        // Update enum items: Scan currently available actors for transfer
        // functions. Add TFs to the list displayed in the combo box of the
        // transferFunctionProperty.
        QStringList availableTFs;
        availableTFs << "None";
        foreach (MActor *ma, glRM->getActors())
        {
            if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(ma))
            {
                availableTFs << tf->transferFunctionName();
            }
        }
        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);

        return true;
    }

    // Find the selected transfer function in the list of actors from the
    // resources manager. Not very efficient, but works well enough for the
    // small number of actors at the moment..
    foreach (MActor *ma, glRM->getActors())
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(ma))
            if (tf->transferFunctionName() == tfName)
            {
                transferFunction = tf;
                return true;
            }
    }

    return false;
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/


/******************************************************************************
***                    MNWP2DSectionActorVariable                           ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNWP2DSectionActorVariable::MNWP2DSectionActorVariable(
        MNWPMultiVarActor *actor)
    : MNWPActorVariable(actor),
      targetGrid2D(nullptr),
      textureTargetGrid(nullptr),
      textureUnitTargetGrid(-1),
      imageUnitTargetGrid(-1)
{
    assert(actor != nullptr);
    MNWPMultiVarActor *a = actor;
    MQtProperties *properties = actor->getQtProperties();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    a->beginInitialiseQtProperties();

    // Property to save the current 2D section grid to a file.
    QtProperty* debugGroup = getPropertyGroup("debug");
    saveXSecGridProperty = a->addProperty(CLICK_PROPERTY, "save xsec grid",
                                          debugGroup);

    // 2D render settings.
    renderSettings.groupProperty = getPropertyGroup("rendering");

    renderSettings.renderMode = RenderMode::Disabled;
    QStringList renderModeNames;
    renderModeNames << "disabled" << "filled contours" << "pseudo colour"
                    << "line contours" << "filled and line contours"
                    << "pcolour and line contours";
    renderSettings.renderModeProperty = a->addProperty(
                ENUM_PROPERTY, "render mode", renderSettings.groupProperty);
    properties->mEnum()->setEnumNames(
                renderSettings.renderModeProperty, renderModeNames);

    renderSettings.thinContourLevelsProperty = a->addProperty(
                STRING_PROPERTY, "thin contour levels",
                renderSettings.groupProperty);

    renderSettings.thinContourColourProperty = a->addProperty(
                COLOR_PROPERTY, "thin contour colour",
                renderSettings.groupProperty);

    renderSettings.thickContourLevelsProperty = a->addProperty(
                STRING_PROPERTY, "thick contour levels",
                renderSettings.groupProperty);

    renderSettings.thickContourColourProperty = a->addProperty(
                COLOR_PROPERTY, "thick contour colour",
                renderSettings.groupProperty);

    a->endInitialiseQtProperties();
}


MNWP2DSectionActorVariable::~MNWP2DSectionActorVariable()
{
    if (imageUnitTargetGrid >= 0)
        actor->releaseImageUnit(imageUnitTargetGrid);
    if (textureUnitTargetGrid >= 0)
        actor->releaseTextureUnit(textureUnitTargetGrid);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MNWP2DSectionActorVariable::initialize()
{
    if (imageUnitTargetGrid >= 0)
        actor->releaseImageUnit(imageUnitTargetGrid);
    if (textureUnitTargetGrid >= 0)
        actor->releaseTextureUnit(textureUnitTargetGrid);

    imageUnitTargetGrid = actor->assignImageUnit();
    textureUnitTargetGrid = actor->assignTextureUnit();

    MNWPActorVariable::initialize();

    MQtProperties *properties = actor->getQtProperties();
    parseContourLevelString(properties->mString()->value(
                                renderSettings.thinContourLevelsProperty),
                            &thinContourLevels);
    parseContourLevelString(properties->mString()->value(
                                renderSettings.thickContourLevelsProperty),
                            &thickContourLevels);
}


bool MNWP2DSectionActorVariable::onQtPropertyChanged(QtProperty *property)
{
    // If the parent function returns true, the property has been accepted
    // by it (and a redraw is necessary).
    if (MNWPActorVariable::onQtPropertyChanged(property)) return true;

    MQtProperties *properties = actor->getQtProperties();

    if (property == saveXSecGridProperty)
    {
        // Click on button "save" --> trigger a saveAsNetCDF() call for the
        // target grid.
        LOG4CPLUS_DEBUG(mlog, "Saving cross-section grid.." << flush);

        QString filename = QString("cross_section_grid_%1_hPa.met3d.nc")
                .arg(variableName);
        if (targetGrid2D)
        {
            targetGrid2D->saveAsNetCDF(filename);
            LOG4CPLUS_DEBUG(mlog, "done." << flush);
        }
        else
        {
            LOG4CPLUS_ERROR(mlog, "No cross-section grid defined.");
        }

        return false;
    }

    else if (property == renderSettings.thinContourColourProperty)
    {
        thinContourColour = properties->mColor()->value(
                    renderSettings.thinContourColourProperty);
        return true; // redraw actor
    }

    else if (property == renderSettings.thinContourLevelsProperty)
    {
        QString cLevelStr = properties->mString()->value(
                    renderSettings.thinContourLevelsProperty);
        parseContourLevelString(cLevelStr, &thinContourLevels);

        if (actor->suppressActorUpdates()) return false;

        contourValuesUpdateEvent();
        return true;
    }

    else if (property == renderSettings.thickContourColourProperty)
    {
        thickContourColour = properties->mColor()->value(
                    renderSettings.thickContourColourProperty);
        return true;
    }

    else if (property == renderSettings.thickContourLevelsProperty)
    {
        QString cLevelStr = properties->mString()->value(
                    renderSettings.thickContourLevelsProperty);
        parseContourLevelString(cLevelStr, &thickContourLevels);

        if (actor->suppressActorUpdates()) return false;

        contourValuesUpdateEvent();
        return true;
    }

    else if (property == renderSettings.renderModeProperty)
    {
        renderSettings.renderMode = static_cast<RenderMode::Type>(
                    properties->mEnum()->value(
                        renderSettings.renderModeProperty));
        return true;
    }

    return false; // no redraw necessary
}


void MNWP2DSectionActorVariable::saveConfiguration(QSettings *settings)
{
    MNWPActorVariable::saveConfiguration(settings);

    MQtProperties *properties = actor->getQtProperties();

    settings->setValue("renderMode", static_cast<int>(renderSettings.renderMode));

    settings->setValue("thinContourColour", thinContourColour);
    settings->setValue("thinContourLevels", properties->mString()->value(
                           renderSettings.thinContourLevelsProperty));

    settings->setValue("thickContourColour", thickContourColour);
    settings->setValue("thickContourLevels", properties->mString()->value(
                           renderSettings.thickContourLevelsProperty));
}


void MNWP2DSectionActorVariable::loadConfiguration(QSettings *settings)
{
    MNWPActorVariable::loadConfiguration(settings);

    MQtProperties *properties = actor->getQtProperties();

    properties->mEnum()->setValue(renderSettings.renderModeProperty,
                                  settings->value("renderMode").toInt());

    const QString thinContourLevs = settings->value("thinContourLevels").toString();
    //parseContourLevelString(thinContourLevs, &thinContourLevels);
    properties->mString()->setValue(renderSettings.thinContourLevelsProperty,
                                    thinContourLevs);

    properties->mColor()->setValue(renderSettings.thinContourColourProperty,
                                   settings->value("thinContourColour").value<QColor>());

    const QString thickContourLevs = settings->value("thickContourLevels").toString();
    //parseContourLevelString(thickContourLevs, &thickContourLevels);
    properties->mString()->setValue(renderSettings.thickContourLevelsProperty,
                                    thickContourLevs);

    properties->mColor()->setValue(renderSettings.thickContourColourProperty,
                                   settings->value("thickContourColour").value<QColor>());
}


void MNWP2DSectionActorVariable::setThinContourLevelsFromString(
        QString cLevelStr)
{
    actor->getQtProperties()->mString()->setValue(
                renderSettings.thinContourLevelsProperty, cLevelStr);
}


void MNWP2DSectionActorVariable::setThickContourLevelsFromString(
        QString cLevelStr)
{
    actor->getQtProperties()->mString()->setValue(
            renderSettings.thickContourLevelsProperty, cLevelStr);
}


void MNWP2DSectionActorVariable::setRenderMode(
        MNWP2DSectionActorVariable::RenderMode::Type mode)
{
    renderSettings.renderMode = mode;
    actor->getQtProperties()->mEnum()->setValue(
                renderSettings.renderModeProperty, int(mode));
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

bool MNWP2DSectionActorVariable::parseContourLevelString(
        QString cLevelStr, QVector<double> *contourLevels)
{
    // Clear the current list of contour levels; if cLevelStr does not match
    // any accepted format no contours are drawn.
    contourLevels->clear();

    // Empty strings, i.e. no contour lines, are accepted.
    if (cLevelStr.isEmpty()) return true;

    // Match strings of format "[0,100,10]" or "[0.5,10,0.5]".
    QRegExp rxRange("^\\[([\\-|\\+]*\\d+\\.*\\d*),([\\-|\\+]*\\d+\\.*\\d*),"
                    "([\\-|\\+]*\\d+\\.*\\d*)\\]$");
    // Match strings of format "1,2,3,4,5" or "0,0.5,1,1.5,5,10" (number of
    // values is arbitrary).
    QRegExp rxList("^([\\-|\\+]*\\d+\\.*\\d*,*)+$");

    if (rxRange.indexIn(cLevelStr) == 0)
    {
        QStringList rangeValues = rxRange.capturedTexts();

        bool ok;
        double from = rangeValues.value(1).toDouble(&ok);
        double to   = rangeValues.value(2).toDouble(&ok);
        double step = rangeValues.value(3).toDouble(&ok);

        if (step > 0)
            for (double d = from; d <= to; d += step) *contourLevels << d;
        else if (step < 0)
            for (double d = from; d >= to; d += step) *contourLevels << d;

        return true;
    }
    else if (rxList.indexIn(cLevelStr) == 0)
    {
        QStringList listValues = cLevelStr.split(",");

        bool ok;
        for (int i = 0; i < listValues.size(); i++)
            *contourLevels << listValues.value(i).toDouble(&ok);

        return true;
    }

    // No RegExp could be matched.
    return false;
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                  MNWP2DHorizontalActorVariable                          ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNWP2DHorizontalActorVariable::MNWP2DHorizontalActorVariable(
        MNWPMultiVarActor *actor)
    : MNWP2DSectionActorVariable(actor),
      llcrnrlon(0),
      llcrnrlat(0),
      urcrnrlon(0),
      urcrnrlat(0),
      contourLabelsEnabled(false),
      contourLabelSuffix("")
{
    assert(actor != nullptr);
    MNWPMultiVarActor *a = actor;

    a->beginInitialiseQtProperties();

    QtProperty* renderGroup = getPropertyGroup("rendering");
    assert(renderGroup != nullptr);

    contourLabelsEnabledProperty = a->addProperty(
                BOOL_PROPERTY, "(thin) contour labels",
                renderGroup);

    contourLabelSuffixProperty = a->addProperty(
                STRING_PROPERTY, "contour label suffix",
                renderGroup);

    a->endInitialiseQtProperties();
}


MNWP2DHorizontalActorVariable::~MNWP2DHorizontalActorVariable()
{
    // Do nothing
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MNWP2DHorizontalActorVariable::saveConfiguration(QSettings *settings)
{
    MNWP2DSectionActorVariable::saveConfiguration(settings);

    settings->setValue("contourLabelsEnabled", contourLabelsEnabled);
    settings->setValue("contourLabelSuffix", contourLabelSuffix);
}


void MNWP2DHorizontalActorVariable::loadConfiguration(QSettings *settings)
{
    MNWP2DSectionActorVariable::loadConfiguration(settings);

    MQtProperties *properties = actor->getQtProperties();

    contourLabelsEnabled = settings->value("contourLabelsEnabled").toBool();
    properties->mBool()->setValue(contourLabelsEnabledProperty,
                                  contourLabelsEnabled);

    contourLabelSuffix = settings->value("contourLabelSuffix").toString();
    properties->mString()->setValue(contourLabelSuffixProperty,
                                    contourLabelSuffix);
}


bool MNWP2DHorizontalActorVariable::onQtPropertyChanged(QtProperty *property)
{
    if (MNWP2DSectionActorVariable::onQtPropertyChanged(property)) return true;

    MQtProperties *properties = actor->getQtProperties();

    if (property == contourLabelsEnabledProperty)
    {
        contourLabelsEnabled = properties->mBool()->value(
                    contourLabelsEnabledProperty);

        if (contourLabelsEnabled) updateContourLabels();

        return true;
    }
    else if (property == contourLabelSuffixProperty)
    {
        contourLabelSuffix = properties->mString()->value(
                    contourLabelSuffixProperty);

        updateContourLabels();

        return true;
    }

    return false;
}


void MNWP2DHorizontalActorVariable::computeRenderRegionParameters(
        double llcrnrlon, double llcrnrlat,
        double urcrnrlon, double urcrnrlat)
{
    this->llcrnrlon = llcrnrlon;
    this->llcrnrlat = llcrnrlat;
    this->urcrnrlon = urcrnrlon;
    this->urcrnrlat = urcrnrlat;

    // Longitudes stored in ascending order.

//FIXME (notes 18Apr2012)
    // Still unsolved:
    // -- If a grid falls apart into two disjunct regions, e.g. the grid is
    //    defined from -90 to 90 and we want to render from 0 to 360.
    // -- Repeating parts of a grid, e.g. the grid is defined from 0 to 360,
    //    we want to render from -180 to 300.

    double deltaLon = grid->lons[1] - grid->lons[0];
    double lon_west = MMOD(grid->lons[0], 360.);
    double lon_east = MMOD(grid->lons[grid->nlons-1] + deltaLon, 360.);
    bool gridIsCyclic = ( fabs(lon_west - lon_east) < M_LONLAT_RESOLUTION);

    double shiftLon = grid->lons[0];
    if (!gridIsCyclic) shiftLon = min(shiftLon, llcrnrlon);

//WORKAROUND -- Usage of M_LONLAT_RESOLUTION defined in mutil.h
    // NOTE (mr, Dec2013): Workaround to fix a float accuracy problem
    // occuring with some NetCDF data files converted from GRIB with
    // netcdf-java): For example, such longitude arrays can occur:
    // -18, -17, -16, -15, -14, -13, -12, -11, -10, -9.000004, -8.000004,
    // The latter should be equal to -9.0, -8.0 etc. The inaccuracy causes
    // wrong indices below, hence we compare to this absolute epsilon to
    // determine equality of two float values.
    // THIS WORKAROUND NEEDS TO BE REMOVED WHEN HIGHER RESOLUTIONS THAN 0.00001
    // ARE HANDLED BY MET.3D.
    // Cf. http://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
    // for potentially better solutions.

    // Find the first lon index larger than llcrnrlon.
    for (i0 = 0; i0 < grid->nlons; i0++)
    {
//        LOG4CPLUS_DEBUG_FMT(mlog, "i0=%i lons=%.10f %.10f NMOD1=%.10f NMOD2=%.10f",
//                            i0, grid->lons[i0], float(grid->lons[i0]),
//                            NMOD(grid->lons[i0]-shiftLon, 360.),
//                            NMOD(llcrnrlon-shiftLon, 360.));
        if (MMOD(grid->lons[i0]-shiftLon, 360.) + M_LONLAT_RESOLUTION
                >= MMOD(llcrnrlon-shiftLon, 360.)) break;
    }
    // Handle overshooting for non-cyclic grids (otherwise i0 = grid->nlons
    // if the bounding box is east of the grid domain).
    if (!gridIsCyclic) i0 = min(i0, grid->nlons-1);

    // Find the last lon index smaller than urcrnrlon.
    int i1;
    for (i1 = grid->nlons-1; i1 > 0; i1--)
    {
        if (MMOD(grid->lons[i1]-shiftLon, 360.)
                <= MMOD(urcrnrlon-shiftLon, 360.)) break;
    }

    // Latitude won't be cyclic, hence no modulo is required here.
    for (j0 = 0; j0 < grid->nlats; j0++)
    {
        if (grid->lats[j0] <= urcrnrlat) break;
    }
    int j1;
    for (j1 = grid->nlats-1; j1 > 0; j1--)
    {
        if (grid->lats[j1] >= llcrnrlat) break;
    }

    nlons = i1 - i0 + 1;
    if (nlons < 0) nlons = grid->nlons + nlons; // handle cyclic grids
    nlats = j1 - j0 + 1;
    if (nlats < 0) nlats = 0;

    LOG4CPLUS_DEBUG(mlog, "(grid is " << (gridIsCyclic ? "" : "not")
                    << " cyclic; shiftLon = " << shiftLon << ") "
                    << "BBox = (" << llcrnrlon << "/" << llcrnrlat << " -> "
                    << urcrnrlon << "/" << urcrnrlat << "); "
                    << "i = (" << i0 << "--" << i1 << "); j = (" << j0 << "--"
                    << j1 << "); n = (" << nlons << "/" << nlats << ")");
}


void MNWP2DHorizontalActorVariable::updateContourIndicesFromTargetGrid(
        float slicePosition_hPa)
{
    // Download generated grid from GPU via imageStore() in vertex shader
    // and glGetTexImage() here.
    textureTargetGrid->bindToLastTextureUnit();
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT,
                  targetGrid2D->data); CHECK_GL_ERROR;

    // Set the current isovalue to the target grid's vertical coordinate.
    targetGrid2D->levels[0] = slicePosition_hPa;

    // Set the target grid points outside the render domain to
    // MISSING_VALUE, so that min() and max() work correctly.
    targetGrid2D->maskRectangularRegion(i0, j0, 0, nlons-1, nlats-1, 1);

    float tgmin = targetGrid2D->min();
    float tgmax = targetGrid2D->max();

    thinContoursStartIndex = 0;
    thinContoursStopIndex  = thinContourLevels.size();

    int i = 0;
    if (!thinContourLevels.isEmpty() && tgmin > thinContourLevels.last())
    {
        thinContoursStartIndex = thinContourLevels.size();
    }
    else
    {
        for (; i < thinContourLevels.size(); i++)
            if (thinContourLevels[i] >= tgmin)
            {
                thinContoursStartIndex = i;
                break;
            }
        for (; i < thinContourLevels.size(); i++)
            if (thinContourLevels[i] > tgmax)
            {
                thinContoursStopIndex = i;
                break;
            }
    }

    thickContoursStartIndex = 0;
    thickContoursStopIndex  = thickContourLevels.size();

    i = 0;
    if (!thickContourLevels.isEmpty() && tgmin > thickContourLevels.last())
    {
        thickContoursStartIndex = thickContourLevels.size();
    }
    else
    {
        for (; i < thickContourLevels.size(); i++)
            if (thickContourLevels[i] >= tgmin)
            {
                thickContoursStartIndex = i;
                break;
            }
        for (; i < thickContourLevels.size(); i++)
            if (thickContourLevels[i] > tgmax)
            {
                thickContoursStopIndex = i;
                break;
            }
    }

    // Update contour labels.
    updateContourLabels();
}


QList<MLabel*> MNWP2DHorizontalActorVariable::getContourLabels(
        bool noOverlapping, MSceneViewGLWidget* sceneView)
{
    // If labels may overlap return the whole list.
    if (!noOverlapping) return contourLabels;

    assert(sceneView != nullptr);

    // Create new empty list of to-render labels.
    QList<MLabel*> renderList;
    renderList.reserve(contourLabels.size());

    // Collect pixel coordinates and pixel size of each selected label.
    QList<QVector3D> contourPixelCoords;
    contourPixelCoords.reserve(contourLabels.size());

    // Loop through all labels and add only those
    // which do not overlap with currently selected labels.
    foreach (MLabel* label, contourLabels)
    {
        // Get label position in clip space.
        QVector3D pixelPos = sceneView->lonLatPToClipSpace(label->anchor);
        // Viewport transformation.
        const int screenHeight = sceneView->getViewPortHeight();
        const int screenWidth = sceneView->getViewPortWidth();

        pixelPos.setX(pixelPos.x() * (screenWidth / 2.0f) + (screenWidth / 2.0f));
        pixelPos.setY(pixelPos.y() * (screenHeight / 2.0f) + (screenHeight / 2.0f));

        // Compute label width in number of pixels.
        const int labelWidth = label->width / 2;

        // Check if label intersects with other labels.
        bool labelIsOverlapping = false;
        foreach (QVector3D pixelCoord, contourPixelCoords)
        {
            // Distance between centers should be more than the sum of their half widths
            // Or greater than the label height.
            if (   std::abs(pixelPos.x() - pixelCoord.x()) <= labelWidth + pixelCoord.z()
                && std::abs(pixelPos.y() - pixelCoord.y()) <= label->size)
            {
                labelIsOverlapping = true;
                break;
            }
        }

        // If current label does not overlap, add it to the render list.
        if (!labelIsOverlapping)
        {
            renderList.append(label);
            contourPixelCoords.append(
                        QVector3D(pixelPos.x(), pixelPos.y(), label->width / 2));
        }
    }

    return renderList;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MNWP2DHorizontalActorVariable::dataFieldChangedEvent()
{
    if (gridTopologyMayHaveChanged)
    {
        if (targetGrid2D) delete targetGrid2D;

        // Initialize data object for downloading the GPU-interpolated section grid
        // to the CPU; copy lat/lon fields.
        targetGrid2D = new MRegularLonLatGrid(grid->nlats, grid->nlons);
        for (unsigned int i = 0; i < grid->nlons; i++)
            targetGrid2D->lons[i] = grid->lons[i];
        for (unsigned int j = 0; j < grid->nlats; j++)
            targetGrid2D->lats[j] = grid->lats[j];

        targetGrid2D->setTextureParameters(GL_R32F, GL_RED, GL_CLAMP, GL_NEAREST);
        textureTargetGrid = targetGrid2D->getTexture();

        computeRenderRegionParameters(llcrnrlon, llcrnrlat, urcrnrlon, urcrnrlat);

        gridTopologyMayHaveChanged = false;
    }
}


void MNWP2DHorizontalActorVariable::contourValuesUpdateEvent()
{
    updateContourIndicesFromTargetGrid(targetGrid2D->levels[0]);
}


void MNWP2DHorizontalActorVariable::updateContourLabels()
{
    // Remove all labels from text manager.
    const int contourLabelSize = contourLabels.size();

    MTextManager* tm = MGLResourcesManager::getInstance()->getTextManager();

    for (int i = 0; i < contourLabelSize; ++i)
    {
        tm->removeText(contourLabels.takeLast());
    }

    // Delta between two grid cells.
    const float deltaGrid = grid->lons[1] - grid->lons[0];

    // Collect infos of data.
    const int widthX = std::ceil(std::abs(urcrnrlon - llcrnrlon) / deltaGrid);
    const int widthY = std::ceil(std::abs(urcrnrlat - llcrnrlat) / deltaGrid);

    const int resLon = grid->nlons;
    const int resLat = grid->nlats;

    // Compute current boundary indices in grid.
    int minX = (llcrnrlon - grid->lons[0]) / deltaGrid;
    int maxX = minX + widthX;

    int minY = (grid->lats[0] - urcrnrlat) / deltaGrid;
    int maxY = minY + widthY;

    minX = min(max(0, minX), resLon - 1);
    minY = min(max(0, minY), resLat - 1);
    maxX = max(min(maxX, resLon - 1), minX);
    maxY = max(min(maxY, resLat - 1), minY);

    // Step incrementation.
    int step = 1;

    // Step along all borders and search for potential iso-contours.
    for (int i = thinContoursStartIndex; i < thinContoursStopIndex; ++i)
    {
        float isoValue = thinContourLevels.at(i);

        // Traverse grid downwards.
        for (int j = minY + 1; j <= maxY; j += step)
        {
            checkGridForContourLabel(targetGrid2D, j, minX, 1, 0, isoValue);
            checkGridForContourLabel(targetGrid2D, j, maxX, 1, 0, isoValue);
        }

        // Traverse grid rightwards.
        for (int j = minX + 1; j <= maxX; j += step)
        {
            checkGridForContourLabel(targetGrid2D, minY, j, 0, 1, isoValue);
            checkGridForContourLabel(targetGrid2D, maxY, j, 0, 1, isoValue);
        }
    }
}


void MNWP2DHorizontalActorVariable::checkGridForContourLabel(
        const MRegularLonLatGrid* grid,
        const int lat, const int lon,
        const int deltaLat, const int deltaLon,
        const float isoValue)
{
    // Check if there is any possible isovalue between two grid cells
    if (!isoLineInGridCell(targetGrid2D, lat - deltaLat, lon - deltaLon,
                           lat, lon, isoValue))
    {
        return;
    }

    QVector3D posPrev(grid->lons[lon],
                      grid->lats[lat],
                      targetGrid2D->levels[0]);

    QVector3D posNext(grid->lons[lon-deltaLon],
                      grid->lats[lat-deltaLat],
                      targetGrid2D->levels[0]);

    float valuePrev = targetGrid2D->getValue(lat, lon);
    float valueNext = targetGrid2D->getValue(lat - deltaLat, lon - deltaLon);

    addNewContourLabel(posPrev, posNext, valuePrev, valueNext, isoValue);
}


bool MNWP2DHorizontalActorVariable::isoLineInGridCell(
        const MRegularLonLatGrid* grid,
        const int jl, const int il,
        const int jr, const int ir,
        const float isoValue)
{
    bool signPrev = grid->getValue(jl, il) >= isoValue;
    bool signNext = grid->getValue(jr, ir) >= isoValue;

    return signPrev != signNext;
}


void MNWP2DHorizontalActorVariable::addNewContourLabel(
        const QVector3D& posPrev, const QVector3D& posNext,
        const float isoPrev, const float isoNext, const float isoValue)

{
    // Compute interpolant.
    float t = std::abs(isoValue - isoPrev) / std::abs(isoNext - isoPrev);

    // Compute world position of label by linear interpolation.
    QVector3D pos = posPrev * (1 - t) + posNext * t;

    MTextManager* tm = MGLResourcesManager::getInstance()->getTextManager();

    // Create text label and add it to the contour label list with text size 16
    // Add the user-defined suffix to the isoValue text.
    contourLabels.append(
                tm->addText(
                    QString("%1 %2").arg(isoValue).arg(contourLabelSuffix),
                    MTextManager::LONLATP, pos.x(), pos.y(), pos.z(),
                    16, thinContourColour, MTextManager::BASELINECENTRE,
                    true, QColor(255, 255, 255, 200), 0.3)
                );
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                NWPActor_VerticalXSec2D_Variable                         ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNWP2DVerticalActorVariable::MNWP2DVerticalActorVariable(
        MNWPMultiVarActor *actor)
    : MNWP2DSectionActorVariable(actor)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MNWP2DVerticalActorVariable::dataFieldChangedEvent()
{
    if (gridTopologyMayHaveChanged)
    {
        if (targetGrid2D) delete targetGrid2D;

        targetGrid2D = new MRegularLonLatGrid(1024, grid->nlevs);

        targetGrid2D->setTextureParameters(GL_R32F, GL_RED, GL_CLAMP, GL_NEAREST);
        textureTargetGrid = targetGrid2D->getTexture();

        gridTopologyMayHaveChanged = false;
    }

    MNWP2DSectionActorVariable::dataFieldChangedEvent();
    updateVerticalLevelRange(p_bot_hPa, p_top_hPa);
}


void MNWP2DVerticalActorVariable::updateVerticalLevelRange(
        double p_bot_hPa, double p_top_hPa)
{
    // Determine the upper/lower model levels that enclose the range
    // pbot..ptop.
    this->p_bot_hPa = p_bot_hPa;
    this->p_top_hPa = p_top_hPa;

    if (MLonLatHybridSigmaPressureGrid *hgrid =
            dynamic_cast<MLonLatHybridSigmaPressureGrid*>(grid))
    {
        // Min and max surface pressure.
        double psfc_hPa_min = hgrid->getSurfacePressureGrid()->min() / 100.;
        double psfc_hPa_max = hgrid->getSurfacePressureGrid()->max() / 100.;

        // Find the psfc_hPa_max model levels that enclose p_top_hPa..
        int p_top_kLowerPressure, p_top_kUpperPressure;
        hgrid->findEnclosingModelLevels(psfc_hPa_max, p_top_hPa,
                                        &p_top_kLowerPressure,    // use this
                                        &p_top_kUpperPressure);

        // ..and the psfc_hPa_min model levels that enclose p_bot_hPa.
        int p_bot_kLowerPressure, p_bot_kUpperPressure;
        hgrid->findEnclosingModelLevels(psfc_hPa_min, p_bot_hPa,
                                        &p_bot_kLowerPressure,
                                        &p_bot_kUpperPressure);   // use this

        gridVerticalLevelStart =
                min(p_top_kLowerPressure, p_bot_kUpperPressure);
        gridVerticalLevelCount =
                abs(p_top_kLowerPressure - p_bot_kUpperPressure);

        LOG4CPLUS_TRACE(mlog, "\tVariable: " << variableName.toStdString()
                        << ": psfc_min = " << psfc_hPa_min
                        << " hPa, psfc_max = " << psfc_hPa_max
                        << " hPa; vertical levels from " << gridVerticalLevelStart
                        << ", count " << gridVerticalLevelCount << flush);
    }
    else
    {
        // All other leveltypes do not have terrain following vertical
        // coordinates --> vertical levels are the same at every location
        // --> we can use horizontal index 0,0.
        int k_bot = grid->findLevel(0, 0, p_bot_hPa);
        int k_top = grid->findLevel(0, 0, p_top_hPa);
        gridVerticalLevelStart = min(k_bot, k_top);
        gridVerticalLevelCount = abs(k_bot - k_top) + 1;
    }
}


/******************************************************************************
*******************************************************************************/
/******************************************************************************
*******************************************************************************/

/******************************************************************************
***                     MNWP3DVolumeActorVariable                           ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNWP3DVolumeActorVariable::MNWP3DVolumeActorVariable(MNWPMultiVarActor *actor)
    : MNWPActorVariable(actor),
      textureMinMaxAccelStructure(nullptr),
      textureUnitMinMaxAccelStructure(-1)
{
}


MNWP3DVolumeActorVariable::~MNWP3DVolumeActorVariable()
{
    if (textureUnitMinMaxAccelStructure >= 0)
        actor->releaseTextureUnit(textureUnitMinMaxAccelStructure);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MNWP3DVolumeActorVariable::initialize()
{
    textureMinMaxAccelStructure = nullptr;

    if (textureUnitMinMaxAccelStructure >= 0)
        actor->releaseTextureUnit(textureUnitMinMaxAccelStructure);

    textureUnitMinMaxAccelStructure = actor->assignTextureUnit();

    MNWPActorVariable::initialize();
}


void MNWP3DVolumeActorVariable::asynchronousDataAvailableEvent(
        MStructuredGrid *grid)
{
#ifdef ENABLE_RAYCASTER_ACCELERATION
    textureMinMaxAccelStructure = grid->getMinMaxAccelTexture3D();
#endif
}


void MNWP3DVolumeActorVariable::releaseDataItems()
{
    // Release currently used data items.
    if (grid)
    {
#ifdef ENABLE_RAYCASTER_ACCELERATION
        grid->releaseMinMaxAccelTexture3D();
        textureMinMaxAccelStructure = nullptr;
#endif
    }

    MNWPActorVariable::releaseDataItems();
}


} // namespace Met3D
