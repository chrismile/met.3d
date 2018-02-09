/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2016-2017 Bianca Tost
**  Copyright 2017      Philipp Kaiser
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
#include "mainwindow.h"
#include "gxfw/selectactordialog.h"
#include "data/trajectoryreader.h"
#include "data/trajectorycalculator.h"
#include "actors/movablepoleactor.h"
#include "actors/nwphorizontalsectionactor.h"
#include "actors/nwpverticalsectionactor.h"
#include "actors/volumebboxactor.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryActor::MTrajectoryActor()
    : MActor(),
      MBoundingBoxInterface(this),
      trajectorySource(nullptr),
      normalsSource(nullptr),
      trajectoryFilter(nullptr),
      dataSourceID(""),
      precomputedDataSource(false),
      initialDataRequest(true),
      suppressUpdate(false),
      renderMode(TRAJECTORY_TUBES),
      synchronizationControl(nullptr),
      synchronizeInitTime(true),
      synchronizeStartTime(true),
      synchronizeParticlePosTime(true),
      synchronizeEnsemble(true),
      transferFunction(nullptr),
      textureUnitTransferFunction(-1),
      tubeRadius(0.1),
      sphereRadius(0.2),
      shadowEnabled(true),
      shadowColoured(false)
{
    bBoxConnection =
            new MBoundingBoxConnection(this, MBoundingBoxConnection::HORIZONTAL);

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setActorType("Trajectories");
    setName(getActorType());

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


    // Property: Synchronize time and ensemble with an MSyncControl instance?
    synchronizationPropertyGroup = addProperty(
                GROUP_PROPERTY, "synchronization", actorPropertiesSupGroup);

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    synchronizationProperty = addProperty(
                ENUM_PROPERTY, "synchronize with", synchronizationPropertyGroup);
    properties->mEnum()->setEnumNames(
                synchronizationProperty, sysMC->getSyncControlIdentifiers());

    synchronizeInitTimeProperty = addProperty(
                BOOL_PROPERTY, "sync init time", synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeInitTimeProperty,
                                  synchronizeInitTime);
    synchronizeStartTimeProperty = addProperty(
                BOOL_PROPERTY, "sync valid with start time",
                synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeStartTimeProperty,
                                  synchronizeStartTime);
    synchronizeParticlePosTimeProperty = addProperty(
                BOOL_PROPERTY, "sync valid with particles",
                synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeParticlePosTimeProperty,
                                  synchronizeParticlePosTime);
    synchronizeEnsembleProperty = addProperty(
                BOOL_PROPERTY, "sync ensemble", synchronizationPropertyGroup);
    properties->mBool()->setValue(synchronizeEnsembleProperty,
                                  synchronizeEnsemble);


    initTimeProperty = addProperty(ENUM_PROPERTY, "initialisation",
                                   actorPropertiesSupGroup);

    startTimeProperty = addProperty(ENUM_PROPERTY, "trajectory start",
                                    actorPropertiesSupGroup);

    particlePosTimeProperty = addProperty(ENUM_PROPERTY, "particle positions",
                                          actorPropertiesSupGroup);
    particlePosTimeProperty->setToolTip("Not selectable for 'tubes' and 'all"
                                        " positions' render mode");

    // Property: Trajectory computation.
    computationPropertyGroup = addProperty(GROUP_PROPERTY,
                                           "trajectory computation",
                                           actorPropertiesSupGroup);
    computationLineTypeProperty = addProperty(ENUM_PROPERTY, "line type",
                                              computationPropertyGroup);
    properties->mEnum()->setEnumNames(computationLineTypeProperty,
                                      QStringList()
                                      << "trajectories (path lines)"
                                      << "stream lines");

    computationIntegrationMethodProperty = addProperty(
                ENUM_PROPERTY, "integration", computationPropertyGroup);
    properties->mEnum()->setEnumNames(computationIntegrationMethodProperty,
                                      QStringList() << "Euler"
                                      << "Runge-Kutta");

    computationInterpolationMethodProperty = addProperty(
                ENUM_PROPERTY, "interpolation", computationPropertyGroup);
    properties->mEnum()->setEnumNames(computationInterpolationMethodProperty,
                                      QStringList()
                                      << "Lagranto"
                                      << "trilinear (lon-lat-lnp)");

    computationIterationCountProperty = addProperty(INT_PROPERTY,
                                                    "iteration per timestep",
                                                    computationPropertyGroup);
    properties->setInt(computationIterationCountProperty, 5, 1, 1000);

    computationIntegrationTimeProperty = addProperty(
                ENUM_PROPERTY, "integration time", computationPropertyGroup);
    computationIntegrationTimeProperty->setToolTip(
                "integration time from \"trajectory start\" time specified above");

    computationSeedPropertyGroup = addProperty(
                GROUP_PROPERTY, "seed points (start positions)",
                computationPropertyGroup);
    computationSeedAddActorProperty = addProperty(CLICK_PROPERTY, "add",
                                                  computationSeedPropertyGroup);

    // Ensemble.
    QStringList ensembleModeNames;
    ensembleModeNames << "member" << "all";
    ensembleModeProperty = addProperty(ENUM_PROPERTY, "ensemble mode",
                                       actorPropertiesSupGroup);
    properties->mEnum()->setEnumNames(ensembleModeProperty, ensembleModeNames);
    ensembleModeProperty->setEnabled(false);

    ensembleMemberProperty = addProperty(INT_PROPERTY, "ensemble member",
                                         actorPropertiesSupGroup);
    properties->setInt(ensembleMemberProperty, 0, 0, 50, 1);

    // Trajectory filtering.
    filtersGroupProperty =  addProperty(
                GROUP_PROPERTY, "trajectory filters",
                actorPropertiesSupGroup);
    enableAscentFilterProperty = addProperty(BOOL_PROPERTY, "ascent",
                                       filtersGroupProperty);
    properties->mBool()->setValue(enableAscentFilterProperty, true);

    deltaPressureFilterProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                              "pressure difference",
                                              enableAscentFilterProperty);
    properties->setDDouble(deltaPressureFilterProperty, 500., 1., 1050., 2, 5.,
                           " hPa");

    deltaTimeFilterProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                          "time interval",
                                          enableAscentFilterProperty);
    properties->setDDouble(deltaTimeFilterProperty, 48, 1, 48, 0, 1, " hrs");

    filtersGroupProperty->addSubProperty(bBoxConnection->getProperty());

    renderingGroupProperty = addProperty(GROUP_PROPERTY, "rendering",
                                         actorPropertiesSupGroup);

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
    transferFunctionProperty = addProperty(ENUM_PROPERTY,
                                           "transfer function pressure",
                                           renderingGroupProperty);
    properties->mEnum()->setEnumNames(transferFunctionProperty, availableTFs);

    // Render mode and parameters.
    tubeRadiusProperty = addProperty(DECORATEDDOUBLE_PROPERTY, "tube radius",
                                     renderingGroupProperty);
    properties->setDDouble(tubeRadiusProperty, tubeRadius,
                           0.01, 1., 2, 0.1, " (world space)");

    sphereRadiusProperty = addProperty(DECORATEDDOUBLE_PROPERTY, "sphere radius",
                                       renderingGroupProperty);
    properties->setDDouble(sphereRadiusProperty, sphereRadius,
                           0.01, 1., 2, 0.1, " (world space)");

    enableShadowProperty = addProperty(BOOL_PROPERTY, "render shadows",
                                       renderingGroupProperty);
    properties->mBool()->setValue(enableShadowProperty, shadowEnabled);

    colourShadowProperty = addProperty(BOOL_PROPERTY, "colour shadows",
                                       renderingGroupProperty);
    properties->mBool()->setValue(colourShadowProperty, shadowColoured);

    // Observe the creation/deletion of other actors
    // -- if these are transfer functions add to the list displayed in the transfer function property.
    // -- if these are used as seed actors, change corresponding properties
    connect(glRM, SIGNAL(actorCreated(MActor*)), SLOT(onActorCreated(MActor*)));
    connect(glRM, SIGNAL(actorDeleted(MActor*)), SLOT(onActorDeleted(MActor*)));
    connect(glRM, SIGNAL(actorRenamed(MActor*, QString)),
            SLOT(onActorRenamed(MActor*, QString)));

    endInitialiseQtProperties();
}


MTrajectoryActor::~MTrajectoryActor()
{
    // Delete synchronization links (don't update the already deleted GUI
    // properties anymore...).
    synchronizeWith(nullptr, false);

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

    // Save synchronization properties.
    settings->setValue("synchronizationID",
                       (synchronizationControl != nullptr) ?
                           synchronizationControl->getID() : "");
    settings->setValue("synchronizeInitTime",
                       properties->mBool()->value(synchronizeInitTimeProperty));
    settings->setValue("synchronizeStartTime",
                       properties->mBool()->value(synchronizeStartTimeProperty));
    settings->setValue("synchronizeParticlePosTime",
                       properties->mBool()->value(
                           synchronizeParticlePosTimeProperty));
    settings->setValue("synchronizeEnsemble",
                       properties->mBool()->value(synchronizeEnsembleProperty));

    settings->setValue("enableFilter",
                       properties->mBool()->value(enableAscentFilterProperty));

    settings->setValue("deltaPressure",
                       properties->mDDouble()->value(deltaPressureFilterProperty));
    settings->setValue("deltaTime",
                       properties->mDDouble()->value(deltaTimeFilterProperty));

    MBoundingBoxInterface::saveConfiguration(settings);

    settings->setValue("transferFunction",
                       properties->getEnumItem(transferFunctionProperty));

    settings->setValue("tubeRadius", tubeRadius);
    settings->setValue("sphereRadius", sphereRadius);
    settings->setValue("shadowEnabled", shadowEnabled);
    settings->setValue("shadowColoured", shadowColoured);

    // Save computation properties.
    settings->setValue(
                "computationLineTypeProperty",
                properties->mEnum()->value(computationLineTypeProperty));
    settings->setValue(
                "computationIterationMethodProperty",
                properties->mEnum()->value(computationLineTypeProperty));
    settings->setValue("computationInterpolationMethodProperty",
                       properties->mEnum()->value(
                           computationInterpolationMethodProperty));
    settings->setValue(
                "computationIterationCountProperty",
                properties->mInt()->value(computationIterationCountProperty));
    settings->setValue(
                "computationDeltaTimeProperty",
                properties->mEnum()->value(computationIntegrationTimeProperty));
    settings->setValue(
                "computationSeedActorSize",
                computationSeedActorProperties.size());
    for (int i = 0; i <  computationSeedActorProperties.size(); i++)
    {
        const SeedActorSettings& sas = computationSeedActorProperties.at(i);
        settings->setValue(QString("computationSeedActorName%1").arg(i),
                           sas.propertyGroup->propertyName());
        settings->setValue(QString("computationSeedActorStepSizeLon%1").arg(i),
                           properties->mDouble()->value(sas.lonSpacing));
        settings->setValue(QString("computationSeedActorStepSizeLat%1").arg(i),
                           properties->mDouble()->value(sas.latSpacing));
        settings->setValue(
                    QString("computationSeedActorPressureLevels%1").arg(i),
                    properties->mString()->value(sas.pressureLevels));
    }

    settings->endGroup();
}


void MTrajectoryActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MTrajectoryActor::getSettingsID());

    QString dataSourceID = settings->value("dataSourceID", "").toString();

    releaseData();
    enableProperties(true);

    bool dataSourceAvailable = false;

    if ( !dataSourceID.isEmpty() )
    {
        dataSourceAvailable =
                MSelectDataSourceDialog::checkForTrajectoryDataSource(
                    dataSourceID);
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
                    properties->mString()->setValue(utilizedDataSourceProperty,
                                                    "");
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
        else
        {
            properties->mString()->setValue(utilizedDataSourceProperty,
                                            dataSourceID);

            this->setDataSource(dataSourceID + QString(" Reader"));
            this->setNormalsSource(dataSourceID + QString(" Normals"));
            this->setTrajectoryFilter(dataSourceID + QString(" timestepFilter"));

            updateInitTimeProperty();
            updateStartTimeProperty();
        }
    }

    properties->mEnum()->setValue(
                renderModeProperty,
                settings->value("renderMode", 0).toInt());

    // Load synchronization properties (AFTER the ensemble mode properties
    // have been loaded; sync may overwrite some settings).
    // ===================================================================
    properties->mBool()->setValue(
                synchronizeInitTimeProperty,
                settings->value("synchronizeInitTime", true).toBool());
    properties->mBool()->setValue(
                synchronizeStartTimeProperty,
                settings->value("synchronizeStartTime", true).toBool());
    properties->mBool()->setValue(
                synchronizeParticlePosTimeProperty,
                settings->value("synchronizeParticlePosTime", true).toBool());
    properties->mBool()->setValue(
                synchronizeEnsembleProperty,
                settings->value("synchronizeEnsemble", true).toBool());

    QString syncID = settings->value("synchronizationID", "").toString();

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
            msgBox.setText(QString("Trajectory actor '%1':\n"
                                   "Synchronization control '%2' does not exist.\n"
                                   "Setting synchronization control to 'None'.")
                           .arg(getName()).arg(syncID));
            msgBox.exec();

            synchronizeWith(nullptr);
        }
    }

    properties->mBool()->setValue(
                enableAscentFilterProperty,
                settings->value("enableFilter", true).toBool());

    properties->mDDouble()->setValue(
                deltaPressureFilterProperty,
                settings->value("deltaPressure", 500.).toDouble());

    // The range is adapted to the data source thus when changing sessions it
    // might appear the value can't be set correctly due to the property is
    // still restricted to the range of the data sourc of the previous session.
    // Therefore we set the range to a range which covers all possible value.
    properties->mDDouble()->setRange(
                deltaTimeFilterProperty, 0.,
                std::numeric_limits<double>::max());

    properties->mDDouble()->setValue(
                deltaTimeFilterProperty,
                settings->value("deltaTime", 48.).toDouble());

    MBoundingBoxInterface::loadConfiguration(settings);

    QString tfName = settings->value("transferFunction").toString();
    while (!setTransferFunction(tfName))
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(this->getName());
        msgBox.setText(QString("Variable '%1' requires a transfer function "
                               "'%2' that does not exist.\n"
                               "Would you like to load the transfer function "
                               "from file?")
                       .arg(getName()).arg(tfName));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.button(QMessageBox::Yes)->setText("Load transfer function");
        msgBox.button(QMessageBox::No)->setText("Discard dependency");
        msgBox.exec();
        if (msgBox.clickedButton() == msgBox.button(QMessageBox::Yes))
        {
            MSystemManagerAndControl *sysMC =
                    MSystemManagerAndControl::getInstance();
            // Create default actor to get name of actor factory.
            MTransferFunction1D *defaultActor = new MTransferFunction1D();
            sysMC->getMainWindow()->getSceneManagementDialog()
                    ->loadRequiredActorFromFile(defaultActor->getName(),
                                                tfName,
                                                settings->fileName());
            delete defaultActor;
        }
        else
        {
            break;
        }
    }

    properties->mDDouble()->setValue(
                tubeRadiusProperty,
                settings->value("tubeRadius", 0.1).toDouble());

    properties->mDDouble()->setValue(
                sphereRadiusProperty,
                settings->value("sphereRadius", 0.2).toDouble());

    properties->mBool()->setValue(
                enableShadowProperty,
                settings->value("shadowEnabled", true).toBool());

    properties->mBool()->setValue(
                colourShadowProperty,
                settings->value("shadowColoured", false).toBool());


    // Load computation properties (The saved actors should already be initialized)
    properties->mEnum()->setValue(
                computationLineTypeProperty,
                settings->value("computationLineTypeProperty", 0).toInt());
    properties->mEnum()->setValue(
                computationIntegrationMethodProperty,
                settings->value("computationIterationMethodProperty",
                                0).toInt());
    properties->mEnum()->setValue(
                computationInterpolationMethodProperty,
                settings->value("computationInterpolationMethodProperty",
                                0).toInt());
    properties->mInt()->setValue(
                computationIterationCountProperty,
                settings->value("computationIterationCountProperty",
                                5).toInt());
    properties->mEnum()->setValue(
                computationIntegrationTimeProperty,
                settings->value("computationDeltaTimeProperty", 0).toInt());

    // Remove current seed actors.
    clearSeedActor();

    const int actorCount =
            settings->value("computationSeedActorSize", 0).toInt();
    for (int i = 0; i < actorCount; i++)
    {
        QString actorName = settings->value(
                    QString("computationSeedActorName%1").arg(i)).toString();
        double deltaLon =
                settings->value(QString("computationSeedActorStepSizeLon%1")
                                .arg(i)).toDouble();
        double deltaLat =
                settings->value(QString("computationSeedActorStepSizeLat%1")
                                .arg(i)).toDouble();
        QString presLvls =
                settings->value(QString("computationSeedActorPressureLevels%1")
                                .arg(i)).toString();
        addSeedActor(actorName, deltaLon, deltaLat,
                     parsePressureLevelString(presLvls));
    }

    settings->endGroup();

    if (isInitialized() && dataSourceAvailable)
    {
        updateActorData();
        asynchronousSelectionRequest();
        asynchronousDataRequest();
    }
    enableProperties(dataSourceAvailable);
}


void MTrajectoryActor::onBoundingBoxChanged()
{
    labels.clear();
    if (dataSourceID == "" || suppressActorUpdates())
    {
        return;
    }
    // Different from other actors the trajectory actor needs a recomputation
    // of its trajectories if swichting to no bounding box since this switch
    // disables the bounding box filter.
    asynchronousSelectionRequest();
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


void MTrajectoryActor::synchronizeWith(
        MSyncControl *sync, bool updateGUIProperties)
{
    if (synchronizationControl == sync)
    {
        return;
    }

    // Reset connection to current synchronization control.
    // ====================================================

    // If the variable is currently connected to a sync control, reset the
    // background colours of the start and init time properties (they have
    // been set to red/green from this class to indicate time sync status,
    // see setStartDateTime()) and disconnect the signals.
    if (synchronizationControl != nullptr)
    {
        foreach (MSceneControl* scene, getScenes())
        {
            scene->variableDeletesSynchronizationWith(synchronizationControl);
        }

#ifdef DIRECT_SYNCHRONIZATION
        synchronizationControl->deregisterSynchronizedClass(this);
#else
        disconnect(synchronizationControl, SIGNAL(initDateTimeChanged(QDateTime)),
                   this, SLOT(setInitDateTime(QDateTime)));
        disconnect(synchronizationControl, SIGNAL(validDateTimeChanged(QDateTime)),
                   this, SLOT(setStartDateTime(QDateTime)));
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
        QString displayedSyncID =
                properties->getEnumItem(synchronizationProperty);
        QString newSyncID =
                (sync == nullptr) ? "None" : synchronizationControl->getID();
        if (displayedSyncID != newSyncID)
        {
            enableActorUpdates(false);
            properties->setEnumItem(synchronizationProperty, newSyncID);
            enableActorUpdates(true);
        }
    }

    // Connect to new sync control and synchronize.
    // ============================================
    if (sync != nullptr)
    {
        // Tell the actor's scenes that this variable synchronized with this
        // sync control.
        foreach (MSceneControl* scene, getScenes())
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
                this, SLOT(setStartDateTime(QDateTime)));
        connect(sync, SIGNAL(ensembleMemberChanged(int)),
                this, SLOT(setEnsembleMember(int)));
#endif
        if (updateGUIProperties)
        {
            enableActorUpdates(false);
            synchronizeInitTimeProperty->setEnabled(true);
            synchronizeInitTime = properties->mBool()->value(
                        synchronizeInitTimeProperty);
            synchronizeStartTimeProperty->setEnabled(true);
            synchronizeStartTime = properties->mBool()->value(
                        synchronizeStartTimeProperty);
            synchronizeParticlePosTimeProperty->setEnabled(true);
            synchronizeParticlePosTime = properties->mBool()->value(
                        synchronizeParticlePosTimeProperty);
            synchronizeEnsembleProperty->setEnabled(true);
            synchronizeEnsemble = properties->mBool()->value(
                        synchronizeEnsembleProperty);
            enableActorUpdates(true);
        }

        if (synchronizeInitTime)
        {
            setInitDateTime(sync->initDateTime());
        }
        if (synchronizeStartTime)
        {
            setStartDateTime(sync->validDateTime());
        }
        if (synchronizeParticlePosTime)
        {
            setParticleDateTime(sync->validDateTime());
        }
        if (synchronizeEnsemble)
        {
            setEnsembleMember(sync->ensembleMember());
        }
    }
    else
    {
        // No synchronization. Reset property colours and disable sync
        // checkboxes.
        foreach (MSceneControl* scene, getScenes())
        {
            scene->resetPropertyColour(initTimeProperty);
            scene->resetPropertyColour(startTimeProperty);
            scene->resetPropertyColour(particlePosTimeProperty);
            scene->resetPropertyColour(ensembleMemberProperty);
        }

        if (updateGUIProperties)
        {
            enableActorUpdates(false);
            synchronizeInitTimeProperty->setEnabled(false);
            synchronizeInitTime = false;
            synchronizeStartTimeProperty->setEnabled(false);
            synchronizeStartTime = false;
            synchronizeParticlePosTimeProperty->setEnabled(false);
            synchronizeParticlePosTime = false;
            synchronizeEnsembleProperty->setEnabled(false);
            synchronizeEnsemble = false;
            enableActorUpdates(true);
        }
    }


    // Update "synchronize xyz" GUI properties.
    // ========================================
    if (updateGUIProperties && isInitialized())
    {
        updateTimeProperties();
        updateEnsembleProperties();
    }
}


