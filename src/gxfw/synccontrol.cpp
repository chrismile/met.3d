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

MLabelledWidgetAction::MLabelledWidgetAction(
        const QString &labelFront, const QString &labelBack,
        QWidget *customWidget, QWidget *parent)
    : QWidgetAction(parent)
{
    QWidget* widget = new QWidget(parent);
    QHBoxLayout* layout = new QHBoxLayout();

    QLabel *label1 = new QLabel(labelFront, parent);
    layout->addWidget(label1);

    this->customWidget = customWidget;
    layout->addWidget(customWidget);

    if ( !labelBack.isEmpty() )
    {
        QLabel *label2 = new QLabel(labelBack, parent);
        layout->addWidget(label2);
    }

    widget->setLayout(layout);
    setDefaultWidget(widget);
}


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
    const int numTimeSteps = 10;
    int timeStepsSeconds[numTimeSteps] = {60, 300, 600, 900, 1800, 3600, 10800,
                                          21600, 43200, 86400};
    timeStepIndexToSeconds = new int[numTimeSteps];
    for (int i = 0; i < numTimeSteps; i++)
        timeStepIndexToSeconds[i] = timeStepsSeconds[i];
    ui->timeStepComboBox->setCurrentIndex(7); // pre-select 6hrs

    // Initialise with 00 UTC of current date.
    setInitDateTime(QDateTime(QDateTime::currentDateTimeUtc().date()));
    setValidDateTime(QDateTime(QDateTime::currentDateTimeUtc().date()));
    updateTimeDifference();

    connect(ui->validTimeEdit, SIGNAL(dateTimeChanged(QDateTime)),
            SLOT(onValidDateTimeChange(QDateTime)));
    connect(ui->initTimeEdit, SIGNAL(dateTimeChanged(QDateTime)),
            SLOT(onInitDateTimeChange(QDateTime)));
    connect(ui->timeForwardButton, SIGNAL(clicked()),
            SLOT(timeForward()));
    connect(ui->timeBackwardButton, SIGNAL(clicked()),
            SLOT(timeBackward()));

    // Initialise a drop down menu that provides time animation settings.
    // ==================================================================
    timeAnimationDropdownMenu = new QMenu(this);

    timeAnimationTimeStepSpinBox = new QSpinBox(this);
    timeAnimationTimeStepSpinBox->setMinimum(10);
    timeAnimationTimeStepSpinBox->setMaximum(10000);
    timeAnimationTimeStepSpinBox->setValue(1000);
    MLabelledWidgetAction *timeStepSpinBoxAction = new MLabelledWidgetAction(
                "animation time step:", "ms", timeAnimationTimeStepSpinBox, this);
    timeAnimationDropdownMenu->addAction(timeStepSpinBoxAction);

    timeAnimationDropdownMenu->addSeparator();

    // "from" entry of drop down menu.
    // -------------------------------

    // Width for all "copy IT/VT to from/to" buttons.
    int widthOfCopyButtons = 30;

    timeAnimationFromWidget = new QWidget(this);

    timeAnimationFrom = new QDateTimeEdit(timeAnimationFromWidget);
    timeAnimationFrom->setDisplayFormat("ddd yyyy-MM-dd hh:mm UTC");
    timeAnimationFrom->setTimeSpec(Qt::UTC);
    copyInitTimeToAnimationFromButton = new QPushButton("IT", timeAnimationFromWidget);
    copyInitTimeToAnimationFromButton->setMinimumWidth(widthOfCopyButtons);
    copyInitTimeToAnimationFromButton->setMaximumWidth(widthOfCopyButtons);
    copyInitTimeToAnimationFromButton->setToolTip("set \"from\" to init time");
    copyValidTimeToAnimationFromButton = new QPushButton("VT", timeAnimationFromWidget);
    copyValidTimeToAnimationFromButton->setMinimumWidth(widthOfCopyButtons);
    copyValidTimeToAnimationFromButton->setMaximumWidth(widthOfCopyButtons);
    copyValidTimeToAnimationFromButton->setToolTip("set \"from\" to valid time");

    timeAnimationFromLayout = new QHBoxLayout();
    timeAnimationFromLayout->addWidget(timeAnimationFrom);
    timeAnimationFromLayout->addWidget(copyInitTimeToAnimationFromButton);
    timeAnimationFromLayout->addWidget(copyValidTimeToAnimationFromButton);

    timeAnimationFromWidget->setLayout(timeAnimationFromLayout);

    MLabelledWidgetAction *animateFromTimeAction =
            new MLabelledWidgetAction("from", "", timeAnimationFromWidget, this);
    timeAnimationDropdownMenu->addAction(animateFromTimeAction);

    connect(copyInitTimeToAnimationFromButton, SIGNAL(clicked()),
            SLOT(copyInitToFrom()));
    connect(copyValidTimeToAnimationFromButton, SIGNAL(clicked()),
            SLOT(copyValidToFrom()));

    // "to" entry of drop down menu.
    // -----------------------------

    timeAnimationToWidget = new QWidget(this);

    timeAnimationTo = new QDateTimeEdit(timeAnimationToWidget);
    timeAnimationTo->setDisplayFormat("ddd yyyy-MM-dd hh:mm UTC");
    timeAnimationTo->setTimeSpec(Qt::UTC);
    copyInitTimeToAnimationToButton = new QPushButton("IT", timeAnimationToWidget);
    copyInitTimeToAnimationToButton->setMinimumWidth(widthOfCopyButtons);
    copyInitTimeToAnimationToButton->setMaximumWidth(widthOfCopyButtons);
    copyInitTimeToAnimationToButton->setToolTip("set \"to\" to init time");
    copyValidTimeToAnimationToButton = new QPushButton("VT", timeAnimationToWidget);
    copyValidTimeToAnimationToButton->setMinimumWidth(widthOfCopyButtons);
    copyValidTimeToAnimationToButton->setMaximumWidth(widthOfCopyButtons);
    copyValidTimeToAnimationToButton->setToolTip("set \"to\" to valid time");

    timeAnimationToLayout = new QHBoxLayout();
    timeAnimationToLayout->addWidget(timeAnimationTo);
    timeAnimationToLayout->addWidget(copyInitTimeToAnimationToButton);
    timeAnimationToLayout->addWidget(copyValidTimeToAnimationToButton);
    timeAnimationToWidget->setLayout(timeAnimationToLayout);

    MLabelledWidgetAction *animateToTimeAction =
            new MLabelledWidgetAction("to", "", timeAnimationToWidget, this);
    timeAnimationDropdownMenu->addAction(animateToTimeAction);

    connect(copyInitTimeToAnimationToButton, SIGNAL(clicked()),
            SLOT(copyInitToTo()));
    connect(copyValidTimeToAnimationToButton, SIGNAL(clicked()),
            SLOT(copyValidToTo()));


    timeAnimationDropdownMenu->addSeparator();

    timeAnimationSinglePassAction = new QAction(this);
    timeAnimationSinglePassAction->setCheckable(true);
    timeAnimationSinglePassAction->setChecked(true);
    timeAnimationSinglePassAction->setText("Single pass");
    timeAnimationDropdownMenu->addAction(timeAnimationSinglePassAction);

    timeAnimationLoopTimeAction = new QAction(this);
    timeAnimationLoopTimeAction->setCheckable(true);
    timeAnimationLoopTimeAction->setText("Loop");
    timeAnimationDropdownMenu->addAction(timeAnimationLoopTimeAction);

    timeAnimationBackForthTimeAction = new QAction(this);
    timeAnimationBackForthTimeAction->setCheckable(true);
    timeAnimationBackForthTimeAction->setText("Back and forth");
    timeAnimationDropdownMenu->addAction(timeAnimationBackForthTimeAction);

    timeAnimationLoopGroup = new QActionGroup(this);
    timeAnimationLoopGroup->setExclusive(true);
    timeAnimationLoopGroup->addAction(timeAnimationSinglePassAction);
    timeAnimationLoopGroup->addAction(timeAnimationLoopTimeAction);
    timeAnimationLoopGroup->addAction(timeAnimationBackForthTimeAction);

    timeAnimationReverseTimeDirectionAction = new QAction(this);
    timeAnimationReverseTimeDirectionAction->setCheckable(true);
    timeAnimationReverseTimeDirectionAction->setText("Reverse time direction");
    timeAnimationDropdownMenu->addAction(timeAnimationReverseTimeDirectionAction);

    ui->animationPlayButton->setMenu(timeAnimationDropdownMenu);

    connect(ui->animationPlayButton, SIGNAL(clicked()),
            SLOT(startTimeAnimation()));
    connect(ui->animationStopButton, SIGNAL(clicked()),
            SLOT(stopTimeAnimation()));

    // Initialise a timer to control the animation.
    animationTimer = new QTimer(this);
    connect(animationTimer, SIGNAL(timeout()),
            SLOT(timeAnimationAdvanceTimeStep()));


    // Ensemble control elements.
    // =========================================================================

