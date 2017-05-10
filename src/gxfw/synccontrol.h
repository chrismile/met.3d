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
#ifndef SYNCCONTROL_H
#define SYNCCONTROL_H

// standard library imports

// related third party imports
#include <QtCore>
#include <QtGui>

// local application imports
#include "util/mstopwatch.h"

namespace Ui {
class MSyncControl;
}

namespace Met3D
{

class MSynchronizedObject;
class MSceneViewGLWidget;

enum MSynchronizationType
{
    SYNC_INIT_TIME = 0,
    SYNC_VALID_TIME = 1,
    SYNC_INIT_VALID_TIME = 2,
    SYNC_ENSEMBLE_MEMBER = 3,
    SYNC_UNKNOWN = 4
};


/**
 @brief MLabelledWidgetAction provides custom entries (=actions) for QMenus.
 A widget that should be displayed can be specified, as well as text that
 appears before and after the widget.
 */
class MLabelledWidgetAction : public QWidgetAction
{
    Q_OBJECT

public:
    MLabelledWidgetAction(const QString& labelFront, const QString& labelBack,
                          QWidget* customWidget, QWidget *parent = nullptr);

    QWidget* getCustomWidget() { return customWidget; }

private:
    QWidget *customWidget;
};


/**
 @brief MSyncControl provides time and ensemble settings with which individual
 actors can synchronize. The functionality has been separated from
 @ref MSceneControl to allow for comprehensive synchronization possibilities.

 @todo MSyncControl should query all connected actors for available ensemble
 members.
 */
class MSyncControl : public QWidget
{
    Q_OBJECT
    
public:
    explicit MSyncControl(QString id, QWidget *parent = 0);

    ~MSyncControl();

    /**
      Returns the current valid time.
     */
    QDateTime validDateTime() const;

    void setValidDateTime(const QDateTime &dateTime);

    /**
      Returns the current initialisation time (forecast base time).
     */
    QDateTime initDateTime() const;

    void setInitDateTime(const QDateTime &dateTime);

    void copyValidTimeToTimeAnimationFromTo();

    /**
     Returns the current ensemble member. If the ensemble mode is set to
     "mean", returns -1.
     */
    int ensembleMember() const;

    QString getID() { return syncID; }

    void registerSynchronizedClass(MSynchronizedObject *object);

    void deregisterSynchronizedClass(MSynchronizedObject *object);

    void synchronizationCompleted(MSynchronizedObject *object);

public slots:
    /**
      Advance scene time by the value specified in @p ui->timeStepComboBox.
      */
    void timeForward();

    /**
      Backward version of @ref timeForward().
      */
    void timeBackward();

    /**
      Opens dialog to select from which data sources to draw valid times,
      init times and members to restrict control to.
      */
    void selectDataSources();

    /**
      @brief Checks @param selectedDataSources for consistency and prints error
      messages if a given data source id has no corresponding data source or if
      the data source does not contain any init times, valid times and ensemble
      members.

      If @selectedDataSources does not contain any, loadDataSourcesFromFrontend
      tests all register data sources for data sources with init times, valid
      times and ensemble members. If the method cannot find any suitable
      data source it prints a error message and returns.
      */
    void restrictToDataSourcesFromFrontend(QStringList selectedDataSources);

    /**
      @brief Fetches init and valid times and members from the data sources
      given by their IDs stored in @param selectedDataSources if present.

      If @param selectedDataSources is empty, it is assumed to use all available
      data sources. It is essential that if the data sources are given that they
      are valid data sources containing init times, valid times and ensemble
      members. But if loadDataSourcesTimesAndMembers uses all available data
      sources, it checks whether they contain init times, valid times and
      ensemble members and quits quietly if no suitable data source was found.
     */
    void retrictControlToDataSources(
            QStringList selectedDataSources = QStringList());

    /**
      Advance time (forward or backward, depending on settings) in animation
      mode (called by the animation timer).
      */
    void timeAnimationAdvanceTimeStep();

    /**
      Start animation over time.
      */
    void startTimeAnimation();

    /**
      Stop animation over time.
      */
    void stopTimeAnimation();

signals:
    /**
     Emitted before any of the synchronization signals below are emitted. Can
     be used, for instance, to block operations during synchonization.
     */
    void beginSynchronization();

    /**
     Emitted after any of the synchronization signals below have been emitted.
     */
    void endSynchronization();

    /**
     Emitted whenever the valid time changes.
     */
    void validDateTimeChanged(const QDateTime &datetime);

    /**
     Emitted whenever the init time changes.
     */
    void initDateTimeChanged(const QDateTime &datetime);

    /**
     Emitted whenever the current ensemble member or ensemble mode changes. If
     the ensemble mode is "mean", a "-1" is transmitted.
     */
    void ensembleMemberChanged(const int member);

    void imageOfTimeAnimationReady(QString path, QString fileName);

protected slots:
    void onValidDateTimeChange(const QDateTime &datetime);

    void onInitDateTimeChange(const QDateTime &datetime);

