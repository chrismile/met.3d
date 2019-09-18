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
#ifndef HESSIANTRAJECTORYFILTER_H
#define HESSIANTRAJECTORYFILTER_H

// standard library imports

// related third party imports

// local application imports
#include "data/trajectoryfilter.h"
#include "isosurfaceintersectionsource.h"
#include "multivarpartialderivativefilter.h"


namespace Met3D
{
/**
 * Computes the Hessian matrix and its eigenvalues at each trajectory vertex
 * and filters out all lines that are maximal, or rather, that have negative
 * eigenvalues.
 */
class MHessianTrajectoryFilter : public MTrajectoryFilter
{
public:
    explicit MHessianTrajectoryFilter();

    /** Input source for intersection lines. */
    void setIsosurfaceSource(MIsosurfaceIntersectionSource* s);

    /** Input source for partial derivative computation. */
    void setMultiVarParialDerivSource(
            MMultiVarPartialDerivativeFilter *multiVarFilter);

    /** Set the request that produced the trajectories in the pipeline. */
    void setLineRequest(const QString& request) { lineRequest = request; }

    /**
     * Overloads @ref MMemoryManagedDataSource::getData() to cast
     * the returned @ref MAbstractDataItem to @ref MTrajectoryEnsembleSelection
     * that contains the intersection lines filtered by the eigenvalues of the
     * Hessian matrix.
    */
    MTrajectoryEnsembleSelection* getData(MDataRequest request)
    {
        return static_cast<MTrajectoryEnsembleSelection*>
                (MTrajectoryFilter::getData(request));
    }

    /**
     * Computes the eigenvalues of the Hessian matrix at each line vertex
     * and returns the selection of lines with eigenvalues < threshold.
    */
    MTrajectoryEnsembleSelection* produceData(MDataRequest request);

    MTask *createTaskGraph(MDataRequest request);

protected:
    const QStringList locallyRequiredKeys();

private:
    /** Computes the eigenvalues of the 2x2 Hessian matrix. */
    inline bool computeEigenvalues(const float dnn, const float dzz,
                                   const float dndz,
                                   float& lambda1, float& lambda2);

    /** Pointer to input source of intersection lines. */
    MIsosurfaceIntersectionSource* isoSurfaceIntersectionSource;

    /** Pointer to the partial derivative source for two variables. */
    MMultiVarPartialDerivativeFilter* multiVarInputSource;

    /** Line producing request. */
    QString lineRequest;

    /** Request for each variable. */
    QVector<QString> varRequests;

};


} // namespace Met3D

#endif //HESSIANTRAJECTORYFILTER_H