//TODO (mr, 22Mar2016): Remove hardcoded limits -- MSynchronizedObject needs
//                      to provide limits of valid/init time and ens members.
    ui->ensembleMemberSpinBox->setMinimum(0);
    ui->ensembleMemberSpinBox->setMaximum(50);

    connect(ui->showMeanCheckBox,
            SIGNAL(stateChanged(int)),
            SLOT(onEnsembleModeChange(int)));
    connect(ui->ensembleMemberSpinBox,
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


void MSyncControl::copyValidTimeToTimeAnimationFromTo()
{
//TODO (mr, 22Mar2016): Update from data sources -- MSynchronizedObject needs
//                      to provide limits of valid/init time and ens members.
    timeAnimationFrom->setDateTime(ui->validTimeEdit->dateTime());
    timeAnimationTo->setDateTime(ui->validTimeEdit->dateTime());
}


int MSyncControl::ensembleMember() const
{
    if (ui->showMeanCheckBox->isChecked())
        return -1;
    else
        return ui->ensembleMemberSpinBox->value();
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
        setSynchronizationGUIEnabled(true);

        // In animation mode force an immediate repaint of the valid and init
        // time displays (otherwise they may not update during animation).
        if (animationTimer->isActive())
        {
            ui->validTimeEdit->repaint();
            ui->initTimeEdit->repaint();
        }

        // Last active QWdiget looses focus through disabling of sync frame
        // -- give it back.
        if (lastFocusWidget) lastFocusWidget->setFocus();
        // For some reason this doesn't always work for the ensemble memnber
        // spinbox.
        if (currentSyncType == SYNC_ENSEMBLE_MEMBER)
        {
            ui->ensembleMemberSpinBox->setFocus();
            // Also, force repaint of ensemble memnber spinbox; it doesn't
            // always update in time (e.g., when the user holds an up/down
            // key to animate over the ensemble members).
            ui->ensembleMemberSpinBox->repaint();
        }

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

        if ( animationTimer->isActive() )
            if (ui->validTimeEdit->dateTime() >= timeAnimationTo->dateTime() )
            {
                if ( timeAnimationLoopTimeAction->isChecked() )
                    ui->validTimeEdit->setDateTime(timeAnimationFrom->dateTime());
                else if ( timeAnimationBackForthTimeAction->isChecked() )
                    timeAnimationReverseTimeDirectionAction->toggle();
                else
                    stopTimeAnimation();

                return;
            }

        applyTimeStep(ui->validTimeEdit, 1);
    }
    else
    {
        // Modify initialisation time.

        if ( animationTimer->isActive() )
            if (ui->initTimeEdit->dateTime() >= timeAnimationTo->dateTime() )
            {
                if ( timeAnimationLoopTimeAction->isChecked() )
                    ui->initTimeEdit->setDateTime(timeAnimationFrom->dateTime());
                else if ( timeAnimationBackForthTimeAction->isChecked() )
                    timeAnimationReverseTimeDirectionAction->toggle();
                else
                    stopTimeAnimation();

                return;
            }

        applyTimeStep(ui->initTimeEdit, 1);
    }
}


