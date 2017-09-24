
#include "variabletrajectoryfilter.h"

using namespace Met3D;

MVariableTrajectoryFilter::MVariableTrajectoryFilter() :
        MTrajectoryFilter(),
        isoSurfaceIntersectionSource(nullptr),
        filterVariableInputSource(nullptr)
{

}


void MVariableTrajectoryFilter::setIsosurfaceSource(MIsosurfaceIntersectionSource* s)
{
    isoSurfaceIntersectionSource = s;
    registerInputSource(isoSurfaceIntersectionSource);
    enablePassThrough(isoSurfaceIntersectionSource);
}


void MVariableTrajectoryFilter::setFilterVariableInputSource(MWeatherPredictionDataSource* s)
{
    filterVariableInputSource = s;
    registerInputSource(filterVariableInputSource);
    enablePassThrough(filterVariableInputSource);
}


MTrajectoryEnsembleSelection* MVariableTrajectoryFilter::produceData(MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(inputSelectionSource         != nullptr);
    assert(filterVariableInputSource != nullptr);

    MDataRequestHelper rh(request);

    //int filterOp = rh.value("VARFILTER_OP").toInt();
    float filterValue = rh.value("VARFILTER_VALUE").toFloat();
    const QStringList members = rh.value("VARFILTER_MEMBERS").split("/");

    MIsosurfaceIntersectionLines* lineSource =
            isoSurfaceIntersectionSource->getData(lineRequest);

    rh.removeAll(locallyRequiredKeys());
    MTrajectoryEnsembleSelection* lineSelection =
            static_cast<MTrajectoryEnsembleSelection*>(
                    inputSelectionSource->getData(rh.request()));

    // Counts the number of new trajectories
    int newNumTrajectories = 0;

    QVector<GLint>  newStartIndices;
    QVector<GLsizei> newIndexCounts;

    QVector<GLint> newEnsStartIndices;
    QVector<GLsizei> newEnsIndexCounts;

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

        const int ensNewStartIndex = newStartIndices.size();
        int ensNewIndexCount = 0;

        MStructuredGrid *varGrid = filterVariableInputSource->getData(varRequest);

        for (int i = ensStartIndex; i < ensEndIndex; ++i)
        {
            int startIndex = lineSelection->getStartIndices()[i];
            const int indexCount = lineSelection->getIndexCount()[i];
            const int endIndex = startIndex + indexCount;

            int newIndexCount = 0;

            for (int j = startIndex; j < endIndex; ++j)
            {
                // Get the 3D position of the current vector.
                const QVector3D& p = lineSource->getVertices()[j];
                // Get the interpolated value at position p.
                const float value = varGrid->interpolateValue(p);
                // Test if the value fulfills the user-defined criterion.
                if (value >= filterValue)
                {
                    newIndexCount++;
                }
                else
                {
                    // If not, close the current trajectory and create a new one.
                    if (newIndexCount > 0)
                    {
                        newStartIndices.push_back(startIndex);
                        newIndexCounts.push_back(newIndexCount);
                        ++newNumTrajectories;
                    }

                    startIndex = j + 1;
                    newIndexCount = 0;
                }
            }

            // If the remaining vertices fulfill the filter criterion,
            // push them to the next index.
            if (newIndexCount > 1)
            {
                newStartIndices.push_back(startIndex);
                newIndexCounts.push_back(newIndexCount);
                ++newNumTrajectories;
            }
        }

        ensNewIndexCount = newNumTrajectories - ensNewStartIndex;

        newEnsStartIndices.push_back(ensNewStartIndex);
        newEnsIndexCounts.push_back(ensNewIndexCount);

        filterVariableInputSource->releaseData(varGrid);
    }

    MWritableTrajectoryEnsembleSelection *filterResult =
            new MWritableTrajectoryEnsembleSelection(lineSelection->refersTo(),
                                                     newNumTrajectories,
                                                     lineSelection->getTimes(),
                                                     lineSelection->getStartGridStride(),
                                                     members.size());

    for (int k = 0; k < newStartIndices.size(); ++k)
    {
        filterResult->setStartIndex(k, newStartIndices[k]);
        filterResult->setIndexCount(k, newIndexCounts[k]);
    }

    for (int e = 0; e < members.size(); ++e)
    {
        filterResult->setEnsembleStartIndex(e, newEnsStartIndices[e]);
        filterResult->setEnsembleIndexCount(e, newEnsIndexCounts[e]);
    }

    isoSurfaceIntersectionSource->releaseData(lineSource);
    inputSelectionSource->releaseData(lineSelection);
    return filterResult;
}


MTask *MVariableTrajectoryFilter::createTaskGraph(MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(inputSelectionSource         != nullptr);
    assert(filterVariableInputSource    != nullptr);
    assert(lineRequest                  != "");

    MTask *task = new MTask(request, this);
    MDataRequestHelper rh(request);

    const QStringList members = rh.value("VARFILTER_MEMBERS").split("/");
    const QString sourceVar = rh.value("VARFILTER_VARIABLE");

    foreach (const auto& member, members)
    {
        MDataRequestHelper rhVar(request);
        rhVar.removeAll(locallyRequiredKeys());

        rhVar.insert("MEMBER", member);
        rhVar.insert("VARIABLE", sourceVar);

        varRequests.push_back(rhVar.request());
        task->addParent(filterVariableInputSource
                                ->getTaskGraph(rhVar.request()));
    }

    rh.removeAll(locallyRequiredKeys());
    // Get previous line selection.
    task->addParent(inputSelectionSource->getTaskGraph(rh.request()));

    // Get original trajectory lines.
    task->addParent(isoSurfaceIntersectionSource
                            ->getTaskGraph(lineRequest));

    return task;
}


const QStringList MVariableTrajectoryFilter::locallyRequiredKeys()
{
    return (QStringList()
            << "VARFILTER_MEMBERS" << "VARFILTER_VARIABLE"
            << "VARFILTER_OP" << "VARFILTER_VALUE"
    );
}
