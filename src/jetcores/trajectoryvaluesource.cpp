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
#include "trajectoryvaluesource.h"

// standard library imports

// related third party imports

// local application imports


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryValueSource::MTrajectoryValueSource()
        : MScheduledDataSource(), valueSource(nullptr), thicknessSource(nullptr)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MTrajectoryValueSource::setIsosurfaceSource(
        MIsosurfaceIntersectionSource* s)
{
    isoSurfaceIntersectionSource = s;
    registerInputSource(isoSurfaceIntersectionSource);
    enablePassThrough(isoSurfaceIntersectionSource);
}


void MTrajectoryValueSource::setInputSelectionSource(
        MTrajectorySelectionSource *s)
{
    inputSelectionSource = s;
    registerInputSource(inputSelectionSource);
    enablePassThrough(inputSelectionSource);
}


void MTrajectoryValueSource::setInputSourceValueVar(
        MWeatherPredictionDataSource *inputSource)
{
    valueSource = inputSource;

    if (!valueSource)
    {
        return;
    }
    registerInputSource(valueSource);
    enablePassThrough(valueSource);
}


void MTrajectoryValueSource::setInputSourceThicknessVar(
        MWeatherPredictionDataSource *inputSource)
{
    thicknessSource = inputSource;

    if (!thicknessSource)
    {
        return;
    }
    registerInputSource(thicknessSource);
    enablePassThrough(thicknessSource);
}


MTrajectoryValues* MTrajectoryValueSource::produceData(MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(inputSelectionSource         != nullptr);
    assert(lineRequest                  != "");

    MDataRequestHelper rh(request);

    const QStringList members = rh.value("TRAJECTORYVALUES_MEMBERS").split("/");

    MIsosurfaceIntersectionLines* lineSource =
            isoSurfaceIntersectionSource->getData(lineRequest);

    rh.removeAll(locallyRequiredKeys());
    MTrajectoryEnsembleSelection* lineSelection =
            static_cast<MTrajectoryEnsembleSelection*>(
                    inputSelectionSource->getData(rh.request()));

    MStructuredGrid *gridSource = nullptr;
    MStructuredGrid *gridThickness = nullptr;

    const int numTrajectories = lineSelection->getNumTrajectories();
    int numVertices = 0;

    for (int i = 0; i < numTrajectories; ++i)
    {
        numVertices += lineSelection->getIndexCount()[i];
    }

    // Contains the array of all values for each vertex.
    MTrajectoryValues *result = new MTrajectoryValues(numVertices * 2);

    QVector<GLint> ensStartIndices = lineSelection->getEnsembleStartIndices();
    QVector<GLsizei> ensIndexCounts = lineSelection->getEnsembleIndexCount();

    int counter = 0;

    // Iterate over all members and filter the lines corresponding to that member.
    for (uint ee = 0; ee < static_cast<uint>(members.size()); ++ee)
    {
        // Obtain the start and end line index for the current member.
        const int ensStartIndex = ensStartIndices[ee];
        const int ensIndexCount = ensIndexCounts[ee];
        const int ensEndIndex = ensStartIndex + ensIndexCount;

        QString varRequest = "";

        // Obtain the grid of the variable chosen for value-sampling.
        if (valueSource)
        {
            varRequest = varRequests[0];
            varRequests.pop_front();
            gridSource = valueSource->getData(varRequest);
        }

        // Obtain the grid of the selected variable for thickness mapping.
        if (thicknessSource)
        {
            varRequest = varRequests[0];
            varRequests.pop_front();
            gridThickness = thicknessSource->getData(varRequest);
        }

        for (int i = ensStartIndex; i < ensEndIndex; ++i)
        {
            int startIndex = lineSelection->getStartIndices()[i];
            const int indexCount = lineSelection->getIndexCount()[i];
            const int endIndex = startIndex + indexCount;

            for (int j = startIndex; j < endIndex; ++j)
            {
                const QVector3D& point = lineSource->getVertices()[j];

                // Obtain the value at the line vertex if any variable was
                // selected.
                const float sourceVal = (gridSource) ?
                                        gridSource->interpolateValue(point) : 0;

                // Obtain the value for thickness mapping at the line vertex if
                // any variable was selected.
                const float thicknessVal =
                        (gridThickness) ?
                            gridThickness->interpolateValue(point) : 0;

                // Add both values to the result array.
                result->setVertex(counter++, sourceVal);
                result->setVertex(counter++, thicknessVal);
            }
        }
    }

    return result;
}


MTask *MTrajectoryValueSource::createTaskGraph(MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(inputSelectionSource         != nullptr);
    //assert(valueSource                  != nullptr);
    //assert(thicknessSource              != nullptr);
    assert(lineRequest                  != "");

    MTask *task = new MTask(request, this);
    MDataRequestHelper rh(request);

    const QStringList members = rh.value("TRAJECTORYVALUES_MEMBERS").split("/");
    const QString sourceVar = rh.value("TRAJECTORYVALUES_VARIABLE");
    const QString thicknessVar = rh.value("TRAJECTORYVALUES_THICKNESSVAR");

    foreach (const auto& member, members)
    {
        MDataRequestHelper rhVar;
        rhVar.insert("MEMBER", member);
        rhVar.insert("INIT_TIME", rh.value("INIT_TIME"));
        rhVar.insert("VALID_TIME", rh.value("VALID_TIME"));
        rhVar.insert("LEVELTYPE", rh.value("LEVELTYPE"));

        if (valueSource)
        {
            rhVar.insert("VARIABLE", sourceVar);
            varRequests.push_back(rhVar.request());
            task->addParent(valueSource->getTaskGraph(rhVar.request()));
        }

        if (thicknessSource)
        {
            rhVar.insert("VARIABLE", thicknessVar);
            varRequests.push_back(rhVar.request());
            task->addParent(thicknessSource->getTaskGraph(rhVar.request()));
        }
    }

    rh.removeAll(locallyRequiredKeys());

    // Get previous line selection.
    task->addParent(inputSelectionSource->getTaskGraph(rh.request()));

    task->addParent(isoSurfaceIntersectionSource
                            ->getTaskGraph(lineRequest));

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MTrajectoryValueSource::locallyRequiredKeys()
{
    return (QStringList()
            << "TRAJECTORYVALUES_VARIABLE" << "TRAJECTORYVALUES_MEMBERS"
            << "TRAJECTORYVALUES_THICKNESSVAR"
    );
}

} // namespace Met3D
