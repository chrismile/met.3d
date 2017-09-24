#ifndef MGEOMETRICLENGTHTRAJECTORYFILTER_H
#define MGEOMETRICLENGTHTRAJECTORYFILTER_H

#include "weatherpredictiondatasource.h"
#include "structuredgrid.h"
#include "datarequest.h"
#include "isosurfaceintersectionsource.h"
#include "trajectoryfilter.h"

namespace Met3D
{
    class MGeometricLengthTrajectoryFilter : public MTrajectoryFilter
    {
    public:
        MGeometricLengthTrajectoryFilter();

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
         * Gathers all value information at each core line vertex and returns
         * a selection of lines for each ensemble member based on the corresponding
         * request.
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

#endif //MGEOMETRICLENGTHTRAJECTORYFILTER_H