bool MTrajectoryActor::synchronizationEvent(
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
        enableActorUpdates(false);
        bool newInitTimeSet = setInitDateTime(data.at(0).toDateTime());
        enableActorUpdates(true);
        if (newInitTimeSet)
        {
            asynchronousDataRequest(true);
        }
        return newInitTimeSet;
    }
    case SYNC_VALID_TIME:
    {
        if (!synchronizeStartTime && !synchronizeParticlePosTime)
        {
            return false;
        }
        enableActorUpdates(false);
        bool newStartTimeSet = false;
        bool newParticleTimeSet = false;
        if (synchronizeStartTime)
        {
            newStartTimeSet = setStartDateTime(data.at(0).toDateTime());
        }
        if (synchronizeParticlePosTime)
        {
            newParticleTimeSet = setParticleDateTime(data.at(0).toDateTime());
        }
        enableActorUpdates(true);
        if (newStartTimeSet || newParticleTimeSet)
        {
            asynchronousDataRequest(true);
        }
        return (newStartTimeSet || newParticleTimeSet);
    }
    case SYNC_INIT_VALID_TIME:
    {
        enableActorUpdates(false);
        bool newInitTimeSet = false;
        bool newStartTimeSet = false;
        bool newParticleTimeSet = false;
        if (synchronizeInitTime)
        {
            newInitTimeSet = setInitDateTime(data.at(0).toDateTime());
        }
        if (synchronizeStartTime)
        {
            newStartTimeSet = setStartDateTime(data.at(0).toDateTime());
        }
        if (synchronizeParticlePosTime)
        {
            newParticleTimeSet = setParticleDateTime(data.at(0).toDateTime());
        }
        enableActorUpdates(true);
        if (newInitTimeSet || newStartTimeSet || newParticleTimeSet)
        {
            asynchronousDataRequest(true);
        }
        return (newInitTimeSet || newStartTimeSet || newParticleTimeSet);
    }
    case SYNC_ENSEMBLE_MEMBER:
    {
        if (!synchronizeEnsemble)
        {
            return false;
        }
        enableActorUpdates(false);
        bool newEnsembleMemberSet = setEnsembleMember(data.at(0).toInt());
        enableActorUpdates(true);
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


void MTrajectoryActor::updateSyncPropertyColourHints(MSceneControl *scene)
{
    if (synchronizationControl == nullptr)
    {
        // No synchronization -- reset all property colours.
        setPropertyColour(initTimeProperty, QColor(), true, scene);
        setPropertyColour(startTimeProperty, QColor(), true, scene);
        setPropertyColour(particlePosTimeProperty, QColor(), true, scene);
        setPropertyColour(ensembleMemberProperty, QColor(), true, scene);
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

        match = (getPropertyTime(startTimeProperty)
                 == synchronizationControl->validDateTime());
        colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);
        setPropertyColour(startTimeProperty, colour,
                          !synchronizeStartTime, scene);

        match = (getPropertyTime(particlePosTimeProperty)
                 == synchronizationControl->validDateTime());
        colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);
        setPropertyColour(particlePosTimeProperty, colour,
                          !synchronizeParticlePosTime, scene);

        // Ensemble.
        // =========
        match = (getEnsembleMember()
                 == synchronizationControl->ensembleMember());
        colour = match ? QColor(0, 255, 0) : QColor(255, 0, 0);
        setPropertyColour(ensembleMemberProperty, colour,
                          !synchronizeEnsemble, scene);
    }
}


void MTrajectoryActor::setPropertyColour(
        QtProperty* property, const QColor &colour, bool resetColour,
        MSceneControl *scene)
{
    if (resetColour)
    {
        if (scene == nullptr)
        {
            // Reset colour for all scenes in which the actor appears.
            foreach (MSceneControl* sc, getScenes())
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
            foreach (MSceneControl* sc, getScenes())
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

        // check whether this datasource is precomputed
        precomputedDataSource =
                (dynamic_cast<MTrajectoryCalculator*>(trajectorySource) == nullptr);
        updateActorData();
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
                   this,
                   SLOT(asynchronousSingleTimeSelectionAvailable(MDataRequest)));
    }

    trajectoryFilter = f;
    if (trajectoryFilter != nullptr)
    {
        connect(trajectoryFilter, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(asynchronousSelectionAvailable(MDataRequest)));
        connect(trajectoryFilter, SIGNAL(dataRequestCompleted(MDataRequest)),
                this,
                SLOT(asynchronousSingleTimeSelectionAvailable(MDataRequest)));
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
    // Don't change member if no data source is selected.
    if (dataSourceID == "")
    {
        return false;
    }

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

        // Update background colour of the property in the connected
        // scene's property browser: green if "value" is an
        // exact match with one of the available values, red otherwise.
        if (synchronizeEnsemble)
        {
            bool exactMatch =
                    (member == properties->mInt()->value(ensembleMemberProperty));
            QColor colour = exactMatch ? QColor(0, 255, 0) : QColor(255, 0, 0);
            foreach (MSceneControl* scene, getScenes())
            {
                scene->setPropertyColour(ensembleMemberProperty, colour);
            }
        }


#ifdef DIRECT_SYNCHRONIZATION
        // Does a new data request need to be emitted?
        if (prevEnsembleMode == 1) return true;
        if (prevEnsembleMember != member) return true;
        return false;
#endif
    }

    return false;
}


bool MTrajectoryActor::setStartDateTime(const QDateTime& datetime)
{
    return internalSetDateTime(availableStartTimes, datetime,
                               startTimeProperty);
}


bool MTrajectoryActor::setParticleDateTime(const QDateTime& datetime)
{
    return internalSetDateTime(availableParticlePosTimes, datetime,
                               particlePosTimeProperty);
}


bool MTrajectoryActor::setInitDateTime(const QDateTime& datetime)
{
    return internalSetDateTime(availableInitTimes, datetime, initTimeProperty);
}


void MTrajectoryActor::asynchronousDataAvailable(MDataRequest request)
{
    // See NWPActorVariabe::asynchronousDataAvailable() for explanations
    // on request queue handling.
    // Iterate over all trajectory requests
    for (int t = 0; t < trajectoryRequests.size(); t++)
    {
        bool queueContainsEntryWithNoPendingRequests = false;
        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size(); i++)
        {
            if (trajectoryRequests[t].pendingRequestsQueue[i].dataRequest.request == request)
            {
                // If this is the first time we are informed about the availability
                // of the request (available still == false) decrease number of
                // pending requests.
                if ( !trajectoryRequests[t].pendingRequestsQueue[i].dataRequest.available )
                    trajectoryRequests[t].pendingRequestsQueue[i].numPendingRequests--;

                trajectoryRequests[t].pendingRequestsQueue[i].dataRequest.available = true;

                if (trajectoryRequests[t].pendingRequestsQueue[i].numPendingRequests == 0)
                    queueContainsEntryWithNoPendingRequests = true;

                // Do NOT break the loop here; "request" might be relevant to
                // multiple entries in the queue.
            }
        }

        if (queueContainsEntryWithNoPendingRequests)
        {
            prepareAvailableDataForRendering(t);
        }
    }
}


void MTrajectoryActor::asynchronousNormalsAvailable(MDataRequest request)
{
    for (int t = 0; t < trajectoryRequests.size(); t++)
    {
        bool queueContainsEntryWithNoPendingRequests = false;
        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size(); i++)
        {
            foreach (MSceneViewGLWidget *view, trajectoryRequests[t].pendingRequestsQueue[i].normalsRequests.keys())
            {
                if (trajectoryRequests[t].pendingRequestsQueue[i].normalsRequests[view].request == request)
                {
                    if ( !trajectoryRequests[t].pendingRequestsQueue[i].normalsRequests[view].available )
                        trajectoryRequests[t].pendingRequestsQueue[i].numPendingRequests--;

                    trajectoryRequests[t].pendingRequestsQueue[i].normalsRequests[view].available = true;

                    if (trajectoryRequests[t].pendingRequestsQueue[i].numPendingRequests == 0)
                        queueContainsEntryWithNoPendingRequests = true;

                    // Do NOT break the loop here; "request" might be relevant to
                    // multiple entries in the queue.
                }
            }
        }

        if (queueContainsEntryWithNoPendingRequests)
        {
            prepareAvailableDataForRendering(t);
        }
    }
}


void MTrajectoryActor::asynchronousSelectionAvailable(MDataRequest request)
{
    for (int t = 0; t < trajectoryRequests.size(); t++)
    {
        bool queueContainsEntryWithNoPendingRequests = false;
        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size(); i++)
        {
            if (trajectoryRequests[t].pendingRequestsQueue[i].filterRequest.request == request)
            {
                if (!trajectoryRequests[t].pendingRequestsQueue[i].filterRequest.available)
                    trajectoryRequests[t].pendingRequestsQueue[i].numPendingRequests--;

                trajectoryRequests[t].pendingRequestsQueue[i].filterRequest.available = true;

                if (trajectoryRequests[t].pendingRequestsQueue[i].numPendingRequests == 0)
                    queueContainsEntryWithNoPendingRequests = true;

                // Do NOT break the loop here; "request" might be relevant to
                // multiple entries in the queue.
            }
        }

        if (queueContainsEntryWithNoPendingRequests)
        {
            prepareAvailableDataForRendering(t);
        }
    }
}


