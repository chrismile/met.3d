//
// Created by kerninator on 13.03.17.
//

#include "mprojectedvelocitiesfilter.h"
#include <log4cplus/loggingmacros.h>

using namespace Met3D;

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MProjectedVelocitiesFilter::MProjectedVelocitiesFilter()
        : MStructuredGridEnsembleFilter()
{

}

/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MTask* MProjectedVelocitiesFilter::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    const QStringList vars = rh.value("VS_UV_VARIABLES").split("/");

    rh.removeAll(locallyRequiredKeys());

    // before proceeding with this task, obtain all required variables
    for (const auto& var : vars)
    {
        rh.insert("VARIABLE", var);
        LOG4CPLUS_DEBUG(mlog, rh.request().toStdString());
        task->addParent(inputSource->getTaskGraph(rh.request()));
    }

    return task;
}

const QStringList MProjectedVelocitiesFilter::locallyRequiredKeys()
{
    return (QStringList() << "VS_UV_VARIABLES" << "VS_SDIRECTION");
}

MStructuredGrid* MProjectedVelocitiesFilter::produceData(
        MDataRequest request)
{
    MStructuredGrid *result = nullptr;

    MDataRequestHelper rh(request);
    const QStringList vars = rh.value("VS_UV_VARIABLES").split("/");
    const QStringList sDirs = rh.value("VS_SDIRECTION").split("/");
    rh.removeAll(locallyRequiredKeys());

    // before proceeding with this task, obtain all required variables
    rh.insert("VARIABLE", vars[0]);
    MStructuredGrid *gridU = inputSource->getData(rh.request());
    rh.insert("VARIABLE", vars[1]);
    MStructuredGrid *gridV = inputSource->getData(rh.request());
    //task->addParent(inputSource->getTaskGraph(rh.request()));

    QVector2D s(sDirs[0].toFloat(), sDirs[1].toFloat());
    s.normalize();

    // Create a new grid with the same topology as the input grid
    result = createAndInitializeResultGrid(gridU);
    //result->setGeneratingRequest(request);

    for (int k = 0; k < static_cast<int>(result->getNumLevels()); ++k)
    {
        for (int j = 0; j < static_cast<int>(result->getNumLats()); ++j)
        {
            for (int i = 0; i < static_cast<int>(result->getNumLons()); ++i)
            {
                // Current wind vector at sample grid point (k, j, i).
                QVector2D V(gridU->getValue(k, j, i), gridV->getValue(k, j, i));

                QVector2D projectedVel = QVector2D::dotProduct(V, s) * s;
                float velLength = static_cast<float>(projectedVel.length());

                result->setValue(k, j, i, velLength);
            }
        }
    }

    // release the recently created grids to reduce memory consumption
    inputSource->releaseData(gridU);
    inputSource->releaseData(gridV);

    return result;
}