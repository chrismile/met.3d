/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2016-2018 Bianca Tost
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
#include "gxfw/memberselectiondialog.h"
#include "actors/nwpvolumeraycasteractor.h"
#include "actors/nwphorizontalsectionactor.h"
#include "mainwindow.h"
#include "data/structuredgridstatisticsanalysis.h"

using namespace std;


#ifdef WIN32

unsigned int abs(unsigned int a)
{
    return a;
}

#endif


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
      aggregationDataSource(nullptr),
      gridAggregation(nullptr),
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
      textureAuxiliaryPressure(nullptr),
      textureUnitAuxiliaryPressure(-1),
      textureDummy1D(nullptr),
      textureDummy2D(nullptr),
      textureDummy3D(nullptr),
      textureUnitUnusedTextures(-1),
      transferFunction(nullptr),
      textureUnitTransferFunction(-1),
      actor(actor),
      singleVariableAnalysisControl(nullptr),
      synchronizeInitTime(true),
      synchronizeValidTime(true),
      synchronizeEnsemble(true),
      ensembleFilterOperation(""),
      ensembleMemberLoadedFromConfiguration(-1),
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

    datasourceNameProperty = a->addProperty(
                STRING_PROPERTY, "data source", varPropertyGroup);
    datasourceNameProperty->setEnabled(false);
    variableLongNameProperty = a->addProperty(
                STRING_PROPERTY, "long name", varPropertyGroup);
    variableLongNameProperty->setEnabled(false);

    changeVariablePropertyGroup = a->addProperty(
                GROUP_PROPERTY, "change/remove", varPropertyGroup);

    changeVariableProperty = a->addProperty(
                CLICK_PROPERTY, "change variable", changeVariablePropertyGroup);

    removeVariableProperty = a->addProperty(
                CLICK_PROPERTY, "remove", changeVariablePropertyGroup);

    dataStatisticsPropertyGroup = a->addProperty(
                GROUP_PROPERTY, "data statistics (entire grid)", varPropertyGroup);

    showDataStatisticsProperty = a->addProperty(
                CLICK_PROPERTY, "show statistics", dataStatisticsPropertyGroup);

    significantDigitsProperty = a->addProperty(
                DOUBLE_PROPERTY, "significant digits",
                dataStatisticsPropertyGroup);
    properties->setDouble(significantDigitsProperty, 0., 0, 1);
    significantDigitsProperty->setToolTip(
                "Digits considered for computation of the data value"
                " distribution."
                "\nIf this number is negative, the corresponding digits in"
                " front of the decimal point will be neglected.");

    histogramDisplayModeProperty = a->addProperty(
                ENUM_PROPERTY, "histogram display",
                dataStatisticsPropertyGroup);
    QStringList histogramDisplayModes;
    histogramDisplayModes << "relative frequencies"
                          << "absolute grid point count";
    properties->mEnum()->setEnumNames(histogramDisplayModeProperty,
                                      histogramDisplayModes);

    // Property: Synchronize time and ensemble with an MSyncControl instance?
    synchronizationPropertyGroup = a->addProperty(
                GROUP_PROPERTY, "synchronization", varPropertyGroup);

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    synchronizationProperty = a->addProperty(
                ENUM_PROPERTY, "synchronize with", synchronizationPropertyGroup);
    properties->mEnum()->setEnumNames(
                synchronizationProperty, sysMC->getSyncControlIdentifiers());

    synchronizeInitTimeProperty = a->addProperty(
                BOOL_PROPERTY, "sync init time", synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeInitTimeProperty, synchronizeInitTime);
    synchronizeValidTimeProperty = a->addProperty(
                BOOL_PROPERTY, "sync valid time", synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeValidTimeProperty, synchronizeValidTime);
    synchronizeEnsembleProperty = a->addProperty(
                BOOL_PROPERTY, "sync ensemble", synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeEnsembleProperty, synchronizeEnsemble);

    // Properties for init and valid time.
    initTimeProperty = a->addProperty(
                ENUM_PROPERTY, "initialisation", varPropertyGroup);
    validTimeProperty = a->addProperty(
                ENUM_PROPERTY, "valid", varPropertyGroup);

    // Properties for ensemble control.
    ensembleMultiMemberSelectionProperty = a->addProperty(
                CLICK_PROPERTY, "select members", varPropertyGroup);
    ensembleMultiMemberSelectionProperty->setToolTip(
                "select which ensemble members this variable should utilize");
    ensembleMultiMemberProperty = a->addProperty(
                STRING_PROPERTY, "utilized members", varPropertyGroup);
    ensembleMultiMemberProperty->setEnabled(false);

    QStringList ensembleModeNames;
    ensembleModeNames << "member" << "mean" << "standard deviation"
                      << "p(> threshold)" << "p(< threshold)"
                      << "min" << "max" << "max-min";
    multipleEnsembleMembersEnabled =
            actor->supportsMultipleEnsembleMemberVisualization();
    // Check if the actor supports simultaneous visualization of multiple
    // ensemble members. If yes, the corresponding ensemble mode will trigger
    // the request of a "grid aggregation" containing all requested members.
    // Search the code for "if (multipleEnsembleMembersEnabled)...".
    if (multipleEnsembleMembersEnabled)
    {
        ensembleModeNames << "multiple members";
    }
    ensembleModeProperty = a->addProperty(
                ENUM_PROPERTY, "ensemble mode", varPropertyGroup);
    properties->mEnum()->setEnumNames(ensembleModeProperty, ensembleModeNames);

    ensembleSingleMemberProperty = a->addProperty(
                ENUM_PROPERTY, "ensemble member", varPropertyGroup);

    ensembleThresholdProperty = a->addProperty(
                DOUBLE_PROPERTY, "ensemble threshold", varPropertyGroup);
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

    // Debug properties.
    QtProperty* debugGroup = getPropertyGroup("debug");
    dumpGridDataProperty = a->addProperty(CLICK_PROPERTY, "dump grid data",
                                          debugGroup);

    // Observe the creation/deletion of other actors -- if these are transfer
    // functions, add to the list displayed in the transfer function property.
    connect(glRM, SIGNAL(actorCreated(MActor*)), SLOT(onActorCreated(MActor*)));
    connect(glRM, SIGNAL(actorDeleted(MActor*)), SLOT(onActorDeleted(MActor*)));
    connect(glRM, SIGNAL(actorRenamed(MActor*, QString)),
            SLOT(onActorRenamed(MActor*, QString)));

    actor->endInitialiseQtProperties();

    if (multipleEnsembleMembersEnabled)
    {
        // Create an aggregation source that belongs to this actor variable.
        aggregationDataSource = new MGridAggregationDataSource();
        connect(aggregationDataSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousDataAvailable(MDataRequest)));
    }
}


MNWPActorVariable::~MNWPActorVariable()
{
    // Release data fields.
    releaseDataItems();
    releaseAggregatedDataItems();

    // Disconnect signals.
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    disconnect(glRM, SIGNAL(actorCreated(MActor*)),
               this, SLOT(onActorCreated(MActor*)));
    disconnect(glRM, SIGNAL(actorDeleted(MActor*)),
               this, SLOT(onActorDeleted(MActor*)));
    disconnect(glRM, SIGNAL(actorRenamed(MActor*, QString)),
               this, SLOT(onActorRenamed(MActor*, QString)));

    // Delete synchronization links (don't update the already deleted GUI
    // properties anymore...).
    synchronizeWith(nullptr, false);

    if (multipleEnsembleMembersEnabled)
    {
        disconnect(aggregationDataSource,
                   SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousDataAvailable(MDataRequest)));
        delete aggregationDataSource;
    }

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
    if (textureUnitAuxiliaryPressure >= 0)
        actor->releaseTextureUnit(textureUnitAuxiliaryPressure);
    if (textureUnitUnusedTextures >= 0)
        actor->releaseTextureUnit(textureUnitUnusedTextures);

    foreach (MRequestProperties *requestProperty, propertiesList)
    {
        delete requestProperty;
    }

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
    actor->enableActorUpdates(false);

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
    if (textureUnitAuxiliaryPressure >= 0)
        actor->releaseTextureUnit(textureUnitAuxiliaryPressure);
    if (textureUnitUnusedTextures >= 0)
        actor->releaseTextureUnit(textureUnitUnusedTextures);

    textureUnitDataField             = actor->assignTextureUnit();
    textureUnitLonLatLevAxes         = actor->assignTextureUnit();
    textureUnitTransferFunction      = actor->assignTextureUnit();
    textureUnitSurfacePressure       = actor->assignTextureUnit();
    textureUnitHybridCoefficients    = actor->assignTextureUnit();
    textureUnitDataFlags             = actor->assignTextureUnit();
    textureUnitPressureTexCoordTable = actor->assignTextureUnit();
    textureUnitAuxiliaryPressure     = actor->assignTextureUnit();
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

        if (multipleEnsembleMembersEnabled)
        {
            connect(dataSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                    this, SLOT(asynchronousDataAvailable(MDataRequest)));

            // Connect the new data source to this variable's aggregation source.
            aggregationDataSource->setMemoryManager(dataSource->getMemoryManager());
            aggregationDataSource->setScheduler(dataSource->getScheduler());
            aggregationDataSource->setInputSource(dataSource);
        }
    }

    textureDataField = textureHybridCoefficients =
            textureLonLatLevAxes = textureSurfacePressure =
            textureDataFlags = texturePressureTexCoordTable =
            textureAuxiliaryPressure = nullptr;

    gridTopologyMayHaveChanged = true;

    actor->getQtProperties()->mString()->setValue(
                datasourceNameProperty,
                dataSourceID);
    actor->getQtProperties()->mString()->setValue(
                variableLongNameProperty,
                dataSource->variableLongName(levelType, variableName));

    requestPropertiesFactory->updateProperties(&propertiesList,
                                               dataSource->requiredKeys());

    updateInitTimeProperty();
    updateValidTimeProperty();
    initEnsembleProperties();

    // Get values from sync control, if connected to one.
    if (synchronizationControl == nullptr)
    {
        synchronizeInitTimeProperty->setEnabled(false);
        synchronizeInitTime = false;
        synchronizeValidTimeProperty->setEnabled(false);
        synchronizeValidTime = false;
        synchronizeEnsembleProperty->setEnabled(false);
        synchronizeEnsemble = false;
    }
    else
    {
        if (synchronizeInitTime)
        {
            setInitDateTime(synchronizationControl->initDateTime());
        }
        updateValidTimeProperty();
        if (synchronizeValidTime)
        {
            setValidDateTime(synchronizationControl->validDateTime());
        }
        if (synchronizeEnsemble)
        {
            setEnsembleMember(synchronizationControl->ensembleMember());
        }
    }

    updateTimeProperties();
    updateEnsembleProperties();
    updateSyncPropertyColourHints();

    setTransferFunctionFromProperty();

    if (textureDummy1D == nullptr)
        textureDummy1D = new GL::MTexture(GL_TEXTURE_1D, GL_ALPHA32F_ARB, 1);
    if (textureDummy2D == nullptr)
        textureDummy2D = new GL::MTexture(GL_TEXTURE_2D, GL_ALPHA32F_ARB, 1, 1);
    if (textureDummy3D == nullptr)
        textureDummy3D = new GL::MTexture(GL_TEXTURE_3D, GL_ALPHA32F_ARB, 1, 1, 1);

    // Load data field.
    asynchronousDataRequest();

    actor->enableActorUpdates(true);
}