void MSyncControl::timeBackward()
{
    if (ui->stepChooseVTITComboBox->currentIndex() == 0)
    {
        // Index 0 of stepChooseVTITComboBox means that the valid time should
        // be modified by the time navigation buttons.

        if ( animationTimer->isActive() )
            if (ui->validTimeEdit->dateTime() <= timeAnimationFrom->dateTime() )
            {
                if ( timeAnimationLoopTimeAction->isChecked() )
                    ui->validTimeEdit->setDateTime(timeAnimationTo->dateTime());
                else if ( timeAnimationBackForthTimeAction->isChecked() )
                    timeAnimationReverseTimeDirectionAction->toggle();
                else
                    stopTimeAnimation();

                return;
            }

        applyTimeStep(ui->validTimeEdit, -1);
    }
    else
    {
        // Modify initialisation time.

        if ( animationTimer->isActive() )
            if (ui->initTimeEdit->dateTime() <= timeAnimationFrom->dateTime() )
            {
                if ( timeAnimationLoopTimeAction->isChecked() )
                    ui->initTimeEdit->setDateTime(timeAnimationTo->dateTime());
                else if ( timeAnimationBackForthTimeAction->isChecked() )
                    timeAnimationReverseTimeDirectionAction->toggle();
                else
                    stopTimeAnimation();

                return;
            }

        applyTimeStep(ui->initTimeEdit, -1);
    }
}