void MTrajectoryActor::asynchronousSingleTimeSelectionAvailable(MDataRequest request)
{
    for (int t = 0; t < trajectoryRequests.size(); t++)
    {
        bool queueContainsEntryWithNoPendingRequests = false;
        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size(); i++)
        {
            if (trajectoryRequests[t].pendingRequestsQueue[i].singleTimeFilterRequest.request == request)
            {
                if (!trajectoryRequests[t].pendingRequestsQueue[i].singleTimeFilterRequest.available)
                    trajectoryRequests[t].pendingRequestsQueue[i].numPendingRequests--;

                trajectoryRequests[t].pendingRequestsQueue[i].singleTimeFilterRequest.available = true;

                if (trajectoryRequests[t].pendingRequestsQueue[i].numPendingRequests == 0)
                    queueContainsEntryWithNoPendingRequests = true;

                // Do NOT break the loop here; "request" might be relevant to
                // multiple entries in the queue.
            }
        }

        if (queueContainsEntryWithNoPendingRequests)
        {
            prepareAvailableDataForRendering(t);
        }
    }
}


void MTrajectoryActor::prepareAvailableDataForRendering(uint slot)
{
    // Prepare datafields for rendering as long as they are available in
    // the order in which they were requested.
    while ( ( !trajectoryRequests[slot].pendingRequestsQueue.isEmpty() ) &&
            ( trajectoryRequests[slot].pendingRequestsQueue.head().numPendingRequests == 0 ) )
    {
        MTrajectoryRequestQueueInfo trqi = trajectoryRequests[slot].pendingRequestsQueue.dequeue();

        // 1. Trajectory data.
        // ===================

        if (trqi.dataRequest.available)
        {
            // Release current and get new trajectories.
            if (trajectoryRequests[slot].trajectories)
            {
                trajectoryRequests[slot].trajectories->releaseVertexBuffer();
                trajectorySource->releaseData(trajectoryRequests[slot].trajectories);
            }
            trajectoryRequests[slot].trajectories = trajectorySource->getData(trqi.dataRequest.request);
            trajectoryRequests[slot].trajectoriesVertexBuffer = trajectoryRequests[slot].trajectories->getVertexBuffer();

            // Update displayed information about timestep length.
            float timeStepLength_hours = trajectoryRequests[slot].trajectories->getTimeStepLength_sec() / 3600.;

            properties->mDDouble()->setSingleStep(
                        deltaTimeFilterProperty, timeStepLength_hours);
            properties->mDDouble()->setRange(
                        deltaTimeFilterProperty, timeStepLength_hours,
                        (trajectoryRequests[slot].trajectories->getNumTimeStepsPerTrajectory() - 1)
                        * timeStepLength_hours);

            updateParticlePosTimeProperty();
        }

        // 2. Normals.
        // ===========

        foreach (MSceneViewGLWidget *view, trqi.normalsRequests.keys())
        {
            if (trqi.normalsRequests[view].available)
            {
                if (trajectoryRequests[slot].normals.value(view, nullptr))
                {
                    trajectoryRequests[slot].normals[view]->releaseVertexBuffer();
                    normalsSource->releaseData(trajectoryRequests[slot].normals[view]);
                }
                trajectoryRequests[slot].normals[view] = normalsSource->getData(
                            trqi.normalsRequests[view].request);
                trajectoryRequests[slot].normalsVertexBuffer[view] = trajectoryRequests[slot].normals[view]->getVertexBuffer();
            }
        }

        // 3. Selection.
        // =============

        if (trqi.filterRequest.available)
        {
            if (trajectoryRequests[slot].trajectorySelection)
                trajectoryFilter->releaseData(trajectoryRequests[slot].trajectorySelection);
            trajectoryRequests[slot].trajectorySelection = trajectoryFilter->getData(trqi.filterRequest.request);
        }

        // 4. Single time selection.
        // =========================

        if (trqi.singleTimeFilterRequest.available)
        {
            if (trajectoryRequests[slot].trajectorySingleTimeSelection)
                trajectoryFilter->releaseData(trajectoryRequests[slot].trajectorySingleTimeSelection);
            trajectoryRequests[slot].trajectorySingleTimeSelection = trajectoryFilter->getData(trqi.singleTimeFilterRequest.request);
        }

#ifdef DIRECT_SYNCHRONIZATION
        // If this was a synchronization request signal to the sync control
        // that it has been completed.
        if (trqi.syncchronizationRequest)
            synchronizationControl->synchronizationCompleted(this);
#endif
    }

    // Special case:
    // Since the particle position times depend on the data requested,
    // we need to call asynchronousSelectionRequest() after data request is
    // complete when requesting data from a data source for the first time.
    if (initialDataRequest)
    {
        initialDataRequest = false;
        asynchronousSelectionRequest();
    }
    else
    {
        emitActorChangedSignal();
        updateSyncPropertyColourHints();
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
        properties->mEnum()->setEnumNames(transferFunctionProperty,
                                          availableTFs);
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

        QString tFName = properties->getEnumItem(transferFunctionProperty);
        QStringList availableTFs = properties->mEnum()->enumNames(
                    transferFunctionProperty);

        availableTFs.removeOne(tf->getName());

        // Get the current index of the transfer function selected. If the
        // transfer function is the one to be deleted, the selection is set to
        // 'None'.
        int index = availableTFs.indexOf(tFName);

        properties->mEnum()->setEnumNames(transferFunctionProperty,
                                          availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);

        enableEmissionOfActorChangedSignal(true);
    }

    // check whether the actor is in our seed list
    else
    {
        for (SeedActorSettings& sas : computationSeedActorProperties)
        {
            if (sas.actor == actor)
            {
                removeSeedActor(sas.actor->getName());
            }
        }
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

        properties->mEnum()->setEnumNames(transferFunctionProperty,
                                          availableTFs);
        properties->mEnum()->setValue(transferFunctionProperty, index);

        enableEmissionOfActorChangedSignal(true);
    }

    // check whether the actor is in our seed list
    else
    {
        for (SeedActorSettings& sas : computationSeedActorProperties)
        {
            if (sas.actor == actor)
            {
                sas.propertyGroup->setPropertyName(sas.actor->getName());
            }
        }
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


void MTrajectoryActor::onSceneViewAdded()
{
    // Only send data request if data source exists, but for each scene since
    // they have different scene views and thus different normals.
    if (dataSourceID != "")
    {
        asynchronousDataRequest();
    }
}


bool MTrajectoryActor::isConnectedTo(MActor *actor)
{
    if (MActor::isConnectedTo(actor))
    {
        return true;
    }

    // This actor is connected to the argument actor if the argument actor is
    // the transfer function this actor.
    if (transferFunction == actor)
    {
        return true;
    }

    return false;
}


void MTrajectoryActor::onSeedActorChanged()
{
    if (suppressActorUpdates()) return;

    releaseData();
    updateActorData();

    asynchronousDataRequest();
    emitActorChangedSignal();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTrajectoryActor::initializeActorResources()
{
    // Initialise texture unit.
    if (textureUnitTransferFunction >= 0)
    {
        releaseTextureUnit(textureUnitTransferFunction);
    }
    textureUnitTransferFunction = assignImageUnit();

    // Since no data source was selected disable actor properties since they
    // have no use without a data source.
    if (dataSourceID == "" && !selectDataSource())
    {
        // TODO (bt, May2017): Why does the program crash when calling a message
        // boxes or dialogs during initialisation of GL?
        bool appInitialized =
                MSystemManagerAndControl::getInstance()->applicationIsInitialized();
        if (appInitialized)
        {
            // User has selected no data source. Display a warning and disable
            // all trajectory properties.
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setWindowTitle(getName());
            msgBox.setText("No data source selected. Disabling all properties.");
            msgBox.exec();
        }
        enableProperties(false);
    }
    else
    {
        enableProperties(true);

        updateInitTimeProperty();
        updateStartTimeProperty();

        // Get values from sync control, if connected to one.
        if (synchronizationControl == nullptr)
        {
            synchronizeInitTimeProperty->setEnabled(false);
            synchronizeInitTime = false;
            synchronizeStartTimeProperty->setEnabled(false);
            synchronizeStartTime = false;
            synchronizeParticlePosTimeProperty->setEnabled(false);
            synchronizeParticlePosTime = false;
            synchronizeEnsembleProperty->setEnabled(false);
            synchronizeEnsemble = false;
        }
        else
        {
            // Set synchronizationControl to nullptr and sync with its
            // original value since otherwise the synchronisation won't
            // be executed.
            MSyncControl *temp = synchronizationControl;
            synchronizationControl = nullptr;
            synchronizeWith(temp);
        }

        //update actor data
        updateActorData();

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
            updateStartTimeProperty();
            // Synchronise with synchronisation control if available.
            if (synchronizationControl != nullptr)
            {
                // Set synchronizationControl to nullptr and sync with its
                // original value since otherwise the synchronisation won't
                // be executed.
                MSyncControl *temp = synchronizationControl;
                synchronizationControl = nullptr;
                synchronizeWith(temp);
            }
            asynchronousDataRequest();
        }
    }

    else if (property == utilizedDataSourceProperty)
    {
        dataSourceID = properties->mString()->value(utilizedDataSourceProperty);
        bool validDataSource =
                MSelectDataSourceDialog::checkForTrajectoryDataSource(
                    dataSourceID);
        if (!validDataSource)
        {
            dataSourceID = "";
        }
        else
        {
            initialDataRequest = true;
        }
        return;
    }

    // Connect to the time signals of the selected scene.
    else if (property == synchronizationProperty)
    {
        if (suppressActorUpdates()) return;

        MSystemManagerAndControl *sysMC =
                MSystemManagerAndControl::getInstance();
        QString syncID = properties->getEnumItem(synchronizationProperty);
        synchronizeWith(sysMC->getSyncControl(syncID));

        return;
    }

    else if (property == synchronizeInitTimeProperty)
    {
        synchronizeInitTime = properties->mBool()->value(
                    synchronizeInitTimeProperty);
        updateTimeProperties();

        if (suppressActorUpdates()) return;

        if (synchronizeInitTime && synchronizationControl != nullptr)
        {
            if (setInitDateTime(synchronizationControl->initDateTime()))
            {
                asynchronousDataRequest();
            }
        }

        return;
    }

    else if (property == synchronizeStartTimeProperty)
    {
        synchronizeStartTime = properties->mBool()->value(
                    synchronizeStartTimeProperty);
        updateTimeProperties();

        if (suppressActorUpdates()) return;

        if (synchronizeStartTime && synchronizationControl != nullptr)
        {
            if (setStartDateTime(synchronizationControl->validDateTime()))
            {
                asynchronousDataRequest();
            }
        }

        return;
    }

    else if (property == synchronizeParticlePosTimeProperty)
    {
        synchronizeParticlePosTime = properties->mBool()->value(
                    synchronizeParticlePosTimeProperty);
        updateTimeProperties();

        if (suppressActorUpdates()) return;

        if (synchronizeParticlePosTime && synchronizationControl != nullptr)
        {
            if (setParticleDateTime(synchronizationControl->validDateTime()))
            {
                asynchronousDataRequest();
            }
        }

        return;
    }

    else if (property == synchronizeEnsembleProperty)
    {
        synchronizeEnsemble = properties->mBool()->value(
                    synchronizeEnsembleProperty);
        updateEnsembleProperties();
        updateSyncPropertyColourHints();

        if (suppressActorUpdates()) return;

        if (synchronizeEnsemble && synchronizationControl != nullptr)
        {
            if (setEnsembleMember(synchronizationControl->ensembleMember()))
            {
                asynchronousDataRequest();
            }
        }

        return;
    }

    else if (property == ensembleMemberProperty)
    {
//        updateSyncPropertyColourHints();
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == enableAscentFilterProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == deltaPressureFilterProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == deltaTimeFilterProperty)
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
            particlePosTimeProperty->setEnabled(false);
            break;
        case SINGLETIME_POSITIONS:
        case TUBES_AND_SINGLETIME:
        case BACKWARDTUBES_AND_SINGLETIME:
            particlePosTimeProperty->setEnabled(!synchronizeParticlePosTime);
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

    // The init time has been changed. Reload start times.
    else if (property == initTimeProperty)
    {
        if (suppressActorUpdates()) return;
        updateStartTimeProperty();

        asynchronousDataRequest();
    }

    else if (property == startTimeProperty)
    {
        if (suppressUpdate) return; // ignore if init times are being updated
        if (suppressActorUpdates()) return;
        updateDeltaTimeProperty();
        asynchronousDataRequest();
    }

    else if (property == particlePosTimeProperty)
    {
        particlePosTimeStep = properties->mEnum()->value(
                    particlePosTimeProperty);

        if (suppressUpdate) return;
        if (suppressActorUpdates()) return;
        asynchronousSelectionRequest();
    }

    else if (property == computationLineTypeProperty)
    {
        TRAJ_CALC_LINE_TYPE lineType = TRAJ_CALC_LINE_TYPE(properties->mEnum()->value(computationLineTypeProperty));
        switch (lineType)
        {
            case PATH_LINE:
                computationIntegrationTimeProperty->setEnabled(true);
                break;
            case STREAM_LINE:
                computationIntegrationTimeProperty->setEnabled(false);
                break;
        }
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == computationIntegrationTimeProperty)
    {
        if (suppressUpdate) return; // ignore if init times are being updated
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == computationIterationCountProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == computationIntegrationMethodProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == computationInterpolationMethodProperty)
    {
        if (suppressActorUpdates()) return;
        asynchronousDataRequest();
    }

    else if (property == computationSeedAddActorProperty)
    {
        if (suppressActorUpdates()) return;
        openSeedActorDialog();

        releaseData();
        updateActorData();

        asynchronousDataRequest();
        emitActorChangedSignal();
    }
    else
    {
        for (SeedActorSettings& sas : computationSeedActorProperties)
        {
            if ( property == sas.removeProperty)
            {
                if (suppressActorUpdates())
                {
                    return;
                }

                QMessageBox::StandardButton reply = QMessageBox::question(
                            nullptr,
                            "Trajectory Actor",
                            "Are you sure you want to remove '"
                            + sas.actor->getName() + "'?",
                            QMessageBox::Yes|QMessageBox::No,
                            QMessageBox::No);

                if (reply == QMessageBox::Yes)
                {
                    removeSeedActor(sas.actor->getName());

                    releaseData();
                    updateActorData();

                    asynchronousDataRequest();
                    emitActorChangedSignal();
                }
            }
            else if (property == sas.lonSpacing || property == sas.latSpacing
                     || property == sas.pressureLevels)
            {
                if (suppressActorUpdates()) return;

                updateActorData();

                asynchronousDataRequest();
            }
        }
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
        for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
        {
            // If any required data item is missing we cannot render.
            if ( (trajectoryRequests[t].trajectories == nullptr)
                 || (trajectoryRequests[t].normals[sceneView] == nullptr)
                 || (trajectoryRequests[t].trajectorySelection == nullptr) ) continue;

            // If the vertical scaling of the view has changed, a recomputation of
            // the normals is necessary, as they are based on worldZ coordinates.
            if (sceneView->visualisationParametersHaveChanged())
            {
                // Discard old normals.
                if (trajectoryRequests[t].normals.value(sceneView, nullptr))
                    normalsSource->releaseData(trajectoryRequests[t].normals[sceneView]);

                trajectoryRequests[t].normals[sceneView] = nullptr;
                continue;
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
                    trajectoryRequests[t].trajectories->getNumTimeStepsPerTrajectory());

            if (renderMode == BACKWARDTUBES_AND_SINGLETIME)
            {
                tubeShader->setUniformValue(
                        "renderTubesUpToIndex",
                        particlePosTimeStep);
            }
            else
            {
                tubeShader->setUniformValue(
                        "renderTubesUpToIndex",
                        trajectoryRequests[t].trajectories->getNumTimeStepsPerTrajectory());
            }

            // Texture bindings for transfer function for data scalar (1D texture
            // from transfer function class). The data scalar is stored in the
            // vertex.w component passed to the vertex shader.
            transferFunction->getTexture()->bindToTextureUnit(
                    textureUnitTransferFunction);
            tubeShader->setUniformValue(
                    "transferFunction", textureUnitTransferFunction);
            tubeShader->setUniformValue(
                    "scalarMinimum", transferFunction->getMinimumValue());
            tubeShader->setUniformValue(
                    "scalarMaximum", transferFunction->getMaximumValue());

            // Bind trajectories and normals vertex buffer objects.
            trajectoryRequests[t].trajectoriesVertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

            trajectoryRequests[t].normalsVertexBuffer[sceneView]->attachToVertexAttribute(SHADER_NORMAL_ATTRIBUTE);

            glPolygonMode(GL_FRONT_AND_BACK,
                          renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
            glLineWidth(1); CHECK_GL_ERROR;

            glMultiDrawArrays(GL_LINE_STRIP_ADJACENCY,
                              trajectoryRequests[t].trajectorySelection->getStartIndices(),
                              trajectoryRequests[t].trajectorySelection->getIndexCount(),
                              trajectoryRequests[t].trajectorySelection->getNumTrajectories());
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
                        trajectoryRequests[t].trajectories->getNumTimeStepsPerTrajectory());

                if (renderMode == BACKWARDTUBES_AND_SINGLETIME)
                {
                    tubeShadowShader->setUniformValue(
                            "renderTubesUpToIndex",
                            particlePosTimeStep);
                }
                else
                {
                    tubeShadowShader->setUniformValue(
                            "renderTubesUpToIndex",
                            trajectoryRequests[t].trajectories->getNumTimeStepsPerTrajectory());
                }

                tubeShadowShader->setUniformValue(
                        "useTransferFunction", GLboolean(shadowColoured));

                if (shadowColoured)
                {
                    tubeShadowShader->setUniformValue(
                            "transferFunction", textureUnitTransferFunction);
                    tubeShadowShader->setUniformValue(
                            "scalarMinimum",
                            transferFunction->getMinimumValue());
                    tubeShadowShader->setUniformValue(
                            "scalarMaximum",
                            transferFunction->getMaximumValue());
                }
                else
                    tubeShadowShader->setUniformValue(
                            "constColour", QColor(20, 20, 20, 155));

                glMultiDrawArrays(GL_LINE_STRIP_ADJACENCY,
                                  trajectoryRequests[t].trajectorySelection->getStartIndices(),
                                  trajectoryRequests[t].trajectorySelection->getIndexCount(),
                                  trajectoryRequests[t].trajectorySelection->getNumTrajectories());
                CHECK_GL_ERROR;
            }

            // Unbind VBO.
            glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
        }
    }


    if ( (renderMode == ALL_POSITION_SPHERES)
         || (renderMode == SINGLETIME_POSITIONS)
         || (renderMode == TUBES_AND_SINGLETIME)
         || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
    {
        for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
        {
            if (trajectoryRequests[t].trajectories == nullptr) continue;

            if (renderMode == ALL_POSITION_SPHERES)
            {
                if (trajectoryRequests[t].trajectorySelection == nullptr) continue;
            }
            else
            {
                if (trajectoryRequests[t].trajectorySingleTimeSelection == nullptr) continue;
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


            // Texture bindings for transfer function for data scalar (1D texture
            // from transfer function class). The data scalar is stored in the
            // vertex.w component passed to the vertex shader.
            transferFunction->getTexture()->bindToTextureUnit(
                    textureUnitTransferFunction);
            positionSphereShader->setUniformValue(
                    "useTransferFunction", GLboolean(true));
            positionSphereShader->setUniformValue(
                    "transferFunction", textureUnitTransferFunction);
            positionSphereShader->setUniformValue(
                    "scalarMinimum", transferFunction->getMinimumValue());
            positionSphereShader->setUniformValue(
                    "scalarMaximum", transferFunction->getMaximumValue());

            // Bind vertex buffer object.
            trajectoryRequests[t].trajectoriesVertexBuffer
                    ->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

            glPolygonMode(GL_FRONT_AND_BACK,
                          renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
            glLineWidth(1); CHECK_GL_ERROR;

            if (renderMode == ALL_POSITION_SPHERES)
                glMultiDrawArrays(GL_POINTS,
                                  trajectoryRequests[t].trajectorySelection->getStartIndices(),
                                  trajectoryRequests[t].trajectorySelection->getIndexCount(),
                                  trajectoryRequests[t].trajectorySelection->getNumTrajectories());
            else
                glMultiDrawArrays(GL_POINTS,
                                  trajectoryRequests[t].trajectorySingleTimeSelection->getStartIndices(),
                                  trajectoryRequests[t].trajectorySingleTimeSelection->getIndexCount(),
                                  trajectoryRequests[t].trajectorySingleTimeSelection->getNumTrajectories());
            CHECK_GL_ERROR;


            if (shadowEnabled)
            {
                positionSphereShadowShader->bind();

                positionSphereShadowShader->setUniformValue(
                        "mvpMatrix",
                        *(sceneView->getModelViewProjectionMatrix()));
                CHECK_GL_ERROR;
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
                        "useTransferFunction",
                        GLboolean(shadowColoured)); CHECK_GL_ERROR;

                if (shadowColoured)
                {
                    // Transfer function texture is still bound from the sphere
                    // shader.
                    positionSphereShadowShader->setUniformValue(
                            "transferFunction",
                            textureUnitTransferFunction); CHECK_GL_ERROR;
                    positionSphereShadowShader->setUniformValue(
                            "scalarMinimum",
                            transferFunction->getMinimumValue()); CHECK_GL_ERROR;
                    positionSphereShadowShader->setUniformValue(
                            "scalarMaximum",
                            transferFunction->getMaximumValue()); CHECK_GL_ERROR;

                }
                else
                    positionSphereShadowShader->setUniformValue(
                            "constColour", QColor(20, 20, 20, 155)); CHECK_GL_ERROR;

                if (renderMode == ALL_POSITION_SPHERES)
                    glMultiDrawArrays(GL_POINTS,
                                      trajectoryRequests[t].trajectorySelection->getStartIndices(),
                                      trajectoryRequests[t].trajectorySelection->getIndexCount(),
                                      trajectoryRequests[t].trajectorySelection->getNumTrajectories());
                else
                    glMultiDrawArrays(GL_POINTS,
                                      trajectoryRequests[t].trajectorySingleTimeSelection->getStartIndices(),
                                      trajectoryRequests[t].trajectorySingleTimeSelection->getIndexCount(),
                                      trajectoryRequests[t].trajectorySingleTimeSelection->getNumTrajectories());
                CHECK_GL_ERROR;
            }

            // Unbind VBO.
            glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
        }
    }
}


void MTrajectoryActor::updateTimeProperties()
{
    enableActorUpdates(false);

    bool enableSync = synchronizationControl != nullptr;

    initTimeProperty->setEnabled(!(enableSync && synchronizeInitTime));
    startTimeProperty->setEnabled(!(enableSync && synchronizeStartTime));
    particlePosTimeProperty->setEnabled(
                !(enableSync && synchronizeParticlePosTime));

    updateSyncPropertyColourHints();

    enableActorUpdates(true);
}


void MTrajectoryActor::updateEnsembleProperties()
{
    enableActorUpdates(false);

    // If the ensemble is synchronized, disable all properties (they are set
    // via the synchronization control).
    ensembleMemberProperty->setEnabled(!synchronizeEnsemble);

    enableActorUpdates(true);
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MTrajectoryActor::updateActorData()
{
    seedActorData.clear();
    for (SeedActorSettings& sas : computationSeedActorProperties)
    {
        if (!sas.actor->isEnabled())
            continue;

        double stepSizeLon = properties->mDouble()->value(sas.lonSpacing);
        double stepSizeLat = properties->mDouble()->value(sas.latSpacing);
        QVector<float> pressureLevels = parsePressureLevelString(properties->mString()->value(sas.pressureLevels));

        switch (sas.type)
        {
        case POLE_ACTOR:
        {
            MMovablePoleActor *actor =
                    dynamic_cast<MMovablePoleActor *>(sas.actor);

            // every pole has 2 vertices for bot and top
            for (int i = 0; i < actor->getPoleVertices().size(); i += 2)
            {
                QVector3D bot = actor->getPoleVertices().at(i);
                QVector3D top = actor->getPoleVertices().at(i + 1);

                SeedActorData data;
                data.type = POLE;
                data.minPosition = QVector3D(top.x(), top.y(), top.z());
                data.maxPosition = QVector3D(bot.x(), bot.y(), bot.z());
                data.stepSize = QVector2D(stepSizeLon, stepSizeLat);
                data.pressureLevels.clear();
                for (float f : pressureLevels)
                {
                    if (bot.z() >= f && f >= top.z())
                        data.pressureLevels << f;
                }
                seedActorData.push_back(data);
            }
            break;
        }
        case HORIZONTAL_ACTOR:
        {
            MNWPHorizontalSectionActor *actor =
                    dynamic_cast<MNWPHorizontalSectionActor *>(sas.actor);

            if (actor->getBoundingBoxName() == "None")
            {
                continue;
            }

            QRectF hor = actor->getHorizontalBBox();
            double p = actor->getSlicePosition();

            SeedActorData data;
            data.type = HORIZONTAL;
            data.minPosition = QVector3D(hor.x(), hor.y(), p);
            data.maxPosition = QVector3D(hor.x() + hor.width(), hor.y() + hor.width(), p);
            data.stepSize = QVector2D(stepSizeLon, stepSizeLat);
            data.pressureLevels.clear();
            data.pressureLevels << p;
            seedActorData.push_back(data);
            break;
        }
        case VERTICAL_ACTOR:
        {
            MNWPVerticalSectionActor *actor =
                    dynamic_cast<MNWPVerticalSectionActor *>(sas.actor);

            if (actor->getBoundingBoxName() == "None")
            {
                continue;
            }

            if (!actor->getWaypointsModel())
                continue;

            for (int i = 0; i < actor->getWaypointsModel()->size() - 1; i++)
            {
                QVector2D point1 = actor->getWaypointsModel()->positionLonLat(i);
                QVector2D point2 = actor->getWaypointsModel()->positionLonLat(i + 1);
                double pbot = actor->getBottomPressure();
                double ptop = actor->getTopPressure();

                SeedActorData data;
                data.type = VERTICAL;
                data.minPosition = QVector3D(point1.x(), point1.y(), ptop);
                data.maxPosition = QVector3D(point2.x(), point2.y(), pbot);
                data.stepSize = QVector2D(stepSizeLon, stepSizeLat);
                data.pressureLevels.clear();
                for (float f : pressureLevels)
                {
                    if (pbot >= f && f >= ptop)
                        data.pressureLevels << f;
                }

                seedActorData.push_back(data);
            }
            break;
        }
        case BOX_ACTOR:
        {
            MVolumeBoundingBoxActor *actor =
                    dynamic_cast<MVolumeBoundingBoxActor *>(sas.actor);

            if (actor->getBoundingBoxName() == "None")
            {
                continue;
            }

            QRectF hor = actor->getHorizontalBBox();
            double pbot = actor->getBottomPressure();
            double ptop = actor->getTopPressure();

            SeedActorData data;
            data.type = BOX;
            data.minPosition = QVector3D(hor.x(), hor.y(), ptop);
            data.maxPosition = QVector3D(hor.x() + hor.width(), hor.y() + hor.width(), pbot);
            data.stepSize = QVector2D(stepSizeLon, stepSizeLat);
            data.pressureLevels.clear();
            for (float f : pressureLevels)
            {
                if (pbot >= f && f >= ptop)
                    data.pressureLevels << f;
            }
            seedActorData.push_back(data);
            break;
        }
        }
    }

    int size = precomputedDataSource ? 1 : seedActorData.size();

    // resize trajectory Request vector containing all buffers
    trajectoryRequests.resize(size);
}


QDateTime MTrajectoryActor::getPropertyTime(QtProperty *enumProperty)
{
    QStringList dateStrings = properties->mEnum()->enumNames(enumProperty);

    // If the list of date strings is empty return an invalid null time.
    if (dateStrings.empty())
    {
        return QDateTime();
    }

    int index = properties->mEnum()->value(enumProperty);
    return QDateTime::fromString(dateStrings.at(index), Qt::ISODate);
}


int MTrajectoryActor::getEnsembleMember()
{
    return properties->mInt()->value(ensembleMemberProperty);
//    QString memberString =
//            getQtProperties()->getEnumItem(ensembleMemberProperty);

//    bool ok = true;
//    int member = memberString.toInt(&ok);

//    if (ok) return member; else return -99999;
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
    // No computations necessary if trajectories are not displayed. (Besides
    // data requests not needed lead to predefined trajectory actor not being
    // displayed and system crash due to waiting for a unfinished thread at
    // program end.)
    if (getViews().size() == 0)
    {
        return;
    }

#ifndef DIRECT_SYNCHRONIZATION
    Q_UNUSED(synchronizationRequest);
#endif

    for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
    {
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
        QDateTime validTime = getPropertyTime(startTimeProperty);
        unsigned int member = properties->mInt()->value(ensembleMemberProperty);

        MDataRequestHelper rh;
        rh.insert("INIT_TIME", initTime);
        rh.insert("VALID_TIME", validTime);
        rh.insert("MEMBER", member);
        rh.insert("TIME_SPAN", "ALL");

        // if computed dataSource is used, additional information is needed
        if (!precomputedDataSource)
        {
            QDateTime endTime = availableStartTimes.at(properties->mEnum()->value(computationIntegrationTimeProperty));
            unsigned int lineType = properties->mEnum()->value(computationLineTypeProperty);
            unsigned int iterationMethod = properties->mEnum()->value(computationIntegrationMethodProperty);
            unsigned int interpolationMethod = properties->mEnum()->value(computationInterpolationMethodProperty);
            unsigned int iterationCount = properties->mInt()->value(computationIterationCountProperty);
            unsigned int seedType = seedActorData[t].type;
            QString seedMinPosition = QString("%1/%2/%3")
                    .arg(seedActorData[t].minPosition.x())
                    .arg(seedActorData[t].minPosition.y())
                    .arg(seedActorData[t].minPosition.z());
            QString seedMaxPosition = QString("%1/%2/%3")
                    .arg(seedActorData[t].maxPosition.x())
                    .arg(seedActorData[t].maxPosition.y())
                    .arg(seedActorData[t].maxPosition.z());
            QString seedStepSize = QString("%1/%2")
                    .arg(seedActorData[t].stepSize.x())
                    .arg(seedActorData[t].stepSize.y());
            QString seedPressureLevels = encodePressureLevels(seedActorData[t].pressureLevels, QString("/"));

            // insert additional infos to request
            rh.insert("END_TIME", endTime);
            rh.insert("LINE_TYPE", lineType);
            rh.insert("ITERATION_PER_TIMESTEP", iterationCount);
            rh.insert("ITERATION_METHOD", iterationMethod);
            rh.insert("INTERPOLATION_METHOD", interpolationMethod);
            rh.insert("SEED_TYPE", seedType);
            rh.insert("SEED_MIN_POSITION", seedMinPosition);
            rh.insert("SEED_MAX_POSITION", seedMaxPosition);
            rh.insert("SEED_STEP_SIZE_LON_LAT", seedStepSize);
            rh.insert("SEED_PRESSURE_LEVELS", seedPressureLevels);
        }

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

        bool filteringEnabled = properties->mBool()->value(enableAscentFilterProperty);
        if (filteringEnabled)
        {
            float deltaPressure_hPa = properties->mDDouble()->value(
                        deltaPressureFilterProperty);
            int deltaTime_hrs =
                    properties->mDDouble()->value(deltaTimeFilterProperty);
            // Request is e.g. 500/48 for 500 hPa in 48 hours.
            rh.insert("FILTER_PRESSURE_TIME",
                      QString("%1/%2").arg(deltaPressure_hPa).arg(deltaTime_hrs));
            // Request bounding box filtering.
            if (bBoxConnection->getBoundingBox() != nullptr)
            {
                rh.insert("FILTER_BBOX", QString("%1/%2/%3/%4")
                          .arg(bBoxConnection->westLon()).arg(
                              bBoxConnection->southLat())
                          .arg(bBoxConnection->eastLon()).arg(
                              bBoxConnection->northLat()));
            }
        }
        else
        {
            rh.insert("FILTER_PRESSURE_TIME", "ALL");
        }

        if ((renderMode == SINGLETIME_POSITIONS)
            || (renderMode == TUBES_AND_SINGLETIME)
            || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
        {
            rh.insert("FILTER_TIMESTEP", QString("%1").arg(particlePosTimeStep));
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
        trajectoryRequests[t].pendingRequestsQueue.enqueue(trqi);
//        debugPrintPendingRequestsQueue();

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
}


void MTrajectoryActor::asynchronousSelectionRequest()
{
    for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
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
        QDateTime validTime = getPropertyTime(startTimeProperty);
        unsigned int member = properties->mInt()->value(ensembleMemberProperty);

        MDataRequestHelper rh;
        rh.insert("INIT_TIME", initTime);
        rh.insert("VALID_TIME", validTime);
        rh.insert("MEMBER", member);
        rh.insert("TIME_SPAN", "ALL");
        //TODO: add property
        rh.insert("TRY_PRECOMPUTED", 1);

        // if computed dataSource is used, additional information is needed
        if (!precomputedDataSource)
        {
            int endTimeIndex =
                    properties->mEnum()->value(computationIntegrationTimeProperty);
            if (endTimeIndex < 0)
            {
                continue;
            }
            QDateTime endTime = availableStartTimes.at(max(0, endTimeIndex));
            unsigned int lineType =
                    properties->mEnum()->value(computationLineTypeProperty);
            unsigned int iterationMethod = properties->mEnum()->value(
                        computationIntegrationMethodProperty);
            unsigned int interpolatonMethod = properties->mEnum()->value(
                        computationInterpolationMethodProperty);
            unsigned int iterationCount = properties->mInt()->value(
                        computationIterationCountProperty);
            unsigned int seedType = seedActorData[t].type;
            QString seedMinPosition = QString("%1/%2/%3")
                    .arg(seedActorData[t].minPosition.x())
                    .arg(seedActorData[t].minPosition.y())
                    .arg(seedActorData[t].minPosition.z());
            QString seedMaxPosition = QString("%1/%2/%3")
                    .arg(seedActorData[t].maxPosition.x())
                    .arg(seedActorData[t].maxPosition.y())
                    .arg(seedActorData[t].maxPosition.z());
            QString seedStepSize = QString("%1/%2")
                    .arg(seedActorData[t].stepSize.x())
                    .arg(seedActorData[t].stepSize.y());
            QString seedPressureLevels = encodePressureLevels(
                        seedActorData[t].pressureLevels, QString("/"));

            // insert additional infos to request
            rh.insert("END_TIME", endTime);
            rh.insert("LINE_TYPE", lineType);
            rh.insert("ITERATION_PER_TIMESTEP", iterationCount);
            rh.insert("ITERATION_METHOD", iterationMethod);
            rh.insert("INTERPOLATION_METHOD", interpolatonMethod);
            rh.insert("SEED_TYPE", seedType);
            rh.insert("SEED_MIN_POSITION", seedMinPosition);
            rh.insert("SEED_MAX_POSITION", seedMaxPosition);
            rh.insert("SEED_STEP_SIZE_LON_LAT", seedStepSize);
            rh.insert("SEED_PRESSURE_LEVELS", seedPressureLevels);
        }

        // Filter the trajectories of this member according to the specified
        // pressure interval (xx hPa over the "lifetime" of the trajectories;
        // e.g. for T-NAWDEX over 48 hours).

        bool filteringEnabled = properties->mBool()->value(enableAscentFilterProperty);
        if (filteringEnabled)
        {
            float deltaPressure_hPa = properties->mDDouble()->value(
                    deltaPressureFilterProperty);
            int deltaTime_hrs = properties->mDDouble()->value(
                        deltaTimeFilterProperty);
            // Request is e.g. 500/48 for 500 hPa in 48 hours.
            rh.insert("FILTER_PRESSURE_TIME",
                      QString("%1/%2").arg(deltaPressure_hPa).arg(deltaTime_hrs));
            // Request bounding box filtering.
            if (bBoxConnection->getBoundingBox() != nullptr)
            {
                rh.insert("FILTER_BBOX", QString("%1/%2/%3/%4")
                      .arg(bBoxConnection->westLon()).arg(
                              bBoxConnection->southLat())
                      .arg(bBoxConnection->eastLon()).arg(
                              bBoxConnection->northLat()));
            }
        }
        else
        {
            rh.insert("FILTER_PRESSURE_TIME", "ALL");
        }

        if ((renderMode == SINGLETIME_POSITIONS)
            || (renderMode == TUBES_AND_SINGLETIME)
            || (renderMode == BACKWARDTUBES_AND_SINGLETIME))
        {
            rh.insert("FILTER_TIMESTEP", QString("%1").arg(particlePosTimeStep));
            trqi.singleTimeFilterRequest.request = rh.request();
            trqi.numPendingRequests++;
        }

        if (renderMode != SINGLETIME_POSITIONS)
        {
            rh.insert("FILTER_TIMESTEP", "ALL");
            trqi.filterRequest.request = rh.request();
            trqi.numPendingRequests++;
        }

        trajectoryRequests[t].pendingRequestsQueue.enqueue(trqi);

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
        {
            timeStrings << availableInitTimes.at(i).toString(Qt::ISODate);
        }

        properties->mEnum()->setEnumNames(initTimeProperty, timeStrings);

        int newIndex = max(0, availableInitTimes.indexOf(initTime));
        properties->mEnum()->setValue(initTimeProperty, newIndex);
        if (synchronizeInitTime && synchronizationControl != nullptr)
        {
            setInitDateTime(synchronizationControl->initDateTime());
        }
    }

    suppressUpdate = false;
}


void MTrajectoryActor::updateStartTimeProperty()
{
    suppressUpdate = true;

    if (trajectorySource == nullptr)
    {
        properties->mEnum()->setEnumNames(startTimeProperty, QStringList());
    }
    else
    {
        // Get the current time values.
        QDateTime initTime  = getPropertyTime(initTimeProperty);
        QDateTime startTime = getPropertyTime(startTimeProperty);

        // Get a list of the available start times for the new init time,
        // convert the QDateTime objects to strings for the enum manager.
        availableStartTimes = trajectorySource->availableValidTimes(initTime);
        QStringList startTimeStrings;
        for (int i = 0; i < availableStartTimes.size(); i++)
            startTimeStrings << availableStartTimes.at(i).toString(Qt::ISODate);

        properties->mEnum()->setEnumNames(startTimeProperty, startTimeStrings);

        int newIndex = max(0, availableStartTimes.indexOf(startTime));
        properties->mEnum()->setValue(startTimeProperty, newIndex);
        if (synchronizeStartTime && synchronizationControl != nullptr)
        {
            setStartDateTime(synchronizationControl->validDateTime());
        }
    }

    updateDeltaTimeProperty();

    suppressUpdate = false;
}


void MTrajectoryActor::updateParticlePosTimeProperty()
{
    suppressUpdate = true;

    // all trajectories have the exact same type, find any initialized source
    int index = -1;
    for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
    {
        if (trajectoryRequests[t].trajectories != nullptr)
        {
            index = t;
            break;
        }
    }

    // no initialized trajectory found
    if (index < 0)
    {
        properties->mEnum()->setEnumNames(particlePosTimeProperty, QStringList());
    }
    else
    {
        QDateTime currentValue = getPropertyTime(particlePosTimeProperty);

        availableParticlePosTimes = trajectoryRequests[index].trajectories->getTimes().toList();
        QStringList particlePosTimeStrings;
        for (int i = 0; i < trajectoryRequests[index].trajectories->getTimes().size(); i++)
        {
            particlePosTimeStrings
                    << trajectoryRequests[index].trajectories->getTimes().at(i).toString(Qt::ISODate);
        }

        properties->mEnum()->setEnumNames(particlePosTimeProperty,
                                          particlePosTimeStrings);

        // Try to restore previous time value. If the previous value is not
        // available for the new trajectories, indexOf() returns -1. This is
        // changed to 0, i.e. the first available time value is selected.
        int newIndex = max(0, trajectoryRequests[index].trajectories->getTimes().indexOf(currentValue));
        properties->mEnum()->setValue(particlePosTimeProperty, newIndex);

        // The trajectory time property is not needed when the entire
        // trajectories are rendered.
        switch (renderMode)
        {
        case TRAJECTORY_TUBES:
        case ALL_POSITION_SPHERES:
            particlePosTimeProperty->setEnabled(false);
            break;
        case SINGLETIME_POSITIONS:
        case TUBES_AND_SINGLETIME:
        case BACKWARDTUBES_AND_SINGLETIME:
            particlePosTimeProperty->setEnabled(!synchronizeParticlePosTime);
            break;
        }
        if (synchronizeParticlePosTime && synchronizationControl != nullptr)
        {
            setParticleDateTime(synchronizationControl->validDateTime());
        }
    }

    suppressUpdate = false;
}


void MTrajectoryActor::updateDeltaTimeProperty()
{
    suppressUpdate = true;

    if (trajectorySource == nullptr || precomputedDataSource)
    {
        properties->mEnum()->setEnumNames(computationIntegrationTimeProperty, QStringList());
    }
    else
    {
        // Get the current time values.
        QDateTime initTime  = getPropertyTime(initTimeProperty);
        QDateTime startTime = getPropertyTime(startTimeProperty);

        // get available timesteps and add time delta to list (ignore currently selected start Time)
        QStringList endTimes;
        for (QDateTime& time : availableStartTimes)
        {
           endTimes << QString("%1 hrs").arg(startTime.secsTo(time) / 3600.0);
        }

        // set new list to property
        int currentIndex = properties->mEnum()->value(computationIntegrationTimeProperty);
        properties->mEnum()->setEnumNames(computationIntegrationTimeProperty, endTimes);

        currentIndex = currentIndex < 0 ? (availableStartTimes.size() - 1) : currentIndex;
        properties->mEnum()->setValue(computationIntegrationTimeProperty, currentIndex);
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
    while (i < availableTimes.size() - 1)
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
            {
                getScenes().at(i)->setPropertyColour(timeProperty, colour);
            }
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

    for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
    {
        if (!precomputedDataSource)
            str += QString("Seed position #%1").arg(t);

        for (int i = 0; i < trajectoryRequests[t].pendingRequestsQueue.size(); i++)
        {
            str += QString("Entry #%1 [%2]\n[%3] %4\n[%5] %6\n[%7] %8\n")
                    .arg(i).arg(trajectoryRequests[t].pendingRequestsQueue[i].numPendingRequests)
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i].dataRequest.available)
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i].dataRequest.request)
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i].filterRequest.available)
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i].filterRequest.request)
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i].singleTimeFilterRequest.available)
                    .arg(trajectoryRequests[t].pendingRequestsQueue[i].singleTimeFilterRequest.request);

                    foreach (MSceneViewGLWidget *view,
                             trajectoryRequests[t].pendingRequestsQueue[i].normalsRequests.keys())
                {
                    str += QString("[%1] %2\n")
                            .arg(trajectoryRequests[t].pendingRequestsQueue[i].normalsRequests[view].available)
                            .arg(trajectoryRequests[t].pendingRequestsQueue[i].normalsRequests[view].request);
                }
        }
    }

    str += QString("\n==================\n");

    LOG4CPLUS_DEBUG(mlog, str.toStdString());
}


