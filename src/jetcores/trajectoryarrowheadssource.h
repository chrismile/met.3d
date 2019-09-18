/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Michael Kern
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
#ifndef TRAJECTORYARROWHEADSSOURCE_H
#define TRAJECTORYARROWHEADSSOURCE_H

// standard library imports
#include <array>

// related third party imports

// local application imports
#include "data/scheduleddatasource.h"
#include "data/weatherpredictiondatasource.h"
#include "data/datarequest.h"
#include "trajectoryarrowheadssource.h"
#include "isosurfaceintersectionsource.h"
#include "data/trajectoryselectionsource.h"


namespace Met3D
{
/**
 * Estimates the direction of flow along the trajectory line with the aid of
 * the current wind field and creates arrow heads at the end of each trajectory
 * line to indicate the flow direction.
 */
class MTrajectoryArrowHeadsSource : public MScheduledDataSource
{
public:
    explicit MTrajectoryArrowHeadsSource();

    /** Input source for intersection lines. */
    void setIsosurfaceSource(MIsosurfaceIntersectionSource* s);

    /** Input source for current line selection. */
    void setInputSelectionSource(MTrajectorySelectionSource* s);

    /** Input sources for variables required to create the arrows. */
    void setInputSourceUVar(MWeatherPredictionDataSource *inputSource);
    void setInputSourceVVar(MWeatherPredictionDataSource *inputSource);
    void setInputSourceVar(MWeatherPredictionDataSource *inputSource);

    /** Set the request that produced the trajectories in the pipeline. */
    void setLineRequest(const QString& request) { lineRequest = request; }

    /**
     * Overloads @ref MMemoryManagedDataSource::getData() to cast
     * the returned @ref MAbstractDataItem to @ref MTrajectoryArrowHeads
     * that contains the information of arrow heads at the endpoints of the
     * intersection lines into the direction of flow.
     */
    MTrajectoryArrowHeads* getData(MDataRequest request)
    {
        return static_cast<MTrajectoryArrowHeads*>
                (MScheduledDataSource::getData(request));
    }

    /**
     * Gathers all information at each core line vertex and returns
     * an array of arrow heads with the arrow's location and orientation.
     */
    MTrajectoryArrowHeads* produceData(MDataRequest request);

    MTask *createTaskGraph(MDataRequest request);

protected:
    const QStringList locallyRequiredKeys();

private:
    /** Pointer to input source of intersection lines. */
    MIsosurfaceIntersectionSource* isoSurfaceIntersectionSource;

    /** Pointer to input source of the current selection. */
    MTrajectorySelectionSource* inputSelectionSource;

    /** Pointer to input sources of each required variable. */
    std::array<MWeatherPredictionDataSource*, 3> inputSources;

    /** Line producing request. */
    QString lineRequest;
    /** Request of each variable. */
    QVector<QString> varRequests;
};


} // namespace Met3D

#endif //TRAJECTORYARROWHEADSSOURCE_H
