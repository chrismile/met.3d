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
#include "lrumemorymanager.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mexception.h"
#include "util/mutil.h"
#include "gxfw/msystemcontrol.h"

using namespace std;


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MLRUMemoryManager::MLRUMemoryManager(QString identifier,
                                     unsigned int allowedMemoryUsage_kb)
    : MAbstractMemoryManager(),
      identifier(identifier),
      systemMemoryLimit_kb(allowedMemoryUsage_kb),
      systemMemoryUsage_kb(0),
      // Initialize "memoryCacheMutex" in "recursive mode" so that it can
      // be locked multiple times from the same thread. This is necessary as
      // some of the methods in this class that lock the mutex call other
      // methods that also want to lock.
      memoryCacheMutex(QMutex::Recursive)
{
    MSystemManagerAndControl *sc = MSystemManagerAndControl::getInstance();

    QtProperty *propertyGroup = sc->getGroupPropertyManager()
            ->addProperty(QString("Memory manager (%1)").arg(identifier));

    updateProperty = sc->getClickPropertyManager()
            ->addProperty("update");
    propertyGroup->addSubProperty(updateProperty);

    memoryStatusProperty = sc->getStringPropertyManager()
            ->addProperty("system memory usage");
    propertyGroup->addSubProperty(memoryStatusProperty);

    itemStatusProperty = sc->getStringPropertyManager()
            ->addProperty("cached items");
    propertyGroup->addSubProperty(itemStatusProperty);

    dumpMemoryContentProperty = sc->getClickPropertyManager()
            ->addProperty("dump memory content");
    propertyGroup->addSubProperty(dumpMemoryContentProperty);

    clearCacheProperty = sc->getClickPropertyManager()
            ->addProperty("clear cache");
    propertyGroup->addSubProperty(clearCacheProperty);

    sc->addProperty(propertyGroup);

    connect(sc->getClickPropertyManager(),
            SIGNAL(propertyChanged(QtProperty*)),
            SLOT(propertyEvent(QtProperty*)));
}


