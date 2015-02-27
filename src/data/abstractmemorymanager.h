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
#ifndef ABSTRACTMEMORYMANAGER_H
#define ABSTRACTMEMORYMANAGER_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "abstractdataitem.h"
#include "datarequest.h"

namespace Met3D
{

/**
  @brief Base class for memory managers that handle @ref MAbstractDataItem
  instances.
  */
class MAbstractMemoryManager : public QObject
{
    Q_OBJECT

public:
    MAbstractMemoryManager();
    virtual ~MAbstractMemoryManager();

    virtual bool storeData(
            MMemoryManagementUsingObject* owner, MAbstractDataItem *item) = 0;

    virtual bool containsData(
            MMemoryManagementUsingObject* owner, MDataRequest request) = 0;

    virtual MAbstractDataItem* getData(
            MMemoryManagementUsingObject* owner, MDataRequest request) = 0;

    virtual void releaseData(
            MMemoryManagementUsingObject* owner, MDataRequest request) = 0;

    virtual void releaseData(MAbstractDataItem *item) = 0;

protected:
};


} // namespace Met3D

#endif // ABSTRACTMEMORYMANAGER_H
