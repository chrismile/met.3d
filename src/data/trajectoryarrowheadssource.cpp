//
// Created by kerninator on 17.05.17.
//

#include "trajectoryarrowheadssource.h"

using namespace Met3D;

MTrajectoryArrowHeadsSource::MTrajectoryArrowHeadsSource()
        : MScheduledDataSource(), inputSources({ nullptr, nullptr, nullptr })
{

}


void MTrajectoryArrowHeadsSource::setIsosurfaceSource(
        MIsosurfaceIntersectionSource* s)
{
    isoSurfaceIntersectionSource = s;
    registerInputSource(isoSurfaceIntersectionSource);
    enablePassThrough(isoSurfaceIntersectionSource);
}


void MTrajectoryArrowHeadsSource::setInputSelectionSource(MTrajectorySelectionSource *s)
{
    inputSelectionSource = s;
    registerInputSource(inputSelectionSource);
}


void MTrajectoryArrowHeadsSource::setInputSourceUVar(
        MWeatherPredictionDataSource *inputSource)
{
    inputSources[0] = inputSource;
    registerInputSource(inputSources[0]);
    enablePassThrough(inputSources[0]);
}


void MTrajectoryArrowHeadsSource::setInputSourceVVar(
        MWeatherPredictionDataSource *inputSource)
{
    inputSources[1] = inputSource;
    registerInputSource(inputSources[1]);
    enablePassThrough(inputSources[1]);
}


void MTrajectoryArrowHeadsSource::setInputSourceVar(
        MWeatherPredictionDataSource *inputSource)
{
    inputSources[2] = inputSource;

    if (!inputSources[2]) { return; }
    registerInputSource(inputSources[2]);
    enablePassThrough(inputSources[2]);
}


MTrajectoryArrowHeads* MTrajectoryArrowHeadsSource::produceData(MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(inputSelectionSource         != nullptr);
    assert(inputSources[0]              != nullptr);
    assert(inputSources[1]              != nullptr);
    //assert(inputSources[2]              != nullptr);
    assert(lineRequest                  != "");

    MDataRequestHelper rh(request);

    const QStringList members = rh.value("ARROWHEADS_MEMBERS").split("/");

    MIsosurfaceIntersectionLines* lineSource = isoSurfaceIntersectionSource->getData(lineRequest);

    rh.removeAll(locallyRequiredKeys());
    MTrajectoryEnsembleSelection* lineSelection =
            static_cast<MTrajectoryEnsembleSelection*>(inputSelectionSource->getData(rh.request()));

    MStructuredGrid *gridU      = nullptr;
    MStructuredGrid *gridV      = nullptr;
    MStructuredGrid *gridSource = nullptr;

    const int numTrajectories = lineSelection->getNumTrajectories();

    MTrajectoryArrowHeads *result = new MTrajectoryArrowHeads(numTrajectories);

    QVector<GLint> ensStartIndices = lineSelection->getEnsembleStartIndices();
    QVector<GLsizei> ensIndexCounts = lineSelection->getEnsembleIndexCount();

    // Loop through each member and filter the lines corresponding to that member.
    for (uint ee = 0; ee < static_cast<uint>(members.size()); ++ee)
    {
        const QString member = members[ee];
        // Obtain the start and end line index for the current member.
        const int ensStartIndex = ensStartIndices[ee];
        const int ensIndexCount = ensIndexCounts[ee];
        const int ensEndIndex = ensStartIndex + ensIndexCount;

        QString varRequest = varRequests[0];
        varRequests.pop_front();
        gridU = inputSources[0]->getData(varRequest);

        varRequest = varRequests[0];
        varRequests.pop_front();
        gridV = inputSources[1]->getData(varRequest);

        if (inputSources[2])
        {
            varRequest = varRequests[0];
            varRequests.pop_front();
            gridSource = inputSources[2]->getData(varRequest);
        }

        for (int i = ensStartIndex; i < ensEndIndex; ++i)
        {
            int startIndex = lineSelection->getStartIndices()[i];
            const int indexCount = lineSelection->getIndexCount()[i];
            const int endIndex = startIndex + indexCount;

            // Get the points of the first trajectory segment.
            const QVector3D &p0 = lineSource->getVertices()[startIndex];
            const QVector3D &p1 = lineSource->getVertices()[startIndex + 1];

            // Get the points of the last trajectory segment.
            const QVector3D &pn0 = lineSource->getVertices()[endIndex - 2];
            const QVector3D &pn1 = lineSource->getVertices()[endIndex - 1];

            // Compute the tangents at each segment. Each tangent points to the
            // end point of the trajectory line.
            QVector2D tangent0(p1.x() - p0.x(), p1.y() - p0.y());
            tangent0.normalize();
            QVector2D tangentn(pn1.x() - pn0.x(), pn1.y() - pn0.y());
            tangentn.normalize();

            // Obtain the wind direction at the start point.
            QVector2D wind(gridU->interpolateValue(p0),
                           gridV->interpolateValue(p0));
            wind.normalize();
            // Compute the dot product between the first tangent and the wind
            // direction.
            const double dotProduct = QVector2D::dotProduct(tangent0, wind);

            // If both vectors point into the same direction, put the
            // arrow head to the end segment of the first segment and let the
            // head point to the inverse tangent direction.
            if (dotProduct <= 0)
            {
                float value = 0;
                if (gridSource) { value = gridSource->interpolateValue(p0); }

                result->setVertex(i, { p0, -tangent0, value });
            }
            // Take the position of the last segment and use the segment at
            // the end for the arrow head.
            else
            {
                float value = 0;
                if (gridSource) { value = gridSource->interpolateValue(pn1); }

                // Add the arrow to the array
                result->setVertex(i, { pn1, tangentn, value });
            }
        }
    }

    return result;
}


