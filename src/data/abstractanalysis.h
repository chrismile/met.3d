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
#ifndef ABSTRACTANALYSIS_H
#define ABSTRACTANALYSIS_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "scheduleddatasource.h"
#include "weatherpredictiondatasource.h"
#include "datarequest.h"
#include "abstractmemorymanager.h"
#include "scheduler.h"


namespace Met3D
{

class MNWPMultiVarActor;


/**
 @brief Abstract base class for classes that store results from an analysis
 data source.
 */
class MAnalysisResult
        : public MAbstractDataItem
{
public:
    MAnalysisResult() : MAbstractDataItem() {}

    unsigned int getMemorySize_kb();

    QStringList textResult;
};


/**
  @brief Abstract base class for data sources that implement data analysis
  tasks.

  @note As in many cases analysis requests include a mouse position that is
  unlikely to occur twice, caching is only of limited use for analysis data
  sources. Analysis data sources should hence in general be connected to an
  analysis-only memory manager using only a small amount of memory (so the
  results are deleted after a while and not "cached forever").
  */
class MAnalysisDataSource
        : public MScheduledDataSource
{
    Q_OBJECT

public:
    MAnalysisDataSource();

    MAnalysisResult* getData(MDataRequest request)
    { return static_cast<MAnalysisResult*>
                (MScheduledDataSource::getData(request)); }

    /**
      Instructs this data source to use @p ds as a data source whose request
      keys will be prefixed by @p prefix in any request. This allows the
      data source to use multiple data sources with equal request keys but
      different request values.
     */
    void addDataSource(QString prefix, MWeatherPredictionDataSource* ds);

    /**
      Remove all data sources added with @ref addDataSource().
     */
    void clearDataSources();

protected:

};


/**
 @brief Abstract base class for modules acting as a broker between an actor and
 an @ref MAnalysisDataSource(). Any instance "I" of a derived class is
 connected to an @ref MNWPMultiVarActor and triggers analysis on request of the
 actor. "I" has access to the actors' actor variables and can thus hide the
 complexity of assembling a suitable pipeline request and emitting an analysis
 request into the pipeline from the actor. The method @ref displayResult()
 implements a suitable way to display the results of the analysis.
 */
class MAnalysisControl
        : public QObject
{
    Q_OBJECT

public:
    MAnalysisControl(MNWPMultiVarActor *actor);
    ~MAnalysisControl();

    /**
      Call this method from the attached @ref MNWPMultiVarActor instance.
      @p analysisRequest contains only parameters required for the analysis
      (e.g. a position). @p run() makes use of @ref prepareRequest(),
      @ref createAnalysisSource() and @ref updateAnalysisSourceInputs()
      to keep the attached @ref MAnalysisDataSource instance up to date and
      to contruct a full pipeline request.
     */
    void run(MDataRequest analysisRequest);

    /**
      Implement this method with a suitable way to display the result of
      the analysis.
     */
    virtual void displayResult(MAnalysisResult *result) = 0;

    /**
      Set the memory manager used for the attached @ref MAnalysisDataSource.
     */
    void setMemoryManager(MAbstractMemoryManager *manager)
    { memoryManager = manager; }

    /**
      Set the scheduler used for the attached @ref MAnalysisDataSource.
     */
    void setScheduler(MAbstractScheduler *s) { scheduler = s; }

public slots:
    /**
      Called when a requested compuation has completed. If the request was
      emitted via run(), displayResult() is called.
     */
    void requestCompleted(MDataRequest request);

protected:
    /**
      Implement this method to create a full pipeline request for
      @ref MAnalysisDataSource::requestData().
     */
    virtual MDataRequest prepareRequest(MDataRequest analysisRequest) = 0;

    /**
      Implement this method to create a new instance of the @ref
      MAnalysisDataSource attached to this control.
     */
    virtual MAnalysisDataSource* createAnalysisSource() = 0;

    /**
      Implement this method to update the @ref MAnalysisDataSource s data
      inputs from the actor variables.
     */
    virtual void updateAnalysisSourceInputs() = 0;

    void setDisplayWidget(QWidget *widget);

    void setDisplayTitle(const QString &title);

    MNWPMultiVarActor *actor;

    MAnalysisDataSource* analysisSource;
    MAbstractMemoryManager *memoryManager;
    MAbstractScheduler *scheduler;

    /** Data request information. */
    QSet<MDataRequest> pendingRequests;

    struct MRequestQueueInfo
    {
        MDataRequest request;
        bool available;
    };
    QQueue<MRequestQueueInfo> pendingRequestsQueue; // to ensure correct request order

    /** Dock widget to display results. */
    QDockWidget *dock;
};


} // namespace Met3D

#endif // ABSTRACTANALYSIS_H