bool MTrajectoryActor::selectDataSource()
{
    // TODO (bt, May2017): Why does the program crash when calling a message
    // boxes or dialogs during initialisation of GL?
    bool appInitialized =
            MSystemManagerAndControl::getInstance()->applicationIsInitialized();
    if (!appInitialized)
    {
        return false;
    }
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

        updateInitTimeProperty();
        updateStartTimeProperty();

        return true;
    }

    return true;
}


void MTrajectoryActor::openSeedActorDialog()
{
    const QList<MSelectActorType> types = {MSelectActorType::POLE_ACTOR, MSelectActorType::HORIZONTAL_ACTOR, MSelectActorType::VERTICAL_ACTOR, MSelectActorType::BOX_ACTOR};
    MSelectActorDialog dialog(types, 0);
    if (dialog.exec() == QDialog::Rejected)
    {
        return;
    }

    QString actorName = dialog.getSelectedActor().actorName;
    QVector<float> defaultPressureLvls = {1000, 800, 400, 200, 100, 50, 25};
    addSeedActor(actorName, 10.0, 10.0, defaultPressureLvls);
}


void MTrajectoryActor::addSeedActor(QString actorName, double lon, double lat, QVector<float> levels)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    MActor* actor = glRM->getActorByName(actorName);

    // actor does not exist
    if (!actor)
    {
        return;
    }

    // check if entry already exists
    for (SeedActorSettings& sas : computationSeedActorProperties)
    {
        if (sas.actor->getName().compare(actorName) == 0)
            return;
    }

    // connect to actor changed signal
    connect(actor, SIGNAL(actorChanged()), this, SLOT(onSeedActorChanged()));

    // create property section
    SeedActorSettings actorSettings;
    actorSettings.actor = actor;
    actorSettings.propertyGroup = addProperty(GROUP_PROPERTY, actorName,
                                              computationSeedPropertyGroup);
    actorSettings.removeProperty = addProperty(CLICK_PROPERTY, "remove",
                                               actorSettings.propertyGroup);

    actorSettings.lonSpacing = addProperty(DOUBLE_PROPERTY, "lon spacing",
                                           actorSettings.propertyGroup);
    actorSettings.latSpacing = addProperty(DOUBLE_PROPERTY, "lat spacing",
                                           actorSettings.propertyGroup);
    actorSettings.pressureLevels = addProperty(STRING_PROPERTY, "pressure leves",
                                               actorSettings.propertyGroup);

    // fill data with corresponding
    bool enableX = false, enableY = false, enableZ = false;
    if (dynamic_cast<MMovablePoleActor*>(actor))
    {
        actorSettings.type = POLE_ACTOR;
        enableX = false;
        enableY = false;
        enableZ = true;
    }
    else if (dynamic_cast<MNWPHorizontalSectionActor*>(actor))
    {
        actorSettings.type = HORIZONTAL_ACTOR;
        enableX = true;
        enableY = true;
        enableZ = false;
    }
    else if (dynamic_cast<MNWPVerticalSectionActor*>(actor))
    {
        actorSettings.type = VERTICAL_ACTOR;
        enableX = true;
        enableY = false;
        enableZ = true;
    }
    else if (dynamic_cast<MVolumeBoundingBoxActor*>(actor))
    {
        actorSettings.type = BOX_ACTOR;
        enableX = true;
        enableY = true;
        enableZ = true;
    }

    // fill property
    properties->setDouble(actorSettings.lonSpacing,
                          enableX ? lon : NC_MAX_DOUBLE, 0.01, 999999.99, 2, 1.0);
    properties->setDouble(actorSettings.latSpacing,
                          enableY ? lat : NC_MAX_DOUBLE, 0.01, 999999.99, 2, 1.0);
    properties->mString()->setValue(actorSettings.pressureLevels,
                                    encodePressureLevels(levels, QString(",")));
    actorSettings.lonSpacing->setEnabled(enableX);
    actorSettings.latSpacing->setEnabled(enableY);
    actorSettings.pressureLevels->setEnabled(enableZ);

    // store settings to list
    computationSeedActorProperties.push_back(actorSettings);
}


