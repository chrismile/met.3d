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
#include "task.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "scheduleddatasource.h"
#include "util/mutil.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTask::MTask(MDataRequest request, MScheduledDataSource *dataSource, bool valid)
    : valid(valid),
      scheduled(false),
      numberChildrenAtScheduleTime(0),
      request(request),
      dataSource(dataSource),
      parentsMutex(QMutex::Recursive),
      gpuTask(false),
      diskReaderTask(false),
      additionalMemoryReservations(0),
      lockChildAccessUntilNewChild(false)
{
}


MTask::~MTask()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MTask::setScheduled()
{
    QMutexLocker locker(&childrenMutex);

    scheduled = true;
    numberChildrenAtScheduleTime = children.size();
}


void MTask::addParent(MTask *task)
{
    QMutexLocker locker(&parentsMutex);
    if (task->isValid())
    {
        parents.append(task);
        task->addChild(this);

        // Remember input sources and requests in case the task needs to be
        // cancelled and reserved input requests need to be released.
        inputRequestsWithParents[task->getDataSource()].append(
                    task->getRequest());
    }
    else
    {
        inputRequestsWithoutParents[task->getDataSource()].append(
                    task->getRequest());

        delete task;
    }
}


void MTask::run()
{    
    dataSource->processRequest(request, this);
}


const QList<MTask*> &MTask::getAndLockParents()
{
    parentsMutex.lock();
    return parents;
}


void MTask::unlockParents()
{
    parentsMutex.unlock();
}


const QList<MTask*> &MTask::getAndLockChildren()
{
    childrenMutex.lock();
    return children;
}


void MTask::unlockChildren()
{
   childrenMutex.unlock();
}


int MTask::numAdditionalMemoryReservations()
{
    QMutexLocker locker(&childrenMutex);
    return additionalMemoryReservations
            + children.size() - numberChildrenAtScheduleTime;
}


void MTask::addAdditionalMemoryReservation(int numReservations)
{
    QMutexLocker locker(&lockChildAccessUntilNewChildMutex);

    additionalMemoryReservations += numReservations;

    if (lockChildAccessUntilNewChild)
    {
        lockChildAccessUntilNewChild = false;
        childrenMutex.unlock();
    }
}


bool MTask::hasParents()
{
    QMutexLocker locker(&parentsMutex);
    return parents.size() > 0;
}


int MTask::numParents()
{
    QMutexLocker locker(&parentsMutex);
    return parents.size();
}


bool MTask::hasChildren()
{
    QMutexLocker locker(&childrenMutex);
    return children.size() > 0;
}


int MTask::numChildren()
{
    QMutexLocker locker(&childrenMutex);
    return children.size();
}


void MTask::exchangeParent(MTask *oldParent, MTask *newParent)
{
    QMutexLocker locker(&parentsMutex);
    parents.removeAll(oldParent);
    parents.append(newParent);
    newParent->addChild(this);
}


void MTask::removeFromTaskGraph()
{
    QMutexLocker lockerP(&parentsMutex);
    for (int i = 0; i < parents.size(); i++) parents[i]->removeChild(this);
    lockerP.unlock();

    QMutexLocker lockerC(&childrenMutex);
    for (int i = 0; i < children.size(); i++) children[i]->removeParent(this);
}


void MTask::cancelAllInputRequests()
{
    foreach (MScheduledDataSource* dataSource, inputRequestsWithParents.keys())
    {
        foreach (MDataRequest r, inputRequestsWithParents.value(dataSource))
        {
            dataSource->releaseData(r);
        }
    }

    cancelInputRequestsWithoutParents();
}


void MTask::cancelInputRequestsWithoutParents()
{
    foreach (MScheduledDataSource* dataSource, inputRequestsWithoutParents.keys())
    {
        foreach (MDataRequest r, inputRequestsWithoutParents.value(dataSource))
        {
            dataSource->releaseData(r);
        }
    }
}


void MTask::lockChildAccessUntilNewChildHasBeenAdded()
{
    childrenMutex.lock();
    QMutexLocker locker(&lockChildAccessUntilNewChildMutex);
    lockChildAccessUntilNewChild = true;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTask::addChild(MTask *task)
{
    QMutexLocker locker(&lockChildAccessUntilNewChildMutex);
    if (lockChildAccessUntilNewChild)
    {
        lockChildAccessUntilNewChild = false;
    }
    else
    {
        childrenMutex.lock();
    }
    children.append(task);
    childrenMutex.unlock();
}


void MTask::removeChild(MTask *task)
{
    QMutexLocker locker(&childrenMutex);
    if (children.contains(task)) children.removeAll(task);
}


void MTask::removeParent(MTask *task)
{
    QMutexLocker locker(&parentsMutex);
    if (parents.contains(task)) parents.removeAll(task);
}

} // namespace Met3D
