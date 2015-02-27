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
#ifndef LRUMEMORYMANAGER_H
#define LRUMEMORYMANAGER_H

// standard library imports

// related third party imports
#include <QtCore>
#include "qtpropertymanager.h"

// local application imports
#include "abstractmemorymanager.h"
#include "abstractdataitem.h"
#include "datarequest.h"

namespace Met3D
{


/**
  @brief An implementation of a memory manager with a "least recently used"
  (LRU) caching policy.

  Always follow the order

  1. @ref storeData() or @ref containsData()
  2. @ref getData()
  3. @ref releaseData()
  */
class MLRUMemoryManager : public MAbstractMemoryManager
{
    Q_OBJECT

public:
    MLRUMemoryManager(QString identifier, unsigned int allowedMemoryUsage_kb);

    virtual ~MLRUMemoryManager();

    /**
      Store the data item in the memory manager. Returns @p false if an item
      is already stored with the given item's request key.

      @note storeData() ALWAYS blocks the item until @ref releaseData() is
      called on the request key (or the item itself). Blocking is done for
      both successful stores as well as for "already contained" items.
     */
    bool storeData(
            MMemoryManagementUsingObject* owner, MAbstractDataItem *item);

    /**
      Is an item with the request key @p request available?

      If yes, the item is blocked until @ref releaseData() is called on the
      request.
     */
    bool containsData(
            MMemoryManagementUsingObject* owner, MDataRequest request);

    /**
      Returns the item stored under the given request.

     Never call this method without calling @ref storeData() or @ref
     containsData() on the request before!
     */
    MAbstractDataItem* getData(
            MMemoryManagementUsingObject* owner, MDataRequest request);

    void releaseData(
            MMemoryManagementUsingObject* owner, MDataRequest request);

    void releaseData(MAbstractDataItem *item);

    /**
      Delete all released but still cached items from memory.

      Mainly for debug purposes.
     */
    void clearCache();

public slots:
    void propertyEvent(QtProperty *property);

protected:
    QString identifier;

    /** Dictionary of active data items. */
    QHash<MDataRequest, MAbstractDataItem*> activeDataItems;
    /** Reference counter for each data item, if 0 data item is released. */
    QHash<MDataRequest, int> referenceCounter;

    /** Released (=cached) data items, can be deleted at any time. */
    QHash<MDataRequest, MAbstractDataItem*> releasedDataItems;
    /** Queue giving the order in which data items are deleted if need be. */
    QList<MDataRequest> releasedDataItemsQueue;

    /** Amount of system memory in kb the data loader is allowed to consume. */
    unsigned int systemMemoryLimit_kb;
    /** Amount of currently consumed memory. */
    unsigned int systemMemoryUsage_kb;

    /** Mutex to protect the above defined cache dictionaries. A single mutex
        is sufficient, as all QHast, QList and int objects need to be in sync
        and hence need to be protected together. */
    QMutex memoryCacheMutex;

    /** Properties to display information in the system control. */
    QtProperty *updateProperty;
    QtProperty *memoryStatusProperty;
    QtProperty *itemStatusProperty;
    QtProperty *dumpMemoryContentProperty;
    QtProperty *clearCacheProperty;

    /**
      Updates the status display in the system control.
     */
    void updateStatusDisplay();

    void dumpMemoryContent();

    MDataRequest addOwnerToRequest(
            MMemoryManagementUsingObject* owner, MDataRequest request);
};


} // namespace Met3D

#endif // LRUMEMORYMANAGER_H
