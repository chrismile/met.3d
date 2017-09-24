//
// Created by kerninator on 04.05.17.
//

#ifndef ENDPRESSUREDIFFERENCETRAJECTORYFILTER_H
#define ENDPRESSUREDIFFERENCETRAJECTORYFILTER_H

#include "trajectoryfilter.h"
#include "isosurfaceintersectionsource.h"

namespace Met3D
{
    /**
     * Computes the angle between the two first / last line segments and calculates
     * the pressure difference between the two first / last points. Removes vertices
     * where the angle of the adjacent segments is too sharp or the pressure difference
     * is too large.
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
         * Overloads @ref MMemoryManagedDataSource::getData() to cast
         * the returned @ref MAbstractDataItem to @ref MTrajectoryEnsembleSelection
         * that contains the intersection lines filtered by geometric length.
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

#endif
