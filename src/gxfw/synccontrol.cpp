/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2016-2017 Bianca Tost
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
#include <QMenu>
#include <QFileDialog>

// local application imports
#include "util/mutil.h"
#include "gxfw/msystemcontrol.h"
#include "gxfw/msceneviewglwidget.h"
#include "selectdatasourcedialog.h"
#include "data/weatherpredictiondatasource.h"

using namespace std;

// Uncomment the following define to enable debug output for sync events.
//#define SYNC_DEBUG_OUTPUT

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
    overwriteAnimationImageSequence(false),
    syncID(id),
    synchronizationInProgress(false),
    forwardBackwardButtonClicked(false),
    validDateTimeHasChanged(false),
    lastFocusWidget(nullptr),
    currentSyncType(SYNC_UNKNOWN)
{
    lastInitDatetime = QDateTime();
    lastValidDatetime = QDateTime();

    selectedDataSourceActionList.clear();
    synchronizedObjects = QSet<MSynchronizedObject*>();
    synchronizedObjects.clear();

    ui->setupUi(this);    

    // Configuration control elements.
    // =========================================================================

    configurationDropdownMenu = new QMenu(this);
    // Load configuration.
    loadConfigurationAction = new QAction(this);
    loadConfigurationAction->setText(
                "load synchronisation configuration");
    configurationDropdownMenu->addAction(loadConfigurationAction);
    connect(loadConfigurationAction, SIGNAL(triggered()),
            SLOT(loadConfigurationFromFile()));

    // Save configuration.
    saveConfigurationAction = new QAction(this);
    saveConfigurationAction->setText(
                "save synchronisation configuration");
    configurationDropdownMenu->addAction(saveConfigurationAction);
    connect(saveConfigurationAction, SIGNAL(triggered()),
            SLOT(saveConfigurationToFile()));

    configurationDropdownMenu->addSeparator();

    // Select data source.
    selectDataSourcesAction = new QAction(this);
    selectDataSourcesAction->setText(
                "select data sources for allowed times and members");
    configurationDropdownMenu->addAction(selectDataSourcesAction);

    // Selected data sources.
    configurationDropdownMenu->addSeparator();
    configurationDropdownMenu->addAction("Selected data sources:")
            ->setEnabled(false);

    ui->configurationButton->setMenu(configurationDropdownMenu);

    connect(selectDataSourcesAction, SIGNAL(triggered()),
            SLOT(selectDataSources()));

    // Show menu also if the users clicks the button not only if only the arrow
    // was clicked.
    connect(ui->configurationButton, SIGNAL(clicked()),
            ui->configurationButton, SLOT(showMenu()));

    // Time control elements.
    // =========================================================================

    // Time steps for navigating valid/init time. (Default value: 6 hours).
    ui->timeStepSpinBox->setValue(6);
    ui->timeUnitsComboBox->setCurrentIndex(2);

    restrictControlToDataSources();

    // Only initialise with initTime and validTime if they are set properly.
    if (!lastInitDatetime.isNull() && !lastValidDatetime.isNull())
    {
        // Initialise with minimum init and valid time.
        setInitDateTime(lastInitDatetime);
        setValidDateTime(lastValidDatetime);
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

    timeAnimationDelaySpinBox = new QSpinBox(this);
    timeAnimationDelaySpinBox->setMinimum(10);
    timeAnimationDelaySpinBox->setMaximum(100000);
    timeAnimationDelaySpinBox->setValue(1000);
    MLabelledWidgetAction *animationDelaySpinBoxAction =
            new MLabelledWidgetAction("delay between animation steps:", "ms",
                                      timeAnimationDelaySpinBox, this);
    timeAnimationDropdownMenu->addAction(animationDelaySpinBoxAction);

    timeAnimationDropdownMenu->addSeparator();

    // "from" entry of drop down menu.
    // -------------------------------

    // Width for all "copy IT/VT to from/to" buttons.
    int widthOfCopyButtons = 30;

    timeAnimationFromWidget = new QWidget(this);

    timeAnimationFrom = new QDateTimeEdit(timeAnimationFromWidget);
    timeAnimationFrom->setDisplayFormat("ddd yyyy-MM-dd hh:mm:ss UTC");
    timeAnimationFrom->setTimeSpec(Qt::UTC);
    copyInitTimeToAnimationFromButton = new QPushButton("IT", timeAnimationFromWidget);
    copyInitTimeToAnimationFromButton->setMinimumWidth(widthOfCopyButtons);
    copyInitTimeToAnimationFromButton->setMaximumWidth(widthOfCopyButtons);
    copyInitTimeToAnimationFromButton->setToolTip("copy current init time");
    copyValidTimeToAnimationFromButton = new QPushButton("VT", timeAnimationFromWidget);
    copyValidTimeToAnimationFromButton->setMinimumWidth(widthOfCopyButtons);
    copyValidTimeToAnimationFromButton->setMaximumWidth(widthOfCopyButtons);
    copyValidTimeToAnimationFromButton->setToolTip("copy current valid time");

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
    timeAnimationTo->setDisplayFormat("ddd yyyy-MM-dd hh:mm:ss UTC");
    timeAnimationTo->setTimeSpec(Qt::UTC);
    copyInitTimeToAnimationToButton = new QPushButton("IT", timeAnimationToWidget);
    copyInitTimeToAnimationToButton->setMinimumWidth(widthOfCopyButtons);
    copyInitTimeToAnimationToButton->setMaximumWidth(widthOfCopyButtons);
    copyInitTimeToAnimationToButton->setToolTip("copy current init time");
    copyValidTimeToAnimationToButton = new QPushButton("VT", timeAnimationToWidget);
    copyValidTimeToAnimationToButton->setMinimumWidth(widthOfCopyButtons);
    copyValidTimeToAnimationToButton->setMaximumWidth(widthOfCopyButtons);
    copyValidTimeToAnimationToButton->setToolTip("copy current valid time");

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

    // As default, copy current init/valid times to from/to fields.
    copyInitToFrom();
    copyValidToTo();

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

    connect(timeAnimationLoopGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(onAnimationLoopGroupChanged(QAction*)));

    // Save animation.
    // ===============
    timeAnimationDropdownMenu->addSeparator();

    saveAnimationImagesCheckBox = new QCheckBox("Save images to files");
    saveAnimationImagesCheckBox->setToolTip(
                "Activate to automatically save an image of\n"
                "the selected view after each\n"
                "synchronisation event.");
    QHBoxLayout *saveAnimationLayout = new QHBoxLayout();
    saveAnimationLayout->addWidget(saveAnimationImagesCheckBox);
    saveAnimationLayout->setAlignment(saveAnimationLayout, Qt::AlignLeft);
    QWidget *saveAnimationWidget = new QWidget();
    saveAnimationWidget->setLayout(saveAnimationLayout);
    QWidgetAction *saveAnimationAction = new QWidgetAction(this);
    saveAnimationAction->setDefaultWidget(saveAnimationWidget);
    timeAnimationDropdownMenu->addAction(saveAnimationAction);

    saveAnimationSceneViewsComboBox = new QComboBox();
    QStringList sceneViewsIdentifiers;
    foreach (MSceneViewGLWidget *sceneView,
             MSystemManagerAndControl::getInstance()->getRegisteredViews())
    {
        sceneViewsIdentifiers << QString("view #%1").arg(sceneView->getID()+1);
    }
    saveAnimationSceneViewsComboBox->addItems(sceneViewsIdentifiers);
    QHBoxLayout *sceneViewLayout = new QHBoxLayout();
    sceneViewLayout->addWidget(saveAnimationSceneViewsComboBox);
    QWidget *sceneViewWidget = new QWidget();
    sceneViewWidget->setLayout(sceneViewLayout);
    MLabelledWidgetAction *sceneViewAction =
            new MLabelledWidgetAction("Save images of scene view:", "",
                                      sceneViewWidget, this);
    timeAnimationDropdownMenu->addAction(sceneViewAction);

    saveAnimationDirectoryLabel = new QLabel(
                MSystemManagerAndControl::getInstance()
                ->getMet3DWorkingDirectory().absoluteFilePath("images")
                );
    // Create directory to save images to if it does not exist.
    QDir().mkpath(saveAnimationDirectoryLabel->text());
    saveAnimationDirectoryLabel->setFixedWidth(175);
    // Set fixed size so the label won't expand the menu.
    saveAnimationDirectoryLabel->setToolTip(saveAnimationDirectoryLabel->text());
    adjustSaveAnimationDirectoryLabelText();
    saveAnimationDirectoryChangeButton = new QPushButton("...");
    QHBoxLayout *directoryLayout = new QHBoxLayout();
    directoryLayout->addWidget(saveAnimationDirectoryLabel);
    directoryLayout->addWidget(saveAnimationDirectoryChangeButton);
    QWidget *directoryWidget = new QWidget();
    directoryWidget->setLayout(directoryLayout);
    MLabelledWidgetAction *directoryAction =
            new MLabelledWidgetAction("to directory:", "", directoryWidget, this);
    timeAnimationDropdownMenu->addAction(directoryAction);

    saveAnimationFileNameLineEdit = new QLineEdit();
    saveAnimationFileNameLineEdit->setFixedWidth(190);
    saveAnimationFileNameLineEdit->setText("met3d-image.%it.%vt.%m");
    saveAnimationFileNameLineEdit->setToolTip(
                "Press return to save image. "
                "(Only if 'save animation' is active.)");
    saveAnimationFileExtensionComboBox = new QComboBox();
    QStringList imageFileExtensions;
    imageFileExtensions << ".png" << ".jpg" << ".bmp" << ".jpeg";
    saveAnimationFileExtensionComboBox->addItems(imageFileExtensions);
    QHBoxLayout *fileNameLayout = new QHBoxLayout();
    fileNameLayout->addWidget(saveAnimationFileNameLineEdit);
    fileNameLayout->addWidget(saveAnimationFileExtensionComboBox);
    QWidget *fileNameWidget = new QWidget();
    fileNameWidget->setLayout(fileNameLayout);
    MLabelledWidgetAction *fileNameAction =
            new MLabelledWidgetAction("file names:", "", fileNameWidget, this);
    timeAnimationDropdownMenu->addAction(fileNameAction);

    QLabel *fileNameLabel = new QLabel(
                "Use placeholder: %vt=valid time, %it=init time, %m=member");
    fileNameLabel->setToolTip("Use these placeholders to insert the according "
                              "values into the filename-string.");
    QHBoxLayout *fileNameLabelLayout = new QHBoxLayout();
    fileNameLabelLayout->addWidget(fileNameLabel);
    fileNameLabelLayout->setAlignment(fileNameLabelLayout, Qt::AlignLeft);
    QWidget *fileNameLabelWidget = new QWidget();
    fileNameLabelWidget->setLayout(fileNameLabelLayout);
    QWidgetAction *fileNameLabelAction = new QWidgetAction(this);
    fileNameLabelAction->setDefaultWidget(fileNameLabelWidget);
    timeAnimationDropdownMenu->addAction(fileNameLabelAction);

    connect(saveAnimationDirectoryChangeButton, SIGNAL(clicked()),
            SLOT(changeSaveAnimationDirectory()));
    connect(saveAnimationImagesCheckBox, SIGNAL(toggled(bool)),
            SLOT(activateTimeAnimationImageSaving(bool)));
    connect(saveAnimationSceneViewsComboBox, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(switchSelectedView(QString)));
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
}


MSyncControl::~MSyncControl()
{
    delete ui;
    selectedDataSourceActionList.clear();
    selectedDataSources.clear();
    delete configurationDropdownMenu;
    delete selectDataSourcesAction;

    // Deregister all registered synchronized object since otherwise this might
    // lead to a system crash if the actor is deleted later.
    disconnectSynchronizedObjects();
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


void MSyncControl::setEnsembleMember(const unsigned int member)
{
    int index = ui->ensembleMemberComboBox->findText(QString("%1").arg(member));
    ui->ensembleMemberComboBox->setCurrentIndex(index);
}


void MSyncControl::registerSynchronizedClass(MSynchronizedObject *object)
{
    if (object != nullptr) synchronizedObjects.insert(object);
}


void MSyncControl::deregisterSynchronizedClass(MSynchronizedObject *object)
{
    if (synchronizedObjects.contains(object))
    {
        synchronizedObjects.remove(object);
    }
}


void MSyncControl::synchronizationCompleted(MSynchronizedObject *object)
{
#ifdef SYNC_DEBUG_OUTPUT
    LOG4CPLUS_DEBUG(mlog, "SYNC: synchronizationCompleted() called by object "
                    << object);
    debugOutputSyncStatus("start of synchronizationCompleted()");
#endif

//NOTE: Each object to which in processSynchronizationEvent() a synchronization
// event was sent is allowed to call synchronizationCompleted() exactly ONCE.
// It is the responsibility of the synchronized object to make sure that no
// multiple calls are issued. In case a synchronized object calls this method
// twice, its pointer will be stored in earlyCompletedSynchronizations but never
// be removed (first call: remove from pendingSynchronizations as intended,
// second call: not contained in pendingSynchronizations anymore, stored in
// earlyCompletedSynchronizations). Then, the sync GUI is not enabled anymore
// and the system is "locked".
// HINT for DEBUGGING: enable "SYNC_DEBUG_OUTPUT" and make sure that both
// pendingSynchronizations and earlyCompletedSynchronizations are empty after
// all computations have finished. -- (mr, 03Apr2019)

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

        // Save the just completed image to file, if applicable in animation
        // mode.
        emitSaveImageSignal();

        synchronizationInProgress = false;
    }
#ifdef SYNC_DEBUG_OUTPUT
    debugOutputSyncStatus("end of synchronizationCompleted()");
#endif
}


void MSyncControl::disconnectSynchronizedObjects()
{
    foreach (MSynchronizedObject *synchronizedObject, synchronizedObjects)
    {
        synchronizedObject->synchronizeWith(nullptr);
    }
    synchronizedObjects.clear();
}


unsigned int MSyncControl::getAnimationDelay_ms()
{
    return timeAnimationDelaySpinBox->value();
}


void MSyncControl::setOverwriteAnimationImageSequence(bool overwrite)
{
    overwriteAnimationImageSequence = overwrite;
}


void MSyncControl::setAnimationTimeRange(int timeRange_sec)
{
    if (!availableInitDateTimes.empty())
    {
        timeAnimationFrom->setDateTime(availableInitDateTimes.at(0));
        timeAnimationTo->setDateTime(
                    availableInitDateTimes.at(0).addSecs(timeRange_sec));
    }
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

bool MSyncControl::animationIsActiveAndForwardDateTimeLimitHasBeenReached(
        QDateTimeEdit* dateTimeEdit)
{
    if (animationTimer->isActive())
    {
        if (dateTimeEdit->dateTime() >= timeAnimationTo->dateTime())
        {
            if (timeAnimationLoopTimeAction->isChecked())
            {
                dateTimeEdit->setDateTime(timeAnimationFrom->dateTime());
            }
            else if (timeAnimationBackForthTimeAction->isChecked())
            {
                timeAnimationReverseTimeDirectionAction->toggle();
            }
            else
            {
                stopTimeAnimation();
            }

            return true;
        }
    }

    return false;
}


bool MSyncControl::animationIsActiveAndBackwardDateTimeLimitHasBeenReached(
        QDateTimeEdit* dateTimeEdit)
{
    if (animationTimer->isActive())
    {
        if (dateTimeEdit->dateTime() <= timeAnimationFrom->dateTime())
        {
            if (timeAnimationLoopTimeAction->isChecked())
            {
                dateTimeEdit->setDateTime(timeAnimationTo->dateTime());
            }
            else if (timeAnimationBackForthTimeAction->isChecked())
            {
                timeAnimationReverseTimeDirectionAction->toggle();
            }
            else
            {
                stopTimeAnimation();
            }

            return true;
        }
    }

    return false;
}


void MSyncControl::timeForward()
{
    if (ui->stepChooseVTITComboBox->currentIndex() == 0)
    {
        // Index 0 of stepChooseVTITComboBox means that the valid time should
        // be modified by the time navigation buttons.

        if (animationIsActiveAndForwardDateTimeLimitHasBeenReached(
                    ui->validTimeEdit))
        {
            return;
        }

        applyTimeStep(ui->validTimeEdit, 1);
    }
    else if (ui->stepChooseVTITComboBox->currentIndex() == 1)
    {
        // Modify initialisation time.

        if (animationIsActiveAndForwardDateTimeLimitHasBeenReached(
                    ui->initTimeEdit))
        {
            return;
        }

        applyTimeStep(ui->initTimeEdit, 1);
    }
    else
    {
        // Both valid and init time should be changed simultaniously.

        if (animationIsActiveAndForwardDateTimeLimitHasBeenReached(
                    ui->validTimeEdit)
                || animationIsActiveAndForwardDateTimeLimitHasBeenReached(
                    ui->initTimeEdit))
        {
            return;
        }

        forwardBackwardButtonClicked = true;

        applyTimeStep(ui->validTimeEdit, 1);
        applyTimeStep(ui->initTimeEdit, 1);
    }
}


void MSyncControl::timeBackward()
{
    if (ui->stepChooseVTITComboBox->currentIndex() == 0)
    {
        if (animationIsActiveAndBackwardDateTimeLimitHasBeenReached(
                    ui->validTimeEdit))
        {
            return;
        }

        applyTimeStep(ui->validTimeEdit, -1);
    }
    else if (ui->stepChooseVTITComboBox->currentIndex() == 1)
    {
        if (animationIsActiveAndBackwardDateTimeLimitHasBeenReached(
                    ui->initTimeEdit))
        {
            return;
        }

        applyTimeStep(ui->initTimeEdit, -1);
    }
    else
    {
        if (animationIsActiveAndBackwardDateTimeLimitHasBeenReached(
                    ui->validTimeEdit)
                || animationIsActiveAndBackwardDateTimeLimitHasBeenReached(
                    ui->initTimeEdit))
        {
            return;
        }

        forwardBackwardButtonClicked = true;

        applyTimeStep(ui->validTimeEdit, -1);
        applyTimeStep(ui->initTimeEdit, -1);
    }
}


void MSyncControl::selectDataSources()
{
    // Ask the user for data sources to which times and ensemble members the
    // sync control should be restricted to.
    MSelectDataSourceDialog dialog(MSelectDataSourceDialogType::SYNC_CONTROL,
                                   this);
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

    // Restrict the sync control's allowed init/valid times to those available
    // from the selected data sources; also update the current from/to times
    // in the animation menu.
    restrictControlToDataSources(selectedDataSources);
    copyInitToFrom();
    copyValidToTo();
}


void MSyncControl::saveConfigurationToFile(QString filename)
{
    if (filename.isEmpty())
    {
        QString directory =
                MSystemManagerAndControl::getInstance()
                ->getMet3DWorkingDirectory().absoluteFilePath("config/synccontrol");
        QDir().mkpath(directory);
        filename = QFileDialog::getSaveFileName(
                    MGLResourcesManager::getInstance(),
                    "Save sync control configuration",
                    QDir(directory).absoluteFilePath(getID()
                                                     + ".synccontrol.conf"),
                    "Sync control configuration files (*.synccontrol.conf)");

        if (filename.isEmpty())
        {
            return;
        }
    }

    LOG4CPLUS_DEBUG(mlog, "Saving configuration to " << filename.toStdString());

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);

    // Overwrite if the file exists.
    if (QFile::exists(filename))
    {
        QFile::remove(filename);
    }

    settings->beginGroup("FileFormat");
    // Save version id of Met.3D.
    settings->setValue("met3dVersion", met3dVersionString);
    settings->endGroup();


    settings->beginGroup("MSyncControl");
    saveConfiguration(settings);
    settings->endGroup();

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been saved.");
}


void MSyncControl::loadConfigurationFromFile(QString filename)
{
    if (filename.isEmpty())
    {
        QString filename = QFileDialog::getOpenFileName(
                    MGLResourcesManager::getInstance(),
                    "Load sync control configuration",
                    MSystemManagerAndControl::getInstance()
                    ->getMet3DWorkingDirectory().absoluteFilePath("config/synccontrol"),
                    "Sync control configuration files (*.synccontrol.conf)");

        if (filename.isEmpty())
        {
            return;
        }
    }

    LOG4CPLUS_DEBUG(mlog, "Loading configuration from "
                    << filename.toStdString());

    QSettings *settings = new QSettings(filename, QSettings::IniFormat);

    QStringList groups = settings->childGroups();
    if ( !groups.contains("MSyncControl") )
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("The selected file does not contain configuration data "
                    "for sync control.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        delete settings;
        return;
    }

    settings->beginGroup("MSyncControl");
    loadConfiguration(settings);
    settings->endGroup();

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been loaded.");
}


void MSyncControl::saveConfiguration(QSettings *settings)
{
    settings->beginGroup("General");
    settings->setValue("initTime",
                       ui->initTimeEdit->dateTime());
    settings->setValue("validTime",
                       ui->validTimeEdit->dateTime());
    settings->setValue("stepChooseVtIt",
                      ui->stepChooseVTITComboBox->currentText());
    settings->setValue("timeStep", QString::number(ui->timeStepSpinBox->value())
                       + " " + ui->timeUnitsComboBox->currentText());
    settings->setValue("showMean", ui->showMeanCheckBox->isChecked());
    settings->setValue("dataSources", selectedDataSources);
    settings->setValue("selectedMember",
                       ui->ensembleMemberComboBox->currentText());
    settings->endGroup();

    settings->beginGroup("Animation");
    settings->setValue("animationTimeStep",
                      timeAnimationDelaySpinBox->value());
    settings->setValue("fromTime",
                      timeAnimationFrom->dateTime());
    settings->setValue("toTime",
                      timeAnimationTo->dateTime());
    settings->setValue("timeAnimationLoop",
                      timeAnimationLoopGroup->actions().indexOf(
                          timeAnimationLoopGroup->checkedAction()));
    settings->setValue("reverseTimeDirection",
                      timeAnimationReverseTimeDirectionAction->isChecked());
    settings->endGroup();

    settings->beginGroup("TimeSeries");
    settings->setValue("fileName", saveAnimationFileNameLineEdit->text());
    settings->setValue("fileExtension",
                       saveAnimationFileExtensionComboBox->currentText());
    settings->setValue("sceneView", saveAnimationSceneViewsComboBox->currentText());
    settings->setValue("directory", saveAnimationDirectoryLabel->toolTip());
    settings->endGroup();
}


void MSyncControl::loadConfiguration(QSettings *settings)
{
    settings->beginGroup("General");

    ui->stepChooseVTITComboBox->setCurrentIndex(
                ui->stepChooseVTITComboBox->findText(
                    settings->value("stepChooseVtIt", "valid").toString()));
    QStringList timeStep =
            settings->value("timeStep", "6 hours").toString().split(" ");
    if (timeStep.size() != 2)
    {
        timeStep.clear();
        timeStep << "6" << "hours";
    }
    ui->timeStepSpinBox->setValue(timeStep.first().toInt());
    ui->timeUnitsComboBox->setCurrentIndex(
                ui->timeUnitsComboBox->findText(timeStep.last()));
    ui->showMeanCheckBox->setChecked(settings->value("showMean", false).toBool());
    selectedDataSources =
            settings->value("dataSources",
                            MSystemManagerAndControl::getInstance()
                            ->getDataSourceIdentifiers()).toStringList();
    restrictToDataSourcesFromSettings(selectedDataSources);

    // Load times after restricting the sync control since otherwise the times
    // might not be set correctly.
    ui->initTimeEdit->setDateTime(
                settings->value("initTime", QDateTime()).value<QDateTime>());

    ui->validTimeEdit->setDateTime(
                settings->value("validTime", QDateTime()).value<QDateTime>());

    unsigned int selectedMember = settings->value("selectedMember", 0).toUInt();
    if (availableEnsembleMembers.contains(selectedMember))
    {
        setEnsembleMember(selectedMember);
    }
    else
    {
        QMessageBox::warning(this, "Error",
                             QString("Member '%1' is not available.\n"
                                     "Reset to 0.").arg(selectedMember));
    }

    settings->endGroup();

    settings->beginGroup("Animation");
    timeAnimationDelaySpinBox->setValue(
                settings->value("animationTimeStep", 1000).toInt());
    timeAnimationFrom->setDateTime(settings->value("fromTime").toDateTime());
    timeAnimationTo->setDateTime(settings->value("toTime").toDateTime());
    timeAnimationLoopGroup->actions().at(
                settings->value("timeAnimationLoop", 0).toInt())->setChecked(true);
    timeAnimationReverseTimeDirectionAction->setChecked(
                settings->value("reverseTimeDirection", false).toBool());
    settings->endGroup();

    settings->beginGroup("TimeSeries");

    saveAnimationFileNameLineEdit->setText(
                settings->value("fileName",
                                "met3d-image.%it.%vt.%m").toString());

    int index = saveAnimationFileExtensionComboBox->findText(
            settings->value("fileExtension", ".png").toString());
    if (index < 0)
    {
        QString fileExt = settings->value("fileExtension", ".png").toString();
        QMessageBox::warning(this, this->getID(),
                             "The file extension '" + fileExt
                             + "' is invalid.\n"
                               "Setting file extension to '.png'.");
        index = saveAnimationFileExtensionComboBox->findText(".png");
    }
    saveAnimationFileExtensionComboBox->setCurrentIndex(index);

    QString sceneView0 = saveAnimationSceneViewsComboBox->itemText(0);
    index = saveAnimationSceneViewsComboBox->findText(
                settings->value("sceneView", sceneView0).toString());
    if (index < 0)
    {
        QString sceneView = settings->value("sceneView", sceneView0).toString();
        QMessageBox::warning(this, this->getID(),
                             "Scene view '" + sceneView
                             + "' is invalid.\nSetting scene view to '"
                             + sceneView0 + "'.");
        index = 0;
    }
    saveAnimationSceneViewsComboBox->setCurrentIndex(index);

    QString defaultDir = QDir::home().absoluteFilePath("met3d/screenshots");
    QString dir = settings->value("directory", defaultDir).toString();
    QFileInfo info(dir);
    if (!(info.isDir() && info.isWritable()))
    {
        QMessageBox::warning(this, this->getID(),
                             "'" + dir
                             + "' either no directory or not writable.\n"
                               "Setting image series save directory to '"
                             + defaultDir + "'.");
        dir = defaultDir;
        // Create default directory to save screenshots to if it does not exist
        // already.
        QDir().mkpath(defaultDir);
    }
    saveAnimationDirectoryLabel->setToolTip(dir);
    saveAnimationDirectoryLabel->setText(dir);
    adjustSaveAnimationDirectoryLabelText();

    settings->endGroup();
}


void MSyncControl::restrictToDataSourcesFromSettings(
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
                msgBox.setText(dataSourceID + " does not exist.");
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
                           " Use data of all data sources given.");
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
                msgBox.setText("No suitable data sources available.");
                msgBox.exec();
                return;
            }
        }
    }

    // Restrict the sync control's allowed init/valid times to those available
    // from the selected data sources; also update the current from/to times
    // in the animation menu.
    restrictControlToDataSources(suitableDataSources);
    copyInitToFrom();
    copyValidToTo();
}


