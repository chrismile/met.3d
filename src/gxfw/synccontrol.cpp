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
#include "selectdatasourcedialog.h"
#include "data/weatherpredictiondatasource.h"

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
    lastIinitTime = QDateTime();
    lastValidTime = QDateTime();

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

    retrictControlToDataSources();

    // Only initialise with initTime and validTime if they are set properly.
    if (!lastIinitTime.isNull() && !lastValidTime.isNull())
    {
        // Initialise with minimum init and valid time.
        setInitDateTime(lastIinitTime);
        setValidDateTime(lastValidTime);
    }
    else
    {
        // Initialise with 00 UTC of current date.
        setInitDateTime(QDateTime(QDateTime::currentDateTimeUtc().date()));
        setValidDateTime(QDateTime(QDateTime::currentDateTimeUtc().date()));
    }
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

    connect(ui->showMeanCheckBox,
            SIGNAL(stateChanged(int)),
            SLOT(onEnsembleModeChange(int)));
    connect(ui->ensembleMemberComboBox,
            SIGNAL(currentIndexChanged(int)),
            SLOT(onEnsembleModeChange(int)));


    // Configuration control elements.
    // =========================================================================

    configurationDropdownMenu = new QMenu(this);

    selectDataSourcesAction = new QAction(this);
    selectDataSourcesAction->setText("data sources");
    configurationDropdownMenu->addAction(selectDataSourcesAction);

    ui->configurationButton->setMenu(configurationDropdownMenu);

    connect(selectDataSourcesAction, SIGNAL(triggered()),
            SLOT(selectDataSources()));
    // Show menu also if the users clicks the button not only if only the arrow
    // was clicked.
    connect(ui->configurationButton, SIGNAL(clicked()),
            ui->configurationButton, SLOT(showMenu()));
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
        return ui->ensembleMemberComboBox->currentText().toInt();
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


void MSyncControl::selectDataSources()
{
    // Ask the user for data sources to which times and ensemble members the
    // sync control should be restricted to.
    MSelectDataSourceDialog dialog(this);
    if (dialog.exec() == QDialog::Rejected)
    {
        return;
    }

    QStringList selectedDataSources = dialog.getSelectedDataSourceIDs();

    if (selectedDataSources.empty())
    {
        // The user has selected an emtpy set of data sources. Display a
        // warning and do NOT accept the empty set.
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("You need to select at least one data source.");
        msgBox.exec();
        return;
    }

    retrictControlToDataSources(selectedDataSources);
}


void MSyncControl::restrictToDataSourcesFromFrontend(
        QStringList selectedDataSources)
{
    QStringList suitableDataSources;
    suitableDataSources.clear();

    if (!selectedDataSources.empty())
    {
        MSystemManagerAndControl* sysMC = MSystemManagerAndControl::getInstance();

        // Check if at least one data source is available with values to load.
        foreach (QString dataSourceID, selectedDataSources)
        {
            MWeatherPredictionDataSource* source =
                    dynamic_cast<MWeatherPredictionDataSource*>
                    (sysMC->getDataSource(dataSourceID));
            if (source == nullptr)
            {
                // The user has defined a dataSource in initialiseFromDatasource
                // in frontend that does not exist. Print warning and continue
                // with next data source.
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setText(dataSourceID + " does not exist!");
                msgBox.exec();
                continue;
            }

            // Only add data source to list of suitable data sources if it
            // contains init times, valid times and ensemble member informations.
            if (MSelectDataSourceDialog::checkDataSourceForData(source))
            {
                suitableDataSources.append(dataSourceID);
            }
        }

        if (suitableDataSources.empty())
        {
            // None of the data sources defined in frontend contains init times,
            // valid time and ensemble members.
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("No suitable data sources given in frontend."
                           " Use data of all data sources given!");
            msgBox.exec();

            QStringList availableDataSources = sysMC->getDataSourceIdentifiers();

            // Check for registered data source containing init times, valid
            // times and ensemble members.
            foreach (QString dataSourceID, availableDataSources)
            {
                MWeatherPredictionDataSource* source =
                        dynamic_cast<MWeatherPredictionDataSource*>
                        (sysMC->getDataSource(dataSourceID));
                // Only add data source to list of suitable data sources if it
                // contains init times, valid times and ensemble members
                // informations.
                if (MSelectDataSourceDialog::checkDataSourceForData(source))
                {
                    suitableDataSources.append(dataSourceID);
                }
            }

            // None of the registered data sources contains times and ensemble
            // members information. Inform user and return.
            if (suitableDataSources.empty())
            {
                msgBox.setText("No suitable data sources available!");
                msgBox.exec();
                return;
            }
        }
    }

    // Use the suitable data sources set to setup the sync control.
    retrictControlToDataSources(suitableDataSources);
}


