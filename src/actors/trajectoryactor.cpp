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
#include "trajectoryactor.h"

// standard library imports
#include <iostream>
#include "math.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/selectdatasourcedialog.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryActor::MTrajectoryActor()
    : MActor(),
      trajectorySource(nullptr),
      trajectories(nullptr),
      trajectoriesVertexBuffer(nullptr),
      normalsSource(nullptr),
      trajectoryFilter(nullptr),
      trajectorySelection(nullptr),
      trajectorySingleTimeSelection(nullptr),
      dataSourceID(""),
      suppressUpdate(false),
      normalsToBeComputed(true),
      renderMode(TRAJECTORY_TUBES),
      syncWithValidTime(true),
      synchronizationControl(nullptr),
      bbox(QRectF(-90., 0., 180., 90.)),
      transferFunction(nullptr),
      textureUnitTransferFunction(-1),
      tubeRadius(0.1),
      sphereRadius(0.2),
      shadowEnabled(true),
      shadowColoured(false)
{
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Trajectories");

    // Remove labels property group since it is not used for trajectory actors
    // yet.
    actorPropertiesSupGroup->removeSubProperty(labelPropertiesSupGroup);

    // Data source selection.
    selectDataSourceProperty = addProperty(CLICK_PROPERTY, "select data source",
                                           actorPropertiesSupGroup);
    utilizedDataSourceProperty = addProperty(STRING_PROPERTY,
                                             "data source",
                                             actorPropertiesSupGroup);
    utilizedDataSourceProperty->setEnabled(false);

    // Render mode.
    QStringList renderModeNames;
    renderModeNames << "tubes"
                    << "all positions"
                    << "positions"
                    << "positions and tubes"
                    << "positions and backward tubes";
    renderModeProperty = addProperty(ENUM_PROPERTY, "render mode",
                                     actorPropertiesSupGroup);
    properties->mEnum()->setEnumNames(renderModeProperty, renderModeNames);
    properties->mEnum()->setValue(renderModeProperty, renderMode);

    // Init, start, valid time.
    QStringList timeSyncModeNames;
    timeSyncModeNames << "with valid time"
                      << "with trajectory time";
    timeSyncModeProperty = addProperty(ENUM_PROPERTY, "time sync",
                                       actorPropertiesSupGroup);
    properties->mEnum()->setEnumNames(timeSyncModeProperty, timeSyncModeNames);
    properties->mEnum()->setValue(timeSyncModeProperty, 0);

    initTimeProperty = addProperty(ENUM_PROPERTY, "initialisation",
                                   actorPropertiesSupGroup);

    validTimeProperty = addProperty(ENUM_PROPERTY, "valid",
                                    actorPropertiesSupGroup);

    trajectoryTimeProperty = addProperty(ENUM_PROPERTY, "trajectory",
                                         actorPropertiesSupGroup);

    // Ensemble.
    QStringList ensembleModeNames;
    ensembleModeNames << "member" << "all";
    ensembleModeProperty = addProperty(ENUM_PROPERTY, "ensemble mode",
                                       actorPropertiesSupGroup);
    properties->mEnum()->setEnumNames(ensembleModeProperty, ensembleModeNames);

    ensembleMemberProperty = addProperty(INT_PROPERTY, "ensemble member",
                                         actorPropertiesSupGroup);
    properties->setInt(ensembleMemberProperty, 0, 0, 50, 1);

    // Trajectory filtering.
    enableFilterProperty = addProperty(BOOL_PROPERTY, "filter trajectories",
                                       actorPropertiesSupGroup);
    properties->mBool()->setValue(enableFilterProperty, true);

    deltaPressureProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                        "pressure difference",
                                        actorPropertiesSupGroup);
    properties->setDDouble(deltaPressureProperty, 500., 1., 1050., 2, 5., " hPa");

    deltaTimeProperty = addProperty(DECORATEDDOUBLE_PROPERTY, "time interval",
                                    actorPropertiesSupGroup);
    properties->setDDouble(deltaTimeProperty, 48, 1, 48, 0, 1, " hrs");

    bboxProperty = addProperty(RECTF_LONLAT_PROPERTY, "bounding box",
                               actorPropertiesSupGroup);
    properties->setRectF(bboxProperty, bbox, 2);

    // Transfer function.
    // Scan currently available actors for transfer functions. Add TFs to
    // the list displayed in the combo box of the transferFunctionProperty.
    QStringList availableTFs;
    availableTFs << "None";
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    foreach (MActor *mactor, glRM->getActors())
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(mactor))
        {
            availableTFs << tf->transferFunctionName();
        }
    }
    transferFunctionProperty = addProperty(ENUM_PROPERTY, "transfer function",
                                           actorPropertiesSupGroup);
    properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);

    // Render mode and parameters.
    tubeRadiusProperty = addProperty(DECORATEDDOUBLE_PROPERTY, "tube radius",
                                     actorPropertiesSupGroup);
    properties->setDDouble(tubeRadiusProperty, tubeRadius,
                           0.01, 1., 2, 0.1, " (world space)");

    sphereRadiusProperty = addProperty(DECORATEDDOUBLE_PROPERTY, "sphere radius",
                                       actorPropertiesSupGroup);
    properties->setDDouble(sphereRadiusProperty, sphereRadius,
                           0.01, 1., 2, 0.1, " (world space)");

    enableShadowProperty = addProperty(BOOL_PROPERTY, "render shadows",
                                       actorPropertiesSupGroup);
    properties->mBool()->setValue(enableShadowProperty, shadowEnabled);

    colourShadowProperty = addProperty(BOOL_PROPERTY, "colour shadows",
                                       actorPropertiesSupGroup);
    properties->mBool()->setValue(colourShadowProperty, shadowColoured);

    // Observe the creation/deletion of other actors -- if these are transfer
    // functions, add to the list displayed in the transfer function property.
    connect(glRM, SIGNAL(actorCreated(MActor*)), SLOT(onActorCreated(MActor*)));
    connect(glRM, SIGNAL(actorDeleted(MActor*)), SLOT(onActorDeleted(MActor*)));
    connect(glRM, SIGNAL(actorRenamed(MActor*, QString)),
            SLOT(onActorRenamed(MActor*, QString)));

    endInitialiseQtProperties();
}


MTrajectoryActor::~MTrajectoryActor()
{
    if (textureUnitTransferFunction >=0)
        releaseTextureUnit(textureUnitTransferFunction);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE 0
#define SHADER_NORMAL_ATTRIBUTE 1

void MTrajectoryActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);

    beginCompileShaders(4);

    compileShadersFromFileWithProgressDialog(
                tubeShader,
                "src/glsl/trajectory_tubes.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                tubeShadowShader,
                "src/glsl/trajectory_tubes_shadow.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                positionSphereShader,
                "src/glsl/trajectory_positions.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                positionSphereShadowShader,
                "src/glsl/trajectory_positions_shadow.fx.glsl");

    endCompileShaders();
}