void MTrajectoryActor::clearSeedActor()
{
    for (SeedActorSettings& sas : computationSeedActorProperties)
    {
        // disconnect signal before deletion
        disconnect(sas.actor, SIGNAL(actorChanged()), this, SLOT(onSeedActorChanged()));

        // delete property
        computationSeedPropertyGroup->removeSubProperty(sas.propertyGroup);
    }

    // clear list
    computationSeedActorProperties.clear();
}


void MTrajectoryActor::removeSeedActor(QString name)
{
    // delete actor with given name
    for (int i = 0; i < computationSeedActorProperties.size(); i++)
    {
        if (computationSeedActorProperties.at(i).actor->getName().compare(name) == 0)
        {
            // disconnect signal before deletion
            disconnect(computationSeedActorProperties.at(i).actor, SIGNAL(actorChanged()), this, SLOT(onSeedActorChanged()));

            // remove from properties and list
            computationSeedPropertyGroup->removeSubProperty(computationSeedActorProperties.at(i).propertyGroup);
            computationSeedActorProperties.removeAt(i);
            break;
        }
    }
}


void MTrajectoryActor::enableProperties(bool enable)
{
    enableActorUpdates(false);
//    ensembleModeProperty->setEnabled(enable);
    enableAscentFilterProperty->setEnabled(enable);
    deltaPressureFilterProperty->setEnabled(enable);
    deltaTimeFilterProperty->setEnabled(enable);
    renderModeProperty->setEnabled(enable);

    // Synchronisation properties should only be enabled if actor is connected
    // to a sync control.
    bool enableSync = (enable && (synchronizationControl != nullptr));
    synchronizationProperty->setEnabled(enable);
    synchronizeInitTimeProperty->setEnabled(enableSync);
    synchronizeStartTimeProperty->setEnabled(enableSync);
    synchronizeParticlePosTimeProperty->setEnabled(enableSync);
    synchronizeEnsembleProperty->setEnabled(enableSync);

    transferFunctionProperty->setEnabled(enable);
    tubeRadiusProperty->setEnabled(enable);
    sphereRadiusProperty->setEnabled(enable);
    enableShadowProperty->setEnabled(enable);
    colourShadowProperty->setEnabled(enable);

    initTimeProperty->setEnabled(
                enable && !(enableSync && synchronizeInitTime));
    startTimeProperty->setEnabled(
                enable && !(enableSync && synchronizeStartTime));
    particlePosTimeProperty->setEnabled(
                enable && !(enableSync && synchronizeParticlePosTime));

    bBoxConnection->getProperty()->setEnabled(enable);
    ensembleMemberProperty->setEnabled(enable && !synchronizeEnsemble);

    // Computation Properties
    bool enableCalc = enable && !precomputedDataSource;
    computationPropertyGroup->setEnabled(enableCalc);

    enableActorUpdates(true);
}


