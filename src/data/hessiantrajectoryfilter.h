//
// Created by kerninator on 03.05.17.
//

#ifndef HESSIANTRAJECTORYFILTER_H
#define HESSIANTRAJECTORYFILTER_H

//#include "hessiantrajectoryfilter.h"
#include "trajectoryfilter.h"
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
        void setMultiVarParialDerivSource(MMultiVarPartialDerivativeFilter *multiVarFilter);
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
         * @param request
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
}


#endif //HESSIANTRAJECTORYFILTER_H
