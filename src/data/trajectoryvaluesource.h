//
// Created by kerninator on 09.06.17.
//

#ifndef MTRAJECTORYVALUES_H
#define MTRAJECTORYVALUES_H

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
         * @param request
         * @return
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
}

#endif //MTRAJECTORYVALUES_H
