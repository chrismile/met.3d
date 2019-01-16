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
#ifndef MTRAJECTORYVALUES_H
#define MTRAJECTORYVALUES_H

// standard library imports

// related third party imports

// local application imports
#include "scheduleddatasource.h"
#include "weatherpredictiondatasource.h"
#include "datarequest.h"
#include "trajectoryarrowheadssource.h"
#include "isosurfaceintersectionsource.h"
#include "trajectoryselectionsource.h"


namespace Met3D
{
/**
 * Obtains information at each trajectory vertex position, such as
 * the value at the vertices for a given variable, tube thickness, ...
 */
class MTrajectoryValueSource : public MScheduledDataSource
{
public:
    explicit MTrajectoryValueSource();
    ~MTrajectoryValueSource() {}

    /** Input sources for each variable. */
    void setIsosurfaceSource(MIsosurfaceIntersectionSource* s);
    void setInputSelectionSource(MTrajectorySelectionSource* s);
    void setInputSourceValueVar(MWeatherPredictionDataSource *inputSource);
    void setInputSourceThicknessVar(MWeatherPredictionDataSource *inputSource);

    /** Set the request that produced the trajectories in the pipeline. */
    void setLineRequest(const QString& request) { lineRequest = request; }

    /**
     * Overloads @ref MMemoryManagedDataSource::getData() to cast the result
     * to the type @ref MTrajectoryValues.
    */
    MTrajectoryValues* getData(MDataRequest request)
    {
        return static_cast<MTrajectoryValues*>
                (MScheduledDataSource::getData(request));
    }

    /**
     * Gathers all value information at each core line vertex and returns
     * an array of floats as @ref MTrajectoryValues based on the corresponding
     * request.
     */
    MTrajectoryValues* produceData(MDataRequest request);

    MTask *createTaskGraph(MDataRequest request);

protected:
    const QStringList locallyRequiredKeys();

private:
    /** Pointer to input source of intersection lines. */
    MIsosurfaceIntersectionSource* isoSurfaceIntersectionSource;

    /** Pointer to the currently selected trajectories. */
    MTrajectorySelectionSource* inputSelectionSource;

    /** Pointer to the source to sample for values at each vertex. */
    MWeatherPredictionDataSource* valueSource;

    /** Pointer to the source to sample for tube thickness computation. */
    MWeatherPredictionDataSource* thicknessSource;

    /** Line producing request. */
    QString lineRequest;
    /** Request for each variable. */
    QVector<QString> varRequests;
};


} // namespace Met3D

#endif //MTRAJECTORYVALUES_H