void MTrajectoryActor::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MTrajectoryActor::getSettingsID());

    settings->setValue("dataSourceID", dataSourceID);

    settings->setValue("renderMode", static_cast<int>(renderMode));

    settings->setValue("enableFilter",
                       properties->mBool()->value(enableFilterProperty));

    settings->setValue("deltaPressure",
                       properties->mDDouble()->value(deltaPressureProperty));
    settings->setValue("deltaTime",
                       properties->mDDouble()->value(deltaTimeProperty));

    settings->setValue("boundingBox", bbox);

    settings->setValue("transferFunction",
                       properties->getEnumItem(transferFunctionProperty));

    settings->setValue("tubeRadius", tubeRadius);
    settings->setValue("sphereRadius", sphereRadius);
    settings->setValue("shadowEnabled", shadowEnabled);
    settings->setValue("shadowColoured", shadowColoured);

    settings->endGroup();
}


void MTrajectoryActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MTrajectoryActor::getSettingsID());

    QString dataSourceID = settings->value("dataSourceID", "").toString();


    properties->mString()->setValue(utilizedDataSourceProperty,
                                    dataSourceID);

    releaseData();
    enableProperties(true);

    this->setDataSource(dataSourceID + QString(" Reader"));
    this->setNormalsSource(dataSourceID + QString(" Normals"));
    this->setTrajectoryFilter(dataSourceID + QString(" timestepFilter"));

    bool dataSourceAvailable =
            MSelectDataSourceDialog::checkForTrajectoryDataSource(dataSourceID);

    // The configuration file specifies a data source which does not exist.
    // Display warning and load rest of the configuration.
    if (!dataSourceAvailable)
    {

        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(QString("Trajectory actor '%1':\n"
                               "Data source '%2' does not exist.\n"
                               "Select a new one.")
                       .arg(getName()).arg(dataSourceID));
        msgBox.exec();
        if (isInitialized())
        {
            if (!selectDataSource())
            {
                // User has selected no data source. Display a warning and
                // disable all trajectory properties.
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setText("No data source selected. Disabling all"
                               " properties.");
                msgBox.exec();
                enableActorUpdates(false);
                properties->mString()->setValue(utilizedDataSourceProperty, "");
                properties->mInt()->setValue(ensembleMemberProperty, 0);
                properties->mInt()->setMaximum(ensembleMemberProperty, 0);
                enableActorUpdates(true);
                enableProperties(false);
            }
            else
            {
                dataSourceAvailable = true;
            }
        }
    }

    updateInitTimeProperty();
    updateValidTimeProperty();
    updateTrajectoryTimeProperty();

    properties->mEnum()->setValue(
                renderModeProperty,
                settings->value("renderMode").toInt());

    properties->mBool()->setValue(
                enableFilterProperty,
                settings->value("enableFilter").toBool());

    properties->mDDouble()->setValue(
                deltaPressureProperty,
                settings->value("deltaPressure").toFloat());

    properties->mDDouble()->setValue(
                deltaTimeProperty,
                settings->value("deltaTime").toFloat());

    bbox = settings->value("boundingBox").toRectF();
    properties->mRectF()->setValue(bboxProperty, bbox);

    QString tfName = settings->value("transferFunction").toString();
    if ( !setTransferFunction(tfName) )
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(QString("Trajectory actor '%1':\n"
                               "Transfer function '%2' does not exist.\n"
                               "Setting transfer function to 'None'.")
                       .arg(getName()).arg(tfName));
        msgBox.exec();
    }

    properties->mDDouble()->setValue(
                tubeRadiusProperty,
                settings->value("tubeRadius").toFloat());

    properties->mDDouble()->setValue(
                sphereRadiusProperty,
                settings->value("sphereRadius").toFloat());

    properties->mBool()->setValue(
                enableShadowProperty,
                settings->value("shadowEnabled").toBool());

    properties->mBool()->setValue(
                colourShadowProperty,
                settings->value("shadowColoured").toBool());

    settings->endGroup();

    if (isInitialized() && dataSourceAvailable)
    {
        asynchronousSelectionRequest();
        asynchronousDataRequest();
    }
}


void MTrajectoryActor::setTransferFunction(MTransferFunction1D *tf)
{
    transferFunction = tf;
}


bool MTrajectoryActor::setTransferFunction(QString tfName)
{
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


void MTrajectoryActor::synchronizeWith(MSyncControl *sync)

{
    // If the variable is currently connected to a sync control, reset the
    // background colours of the valid and init time properties (they have
    // been set to red/green from this class to indicate time sync status,
    // see setValidDateTime()) and disconnect the signals.
    if (synchronizationControl != nullptr)
    {
        for (int i = 0; i < getScenes().size(); i++)
        {
            getScenes().at(i)->variableDeletesSynchronizationWith(
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
    // Connect to new sync control and try to switch to its current times.
    synchronizationControl = sync;
    if (sync != nullptr)
    {
        // Tell the actor's scenes that this variable synchronized with this
        // sync control.
        for (int i = 0; i < getScenes().size(); i++)
            getScenes().at(i)->variableSynchronizesWith(sync);

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
        for (int i = 0; i < getScenes().size(); i++)
        {
            getScenes().at(i)->resetPropertyColour(initTimeProperty);
            getScenes().at(i)->resetPropertyColour(validTimeProperty);
            getScenes().at(i)->resetPropertyColour(trajectoryTimeProperty);
        }
    }

    // Paint the time property that is NOT synchronized in yellow.
    for (int i = 0; i < getScenes().size(); i++)
    {
        getScenes().at(i)->setPropertyColour(
                    syncWithValidTime ? trajectoryTimeProperty : validTimeProperty,
                    QColor(200, 200, 0));
    }
}


bool MTrajectoryActor::synchronizationEvent(
        MSynchronizationType syncType, QVector<QVariant> data)
{
    switch (syncType)
    {
    case SYNC_INIT_TIME:
    {
        enableActorUpdates(false);
        bool newInitTimeSet = setInitDateTime(data.at(0).toDateTime());
        enableActorUpdates(true);
        if (newInitTimeSet) asynchronousDataRequest(true);
        return newInitTimeSet;
    }
    case SYNC_VALID_TIME:
    {
        enableActorUpdates(false);
        bool newValidTimeSet = setValidDateTime(data.at(0).toDateTime());
        enableActorUpdates(true);
        if (newValidTimeSet) asynchronousDataRequest(true);
        return newValidTimeSet;
        break;
    }
    case SYNC_INIT_VALID_TIME:
    {
        enableActorUpdates(false);
        bool newInitTimeSet = setInitDateTime(data.at(0).toDateTime());
        bool newValidTimeSet = setValidDateTime(data.at(1).toDateTime());
        enableActorUpdates(true);
        if (newInitTimeSet || newValidTimeSet) asynchronousDataRequest(true);
        return (newInitTimeSet || newValidTimeSet);
    }
    case SYNC_ENSEMBLE_MEMBER:
    {
        enableActorUpdates(false);
        bool newEnsembleMemberSet = setEnsembleMember(data.at(0).toInt());
        enableActorUpdates(true);
        if (newEnsembleMemberSet) asynchronousDataRequest(true);
        return newEnsembleMemberSet;
    }
    default:
        break;
    }

    return false;
}


void MTrajectoryActor::setDataSource(MTrajectoryDataSource *ds)
{
    if (trajectorySource != nullptr)
    {
        disconnect(trajectorySource, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousDataAvailable(MDataRequest)));
    }

    trajectorySource = ds;
    if (trajectorySource != nullptr)
    {
        connect(trajectorySource, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousDataAvailable(MDataRequest)));
    }
}


void MTrajectoryActor::setDataSource(const QString& id)
{
   MAbstractDataSource* ds =
           MSystemManagerAndControl::getInstance()->getDataSource(id);
   setDataSource(dynamic_cast<MTrajectoryDataSource*>(ds));
}


void MTrajectoryActor::setNormalsSource(MTrajectoryNormalsSource *s)
{
    if (normalsSource != nullptr)
    {
        disconnect(normalsSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousNormalsAvailable(MDataRequest)));
    }

    normalsSource = s;
    if (normalsSource != nullptr)
    {
        connect(normalsSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousNormalsAvailable(MDataRequest)));
    }
}


