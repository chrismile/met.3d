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
#include "scheduleddatasource.h"

// standard library imports
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"

using namespace std;

#define ENABLE_REQUEST_PASSTHROUGH

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MScheduledDataSource::MScheduledDataSource()
    : MMemoryManagedDataSource(),
      scheduler(nullptr),
      scheduledPassThroughSource(nullptr)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MScheduledDataSource::setScheduler(MAbstractScheduler *s)
{
    if (s == nullptr)
        throw MValueError("no valid pointer to a task scheduler passed",
                          __FILE__, __LINE__);
    scheduler = s;
}


void MScheduledDataSource::requestData(MDataRequest request)
{
    assert(scheduler != nullptr);

    MTask *task = getTaskGraph(request);
    if (task->isValid())
    {
        // A valid task has been returned. This means the requested data item
        // is not available from the memory manager.

        if (task->isScheduled())
        {
            // The task is already scheduled -- increase the number of memory
            // reservations and thus unlock its child access (see
            // MTask::lockChildAccessUntilNewChildHasBeenAdded())
            task->addAdditionalMemoryReservation(1);
        }
        else
        {
            // Schedule the task for execution.
            scheduler->scheduleTaskGraph(task);
        }
    }
    else        
    {
        // An invalid task has been returned (not needed here hence deleted
        // immediately): The requested data item is already present in the
        // memory manager. It is blocked until released by the calling object.
        delete task;
        emit dataRequestCompleted(request);
    }
}


void MScheduledDataSource::processRequest(MDataRequest request,
                                          MTask *handlingTask)
{
    assert(memoryManager != nullptr);

    // Thread-safety: The memory manager cannot be changed during the lifetime
    // of this data source (see MMemoryManagedDataSource::setMemoryManager()).
    // As the memory manager itself provides thread-safe methods, this method
    // can access the memory manager without blocking.

    MDataRequestHelper rh(request);

#ifdef ENABLE_REQUEST_PASSTHROUGH
    if (rh.contains("PASS"))
    {
        rh.remove("PASS");

        // If the task requires the produced data item to be blocked for more
        // than one "consumer", let the pass-through scource reserve the item
        // accordingly.
        if (passThroughSource)
        {
            passThroughSource->reserveData(
                        rh.request(),
                        handlingTask->numAdditionalMemoryReservations());
        }

        emit dataRequestCompleted(rh.request());
        return;
    }
#endif

    // Remove all keys that are not required for the task from the request.
    // This avoids redundant processing and storage due to spurious keys.
    rh.removeAllKeysExcept(requiredKeys());

//NOTE: The following will only happen if a task is scheduled for
//      execution in MMultiThreadScheduler while its duplicate is processing.
//      See MMultiThreadScheduler::traverseAndEnqueueDepthFirst().
    // In some cases the same request task graph can be put twice or more times
    // into the scheduler queue. For example, when trajectories are displayed
    // in multiple views with the same vertical scaling, each view will emit a
    // request for normals. If the requests are the same, they will all be
    // places into the queue. If the item that is to be produced in this method
    // was already produced and stored by another task/thread, the processing
    // in produceData() is unnecessary. We can hence cancel processing here,
    // however, input requests that would be released in produceData() need to
    // be released before we cancel -- otherwise we'd get a memory leak.
    if ( memoryManager->containsData(this, rh.request()) )
    {
        for (int i = 0; i < handlingTask->numAdditionalMemoryReservations(); i++)
        {
            memoryManager->containsData(this, rh.request());
        }

        handlingTask->cancelAllInputRequests();
        return;
    }

    // produceData() needs to be implemented in a thread-safe manner in
    // derived classes.
    MAbstractDataItem *item = produceData(rh.request());
    if (item)
    {
        item->setGeneratingRequest(rh.request());

        // Store the item in the memory manager. If the item cannot be stored
        // (this happens if another thread has in the mean time stored the same
        // item), free its memory. The commands are locked by the result mutex
        // in case another thread concurrently executes getTaskGraph() with the
        // same request -- see comments in getTaskGraph().
        QMutexLocker resultLocker(&resultMutex);
        if ( !memoryManager->storeData(this, item) ) delete item;

        for (int i = 0; i < handlingTask->numAdditionalMemoryReservations(); i++)
        {
            memoryManager->containsData(this, rh.request());
        }
        resultLocker.unlock();

        // Whether the item was successfully stored or not (in which case the
        // requested item is still available, just produced by another thread),
        // it is blocked by the call to storeData() until it is released. Emit
        // "completed" request.
        emit dataRequestCompleted(request);
    }
}