void MNWPActorVariable::synchronizeWith(
        MSyncControl *sync, bool updateGUIProperties)

{
    if (synchronizationControl == sync)
    {
        return;
    }

    // Reset connection to current synchronization control.
    // ====================================================

    // If the variable is currently connected to a sync control, reset the
    // background colours of the valid and init time properties (they have
    // been set to red/green from this class to indicate time sync status,
    // see setValidDateTime()) and disconnect the signals.
    if (synchronizationControl != nullptr)
    {
        foreach (MSceneControl* scene, actor->getScenes())
        {
            scene->variableDeletesSynchronizationWith(synchronizationControl);
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
    // Connect to new sync control and try to switch to its current times.
    synchronizationControl = sync;

    // Update "synchronizationProperty".
    // =================================
    if (updateGUIProperties)
    {
        MQtProperties *properties = actor->getQtProperties();
        QString displayedSyncID =
                properties->getEnumItem(synchronizationProperty);
        QString newSyncID =
                (sync == nullptr) ? "None" : synchronizationControl->getID();
        if (displayedSyncID != newSyncID)
        {
            actor->enableActorUpdates(false);
            properties->setEnumItem(synchronizationProperty, newSyncID);
            actor->enableActorUpdates(true);
        }
    }

    // Connect to new sync control and synchronize.
    // ============================================
    if (sync != nullptr)
    {
        // Tell the actor's scenes that this variable synchronized with this
        // sync control.
        foreach (MSceneControl* scene, actor->getScenes())
        {
            scene->variableSynchronizesWith(sync);
        }

#ifdef DIRECT_SYNCHRONIZATION
        synchronizationControl->registerSynchronizedClass(this);
#else
//TODO (mr, 01Dec2015) -- add checks for synchronizeInitTime etc..
        connect(sync, SIGNAL(initDateTimeChanged(QDateTime)),
                this, SLOT(setInitDateTime(QDateTime)));
        connect(sync, SIGNAL(validDateTimeChanged(QDateTime)),
                this, SLOT(setValidDateTime(QDateTime)));
        connect(sync, SIGNAL(ensembleMemberChanged(int)),
                this, SLOT(setEnsembleMember(int)));
#endif
        if (updateGUIProperties)
        {
            actor->enableActorUpdates(false);
            MQtProperties *properties = actor->getQtProperties();
            synchronizeInitTimeProperty->setEnabled(true);
            synchronizeInitTime = properties->mBool()->value(
                        synchronizeInitTimeProperty);
            synchronizeValidTimeProperty->setEnabled(true);
            synchronizeValidTime = properties->mBool()->value(
                        synchronizeValidTimeProperty);
            synchronizeEnsembleProperty->setEnabled(true);
            synchronizeEnsemble = properties->mBool()->value(
                        synchronizeEnsembleProperty);
            actor->enableActorUpdates(true);
        }

        // Disable actor updates to avoid asynchonous data requests being
        // triggered from the individual time/ensemble updates before all
        // properties have been updated.
        actor->enableActorUpdates(false);
        if (synchronizeInitTime)
        {
            setInitDateTime(sync->initDateTime());
        }
        if (synchronizeValidTime)
        {
            setValidDateTime(sync->validDateTime());
        }
        if (synchronizeEnsemble)
        {
            setEnsembleMember(sync->ensembleMember());
        }
        actor->enableActorUpdates(true);

        // Trigger data request manually after all properties have been
        // synchronised.
        if (actor->isInitialized())
        {
            asynchronousDataRequest();
        }
    }
    else
    {
        // No synchronization. Reset property colours and disable sync
        // checkboxes.
        foreach (MSceneControl* scene, actor->getScenes())
        {
            scene->resetPropertyColour(initTimeProperty);
            scene->resetPropertyColour(validTimeProperty);
            scene->resetPropertyColour(ensembleSingleMemberProperty);
        }

        if (updateGUIProperties)
        {
            actor->enableActorUpdates(false);
            synchronizeInitTimeProperty->setEnabled(false);
            synchronizeInitTime = false;
            synchronizeValidTimeProperty->setEnabled(false);
            synchronizeValidTime = false;
            synchronizeEnsembleProperty->setEnabled(false);
            synchronizeEnsemble = false;
            actor->enableActorUpdates(true);
        }
    }

    // Update "synchronize xyz" GUI properties.
    // ========================================
    if (updateGUIProperties && actor->isInitialized())
    {
        updateTimeProperties();
        updateEnsembleProperties();
    }
}


bool MNWPActorVariable::synchronizationEvent(
        MSynchronizationType syncType, QVector<QVariant> data)
{
    switch (syncType)
    {
    case SYNC_INIT_TIME:
    {
        if (!synchronizeInitTime)
        {
            return false;
        }
        actor->enableActorUpdates(false);
        bool newInitTimeSet = setInitDateTime(data.at(0).toDateTime());
        actor->enableActorUpdates(true);
        if (newInitTimeSet)
        {
            asynchronousDataRequest(true);
        }
        return newInitTimeSet;
    }
    case SYNC_VALID_TIME:
    {
        if (!synchronizeValidTime)
        {
            return false;
        }
        actor->enableActorUpdates(false);
        bool newValidTimeSet = setValidDateTime(data.at(0).toDateTime());
        actor->enableActorUpdates(true);
        if (newValidTimeSet)
        {
            asynchronousDataRequest(true);
        }
        return newValidTimeSet;
    }
    case SYNC_INIT_VALID_TIME:
    {
        actor->enableActorUpdates(false);
        bool newInitTimeSet = false;
        bool newValidTimeSet = false;
        if (synchronizeInitTime)
        {
            newInitTimeSet = setInitDateTime(data.at(0).toDateTime());
        }
        if (synchronizeValidTime)
        {
            newValidTimeSet = setValidDateTime(data.at(1).toDateTime());
        }
        actor->enableActorUpdates(true);
        if (newInitTimeSet || newValidTimeSet)
        {
            asynchronousDataRequest(true);
        }
        return (newInitTimeSet || newValidTimeSet);
    }
    case SYNC_ENSEMBLE_MEMBER:
    {
        if (!synchronizeEnsemble)
        {
            return false;
        }
        actor->enableActorUpdates(false);
        bool newEnsembleMemberSet = setEnsembleMember(data.at(0).toInt());
        actor->enableActorUpdates(true);
        if (newEnsembleMemberSet)
        {
            asynchronousDataRequest(true);
        }
        return newEnsembleMemberSet;
    }
    default:
        break;
    }

    return false;
}


void MNWPActorVariable::updateSyncPropertyColourHints(MSceneControl *scene)
{
    if (synchronizationControl == nullptr)
    {
        // No synchronization -- reset all property colours.
        setPropertyColour(initTimeProperty, QColor(), true, scene);
        setPropertyColour(validTimeProperty, QColor(), true, scene);
        setPropertyColour(ensembleSingleMemberProperty, QColor(), true, scene);
    }
    else
    {
        // ( Also see internalSetDateTime() ).

        // Init time.
        // ==========
        bool match = (getPropertyTime(initTimeProperty)
                      == synchronizationControl->initDateTime());
        QColor colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);
        setPropertyColour(initTimeProperty, colour,
                          !synchronizeInitTime, scene);

        // Valid time.
        // ===========
        match = (getPropertyTime(validTimeProperty)
                 == synchronizationControl->validDateTime());
        colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);
        setPropertyColour(validTimeProperty, colour,
                          !synchronizeValidTime, scene);

        // Ensemble.
        // =========
        match = (getEnsembleMember()
                 == synchronizationControl->ensembleMember());
        colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);
        setPropertyColour(ensembleSingleMemberProperty, colour,
                          !synchronizeEnsemble, scene);
    }
}


