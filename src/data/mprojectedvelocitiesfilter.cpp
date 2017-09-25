/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Michael Kern
**
**  Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  Met.3D is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Met.3D is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Met.3D.  If not, see <http://www.gnu.org/licenses/>.
**
*******************************************************************************/
#include "mprojectedvelocitiesfilter.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports


namespace Met3D
{

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

    // Before proceeding with this task, obtain all required variables.
    foreach (const QString var, vars)
    {
        rh.insert("VARIABLE", var);
        LOG4CPLUS_DEBUG(mlog, rh.request().toStdString());
        task->addParent(inputSource->getTaskGraph(rh.request()));
    }

    return task;
}


MStructuredGrid* MProjectedVelocitiesFilter::produceData(
        MDataRequest request)
{
    MStructuredGrid *result = nullptr;

    MDataRequestHelper rh(request);
    const QStringList vars = rh.value("VS_UV_VARIABLES").split("/");
    const QStringList sDirs = rh.value("VS_SDIRECTION").split("/");
    rh.removeAll(locallyRequiredKeys());

    // Before proceeding with this task, obtain all required variables.
    rh.insert("VARIABLE", vars[0]);
    MStructuredGrid *gridU = inputSource->getData(rh.request());
    rh.insert("VARIABLE", vars[1]);
    MStructuredGrid *gridV = inputSource->getData(rh.request());
    //task->addParent(inputSource->getTaskGraph(rh.request()));

    QVector2D s(sDirs[0].toFloat(), sDirs[1].toFloat());
    s.normalize();

    // Create a new grid with the same topology as the input grid.
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

    // Release the recently created grids to reduce memory consumption.
    inputSource->releaseData(gridU);
    inputSource->releaseData(gridV);

    return result;
}


/******************************************************************************
***                           PROTECTED METHODS                             ***
*******************************************************************************/

const QStringList MProjectedVelocitiesFilter::locallyRequiredKeys()
{
    return (QStringList() << "VS_UV_VARIABLES" << "VS_SDIRECTION");
}

} // namespace Met3D
