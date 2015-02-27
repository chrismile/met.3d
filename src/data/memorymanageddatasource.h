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
#ifndef MEMORYMANAGEDDATASOURCE_H
#define MEMORYMANAGEDDATASOURCE_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "abstractdatasource.h"
#include "abstractdataitem.h"
#include "abstractmemorymanager.h"
#include "datarequest.h"


namespace Met3D
{

/**
  @brief MMemoryManagedDataSource is the base class for memory managed
  data sources (i.e. those that use caching).
  */
class MMemoryManagedDataSource
        : public MAbstractDataSource, public MMemoryManagementUsingObject
{
public:
    MMemoryManagedDataSource();

    /**
      Specify the memory manager for this data source. The manager can only
      be specified once. It cannot be changed during the lifetime of the
      data source.
     */
    void setMemoryManager(MAbstractMemoryManager *manager);

    void requestData(MDataRequest request);

    MAbstractDataItem* getData(MDataRequest request);

    void releaseData(MDataRequest request);

    void releaseData(MAbstractDataItem *item);

    const QStringList& requiredKeys();

    /**
     Produces the data item corresponding to @p request.

     @note This function needs to be implemented in a <em> thread-safe </em>
     manner, i.e. all access to shared data/resources within this class needs
     to be serialized (@see
     https://qt-project.org/doc/qt-4.8/threads-reentrancy.html).
     */
    virtual MAbstractDataItem* produceData(MDataRequest request) = 0;

protected:
    MAbstractMemoryManager *memoryManager;
    MAbstractDataSource *passThroughSource;

    /**
      Needs to be implemented in derived classes. Returns a list with the
      keys required by the data source (not including the keys required by
      its sources).
     */
    virtual const QStringList locallyRequiredKeys() = 0;

    /**
      Derived classes should call this method for every data source they use as
      input. If the optional prefix is specified, the source's request keys are
      prefixed before they are passed to the source.
     */
    void registerInputSource(MAbstractDataSource *source, QString prefix = "");

    /**
      Removes all data sources with prefixes from the registry.
     */
    void deregisterPrefixedInputSources();

    /**
      Returns a pointer to the data source with the specified @p prefix.
     */
    MAbstractDataSource* getPrefixedDataSource(QString prefix);

    /**
      Call this method from derived classes to enable request pass through,
      i.e. passing the request to the next source @p s in the pipeline when
      the required keywords are not present in the request.

      @note This method is NOT thread-safe -- only call once after
      initialization of this object; do not change after other objects have
      started to request/get/release data items.
     */
    virtual void enablePassThrough(MAbstractDataSource *s);

    void reserveData(MDataRequest request, int numRequests);

private:
    QStringList requiredRequestKeys;
    QMultiMap<QString, MAbstractDataSource*> registeredDataSources;
    QReadWriteLock registeredDataSourcesLock;

    void updateRequiredKeys();
};


} // namespace Met3D

#endif // MEMORYMANAGEDDATASOURCE_H
