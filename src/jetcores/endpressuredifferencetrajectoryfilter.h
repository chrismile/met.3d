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
#ifndef ENDPRESSUREDIFFERENCETRAJECTORYFILTER_H
#define ENDPRESSUREDIFFERENCETRAJECTORYFILTER_H

// standard library imports

// related third party imports

// local application imports
#include "data/trajectoryfilter.h"
#include "isosurfaceintersectionsource.h"


namespace Met3D
{
/**
 * Computes the angle between the two first / last line segments and calculates
 * the pressure difference between the two first / last points. Removes
 * vertices where the angle of the adjacent segments is too sharp or the
 * pressure difference is too large.
 */
class MEndPressureDifferenceTrajectoryFilter : public MTrajectoryFilter
{
public:
    explicit MEndPressureDifferenceTrajectoryFilter();

    /** Input source for intersection lines. */
    void setIsosurfaceSource(MIsosurfaceIntersectionSource* s);

    /** Set the request that produced the trajectories in the pipeline. */
    void setLineRequest(const QString& request) { lineRequest = request; }

    /**
     * Overloads @ref MMemoryManagedDataSource::getData() to cast the returned
     * @ref MAbstractDataItem to @ref MTrajectoryEnsembleSelection that
     * contains the intersection lines filtered by geometric length.
    */
    MTrajectoryEnsembleSelection* getData(MDataRequest request)
    {
        return static_cast<MTrajectoryEnsembleSelection*>
                (MTrajectoryFilter::getData(request));
    }

    /**
     * Computes the pressure difference of the first and last two points of each
     * intersection lines and removes the endpoints if the pressure gain / drop
     * is too high. Returns a selection of lines for each ensemble member after
     * filtering.
     */
    MTrajectoryEnsembleSelection* produceData(MDataRequest request);

    MTask *createTaskGraph(MDataRequest request);

protected:
    const QStringList locallyRequiredKeys();

private:
    /** Pointer to input source of intersection lines. */
    MIsosurfaceIntersectionSource* isoSurfaceIntersectionSource;

    /** Line producing request. */
    QString lineRequest;
};


} // namespace Met3D

#endif
