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
#include "synccontrol.h"
#include "ui_synccontrol.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "gxfw/msystemcontrol.h"
#include "gxfw/msceneviewglwidget.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSyncControl::MSyncControl(QString id, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MSyncControl),
    syncID(id),
    synchronizationInProgress(false),
    lastFocusWidget(nullptr),
    currentSyncType(SYNC_UNKNOWN)
{
    ui->setupUi(this);

    // Time control elements.
    // =========================================================================

    // Time steps for navigating valid/init time in seconds (5min, 10min, ..).
    const int numTimeSteps = 9;
    int timeStepsSeconds[numTimeSteps] = {300, 600, 900, 1800, 3600, 10800,
                                          21600, 43200, 86400};
    timeStepIndexToSeconds = new int[numTimeSteps];
    for (int i = 0; i < numTimeSteps; i++)
        timeStepIndexToSeconds[i] = timeStepsSeconds[i];
    ui->timeStepComboBox->setCurrentIndex(6); // pre-select 6hrs

    // Initialise with 00 UTC of current date.
    setInitDateTime(QDateTime(QDateTime::currentDateTimeUtc().date()));
    setValidDateTime(QDateTime(QDateTime::currentDateTimeUtc().date()));
    updateTimeDifference();

    connect(ui->timeForwardButton, SIGNAL(clicked()), SLOT(timeForward()));
    connect(ui->timeBackwardButton, SIGNAL(clicked()), SLOT(timeBackward()));
    connect(ui->animationPlayButton, SIGNAL(clicked()), SLOT(startTimeAnimation()));

    connect(ui->validTimeEdit,
            SIGNAL(dateTimeChanged(QDateTime)),
            SLOT(onValidDateTimeChange(QDateTime)));
    connect(ui->initTimeEdit,
            SIGNAL(dateTimeChanged(QDateTime)),
            SLOT(onInitDateTimeChange(QDateTime)));

    animationTimer = new QTimer(this);
    connect(animationTimer, SIGNAL(timeout()), SLOT(timeForward()));


    // Ensemble control elements.
    // =========================================================================

//TODO: Remove hardcoded limits!
    ui->ensembeMemberSpinBox->setMinimum(0);
    ui->ensembeMemberSpinBox->setMaximum(50);

    connect(ui->showMeanCheckBox,
            SIGNAL(stateChanged(int)),
            SLOT(onEnsembleModeChange(int)));
    connect(ui->ensembeMemberSpinBox,
            SIGNAL(valueChanged(int)),
            SLOT(onEnsembleModeChange(int)));
}