void MTrajectoryActor::setNormalsSource(const QString& id)
{
    MAbstractDataSource* ds =
            MSystemManagerAndControl::getInstance()->getDataSource(id);
    setNormalsSource(dynamic_cast<MTrajectoryNormalsSource*>(ds));
}


void MTrajectoryActor::setTrajectoryFilter(MTrajectoryFilter *f)
{
    if (trajectoryFilter != nullptr)
    {
        disconnect(trajectoryFilter, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousSelectionAvailable(MDataRequest)));
        disconnect(trajectoryFilter, SIGNAL(dataRequestCompleted(MDataRequest)),
                   this, SLOT(asynchronousSingleTimeSelectionAvailable(MDataRequest)));
    }

    trajectoryFilter = f;
    if (trajectoryFilter != nullptr)
    {
        connect(trajectoryFilter, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousSelectionAvailable(MDataRequest)));
        connect(trajectoryFilter, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousSingleTimeSelectionAvailable(MDataRequest)));
    }
}


void MTrajectoryActor::setTrajectoryFilter(const QString& id)
{
    MAbstractDataSource* ds =
            MSystemManagerAndControl::getInstance()->getDataSource(id);
    setTrajectoryFilter(dynamic_cast<MTrajectoryFilter*>(ds));
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

bool MTrajectoryActor::setEnsembleMember(int member)
{
    int prevEnsembleMode = properties->mEnum()->value(ensembleModeProperty);

    if (member < 0)
    {
        // Ensemble mean: member == -1. As there are no "mean trajectories"
        // the ensemble mean is interpreted as "render all trajectories".

        // If the ensemble mode is already set to "ALL" return false; nothing
        // needs to be done.
        if (prevEnsembleMode == 1) return false;

        // Else set the property.
        properties->mEnum()->setValue(ensembleModeProperty, 1);
    }
    else
    {
#ifdef DIRECT_SYNCHRONIZATION
        int prevEnsembleMember = properties->mInt()->value(
                    ensembleMemberProperty);
#endif
        // Change ensemble member.
        properties->mInt()->setValue(ensembleMemberProperty, member);
        properties->mEnum()->setValue(ensembleModeProperty, 0);

#ifdef DIRECT_SYNCHRONIZATION
        // Does a new data request need to be emitted?
        if (prevEnsembleMode == 1) return true;
        if (prevEnsembleMember != member) return true;
        return false;
#endif
    }

    return false;
}


bool MTrajectoryActor::setValidDateTime(const QDateTime& datetime)
{
    if (syncWithValidTime)
    {
        return internalSetDateTime(availableValidTimes, datetime,
                                   validTimeProperty);
    }
    else
    {
        return internalSetDateTime(availableTrajectoryTimes, datetime,
                                   trajectoryTimeProperty);
    }
}


bool MTrajectoryActor::setInitDateTime(const QDateTime& datetime)
{
    return internalSetDateTime(availableInitTimes, datetime, initTimeProperty);
}


void MTrajectoryActor::asynchronousDataAvailable(MDataRequest request)
{
    // See NWPActorVariabe::asynchronousDataAvailable() for explanations
    // on request queue handling.

    bool queueContainsEntryWithNoPendingRequests = false;
    for (int i = 0; i < pendingRequestsQueue.size(); i++)
    {
        if (pendingRequestsQueue[i].dataRequest.request == request)
        {
            // If this is the first time we are informed about the availability
            // of the request (available still == false) decrease number of
            // pending requests.
            if ( !pendingRequestsQueue[i].dataRequest.available )
                pendingRequestsQueue[i].numPendingRequests--;

            pendingRequestsQueue[i].dataRequest.available = true;

            if (pendingRequestsQueue[i].numPendingRequests == 0)
                queueContainsEntryWithNoPendingRequests = true;

            // Do NOT break the loop here; "request" might be relevant to
            // multiple entries in the queue.
        }
    }

    if (queueContainsEntryWithNoPendingRequests)
        prepareAvailableDataForRendering();
}


void MTrajectoryActor::asynchronousNormalsAvailable(MDataRequest request)
{
    bool queueContainsEntryWithNoPendingRequests = false;
    for (int i = 0; i < pendingRequestsQueue.size(); i++)
    {
        foreach (MSceneViewGLWidget *view,
                 pendingRequestsQueue[i].normalsRequests.keys())
        {
            if (pendingRequestsQueue[i].normalsRequests[view].request == request)
            {
                if ( !pendingRequestsQueue[i].normalsRequests[view].available )
                    pendingRequestsQueue[i].numPendingRequests--;

                pendingRequestsQueue[i].normalsRequests[view].available = true;

                if (pendingRequestsQueue[i].numPendingRequests == 0)
                    queueContainsEntryWithNoPendingRequests = true;

                // Do NOT break the loop here; "request" might be relevant to
                // multiple entries in the queue.
            }
        }
    }

    if (queueContainsEntryWithNoPendingRequests)
        prepareAvailableDataForRendering();
}


void MTrajectoryActor::asynchronousSelectionAvailable(MDataRequest request)
{
    bool queueContainsEntryWithNoPendingRequests = false;
    for (int i = 0; i < pendingRequestsQueue.size(); i++)
    {
        if (pendingRequestsQueue[i].filterRequest.request == request)
        {
            if (!pendingRequestsQueue[i].filterRequest.available)
                pendingRequestsQueue[i].numPendingRequests--;

            pendingRequestsQueue[i].filterRequest.available = true;

            if (pendingRequestsQueue[i].numPendingRequests == 0)
                queueContainsEntryWithNoPendingRequests = true;

            // Do NOT break the loop here; "request" might be relevant to
            // multiple entries in the queue.
        }
    }

    if (queueContainsEntryWithNoPendingRequests)
        prepareAvailableDataForRendering();
}


void MTrajectoryActor::asynchronousSingleTimeSelectionAvailable(
        MDataRequest request)
{
    bool queueContainsEntryWithNoPendingRequests = false;
    for (int i = 0; i < pendingRequestsQueue.size(); i++)
    {
        if (pendingRequestsQueue[i].singleTimeFilterRequest.request == request)
        {
            if (!pendingRequestsQueue[i].singleTimeFilterRequest.available)
                pendingRequestsQueue[i].numPendingRequests--;

            pendingRequestsQueue[i].singleTimeFilterRequest.available = true;

            if (pendingRequestsQueue[i].numPendingRequests == 0)
                queueContainsEntryWithNoPendingRequests = true;

            // Do NOT break the loop here; "request" might be relevant to
            // multiple entries in the queue.
        }
    }

    if (queueContainsEntryWithNoPendingRequests)
        prepareAvailableDataForRendering();
}


void MTrajectoryActor::prepareAvailableDataForRendering()
{
    // Prepare datafields for rendering as long as they are available in
    // the order in which they were requested.
    while ( ( !pendingRequestsQueue.isEmpty() ) &&
            ( pendingRequestsQueue.head().numPendingRequests == 0 ) )
    {
        MTrajectoryRequestQueueInfo trqi = pendingRequestsQueue.dequeue();

        // 1. Trajectory data.
        // ===================

        if (trqi.dataRequest.available)
        {
            // Release current and get new trajectories.
            if (trajectories)
            {
                trajectories->releaseVertexBuffer();
                trajectorySource->releaseData(trajectories);
            }
            trajectories = trajectorySource->getData(trqi.dataRequest.request);
            trajectoriesVertexBuffer = trajectories->getVertexBuffer();

            // Update displayed information about timestep length.
            float timeStepLength_hours =
                    trajectories->getTimeStepLength_sec() / 3600.;

            properties->mDDouble()->setSingleStep(
                        deltaTimeProperty, timeStepLength_hours);
            properties->mDDouble()->setRange(
                        deltaTimeProperty, timeStepLength_hours,
                        (trajectories->getNumTimeStepsPerTrajectory() - 1)
                        * timeStepLength_hours);

            updateTrajectoryTimeProperty();
        }

        // 2. Normals.
        // ===========

        foreach (MSceneViewGLWidget *view, trqi.normalsRequests.keys())
        {
            if (trqi.normalsRequests[view].available)
            {
                if (normals.value(view, nullptr))
                {
                    normals[view]->releaseVertexBuffer();
                    normalsSource->releaseData(normals[view]);
                }
                normals[view] = normalsSource->getData(
                            trqi.normalsRequests[view].request);
                normalsVertexBuffer[view] = normals[view]->getVertexBuffer();
            }
        }

        // 3. Selection.
        // =============

        if (trqi.filterRequest.available)
        {
            if (trajectorySelection)
                trajectoryFilter->releaseData(trajectorySelection);
            trajectorySelection =
                    trajectoryFilter->getData(trqi.filterRequest.request);
        }

        // 4. Single time selection.
        // =========================

        if (trqi.singleTimeFilterRequest.available)
        {
            if (trajectorySingleTimeSelection)
                trajectoryFilter->releaseData(trajectorySingleTimeSelection);
            trajectorySingleTimeSelection =
                    trajectoryFilter->getData(
                        trqi.singleTimeFilterRequest.request);
        }

#ifdef DIRECT_SYNCHRONIZATION
        // If this was a synchronization request signal to the sync control
        // that it has been completed.
        if (trqi.syncchronizationRequest)
            synchronizationControl->synchronizationCompleted(this);
#endif

        emitActorChangedSignal();
    }
}


void MTrajectoryActor::onActorCreated(MActor *actor)
{
    // If the new actor is a transfer function, add it to the list of
    // available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        // Don't render while the properties are being updated.
        enableEmissionOfActorChangedSignal(false);

        int index = properties->mEnum()->value(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                    transferFunctionProperty);
        availableTFs << tf->transferFunctionName();
        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);

        enableEmissionOfActorChangedSignal(true);
    }
}


