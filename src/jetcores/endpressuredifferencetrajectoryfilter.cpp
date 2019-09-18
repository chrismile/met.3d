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
#include "endpressuredifferencetrajectoryfilter.h"

// standard library imports

// related third party imports

// local application imports


namespace Met3D
{
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MEndPressureDifferenceTrajectoryFilter::MEndPressureDifferenceTrajectoryFilter()
    : MTrajectoryFilter()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MEndPressureDifferenceTrajectoryFilter::setIsosurfaceSource(
        MIsosurfaceIntersectionSource* s)
{
    isoSurfaceIntersectionSource = s;
    registerInputSource(isoSurfaceIntersectionSource);
    enablePassThrough(isoSurfaceIntersectionSource);
}


MTrajectoryEnsembleSelection*
MEndPressureDifferenceTrajectoryFilter::produceData(MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(inputSelectionSource != nullptr);

    MDataRequestHelper rh(request);

    float pressureDiffThreshold =
            rh.value("ENDPRESSUREDIFFFILTER_VALUE").toFloat();
    float angleThreshold = rh.value("ENDPRESSUREDIFFFILTER_ANGLE").toFloat();
    const QStringList members =
            rh.value("ENDPRESSUREDIFFFILTER_MEMBERS").split("/");

    MIsosurfaceIntersectionLines *lineSource =
            isoSurfaceIntersectionSource->getData(lineRequest);

    rh.removeAll(locallyRequiredKeys());
    MTrajectoryEnsembleSelection *lineSelection =
            static_cast<MTrajectoryEnsembleSelection *>(
                inputSelectionSource->getData(rh.request()));

    // Counts the number of new trajectories.
    int newNumTrajectories = 0;

    QVector<GLint> newStartIndices;
    QVector<GLsizei> newIndexCounts;

    QVector<GLint> newEnsStartIndices;
    QVector<GLsizei> newEnsIndexCounts;

    QVector<GLint> ensStartIndices = lineSelection->getEnsembleStartIndices();
    QVector<GLsizei> ensIndexCounts = lineSelection->getEnsembleIndexCount();

    const int numEnsembles = lineSelection->getNumEnsembleMembers();

    // Loop through each member and filter the lines corresponding to that
    // member.
    for (uint ee = 0; ee < static_cast<uint>(numEnsembles); ++ee)
    {
        // Obtain the start and end line index for the current member.
        const int ensStartIndex = ensStartIndices[ee];
        const int ensIndexCount = ensIndexCounts[ee];
        const int ensEndIndex = ensStartIndex + ensIndexCount;

        const int ensNewStartIndex = newStartIndices.size();
        int ensNewIndexCount = 0;

        for (int i = ensStartIndex; i < ensEndIndex; ++i)
        {
            int startIndex = lineSelection->getStartIndices()[i];
            const int indexCount = lineSelection->getIndexCount()[i];
            const int endIndex = startIndex + indexCount;

            int newIndexCount = 0;

            for (int j = startIndex; j < endIndex; ++j)
            {
                if (indexCount <= 2 || (j > startIndex && j < endIndex - 1))
                {
                    newIndexCount++;
                    continue;
                }

                QVector3D p0, p1, p2;

                // Obtain the first / last three points at each trajectory.
                if (j == startIndex)
                {
                    p0 = lineSource->getVertices()[j];
                    p1 = lineSource->getVertices()[j + 1];
                    p2 = lineSource->getVertices()[j + 2];
                }
                else
                {
                    p0 = lineSource->getVertices()[j];
                    p1 = lineSource->getVertices()[j - 1];
                    p2 = lineSource->getVertices()[j - 2];
                }

                const float deltaLonFactor = std::cos(p1.y() / 180.0 * M_PI);

                // Compute the segment directions between the three points.
                // Normalize vectors before computing the angle.
                QVector2D prevTangent = (p1 - p0).toVector2D();
                prevTangent.setX(prevTangent.x() * deltaLonFactor);
                QVector2D nextTangent = (p2 - p1).toVector2D();
                nextTangent.setX(nextTangent.x() * deltaLonFactor);

                prevTangent.normalize();
                nextTangent.normalize();

                // Compute the angles between the current and the next tangent.
                double angleSegments =
                        std::acos(QVector2D::dotProduct(prevTangent,
                                                        nextTangent));
                angleSegments *= 180.0f / M_PI;

                // Compute the pressure difference between the end and the
                // second / second-last point.
                double pressureDiff = std::abs(p1.z() - p0.z());

                // Filter out lines that exceed the angle and pressure
                // differnce thresholds.
                bool isFulfilled = ((angleSegments <= angleThreshold)
                                    && (pressureDiff <= pressureDiffThreshold));

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

            // If the remaining vertices fulfill the filter criterion, push them
            // to the next index
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
    }

    // Create the new result for each ensemble member.
    MWritableTrajectoryEnsembleSelection *filterResult =
            new MWritableTrajectoryEnsembleSelection(
                lineSelection->refersTo(),
                newNumTrajectories,
                lineSelection->getTimes(),
                lineSelection->getStartGridStride(),
                numEnsembles);

    for (int k = 0; k < newStartIndices.size(); ++k)
    {
        filterResult->setStartIndex(k, newStartIndices[k]);
        filterResult->setIndexCount(k, newIndexCounts[k]);
    }

    for (int e = 0; e < numEnsembles; ++e)
    {
        filterResult->setEnsembleStartIndex(e, newEnsStartIndices[e]);
        filterResult->setEnsembleIndexCount(e, newEnsIndexCounts[e]);
    }

    isoSurfaceIntersectionSource->releaseData(lineSource);
    inputSelectionSource->releaseData(lineSelection);

    return filterResult;
}


MTask *MEndPressureDifferenceTrajectoryFilter::createTaskGraph(
        MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(inputSelectionSource         != nullptr);
    assert(lineRequest                  != "");

    MTask *task = new MTask(request, this);
    MDataRequestHelper rh(request);

    rh.removeAll(locallyRequiredKeys());

    // Get previous line selection.
    task->addParent(inputSelectionSource->getTaskGraph(rh.request()));
    // Get original trajectory lines.
    task->addParent(isoSurfaceIntersectionSource
                    ->getTaskGraph(lineRequest));

    return task;
}


/******************************************************************************
***                           PROTECTED METHODS                             ***
*******************************************************************************/

const QStringList MEndPressureDifferenceTrajectoryFilter::locallyRequiredKeys()
{
    return (QStringList()
            << "ENDPRESSUREDIFFFILTER_VALUE"
            << "ENDPRESSUREDIFFFILTER_ANGLE"
            << "ENDPRESSUREDIFFFILTER_MEMBERS"
            );
}

} // namespace Met3D
