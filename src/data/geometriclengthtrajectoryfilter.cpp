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
#include "geometriclengthtrajectoryfilter.h"

// standard library imports

// related third party imports

// local application imports


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MGeometricLengthTrajectoryFilter::MGeometricLengthTrajectoryFilter() :
    MTrajectoryFilter(),
    isoSurfaceIntersectionSource(nullptr)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MGeometricLengthTrajectoryFilter::setIsosurfaceSource(
        MIsosurfaceIntersectionSource* s)
{
    isoSurfaceIntersectionSource = s;
    registerInputSource(isoSurfaceIntersectionSource);
    enablePassThrough(isoSurfaceIntersectionSource);
}


MTrajectoryEnsembleSelection*
MGeometricLengthTrajectoryFilter::produceData(MDataRequest request)
{
    assert(isoSurfaceIntersectionSource != nullptr);
    assert(inputSelectionSource         != nullptr);

    MDataRequestHelper rh(request);

    //int filterOp = rh.value("GEOLENFILTER_OP").toInt();
    // Geometric length threshold.
    float filterValue = rh.value("GEOLENFILTER_VALUE").toFloat();

    // Obtain the intersection line source.
    MIsosurfaceIntersectionLines* lineSource =
            isoSurfaceIntersectionSource->getData(lineRequest);

    rh.removeAll(locallyRequiredKeys());
    // Obtain the selection of intersection lines from the input intersection
    // line source.
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

            // Geometric length in km.
            float length = 0;
            // Nearly the distance of two grid points in latitudinal direction
            // on the Earth's sphere, ~111.2 km.
            const float deltaLat = 111.2;

            QVector3D p1 = lineSource->getVertices()[startIndex];
            //QVector3D& p0;

            for (int j = startIndex + 1; j < endIndex; ++j)
            {
                const QVector3D p0 = p1;
                p1 = lineSource->getVertices()[j];

                // Compute the distance between the two adjacent vertices in km.
                // The longitudinal distance vanishes towards the poles and
                // increases towards the equator. This effect is approximated by
                // multiplying the x-distance with the cosine of the current
                // latitude.
                QVector2D distance((p1.x() - p0.x())
                                   * std::cos(p1.y() * M_PI / 180.0f),
                                   p1.y() - p0.y());

                length += distance.length() * deltaLat;
            }

            // Filter the line by the user-defined geometric length threshold.
            if (length >= filterValue)
            {
                newStartIndices.push_back(startIndex);
                newIndexCounts.push_back(indexCount);
                newNumTrajectories++;
            }
        }

        ensNewIndexCount = newNumTrajectories - ensNewStartIndex;

        newEnsStartIndices.push_back(ensNewStartIndex);
        newEnsIndexCounts.push_back(ensNewIndexCount);
    }

    // Create the new selection of trajectory lines.
    MWritableTrajectoryEnsembleSelection *filterResult =
            new MWritableTrajectoryEnsembleSelection(
                lineSelection->refersTo(),
                newNumTrajectories,
                lineSelection->getTimes(),
                lineSelection->getStartGridStride(),
                numEnsembles);

    // Write back only those lines that satisfied the threshold criterion.
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


MTask *MGeometricLengthTrajectoryFilter::createTaskGraph(MDataRequest request)
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
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MGeometricLengthTrajectoryFilter::locallyRequiredKeys()
{
    return (QStringList()
            << "GEOLENFILTER_OP" << "GEOLENFILTER_VALUE"
            );
}

} // namespace Met3D
