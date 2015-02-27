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
#include "abstractdataitem.h"

// standard library imports

// related third party imports

// local application imports
#include "data/abstractmemorymanager.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

unsigned int MMemoryManagementUsingObject::idCounter = 0;

MMemoryManagementUsingObject::MMemoryManagementUsingObject()
    : myID(QString("%1").arg(idCounter++))
{
}


MAbstractDataItem::MAbstractDataItem()
    : MMemoryManagementUsingObject(),
      memoryManager(nullptr),
      generatingRequest(""),
      storingObject(nullptr)
{
}


MAbstractDataItem::~MAbstractDataItem()
{
}


bool MAbstractDataItem::increaseReferenceCounter()
{
    if (memoryManager != nullptr)
    {
        // If successfull, containsData() increases the item's reference
        // counter.
        return memoryManager->containsData(storingObject, generatingRequest);
    }

    return false;
}


MWeatherPredictionMetaData::MWeatherPredictionMetaData()
{
    ensembleMember = -1;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MWeatherPredictionMetaData::setMetaData(
        QDateTime initTime, QDateTime validTime,
        QString variableName, int ensembleMember)
{
    this->initTime       = initTime;
    this->validTime      = validTime;
    this->variableName   = variableName;
    this->ensembleMember = ensembleMember;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MAbstractDataItem::setMemoryManager(MAbstractMemoryManager *m)
{
    memoryManager = m;
}


void MAbstractDataItem::setStoringObject(MMemoryManagementUsingObject *object)
{
    storingObject = object;
}


MMemoryManagementUsingObject *MAbstractDataItem::getStoringObject()
{
    return storingObject;
}


} // namespace Met3D
