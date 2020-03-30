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
#include "scheduler.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <QtConcurrentRun>

// local application imports
#include "util/mutil.h"
#include "data/datarequest.h"

using namespace std;


namespace Met3D
{

/******************************************************************************
***                      MSingleThreadScheduler                             ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSingleThreadScheduler::MSingleThreadScheduler()
    : MAbstractScheduler()
{
}


MSingleThreadScheduler::~MSingleThreadScheduler()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MSingleThreadScheduler::scheduleTaskGraph(MTask *task)
{
    emit schedulerIsProcessing(true);

    LOG4CPLUS_DEBUG(mlog, "Scheduling task graph for execution: "
                    << task->getRequest().toStdString());

//    printTaskGraphDepthFirst(task, 0);
    executeTaskGraphDepthFirst(task, 0);

    emit schedulerIsProcessing(false);
}


MTask *MSingleThreadScheduler::isScheduled(MScheduledDataSource *dataSource,
                                           MDataRequest request)
{
    Q_UNUSED(dataSource);
    Q_UNUSED(request);

    return nullptr;
}


void MSingleThreadScheduler::printTaskGraphDepthFirst(MTask *task, int level)
{
    if (level == 0)
    {
        LOG4CPLUS_DEBUG(mlog, "Printing Task Graph =======================");
    }

    LOG4CPLUS_DEBUG(mlog, "Level " << level << ": "
                    << task->getRequest().toStdString());

    foreach (MTask *parent, task->getAndLockParents())
    {
        printTaskGraphDepthFirst(parent, level + 1);
    }
    task->unlockParents();

    if (level == 0)
    {
        LOG4CPLUS_DEBUG(mlog, "Print of task graph finished. "
                        "=======================");
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MSingleThreadScheduler::executeTaskGraphDepthFirst(MTask *task, int level)
{
    if (level == 0)
    {
        LOG4CPLUS_DEBUG(mlog, "Executing task graph =======================");
    }

    LOG4CPLUS_DEBUG(mlog, "Level " << level << ": "
                    << task->getRequest().toStdString());

    // First execute all parents on which "task" is dependent.
    foreach (MTask *parent, task->getAndLockParents())
    {
        executeTaskGraphDepthFirst(parent, level + 1);
    }
    task->unlockParents();

    // Run the task and remove it from the graph.
    task->run();
    task->removeFromTaskGraph();
    delete task;

    if (level == 0)
    {
        LOG4CPLUS_DEBUG(mlog, "Execution of task graph finished. "
                        "=======================");
    }
}


/******************************************************************************
***                       MMultiThreadScheduler                             ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MMultiThreadScheduler::MMultiThreadScheduler()
    : MAbstractScheduler(),
//TODO: Make the number of maximum concurrent disk-reader-tasks and GPU-tasks
//      user-controllable.
      maxActiveDiskReaderTasks(2),
      currentlyActiveDiskReaderTasks(0),
      maxActiveGPUTasks(1),
      currentlyActiveGPUTasks(0),
      exitAllThreads(false)
{
    LOG4CPLUS_DEBUG(mlog, "Initializing new multithread scheduler.");
    LOG4CPLUS_DEBUG(mlog, "  > Maximum number of threads in global thread "
                    "pool: " << QThreadPool::globalInstance()->maxThreadCount());

    // Explicitly register signal type "MDataRequest" so that cross-thread
    // signals work correctly.
    qRegisterMetaType<MDataRequest>("MDataRequest");

    // Initialize monitoring of current processing status.
    busyStatus = false;
    numCurrentlyActiveTasks = 0;

    // Let a different thread from the global thread pool traverse the task
    // graph and schedule the individual tasks for execution after a new
    // task graph has been scheduled in scheduleTaskGraph().
    LOG4CPLUS_DEBUG(mlog, "  > Starting task graph traversal thread.");
    taskGraphTraversalFuture = QtConcurrent::run(
                this, &MMultiThreadScheduler::traverseTaskGraphAndEnqueueTasks);

//TODO: Which is the best number of worker threads? If maxThreadCount() returns
//      the number of CPU cores, we need one thread for the main application.
//      The task graph traversal thread doesn't require an entire core, hence
//      I take maxThreadCount() - 1 for now...
    uint numTaskExecutionThreads =
            QThreadPool::globalInstance()->maxThreadCount() - 1;

    // Start numTaskExecutionThreads from the global thread pool to execute
    // tasks that have been queued to taskQueue.
    LOG4CPLUS_DEBUG(mlog, "  > Starting " << numTaskExecutionThreads
                    << " worker threads.");
    for (uint i = 0; i < numTaskExecutionThreads; i++)
    {
        workerThreadFutures.append(
                    QtConcurrent::run(
                        this, &MMultiThreadScheduler::executeTasks, i)
                    );
    }
}


MMultiThreadScheduler::~MMultiThreadScheduler()
{
    // Signal threads that they should finish.
    LOG4CPLUS_DEBUG(mlog, "Asking scheduler worker threads to finish...");
    exitAllThreadsLock.lockForWrite();
    exitAllThreads = true;
    exitAllThreadsLock.unlock();
    taskGraphTraversalWaitCondition.wakeAll();
    taskExecutionWaitCondition.wakeAll();

    // Wait for the threads to finish.
    taskGraphTraversalFuture.waitForFinished();
    foreach (QFuture<void> future, workerThreadFutures)
        future.waitForFinished();
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MMultiThreadScheduler::scheduleTaskGraph(MTask *task)
{
    LOG4CPLUS_DEBUG(mlog, "Scheduling task graph for execution: "
                    << task->getRequest().toStdString());

    // Enqueue the incoming task graph.
    taskGraphQueueMutex.lock();
    taskGraphQueue.append(task);
    taskGraphQueueMutex.unlock();

    // Notify listening objects that the scheduler is processing tasks now.
    updateBusyStatus();

    // Tell task graph traversal thread that new items have been added to the
    // queue.
    taskGraphTraversalWaitCondition.wakeAll();
}


MTask *MMultiThreadScheduler::isScheduled(MScheduledDataSource *dataSource,
                                          MDataRequest request)
{
    MTask *task = nullptr;

    // No new task graphs should be scheduled while we query the enqueued tasks.
    QMutexLocker taskGraphQueueLocker(&taskGraphQueueMutex);
    // And no tasks should be added or removed from the task queue.
    QMutexLocker taskQueueLocker(&taskQueueMutex);

    // In the unlikely event that isScheduled() is called between a call
    // to scheduleTaskGraph() and the subsequent execution of
    // traverseTaskGraphAndEnqueueTasks() in the task graph thread, we
    // need to enforce task graph traversal here. See
    // traverseTaskGraphAndEnqueueTasks() for comments.
    while ( !taskGraphQueue.isEmpty() )
    {
        MTask *taskGraph = taskGraphQueue.takeFirst();
#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
        LOG4CPLUS_DEBUG(mlog, "Scheduler forcing traversal of task graph in "
                        "isScheduled(): "
                        << taskGraph->getDataSource() << " / "
                        << taskGraph->getRequest().toStdString());
#endif
        traverseAndEnqueueDepthFirst(taskGraph, &taskQueue);
#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
        debugPrintTaskQueue();
#endif
        taskExecutionWaitCondition.wakeAll();
    }

    // Check if the request is contained in an already scheduled task.
    if (currentlyEnqueuedTasks[dataSource].contains(request))
    {
        task = currentlyEnqueuedTasks[dataSource][request];
    }
    else if (currentlyActiveTasks[dataSource].contains(request))
    {
        task = currentlyActiveTasks[dataSource][request];
    }

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MMultiThreadScheduler::traverseTaskGraphAndEnqueueTasks()
{
    QMutex localWaitConditionMutex;

    forever
    {
        // Check if the thread should be exited.
        exitAllThreadsLock.lockForRead();
        if (exitAllThreads)
        {
            LOG4CPLUS_DEBUG(mlog, "Scheduler THREAD# (task graph traversal)"
                            " finishes execution.");
            exitAllThreadsLock.unlock();
            break;
        }
        exitAllThreadsLock.unlock();

        // Check if taskGraphQueue contains items that need to be traversed.
        taskGraphQueueMutex.lock();
        while ( !taskGraphQueue.isEmpty() )
        {
            // Get a task graph from the task graph queue.
            MTask *taskGraph = taskGraphQueue.takeFirst();

            // Traverse the graph in depth-first order and enqueue its tasks
            // in the task queue.
            taskQueueMutex.lock();
            traverseAndEnqueueDepthFirst(taskGraph, &taskQueue);
#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
            debugPrintTaskQueue();
#endif
            taskQueueMutex.unlock();

            // Wake task execution threads, in case they have gone to sleep.
            taskExecutionWaitCondition.wakeAll();
        }
        taskGraphQueueMutex.unlock();

        // Task graph queue is empty. Wait until new item is added.
        localWaitConditionMutex.lock();
        taskGraphTraversalWaitCondition.wait(&localWaitConditionMutex);
        localWaitConditionMutex.unlock();
    } // forever
}


void MMultiThreadScheduler::traverseAndEnqueueDepthFirst(
        MTask *task, QList<MTask *> *queue)
{
    // Check if the new task is a duplicate of another task that is already
    // enqueued.

    // Special case: The task graph is re-using tasks that already have been
    // scheduled. If this is a task that has already been scheduled don't
    // schedule it again!
    if (task->isScheduled()) return;

//TODO: If a duplicate task is currently processing it won't be noticed here.
//      In this case, the new task will be scheduled and executed. It will
//      be cancelled in MScheduledDataSource::processRequest().
//      To also handle tasks in currentlyActiveTasks we need a way to make sure
//      that the task result is not stored in the memory manager before its
//      child/parent links are updated (for correct memory manager reservations).
//      >> Also see "DEQUEUE: putting duplicate task on hold" in
//         dequeueFirstTaskWithoutDependency()

    if ( currentlyEnqueuedTasks[task->getDataSource()].contains(
                task->getRequest()) )
    {
#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
        LOG4CPLUS_DEBUG(mlog, "Scheduler discarding duplicate task: "
                        << task->getDataSource() << " / "
                        << task->getRequest().toStdString());
#endif
        MTask *duplicateTask =
                currentlyEnqueuedTasks[task->getDataSource()][task->getRequest()];

        // Exchange the link of all children of "task" to pointing to the
        // identified duplicate task instead of this one.
        if (task->hasChildren())
        {
            // Qt only calls getAndLockChildren() once (no deadlock due to the
            // lock) -- see http://qt-project.org/doc/qt-4.8/containers.html#foreach
            foreach (MTask *child, task->getAndLockChildren())
            {
                child->exchangeParent(task, duplicateTask);
            }
            task->unlockChildren();
        }
        else
        {
            duplicateTask->addAdditionalMemoryReservation(1);
        }

        // Delete "task" and all its parents; they are not needed anymore.
        deleteUnscheduledTaskGraph(task);

        return;
    }

    // Enqueue all parents (i.e. the dependencies) of this task first.
    foreach (MTask *parent, task->getAndLockParents())
    {
        traverseAndEnqueueDepthFirst(parent, queue);
    }
    task->unlockParents();

    // Enqueue this task.
    queue->append(task);
    currentlyEnqueuedTasks[task->getDataSource()][task->getRequest()] = task;
    task->setScheduled();
}


void MMultiThreadScheduler::deleteUnscheduledTaskGraph(MTask *task)
{
    // NOTE: This method doesn't properly remove the task from the taskgraph;
    // it simply recursively deletes all parents -- only use if the the root
    // task for which cancelUnscheduledTaskGraph() is initially called is
    // properly disconnected from all its children.

    // Special care needs to be taken if the task graph to be deleted
    // contains links to tasks that are already scheduled by another task
    // graph. Don't delete those (and their subgraphs)!
    if (task->isScheduled()) return;

    foreach (MTask *parent, task->getAndLockParents())
    {
        deleteUnscheduledTaskGraph(parent);
    }
    task->unlockParents();

    task->removeFromTaskGraph();

    // Cancel the task's input requests that were available during task
    // construction; they were reserved in the memory manager and are not
    // needed anymore.
    task->cancelInputRequestsWithoutParents();
    delete task;
}


void MMultiThreadScheduler::updateBusyStatus()
{
    QMutexLocker locker(&busyStatusMutex);

    if (busyStatus)
    {
        if (numCurrentlyActiveTasks == 0)
        {
            busyStatus = false;
            emit schedulerIsProcessing(false);
        }
    }
    else
    {
        QMutexLocker tgLocker(&taskGraphQueueMutex);
        QMutexLocker tqLocker(&taskQueueMutex);

        if (numCurrentlyActiveTasks > 0 || !taskGraphQueue.isEmpty()
                || !taskQueue.isEmpty())
        {
            busyStatus = true;
            emit schedulerIsProcessing(true);
        }
    }
}


void MMultiThreadScheduler::debugPrintTaskQueue()
{
    QString str = "\n\nTASK QUEUE:\n\n";

    foreach (MTask* task, taskQueue)
    {
        QString parentsString;
        foreach (MTask* parent, task->getAndLockParents())
        {
            QString s; s.sprintf(" %p", parent); parentsString += s;
        }
        task->unlockParents();

        QString childrenString;
        foreach (MTask* child, task->getAndLockChildren())
        {
            QString s; s.sprintf(" %p", child); childrenString += s;
        }
        task->unlockChildren();

        QString taskString;
        taskString.sprintf("* task %p [mem.res.: %i][children: %i,%s]"
                           "[parents: %i,%s],\n    > data source %p: "
                           "request %s\n",
                           task,
                           task->numAdditionalMemoryReservations(),
                           task->numChildren(),
                           childrenString.toStdString().c_str(),
                           task->numParents(),
                           parentsString.toStdString().c_str(),
                           task->getDataSource(),
                           task->getRequest().toStdString().c_str());
        str += taskString;
    }

    str += "\n";
    LOG4CPLUS_DEBUG(mlog, str.toStdString());
}


MTask *MMultiThreadScheduler::dequeueFirstTaskWithoutDependency(uint execThreadID)
{
#ifndef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
    Q_UNUSED(execThreadID);
#endif

    // QMutexLocker is used here because of the two return statements;
    // see Qt documentation.
    QMutexLocker locker(&taskQueueMutex);

    for (int i = 0; i < taskQueue.size(); i++)
    {
        MTask* task = taskQueue.at(i);
        if ( !task->hasParents() )
        {
            // Task does not have any parents, i.e. no dependencies. A
            // potential candidate for execution.

            if ( currentlyActiveTasks[task->getDataSource()].contains(
                     task->getRequest()) )
            {
#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
                LOG4CPLUS_DEBUG(mlog, "Scheduler THREAD#" << execThreadID
                                << ": DEQUEUE: putting duplicate task on hold: "
                                << task);
#endif
                // The job defined by this task is currently executed by
                // another task. NOTE: This can only happen if a duplicate task
                // is enqueued while its "sibling" is already processing see
                // comment in traverseAndEnqueueDepthFirst(). If the duplicate
                // becomes a candidate for execution while its sibling is still
                // processing, we're here... Ignore the task but leave it in
                // the queue, so it can be handled correctly later.
                continue;
            }
            else if (task->isDiskReaderTask())
            {
                if (currentlyActiveDiskReaderTasks >= maxActiveDiskReaderTasks)
                {
#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
                LOG4CPLUS_DEBUG(mlog, "Scheduler THREAD#" << execThreadID
                                << ": DEQUEUE: putting disk reader task on hold: "
                                << task);
#endif
                    continue;
                }
                else
                {
                    currentlyActiveDiskReaderTasks++;
                }
            }
            else if (task->isGPUTask())
            {
                if (currentlyActiveGPUTasks >= maxActiveGPUTasks)
                {
#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
                LOG4CPLUS_DEBUG(mlog, "Scheduler THREAD#" << execThreadID
                                << ": DEQUEUE: putting GPU task on hold: "
                                << task);
#endif
                    continue;
                }
                else
                {
                    currentlyActiveGPUTasks++;
                }
            }

#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
            LOG4CPLUS_DEBUG(mlog, "Scheduler THREAD#" << execThreadID
                            << ": DEQUEUE: accepting task: " << task);
#endif
            // Put the task into the list of currently processed tasks,
            // remove it from the queue and return.
            currentlyActiveTasks[task->getDataSource()].insert(
                        task->getRequest(), task);
            numCurrentlyActiveTasks.ref(); // increment num of active tasks
            taskQueue.removeAt(i);
            currentlyEnqueuedTasks[task->getDataSource()].remove(
                        task->getRequest());

#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
            debugPrintTaskQueue();
#endif
            return task;
        }
    }

    return nullptr;
}


void MMultiThreadScheduler::executeTasks(uint execThreadID)
{
    QMutex localWaitConditionMutex;

    forever
    {
        // Check if the thread should be exited.
        exitAllThreadsLock.lockForRead();
        if (exitAllThreads)
        {
            LOG4CPLUS_DEBUG(mlog, "Scheduler THREAD#" << execThreadID
                            << " finishes execution.");
            exitAllThreadsLock.unlock();
            break;
        }
        exitAllThreadsLock.unlock();

        // Obtain the first task without dependency from the queue.
        MTask *task = dequeueFirstTaskWithoutDependency(execThreadID);

        if (task == nullptr)
        {
            // No task without dependencies is currently available from the
            // queue -- wait until one becomes available.
            updateBusyStatus(); // this could be the last task to finish execution
            localWaitConditionMutex.lock();
            taskExecutionWaitCondition.wait(&localWaitConditionMutex);
            localWaitConditionMutex.unlock();
        }
        else
        {           
#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
            LOG4CPLUS_DEBUG(mlog, "Scheduler THREAD#" << execThreadID
                            << " starts execution of task " << task);
#endif
            // Run the task and remove it from the graph.
            task->run();

            // The task removal affects other tasks in the queue (that depend
            // on this task), hence the queue needs to be blocked.
            taskQueueMutex.lock();
            currentlyActiveTasks[task->getDataSource()].remove(
                        task->getRequest());
            numCurrentlyActiveTasks.deref(); // decrement num of active tasks
            if (task->isDiskReaderTask()) currentlyActiveDiskReaderTasks--;
            if (task->isGPUTask()) currentlyActiveGPUTasks--;
            task->removeFromTaskGraph();
            delete task;
            taskQueueMutex.unlock();

#ifdef DEBUG_OUTPUT_MULTITHREAD_SCHEDULER
            LOG4CPLUS_DEBUG(mlog, "Scheduler THREAD#" << execThreadID
                            << " finishes execution of task " << task);
#endif
            // Wake other task execution threads, in case they have gone to
            // sleep.
            taskExecutionWaitCondition.wakeAll();
        }
    }
}


}