    void onEnsembleModeChange(const int foo);

    /**
      Copy initial time to @ref timeAnimationFrom.
      */
    void copyInitToFrom();

    /**
      Copy valid time to @ref timeAnimationFrom.
      */
    void copyValidToFrom();

    /**
      Copy inital time to @ref timeAnimationTo.
      */
    void copyInitToTo();

    /**
      Copy valid time to @ref timeAnimationTo.
      */
    void copyValidToTo();

    void onAnimationLoopGroupChanged(QAction *action);

    void activateTimeAnimationImageSaving(bool activate);
    void saveTimeAnimation();
    void switchSelectedView(QString viewID);

    void changeSaveTADirectory();
    void adjustSaveTADirLabelText();

private:
    /**
      Used by @ref timeForward() and @ref timeBackward() to apply a change to a
      QDateTimeEdit (which should be either @p ui->validTimeEdit or @p
      ui->initTimeEdit).
      */
    void applyTimeStep(QDateTimeEdit *dte, int sign);

    /**
     Updates the label that displays the time difference between valid and init
     time.
     */
    void updateTimeDifference();

    void handleMissingDateTime(QDateTimeEdit *dte,
                               QList<QDateTime> *availableDatetimes,
                               QDateTime datetime, QDateTime *lastDatetime);

    void beginSceneSynchronization();

    void endSceneSynchronization();

    void processSynchronizationEvent(MSynchronizationType syncType,
                                     QVariant syncVariant);

    /**
      Enables/disables the GUI elements that control time. (Used to disable
      time control during time animation).
     */
    void setTimeSynchronizationGUIEnabled(bool enabled);

    void setSynchronizationGUIEnabled(bool enabled);

    void emitSaveImageSignal();

    void setAnimationTimeToStartTime(QDateTime startDateTime,
                                     QDateTime endDateTime);

    Ui::MSyncControl *ui;

    // Maps index of ui->timeStepComboBox to seconds (see constructor and
    // applyTimeStep()).
    int *timeStepIndexToSeconds;

    // Properties to control time animations.
    QMenu *timeAnimationDropdownMenu;
    QSpinBox *timeAnimationTimeStepSpinBox;
    QWidget *timeAnimationFromWidget;
    QWidget *timeAnimationToWidget;
    QHBoxLayout *timeAnimationFromLayout;
    QHBoxLayout *timeAnimationToLayout;
    QDateTimeEdit *timeAnimationFrom;
    QDateTimeEdit *timeAnimationTo;
    QPushButton *copyInitTimeToAnimationFromButton;
    QPushButton *copyValidTimeToAnimationFromButton;
    QPushButton *copyInitTimeToAnimationToButton;
    QPushButton *copyValidTimeToAnimationToButton;
    QActionGroup *timeAnimationLoopGroup;
    QAction *timeAnimationSinglePassAction;
    QAction *timeAnimationLoopTimeAction;
    QAction *timeAnimationBackForthTimeAction;
    QAction *timeAnimationReverseTimeDirectionAction;
    QTimer *animationTimer;

    // Properties to control saving of images of time serie.
    QCheckBox          *saveTimeAnimationCheckBox;
    QLineEdit          *saveTAFileNameLineEdit;
    QComboBox          *saveTAFileExtensionComboBox;
    QLabel             *saveTADirectoryLabel;
    QPushButton        *saveTADirectoryChangeButton;
    QComboBox          *saveTASceneViewsComboBox;
    MSceneViewGLWidget *saveTASceneView;

    QString syncID;

    bool synchronizationInProgress;
    bool forwardBackwardButtonClicked;
    /**
     * Handle sync event if valid and init time should be updated, but only
     * valid time changes due to restriction to the data set.
     */
    bool validDateTimeHasChanged;
    QWidget *lastFocusWidget;
    MSynchronizationType currentSyncType;
    QSet<MSynchronizedObject*> synchronizedObjects;
    QSet<MSynchronizedObject*> pendingSynchronizations;
    QSet<MSynchronizedObject*> earlyCompletedSynchronizations;    

    // Properties to control configuration.
    QMenu *configurationDropdownMenu;
    QAction *selectDataSourcesAction;
    QDateTime lastInitDatetime;
    QDateTime lastValidDatetime;
    QList<QDateTime> availableInitDatetimes;
    QList<QDateTime> availableValidDatetimes;
    QSet<unsigned int> availableEnsembleMembers;
    QList<QAction*> selectedDataSourceActionList;

#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
#endif
};


/**

 */
class MSynchronizedObject
{
public:
    MSynchronizedObject() { }

    /**
       Handles synchronization event. The type of the synchronization event is
       given by @param syncType while @param data stores the data for updating.
       @param data is implemented as a vector to handle the simultanious
       synchronization event of init(index 0) and valid(index 1) time.
     */
    virtual bool synchronizationEvent(
            MSynchronizationType syncType, QVector<QVariant> data) = 0;
};

} // namespace Met3D

#endif // SYNCCONTROL_H