MTask *MTrajectoryArrowHeadsSource::createTaskGraph(MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(inputSelectionSource         != nullptr);
    assert(inputSources[0]              != nullptr);
    assert(inputSources[1]              != nullptr);
    //assert(inputSources[2]              != nullptr);
    assert(lineRequest                  != "");

    MTask *task = new MTask(request, this);
    MDataRequestHelper rh(request);

    const QStringList members = rh.value("ARROWHEADS_MEMBERS").split("/");
    const QStringList uvVars = rh.value("ARROWHEADS_UV_VARIABLES").split("/");
    const QString sourceVar = rh.value("ARROWHEADS_SOURCEVAR");

    foreach (const auto& member, members)
    {
        MDataRequestHelper rhVar;
        rhVar.insert("MEMBER", member);
        rhVar.insert("INIT_TIME", rh.value("INIT_TIME"));
        rhVar.insert("VALID_TIME", rh.value("VALID_TIME"));
        rhVar.insert("LEVELTYPE", rh.value("LEVELTYPE"));

        for (int i = 0; i < uvVars.size(); ++i)
        {
            rhVar.insert("VARIABLE", uvVars[i]);
            varRequests.push_back(rhVar.request());
            task->addParent(inputSources[i]->getTaskGraph(rhVar.request()));
        }

        if (inputSources[2])
        {
            rhVar.insert("VARIABLE", sourceVar);
            varRequests.push_back(rhVar.request());
            task->addParent(inputSources[2]->getTaskGraph(rhVar.request()));
        }
    }

    rh.removeAll(locallyRequiredKeys());

    // Get previous line selection
    task->addParent(inputSelectionSource->getTaskGraph(rh.request()));

    task->addParent(isoSurfaceIntersectionSource
                            ->getTaskGraph(lineRequest));

    return task;
}


const QStringList MTrajectoryArrowHeadsSource::locallyRequiredKeys()
{
    return (QStringList()
            << "ARROWHEADS_UV_VARIABLES" << "ARROWHEADS_MEMBERS"
            << "ARROWHEADS_SOURCEVAR"
    );
}