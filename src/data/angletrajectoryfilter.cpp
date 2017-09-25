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
#include "angletrajectoryfilter.h"

// standard library imports

// related third party imports

// local application imports


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MAngleTrajectoryFilter::MAngleTrajectoryFilter()
    : MTrajectoryFilter()
{
}


void MAngleTrajectoryFilter::setIsosurfaceSource(
        MIsosurfaceIntersectionSource* s)
{
    isoSurfaceIntersectionSource = s;
    registerInputSource(isoSurfaceIntersectionSource);
    enablePassThrough(isoSurfaceIntersectionSource);
}


MTrajectoryEnsembleSelection* MAngleTrajectoryFilter::produceData(
        MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(inputSelectionSource         != nullptr);

    MDataRequestHelper rh(request);

    float angleThreshold =
            rh.value("ANGLEFILTER_VALUE").toFloat() / 180.0 * M_PI;
    const QStringList members = rh.value("ANGLEFILTER_MEMBERS").split("/");

    MIsosurfaceIntersectionLines* lineSource =
            isoSurfaceIntersectionSource->getData(lineRequest);

    rh.removeAll(locallyRequiredKeys());
    MTrajectoryEnsembleSelection* lineSelection =
            static_cast<MTrajectoryEnsembleSelection*>(
                inputSelectionSource->getData(rh.request()));

    // Counts the number of new trajectories.
    int newNumTrajectories = 0;

    QVector<GLint>  newStartIndices;
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
                if (j == startIndex || j == endIndex - 1)
                {
                    newIndexCount++;
                    continue;
                }

                // Obtain the position of the current vertex point and its
                // surrounding vertices.
                const QVector2D& p0 =
                        lineSource->getVertices()[j - 1].toVector2D();
                const QVector2D& p1 = lineSource->getVertices()[j].toVector2D();
                const QVector2D& p2 =
                        lineSource->getVertices()[j + 1].toVector2D();

                // As we are on a sphere, the distance between two points in
                // longitudinal direction is not uniform. It decreases towards
                // the poles and increases towards to equator. Approximate this
                // effect by multiplying the distance with the cosine of the
                // current latitude.
                const float deltaLonFactor = std::cos(p1.y() / 180.0 * M_PI);

                // Normalize vectors before computing the angle.
                QVector2D prevTangent = (p1 - p0);
                prevTangent.setX(prevTangent.x() * deltaLonFactor);
                QVector2D nextTangent = (p2 - p1);
                nextTangent.setX(nextTangent.x() * deltaLonFactor);

                prevTangent.normalize();
                nextTangent.normalize();

                // Compute the angles between the current tangent and the
                // surrounding ones.
                double angleSegments =
                        std::acos(QVector2D::dotProduct(prevTangent,
                                                        nextTangent));

                // The angle between the segments should not exceed the
                // user-defined threshold.
                bool isFulfilled = angleSegments <= angleThreshold;

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
            // to the next index.
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

    // Create the result for each ensemble member.
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


MTask *MAngleTrajectoryFilter::createTaskGraph(MDataRequest request)
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
***                            PUBLIC METHODS                               ***
*******************************************************************************/

const QStringList MAngleTrajectoryFilter::locallyRequiredKeys()
{
    return (QStringList()
            << "ANGLEFILTER_VALUE"
            << "ANGLEFILTER_MEMBERS"
            );
}

} // namespace Met3D
