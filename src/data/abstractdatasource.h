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
#ifndef ABSTRACTDATASOURCE_H
#define ABSTRACTDATASOURCE_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "abstractdataitem.h"


namespace Met3D
{

/**
  @brief MAbstractDataSource is the base class for all modules in the
  visualization pipeline that produce data, including data readers, memory
  managers and modules that process data from another source to create new
  data.

  It inherits from QObject to be able to use Qt's signal/slot mechanism.
  */
class MAbstractDataSource : public QObject
{
    Q_OBJECT

public:
    MAbstractDataSource();
    virtual ~MAbstractDataSource();

    /**
     Asynchronous data request. The method triggers the preparation of the
     requested data item and returns immediately. The signal @ref
     dataRequestCompleted() is emitted when the data is ready to be fetched
     with @ref getData().
     */
    virtual void requestData(MDataRequest request) = 0;

    /**
     Returns a data item that has previously been requested with @ref
     requestData(). If requestData() has not been called on @p request of if
     the request has not yet completed, a null pointer is returned. The method
     increases the reference counter for the request.

     @note The returned pointer must not be destroyed by the caller, instead,
     @ref releaseData() needs to be called.
     */
    virtual MAbstractDataItem* getData(MDataRequest request) = 0;

    /**
     Decreases the reference counter of @p item. Needs to be called when @p
     item is no longer used by the caller.
     */
    virtual void releaseData(MAbstractDataItem *item) = 0;

    virtual void releaseData(MDataRequest request) = 0;

    /**
      Returns a list of keys that are required in any @ref MDataRequest passed
      to this data source's methods.
     */
    virtual const QStringList& requiredKeys() = 0;

signals:
    /**
     Emitted when a data request issued with @ref requestData() has completed.
     The listener can call @ref getData() after having received this signal.
     */
    void dataRequestCompleted(MDataRequest request);

protected:
    friend class MMemoryManagedDataSource;
    friend class MScheduledDataSource;
    virtual void reserveData(MDataRequest request, int numRequests) = 0;

};


} // namespace Met3D

#endif // ABSTRACTDATASOURCE_H
