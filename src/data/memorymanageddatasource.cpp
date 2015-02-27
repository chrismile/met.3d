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
#include "memorymanageddatasource.h"

// standard library imports
#include "assert.h"

// related third party imports
#include <QtConcurrentRun>

// local application imports
#include "util/mexception.h"

using namespace std;

#define ENABLE_REQUEST_PASSTHROUGH

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MMemoryManagedDataSource::MMemoryManagedDataSource()
    : MAbstractDataSource(),
      memoryManager(nullptr),
      passThroughSource(nullptr),
      registeredDataSourcesLock(QReadWriteLock::Recursive)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MMemoryManagedDataSource::setMemoryManager(MAbstractMemoryManager *m)
{
    if (m == nullptr)
        throw MValueError("no valid pointer to a memory manager passed",
                          __FILE__, __LINE__);

    if (memoryManager != nullptr)
        throw MValueError("memory manager cannot be changed after it has been "
                          "specified",
                          __FILE__, __LINE__);

    memoryManager = m;
}


void MMemoryManagedDataSource::requestData(MDataRequest request)
{
//TODO: This method might be broken. It is currently not used in Met.3D;
//      the MScheduledDataSource implementation is used instead. If you
//      require this implementation, please test it first!

    assert(memoryManager != nullptr);

    // For interaction with the memory manger, keep only the required keys in
    // the request to avoid contamination of the request. For example, if a
    // source only requires the key "INIT_TIME" and reveives a query with both
    // "INIT_TIME" and "VALID_TIME", several copies of a data item might be
    // generated for the same value of INIT_TIME but for different values of
    // VALID_TIME (although they all refer to the same data).
    MDataRequestHelper rh(request);
    rh.removeAllKeysExcept(requiredKeys());

    if ( memoryManager->containsData(this, rh.request()) )
    {
        // Signal that the data is available.
        emit dataRequestCompleted(request);
    }
    else
    {
        // Data need to be computed (compare to MScheduledDataSource, where the
        // mechanism is different!).
        MAbstractDataItem *item = produceData(rh.request());
        if (item)
        {
            item->setGeneratingRequest(rh.request());

            if (memoryManager->storeData(this, item))
                // Item was successfully stored, emit "completed" request.
                emit dataRequestCompleted(request);
            else
                // Item could not be stored in memory manager, free its memory.
                delete item;
        }
    }
}


MAbstractDataItem* MMemoryManagedDataSource::getData(MDataRequest request)
{
    assert(memoryManager != nullptr);

    MDataRequestHelper rh(request);

#ifdef ENABLE_REQUEST_PASSTHROUGH
    // Is this source handling this request? If none of its keywords is
    // contained in the request, simply pass the request to the next source
    // in the pipeline (if there is one).
    if (!rh.containsAll(locallyRequiredKeys()))
        if (passThroughSource)
            return passThroughSource->getData(request);
#endif

    // For interaction with the memory manger, keep only the required keys in
    // the request to avoid contamination of the request. For example, if a
    // source only requires the key "INIT_TIME" and reveives a query with both
    // "INIT_TIME" and "VALID_TIME", several copies of a data item might be
    // generated for the same value of INIT_TIME but for different values of
    // VALID_TIME (although they all refer to the same data).
    rh.removeAllKeysExcept(requiredKeys());
    return memoryManager->getData(this, rh.request());
}


void MMemoryManagedDataSource::releaseData(MDataRequest request)
{
    MDataRequestHelper rh(request);

#ifdef ENABLE_REQUEST_PASSTHROUGH
    // Is this source handling this request? If none of its keywords is
    // contained in the request, simply pass the request to the next source
    // in the pipeline (if there is one).
    if (!rh.containsAll(locallyRequiredKeys()))
        if (passThroughSource)
        {
            passThroughSource->releaseData(request);
            return;
        }
#endif

    assert(memoryManager != nullptr);
    rh.removeAllKeysExcept(requiredKeys());
    memoryManager->releaseData(this, rh.request());
}


void MMemoryManagedDataSource::releaseData(MAbstractDataItem *item)
{
    releaseData(item->getGeneratingRequest());
}


const QStringList& MMemoryManagedDataSource::requiredKeys()
{
    if (requiredRequestKeys.empty()) updateRequiredKeys();
    return requiredRequestKeys;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MMemoryManagedDataSource::registerInputSource(
        MAbstractDataSource *source, QString prefix)
{
    if (source == nullptr) return;

    QWriteLocker writeLocker(&registeredDataSourcesLock);

    if (prefix == "")
    {
        // No prefix. Add source, if not already contained.
        if (!registeredDataSources.contains(prefix, source))
            registeredDataSources.insert(prefix, source);
    }
    else
    {
        // Prefix specified. Only one data source is allowed per prefix.
        registeredDataSources.replace(prefix, source);
    }

    // Clear list of required keys so that it is updated on the next call
    // to requiredKeys() and the new data source is considered.
    requiredRequestKeys.clear();
}


void MMemoryManagedDataSource::deregisterPrefixedInputSources()
{
    QWriteLocker writeLocker(&registeredDataSourcesLock);

    QStringList keys = registeredDataSources.keys();

    // Remove all prefixed entries; keep those without prefix.
    for (int i = 0; i < keys.size(); i++)
        if (keys[i] != "") registeredDataSources.remove(keys[i]);

    requiredRequestKeys.clear();
}


MAbstractDataSource *MMemoryManagedDataSource::getPrefixedDataSource(
        QString prefix)
{
    QReadLocker readLocker(&registeredDataSourcesLock);
    return registeredDataSources.value(prefix);
}


void MMemoryManagedDataSource::enablePassThrough(MAbstractDataSource *s)
{
    if (s) passThroughSource = s;
}


void MMemoryManagedDataSource::reserveData(MDataRequest request, int numRequests)
{
    MDataRequestHelper rh(request);

#ifdef ENABLE_REQUEST_PASSTHROUGH
    // Is this source handling this request? If none of its keywords is
    // contained in the request, simply pass the request to the next source
    // in the pipeline (if there is one).
    if (!rh.containsAll(locallyRequiredKeys()))
        if (passThroughSource)
        {
            passThroughSource->reserveData(request, numRequests);
            return;
        }
#endif

    assert(memoryManager != nullptr);
    rh.removeAllKeysExcept(requiredKeys());

    // Calling containsData() blocks an item until release.
    for (int i = 0; i < numRequests; i++)
    {
        memoryManager->containsData(this, rh.request());
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MMemoryManagedDataSource::updateRequiredKeys()
{
    requiredRequestKeys.clear();
    requiredRequestKeys << locallyRequiredKeys();

    QReadLocker readLocker(&registeredDataSourcesLock);

    for (auto it = registeredDataSources.constBegin();
         it != registeredDataSources.constEnd(); ++it)
    {
        if (it.key() == "")
        {
            // No prefix. Add required keys of this source.
            requiredRequestKeys << it.value()->requiredKeys();
        }
        else
        {
            // Add data source key with prefix.
            QString prefix = it.key();
            const QStringList& keys = it.value()->requiredKeys();

            for (int i = 0; i < keys.size(); i++)
                requiredRequestKeys << prefix + keys[i];
        }
    }
}


} // namespace Met3D