MSyncControl::~MSyncControl()
{
    delete[] timeStepIndexToSeconds;
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QDateTime MSyncControl::validDateTime() const
{
    return ui->validTimeEdit->dateTime();
}


void MSyncControl::setValidDateTime(const QDateTime &dateTime)
{
    ui->validTimeEdit->setDateTime(dateTime);
}


QDateTime MSyncControl::initDateTime() const
{
    return ui->initTimeEdit->dateTime();
}


void MSyncControl::setInitDateTime(const QDateTime &dateTime)
{
    ui->initTimeEdit->setDateTime(dateTime);
}


int MSyncControl::ensembleMember() const
{
    if (ui->showMeanCheckBox->isChecked())
        return -1;
    else
        return ui->ensembeMemberSpinBox->value();
}


void MSyncControl::registerSynchronizedClass(MSynchronizedObject *object)
{
    if (object != nullptr) synchronizedObjects.insert(object);
}


void MSyncControl::deregisterSynchronizedClass(MSynchronizedObject *object)
{
    if (synchronizedObjects.contains(object)) synchronizedObjects.remove(object);
}


void MSyncControl::synchronizationCompleted(MSynchronizedObject *object)
{
    if (object != nullptr)
    {
        if (pendingSynchronizations.contains(object))
        {
            pendingSynchronizations.remove(object);
        }
        else
        {
            earlyCompletedSynchronizations.insert(object);
        }
    }

    if (pendingSynchronizations.empty() && earlyCompletedSynchronizations.empty())
    {
        // Enable GUI for next event.
        ui->syncFrame->setEnabled(true);

        // Last active QWdiget looses focus through disabling of sync frame
        // -- give it back.
        if (lastFocusWidget) lastFocusWidget->setFocus();
        // For some reason this doesn't always work for the ensemble memnber
        // spinbox.
        if (currentSyncType == SYNC_ENSEMBLE_MEMBER)
            ui->ensembeMemberSpinBox->setFocus();

        currentSyncType = SYNC_UNKNOWN;

        endSceneSynchronization();
        synchronizationInProgress = false;
    }
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MSyncControl::timeForward()
{
    if (ui->stepChooseVTITComboBox->currentIndex() == 0)
    {
        // Index 0 of stepChooseVTITComboBox means that the valid time should
        // be modified by the time navigation buttons.
        applyTimeStep(ui->validTimeEdit, 1);
    }
    else
    {
        // Modify initialisation time.
        applyTimeStep(ui->initTimeEdit, 1);
    }
}


void MSyncControl::timeBackward()
{
    if (ui->stepChooseVTITComboBox->currentIndex() == 0)
    {
        // Index 0 of stepChooseVTITComboBox means that the valid time should
        // be modified by the time navigation buttons.
        applyTimeStep(ui->validTimeEdit, -1);
    }
    else
    {
        // Modify initialisation time.
        applyTimeStep(ui->initTimeEdit, -1);
    }
}


void MSyncControl::startTimeAnimation()
{
    if (ui->animationPlayButton->isChecked())
        animationTimer->start(1000);
    else
        animationTimer->stop();
}


void MSyncControl::onValidDateTimeChange(const QDateTime &datetime)
{
#ifdef LOG_EVENT_TIMES
    LOG4CPLUS_DEBUG(mlog, "valid time change has been triggered at "
                    << MSystemManagerAndControl::getInstance()
                    ->elapsedTimeSinceSystemStart(MStopwatch::MILLISECONDS)
                    << " ms");
#endif
#ifdef DIRECT_SYNCHRONIZATION
    // Ignore incoming signals if synchronization is currently in progress.
    // This might happen e.g. for the ensemble member if the user holds the
    // up/down key pressed (the spin box emits multiple signals before this
    // slot can start to process them).
//TODO: Ignoring signals might cause the system to "jump" over time steps,
//      or members. Is there a more immediate way than the current disabling
//      to BLOCK the GUI elements?
    if (synchronizationInProgress) return;
    synchronizationInProgress = true;
    processSynchronizationEvent(SYNC_VALID_TIME, QVariant(datetime));
#else
    emit beginSynchronization();
    updateTimeDifference();
    emit validDateTimeChanged(datetime);
    emit endSynchronization();
#endif
}


void MSyncControl::onInitDateTimeChange(const QDateTime &datetime)
{
#ifdef DIRECT_SYNCHRONIZATION
    if (synchronizationInProgress) return;
    synchronizationInProgress = true;
    processSynchronizationEvent(SYNC_INIT_TIME, QVariant(datetime));
#else
    emit beginSynchronization();
    updateTimeDifference();
    emit initDateTimeChanged(datetime);
    emit endSynchronization();
#endif
}


void MSyncControl::onEnsembleModeChange(const int foo)
{
    Q_UNUSED(foo);
#ifdef DIRECT_SYNCHRONIZATION
    if (synchronizationInProgress) return;
    synchronizationInProgress = true;
#else
    emit beginSynchronization();
#endif
    int member = -1;

    if (ui->showMeanCheckBox->isChecked())
    {
        // Ensemble mean.
        ui->ensembeMemberSpinBox->setEnabled(false);
        ui->ensembleMemberLabel->setEnabled(false);
    }
    else
    {
        // Change to specified ensemble member.
        ui->ensembeMemberSpinBox->setEnabled(true);
        ui->ensembleMemberLabel->setEnabled(true);
        member = ui->ensembeMemberSpinBox->value();
    }

#ifdef DIRECT_SYNCHRONIZATION
    processSynchronizationEvent(SYNC_ENSEMBLE_MEMBER, QVariant(member));
#else
    emit ensembleMemberChanged(member);
    emit endSynchronization();
#endif
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MSyncControl::applyTimeStep(QDateTimeEdit *dte, int sign)
{
    QDateTime time = dte->dateTime();
    int timeStepIndex = ui->timeStepComboBox->currentIndex();
    dte->setDateTime(time.addSecs(sign * timeStepIndexToSeconds[timeStepIndex]));
}


void MSyncControl::updateTimeDifference()
{
    QDateTime validTime = ui->validTimeEdit->dateTime();
    QDateTime initTime  = ui->initTimeEdit->dateTime();
    QString s = QString("%1 hrs from").arg(
                int(initTime.secsTo(validTime) / 3600.));
    ui->differenceValidInitLabel->setText(s);
}


void MSyncControl::beginSceneSynchronization()
{
#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
#endif

    foreach (MSceneViewGLWidget *view,
             MSystemManagerAndControl::getInstance()->getRegisteredViews())
    {
        view->setFreeze(true);
    }
}


void MSyncControl::endSceneSynchronization()
{
    foreach (MSceneViewGLWidget *view,
             MSystemManagerAndControl::getInstance()->getRegisteredViews())
    {
        view->setFreeze(false);
    }

#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "synchronization event processed in "
                    << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif
}


void MSyncControl::processSynchronizationEvent(MSynchronizationType syncType,
                                               QVariant syncVariant)
{
    // Begin synchronization: disable sync GUI, tell scenes that sync
    // begins (so they can block redraws).
    lastFocusWidget = QApplication::focusWidget();
    currentSyncType = syncType;
    ui->syncFrame->setEnabled(false);
    beginSceneSynchronization();

    if ( (syncType == SYNC_VALID_TIME) || (syncType == SYNC_INIT_TIME) )
    {
        updateTimeDifference();
    }

    // Send sync info to each registered synchronized object. Collect those
    // objects that will process the sync request (they return true).
    foreach (MSynchronizedObject *syncObj, synchronizedObjects)
    {
        if ( syncObj->synchronizationEvent(syncType, syncVariant) )
        {
            pendingSynchronizations.insert(syncObj);
        }
    }

    // If objects have completed the synchronization request before the
    // loop above has completed they are stored in
    // earlyCompletedSynchronizations (see synchronizationCompleted()).
    // Remove those from pendingSynchronizations.
    if ( !earlyCompletedSynchronizations.empty() )
    {
        pendingSynchronizations.subtract(earlyCompletedSynchronizations);
        earlyCompletedSynchronizations.clear();
    }

    // If no object accepted the sync event we can finish the sync.
    if (pendingSynchronizations.empty()) synchronizationCompleted(nullptr);
}


} // namespace Met3D
