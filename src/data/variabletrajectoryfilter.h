#ifndef MVARIABLETRAJECTORYFILTER_H
#define MVARIABLETRAJECTORYFILTER_H

// local application imports
#include "weatherpredictiondatasource.h"
#include "structuredgrid.h"
#include "datarequest.h"
#include "isosurfaceintersectionsource.h"
#include "trajectoryfilter.h"

namespace Met3D
{
    /**
     * Filters the trajectory by the scalar value from a certain scalar
     * field (corresponding to a selected variable) at each trajectory vertex.
     */
    class MVariableTrajectoryFilter : public MTrajectoryFilter
    {
    public:
        explicit MVariableTrajectoryFilter();

        /** Input source for intersection lines. */
        void setIsosurfaceSource(MIsosurfaceIntersectionSource* s);
        /** Input source for the variable used to filter the lines. */
        void setFilterVariableInputSource(MWeatherPredictionDataSource* s);
        /** Set the request that produced the trajectories in the pipeline. */
        void setLineRequest(const QString& request) { lineRequest = request; }

        /**
         * Overloads @ref MMemoryManagedDataSource::getData() to cast
         * the returned @ref MAbstractDataItem to @ref MTrajectoryEnsembleSelection
         * that contains the intersection lines filtered by variable value at
         * each vertex position.
        */
        MTrajectoryEnsembleSelection* getData(MDataRequest request)
        {
            return static_cast<MTrajectoryEnsembleSelection*>
            (MTrajectoryFilter::getData(request));
        }

        /**
         * Obtains the value of the chosen variable at each core line vertex and returns
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
        /** Pointer to input source of the variable used for filtering. */
        MWeatherPredictionDataSource* filterVariableInputSource;

        /** Line producing request. */
        QString lineRequest;
        /** Request for each variable. */
        QVector<QString> varRequests;
    };
}

#endif //MVARIABLETRAJECTORYFILTER_H