void MSyncControl::restrictControlToDataSources(
        QStringList selectedDataSources, bool resetInitValidToFirstAvailable)
{
    MSystemManagerAndControl* sysMC = MSystemManagerAndControl::getInstance();

    // Clear GUI drop down menu displaying selected data sources.
    for (QAction *action : selectedDataSourceActionList)
    {
        configurationDropdownMenu->removeAction(action);
    }
    selectedDataSourceActionList.clear();

    // TODO (bt, 23Feb2017): If updated to Qt 5.0 use QSets and unite instead of
    // lists and contains since for version 4.8 there is no qHash method for QDateTime
    // and thus it is not possible to use toSet on QList<QDateTime>.
    // (See: http://doc.qt.io/qt-5/qhash.html#qHashx)
    availableInitDateTimes.clear();
    availableValidDateTimes.clear();
    availableEnsembleMembers.clear();

    // If no list of data sources is passed to this function, use all data
    // sources registered in the system.
    if (selectedDataSources.empty())
    {
        QStringList availableDataSources = sysMC->getDataSourceIdentifiers();

        // Check for each data sourcs if it is suitable to restrict control.
        // Since in the list of all data sources might be data sources without
        // time and member informations.
        for (const QString& dataSourceID : availableDataSources)
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

    // If no data sources are available return from this method.
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

    this->selectedDataSources = selectedDataSources;

    // From each data source in the list, determine the available init and
    // valid times.
    for (QString dataSourceID : selectedDataSources)
    {
        MWeatherPredictionDataSource* source =
                dynamic_cast<MWeatherPredictionDataSource*>
                (sysMC->getDataSource(dataSourceID));
        // Add selected data source as action to the configuration drop down
        // menu and insert the action into a list for easy remove from the menu.
        selectedDataSourceActionList.append(
                    configurationDropdownMenu->addAction(dataSourceID));

        QList<MVerticalLevelType> levelTypes = source->availableLevelTypes();
        for (MVerticalLevelType levelType : levelTypes)
        {
            QStringList variables = source->availableVariables(levelType);

            for (QString var : variables)
            {
                currentInitTimes = source->availableInitTimes(levelType, var);

                if (currentInitTimes.empty())
                {
                    continue;
                }

                for (QDateTime initTime : currentInitTimes)
                {
                    if (!availableInitDateTimes.contains(initTime))
                    {
                        availableInitDateTimes.append(initTime);
                    }

                    currentValidTimes = source->availableValidTimes(
                                levelType, var, initTime);
                    if (currentValidTimes.empty())
                    {
                        continue;
                    }

                    for (QDateTime validTime : currentValidTimes)
                    {
                        if (!availableValidDateTimes.contains(validTime))
                        {
                            availableValidDateTimes.append(validTime);
                        }
                    } // validTimes
                } // initTimes

                availableEnsembleMembers =
                        availableEnsembleMembers.unite(
                            source->availableEnsembleMembers(levelType, var));

                // Sort available times for finding the nearest time if the
                // user selects a missing time.
                qSort(availableInitDateTimes);
                qSort(availableValidDateTimes);
            } // variables
        } // levelTypes
    } // dataSources

    // Store previous date times of init/valid GUI fields.
    QDateTime previousInitTime = ui->initTimeEdit->dateTime();
    QDateTime previousValidTime = ui->validTimeEdit->dateTime();
    int previousEnsembleMember = ensembleMember();

    // Search for earliest and latest init date values to restrict the init
    // time edits to them.
    QDateTime earliestDateTime = availableInitDateTimes.first();
    QDateTime latestDateTime = earliestDateTime;
    foreach (QDateTime dateTime, availableInitDateTimes)
    {
        earliestDateTime = min(dateTime, earliestDateTime);
        latestDateTime = max(dateTime, latestDateTime);
    }
    ui->initTimeEdit->setDateRange(earliestDateTime.date(), latestDateTime.date());
    // Set time range to full day since otherwise it is not possible to change
    // the time properly for the first and last day of the range.
    ui->initTimeEdit->setTimeRange(QTime(0,0,0), QTime(23,59,59));
    lastInitDatetime = earliestDateTime;

    // The same for valid times.
    earliestDateTime = availableValidDateTimes.first();
    latestDateTime = earliestDateTime;
    foreach (QDateTime dateTime, availableValidDateTimes)
    {
        earliestDateTime = min(dateTime, earliestDateTime);
        latestDateTime = max(dateTime, latestDateTime);
    }
    ui->validTimeEdit->setDateRange(earliestDateTime.date(), latestDateTime.date());
    // Set time range to full day since otherwise it is not possible to change
    // the time properly for the first and last day of the range.
    ui->validTimeEdit->setTimeRange(QTime(0,0,0), QTime(23,59,59));
    lastValidDatetime = earliestDateTime;

    // Check if previous init/valid times are still in new range of available
    // times -- if not, reset to first available init/valid time. Also reset
    // to first available if "resetInitValidToFirstAvailable" is true.
    if (!availableInitDateTimes.contains(previousInitTime)
            || resetInitValidToFirstAvailable)
    {
       ui->initTimeEdit->setDateTime(lastInitDatetime);
    }
    if (!availableValidDateTimes.contains(previousValidTime)
            || resetInitValidToFirstAvailable)
    {
       ui->validTimeEdit->setDateTime(lastValidDatetime);
    }
    updateTimeDifference();

    QStringList memberList;
    QList<unsigned int> intMemberList;
    memberList.clear();
    ui->ensembleMemberComboBox->clear();
    // Get list of member to be able to sort them from smallest to greatest
    // value.
    intMemberList = availableEnsembleMembers.toList();
    qSort(intMemberList);
    for (unsigned int member : intMemberList)
    {
        memberList.append(QString("%1").arg(member));
    }
    ui->ensembleMemberComboBox->addItems(memberList);
    // Restore previous ensemble member, if possible.
    setEnsembleMember(previousEnsembleMember);

    // Disable all data source entries since they are supposed to be just labels.
    for (QAction *action : selectedDataSourceActionList)
    {
        action->setEnabled(false);
    }
}


void MSyncControl::timeAnimationAdvanceTimeStep()
{
    // This method is called by the animationTimer to advance the animation
    // to the next time step.

#ifdef DIRECT_SYNCHRONIZATION
    // In case the previous request hasn't been completed yet, do nothing!
    // We will advance the time step the next time the timer times out.
    if (synchronizationInProgress)
    {
        return;
    }
#endif

    if (timeAnimationReverseTimeDirectionAction->isChecked())
    {
        timeBackward();
    }
    else
    {
        timeForward();
    }
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

        // Set the init/valid datetime edits to the animation start time.
        if (timeAnimationLoopGroup->checkedAction()
                == timeAnimationSinglePassAction)
        {
            if (timeAnimationReverseTimeDirectionAction->isChecked())
            {
                setAnimationTimeToStartTime(timeAnimationTo->dateTime());
            }
            else
            {
                setAnimationTimeToStartTime(timeAnimationFrom->dateTime());
            }
        }

        // Pass to scene view that generates animation images: Force to
        // overwrite images in case files already exist?
        getSceneViewChosenInAnimationPane()->forceOverwriteImageSequence(
                    overwriteAnimationImageSequence);

        // Emit the "timeAnimationBegins" signal.
        emit timeAnimationBegins();

        // Save the current image (the next image will be stored after the next
        // synchronization event, triggered by the animationTimer, has
        // completed) UNLESS the previous synchronization request is still in
        // progress -- in this case the image will be stored upon completion
        // of synchronization (see synchronizationCompleted()).
        if (!synchronizationInProgress)
        {
            emitSaveImageSignal();
        }

        // Start the animation timer. It will periodically call
        // timeAnimationAdvanceTimeStep().
        animationTimer->start(timeAnimationDelaySpinBox->value());
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

    // Reset option to overwrite existing images.
    getSceneViewChosenInAnimationPane()->forceOverwriteImageSequence(false);

    // Emit the "timeAnimationEnds" signal.
    emit timeAnimationEnds();
}


void MSyncControl::startTimeAnimationProgrammatically(bool saveImages)
{
    saveAnimationImagesCheckBox->setChecked(saveImages);

    ui->animationPlayButton->setChecked(true);
    startTimeAnimation();
}


void MSyncControl::onValidDateTimeChange(const QDateTime &datetime)
{
    // Only restrict valid time to available valid if times they are set yet.
    if (!availableValidDateTimes.empty())
    {
        // Reseting to previous time - do nothing.
        if (lastValidDatetime == datetime)
        {
            return;
        }
        // Check if selected time is part of available times. If not, reset to
        // previous time.
        if (!availableValidDateTimes.contains(datetime))
        {
            handleMissingDateTime(ui->validTimeEdit, &availableValidDateTimes,
                                  datetime, &lastValidDatetime);
            return;
        }

    }
    lastValidDatetime = datetime;
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

    // Ignore updates on valid time if init and valid time are changed
    // simultaniously, since synchronization of valid time is handled by process
    // event of init time.
    if ((ui->stepChooseVTITComboBox->currentIndex() == 2)
            && (animationTimer->isActive() || forwardBackwardButtonClicked) )
    {
        validDateTimeHasChanged = true;
        return;
    }
    else
    {
        synchronizationInProgress = true;
        processSynchronizationEvent(SYNC_VALID_TIME, QVariant(datetime));
    }
#else
    emit beginSynchronization();
    updateTimeDifference();
    emit validDateTimeChanged(datetime);
    emit endSynchronization();
    emitSaveImageSignal();
#endif
}


void MSyncControl::onInitDateTimeChange(const QDateTime &datetime)
{
    // Only restrict init time to available init times if they are set yet.
    if (!availableInitDateTimes.empty())
    {
        // Reseting to previous time - do nothing.
        if (lastInitDatetime == datetime)
        {
            // Don't prevent synchonisation event if both valid and init time
            // should change and valid time has changed.
            if ((ui->stepChooseVTITComboBox->currentIndex() == 2)
                    && !validDateTimeHasChanged)
            {
                return;
            }
        }
        // Check if selected time is part of available times. If not, reset to
        // previous time.
        if (!availableInitDateTimes.contains(datetime))
        {
            handleMissingDateTime(ui->initTimeEdit, &availableInitDateTimes,
                                  datetime, &lastInitDatetime);
            return;
        }
    }
    lastInitDatetime = datetime;
#ifdef DIRECT_SYNCHRONIZATION
    if (synchronizationInProgress) return;
    synchronizationInProgress = true;

    // If init and valid time are changed simultaniously, handle synchronization
    // of both in one event. Check for index of combo box is not enough, since
    // the user is still able to change one time manually (without forward or
    // backward button).
    if ((ui->stepChooseVTITComboBox->currentIndex() == 2)
            && (animationTimer->isActive() || forwardBackwardButtonClicked) )
    {
        forwardBackwardButtonClicked = false;
        validDateTimeHasChanged = false;
        processSynchronizationEvent(SYNC_INIT_VALID_TIME, QVariant(datetime));
    }
    else
    {
        processSynchronizationEvent(SYNC_INIT_TIME, QVariant(datetime));
    }
#else
    emit beginSynchronization();
    updateTimeDifference();
    emit initDateTimeChanged(datetime);
    emit endSynchronization();
    emitSaveImageSignal();
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
    emitSaveImageSignal();
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


void MSyncControl::onAnimationLoopGroupChanged(QAction *action)
{
    if (action == timeAnimationSinglePassAction)
    {
        saveAnimationImagesCheckBox->setEnabled(true);
    }
    else
    {
        if (saveAnimationImagesCheckBox->isChecked())
        {
            saveAnimationImagesCheckBox->setChecked(false);
        }
        saveAnimationImagesCheckBox->setEnabled(false);
    }
}


MSceneViewGLWidget* MSyncControl::getSceneViewChosenInAnimationPane()
{
    unsigned int sceneViewID =
            saveAnimationSceneViewsComboBox->currentText().split("#").at(1).toUInt();

    MSceneViewGLWidget *sView = nullptr;
    foreach (sView, MSystemManagerAndControl::getInstance()->getRegisteredViews())
    {
        if (sView->getID() + 1 == sceneViewID)
        {
            break;
        }
    }

    return sView;
}


void MSyncControl::activateTimeAnimationImageSaving(bool activate)
{
    MSceneViewGLWidget *animSceneView = getSceneViewChosenInAnimationPane();

    if (activate)
    {
        saveAnimationSceneView = animSceneView;

        connect(this, SIGNAL(imageOfTimeAnimationReady(QString, QString)),
                saveAnimationSceneView,
                SLOT(saveTimeAnimationImage(QString, QString)));

        // Connect editable save animation gui elements to achieve saving if
        // one is changed.
        connect(saveAnimationFileNameLineEdit, SIGNAL(returnPressed()),
                this, SLOT(emitSaveImageSignal()));

        if (!animSceneView->isVisible())
        {
            QMessageBox::warning(
                        this, "Warning",
                        QString("View #%1 selected in time animation pane is not visible.\n"
                                "Please select another view or view layout.\n"
                                "(No images will be saved.)").arg(animSceneView->getID()+1));
            saveAnimationImagesCheckBox->setChecked(false);
            return;
        }
    }
    else
    {
        disconnect(this, SIGNAL(imageOfTimeAnimationReady(QString, QString)),
                   saveAnimationSceneView,
                   SLOT(saveTimeAnimationImage(QString, QString)));

        // Disconnect editable save animation gui elements.
        disconnect(saveAnimationFileNameLineEdit, SIGNAL(returnPressed()),
                   this, SLOT(emitSaveImageSignal()));

        animSceneView->forceOverwriteImageSequence(false);
    }
}


void MSyncControl::switchSelectedView(QString viewID)
{
    if (saveAnimationImagesCheckBox->isChecked())
    {
        unsigned int sceneViewID = viewID.split("#").at(1).toUInt();
        MSceneViewGLWidget *currentSceneView;
        foreach (currentSceneView,
                 MSystemManagerAndControl::getInstance()->getRegisteredViews())
        {
            if (currentSceneView->getID() + 1 == sceneViewID)
            {
                break;
            }
        }

        // Check if current view is visible. If not, deactivate auto save.
        // (This results in s disconnect-call, thus disconnect needs only to be
        // called if selected view is visible.)
        if (!currentSceneView->isVisible())
        {
            QMessageBox::warning(
                        this, "Warning",
                        QString("View #%1 is not visible.\n"
                                "Please select another view or view layout.\n"
                                "(No images will be saved.)").arg(sceneViewID));
            saveAnimationImagesCheckBox->setChecked(false);
            return;
        }
        // Disconnect previous scene view.
        disconnect(this, SIGNAL(imageOfTimeAnimationReady(QString, QString)),
                   saveAnimationSceneView,
                   SLOT(saveTimeAnimationImage(QString, QString)));

        saveAnimationSceneView = currentSceneView;

        // Connect selected scene view.
        connect(this, SIGNAL(imageOfTimeAnimationReady(QString, QString)),
                saveAnimationSceneView,
                SLOT(saveTimeAnimationImage(QString, QString)));
    }
}


void MSyncControl::changeSaveAnimationDirectory()
{
    QString path = QFileDialog::getExistingDirectory(
                this, "Select directory in which image file shall be stored",
                saveAnimationDirectoryLabel->toolTip());
    if (path != "")
    {
        // Only change to directory to which Met.3D has write access.
        if (QFileInfo(path).isWritable())
        {
            saveAnimationDirectoryLabel->setText(path);
            saveAnimationDirectoryLabel->setToolTip(path);
            adjustSaveAnimationDirectoryLabelText();
        }
        else
        {
            QMessageBox msg;
            msg.setWindowTitle("Error");
            msg.setText("No write access to ''" + path + "''.");
            msg.setIcon(QMessageBox::Warning);
            msg.exec();
            return;
        }
    }
}


void MSyncControl::adjustSaveAnimationDirectoryLabelText()
{
    QString path = saveAnimationDirectoryLabel->text();
    int textWidth = saveAnimationDirectoryLabel->fontMetrics().width(path);
    if (textWidth > saveAnimationDirectoryLabel->width())
    {
        int dotsWidth =
                saveAnimationDirectoryLabel->fontMetrics().width("...");
        while (textWidth + dotsWidth > saveAnimationDirectoryLabel->width())
        {
            path.chop(1);
            textWidth =
                    saveAnimationDirectoryLabel->fontMetrics().width(path);
        }
        saveAnimationDirectoryLabel->setText(path + "...");
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MSyncControl::applyTimeStep(QDateTimeEdit *dte, int sign)
{
    QDateTime time = dte->dateTime();
    int timeUnit = ui->timeUnitsComboBox->currentIndex();
    int timeStep = ui->timeStepSpinBox->value();
    switch (timeUnit)
    {
    case 0: // seconds
    {
        dte->setDateTime(time.addSecs(sign * timeStep));
        break;
    }
    case 1: // minutes
    {
        timeStep *= 60;
        dte->setDateTime(time.addSecs(sign * timeStep));
        break;
    }
    case 2: // hours
    {
        timeStep *= 3600;
        dte->setDateTime(time.addSecs(sign * timeStep));
        break;
    }
    case 3: // days
    {
        dte->setDateTime(time.addDays(sign * timeStep));
        break;
    }
    case 4: // months
    {
        dte->setDateTime(time.addMonths(sign * timeStep));
        break;
    }
    case 5: // years
    {
        dte->setDateTime(time.addYears(sign * timeStep));
        break;
    }
    default:
    {
        break;
    }
    }
}


void MSyncControl::updateTimeDifference()
{
    QDateTime validTime = ui->validTimeEdit->dateTime();
    QDateTime initTime  = ui->initTimeEdit->dateTime();
    int timeDifferenceSecs = initTime.secsTo(validTime);
    int timeDifferenceHrs = timeDifferenceSecs / 3600.;
    timeDifferenceSecs = fmod(timeDifferenceSecs, 3600.);
    int timeDifferenceMin = timeDifferenceSecs / 60.;
    timeDifferenceSecs = fmod(timeDifferenceSecs, 60.);
    QString s = QString("%1:%2:%3 hrs from").arg(
                timeDifferenceHrs, 2, 10, QLatin1Char('0')).arg(
                timeDifferenceMin, 2, 10, QLatin1Char('0')).arg(
                timeDifferenceSecs, 2, 10, QLatin1Char('0'));
    ui->differenceValidInitLabel->setText(s);
}


void MSyncControl::handleMissingDateTime(QDateTimeEdit *dte,
                                         QList<QDateTime> *availableDatetimes,
                                         QDateTime datetime,
                                         QDateTime *lastDatetime)
{
    QDateTime newDatetime;
    if (datetime < availableDatetimes->first())
    {
        newDatetime = availableDatetimes->first();
    }
    else if (datetime > availableDatetimes->last())
    {
        newDatetime = availableDatetimes->last();
    }
    else
    {
        // Moving forward in time.(Find next time step bigger than current one.)
        if (datetime > *lastDatetime)
        {
            foreach (QDateTime availableTime, *availableDatetimes)
            {
                newDatetime = availableTime;
                if (availableTime > datetime)
                {
                    break;
                }
            }
        }
        // Moving backward in time.(Find next time step smaller than current one.)
        else
        {
            newDatetime = availableDatetimes->first();
            foreach (QDateTime availableTime, *availableDatetimes)
            {
                if (availableTime > datetime)
                {
                    break;
                }
                newDatetime = availableTime;
            }
        }
    }
    dte->setDateTime(newDatetime);
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
#ifdef SYNC_DEBUG_OUTPUT
    debugOutputSyncStatus("start of processSynchronizationEvent()");
#endif

    // Begin synchronization: disable sync GUI (unless the event is caused by
    // the animationTimer; in this case the GUI remains active so the user can
    // stop the animation), tell scenes that sync begins (so they can block
    // redraws).
    lastFocusWidget = QApplication::focusWidget();
    currentSyncType = syncType;
    if ( !animationTimer->isActive() ) setSynchronizationGUIEnabled(false);
    beginSceneSynchronization();

    if ( (syncType == SYNC_VALID_TIME) || (syncType == SYNC_INIT_TIME)
         || (syncType == SYNC_INIT_VALID_TIME))
    {
        updateTimeDifference();
    }

    QVector<QVariant> syncVariantVector(0);

    // Append current sync variant.
    syncVariantVector.append(syncVariant);

    // For simultanious init and valid time synchronisation append current valid
    // time in addition.
    if (syncType == SYNC_INIT_VALID_TIME)
    {
        syncVariantVector.append(QVariant(ui->validTimeEdit->dateTime()));
    }

    // Send sync info to each registered synchronized object. Collect those
    // objects that will process the sync request (they return true).
    foreach (MSynchronizedObject *syncObj, synchronizedObjects)
    {
#ifdef SYNC_DEBUG_OUTPUT
        LOG4CPLUS_DEBUG(mlog, "SYNC: sending sync info to object " << syncObj);
#endif
        if ( syncObj->synchronizationEvent(syncType, syncVariantVector) )
        {
            pendingSynchronizations.insert(syncObj);
#ifdef SYNC_DEBUG_OUTPUT
            LOG4CPLUS_DEBUG(mlog, "SYNC: object " << syncObj << " accepted "
                            "sync info.");
#endif
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

#ifdef SYNC_DEBUG_OUTPUT
    debugOutputSyncStatus("end of processSynchronizationEvent()");
#endif
}


void MSyncControl::setTimeSynchronizationGUIEnabled(bool enabled)
{
    ui->initTimeEdit->setEnabled(enabled);
    ui->validTimeEdit->setEnabled(enabled);
    ui->timeBackwardButton->setEnabled(enabled);
    ui->timeForwardButton->setEnabled(enabled);
    ui->timeStepSpinBox->setEnabled(enabled);
    ui->timeUnitsComboBox->setEnabled(enabled);
    ui->stepChooseVTITComboBox->setEnabled(enabled);
}


void MSyncControl::setSynchronizationGUIEnabled(bool enabled)
{
    ui->syncFrame->setEnabled(enabled);
    ui->timeBackwardButton->blockSignals(!enabled);
    ui->timeForwardButton->blockSignals(!enabled);
}


void MSyncControl::emitSaveImageSignal()
{
    if (!saveAnimationImagesCheckBox->isChecked())
    {
        return;
    }

    // Get content of file name line edit.
    QString filename = saveAnimationFileNameLineEdit->text();

    // Replace placeholders with their according values.
    filename.replace("%it", QString("IT%1").arg(
                         initDateTime().toString(Qt::ISODate)));
    filename.replace("%vt", QString("VT%1").arg(
                         validDateTime().toString(Qt::ISODate)));
    QString memberString = QString("M%1").arg(ensembleMember());
    // Use 'mean' instead of selected ensemble member if mean is checked.
    if (ensembleMember() == -1)
    {
        memberString = "mean";
    }
    filename.replace("%m", memberString);
    // Use tool tip to get directory since the text of the label might be
    // shorten and only the tool tip holds the whole path.
    emit imageOfTimeAnimationReady(
                saveAnimationDirectoryLabel->toolTip(),
                filename + saveAnimationFileExtensionComboBox->currentText());
}


void MSyncControl::setAnimationTimeToStartTime(QDateTime startDateTime)
{
    if (ui->stepChooseVTITComboBox->currentIndex() == 0)
    {
        // Index 0 of stepChooseVTITComboBox means that the valid time should
        // be modified by the time navigation buttons.
        ui->validTimeEdit->setDateTime(startDateTime);
    }
    else if (ui->stepChooseVTITComboBox->currentIndex() == 1)
    {
        // Modify initialisation time.
        ui->initTimeEdit->setDateTime(startDateTime);
    }
    else
    {
        ui->initTimeEdit->setDateTime(startDateTime);
        ui->validTimeEdit->setDateTime(startDateTime);
    }

}


void MSyncControl::debugOutputSyncStatus(QString callPoint)
{
    QString s = QString("SYNC: status at call point %1:\n").arg(callPoint);

    s += QString("\ncurrent sync type: %1\n").arg(currentSyncType);

    s += "\nregistered synchronized objects:\n";
    for (MSynchronizedObject* o : synchronizedObjects)
    {
        // https://stackoverflow.com/questions/8881923/how-to-convert-a-pointer-value-to-qstring
        s += QString("%1 / ").arg((quintptr)o, QT_POINTER_SIZE * 2, 16,
                                  QChar('0'));
    }

    s += "\npending synchronizations:\n";
    for (MSynchronizedObject* o : pendingSynchronizations)
    {
        s += QString("%1 / ").arg((quintptr)o, QT_POINTER_SIZE * 2, 16,
                                  QChar('0'));
    }

    s += "\nearly completed synchronizations:\n";
    for (MSynchronizedObject* o : earlyCompletedSynchronizations)
    {
        s += QString("%1 / ").arg((quintptr)o, QT_POINTER_SIZE * 2, 16,
                                  QChar('0'));
    }

    s += "\n\n";

    LOG4CPLUS_DEBUG(mlog, s.toStdString());
}

} // namespace Met3D
