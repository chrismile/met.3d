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
#ifndef TASK_H
#define TASK_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "datarequest.h"


namespace Met3D
{

class MScheduledDataSource;

/**
  @brief MTask implements a node of a task graph. MTask references a single
  computational task (defined by a request to a data source and executed by
  calling run()) and can have parents and children to store a task graph.

  @note The MTask class itself is NOT thread-safe. However, an instance
  should only be handled by a single thread (see @ref
  MMultiThreadScheduler::executeTasks()). Care needs to be taken with respect
  to @ref removeFromTaskGraph(), which affects other tasks in a task graph
  (make sure that only one instance of removeFromTaskGraph() is executed
  simultaneously in a task graph).
  */
class MTask
{
public:
    MTask(MDataRequest request, MScheduledDataSource* dataSource,
          bool valid=true);

    virtual ~MTask();

    bool isValid() { return valid; }

    bool isScheduled() { return scheduled; }

    void setScheduled();

    void setGPUTask() { gpuTask = true; }

    void setDiskReaderTask() { diskReaderTask = true; }

    void addParent(MTask *task);

    MDataRequest getRequest() const { return request; }

    MScheduledDataSource* getDataSource() const { return dataSource; }

    /**
      Executes the task by calling @ref MScheduledDataSource::processRequest().

      @note Don't call any other methods after the task has run!
     */
    void run();

    bool isGPUTask() const { return gpuTask; }

    bool isDiskReaderTask() const { return diskReaderTask; }

    const QList<MTask*>& getAndLockParents();

    void unlockParents();

    const QList<MTask*>& getAndLockChildren();

    void unlockChildren();

    int numAdditionalMemoryReservations();

    /**
      Use this method to let the task know that @p numReservations additional
      reservations are required for the data item that will be computed. This
      happens e.g. when another actor requests the data item associated with
      this task. The data item then needs to be blocked one more time in the
      memory manager.
     */
    void addAdditionalMemoryReservation(int numReservations);

    bool hasParents();

    int numParents();

    bool hasChildren();

    int numChildren();

    void exchangeParent(MTask *oldParent, MTask *newParent);

    /**
      Removes the links to parent and children tasks.
     */
    void removeFromTaskGraph();

    void cancelAllInputRequests();

    void cancelInputRequestsWithoutParents();

    /**
      Locks all access to methods that use the "children" list until a new
      child or an additional memory reservation has been added.

      Call addChild() or addAdditionalMemoryReservation() as soon as possible
      after this method from the same thread that called this method to avoid
      other threads to wait for child access.
     */
    void lockChildAccessUntilNewChildHasBeenAdded();

protected:
    void addChild(MTask *task);

    void removeChild(MTask *task);

    void removeParent(MTask *task);

private:
    bool valid;
    bool scheduled;
    int numberChildrenAtScheduleTime;

    // The request that generated this data item.
    MDataRequest request;

    MScheduledDataSource *dataSource;

    QList<MTask*> parents;
    QMutex parentsMutex;
    QList<MTask*> children;
    QMutex childrenMutex;

    // A list of input requests (corresponding to the parents) is kept for
    // the case that parts of the task graph are cancelled and the input
    // requests need to be released.
    QHash< MScheduledDataSource*, QList<MDataRequest> > inputRequestsWithParents;
    QHash< MScheduledDataSource*, QList<MDataRequest> > inputRequestsWithoutParents;

    // Information for the task scheduler: Is this task using the GPU? Is this
    // task reading data from disk? The scheduler can decide how many tasks
    // that access a certain resource can be executed simultaneously.
    bool gpuTask;
    bool diskReaderTask;

    int additionalMemoryReservations;
    bool lockChildAccessUntilNewChild;
    QMutex lockChildAccessUntilNewChildMutex;
};


} // namespace Met3D

#endif // TASK_H