void MSyncControl::retrictControlToDataSources(QStringList selectedDataSources)
{

    MSystemManagerAndControl* sysMC = MSystemManagerAndControl::getInstance();

    // Use all data sources if no data sources are given.
    if (selectedDataSources.empty())
    {
        QStringList availableDataSources = sysMC->getDataSourceIdentifiers();

        // Check for each data sourcs if it is suitable to restrict control.
        // Since in the list of all data sources might be data sources without
        // time and member informations.
        foreach (QString dataSourceID, availableDataSources)
        {
            MWeatherPredictionDataSource* source =
                    dynamic_cast<MWeatherPredictionDataSource*>
                    (sysMC->getDataSource(dataSourceID));
            // Only add data source to list of suitable data sources if it
            // contains init times, valid times and ensemble member
            // informations.
            if (MSelectDataSourceDialog::checkDataSourceForData(source))
            {
                selectedDataSources.append(dataSourceID);
            }
        }

    }

    // Return if no data sources are available.
    if (selectedDataSources.empty())
    {
        return;
    }

    QStringList variables;
    QList<QDateTime> currentInitTimes;
    QList<QDateTime> currentValidTimes;
    variables.clear();
    currentInitTimes.clear();
    currentValidTimes.clear();

// TODO (bt, 23Feb2017): If updated to Qt 5.0 use QSets and unite instead of
// lists and contains since for version 4.8 there is no qHash method for QDateTime
// and thus it is not possible to use toSet on QList<QDateTime>.
// (See: http://doc.qt.io/qt-5/qhash.html#qHashx)
    availableInitTimes.clear();
    availableValidTimes.clear();
    availableEnsembleMembers.clear();

    foreach (QString dataSourceID, selectedDataSources)
    {
        MWeatherPredictionDataSource* source =
                dynamic_cast<MWeatherPredictionDataSource*>
                (sysMC->getDataSource(dataSourceID));

        QList<MVerticalLevelType> levelTypes = source->availableLevelTypes();
        for (int ilvl = 0; ilvl < levelTypes.size(); ilvl++)
        {
            MVerticalLevelType levelType = levelTypes.at(ilvl);

            QStringList variables = source->availableVariables(levelType);

            for (int ivar = 0; ivar < variables.size(); ivar++)
            {
                QString var = variables.at(ivar);
                currentInitTimes =
                        source->availableInitTimes(levelType, var);
                if (currentInitTimes.empty())
                {
                    continue;
                }

                for (int iInitTime = 0; iInitTime < currentInitTimes.size();
                     iInitTime++)
                {
                    QDateTime initTime = currentInitTimes.at(iInitTime);
                    if (!availableInitTimes.contains(initTime))
                    {
                        availableInitTimes.append(initTime);
                    }
                    currentValidTimes = source->availableValidTimes(levelType,
                                                                    var,
                                                                    initTime);
                    if (currentValidTimes.empty())
                    {
                        continue;
                    }

                    for (int iValidTime = 0; iValidTime < currentValidTimes.size();
                         iValidTime++)
                    {
                        QDateTime validTime = currentValidTimes[iValidTime];
                        if (!availableValidTimes.contains(validTime))
                        {
                            availableValidTimes.append(validTime);
                        }
                    } // validTimes
                } // initTimes
                availableEnsembleMembers =
                        availableEnsembleMembers.unite(
                            source->availableEnsembleMembers(levelType, var));
            } // variables
        } // levelTypes
    } // dataSources

    // Search for minium and maximum date values to restrict the time edits to
    // them respectively.
    QDateTime minTime = availableInitTimes.first();
    QDateTime maxTime = minTime;
    foreach (QDateTime time, availableInitTimes)
    {
        minTime = min(time, minTime);
        maxTime = max(time, maxTime);
    }
    ui->initTimeEdit->setDateRange(minTime.date(), maxTime.date());
    // Set time range to full day since otherwise it is not possible to change
    // the time properly for the first and last day of the range.
    ui->initTimeEdit->setTimeRange(QTime(0,0,0), QTime(23,59,59));
    lastIinitTime = minTime;
    minTime = availableValidTimes.first();
    maxTime = minTime;
    foreach (QDateTime time, availableValidTimes)
    {
        minTime = min(time, minTime);
        maxTime = max(time, maxTime);
    }
    ui->validTimeEdit->setDateRange(minTime.date(), maxTime.date());
    // Set time range to full day since otherwise it is not possible to change
    // the time properly for the first and last day of the range.
    ui->validTimeEdit->setTimeRange(QTime(0,0,0), QTime(23,59,59));
    lastValidTime = minTime;

    QStringList memberList;
    QList<unsigned int> intMemberList;
    memberList.clear();
    ui->ensembleMemberComboBox->clear();
    // Get list of member to be able to sort them from smallest to greatest
    // value.
    intMemberList = availableEnsembleMembers.toList();
    qSort(intMemberList);
    foreach (unsigned int member, intMemberList)
    {
        memberList.append(QString("%1").arg(member));
    }
    ui->ensembleMemberComboBox->addItems(memberList);
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
    // Only restrict valid time to available valid if times they are set yet.
    if (!lastValidTime.isNull() && !availableValidTimes.empty())
    {
        // Reseting to previous time - do nothing.
        if (lastValidTime == datetime)
        {
            return;
        }
        // Check if selected time is part of available times. If not, reset to
        // previous time.
        if (!availableValidTimes.contains(datetime))
        {
            ui->validTimeEdit->setDateTime(lastValidTime);
            return;
        }

        lastValidTime = datetime;
    }
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
    // Only restrict init time to available init times if they are set yet.
    if (!lastIinitTime.isNull() && !availableInitTimes.empty())
    {
        // Reseting to previous time - do nothing.
        if (lastIinitTime == datetime)
        {
            return;
        }
        // Check if selected time is part of available times. If not, reset to
        // previous time.
        if (!availableInitTimes.contains(datetime))
        {
            ui->initTimeEdit->setDateTime(lastIinitTime);
            return;
        }

        lastIinitTime = datetime;
    }
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
        ui->ensembleMemberComboBox->setEnabled(false);
        ui->ensembleMemberLabel->setEnabled(false);
    }
    else
    {
        // Change to specified ensemble member.
        ui->ensembleMemberComboBox->setEnabled(true);
        ui->ensembleMemberLabel->setEnabled(true);
        member = ui->ensembleMemberComboBox->currentText().toInt();
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