MTask *MScheduledDataSource::getTaskGraph(MDataRequest request)
{
    assert(memoryManager != nullptr);

    // Check if all required keys are present in the request. It is enough to
    // check if the keys required by this particular data source instance are
    // present, the keys required by the dependencies are checked by traversing
    // the pipeline in createTaskGraph().
    MDataRequestHelper rh(request);
    if (!rh.containsAll(locallyRequiredKeys()))
    {
#ifdef ENABLE_REQUEST_PASSTHROUGH
        if (scheduledPassThroughSource)
        {
            // Request pass-through is enabled and not all keywords required by
            // this source are present in the request. Hence we pass the
            // request to the next source in the pipeline. When the task graph
            // returned by "scheduledPassThroughSource" is invalid, the data
            // item is available in memory --> return the invalid task. Else,
            // create an "empty" task (request "PASS"). On execution, this will
            // simply trigger the emission of the "dataRequestCompleted" signal
            // in processRequest(). See notes (24Oct2013, mr).

//TODO: The scheduler should be queried here for an existing dublicate
//      pass-through task as well (as done below to avoid duplicates). Problem:
//      As we don't query the memory manager first we can't determine whether a
//      returned scheduled task has already excuted .. maybe add information
//      about task status to MTask?

            MTask *t = scheduledPassThroughSource->getTaskGraph(request);
            if (t->isValid())
            {
                rh.insert("PASS", "");
                MTask *passThroughTask = new MTask(rh.request(), this);
                passThroughTask->addParent(t);
                return passThroughTask;
            }
            else
            {
                // scheduledPassThroughSource returns an invalid task, i.e. the
                // result is already stored in the memory manager.
                return t;
            }
        }
        else
        {
#endif
        QString msg = QString(
                    "Request %1 is missing required keys. Required are: %2")
                .arg(request).arg(locallyRequiredKeys().join(";"));

        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MKeyError(msg.toStdString(), __FILE__, __LINE__);
#ifdef ENABLE_REQUEST_PASSTHROUGH
        }
#endif
    }

    // Remove all keys that are not required in the request to create a unique
    // key for the memory manager (see MMemoryManagedDataSource::requestData()).
    rh.removeAllKeysExcept(requiredKeys());

    // Lock the result mutex to ensure that no other thread that by chance
    // currently executes processData() with the same request is able to store
    // the data item in the memory manager BETWEEN the calls to containsData()
    // and isScheduled() here (otherwise containsData() might return false,
    // then the item is stored, then isScheduled() returns true but the other
    // task won't memory-manager-block the item for this request anymore -->
    // memory error).
//TODO: Should the result mutex be per-task?
    QMutexLocker resultLocker(&resultMutex);

    if ( memoryManager->containsData(this, rh.request()) )
    {
        // Data available in cache -- no need to create any valid task. The
        // successful call to containsData() will block the item until it is
        // released. NOTE: An invalid task is returned (instead of a nullptr)
        // so that tasks that have this request as "parent" know that the
        // request is NOT associated with a task. This information is
        // required in case of task cancellation. See MTask::addParent().
        return new MTask(request, this, false);
    }
    else if (MTask *task = scheduler->isScheduled(this, rh.request()) )
    {
        // The already scheduled task can be at different execution stages: (A)
        // Still waiting for execution -- no problem, it will get an additional
        // child (or reservation notice). (B) Currently executed but before the
        // call to storeData() in processRequest() -- the call to
        // lockChildAccessUntilNewChildHasBeenAdded() below will cause
        // processRequest() to wait until the children links of the task have
        // been updated, then the correct number of memory reservations will be
        // issued. (C) Currently executed but after the call to storeData() --
        // then we won't arrive here as the item will already be stored in the
        // memory manager and, due to the result mutex, the above query to the
        // memory manager will have returned true.
#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
        LOG4CPLUS_DEBUG(mlog, "Scheduled data source "
                        "reusing already scheduled task: "
                        << task->getDataSource() << " / "
                        << task->getRequest().toStdString());
#endif
        task->lockChildAccessUntilNewChildHasBeenAdded();
        return task;
    }

    resultLocker.unlock();

    // Recursively create a task graph of a task representing this data source
    // and of tasks representing the required input data fields. Use the
    // original request here (including not required keys); it may contain
    // control keywords that do not influence the result, but its computation.
    return createTaskGraph(request);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MScheduledDataSource::enablePassThrough(MAbstractDataSource *s)
{
    MMemoryManagedDataSource::enablePassThrough(s);

    if (MScheduledDataSource *schedDS = dynamic_cast<MScheduledDataSource*>(s))
    {
        scheduledPassThroughSource = schedDS;
    }
}


} // namespace Met3D
