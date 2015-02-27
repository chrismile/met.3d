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
#ifndef SCHEDULEDDATASOURCE_H
#define SCHEDULEDDATASOURCE_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "memorymanageddatasource.h"
#include "abstractdataitem.h"
#include "abstractmemorymanager.h"
#include "datarequest.h"
#include "task.h"
#include "scheduler.h"


namespace Met3D
{


/**
  @brief MScheduledDataSource is the base class for all memory managed AND
  scheduled data sources (i.e. computation of the result is controlled by
  the global task scheduler).

  @note Currently (only) processRequest() is thread-safe.
  */
class MScheduledDataSource : public MMemoryManagedDataSource
{
public:
    MScheduledDataSource();

    void setScheduler(MAbstractScheduler *s);

    void requestData(MDataRequest request) override;

    /**
      Calls the implementation of @ref produceData() to produce the requested
      data item and, if successful, stores the data item in the memory manager.

      @note This function is thread-safe. It is usually called from
      @ref MTask::run(), which may be executed by a multi-threaded scheduler
      (e.g. @ref MMultiThreadScheduler).

      @todo Is there a better solution for "NO_KEY_CHECK" (mr, 15Apr2014)?
     */
    void processRequest(MDataRequest request, MTask *handlingTask=nullptr);

    MTask* getTaskGraph(MDataRequest request);

    virtual MTask* createTaskGraph(MDataRequest request) = 0;

protected:
    MAbstractScheduler *scheduler;
    MScheduledDataSource *scheduledPassThroughSource;

    void enablePassThrough(MAbstractDataSource *s) override;

private:
    QMutex resultMutex;
};


} // namespace Met3D

#endif // SCHEDULEDDATASOURCE_H
