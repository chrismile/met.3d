//
// Created by kerninator on 17.05.17.
//

#ifndef TRAJECTORYARROWHEADSSOURCE_H
#define TRAJECTORYARROWHEADSSOURCE_H

#include <array>
#include "scheduleddatasource.h"
#include "weatherpredictiondatasource.h"
#include "datarequest.h"
#include "trajectoryarrowheadssource.h"
#include "isosurfaceintersectionsource.h"
#include "trajectoryselectionsource.h"

namespace Met3D
{
    /**
     * Estimates the direction of flow along the trajectory line with the aid of
     * the current wind field and creates arrow heads at the end of each
     * trajectory line to indicate the flow direction.
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
         * @param request
         * @return
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
}

#endif //TRAJECTORYARROWHEADSSOURCE_H