void MNWPActorVariable::setPropertyColour(
        QtProperty* property, const QColor &colour, bool resetColour,
        MSceneControl *scene)
{
    if (resetColour)
    {
        if (scene == nullptr)
        {
            // Reset colour for all scenes in which the actor appears.
            foreach (MSceneControl* sc, actor->getScenes())
            {
                sc->resetPropertyColour(property);
            }
        }
        else
        {
            // Reset colour only for the specifed scene.
            scene->resetPropertyColour(property);
        }
    }
    else
    {
        if (scene == nullptr)
        {
            // Set colour for all scenes in which the actor appears.
            foreach (MSceneControl* sc, actor->getScenes())
            {
                sc->setPropertyColour(property, colour);
            }
        }
        else
        {
            // Set colour only for the specifed scene.
            scene->setPropertyColour(property, colour);
        }
    }
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

    if ((ensembleFilterOperation == "")
            || (ensembleFilterOperation == "MULTIPLE_MEMBERS"))
    {
        rh.insert("MEMBER", member);
    }
    else
    {
        rh.insert("ENS_OPERATION", ensembleFilterOperation);
        rh.insert("SELECTED_MEMBERS", selectedEnsembleMembers);
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

    // Special case: If ensemble mode is set to "multiple members". A grid
    // aggregation containing the required members is requested IN ADDITION to
    // the single member requested before. The single member request before is
    // REDUNDANT and should be suppressed in the future.

    //TODO (mr, 18Apr2018) -- Revise this architecture.
    if (ensembleFilterOperation == "MULTIPLE_MEMBERS")
    {
        if (!multipleEnsembleMembersEnabled)
        {
            return;
        }
        rh.remove("MEMBER");
        rh.insert("ENS_OPERATION", ensembleFilterOperation);
        rh.insert("SELECTED_MEMBERS", selectedEnsembleMembers);
        r = rh.request();

        LOG4CPLUS_DEBUG(mlog, "Emitting additional 'multiple ensemble member'"
                              " request " << r.toStdString() << " ...");

        pendingRequests.insert(r);
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

        aggregationDataSource->requestData(r);
    }
}


bool MNWPActorVariable::onQtPropertyChanged(QtProperty *property)
{
    // NOTE: This function returns true if the actor should be redrawn.

    MQtProperties *properties = actor->getQtProperties();

    if (property == changeVariableProperty)
    {
        if (actor->suppressActorUpdates())
        {
            return false;
        }

        return changeVariable();
    }

    // Show data statistics of the variable.
    else if (property == showDataStatisticsProperty)
    {
        if (actor->suppressActorUpdates())
        {
            return false;
        }
        runStatisticalAnalysis(
                    properties->mDouble()->value(significantDigitsProperty),
                    properties->mEnum()->value(histogramDisplayModeProperty));
    }

    // Connect to the time signals of the selected scene.
    else if (property == synchronizationProperty)
    {
        if (actor->suppressActorUpdates()) return false;

        MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
        QString syncID = properties->getEnumItem(synchronizationProperty);
        synchronizeWith(sysMC->getSyncControl(syncID));

        return false;
    }

    else if (property == synchronizeInitTimeProperty)
    {
        synchronizeInitTime = properties->mBool()->value(
                    synchronizeInitTimeProperty);
        updateTimeProperties();

        if (actor->suppressActorUpdates()) return false;

        if (synchronizeInitTime)
        {
            if (setInitDateTime(synchronizationControl->initDateTime()))
            {
                asynchronousDataRequest();
            }
        }

        return false;
    }

    else if (property == synchronizeValidTimeProperty)
    {
        synchronizeValidTime = properties->mBool()->value(
                    synchronizeValidTimeProperty);
        updateTimeProperties();

        if (actor->suppressActorUpdates()) return false;

        if (synchronizeValidTime)
        {
            if (setValidDateTime(synchronizationControl->validDateTime()))
            {
                asynchronousDataRequest();
            }
        }

        return false;
    }

    else if (property == synchronizeEnsembleProperty)
    {
        synchronizeEnsemble = properties->mBool()->value(
                    synchronizeEnsembleProperty);
        updateEnsembleProperties();
        updateSyncPropertyColourHints();

        if (actor->suppressActorUpdates()) return false;

        if (synchronizeEnsemble)
        {
            if (setEnsembleMember(synchronizationControl->ensembleMember()))
            {
                asynchronousDataRequest();
            }
        }

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

    else if (property == ensembleSingleMemberProperty)
    {
        if (actor->suppressActorUpdates()) return false;

        if ( ensembleSingleMemberProperty->isEnabled() )
        {
            asynchronousDataRequest();
            return false;
        }
    }

    else if (property == ensembleMultiMemberSelectionProperty)
    {
        if (actor->suppressActorUpdates()) return false;

        MMemberSelectionDialog dlg;
        dlg.setAvailableEnsembleMembers(dataSource->availableEnsembleMembers(
                                            levelType, variableName));
        dlg.setSelectedMembers(selectedEnsembleMembers);

        if ( dlg.exec() == QDialog::Accepted )
        {
            // Get set of selected members from dialog, update
            // ensembleMultiMemberProperty to display set to user and, if
            // necessary, request new data field.
            QSet<unsigned int> selMembers = dlg.getSelectedMembers();
            if ( !selMembers.isEmpty() )
            {
                selectedEnsembleMembers = selMembers;

                // Update the current data field if either the currently
                // selected member has changed (because the previously selected
                // one is not available anymore) or the ensemble more is set to
                // mean, std.dev, etc (in this case the computed field needs to
                // be recomputed based on the new member set).

                // Selected member has changed?
                bool updateDataField = updateEnsembleSingleMemberProperty();
                // ..or ens mode is != member.
                updateDataField |= !ensembleFilterOperation.isEmpty();

                if (updateDataField) asynchronousDataRequest();
                return false;
            }
            else
            {
                // The user has selected an emtpy set of members. Display a
                // warning and do NOT accept the empty set.
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setText("You need to select at least one member.");
                msgBox.exec();
            }
        }
    }

    else if (property == transferFunctionProperty)
    {
        return setTransferFunctionFromProperty();
    }

    else if (property == dumpGridDataProperty)
    {
        if (grid)
        {
            // Dump raw grid data to console, printing first 200 data values.
            grid->dumpGridData(200);
        }
        return false;
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

    // Save data statistics properties.
    // ================================
    settings->setValue("statisticsSignificantDigits",
                       properties->mDouble()->value(significantDigitsProperty));
    settings->setValue("statisticsHistogramDisplayMode",
                       properties->mEnum()->value(histogramDisplayModeProperty));

    // Save synchronization properties.
    // ================================
    settings->setValue("synchronizationID",
                       (synchronizationControl != nullptr) ?
                           synchronizationControl->getID() : "");
    settings->setValue("synchronizeInitTime",
                       properties->mBool()->value(synchronizeInitTimeProperty));
    settings->setValue("synchronizeValidTime",
                       properties->mBool()->value(synchronizeValidTimeProperty));
    settings->setValue("synchronizeEnsemble",
                       properties->mBool()->value(synchronizeEnsembleProperty));

    // Save ensemble mode properties.
    // ==============================
    settings->setValue("ensembleUtilizedMembers",
                       MDataRequestHelper::uintSetToString(
                           selectedEnsembleMembers));
    settings->setValue("ensembleMode",
                       properties->getEnumItem(ensembleModeProperty));
    settings->setValue("ensembleSingleMember",
                       properties->getEnumItem(ensembleSingleMemberProperty));
    settings->setValue("ensembleThreshold",
                       properties->mDouble()->value(ensembleThresholdProperty));

    // Save rendering properties.
    // ==========================
    settings->setValue("transferFunction",
                       properties->getEnumItem(transferFunctionProperty));

    // Save properties of connected request property subgroups.
    foreach (MRequestProperties* props, propertiesList)
        props->saveConfiguration(settings);
}


void MNWPActorVariable::loadConfiguration(QSettings *settings)
{
    // This method is only called from MNWPMultiVarActor::loadConfiguration().
    // Data source is set in MNWPMultiVarActor::loadConfiguration() to be
    // able to handle the case in which the stored data source is not
    // available and the user is asked for an alternative source. The
    // remaining configuration should nevertheless be loaded.

    MQtProperties *properties = actor->getQtProperties();

    // Load data statistics properties.
    // ================================
    properties->mDouble()->setValue(
                significantDigitsProperty,
                settings->value("statisticsSignificantDigits", 0.).toDouble());
    properties->mEnum()->setValue(
                histogramDisplayModeProperty,
                settings->value(
                    "statisticsHistogramDisplayMode",
                    MStructuredGridStatisticsAnalysisControl
                    ::RELATIVE_FREQUENCY_DISTRIBUTION).toInt());

    // Load ensemble mode properties.
    // ==============================
    selectedEnsembleMembers = MDataRequestHelper::uintSetFromString(
                settings->value("ensembleUtilizedMembers").toString());

    // At this time, the variables hasn't been initialized yet .. store the
    // loaded ensemble member temporarily; updateEnsembleSingleMemberProperty()
    // will make use of this variable.
    ensembleMemberLoadedFromConfiguration =
            settings->value("ensembleSingleMember", -1).toInt();

    properties->mDouble()->setValue(
                ensembleThresholdProperty,
                settings->value("ensembleThreshold", 0.).toDouble());

    QString emName = settings->value("ensembleMode").toString();
    if ( !setEnsembleMode(emName) )
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(QString("Variable '%1':\n"
                               "Ensemble mode '%2' does not exist.\n"
                               "Setting ensemble mode to 'member'.")
                       .arg(variableName).arg(emName));
        msgBox.exec();
    }

    // Load synchronization properties (AFTER the ensemble mode properties
    // have been loaded; sync may overwrite some settings).
    // ===================================================================
    properties->mBool()->setValue(
                synchronizeInitTimeProperty,
                settings->value("synchronizeInitTime", true).toBool());
    properties->mBool()->setValue(
                synchronizeValidTimeProperty,
                settings->value("synchronizeValidTime", true).toBool());
    properties->mBool()->setValue(
                synchronizeEnsembleProperty,
                settings->value("synchronizeEnsemble", true).toBool());

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

    // Load rendering properties.
    // ==========================
    QString tfName = settings->value("transferFunction").toString();
    while (!setTransferFunction(tfName))
    {
        if (!MTransferFunction::loadMissingTransferFunction(
                    tfName, MTransferFunction1D::staticActorType(),
                    "Variable ", variableName, settings))
        {
            break;
        }
    }

    // We need the properties list filled at this point to be able to load the
    // configurations of properties at creation of actor.
    if (propertiesList.empty())
    {
        // Get a pointer to the new source and connect to its request completed
        // signal.
        MAbstractDataSource *source =
                MSystemManagerAndControl::getInstance()->getDataSource(dataSourceID);
        dataSource = dynamic_cast<MWeatherPredictionDataSource*>(source);

        if (dataSource != nullptr)
        {
            requestPropertiesFactory->updateProperties(
                        &propertiesList, dataSource->requiredKeys());
        }
    }

    // Load properties of connected request property subgroups.
    foreach (MRequestProperties* props, propertiesList)
        props->loadConfiguration(settings);
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


void MNWPActorVariable::setTransferFunctionToFirstAvailable()
{
    MQtProperties *properties = actor->getQtProperties();
    // Set transfer function property to first entry below "None" if present.
    properties->mEnum()->setValue(transferFunctionProperty, 1);
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
    QString memberString = actor->getQtProperties()->getEnumItem(
                ensembleSingleMemberProperty);

    bool ok = true;
    int member = memberString.toInt(&ok);

    if (ok) return member; else return -99999;
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
    // Ensemble mean: member == -1.
    // ============================
    if (member < 0)
    {
        // Set ensemble filter operation to MEAN.
#ifdef DIRECT_SYNCHRONIZATION
        if (ensembleFilterOperation == "MEAN")
        {
            // The ensemble mode was already set to MEAN? Nothing needs to
            // be done.
            return false;
        }
        else
        {
            setEnsembleMode("mean");
            return true;
        }
#else
        ensembleFilterOperation = "MEAN";
        asynchronousDataRequest();
#endif
    }

    // Change to the specified member.
    // ===============================
    else
    {
        // Change member via ensemble member property.

#ifdef DIRECT_SYNCHRONIZATION
        QString prevEnsembleFilterOperation = ensembleFilterOperation;
        int prevEnsembleMember = getEnsembleMember();
#endif

        if (ensembleFilterOperation != "MEMBER") setEnsembleMode("member");

        setEnumPropertyClosest<unsigned int>(
                    selectedEnsembleMembersAsSortedList, (unsigned int)member,
                    ensembleSingleMemberProperty);

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
        // Don't render while the properties are being updated.
        this->actor->enableEmissionOfActorChangedSignal(false);

        MQtProperties *properties = actor->getQtProperties();
        int index = properties->mEnum()->value(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                    transferFunctionProperty);
        availableTFs << tf->transferFunctionName();
        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);

        this->actor->enableEmissionOfActorChangedSignal(true);
    }

    if (MSpatial1DTransferFunction *stf =
            dynamic_cast<MSpatial1DTransferFunction*>(actor))
    {
        if (dynamic_cast<MNWP2DHorizontalActorVariable*>(this))
        {
            // Don't render while the properties are being updated.
            this->actor->enableEmissionOfActorChangedSignal(false);

            MQtProperties *properties = actor->getQtProperties();
            int index = properties->mEnum()->value(spatialTransferFunctionProperty);
            QStringList availableSTFs = properties->mEnum()->enumNames(
                        spatialTransferFunctionProperty);
            availableSTFs << stf->transferFunctionName();
            properties->mEnum()->setEnumNames(spatialTransferFunctionProperty,
                                              availableSTFs);
            properties->mEnum()->setValue(spatialTransferFunctionProperty, index);

            this->actor->enableEmissionOfActorChangedSignal(true);
        }
    }
}


void MNWPActorVariable::onActorDeleted(MActor *actor)
{
    // If the deleted actor is a transfer function, remove it from the list of
    // available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        this->actor->enableEmissionOfActorChangedSignal(false);

        MQtProperties *properties = actor->getQtProperties();
        QString tFName = properties->getEnumItem(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                    transferFunctionProperty);

        availableTFs.removeOne(tf->getName());

        // Get the current index of the transfer function selected. If the
        // transfer function is the one to be deleted, the selection is set to
        // 'None'.
        int index = availableTFs.indexOf(tFName);

        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);

        this->actor->enableEmissionOfActorChangedSignal(true);
    }

    // If the deleted actor is a spatial transfer function, remove it from the
    // list of available spatial transfer functions.
    if (MSpatial1DTransferFunction *stf =
            dynamic_cast<MSpatial1DTransferFunction*>(actor))
    {
        if (dynamic_cast<MNWP2DHorizontalActorVariable*>(this))
        {
            this->actor->enableEmissionOfActorChangedSignal(false);

            MQtProperties *properties = actor->getQtProperties();
            QString sTFName =
                    properties->getEnumItem(spatialTransferFunctionProperty);
            QStringList availableSTFs = properties->mEnum()->enumNames(
                        spatialTransferFunctionProperty);

            availableSTFs.removeOne(stf->getName());

            // Get the current index of the spatial transfer function selected.
            // If the transfer function is the one to be deleted, the selection
            // is set to 'None'.
            int index = availableSTFs.indexOf(sTFName);

            properties->mEnum()->setEnumNames(spatialTransferFunctionProperty,
                                              availableSTFs);
            properties->mEnum()->setValue(spatialTransferFunctionProperty,
                                          index);

            this->actor->enableEmissionOfActorChangedSignal(true);
        }
    }
}