void MTrajectoryActor::releaseData()
{
    // Release for all seed points or for single precomputed trajectories
    for (int t = 0; t < (precomputedDataSource ? 1 : seedActorData.size()); t++)
    {
        releaseData(t);
    }
}

void MTrajectoryActor::releaseData(int slot)
{
    // 1. Trajectory data.
    // ===================
    if (trajectoryRequests[slot].trajectories)
    {
        trajectoryRequests[slot].trajectories->releaseVertexBuffer();
        trajectorySource->releaseData(trajectoryRequests[slot].trajectories);
        trajectoryRequests[slot].trajectories = nullptr;
    }

    // 2. Normals.
    // ===========
            foreach (MSceneViewGLWidget *view,
                     MSystemManagerAndControl::getInstance()->getRegisteredViews())
        {
            if (trajectoryRequests[slot].normals.value(view, nullptr))
            {
                trajectoryRequests[slot].normals[view]->releaseVertexBuffer();
                normalsSource->releaseData(trajectoryRequests[slot].normals[view]);
                trajectoryRequests[slot].normals[view] = nullptr;
            }
        }

    // 3. Selection.
    // =============
    if (trajectoryRequests[slot].trajectorySelection)
    {
        trajectoryFilter->releaseData(trajectoryRequests[slot].trajectorySelection);
        trajectoryRequests[slot].trajectorySelection = nullptr;
    }

    // 4. Single time selection.
    // =========================
    if (trajectoryRequests[slot].trajectorySingleTimeSelection)
    {
        trajectoryFilter->releaseData(trajectoryRequests[slot].trajectorySingleTimeSelection);
        trajectoryRequests[slot].trajectorySingleTimeSelection = nullptr;
    }
}


} // namespace Met3D
