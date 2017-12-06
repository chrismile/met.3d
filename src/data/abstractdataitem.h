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
#ifndef ABSTRACTDATAITEM_H
#define ABSTRACTDATAITEM_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "datarequest.h"


namespace Met3D
{

class MAbstractMemoryManager;

/**
  @brief Abstract base class (more an interface) for all classes that
  use a memory manager.
 */
class MMemoryManagementUsingObject
{
public:
    MMemoryManagementUsingObject();

    virtual ~MMemoryManagementUsingObject() { }

    QString getID() { return myID; }

private:
    static unsigned int idCounter;
    QString myID;

};


/**
  @brief MAbstractDataItem is the abstract base class for all classes that
  represent a (memory managed) data item in Met.3D.
  */
class MAbstractDataItem : public MMemoryManagementUsingObject
{
public:
    MAbstractDataItem();

    ~MAbstractDataItem();

    virtual unsigned int getMemorySize_kb() = 0;

    MDataRequest getGeneratingRequest() { return generatingRequest; }

    void setGeneratingRequest(MDataRequest r) { generatingRequest = r; }

    /**
      If this item is memory managed, increases its reference counter. Only
      use this method if you know what you are doing (e.g. for direct pointer
      copies). Returns @p true if the counter was increased.
     */
    bool increaseReferenceCounter();

    MAbstractMemoryManager* getMemoryManager() { return memoryManager; }

protected:
    friend class MLRUMemoryManager;

    void setMemoryManager(MAbstractMemoryManager *m);

    void setStoringObject(MMemoryManagementUsingObject* object);

    MMemoryManagementUsingObject* getStoringObject();

    // If not nullptr, the memory manager that controls this item. May be used
    // by the item to release dependent items.
    MAbstractMemoryManager *memoryManager;

private:
    // The request that generated this data item.
    MDataRequest generatingRequest;
    // The object that stored this item in the memory manager.
    MMemoryManagementUsingObject* storingObject;

};


/**
  @brief MWeatherPredictionMetaData defines metadata-fields for data items that
  represent numerical weather prediction data.
  */
class MWeatherPredictionMetaData
{
public:
    MWeatherPredictionMetaData();

    void setMetaData(QDateTime initTime, QDateTime validTime,
                     QString variableName, int ensembleMember);

    const QDateTime& getInitTime() { return initTime; }

    const QDateTime& getValidTime() { return validTime; }

    const QString& getVariableName() { return variableName; }

    int getEnsembleMember() { return ensembleMember; }

protected:
    QDateTime initTime;
    QDateTime validTime;
    QString variableName;
    int ensembleMember;
};



} // namespace Met3D

#endif // ABSTRACTDATAITEM_H