MLRUMemoryManager::~MLRUMemoryManager()
{
    // If "this->" is missing the compiler for some reason cannot identify
    // memoryCacheMutex as a member variable...
    QMutexLocker memoryCacheLocker( &(this->memoryCacheMutex) );

    // Delete all released data items that are still in the cache.
    while (!releasedDataItemsQueue.empty())
        delete releasedDataItems.take(releasedDataItemsQueue.takeFirst());

    // When this memory manager is being destroyed all items in the cache
    // should be released. What do we do when there are still active items? Not
    // deleting them is a potential memory leak, deleting them can trigger a
    // segmentation fault... The items are deleted in this implementation, so
    // take care to release all items before the memory manger gets destroyed.

    if (!activeDataItems.empty())
    {
        QHashIterator<MDataRequest, MAbstractDataItem*> i(activeDataItems);
        while (i.hasNext())
            delete i.next().value();
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

bool MLRUMemoryManager::storeData(
        MMemoryManagementUsingObject* owner, MAbstractDataItem *item)
{
    MDataRequest request = item->getGeneratingRequest();
#ifdef DEBUG_OUTPUT_MEMORYMANAGER
    LOG4CPLUS_DEBUG(mlog, "storeData() for request " << request.toStdString()
                    << flush);
#endif
    // Use QMutexLocker to simplify unlocking for the cases in which this
    // method returns before "full" completion.
    QMutexLocker memoryCacheLocker( &(this->memoryCacheMutex) );

    // Items that are already stored in the cache cannot be stored again.
    if (containsData(owner, request))
    {
        LOG4CPLUS_WARN(mlog, "WARNING: storeData() for request "
                       << request.toStdString()
                       << " declined, request key already exists.");
        return false;
    }

    // Test if the system memory limit will be exceeded by adding the new data
    // item. If so, remove some of the released data items.
    unsigned int itemMemoryUsage_kb = item->getMemorySize_kb();
    while ((systemMemoryUsage_kb + itemMemoryUsage_kb >= systemMemoryLimit_kb)
           && !releasedDataItemsQueue.empty())
    {
        MDataRequest removeKey = releasedDataItemsQueue.takeFirst();
        referenceCounter.take(removeKey);
        MAbstractDataItem* removeItem = releasedDataItems.take(removeKey);
        systemMemoryUsage_kb -= removeItem->getMemorySize_kb();
        delete removeItem;
    }

    // If not enough memory could be freed throw an exception.
    if ( systemMemoryUsage_kb + itemMemoryUsage_kb >= systemMemoryLimit_kb )
        throw MMemoryError("system memory limit exceeded, cannot release"
                           " any further data fields", __FILE__, __LINE__);

    // Memory is fine, so insert the new grid into pool of data items.
    request = addOwnerToRequest(owner, request);
    activeDataItems.insert(request, item);
    item->setMemoryManager(this);
    item->setStoringObject(owner);
    // Place an initial reference on this item (it won't be deleted until the
    // corresponding call to release()).
    referenceCounter.insert(request, 1);
    systemMemoryUsage_kb += itemMemoryUsage_kb;
    //updateStatusDisplay();
    return true;
}


bool MLRUMemoryManager::containsData(
        MMemoryManagementUsingObject* owner, MDataRequest request)
{
    request = addOwnerToRequest(owner, request);

    QMutexLocker memoryCacheLocker( &(this->memoryCacheMutex) );

    if (activeDataItems.contains(request))
    {
        // The data item is available and currently active. Increase the
        // reference counter and return true.
        referenceCounter[request] += 1;
#ifdef DEBUG_OUTPUT_MEMORYMANAGER
    LOG4CPLUS_DEBUG(mlog, "containsData() for request "
                    << request.toStdString()
                    << "; reference counter set to "
                    << referenceCounter[request]);
#endif
        return true;
    }

    if (releasedDataItems.contains(request))
    {
        // The data item is still in memory, but released. Make it active and
        // set the reference counter to 1 (this is the first active request
        // after the last release).
        releasedDataItemsQueue.removeOne(request);
        MAbstractDataItem *item = releasedDataItems.take(request);
        activeDataItems.insert(request, item);
        referenceCounter[request] = 1;
        //updateStatusDisplay(); // # of active/released items has changed
#ifdef DEBUG_OUTPUT_MEMORYMANAGER
    LOG4CPLUS_DEBUG(mlog, "containsData() for request "
                    << request.toStdString()
                    << "; reference counter set to "
                    << referenceCounter[request]);
#endif
        return true;
    }

#ifdef DEBUG_OUTPUT_MEMORYMANAGER
    LOG4CPLUS_DEBUG(mlog, "containsData() for request "
                    << request.toStdString()
                    << "; request is not contained.");
#endif
    return false;
}


MAbstractDataItem* MLRUMemoryManager::getData(
        MMemoryManagementUsingObject* owner, MDataRequest request)
{
    request = addOwnerToRequest(owner, request);

#ifdef DEBUG_OUTPUT_MEMORYMANAGER
    LOG4CPLUS_DEBUG(mlog, "getData() for request " << request.toStdString()
                    << flush);
#endif
    QMutexLocker memoryCacheLocker( &(this->memoryCacheMutex) );

    if (activeDataItems.contains(request))
    {
        return activeDataItems[request];
    }

    if (releasedDataItems.contains(request))
    {
        QString msg = QString("ERROR: getData() called on non-active data "
                              "item %1 -- the item is still cached, but not "
                              "active -- call containsData() before you call "
                              "getData()").arg(request);
        LOG4CPLUS_ERROR(mlog, msg.toStdString());
        throw MBadDataFieldRequest(msg.toStdString(), __FILE__, __LINE__);
    }

    // The data item is not stored in cache memory. Return a null pointer.
    return nullptr;
}


void MLRUMemoryManager::releaseData(
        MMemoryManagementUsingObject *owner, MDataRequest request)
{
    request = addOwnerToRequest(owner, request);

    QMutexLocker memoryCacheLocker( &(this->memoryCacheMutex) );

    if (activeDataItems.contains(request))
    {
        // Decrement reference counter. If it is zero afterwards, we can safely
        // release the system memory data field -- no consumer requires it at
        // the moment.
        referenceCounter[request] -= 1;
#ifdef DEBUG_OUTPUT_MEMORYMANAGER
        LOG4CPLUS_DEBUG(mlog, "releaseData() for request "
                        << request.toStdString()
                        << "; reference counter set to "
                        << referenceCounter[request]);
#endif
        if (referenceCounter[request] == 0)
        {
            // Move the data grid in system memory to the list of released
            // objects. It might be deleted by the storeData() method if memory
            // is required.
            releasedDataItems.insert(request, activeDataItems.take(request));
            releasedDataItemsQueue.push_back(request);
            //updateStatusDisplay(); // # of active/released items has changed
        }
    }
    else
    {
#ifdef DEBUG_OUTPUT_MEMORYMANAGER
    LOG4CPLUS_DEBUG(mlog, "releaseData() for request "
                    << request.toStdString() << flush);
#endif
        QString msg = QString("You shouldn't release a data item that is not "
                              " currently active: %1").arg(request);
        LOG4CPLUS_ERROR(mlog, "ERROR: " << msg.toStdString());
        throw MMemoryError(msg.toStdString(), __FILE__, __LINE__);
    }
}


void MLRUMemoryManager::releaseData(MAbstractDataItem *item)
{
    releaseData(item->getStoringObject(), item->getGeneratingRequest());
}


void MLRUMemoryManager::clearCache()
{
    QMutexLocker memoryCacheLocker( &(this->memoryCacheMutex) );

    while ( !releasedDataItemsQueue.empty() )
    {
        MDataRequest removeKey = releasedDataItemsQueue.takeFirst();
        referenceCounter.take(removeKey);
        MAbstractDataItem* removeItem = releasedDataItems.take(removeKey);
        systemMemoryUsage_kb -= removeItem->getMemorySize_kb();
        delete removeItem;
    }
    updateStatusDisplay();
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MLRUMemoryManager::propertyEvent(QtProperty *property)
{
    if (property == updateProperty) updateStatusDisplay();
    else if (property == dumpMemoryContentProperty) dumpMemoryContent();
    else if (property == clearCacheProperty) clearCache();
}


/******************************************************************************
***                            PRIVATE METHODS                              ***
*******************************************************************************/

void MLRUMemoryManager::updateStatusDisplay()
{
    MSystemManagerAndControl *sc = MSystemManagerAndControl::getInstance();

    sc->getStringPropertyManager()->setValue(
                memoryStatusProperty, QString("%1 / %2 MiB")
                .arg(systemMemoryUsage_kb/1024).arg(systemMemoryLimit_kb/1024));

    sc->getStringPropertyManager()->setValue(
                itemStatusProperty, QString("%1 active / %2 released")
                .arg(activeDataItems.size()).arg(releasedDataItems.size()));
}


void MLRUMemoryManager::dumpMemoryContent()
{
    QMutexLocker memoryCacheLocker( &(this->memoryCacheMutex) );

    QString s = QString("\n\nSYSTEM MEMORY CACHE CONTENT (%1)\n"
                        "===========================\n"
                        "Active items:\n").arg(identifier);

    QHashIterator<Met3D::MDataRequest,
            MAbstractDataItem*> iter(activeDataItems);
    while (iter.hasNext()) {
        iter.next();
        s += QString("REQUEST: %1, SIZE: %2 kb, REFERENCES: %3\n")
                .arg(iter.key())
                .arg(iter.value()->getMemorySize_kb())
                .arg(referenceCounter[iter.key()]);
    }

    s += "\nReleased items (in queued order):\n";

    for (int i = 0; i < releasedDataItemsQueue.size(); i++)
    {
        Met3D::MDataRequest r = releasedDataItemsQueue[i];
        s += QString("REQUEST: %1, SIZE: %2 kb, REFERENCES: %3\n")
                .arg(r)
                .arg(releasedDataItems[r]->getMemorySize_kb())
                .arg(referenceCounter[r]);
    }

    s += "\n\n===========================\n";

    LOG4CPLUS_INFO(mlog, s.toStdString());
    updateStatusDisplay();
}


MDataRequest MLRUMemoryManager::addOwnerToRequest(
        MMemoryManagementUsingObject* owner, MDataRequest request)
{
    return owner->getID() + "/" + request;
}

} // namespace Met3D