void MTrajectoryActor::onActorDeleted(MActor *actor)
{
    // If the deleted actor is a transfer function, remove it from the list of
    // available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        enableEmissionOfActorChangedSignal(false);

        int index = properties->mEnum()->value(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                    transferFunctionProperty);

        // If the deleted transfer function is currently connected to this
        // variable, set current transfer function to "None" (index 0).
        if (availableTFs.at(index) == tf->getName()) index = 0;

        availableTFs.removeOne(tf->getName());
        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);

        enableEmissionOfActorChangedSignal(true);
    }
}


void MTrajectoryActor::onActorRenamed(MActor *actor, QString oldName)
{
    // If the renamed actor is a transfer function, change its name in the list
    // of available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        // Don't render while the properties are being updated.
        enableEmissionOfActorChangedSignal(false);

        int index = properties->mEnum()->value(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                    transferFunctionProperty);

        // Replace affected entry.
        availableTFs[availableTFs.indexOf(oldName)] = tf->getName();

        properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);

        enableEmissionOfActorChangedSignal(true);
    }
}


void MTrajectoryActor::registerScene(MSceneControl *scene)
{
    MActor::registerScene(scene);
    // Only send data request if data source exists, but for each scene since
    // they have different scene views and thus different normals.
    if (dataSourceID != "")
    {
        asynchronousDataRequest();
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTrajectoryActor::initializeActorResources()
{    
    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();

    // Since no data source was selected disable actor properties since they
    // have no use without a data source.
    if (dataSourceID == "" && !selectDataSource())
    {
        // User has selected no data source. Display a warning and disable all
        // trajectory properties.
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("No data source selected. Disabling all properties.");
        msgBox.exec();
        enableProperties(false);
    }
    else
    {
        enableProperties(true);
        this->synchronizeWith(sysMC->getSyncControl("Synchronization"));

        if (textureUnitTransferFunction >=0)
            releaseTextureUnit(textureUnitTransferFunction);
        textureUnitTransferFunction = assignImageUnit();

        updateInitTimeProperty();
        updateValidTimeProperty();

        // Get values from sync control, if connected to one.
        if (synchronizationControl != nullptr)
        {
            setInitDateTime(synchronizationControl->initDateTime());
            updateValidTimeProperty();
            setValidDateTime(synchronizationControl->validDateTime());
            setEnsembleMember(synchronizationControl->ensembleMember());
        }

        asynchronousDataRequest();
    }

    // Load shader program if the returned program is new.
    bool loadShaders = false;
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    loadShaders |= glRM->generateEffectProgram("trajectory_tube",
                                               tubeShader);
    loadShaders |= glRM->generateEffectProgram("trajectory_tubeshadow",
                                               tubeShadowShader);
    loadShaders |= glRM->generateEffectProgram("trajectory_spheres",
                                               positionSphereShader);
    loadShaders |= glRM->generateEffectProgram("trajectory_spheresshadow",
                                               positionSphereShadowShader);

    if (loadShaders) reloadShaderEffects();
}


void MTrajectoryActor::onQtPropertyChanged(QtProperty *property)
{
    if (property == selectDataSourceProperty)
    {
        if (suppressActorUpdates()) return;

        if (selectDataSource())
        {
            enableProperties(true);
            updateInitTimeProperty();
            updateValidTimeProperty();
            asynchronousDataRequest();
        }
    }

    else if (property == utilizedDataSourceProperty)
    {
        dataSourceID = properties->mString()->value(utilizedDataSourceProperty);
        return;
    }

    else if (property == ensembleMemberProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == enableFilterProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == deltaPressureProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == deltaTimeProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == renderModeProperty)
    {
        renderMode = TrajectoryRenderType(
                    properties->mEnum()->value(renderModeProperty));

        // The trajectory time property is not needed when the entire
        // trajectories are rendered.
        switch (renderMode)
        {
        case TRAJECTORY_TUBES:
        case ALL_POSITION_SPHERES:
            trajectoryTimeProperty->setEnabled(false);
            break;
        case SINGLETIME_POSITIONS:
        case TUBES_AND_SINGLETIME:
        case BACKWARDTUBES_AND_SINGLETIME:
            trajectoryTimeProperty->setEnabled(true);
            break;
        }

        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == transferFunctionProperty)
    {
        setTransferFunctionFromProperty();
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else if (property == tubeRadiusProperty)
    {
        tubeRadius = properties->mDDouble()->value(tubeRadiusProperty);
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else if (property == sphereRadiusProperty)
    {
        sphereRadius = properties->mDDouble()->value(sphereRadiusProperty);
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else if (property == enableShadowProperty)
    {
        shadowEnabled = properties->mBool()->value(enableShadowProperty);
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else if (property == colourShadowProperty)
    {
        shadowColoured = properties->mBool()->value(colourShadowProperty);
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    // The init time has been changed. Reload valid times.
    else if (property == initTimeProperty)
    {
        if (suppressActorUpdates()) return;
        updateValidTimeProperty();

        asynchronousDataRequest();
    }

    else if (property == validTimeProperty)
    {
        if (suppressUpdate) return; // ignore if init times are being updated
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == trajectoryTimeProperty)
    {
        trajectoryTimeStep = properties->mEnum()->value(trajectoryTimeProperty);

        if (suppressUpdate) return;
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == timeSyncModeProperty)
    {
        int index = properties->mEnum()->value(timeSyncModeProperty);
        syncWithValidTime = (index == 0);
        if (suppressActorUpdates()) return;

        // Paint the time property that is NOT synchronized in yellow.
        for (int i = 0; i < getScenes().size(); i++)
            getScenes().at(i)->setPropertyColour(
                        syncWithValidTime ? trajectoryTimeProperty : validTimeProperty,
                        QColor(200, 200, 0));

        setValidDateTime(synchronizationControl->validDateTime());
    }

    else if (property == bboxProperty)
    {
        bbox = properties->mRectF()->value(bboxProperty);
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }
}


void MTrajectoryActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{    
    // Only render if transfer function is available.
    if (transferFunction == nullptr)
    {
        return;
    }

    if ( (renderMode == TRAJECTORY_TUBES)
         || (renderMode == TUBES_AND_SINGLETIME)
         || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
    {        
        // If any required data item is missing we cannot render.
        if ( (trajectories == nullptr)
             || (normals[sceneView] == nullptr)
             || (trajectorySelection == nullptr) ) return;

        // If the vertical scaling of the view has changed, a recomputation of
        // the normals is necessary, as they are based on worldZ coordinates.
        if (sceneView->visualisationParametersHaveChanged())
        {
            // Discard old normals.
            if (normals.value(sceneView, nullptr))
                normalsSource->releaseData(normals[sceneView]);

            normals[sceneView] = nullptr;
            return;
        }

        tubeShader->bind();

        tubeShader->setUniformValue(
                    "mvpMatrix",
                    *(sceneView->getModelViewProjectionMatrix()));
        tubeShader->setUniformValue(
                    "pToWorldZParams",
                    sceneView->pressureToWorldZParameters());
        tubeShader->setUniformValue(
                    "lightDirection",
                    sceneView->getLightDirection());
        tubeShader->setUniformValue(
                    "cameraPosition",
                    sceneView->getCamera()->getOrigin());
        tubeShader->setUniformValue(
                    "radius",
                    GLfloat(tubeRadius));
        tubeShader->setUniformValue(
                    "numObsPerTrajectory",
                    trajectories->getNumTimeStepsPerTrajectory());

        if (renderMode == BACKWARDTUBES_AND_SINGLETIME)
        {
            tubeShader->setUniformValue(
                        "renderTubesUpToIndex",
                        trajectoryTimeStep);
        }
        else
        {
            tubeShader->setUniformValue(
                        "renderTubesUpToIndex",
                        trajectories->getNumTimeStepsPerTrajectory());
        }

        // Texture bindings for transfer function for data scalar (1D texture from
        // transfer function class). The data scalar is stored in the vertex.w
        // component passed to the vertex shader.
        transferFunction->getTexture()->bindToTextureUnit(
                    textureUnitTransferFunction);
        tubeShader->setUniformValue(
                    "transferFunction", textureUnitTransferFunction);
        tubeShader->setUniformValue(
                    "scalarMinimum", transferFunction->getMinimumValue());
        tubeShader->setUniformValue(
                    "scalarMaximum", transferFunction->getMaximimValue());

        // Bind trajectories and normals vertex buffer objects.
        trajectoriesVertexBuffer
                ->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

        normalsVertexBuffer[sceneView]
                ->attachToVertexAttribute(SHADER_NORMAL_ATTRIBUTE);

        glPolygonMode(GL_FRONT_AND_BACK,
                      renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
        glLineWidth(1); CHECK_GL_ERROR;

        glMultiDrawArrays(GL_LINE_STRIP_ADJACENCY,
                          trajectorySelection->getStartIndices(),
                          trajectorySelection->getIndexCount(),
                          trajectorySelection->getNumTrajectories());
        CHECK_GL_ERROR;


        if (shadowEnabled)
        {

            tubeShadowShader->bind();

            tubeShadowShader->setUniformValue(
                        "mvpMatrix",
                        *(sceneView->getModelViewProjectionMatrix()));
            tubeShadowShader->setUniformValue(
                        "pToWorldZParams",
                        sceneView->pressureToWorldZParameters());
            tubeShadowShader->setUniformValue(
                        "lightDirection",
                        sceneView->getLightDirection());
            tubeShadowShader->setUniformValue(
                        "cameraPosition",
                        sceneView->getCamera()->getOrigin());
            tubeShadowShader->setUniformValue(
                        "radius",
                        GLfloat(tubeRadius));
            tubeShadowShader->setUniformValue(
                        "numObsPerTrajectory",
                        trajectories->getNumTimeStepsPerTrajectory());

            if (renderMode == BACKWARDTUBES_AND_SINGLETIME)
            {
                tubeShadowShader->setUniformValue(
                            "renderTubesUpToIndex",
                            trajectoryTimeStep);
            }
            else
            {
                tubeShadowShader->setUniformValue(
                            "renderTubesUpToIndex",
                            trajectories->getNumTimeStepsPerTrajectory());
            }

            tubeShadowShader->setUniformValue(
                        "useTransferFunction", GLboolean(shadowColoured));

            if (shadowColoured)
            {
                tubeShadowShader->setUniformValue(
                            "transferFunction", textureUnitTransferFunction);
                tubeShadowShader->setUniformValue(
                            "scalarMinimum", transferFunction->getMinimumValue());
                tubeShadowShader->setUniformValue(
                            "scalarMaximum", transferFunction->getMaximimValue());
            }
            else
                tubeShadowShader->setUniformValue(
                            "constColour", QColor(20, 20, 20, 155));

            glMultiDrawArrays(GL_LINE_STRIP_ADJACENCY,
                              trajectorySelection->getStartIndices(),
                              trajectorySelection->getIndexCount(),
                              trajectorySelection->getNumTrajectories());
            CHECK_GL_ERROR;
        }

        // Unbind VBO.
        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    }


    if ( (renderMode == ALL_POSITION_SPHERES)
         || (renderMode == SINGLETIME_POSITIONS)
         || (renderMode == TUBES_AND_SINGLETIME)
         || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
    {
        if (trajectories == nullptr) return;

        if (renderMode == ALL_POSITION_SPHERES)
        {
            if (trajectorySelection == nullptr) return;
        }
        else
        {
            if (trajectorySingleTimeSelection == nullptr) return;
        }

        positionSphereShader->bindProgram("Normal");

        // Set MVP-matrix and parameters to map pressure to world space in the
        // vertex shader.
        positionSphereShader->setUniformValue(
                    "mvpMatrix",
                    *(sceneView->getModelViewProjectionMatrix()));
        positionSphereShader->setUniformValue(
                    "pToWorldZParams",
                    sceneView->pressureToWorldZParameters());
        positionSphereShader->setUniformValue(
                    "lightDirection",
                    sceneView->getLightDirection());
        positionSphereShader->setUniformValue(
                    "cameraPosition",
                    sceneView->getCamera()->getOrigin());
        positionSphereShader->setUniformValue(
                    "cameraUpDir",
                    sceneView->getCamera()->getYAxis());
        positionSphereShader->setUniformValue(
                    "radius",
                    GLfloat(sphereRadius));
        positionSphereShader->setUniformValue(
                    "scaleRadius",
                    GLboolean(false));


        // Texture bindings for transfer function for data scalar (1D texture from
        // transfer function class). The data scalar is stored in the vertex.w
        // component passed to the vertex shader.
        transferFunction->getTexture()->bindToTextureUnit(
                    textureUnitTransferFunction);
        positionSphereShader->setUniformValue(
                    "useTransferFunction", GLboolean(true));
        positionSphereShader->setUniformValue(
                    "transferFunction", textureUnitTransferFunction);
        positionSphereShader->setUniformValue(
                    "scalarMinimum", transferFunction->getMinimumValue());
        positionSphereShader->setUniformValue(
                    "scalarMaximum", transferFunction->getMaximimValue());

        // Bind vertex buffer object.
        trajectoriesVertexBuffer
                ->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

        glPolygonMode(GL_FRONT_AND_BACK,
                      renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
        glLineWidth(1); CHECK_GL_ERROR;

        if (renderMode == ALL_POSITION_SPHERES)
            glMultiDrawArrays(GL_POINTS,
                              trajectorySelection->getStartIndices(),
                              trajectorySelection->getIndexCount(),
                              trajectorySelection->getNumTrajectories());
        else
            glMultiDrawArrays(GL_POINTS,
                              trajectorySingleTimeSelection->getStartIndices(),
                              trajectorySingleTimeSelection->getIndexCount(),
                              trajectorySingleTimeSelection->getNumTrajectories());
        CHECK_GL_ERROR;


        if (shadowEnabled)
        {
            positionSphereShadowShader->bind();

            positionSphereShadowShader->setUniformValue(
                        "mvpMatrix",
                        *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;
            positionSphereShadowShader->setUniformValue(
                        "pToWorldZParams",
                        sceneView->pressureToWorldZParameters()); CHECK_GL_ERROR;
            positionSphereShadowShader->setUniformValue(
                        "lightDirection",
                        sceneView->getLightDirection());
            positionSphereShadowShader->setUniformValue(
                        "cameraPosition",
                        sceneView->getCamera()->getOrigin()); CHECK_GL_ERROR;
            positionSphereShadowShader->setUniformValue(
                        "radius",
                        GLfloat(sphereRadius)); CHECK_GL_ERROR;
            positionSphereShadowShader->setUniformValue(
                        "scaleRadius",
                        GLboolean(false)); CHECK_GL_ERROR;

            positionSphereShadowShader->setUniformValue(
                        "useTransferFunction", GLboolean(shadowColoured)); CHECK_GL_ERROR;

            if (shadowColoured)
            {
                // Transfer function texture is still bound from the sphere
                // shader.
                positionSphereShadowShader->setUniformValue(
                            "transferFunction", textureUnitTransferFunction); CHECK_GL_ERROR;
                positionSphereShadowShader->setUniformValue(
                            "scalarMinimum", transferFunction->getMinimumValue()); CHECK_GL_ERROR;
                positionSphereShadowShader->setUniformValue(
                            "scalarMaximum", transferFunction->getMaximimValue()); CHECK_GL_ERROR;

            }
            else
                positionSphereShadowShader->setUniformValue(
                            "constColour", QColor(20, 20, 20, 155)); CHECK_GL_ERROR;

            if (renderMode == ALL_POSITION_SPHERES)
                glMultiDrawArrays(GL_POINTS,
                                  trajectorySelection->getStartIndices(),
                                  trajectorySelection->getIndexCount(),
                                  trajectorySelection->getNumTrajectories());
            else
                glMultiDrawArrays(GL_POINTS,
                                  trajectorySingleTimeSelection->getStartIndices(),
                                  trajectorySingleTimeSelection->getIndexCount(),
                                  trajectorySingleTimeSelection->getNumTrajectories());
            CHECK_GL_ERROR;
        }

        // Unbind VBO.
        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

QDateTime MTrajectoryActor::getPropertyTime(QtProperty *enumProperty)
{
    QStringList dateStrings = properties->mEnum()->enumNames(enumProperty);

    // If the list of date strings is empty return an invalid null time.
    if (dateStrings.empty()) return QDateTime();

    int index = properties->mEnum()->value(enumProperty);
    return QDateTime::fromString(dateStrings.at(index), Qt::ISODate);
}


void MTrajectoryActor::setTransferFunctionFromProperty()
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    QString tfName = properties->getEnumItem(transferFunctionProperty);

    if (tfName == "None")
    {
        transferFunction = nullptr;
        return;
    }

    // Find the selected transfer function in the list of actors from the
    // resources manager. Not very efficient, but works well enough for the
    // small number of actors at the moment..
    foreach (MActor *actor, glRM->getActors())
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
        {
            if (tf->transferFunctionName() == tfName)
            {
                transferFunction = tf;
                return;
            }

        }
    }
}


void MTrajectoryActor::asynchronousDataRequest(bool synchronizationRequest)
{
#ifndef DIRECT_SYNCHRONIZATION
    Q_UNUSED(synchronizationRequest);
#endif
    // Initialize empty MTrajectoryRequestQueueInfo.
    MTrajectoryRequestQueueInfo trqi;
    trqi.dataRequest.request = "NULL";
    trqi.dataRequest.available = false;
    trqi.singleTimeFilterRequest.request = "NULL";
    trqi.singleTimeFilterRequest.available = false;
    trqi.filterRequest.request = "NULL";
    trqi.filterRequest.available = false;
    trqi.numPendingRequests = 0;
#ifdef DIRECT_SYNCHRONIZATION
    trqi.syncchronizationRequest = synchronizationRequest;
#endif

    // Request 1: Trajectories for the current time and ensemble settings.
    // ===================================================================
    QDateTime initTime  = getPropertyTime(initTimeProperty);
    QDateTime validTime = getPropertyTime(validTimeProperty);
    unsigned int member = properties->mInt()->value(ensembleMemberProperty);

    MDataRequestHelper rh;
    rh.insert("INIT_TIME", initTime);
    rh.insert("VALID_TIME", validTime);
    rh.insert("MEMBER", member);
    rh.insert("TIME_SPAN", "ALL");

    trqi.dataRequest.request = rh.request();
    trqi.numPendingRequests++;

    // Request 2: Normals for all scene views that display the trajectories.
    // =====================================================================
    foreach (MSceneViewGLWidget* view, getViews())
    {
        QVector2D params = view->pressureToWorldZParameters();
        QString query = QString("%1/%2").arg(params.x()).arg(params.y());
        LOG4CPLUS_DEBUG(mlog, "NORMALS: " << query.toStdString());

        rh.insert("NORMALS_LOGP_SCALED", query);
        MRequestQueueInfo rqi;
        rqi.request = rh.request();
        rqi.available = false;
        trqi.normalsRequests.insert(view, rqi);
        trqi.numPendingRequests++;
    }
    rh.remove("NORMALS_LOGP_SCALED");

    // Request 3: Pressure/Time selection filter.
    // ==========================================

//TODO: add property
    rh.insert("TRY_PRECOMPUTED", 1);

    bool filteringEnabled = properties->mBool()->value(enableFilterProperty);
    if (filteringEnabled)
    {
        float deltaPressure_hPa = properties->mDDouble()->value(
                    deltaPressureProperty);
        int deltaTime_hrs = properties->mDDouble()->value(deltaTimeProperty);
        // Request is e.g. 500/48 for 500 hPa in 48 hours.
        rh.insert("FILTER_PRESSURE_TIME",
                  QString("%1/%2").arg(deltaPressure_hPa).arg(deltaTime_hrs));
        // Request bounding box filtering.
        rh.insert("FILTER_BBOX", QString("%1/%2/%3/%4")
                  .arg(bbox.x()).arg(bbox.y())
                  .arg(bbox.width() + bbox.x()).arg(bbox.height() + bbox.y()));
    }
    else
    {
        rh.insert("FILTER_PRESSURE_TIME", "ALL");
        rh.insert("FILTER_BBOX", "ALL");
    }

    if ((renderMode == SINGLETIME_POSITIONS)
            || (renderMode == TUBES_AND_SINGLETIME)
            || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
    {
        rh.insert("FILTER_TIMESTEP", QString("%1").arg(trajectoryTimeStep));        
        trqi.singleTimeFilterRequest.request = rh.request();
        trqi.numPendingRequests++;
    }

    if (renderMode != SINGLETIME_POSITIONS)
    {
        rh.insert("FILTER_TIMESTEP", "ALL");
        trqi.filterRequest.request = rh.request();
        trqi.numPendingRequests++;
    }

    LOG4CPLUS_DEBUG(mlog, "Enqueuing with [" << trqi.numPendingRequests
                    << "] pending requests.");
    pendingRequestsQueue.enqueue(trqi);
//    debugPrintPendingRequestsQueue();

    // Emit requests AFTER its information has been posted to the queue.
    // (Otherwise requestData() may trigger a call to asynchronous...Available()
    // before the request information has been posted; then the incoming
    // request is not accepted).

    trajectorySource->requestData(trqi.dataRequest.request);

    foreach (MSceneViewGLWidget* view, getViews())
    {
        normalsSource->requestData(trqi.normalsRequests[view].request);
    }

    if ((renderMode == SINGLETIME_POSITIONS)
            || (renderMode == TUBES_AND_SINGLETIME)
            || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
    {
        trajectoryFilter->requestData(trqi.singleTimeFilterRequest.request);
    }

    if (renderMode != SINGLETIME_POSITIONS)
    {
        trajectoryFilter->requestData(trqi.filterRequest.request);
    }
}


void MTrajectoryActor::asynchronousSelectionRequest()
{
    MTrajectoryRequestQueueInfo trqi;
    trqi.dataRequest.request = "NULL";
    trqi.dataRequest.available = false;
    trqi.singleTimeFilterRequest.request = "NULL";
    trqi.singleTimeFilterRequest.available = false;
    trqi.filterRequest.request = "NULL";
    trqi.filterRequest.available = false;
    trqi.numPendingRequests = 0;
#ifdef DIRECT_SYNCHRONIZATION
    // Selection requests currently are not synchronized.
    trqi.syncchronizationRequest = false;
#endif

    // Get the current init and valid (= trajectory start) time.
    QDateTime initTime  = getPropertyTime(initTimeProperty);
    QDateTime validTime = getPropertyTime(validTimeProperty);
    unsigned int member = properties->mInt()->value(ensembleMemberProperty);

    MDataRequestHelper rh;
    rh.insert("INIT_TIME", initTime);
    rh.insert("VALID_TIME", validTime);
    rh.insert("MEMBER", member);
    rh.insert("TIME_SPAN", "ALL");
//TODO: add property
    rh.insert("TRY_PRECOMPUTED", 1);

    // Filter the trajectories of this member according to the specified
    // pressure interval (xx hPa over the "lifetime" of the trajectories;
    // e.g. for T-NAWDEX over 48 hours).

    bool filteringEnabled = properties->mBool()->value(enableFilterProperty);
    if (filteringEnabled)
    {
        float deltaPressure_hPa = properties->mDDouble()->value(
                    deltaPressureProperty);
        int deltaTime_hrs = properties->mDDouble()->value(deltaTimeProperty);
        // Request is e.g. 500/48 for 500 hPa in 48 hours.
        rh.insert("FILTER_PRESSURE_TIME",
                  QString("%1/%2").arg(deltaPressure_hPa).arg(deltaTime_hrs));
        // Request bounding box filtering.
        rh.insert("FILTER_BBOX", QString("%1/%2/%3/%4")
                  .arg(bbox.x()).arg(bbox.y())
                  .arg(bbox.width() + bbox.x()).arg(bbox.height() + bbox.y()));
    }
    else
    {
        rh.insert("FILTER_PRESSURE_TIME", "ALL");
        rh.insert("FILTER_BBOX", "ALL");
    }

    if ((renderMode == SINGLETIME_POSITIONS)
            || (renderMode == TUBES_AND_SINGLETIME)
            || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
    {
        rh.insert("FILTER_TIMESTEP", QString("%1").arg(trajectoryTimeStep));
        trqi.singleTimeFilterRequest.request = rh.request();
        trqi.numPendingRequests++;
    }

    if (renderMode != SINGLETIME_POSITIONS)
    {
        rh.insert("FILTER_TIMESTEP", "ALL");
        trqi.filterRequest.request = rh.request();
        trqi.numPendingRequests++;
    }

    pendingRequestsQueue.enqueue(trqi);

    if ((renderMode == SINGLETIME_POSITIONS)
            || (renderMode == TUBES_AND_SINGLETIME)
            || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
    {
        trajectoryFilter->requestData(trqi.singleTimeFilterRequest.request);
    }

    if (renderMode != SINGLETIME_POSITIONS)
    {
        trajectoryFilter->requestData(trqi.filterRequest.request);
    }
}


void MTrajectoryActor::updateInitTimeProperty()
{
    suppressUpdate = true;

    if (trajectorySource == nullptr)
    {
        properties->mEnum()->setEnumNames(initTimeProperty, QStringList());
    }
    else
    {
        // Get the current init time value.
        QDateTime initTime  = getPropertyTime(initTimeProperty);

        // Get available init times from the data loader. Convert the QDateTime
        // objects to strings for the enum manager.
        availableInitTimes = trajectorySource->availableInitTimes();
        QStringList timeStrings;
        for (int i = 0; i < availableInitTimes.size(); i++)
            timeStrings << availableInitTimes.at(i).toString(Qt::ISODate);

        properties->mEnum()->setEnumNames(initTimeProperty, timeStrings);

        int newIndex = max(0, availableInitTimes.indexOf(initTime));
        properties->mEnum()->setValue(initTimeProperty, newIndex);
    }

    suppressUpdate = false;
}


void MTrajectoryActor::updateValidTimeProperty()
{
    suppressUpdate = true;

    if (trajectorySource == nullptr)
    {
        properties->mEnum()->setEnumNames(validTimeProperty, QStringList());
    }
    else
    {
        // Get the current time values.
        QDateTime initTime  = getPropertyTime(initTimeProperty);
        QDateTime validTime = getPropertyTime(validTimeProperty);

        // Get a list of the available valid times for the new init time,
        // convert the QDateTime objects to strings for the enum manager.
        availableValidTimes = trajectorySource->availableValidTimes(initTime);
        QStringList validTimeStrings;
        for (int i = 0; i < availableValidTimes.size(); i++)
            validTimeStrings << availableValidTimes.at(i).toString(Qt::ISODate);

        properties->mEnum()->setEnumNames(validTimeProperty, validTimeStrings);

        int newIndex = max(0, availableValidTimes.indexOf(validTime));
        properties->mEnum()->setValue(validTimeProperty, newIndex);
    }

    suppressUpdate = false;
}


void MTrajectoryActor::updateTrajectoryTimeProperty()
{
    suppressUpdate = true;

    if (trajectories == nullptr)
    {
        properties->mEnum()->setEnumNames(trajectoryTimeProperty, QStringList());
    }
    else
    {
        QDateTime currentValue = getPropertyTime(trajectoryTimeProperty);

        availableTrajectoryTimes = trajectories->getTimes().toList();
        QStringList trajectoryTimeStrings;
        for (int i = 0; i < trajectories->getTimes().size(); i++)
            trajectoryTimeStrings
                    << trajectories->getTimes().at(i).toString(Qt::ISODate);

        properties->mEnum()->setEnumNames(trajectoryTimeProperty,
                                          trajectoryTimeStrings);

        // Try to restore previous time value. If the previous value is not
        // available for the new trajectories, indexOf() returns -1. This is
        // changed to 0, i.e. the first available time value is selected.
        int newIndex = max(0, trajectories->getTimes().indexOf(currentValue));
        properties->mEnum()->setValue(trajectoryTimeProperty, newIndex);

        // The trajectory time property is not needed when the entire
        // trajectories are rendered.
        switch (renderMode)
        {
        case TRAJECTORY_TUBES:
        case ALL_POSITION_SPHERES:
            trajectoryTimeProperty->setEnabled(false);
            break;
        case SINGLETIME_POSITIONS:
        case TUBES_AND_SINGLETIME:
        case BACKWARDTUBES_AND_SINGLETIME:
            trajectoryTimeProperty->setEnabled(true);
            break;
        }
    }

    suppressUpdate = false;
}


bool MTrajectoryActor::internalSetDateTime(
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
        // Update background colour of the valid time property in the connected
        // scene's property browser: green if the scene's valid time is an
        // exact match with one of the available valid time, red otherwise.
        if (synchronizationControl != nullptr)
        {
            QColor colour = exactMatch ? QColor(0, 255, 0) : QColor(255, 0, 0);
            for (int i = 0; i < getScenes().size(); i++)
                getScenes().at(i)->setPropertyColour(timeProperty, colour);
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


void MTrajectoryActor::debugPrintPendingRequestsQueue()
{
    // Debug: Output content of request queue.

    QString str = QString("\n==================\nPending requests queue:\n");

    for (int i = 0; i < pendingRequestsQueue.size(); i++)
    {
        str += QString("Entry #%1 [%2]\n[%3] %4\n[%5] %6\n[%7] %8\n")
                .arg(i).arg(pendingRequestsQueue[i].numPendingRequests)
                .arg(pendingRequestsQueue[i].dataRequest.available)
                .arg(pendingRequestsQueue[i].dataRequest.request)
                .arg(pendingRequestsQueue[i].filterRequest.available)
                .arg(pendingRequestsQueue[i].filterRequest.request)
                .arg(pendingRequestsQueue[i].singleTimeFilterRequest.available)
                .arg(pendingRequestsQueue[i].singleTimeFilterRequest.request);

        foreach (MSceneViewGLWidget *view,
                 pendingRequestsQueue[i].normalsRequests.keys())
        {
            str += QString("[%1] %2\n")
                    .arg(pendingRequestsQueue[i].normalsRequests[view].available)
                    .arg(pendingRequestsQueue[i].normalsRequests[view].request);
        }
    }

    str += QString("\n==================\n");

    LOG4CPLUS_DEBUG(mlog, str.toStdString());
}


bool MTrajectoryActor::selectDataSource()
{
    // Ask the user for data sources to which times and ensemble members the
    // sync control should be restricted to.
    MSelectDataSourceDialog dialog(MSelectDataSourceDialogType::TRAJECTORIES, 0);
    if (dialog.exec() == QDialog::Rejected)
    {
        return false;
    }

    QString dataSourceID = dialog.getSelectedDataSourceID();

    if (dataSourceID == "")
    {
        return false;
    }

    // Only change data sources if necessary.
    if (this->dataSourceID != dataSourceID)
    {
        properties->mString()->setValue(utilizedDataSourceProperty,
                                        dataSourceID);

        // Release data from old data sources before switching to new data
        // sources.
        releaseData();

        this->setDataSource(dataSourceID + QString(" Reader"));
        this->setNormalsSource(dataSourceID + QString(" Normals"));
        this->setTrajectoryFilter(dataSourceID + QString(" timestepFilter"));

        return true;
    }

    return false;
}


void MTrajectoryActor::enableProperties(bool enable)
{
    enableActorUpdates(false);
    ensembleModeProperty->setEnabled(enable);
    enableFilterProperty->setEnabled(enable);
    deltaPressureProperty->setEnabled(enable);
    deltaTimeProperty->setEnabled(enable);
    renderModeProperty->setEnabled(enable);
    transferFunctionProperty->setEnabled(enable);
    tubeRadiusProperty->setEnabled(enable);
    sphereRadiusProperty->setEnabled(enable);
    enableShadowProperty->setEnabled(enable);
    colourShadowProperty->setEnabled(enable);
    initTimeProperty->setEnabled(enable);
    validTimeProperty->setEnabled(enable);
    trajectoryTimeProperty->setEnabled(enable);
    timeSyncModeProperty->setEnabled(enable);
    bboxProperty->setEnabled(enable);
    ensembleMemberProperty->setEnabled(enable);
    enableActorUpdates(true);
}


void MTrajectoryActor::releaseData()
{
    // 1. Trajectory data.
    // ===================
    if (trajectories)
    {
        trajectories->releaseVertexBuffer();
        trajectorySource->releaseData(trajectories);
        trajectories = nullptr;
    }

    // 2. Normals.
    // ===========
    foreach (MSceneViewGLWidget *view,
             MSystemManagerAndControl::getInstance()->getRegisteredViews())
    {
        if (normals.value(view, nullptr))
        {
            normals[view]->releaseVertexBuffer();
            normalsSource->releaseData(normals[view]);
            normals[view] = nullptr;
        }
    }

    // 3. Selection.
    // =============
    if (trajectorySelection)
    {
        trajectoryFilter->releaseData(trajectorySelection);
        trajectorySelection = nullptr;
    }

    // 4. Single time selection.
    // =========================
    if (trajectorySingleTimeSelection)
    {
        trajectoryFilter->releaseData(trajectorySingleTimeSelection);
        trajectorySingleTimeSelection = nullptr;
    }
}


} // namespace Met3D
