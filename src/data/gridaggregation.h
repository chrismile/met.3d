/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2018 Marc Rautenhaus
**  Copyright 2018 Bianca Tost
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
#ifndef GRIDAGGREGATION_H
#define GRIDAGGREGATION_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "data/abstractdataitem.h"
#include "data/structuredgrid.h"
#include "data/scheduleddatasource.h"
#include "data/weatherpredictiondatasource.h"


namespace Met3D
{

/**
  @brief MGridAggregation provides a set of multiple instances of @ref
  MStructuredGrid. Upon adding a grid with @ref addGrid(), grid reference is
  increased in the grid's memory manager; reference is released on destruction
  of the aggregation.
 */
class MGridAggregation : public MAbstractDataItem
{
public:
    MGridAggregation();

    ~MGridAggregation();

    /** Memory required for the data field in kilobytes. */
    unsigned int getMemorySize_kb();

    const QList<MStructuredGrid*>& getGrids() { return grids; }

protected:
    friend class MGridAggregationDataSource;

    /**
      Adds a grid to the aggregation and increases the grid's reference count
      in the corresponding memory manager. The reference count is decreased
      upon deletion of the aggregation object.
     */
    void addGrid(MStructuredGrid *grid);

private:
    QList<MStructuredGrid*> grids;
};


/**
  @brief MGridAggregationDataSource creates an aggregation of multiple @ref
  MStructuredGrid instances. To be used with actors that require multiple grids
  at once, e.g., to display spaghetti plots of multiple ensemble members. @ref
  produceData() returns an MGridAggregation instance containing pointers to the
  requested ensemble members.

  @note Inefficient architecture, may be revised in the future. All grids are
  required at the same time in memory.
 */
class MGridAggregationDataSource : public MScheduledDataSource
{
public:
    MGridAggregationDataSource();

    MGridAggregation* getData(MDataRequest request)
    { return static_cast<MGridAggregation*>
                (MScheduledDataSource::getData(request)); }

    MGridAggregation* produceData(MDataRequest request) override;

    MTask* createTaskGraph(MDataRequest request) override;

    void setInputSource(MWeatherPredictionDataSource* s);

protected:
    const QStringList locallyRequiredKeys() override;

    MWeatherPredictionDataSource *inputSource;
};


} // namespace Met3D

#endif // GRIDAGGREGATION_H