void MSyncControl::timeAnimationAdvanceTimeStep()
{
#ifdef DIRECT_SYNCHRONIZATION
    // Don't apply the time change if the previous request hasn't been
    // completed.
    if (synchronizationInProgress) return;
#endif

    if ( timeAnimationReverseTimeDirectionAction->isChecked() )
        timeBackward();
    else
        timeForward();
}


void MSyncControl::startTimeAnimation()
{
    if (ui->animationPlayButton->isChecked())
    {
        // Disable time control GUI elements; enable STOP button.
        ui->animationPlayButton->setEnabled(false);
        ui->animationStopButton->setEnabled(true);
        timeAnimationDropdownMenu->setEnabled(false);
        setTimeSynchronizationGUIEnabled(false);

        // Start the animation timer.
        animationTimer->start(timeAnimationTimeStepSpinBox->value());
    }
}


void MSyncControl::stopTimeAnimation()
{
    // Stop the animation timer.
    animationTimer->stop();

    // Enable time control GUI elements; disable STOP button.
    ui->animationPlayButton->setEnabled(true);
    ui->animationPlayButton->setChecked(false);
    timeAnimationDropdownMenu->setEnabled(true);
    setTimeSynchronizationGUIEnabled(true);
    ui->animationStopButton->setEnabled(false);
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
        ui->ensembleMemberSpinBox->setEnabled(false);
        ui->ensembleMemberLabel->setEnabled(false);
    }
    else
    {
        // Change to specified ensemble member.
        ui->ensembleMemberSpinBox->setEnabled(true);
        ui->ensembleMemberLabel->setEnabled(true);
        member = ui->ensembleMemberSpinBox->value();
    }

#ifdef DIRECT_SYNCHRONIZATION
    processSynchronizationEvent(SYNC_ENSEMBLE_MEMBER, QVariant(member));
#else
    emit ensembleMemberChanged(member);
    emit endSynchronization();
#endif
}


void MSyncControl::copyInitToFrom()
{
    timeAnimationFrom->setDateTime(initDateTime());
}


void MSyncControl::copyValidToFrom()
{
    timeAnimationFrom->setDateTime(validDateTime());
}


void MSyncControl::copyInitToTo()
{
    timeAnimationTo->setDateTime(initDateTime());
}


void MSyncControl::copyValidToTo()
{
    timeAnimationTo->setDateTime(validDateTime());
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
    // Begin synchronization: disable sync GUI (unless the event is caused by
    // the animationTimer; in this case the GUI remains active so the user can
    // stop the animation), tell scenes that sync begins (so they can block
    // redraws).
    lastFocusWidget = QApplication::focusWidget();
    currentSyncType = syncType;
    if ( !animationTimer->isActive() ) setSynchronizationGUIEnabled(false);
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


void MSyncControl::setTimeSynchronizationGUIEnabled(bool enabled)
{
    ui->initTimeEdit->setEnabled(enabled);
    ui->validTimeEdit->setEnabled(enabled);
    ui->timeBackwardButton->setEnabled(enabled);
    ui->timeForwardButton->setEnabled(enabled);
    ui->timeStepComboBox->setEnabled(enabled);
    ui->stepChooseVTITComboBox->setEnabled(enabled);
}


void MSyncControl::setSynchronizationGUIEnabled(bool enabled)
{
    ui->syncFrame->setEnabled(enabled);
    ui->timeBackwardButton->blockSignals(!enabled);
    ui->timeForwardButton->blockSignals(!enabled);
}

} // namespace Met3D