void MNWPActorVariable::onActorRenamed(MActor *actor, QString oldName)
{
    // If the renamed actor is a transfer function, change its name in the list
    // of available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        // Don't render while the properties are being updated.
        this->actor->enableEmissionOfActorChangedSignal(false);

        MQtProperties *properties = this->actor->getQtProperties();
        int index = properties->mEnum()->value(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                    transferFunctionProperty);

        // Replace affected entry.
        availableTFs[availableTFs.indexOf(oldName)] = tf->getName();

        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);

        this->actor->enableEmissionOfActorChangedSignal(true);
    }

    // If the renamed actor is a spatial transfer function, change its name in
    // the list of available spatial transfer functions.
    if (MSpatial1DTransferFunction *stf =
            dynamic_cast<MSpatial1DTransferFunction*>(actor))
    {
        if (dynamic_cast<MNWP2DHorizontalActorVariable*>(this))
        {
            // Don't render while the properties are being updated.
            this->actor->enableEmissionOfActorChangedSignal(false);

            MQtProperties *properties = this->actor->getQtProperties();
            int index =
                    properties->mEnum()->value(spatialTransferFunctionProperty);
            QStringList availableSTFs = properties->mEnum()->enumNames(
                        spatialTransferFunctionProperty);

            // Replace affected entry.
            availableSTFs[availableSTFs.indexOf(oldName)] = stf->getName();

            properties->mEnum()->setEnumNames(spatialTransferFunctionProperty,
                                              availableSTFs);
            properties->mEnum()->setValue(spatialTransferFunctionProperty, index);

            this->actor->enableEmissionOfActorChangedSignal(true);
        }
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

        if (processRequest.contains("MULTIPLE_MEMBERS"))
        {
            if (multipleEnsembleMembersEnabled)
            {
                // Handle incoming grid aggregation.
                releaseAggregatedDataItems();
                gridAggregation = aggregationDataSource->getData(processRequest);
            }
        }
        else
        {
            // Release currently used data items.
            releaseDataItems();

            // If the ensemble mode was "multiple members" before but is not
            // anymore, the aggregated data grids have not yet been released.
            // Hence, if the current ensemble more is NOT "multiple members",
            // release them here.
            if (ensembleFilterOperation != "MULTIPLE_MEMBERS")
            {
                releaseAggregatedDataItems();
            }

            // Acquire the new ones.
            grid = dataSource->getData(processRequest);
            textureDataField  = grid->getTexture();
            textureLonLatLevAxes = grid->getLonLatLevTexture();

            if (useFlagsIfAvailable && grid->flagsEnabled())
            {
                textureDataFlags = grid->getFlagsTexture();
            }
            else
            {
                textureDataFlags = nullptr;
            }

            if (MLonLatHybridSigmaPressureGrid *hgrid =
                    dynamic_cast<MLonLatHybridSigmaPressureGrid*>(grid))
            {
                textureHybridCoefficients = hgrid->getHybridCoeffTexture();
                textureSurfacePressure = hgrid->getSurfacePressureGrid()->getTexture();
#ifdef ENABLE_HYBRID_PRESSURETEXCOORDTABLE
                texturePressureTexCoordTable = hgrid->getPressureTexCoordTexture2D();
#endif
            }

            if (MLonLatAuxiliaryPressureGrid *apgrid =
                    dynamic_cast<MLonLatAuxiliaryPressureGrid*>(grid))
            {
                textureAuxiliaryPressure =
                        apgrid->getAuxiliaryPressureFieldGrid()->getTexture();
            }

            if (MRegularLonLatStructuredPressureGrid *pgrid =
                    dynamic_cast<MRegularLonLatStructuredPressureGrid*>(grid))
            {
                texturePressureTexCoordTable = pgrid->getPressureTexCoordTexture1D();
            }

            asynchronousDataAvailableEvent(grid);
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

        if (MLonLatAuxiliaryPressureGrid *apgrid =
                dynamic_cast<MLonLatAuxiliaryPressureGrid*>(grid))
        {
            apgrid->getAuxiliaryPressureFieldGrid()->releaseTexture();
            textureAuxiliaryPressure = nullptr;
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

    // Remove data statistics analysis control if present.
    if (singleVariableAnalysisControl != nullptr)
    {
        delete singleVariableAnalysisControl;
        singleVariableAnalysisControl = nullptr;
    }
}


void MNWPActorVariable::releaseAggregatedDataItems()
{
    if (gridAggregation)
    {
        aggregationDataSource->releaseData(gridAggregation);
        gridAggregation = nullptr;
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


void MNWPActorVariable::updateTimeProperties()
{
    actor->enableActorUpdates(false);

    initTimeProperty->setEnabled(!synchronizeInitTime);
    validTimeProperty->setEnabled(!synchronizeValidTime);

    updateSyncPropertyColourHints();

    actor->enableActorUpdates(true);
}


void MNWPActorVariable::initEnsembleProperties()
{
    // Initially all ensemble members are selected to be used for ensemble
    // operations. Exception: loadConfiguration() has loaded a set of
    // selected members from the configuration file. In this case, test
    // whether all those members are actually available.
    if (selectedEnsembleMembers.empty())
    {
        selectedEnsembleMembers = dataSource->availableEnsembleMembers(
                    levelType, variableName);
    }
    else
    {
        selectedEnsembleMembers = selectedEnsembleMembers.intersect(
                    dataSource->availableEnsembleMembers(
                        levelType, variableName));

    }
    updateEnsembleSingleMemberProperty();
}


void MNWPActorVariable::updateEnsembleProperties()
{
    MQtProperties *properties = actor->getQtProperties();
    int mode = properties->mEnum()->value(ensembleModeProperty);

    actor->enableActorUpdates(false);

    // Ensemble properties are only enabled if not synchronized.
    ensembleModeProperty->setEnabled(!synchronizeEnsemble);

    switch ( mode )
    {
    case (0):
        // single ensemble member
        ensembleSingleMemberProperty->setEnabled(true);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "";
        break;
    case (1):
        // mean
        ensembleSingleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "MEAN";
        break;
    case (2):
        // stddev
        ensembleSingleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "STDDEV";
        break;
    case (3):
        // > threshold
        ensembleSingleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(true);
        ensembleFilterOperation = QString("P>%1").arg(
                    properties->mDouble()->value(ensembleThresholdProperty));
        break;
    case (4):
        // < threshold
        ensembleSingleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(true);
        ensembleFilterOperation = QString("P<%1").arg(
                    properties->mDouble()->value(ensembleThresholdProperty));
        break;
    case (5):
        // min
        ensembleSingleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "MIN";
        break;
    case (6):
        // max
        ensembleSingleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "MAX";
        break;
    case (7):
        // max-min
        ensembleSingleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "MAX-MIN";
        break;
    case (8):
        // multiple members
        ensembleSingleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
        ensembleFilterOperation = "MULTIPLE_MEMBERS";
    }

    // If the ensemble is synchronized, disable all properties (they are set
    // via the synchronization control).
    if (synchronizeEnsemble)
    {
        ensembleSingleMemberProperty->setEnabled(false);
        ensembleThresholdProperty->setEnabled(false);
    }

    actor->enableActorUpdates(true);
}


bool MNWPActorVariable::updateEnsembleSingleMemberProperty()
{
    MQtProperties *properties = actor->getQtProperties();

    // Remember currently set ensemble member in order to restore it below
    // (if getEnsembleMember() returns a value < 0 the list is currently
    // empty; however, since the ensemble members are represented by
    // unsigned ints below we cast this case to 0).
    int prevEnsembleMember = max(0, getEnsembleMember());

    if (ensembleMemberLoadedFromConfiguration > 0)
    {
        // If an ensemble member has been loaded from a config file, try
        // to restore this member.
        prevEnsembleMember = ensembleMemberLoadedFromConfiguration;
        ensembleMemberLoadedFromConfiguration = -1;
    }

    // Update ensembleMultiMemberProperty to display the currently selected
    // list of ensemble members.
    QString s = MDataRequestHelper::uintSetToString(selectedEnsembleMembers);
    properties->mString()->setValue(ensembleMultiMemberProperty, s);
    ensembleMultiMemberProperty->setToolTip(s);

    // Update ensembleSingleMemberProperty so that the user can choose only
    // from the list of selected members. (Requires first sorting the set of
    // members as a list, which can then be converted  to a string list).
    selectedEnsembleMembersAsSortedList = selectedEnsembleMembers.toList();
    qSort(selectedEnsembleMembersAsSortedList);

    QStringList selectedMembersAsStringList;
    foreach (unsigned int member, selectedEnsembleMembersAsSortedList)
        selectedMembersAsStringList << QString("%1").arg(member);

    actor->enableActorUpdates(false);
    properties->mEnum()->setEnumNames(
                ensembleSingleMemberProperty, selectedMembersAsStringList);
    setEnumPropertyClosest<unsigned int>(
                selectedEnsembleMembersAsSortedList,
                (unsigned int)prevEnsembleMember,
                ensembleSingleMemberProperty, synchronizeEnsemble);
    actor->enableActorUpdates(true);

    bool displayedMemberHasChanged = (getEnsembleMember() != prevEnsembleMember);
    return displayedMemberHasChanged;
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
        {
            if (tf->transferFunctionName() == tfName)
            {
                transferFunction = tf;
                return true;
            }
        }
    }

    return false;
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


template<typename T> bool MNWPActorVariable::setEnumPropertyClosest(
        const QList<T>& availableValues, const T& value,
        QtProperty *property, bool setSyncColour)
{
    // Find the value closest to "value" in the list of available values.
    int i = -1; // use of "++i" below
    bool exactMatch = false;
    while (i < availableValues.size()-1)
    {
        // Loop as long as "value" is larger that the currently inspected
        // element (use "++i" to have the same i available for the remaining
        // statements in this block).
        if (value > availableValues.at(++i)) continue;

        // We'll only get here if "value" <= availableValues.at(i). If we
        // have an exact match, break the loop. This is our value.
        if (availableValues.at(i) == value)
        {
            exactMatch = true;
            break;
        }

        // If "value" cannot be exactly matched it lies between indices i-1
        // and i in availableValues. Determine which is closer.
        if (i == 0) break; // if there's no i-1 we're done

//        LOG4CPLUS_DEBUG(mlog, value << " " << availableValues.at(i-1) << " "
//                        << availableValues.at(i) << " "
//                        << abs(value - availableValues.at(i-1)) << " "
//                        << abs(availableValues.at(i) - value) << " ");

        T v1;
        if (value < availableValues.at(i-1)) v1 = availableValues.at(i-1) - value;
        else v1 = value - availableValues.at(i-1);

        T v2;
        if (availableValues.at(i) < value) v2 = value - availableValues.at(i);
        else v2 = availableValues.at(i) - value;

        if ( v1 <= v2 ) i--;

        // "i" now contains the index of the closest available value.
        break;
    }

    if (i > -1)
    {
        // ( Also see updateSyncPropertyColourHints() ).

        // Update background colour of the property in the connected
        // scene's property browser: green if "value" is an
        // exact match with one of the available values, red otherwise.
        if (setSyncColour)
        {
            QColor colour = exactMatch ? QColor(0, 255, 0) : QColor(255, 0, 0);
            foreach (MSceneControl* scene, actor->getScenes())
                scene->setPropertyColour(property, colour);
        }

        // Get the currently selected index.
        int currentIndex = static_cast<QtEnumPropertyManager*> (
                    property->propertyManager())->value(property);

        if (i == currentIndex)
        {
            // Index i is already the current one. Nothing needs to be done.
            return false;
        }
        else
        {
            // Set the new value.
            static_cast<QtEnumPropertyManager*> (property->propertyManager())
                    ->setValue(property, i);
            // A new index was set. Return true.
            return true;
        }
    }

    return false;
}


void MNWPActorVariable::runStatisticalAnalysis(double significantDigits,
                                               int histogramDisplayMode)
{
    if (singleVariableAnalysisControl == nullptr)
    {
        // Create new analysis control. (Constructor of analysis control
        // sets analysis control of variable.)
        new MStructuredGridStatisticsAnalysisControl(this);
        singleVariableAnalysisControl->setMemoryManager(
                    grid->getMemoryManager());
        singleVariableAnalysisControl->setScheduler(dataSource->getScheduler());
    }
    MDataRequestHelper rh;
    rh.insert("HISTOGRAM_SIGNIFICANT_DIGITS",
              QString::number(significantDigits));
    rh.insert("HISTOGRAM_DISPLAYMODE", histogramDisplayMode);
    singleVariableAnalysisControl->run(rh.request());
}


bool MNWPActorVariable::changeVariable()
{
    // Open an MSelectDataSourceDialog and re-initialize the variable with
    // information returned from the dialog.
    MSelectDataSourceDialog dialog(actor->supportedLevelTypes());

    if (dialog.exec() == QDialog::Rejected)
    {
        return false;
    }

    MSelectableDataSource dsrc = dialog.getSelectedDataSource();

    if (dynamic_cast<MNWP2DVerticalActorVariable*>(this))
    {
        if (actor->getNWPVariables().size() > 1
                && dataSourceID != dsrc.dataSourceID)
        {
            QMessageBox::warning(
                        nullptr, actor->getName(),
                        "Vertical cross-section actors cannot handle"
                        " multiple variables coming from different data"
                        " sources.\n"
                        "(Varible was not changed.)");
            return false;
        }
    }

    LOG4CPLUS_DEBUG(mlog, "New variable has been selected: "
                    << dsrc.variableName.toStdString());

    dataSourceID = dsrc.dataSourceID;
    levelType    = dsrc.levelType;
    variableName = dsrc.variableName;

    releaseDataItems();
    releaseAggregatedDataItems();
    actor->enableActorUpdates(false);
    initialize();
    actor->enableActorUpdates(true);

    return true;
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
      imageUnitTargetGrid(-1),
      renderContourLabels(0)
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

    // Place render mode property at the top of all entries in the "rendering"
    // group.
    QList<QtProperty *> subPropertiesList =
            renderSettings.groupProperty->subProperties();
    foreach(QtProperty *subProperty, subPropertiesList)
    {
        renderSettings.groupProperty->removeSubProperty(subProperty);
    }
    renderSettings.renderMode = RenderMode::Disabled;
    QStringList renderModeNames;
    renderModeNames << "disabled" << "filled contours" << "pseudo colour"
                    << "line contours" << "filled and line contours"
                    << "pcolour and line contours";
    renderSettings.renderModeProperty = a->addProperty(
                ENUM_PROPERTY, "render mode", renderSettings.groupProperty);
    properties->mEnum()->setEnumNames(
                renderSettings.renderModeProperty, renderModeNames);
    foreach(QtProperty *subProperty, subPropertiesList)
    {
        renderSettings.groupProperty->addSubProperty(subProperty);
    }

    renderSettings.contourSetGroupProperty = a->addProperty(
                GROUP_PROPERTY, "contour sets",
                renderSettings.groupProperty);

    renderSettings.addContourSetProperty = a->addProperty(
                CLICK_PROPERTY, "add contour set",
                renderSettings.contourSetGroupProperty);

    renderSettings.contoursUseTF = false;
    renderSettings.contoursUseTFProperty = a->addProperty(
                BOOL_PROPERTY, "use transfer function",
                renderSettings.contourSetGroupProperty);
    properties->mBool()->setValue(renderSettings.contoursUseTFProperty,
                                  renderSettings.contoursUseTF);
    renderSettings.contoursUseTFProperty->setToolTip(
                "Use transfer function for all contour sets");

    a->endInitialiseQtProperties();

    addContourSet();
}


MNWP2DSectionActorVariable::~MNWP2DSectionActorVariable()
{
    if (imageUnitTargetGrid >= 0)
        actor->releaseImageUnit(imageUnitTargetGrid);
    if (textureUnitTargetGrid >= 0)
        actor->releaseTextureUnit(textureUnitTargetGrid);
}


MNWP2DSectionActorVariable::ContourSettings::ContourSettings(
        MActor *actor, const uint8_t index, bool enabled, double thickness,
        bool useTransferFunction, QColor colour,
        bool labelsEnabled, QString levelsString)
    : enabled(enabled),
      levels(QVector<double>()),
      thickness(thickness),
      useTF(useTransferFunction),
      colour(colour),
      labelsEnabled(labelsEnabled),
      startIndex(0),
      stopIndex(0)
{
    MQtProperties *properties = actor->getQtProperties();

    actor->beginInitialiseQtProperties();

    QString propertyTitle = QString("contour set #%1").arg(index  + 1);
    groupProperty = actor->addProperty(GROUP_PROPERTY, propertyTitle);

    enabledProperty = actor->addProperty(BOOL_PROPERTY, "enabled", groupProperty);
    properties->mBool()->setValue(enabledProperty, enabled);

    levelsProperty = actor->addProperty(STRING_PROPERTY, "levels", groupProperty);
    properties->mString()->setValue(levelsProperty, levelsString);

    thicknessProperty =
            actor->addProperty(DOUBLE_PROPERTY, "thickness", groupProperty);
    properties->setDouble(thicknessProperty, thickness, 0.1, 10.0, 2, 0.1);

    useTFProperty =
            actor->addProperty(BOOL_PROPERTY, "use transfer function",
                               groupProperty);
    properties->mBool()->setValue(useTFProperty, useTransferFunction);

    colourProperty = actor->addProperty(COLOR_PROPERTY, "colour", groupProperty);
    properties->mColor()->setValue(colourProperty, colour);

    // Contour labels are only implemented for horizontal cross-section actors.
    if (dynamic_cast<MNWPHorizontalSectionActor*>(actor))
    {
        labelsEnabledProperty = actor->addProperty(BOOL_PROPERTY, "labels",
                                                   groupProperty);
        properties->mBool()->setValue(labelsEnabledProperty, labelsEnabled);
    }

    removeProperty = actor->addProperty(CLICK_PROPERTY, "remove", groupProperty);

    actor->endInitialiseQtProperties();
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

    else if (property == renderSettings.renderModeProperty)
    {
        renderSettings.renderMode = static_cast<RenderMode::Type>(
                    properties->mEnum()->value(
                        renderSettings.renderModeProperty));
        return true;
    }

    else if (property == renderSettings.addContourSetProperty)
    {
        actor->enableEmissionOfActorChangedSignal(false);
        addContourSet();
        actor->enableEmissionOfActorChangedSignal(true);
        return false; // no redraw necessary
    }

    else if (property == renderSettings.contoursUseTFProperty)
    {
        renderSettings.contoursUseTF =
                properties->mBool()->value(renderSettings.contoursUseTFProperty);
        return true; // redraw necessary
    }

    else
    {
        ContourSettings *contourSet = nullptr;
        for (int index = 0; index < contourSetList.size(); index++)
        {
            contourSet = &contourSetList[index];

            if (property == contourSet->enabledProperty)
            {
                contourSet->enabled = properties->mBool()->value(
                            contourSet->enabledProperty);
                if ( contourSet->labelsEnabled && contourSet->enabled )
                {
                    renderContourLabels++;
                    updateContourLabels();
                }
                // Since labels are only visible if contour set and labels are
                // enabled, we need to decrement the indicator only if labels
                // are enabled.
                else if ( contourSet->labelsEnabled )
                {
                    renderContourLabels--;
                    if (renderContourLabels > 0)
                    {
                        updateContourLabels();
                    }
                }
                return true;
            }

            if (property == contourSet->removeProperty)
            {
                return removeContourSet(index);
            }

            else if (property == contourSet->levelsProperty)
            {
                QString cLevelStr = properties->mString()->value(
                            contourSet->levelsProperty);
                parseContourLevelString(cLevelStr, contourSet);

                if (actor->suppressActorUpdates()) return false;

                contourValuesUpdateEvent(contourSet);
                return true;
            }

            else if (property == contourSet->thicknessProperty)
            {
                contourSet->thickness = properties->mDouble()->value(
                            contourSet->thicknessProperty);
                return true;
            }

            else if (property == contourSet->useTFProperty)
            {
                contourSet->useTF = properties->mBool()->value(
                            contourSet->useTFProperty);
                return true;
            }

            else if (property == contourSet->colourProperty)
            {
                contourSet->colour = properties->mColor()->value(
                            contourSet->colourProperty);

                return true;
            }

            else if (property == contourSet->labelsEnabledProperty)
            {
                contourSet->labelsEnabled = properties->mBool()->value(
                            contourSet->labelsEnabledProperty);
                if ( contourSet->labelsEnabled && contourSet->enabled )
                {
                    renderContourLabels++;
                    updateContourLabels();
                }
                // Since labels are only visible if contour set and labels are
                // enabled, we need to decrement the indicator only if the
                // contour set is enabled.
                else if ( contourSet->enabled )
                {
                    renderContourLabels--;
                    if (renderContourLabels > 0)
                    {
                        updateContourLabels();
                    }
                }
                return true;
            }
        }
    }

    return false; // no redraw necessary
}


void MNWP2DSectionActorVariable::saveConfiguration(QSettings *settings)
{
    MNWPActorVariable::saveConfiguration(settings);

    MQtProperties *properties = actor->getQtProperties();

    settings->setValue("renderMode",
                       renderModeToString(renderSettings.renderMode));

    // Contour Sets.
    //================
    settings->beginWriteArray("contourSet", contourSetList.size());
    settings->setValue("useTransferFunction", renderSettings.contoursUseTF);
    int index = 0;
    foreach (ContourSettings contourSet, contourSetList)
    {
        settings->setArrayIndex(index);
        settings->setValue("enabled", contourSet.enabled);
        settings->setValue("levels", properties->mString()->value(
                               contourSet.levelsProperty));
        settings->setValue("thickness", contourSet.thickness);
        settings->setValue("useTF", contourSet.useTF);
        settings->setValue("colour", contourSet.colour);
        settings->setValue("labelsEnabled", contourSet.labelsEnabled);
        index++;
    }
    settings->endArray();
}


void MNWP2DSectionActorVariable::loadConfiguration(QSettings *settings)
{
    MNWPActorVariable::loadConfiguration(settings);

    MQtProperties *properties = actor->getQtProperties();

    QString renderModeName =
            settings->value("renderMode", "disabled").toString();
    RenderMode::Type renderMode = stringToRenderMode(renderModeName);

    // Print message if render mode name is no defined and set render mode to
    // disabled.
    if (renderMode == RenderMode::Invalid)
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(QString("Error reading configuration file: "
                               "Could not find render mode '%1'.\n"
                               "Setting render mode to 'disabled'.").arg(renderModeName));
        msgBox.exec();

        renderMode = stringToRenderMode(QString("disabled"));
    }

    properties->mEnum()->setValue(renderSettings.renderModeProperty,
                                  renderMode);

    // Contour Sets.
    //================
    foreach (ContourSettings contourSet, contourSetList)
    {
        renderSettings.contourSetGroupProperty->removeSubProperty(
                    contourSet.groupProperty);
    }
    contourSetList.clear();
    int numContourSet = settings->beginReadArray("contourSet");
    properties->mBool()->setValue(
                renderSettings.contoursUseTFProperty,
                settings->value("useTransferFunction", false).toBool());
    for (int i = 0; i < numContourSet; i++)
    {
        settings->setArrayIndex(i);
        bool enabled = settings->value("enabled", true).toBool();
        QString levels = settings->value("levels", "").toString();
        double thickness = settings->value("thickness", 1.5).toDouble();
        bool useTF = settings->value("useTF", false).toBool();
        QColor colour = settings->value("colour",
                                        QColor(0, 0, 0, 255)).value<QColor>();
        bool labelsEnabled = settings->value("labelsEnabled", false).toBool();
        addContourSet(enabled, thickness, useTF, colour, labelsEnabled, levels);
    }
    settings->endArray();

    // Read version id of config file.
    // ===============================

    QStringList configVersionID = readConfigVersionID(settings);

    // Thin and thick contour lines. (For compatibility with version < 1.2)
    //=====================================================================

    if (configVersionID[0].toInt() <= 1 && configVersionID[1].toInt() < 2)
    {
        QString levels =
                settings->value("thinContourLevels").toString();

        double thickness =
                settings->value("thinContourThickness", 1.2).toDouble();
        QColor colour = settings->value("thinContourColour",
                                        QColor(0, 0, 0, 255)).value<QColor>();

        addContourSet(true, thickness, false, colour, false, levels);


        levels = settings->value("thickContourLevels").toString();
        thickness = settings->value("thickContourThickness", 2.).toDouble();
        colour = settings->value("thickContourColour",
                                 QColor(0, 0, 0, 255)).value<QColor>();

        addContourSet(true, thickness, false, colour, false, levels);

        numContourSet = 2;
    }

    // If configuration file has no contour set setting defined, add one so
    // that at least one contour sets setting is available.
    if (numContourSet == 0)
    {
        addContourSet();
    }
}


QString MNWP2DSectionActorVariable::renderModeToString(
        RenderMode::Type renderMode)
{
    switch (renderMode)
    {
    case RenderMode::Disabled:
        return QString("disabled");
    case RenderMode::FilledContours:
        return QString("filled contours");
    case RenderMode::PseudoColour:
        return QString("pseudo colour");
    case RenderMode::LineContours:
        return QString("line contours");
    case RenderMode::FilledAndLineContours:
        return QString("filled and line contours");
    case RenderMode::PseudoColourAndLineContours:
        return QString("pcolour and line contours");
    case RenderMode::TexturedContours:
        return QString("textured contours");
    case RenderMode::FilledAndTexturedContours:
        return QString("filled and textured contours");
    case RenderMode::LineAndTexturedContours:
        return QString("line and textured contours");
    case RenderMode::PseudoColourAndTexturedContours:
        return QString("pcolour and textured contours");
    case RenderMode::FilledAndLineAndTexturedContours:
        return QString("filled, line and textured contours");
    case RenderMode::PseudoColourAndLineAndTexturedContours:
        return QString("pcolour and line and textured contours");
    default:
        return QString("");
    }
}


MNWP2DSectionActorVariable::RenderMode::Type
MNWP2DSectionActorVariable::stringToRenderMode(QString renderModeName)
{
    // NOTE: Render mode identification was changed in Met.3D version 1.1. For
    // compatibility with version 1.0, the old numeric identifiers are
    // considered here as well.
    if (renderModeName == QString("disabled")
            || renderModeName == QString("0")) // compatibility with Met.3D 1.0
    {
        return RenderMode::Disabled;
    }
    else if (renderModeName == QString("filled contours")
             || renderModeName == QString("1"))
    {
        return RenderMode::FilledContours;
    }
    else if (renderModeName == QString("pseudo colour")
             || renderModeName == QString("2"))
    {
        return RenderMode::PseudoColour;
    }
    else if (renderModeName == QString("line contours")
             || renderModeName == QString("3"))
    {
        return RenderMode::LineContours;
    }
    else if (renderModeName == QString("filled and line contours")
             || renderModeName == QString("4"))
    {
        return RenderMode::FilledAndLineContours;
    }
    else if (renderModeName == QString("pcolour and line contours")
             || renderModeName == QString("5"))
    {
        return RenderMode::PseudoColourAndLineContours;
    }
    else
    {
        return RenderMode::Invalid;
    }
}


void MNWP2DSectionActorVariable::addContourSet(
        const bool enabled, const double thickness, const bool useTF,
        const QColor colour, const bool labelsEnabled, QString levelString)
{
    ContourSettings contourSet(actor, contourSetList.size(), enabled,thickness,
                               useTF, colour, labelsEnabled, levelString);
    contourSetList.append(contourSet);
    renderSettings.contourSetGroupProperty->addSubProperty(
                contourSetList.back().groupProperty);
    parseContourLevelString(levelString, &(contourSetList.back()));

}



bool MNWP2DSectionActorVariable::removeContourSet(int index)
{
    // Avoid removing all contour sets.
    if (contourSetList.size() > 1)
    {
        // Don't ask for confirmation during application start.
        if (MSystemManagerAndControl::getInstance()->applicationIsInitialized())
        {
            QMessageBox yesNoBox;
            yesNoBox.setWindowTitle("Delete contour set");
            yesNoBox.setText(QString("Do you really want to delete "
                                     "contour set #%1").arg(index + 1));
            yesNoBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            yesNoBox.setDefaultButton(QMessageBox::No);

            if (yesNoBox.exec() != QMessageBox::Yes)
            {
                return false;
            }
        }

        ContourSettings *contourSet = &contourSetList[index];
        // Redraw is only needed if contour sets to delete are enabled.
        bool needsRedraw = contourSet->enabled;
        MQtProperties *properties = actor->getQtProperties();
        properties->mBool()->setValue(contourSet->enabledProperty,
                                      false);
        renderSettings.contourSetGroupProperty->removeSubProperty(
                    contourSet->groupProperty);
        contourSetList.remove(index);

        QString text = contourSet->groupProperty->propertyName();
        text = text.mid(0, text.indexOf("#") + 1);
        // Rename contour sets after deleted contour sets to
        // close gap left by deleted contour sets.
        for (; index < contourSetList.size(); index++)
        {
            contourSetList[index].groupProperty->setPropertyName(
                        text + QString::number(index + 1));
        }
        return needsRedraw;
    }
    else
    {
        // Display error message if user wants to delete last contour set.
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("At least one contour set needs to be present.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return false;
    }
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
        QString cLevelStr, ContourSettings *contours)
{
    QVector<double> *contourSet = &(contours->levels);
    contours->startIndex = 0;
    contours->stopIndex = 0;
    // Clear the current list of contour sets; if cLevelStr does not match
    // any accepted format no contours are drawn.
    contourSet->clear();

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
            for (double d = from; d <= to; d += step) *contourSet << d;
        else if (step < 0)
            for (double d = from; d >= to; d += step) *contourSet << d;

        contours->stopIndex = contourSet->size();
        return true;
    }
    else if (rxList.indexIn(cLevelStr) == 0)
    {
        QStringList listValues = cLevelStr.split(",");

        bool ok;
        for (int i = 0; i < listValues.size(); i++)
            *contourSet << listValues.value(i).toDouble(&ok);

        contours->stopIndex = contourSet->size();
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
      spatialTransferFunction(nullptr),
      textureUnitSpatialTransferFunction(-1),
      llcrnrlon(0.),
      llcrnrlat(0.),
      urcrnrlon(0.),
      urcrnrlat(0.),
      shiftForWesternLon(0.f),
      contourLabelSuffix("")
{
    assert(actor != nullptr);
    MNWPMultiVarActor *a = actor;
    MQtProperties *properties = actor->getQtProperties();

    a->beginInitialiseQtProperties();

    QtProperty* renderGroup = getPropertyGroup("rendering");
    assert(renderGroup != nullptr);
    // Remove properties to place spatial transfer function selection propery
    // above them.
    renderGroup->removeSubProperty(renderSettings.contourSetGroupProperty);

    QStringList renderModeNames =
            properties->getEnumItems(renderSettings.renderModeProperty);
    renderModeNames << "textured contours"
                    << "filled and textured contours"
                    << "line and textured contours"
                    << "pcolour and textured contours"
                    << "filled, line and textured contours"
                    << "pcolour and line and textured contours";
    properties->mEnum()->setEnumNames(renderSettings.renderModeProperty,
                                      renderModeNames);

    // Scan currently available actors for spatial transfer functions. Add STFs
    // to the list displayed in the combo box of the
    // spatialTransferFunctionProperty.
    QStringList availableSTFs;
    availableSTFs << "None";
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    foreach (MActor *mactor, glRM->getActors())
    {
        if (MSpatial1DTransferFunction *stf =
                dynamic_cast<MSpatial1DTransferFunction*>(mactor))
        {
            availableSTFs << stf->transferFunctionName();
        }
    }

    spatialTransferFunctionProperty = a->addProperty(ENUM_PROPERTY,
                                                     "textured transfer function",
                                                     renderGroup);
    properties->mEnum()->setEnumNames(spatialTransferFunctionProperty,
                                      availableSTFs);

    // Re-add properties after spatial transfer function selection property.
    renderGroup->addSubProperty(renderSettings.contourSetGroupProperty);

    contourLabelSuffixProperty = a->addProperty(
                STRING_PROPERTY, "contour label suffix",
                renderGroup);

    a->endInitialiseQtProperties();
}


MNWP2DHorizontalActorVariable::~MNWP2DHorizontalActorVariable()
{
    if (textureUnitSpatialTransferFunction >=0)
        actor->releaseTextureUnit(textureUnitSpatialTransferFunction);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MNWP2DHorizontalActorVariable::initialize()
{
    MNWP2DSectionActorVariable::initialize();

    if (textureUnitSpatialTransferFunction >=0)
        actor->releaseTextureUnit(textureUnitSpatialTransferFunction);

    textureUnitSpatialTransferFunction = actor->assignTextureUnit();

    setSpatialTransferFunctionFromProperty();
}


void MNWP2DHorizontalActorVariable::saveConfiguration(QSettings *settings)
{
    MNWP2DSectionActorVariable::saveConfiguration(settings);

    MQtProperties *properties = actor->getQtProperties();

    // Save rendering properties.
    settings->setValue("spatialTransferFunction",
                       properties->getEnumItem(spatialTransferFunctionProperty));

    settings->setValue("contourLabelSuffix", contourLabelSuffix);
}


void MNWP2DHorizontalActorVariable::loadConfiguration(QSettings *settings)
{
    MNWP2DSectionActorVariable::loadConfiguration(settings);

    MQtProperties *properties = actor->getQtProperties();

    // Load rendering properties.
    // ==========================
    QString stfName = settings->value("spatialTransferFunction", "None").toString();
    while (!setSpatialTransferFunction(stfName))
    {
        if (!MTransferFunction::loadMissingTransferFunction(
                    stfName, MSpatial1DTransferFunction::staticActorType(),
                    "Variable ", variableName, settings))
        {
            break;
        }
    }

    contourLabelSuffix = settings->value("contourLabelSuffix").toString();
    properties->mString()->setValue(contourLabelSuffixProperty,
                                    contourLabelSuffix);
}


bool MNWP2DHorizontalActorVariable::onQtPropertyChanged(QtProperty *property)
{
    if (MNWP2DSectionActorVariable::onQtPropertyChanged(property)) return true;

    MQtProperties *properties = actor->getQtProperties();

    if (property == contourLabelSuffixProperty)
    {
        contourLabelSuffix = properties->mString()->value(
                    contourLabelSuffixProperty);

        updateContourLabels();

        return true;
    }

    else if (property == spatialTransferFunctionProperty)
    {
        return setSpatialTransferFunctionFromProperty();
    }

    return false;
}


void MNWP2DHorizontalActorVariable::computeRenderRegionParameters(
        double llcrnrlon, double llcrnrlat,
        double urcrnrlon, double urcrnrlat)
{
    // Copy bbox coordinates to member variables.
    this->llcrnrlon = llcrnrlon;
    this->llcrnrlat = llcrnrlat;
    this->urcrnrlon = urcrnrlon;
    this->urcrnrlat = urcrnrlat;

    bool gridIsCyclic = grid->gridIsCyclicInLongitude();

    // 1A) Find longitudinal index "i0" in data grid that corresponds to
    //     the WESTmost rendered grid point.
    // ================================================================

    // Longitudes stored in ascending order.
    double shiftLon = grid->lons[0];
    float westBBoxBorderRelativeToWestDataBorder = llcrnrlon - shiftLon;
    float eastBBoxBorderRelativeToWestDataBorder = urcrnrlon - shiftLon;

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
        if (MMOD(grid->lons[i0] - shiftLon, 360.) + M_LONLAT_RESOLUTION
                >= MMOD(westBBoxBorderRelativeToWestDataBorder, 360.)) break;
    }
    // Handle overshooting for non-cyclic grids (otherwise i0 = grid->nlons
    // if the bounding box is east of the grid domain).
    if (!gridIsCyclic) i0 = min(i0, grid->nlons-1);

    // 1B) Find longitudinal index "i1" in data grid that corresponds to
    //     the EASTmost rendered grid point.
    // ================================================================

    // Find the last lon index smaller than urcrnrlon.
    unsigned int i1;
    for (i1 = grid->nlons - 1; i1 > 0; i1--)
    {
        if (MMOD(grid->lons[i1] - shiftLon, 360.)
                <= MMOD(eastBBoxBorderRelativeToWestDataBorder, 360.)) break;
    }

    // 2) Find lat indices "j0"/"j1" that corresponds to SOUTHmost/NORTHmost
    //    grid points.
    // =====================================================================

    // Latitude is never cyclic, hence no modulo is required here.
    for (j0 = 0; j0 < grid->nlats; j0++)
    {
        if (grid->lats[j0] <= urcrnrlat) break;
    }
    int j1;
    for (j1 = grid->nlats - 1; j1 > 0; j1--)
    {
        if (grid->lats[j1] >= llcrnrlat) break;
    }

    // 3) Determine the number "nlons" of longitudes to be rendered. Special
    // care has to be taken as data grid may need to be rendered multiple
    // times or can fall apart into disjoint regions, depending on bbox.
    // =====================================================================

    // Possible cases:
    //  a) data region is completely contained in bbox
    //     --> nlons = num. grid points of data grid
    //  b) bbox cuts out part of data region
    //     --> nlons = num. grid points in displayed region
    //  c) bbox is large enough in lon so that data region falls apart into
    //     disjoint regions
    //     --> nlons = num. grid points in western region + num. grid points
    //         in eastern region
    //  d) bbox is >360deg in lon and (parts) of the data region are repeated
    //     --> nlons = sum of grid points in all regions

    // Mapped to longitudes in the range [0..360), eastern bbox border is
    // east of western bbox border...
    if (MMOD(westBBoxBorderRelativeToWestDataBorder, 360.)
            <= MMOD(eastBBoxBorderRelativeToWestDataBorder, 360.))
    {
        // One region, no disjoint parts.
        nlons = i1 - i0 + 1;
    }
    // ... or eastern bbox border is WEST of western bbox border.
    else
    {
        // Treat west and east subregions separately. Eastern subregion goes
        // from i0 to nlons (nlons = grid->nlons - i0), western subregion from
        // 0 to i1 (nlons = i1 + 1).
        nlons = i1 + grid->nlons - i0 + 1;
    }

    // Since the modulo operation maps to a domain of width 360, it is necessary
    // to add the missing width if the domain is larger than 360.
    nlons += floor((urcrnrlon - llcrnrlon) / 360.) * grid->nlons;

    // Compute initial shift to place the grid at the correct position during
    // rendering. It contains a multiple of 360.
    shiftForWesternLon = float(floor((llcrnrlon - grid->lons[0]) / 360.) * 360.f);


    // Bianca's explanation for the above equations (sorry, only in German...):
    // ------------------------------------------------------------------------
    // Dies funktioniert auch, falls die westlichste und oestlichste Teilregionen
    // fuer sich genommen jeweils nur ein Ausschnitt der Gitterregion sind
    // (eine Teilregion ergibt sich hierbei aus der Begrenzung durch die
    // Boundingbox und einer Aufteilung des gesamten Raums in disjunkte
    // Teilregionen mit einer Breite von 360deg und einer westlichen Grenze
    // entsprechend der westlichen Grenze plus ein (positives/negatives)
    // Vielfaches von 360deg). Grundsaetzlich laesst berechnet sich nlons in diesem
    // Fall nach folgender Formel:
    //    nlonsWestPart + nlonsEastPart + nlonsGrid * numSubregions
    //  mit (nlonsWestPart = grid->nlons - i0) und (nlonsEastPart = i1 + 1) folgt:
    //   (i1 + 1 + grid->nlons - i0) + (grid->nlons * numSubregions)
    // Wobei numSubregions die Anzahl der disjunkten Teilregionen angibt, die
    // vollstaendig in der Boundingbox enthalten sind.
    // Wird die oestliche Grenze westlich der westlichen Grenze abgebildet, so
    // gilt:
    //   (i1 < i0) -> (i1 - i0 <= -1) -> (i1 - i0 + grid->nlons) < grid->nlons
    // Da wiederum gilt, dass in einer Boundingboxregion von 360deg Breite, die
    // Gitterregion ein Mal enthalten ist, muss der Abstand zwischen der
    // westlichen und oestlichen Boundingboxgrenze (abzueglich 360deg fuer jede
    // vollstaendig enthaltenen Teilregion) in diesem Fall kleiner als 360deg sein.
    // Wiederrum gilt:
    //   (i1 >= i0) -> (i1 - i0 >= 0) -> (i1 - i0 + grid->nlons) >= grid->nlons
    // Der Umkehrschluss zeigt, dass, wenn die oestliche Boundingboxgrenze
    // oestlich der westlichen abgebildet wird, der Abstand zwischen den
    // Boundingboxgrenzen (abzueglich der vollstaendig enthaltenen Teilregion)
    // groesser als 360deg sein muss (ausgenommen Boundingbox mit einer
    // Gesammtbreite unter 360deg). Da die Boundingbox abzueglich der Teilregionen
    // groesser als 360deg ist, ergibt sich:
    //    factor = floor((urcrnrlon - llcrnrlon) / 360.) = numSubregions + 1
    // Ausserdem laesst sich die Formel fuer nlons folgendermassen umstellen:
    //    (i1 - i0 + 1) + nlonsGrid * (numSubregions + 1)
    // Da wir factor fuer die Abschaetzung von numSubregions nutzen, koennen wir
    // also fuer den Fall, dass die oestliche Boundingboxgrenze oestlich der
    // westlichen Boundingboxgrenze abgebildet wird, stets (i1 - i0 + 1) zur
    // Berechnung von nlons in einem ersten Schritt nutzen.
    // Fuer Boundingboxbreiten unter 360deg koennen nur dann seperate Teilregionen
    // entstehen, wenn die oestliche Boundingboxgrenze westlich der westlichen
    // Boundingboxgrenze abgebildet wird.

    // 4) Determine the number "nlats" of latitudes to be rendered.
    // ============================================================

    nlats = j1 - j0 + 1;
    if (nlats < 0) nlats = 0;


    // DEBUG output.
    // =============

    LOG4CPLUS_DEBUG(mlog, "(grid is " << (gridIsCyclic ? "" : "not")
                    << " cyclic; shiftLon = " << shiftLon << ") "
                    << "BBox = (" << llcrnrlon << "/" << llcrnrlat << " -> "
                    << urcrnrlon << "/" << urcrnrlat << "); "
                    << "i = (" << i0 << "--" << i1 << "); j = (" << j0 << "--"
                    << j1 << "); n = (" << nlons << "/" << nlats << ")");
}


void MNWP2DHorizontalActorVariable::updateContourIndicesFromTargetGrid(
        float slicePosition_hPa, ContourSettings *contourSet)
{
    // Download generated grid from GPU via imageStore() in vertex shader
    // and glGetTexImage() here.
    textureTargetGrid->bindToLastTextureUnit();
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT,
                  targetGrid2D->data); CHECK_GL_ERROR;

    // Set the current isovalue to the target grid's vertical coordinate.
    targetGrid2D->levels[0] = slicePosition_hPa;

    // Number of longitudinal grid points required for rendering.
    int numRequiredDataLons = nlons - 1;
    // Use the sub grid but don't overshoot the grid's nlons (nlons can contain
    // a number larger than grid->nlons). maskRectangularRegion() does not
    // mark the outside region correctly if the provided nlons is larger than
    // the grid's nlons.
    numRequiredDataLons = min(numRequiredDataLons, int(grid->nlons - 1));

    // Set the target grid points outside the render domain to
    // MISSING_VALUE, so that min() and max() work correctly.
    targetGrid2D->maskRectangularRegion(
                i0, j0, 0, numRequiredDataLons, nlats-1, 1);

    float tgmin = targetGrid2D->min();
    float tgmax = targetGrid2D->max();

    // Update start and stop indices of contour sets.

    QVector<ContourSettings*> localContourSetList;
    localContourSetList.clear();

    // No contour sets given results in updating all levels available.
    if (contourSet == nullptr)
    {
        for (int i = 0; i < contourSetList.size(); i++)
        {
            localContourSetList.append(&contourSetList[i]);
        }
    }
    else
    {
        localContourSetList.append(contourSet);
    }

    foreach (ContourSettings *contourSet, localContourSetList)
    {
        contourSet->startIndex = 0;
        contourSet->stopIndex  = contourSet->levels.size();

        int i = 0;
        if (!contourSet->levels.isEmpty()
                && tgmin > contourSet->levels.last())
        {
            contourSet->startIndex = contourSet->levels.size();
        }
        else
        {
            for (; i < contourSet->levels.size(); i++)
            {
                if (contourSet->levels[i] >= tgmin)
                {
                    contourSet->startIndex = i;
                    break;
                }
            }
            for (; i < contourSet->levels.size(); i++)
            {
                if (contourSet->levels[i] > tgmax)
                {
                    contourSet->stopIndex = i;
                    break;
                }
            }
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


MNWP2DSectionActorVariable::RenderMode::Type
MNWP2DHorizontalActorVariable::stringToRenderMode(QString renderModeName)
{
    RenderMode::Type renderMode;

    renderMode = MNWP2DSectionActorVariable::stringToRenderMode(renderModeName);

    // Check if string matched 2D section render mode.
    if (renderMode != RenderMode::Invalid)
    {
        return renderMode;
    }

    // Check if string matches horizontal section render mode and return invalid
    // if not.
    if (renderModeName == QString("textured contours"))
    {
        return RenderMode::TexturedContours;
    }
    else if (renderModeName == QString("filled and textured contours"))
    {
        return RenderMode::FilledAndTexturedContours;
    }
    else if (renderModeName == QString("line and textured contours"))
    {
        return RenderMode::LineAndTexturedContours;
    }
    else if (renderModeName == QString("pcolour and textured contours"))
    {
        return RenderMode::PseudoColourAndTexturedContours;
    }
    else if (renderModeName == QString("filled, line and textured contours"))
    {
        return RenderMode::FilledAndLineAndTexturedContours;
    }
    else if (renderModeName == QString("pcolour and line and textured contours"))
    {
        return RenderMode::PseudoColourAndLineAndTexturedContours;
    }
    else
    {
        return RenderMode::Invalid;
    }
}


bool MNWP2DHorizontalActorVariable::setSpatialTransferFunction(QString stfName)
{
    MQtProperties *properties = actor->getQtProperties();
    QStringList stfNames = properties->mEnum()->enumNames(
                spatialTransferFunctionProperty);
    int stfIndex = stfNames.indexOf(stfName);

    if (stfIndex >= 0)
    {
        properties->mEnum()->setValue(spatialTransferFunctionProperty, stfIndex);
        return true;
    }

    // Set transfer function property to "None".
    properties->mEnum()->setValue(spatialTransferFunctionProperty, 0);

    return false; // the given tf name could not be found
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


void MNWP2DHorizontalActorVariable::contourValuesUpdateEvent(
        ContourSettings *levels)
{
    updateContourIndicesFromTargetGrid(targetGrid2D->levels[0], levels);
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
    int contourSetIndex = 0;

    foreach (ContourSettings contourSet, contourSetList)
    {
        // Skip labels of contour sets, which are not enabled or which
        // labels are not enabled.
        if (!(contourSet.enabled && contourSet.labelsEnabled))
        {
            contourSetIndex++;
            continue;
        }
        // Step along all borders and search for potential iso-contours.
        for (int i = contourSet.startIndex; i < contourSet.stopIndex; ++i)
        {
            float isoValue = contourSet.levels.at(i);

            // Traverse grid downwards.
            for (int j = minY + 1; j <= maxY; j += step)
            {
                checkGridForContourLabel(targetGrid2D, j, minX, 1, 0, isoValue,
                                         contourSetIndex);
                checkGridForContourLabel(targetGrid2D, j, maxX, 1, 0, isoValue,
                                         contourSetIndex);
            }

            // Traverse grid rightwards.
            for (int j = minX + 1; j <= maxX; j += step)
            {
                checkGridForContourLabel(targetGrid2D, minY, j, 0, 1, isoValue,
                                         contourSetIndex);
                checkGridForContourLabel(targetGrid2D, maxY, j, 0, 1, isoValue,
                                         contourSetIndex);
            }
        }
        contourSetIndex++;
    }
}


void MNWP2DHorizontalActorVariable::checkGridForContourLabel(
        const MRegularLonLatGrid* grid,
        const int lat, const int lon,
        const int deltaLat, const int deltaLon,
        const float isoValue, const int index)
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

    addNewContourLabel(posPrev, posNext, valuePrev, valueNext, isoValue, index);
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
        const float isoPrev, const float isoNext, const float isoValue,
        const int index)

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
                    16, contourSetList[index].colour,
                    MTextManager::BASELINECENTRE, true,
                    QColor(255, 255, 255, 200), 0.3)
                );
}


/******************************************************************************
***                            PRIVATE METHODS                              ***
*******************************************************************************/

bool MNWP2DHorizontalActorVariable::setSpatialTransferFunctionFromProperty()
{
    MQtProperties *properties = actor->getQtProperties();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    QString stfName = properties->getEnumItem(spatialTransferFunctionProperty);

    if (stfName == "None")
    {
        spatialTransferFunction = nullptr;

        // Update enum items: Scan currently available actors for transfer
        // functions. Add TFs to the list displayed in the combo box of the
        // transferFunctionProperty.
        QStringList availableSTFs;
        availableSTFs << "None";
        foreach (MActor *mactor, glRM->getActors())
        {
            if (MSpatial1DTransferFunction *stf =
                    dynamic_cast<MSpatial1DTransferFunction*>(mactor))
            {
                availableSTFs << stf->transferFunctionName();
            }
        }
        properties->mEnum()->setEnumNames(spatialTransferFunctionProperty,
                                          availableSTFs);

        return true;
    }

    // Find the selected transfer function in the list of actors from the
    // resources manager. Not very efficient, but works well enough for the
    // small number of actors at the moment..
    foreach (MActor *mactor, glRM->getActors())
    {
        if (MSpatial1DTransferFunction *stf =
                dynamic_cast<MSpatial1DTransferFunction*>(mactor))
            if (stf->transferFunctionName() == stfName)
            {
                spatialTransferFunction = stf;
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
    else  if (MLonLatAuxiliaryPressureGrid *apgrid =
              dynamic_cast<MLonLatAuxiliaryPressureGrid*>(grid))
    {
        // Min and max surface pressure.
        double pfield_hPa_min = apgrid->getAuxiliaryPressureFieldGrid()->min() / 100.;
        double pfield_hPa_max = apgrid->getAuxiliaryPressureFieldGrid()->max() / 100.;

        gridVerticalLevelStart = 0;
        gridVerticalLevelCount = grid->nlevs;

        LOG4CPLUS_TRACE(mlog, "\tVariable: " << variableName.toStdString()
                        << ": auxPressureField_min = " << pfield_hPa_min
                        << " hPa, auxPressureField_max = " << pfield_hPa_max
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


void MNWP2DVerticalActorVariable::contourValuesUpdateEvent(
        ContourSettings *levels)
{
    // Update start and stop indices of contour sets.

    QVector<ContourSettings*> localContourSetList;
    localContourSetList.clear();

    // If no contour sets are given, update all levels available.
    if (levels == nullptr)
    {
        for (int i = 0; i < contourSetList.size(); i++)
        {
            localContourSetList.append(&contourSetList[i]);
        }
    }
    else
    {
        localContourSetList.append(levels);
    }

    foreach (ContourSettings *contourSet, localContourSetList)
    {
        contourSet->startIndex = 0;
        contourSet->stopIndex  = contourSet->levels.size();
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


bool MNWP3DVolumeActorVariable::setTransferFunctionFromProperty()
{
    // Since the shadow of volume raycaster actor depends on the transfer
    // function, it is necessary to trigger an update if the transfer function
    // changes otherwise the shadow won't adapt to the changes.
    if (transferFunction != nullptr)
    {
        disconnect(transferFunction, SIGNAL(actorChanged()),
                   static_cast<MNWPVolumeRaycasterActor*>(actor),
                   SLOT(updateShadow()));
    }

    bool returnValue = MNWPActorVariable::setTransferFunctionFromProperty();

    if (returnValue == true && transferFunction != nullptr)
    {
        connect(transferFunction, SIGNAL(actorChanged()),
                static_cast<MNWPVolumeRaycasterActor*>(actor),
                SLOT(updateShadow()));
    }

    return returnValue;
}


} // namespace Met3D
