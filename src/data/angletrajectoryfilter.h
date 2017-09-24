//
// Created by kerninator on 03.05.17.
//

#ifndef ANGLETRAJECTORYFILTER_H
#define ANGLETRAJECTORYFILTER_H

#include "trajectoryfilter.h"
#include "isosurfaceintersectionsource.h"

namespace Met3D
{
    /**
     * Computes the angles between two line segments and removes vertices where
     * the angle between the two adjacent line segments is too sharp.
     */
    class MAngleTrajectoryFilter : public MTrajectoryFilter
    {
    public:
        explicit MAngleTrajectoryFilter();

        /** Input source for intersection lines. */
        void setIsosurfaceSource(MIsosurfaceIntersectionSource* s);
        /** Set the request that produced the trajectories in the pipeline. */
        void setLineRequest(const QString& request) { lineRequest = request; }

        /**
         * Overloads @ref MMemoryManagedDataSource::getData() to cast
         * the returned @ref MAbstractDataItem to @ref MTrajectoryEnsembleSelection
         * that contains the intersection lines filtered by the line segment angle.
        */
        MTrajectoryEnsembleSelection* getData(MDataRequest request)
        {
            return static_cast<MTrajectoryEnsembleSelection*>
            (MTrajectoryFilter::getData(request));
        }

        /**
         * Computes the angle between the two adjacent line segment at a vertex and
         * returns a selection of lines for each ensemble member that does not
         * exceed a user-defined angle threshold.
         * @param request
         * @return
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

}

#endif //ANGLETRAJECTORYFILTER_H
