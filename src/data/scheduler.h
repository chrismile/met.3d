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
#ifndef SCHEDULER_H
#define SCHEDULER_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "task.h"


namespace Met3D
{

/**
  @brief MAbstractScheduler is the base class for task schedulers. Derived
  classes must implement @ref scheduleTaskGraph().
  */
class MAbstractScheduler
{
public:
    MAbstractScheduler() {}

    virtual ~MAbstractScheduler() {}

    /**
      Schedules a task graph @p task for (asynchroneous) execution and returns.

      Must be reimplemented in derived classes.
     */
    virtual void scheduleTaskGraph(MTask *task) = 0;


    /**
      Queries whether a task with the specified data source and request has
      already been scheduled for execution. If yes, the corresponding task is
      returned. If no, a @p nullptr is returned.

      Must be reimplemented in derived classes.
     */
    virtual MTask* isScheduled(MScheduledDataSource* dataSource,
                               MDataRequest request) = 0;
};


/**
  @brief MSingleThreadScheduler immediately executes a scheduled task graph
  in the main thread (simple recursive depth first graph traversal).
  */
class MSingleThreadScheduler : public MAbstractScheduler
{
public:
    MSingleThreadScheduler();

    virtual ~MSingleThreadScheduler();

    void scheduleTaskGraph(MTask *task) override;

    /**
      Always returns a @p nullptr, as tasks are immediately executed in the
      call to @ref scheduleTaskGraph().
     */
    MTask* isScheduled(MScheduledDataSource* dataSource,
                       MDataRequest request) override;

    static void printTaskGraphDepthFirst(MTask *task, int level);

private:   
    void executeTaskGraphDepthFirst(MTask *task, int level);
};


/**
  @brief MMultiThreadScheduler employs N=(#CPU cores - 1) worker threads to
  execute the task graphs. The main application thread continues without having
  to wait for the result to be finished. Once a data item is available, @ref
  MScheduledDataSource emits a signal.
  */
class MMultiThreadScheduler : public MAbstractScheduler
{
public:
    MMultiThreadScheduler();

    virtual ~MMultiThreadScheduler();

    /**
      Schedule a new task graph for multi-threaded execution.
     */
    void scheduleTaskGraph(MTask *task) override;

    MTask* isScheduled(MScheduledDataSource* dataSource,
                       MDataRequest request) override;

private:
    // Queue for incoming task graphs and corresponding mutex. We need a mutex
    // here (and no QReadWriteLock) as enqueue and dequeue operations modify
    // the queue. The thread that calls scheduleTaskGraph() writes to the
    // queue ("posts" a new task graph"), the graph traversal thread removes
    // items from the queue.
    QList<MTask*> taskGraphQueue;
    QMutex taskGraphQueueMutex;

    QWaitCondition taskGraphTraversalWaitCondition;
    QFuture<void> taskGraphTraversalFuture;

    /**
      This method is run in a separate thread. It traverses new task graphs
      that are scheduled for execution with @ref scheduleTaskGraph() and
      inserts the tasks in the graph in the @ref taskQueue.
     */
    void traverseTaskGraphAndEnqueueTasks();

    /**
      Recursive depth-first traversal of a task graph.
     */
    void traverseAndEnqueueDepthFirst(MTask *task, QList<MTask*> *queue);

    void deleteUnscheduledTaskGraph(MTask *task);

    // Task queue-related member variables. All access must be blocked with
    // taskQueueMutex.
    QMutex taskQueueMutex;
    QList<MTask*> taskQueue;
    QHash< MScheduledDataSource*, QHash<MDataRequest, MTask*> > currentlyActiveTasks;
    QHash< MScheduledDataSource*, QHash<MDataRequest, MTask*> > currentlyEnqueuedTasks;
    int maxActiveDiskReaderTasks;
    int currentlyActiveDiskReaderTasks;
    int maxActiveGPUTasks;
    int currentlyActiveGPUTasks;

    /**
      Print the contents of the task queue to the log. For debug purposes.
     */
    void debugPrintTaskQueue();

    /**
      Take the first task from @ref taskQueue that does not have any dependency
      (i.e. parent task) that needs to be executed before the task.
     */
    MTask* dequeueFirstTaskWithoutDependency(uint execThreadID);

    QWaitCondition taskExecutionWaitCondition;

    /**
      This method is started for each worker thread in the constructor.
      Queries the @ref taskQueue for an item without dependency (via
      @ref dequeueFirstTaskWithoutDependency() and executes the task.
     */
    void executeTasks(uint execThreadID);

    // To correctly quit the tasks on destruction of the scheduler.
    QList< QFuture<void> > workerThreadFutures;
    bool exitAllThreads;
    QReadWriteLock exitAllThreadsLock;
};


} // namespace Met3D

#endif // SCHEDULER_H
