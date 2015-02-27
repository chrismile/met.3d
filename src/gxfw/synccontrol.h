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

enum MSynchronizationType
{
    SYNC_INIT_TIME = 0,
    SYNC_VALID_TIME = 1,
    SYNC_ENSEMBLE_MEMBER = 2,
    SYNC_UNKNOWN = 3
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
      Starts an animation over time.
      */
    void startTimeAnimation();

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

protected slots:
    void onValidDateTimeChange(const QDateTime &datetime);

    void onInitDateTimeChange(const QDateTime &datetime);

    void onEnsembleModeChange(const int foo);

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

    void beginSceneSynchronization();

    void endSceneSynchronization();

    void processSynchronizationEvent(MSynchronizationType syncType,
                                     QVariant syncVariant);

    Ui::MSyncControl *ui;

    // Maps index of ui->timeStepComboBox to seconds (see constructor and
    // applyTimeStep()).
    int *timeStepIndexToSeconds;

    // Timer to control animations.
    QTimer *animationTimer;

    QString syncID;

    bool synchronizationInProgress;
    QWidget *lastFocusWidget;
    MSynchronizationType currentSyncType;
    QSet<MSynchronizedObject*> synchronizedObjects;
    QSet<MSynchronizedObject*> pendingSynchronizations;
    QSet<MSynchronizedObject*> earlyCompletedSynchronizations;

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

    virtual bool synchronizationEvent(
            MSynchronizationType syncType, QVariant data) = 0;
};

} // namespace Met3D

#endif // SYNCCONTROL_H
