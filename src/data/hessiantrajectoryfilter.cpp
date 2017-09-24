//
// Created by kerninator on 03.05.17.
//

#include "hessiantrajectoryfilter.h"

#include <fstream>
#include <iomanip>
#include <omp.h>

// Enables the output of eigenvalues per line vertex into a text file.
//#define DEBUG_HESSIAN

using namespace Met3D;

MHessianTrajectoryFilter::MHessianTrajectoryFilter()
        : MTrajectoryFilter(),
          isoSurfaceIntersectionSource(nullptr),
          multiVarInputSource(nullptr)
{

}


void MHessianTrajectoryFilter::
setIsosurfaceSource(MIsosurfaceIntersectionSource* s)
{
    isoSurfaceIntersectionSource = s;
    registerInputSource(isoSurfaceIntersectionSource);
    enablePassThrough(isoSurfaceIntersectionSource);
}


void MHessianTrajectoryFilter::
setMultiVarParialDerivSource(MMultiVarPartialDerivativeFilter *multiVarFilter)
{
    multiVarInputSource = multiVarFilter;
    registerInputSource(multiVarFilter);
    enablePassThrough(multiVarFilter);
}


MTrajectoryEnsembleSelection* MHessianTrajectoryFilter::produceData(MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(multiVarInputSource != nullptr);

#ifdef DEBUG_HESSIAN
    std::ofstream debugFile;
#endif

    MDataRequestHelper rh(request);

    QString initTime = rh.value("INIT_TIME");
    QString validTime = rh.value("VALID_TIME");

    double lambdaThreshold = rh.value("HESSIANFILTER_VALUE").toDouble();
    const QStringList members = rh.value("HESSIANFILTER_MEMBERS").split("/");

    MIsosurfaceIntersectionLines* lineSource = isoSurfaceIntersectionSource->getData(lineRequest);

    rh.removeAll(locallyRequiredKeys());
    MTrajectoryEnsembleSelection* lineSelection =
            static_cast<MTrajectoryEnsembleSelection*>(inputSelectionSource->getData(rh.request()));

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

        // Obtain grid containing the second derivative along n (d²/dn²)
        QString varRequest = varRequests[0];
        varRequests.pop_front();
        MStructuredGrid *gridDDN = multiVarInputSource->getData(varRequest);

        // Obtain grid containing the second derivative along z (d²/dz²)
        varRequest = varRequests[0];
        varRequests.pop_front();
        MStructuredGrid *gridDDZ = multiVarInputSource->getData(varRequest);

        // Obtain grid containing the mixed partial derivative along n and z (d²/dndz)
        varRequest = varRequests[0];
        varRequests.pop_front();
        MStructuredGrid *gridDNDZ = multiVarInputSource->getData(varRequest);


#ifdef DEBUG_HESSIAN
        debugFile = std::ofstream("data/hessian_"
                      + initTime.toStdString() + "-" + validTime.toStdString()
                      + "_member" + member.toStdString() + ".txt");
#endif

        const int ensNewStartIndex = newStartIndices.size();
        int ensNewIndexCount = 0;

        for (int i = ensStartIndex; i < ensEndIndex; ++i)
        {
            int startIndex = lineSelection->getStartIndices()[i];
            const int indexCount = lineSelection->getIndexCount()[i];
            const int endIndex = startIndex + indexCount;

            int newIndexCount = 0;

            QVector<bool> fulfilledArr(indexCount, false);

#ifndef DEBUG_HESSIAN
#pragma omp parallel for
#endif
            for (int j = startIndex; j < endIndex; ++j)
            {
                const QVector3D &p = lineSource->getVertices()[j];

                // Get the second derivatives.
                const float dnn = gridDDN->interpolateValue(p);
                const float dzz = gridDDZ->interpolateValue(p);
                const float dndz = gridDNDZ->interpolateValue(p);

                // Compute the eigenvalues
//                float lambdaN = std::numeric_limits<float>::max();
//                float lambdaZ = std::numeric_limits<float>::max();

                //computeEigenvalues(dnn, dzz, dndz, lambdaN, lambdaZ);
                const float D = dnn * dzz - dndz * dndz;

                fulfilledArr[endIndex - 1 - j] = dnn < lambdaThreshold && D > -lambdaThreshold;

#ifdef DEBUG_HESSIAN
                debugFile << std::fixed << std::setprecision(12);
                debugFile << "Point(" << p.x() << "," << p.y() << "," << p.z() << "): \t"
                          << "\t | dnn=" << dnn << "\t | dzz=" << dzz << "\t | dndz=" << dndz << "\t"
                          << "\t | lambdaN=" << lambdaN << "\t | lambdaZ=" << lambdaZ << "\n";
#endif

                // Check if the eigenvalues are below the user-defined threshold.
                // Save the results in a temporary array. This array is used to
                // fill line gaps if present caused by numerical issues.
//                fulfilledArr[endIndex - 1 - j] = lambdaN <= lambdaThreshold
//                              && lambdaZ <= lambdaThreshold;



            }

            // Search for line gaps (false accepted / rejected) and fill those.
            for (int j = startIndex; j < endIndex; ++j)
            {
                bool isFulfilled = fulfilledArr[endIndex - 1 - j];

                // Obtain the results for the two surrounding vertices.
                int prevJ = endIndex - 1 - std::max(j - 1, startIndex);
                int nextJ = endIndex - 1 - std::min(j + 1, endIndex - 1);

                // If the two surrounding values are classified as maximum
                // or minimum / saddle point, then this point was probably
                // falsely rejected / accepted.
                if (fulfilledArr[prevJ] != isFulfilled &&
                    fulfilledArr[nextJ] != isFulfilled)
                {
                    isFulfilled = fulfilledArr[prevJ];
                }

                if (isFulfilled)
                {
                    newIndexCount++;
                }
                else
                {
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

            // If the remaining vertices fulfill the filter criterion, push them to the next index
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

        multiVarInputSource->releaseData(gridDDN);
        multiVarInputSource->releaseData(gridDDZ);
        multiVarInputSource->releaseData(gridDNDZ);

#ifdef DEBUG_HESSIAN
        debugFile.close();
#endif
    }

    // Create the result of line selection and compute the new start indices /
    // index counts.
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


MTask *MHessianTrajectoryFilter::createTaskGraph(MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(multiVarInputSource          != nullptr);
    assert(lineRequest                  != "");

    MTask *task = new MTask(request, this);
    MDataRequestHelper rh(request);

    const QStringList members = rh.value("HESSIANFILTER_MEMBERS").split("/");
    const QStringList derivOps = rh.value("HESSIANFILTER_DERIVOPS").split("/");
    const QString varGeoPot = rh.value("HESSIANFILTER_GEOPOTENTIAL");
    const QString varGeoOnly = rh.value("HESSIANFILTER_GEOPOTENTIAL_TYPE");
    const QStringList uvVars = rh.value("HESSIANFILTER_VARIABLES").split("/");

    foreach (const auto& member, members)
    {
        MDataRequestHelper rhVar;

        rhVar.insert("MEMBER", member);
        rhVar.insert("VARIABLE", uvVars[0]);
        rhVar.insert("INIT_TIME", rh.value("INIT_TIME"));
        rhVar.insert("VALID_TIME", rh.value("VALID_TIME"));
        rhVar.insert("LEVELTYPE", rh.value("LEVELTYPE"));

        for (int i = 0; i < derivOps.size(); ++i)
        {
            rhVar.insert("MULTI_VARIABLES", uvVars[0] + "/" + uvVars[1]);
            rhVar.insert("MULTI_GEOPOTENTIAL", varGeoPot);
            rhVar.insert("MULTI_GEOPOTENTIAL_TYPE", varGeoOnly);
            rhVar.insert("MULTI_DERIVATIVE_OPS", derivOps[i]);

            varRequests.push_back(rhVar.request());
            task->addParent(multiVarInputSource->getTaskGraph(rhVar.request()));
        }
    }

    rh.removeAll(locallyRequiredKeys());

    // Get previous line selection
    task->addParent(inputSelectionSource->getTaskGraph(rh.request()));

    task->addParent(isoSurfaceIntersectionSource
                            ->getTaskGraph(lineRequest));

    return task;
}


bool MHessianTrajectoryFilter::computeEigenvalues(const float dnn,
                                                  const float dzz,
                                                  const float dndz,
                                                  float& lambda1, float& lambda2)
{
    // Compute the characteristic poly
    const double sumUV = dnn + dzz;
    const double prodUV = dnn * dzz;

    const double rootVal = sumUV * sumUV - 4 * (prodUV - dndz * dndz);

    // If the root is zero / positive, than the solution is non-complex and
    // we are able to obtain two real eigenvalues.
    if (rootVal >= 0)
    {
        const double root = std::sqrt(rootVal);

        // Obtain both eigenvalues
        lambda1 = (sumUV + root) * 0.5;
        lambda2 = (sumUV - root) * 0.5;

        return true;
    }
    else
    {
        return false;
    }
}


const QStringList MHessianTrajectoryFilter::locallyRequiredKeys()
{
    return (QStringList()
            << "HESSIANFILTER_VALUE" << "HESSIANFILTER_VARIABLES"
            << "HESSIANFILTER_MEMBERS" << "HESSIANFILTER_DERIVOPS"
            << "HESSIANFILTER_GEOPOTENTIAL" << "HESSIANFILTER_GEOPOTENTIAL_TYPE"
    );
}
