/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015      Florian Ferstl
**  Copyright 2015-2017 Bianca Tost
**  Copyright 2015-2017 Marc Rautenhaus
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
#include "fastmarch.h"

// standard library imports
#include <math.h>
#include <queue>
#include <iostream>

// related third party imports
#include <QtCore>
#include <QVector2D>
#include <QVector>

// local application imports
#include "util/mutil.h"


namespace Met3D
{
namespace MFastMarch
{

void addSurfacePoint(QVector2D contourPos, MIntVector2D resolution,
                     QVector<int> &step, QVector<QVector2D> &contourPositions,
                     float *distanceField,
                     std::function<bool (int lon)> checkLonCoordinate)
{
    const float r = 2;
    const float rSqr = r * r;
    MIntVector2D minVertex(int(contourPos.x() - r) + 1,
                           int(contourPos.y() - r) + 1);

    MIntVector2D maxVertex(int(contourPos.x() + r),
                           int(contourPos.y() + r));

    minVertex.x = std::min(std::max(minVertex.x, 0), resolution.x - 1);
    minVertex.y = std::min(std::max(minVertex.y, 0), resolution.y - 1);
    maxVertex.x = std::min(std::max(maxVertex.x, 0), resolution.x - 1);
    maxVertex.y = std::min(std::max(maxVertex.y, 0), resolution.y - 1);

    MIntVector2D vertex;
    for (vertex.y = minVertex.y; vertex.y <= maxVertex.y; vertex.y++)
    {
        for (vertex.x = minVertex.x; vertex.x <= maxVertex.x; vertex.x++)
        {
            // Skip vertices which aren't part of the region the distance field
            // is computed for.
            if (!checkLonCoordinate(vertex.x))
            {
                continue;
            }

            float distance = (QVector2D(vertex.x, vertex.y)
                    - contourPos).lengthSquared();

            if (distance >= rSqr)
            {
                continue;
            }

            int iv = vertex.y * resolution.x + vertex.x % resolution.x;

            if (distance < distanceField[iv])
            {
                distanceField[iv] = distance;
                contourPositions[iv] = contourPos;
                step[iv] = 0;
            }
        }
    }
}


void fastMarch2D(const float* scalarField, float isoValue,
                 MIntVector2D &resolution, MIntVector2D &offset,
                 const float &maxDistanceSquared, MIntVector2D &regionResolution,
                 bool gridIsCyclicInLongitude, float* distanceField)
{
    MIntVector2D northEastRegion = offset + regionResolution;

    int westLonRegion = offset.x;
    int eastLonRegion = (westLonRegion + regionResolution.x) % (resolution.x);

    // Method to test if lon is inside the region the distance field is computed
    // for. (Check needs to look different if the region falls apart;
    // compare: MNWP2DHorizontalActorVariable::computeRenderRegionParameters() )
    // The Method also takes a "cycilc shift" into account if the grid is cyclic.
    std::function<bool (int lon)> checkLonCoordinate;
    if (westLonRegion < eastLonRegion)
    {
        // Apply cyclic shift to vertices since the grid is cyclic.
        if (gridIsCyclicInLongitude)
        {
            checkLonCoordinate = [&westLonRegion, &eastLonRegion,
                    &resolution](int lon)
            {
                lon = lon % resolution.x;
                return westLonRegion <= lon && lon < eastLonRegion;
            };
        }
        // Don't to apply "cycilc shift" to vertices since the grid isn't cyclic.
        else
        {
            checkLonCoordinate = [&westLonRegion, &eastLonRegion,
                    &resolution](int lon)
            {
                return westLonRegion <= lon && lon < eastLonRegion;
            };
        }
    }
    else
    {
        // Apply cyclic shift to vertices since the grid is cyclic.
        if (gridIsCyclicInLongitude)
        {
            checkLonCoordinate = [&westLonRegion, &eastLonRegion,
                    &resolution](int lon)
            {
                lon = lon % resolution.x;
                return (0 <= lon && lon < eastLonRegion)
                        || (westLonRegion <= lon && lon < resolution.x);
            };
        }
        // Don't to apply "cycilc shift" to vertices since the grid isn't cyclic.
        else
        {
            checkLonCoordinate = [&westLonRegion, &eastLonRegion,
                    &resolution](int lon)
            {
                return (0 <= lon && lon < eastLonRegion)
                        || (westLonRegion <= lon && lon < resolution.x);
            };
        }
    }

    // Stores whether vertex at index has been visited (1) or not (-1) and if
    // the search has been completed for this vertex (0 or 2).
    QVector<int> step(resolution.x * resolution.y, -1);
    // Stoes the position of the contour the vertex with the same index is
    // nearest to.
    QVector<QVector2D> nearestContourPositions(resolution.x * resolution.y,
                                               QVector2D(-1.f, -1.f));

    // Initialize distanceField.
    for (int i = 0; i < (resolution.x * resolution.y); i++)
    {
        distanceField[i] = maxDistanceSquared;
    }

    // Initialize distanceField around intersected grid edges.
    MIntVector2D vertex;

    for (vertex.y = offset.y;
         vertex.y < std::min(northEastRegion.y, resolution.y);
         vertex.y++)
    {
        for (vertex.x = offset.x; vertex.x < northEastRegion.x;
             vertex.x++)
        {
            for (int d = 0; d < 2; d++)
            {
                MIntVector2D neighbour = vertex;
                neighbour.x = neighbour.x % resolution.x;
                switch (d)
                {
                case 0:
                    neighbour.x++;
                    break;
                case 1:
                    neighbour.y++;
                    break;
                }

                if (checkLonCoordinate(neighbour.x)
                        && offset.y <= neighbour.y
                        && neighbour.y < northEastRegion.y)
                {
                    int iv = vertex.y * resolution.x
                            + vertex.x % resolution.x;
                    int in = neighbour.y * resolution.x
                            + neighbour.x % resolution.x;

                    // Avoid close to zero values to circumvent special
                    // cases.
                    float isoValueDiffVertex = scalarField[iv] < isoValue
                            ? std::min(scalarField[iv] - isoValue,
                                       (float)-1E-8)
                            : std::max(scalarField[iv] - isoValue,
                                       (float)1E-8);
                    float isoValueDiffNeighbour = scalarField[in] < isoValue
                            ? std::min(scalarField[in] - isoValue,
                                       (float)-1E-8)
                            : std::max(scalarField[in] - isoValue,
                                       (float)1E-8);

                    // Check for edge/zero-contour intersection.
                    // If isoValueDiffVertex and isoValueDiffNeighbour differ in
                    // sign (isoValueDiffVertex * isoValueDiffNeighbour < 0), we
                    // cross a iso contour between the vertices at index in and
                    // iv.
                    if (isoValueDiffVertex * isoValueDiffNeighbour < 0)
                    {
                        float t = isoValueDiffVertex
                                / (isoValueDiffVertex - isoValueDiffNeighbour);

                        QVector2D contourPos = QVector2D(vertex.x, vertex.y);
                        contourPos.setX(MMOD(contourPos.x(), resolution.x));
                        contourPos[d] += t;
                        addSurfacePoint(contourPos, resolution, step,
                                        nearestContourPositions, distanceField,
                                        checkLonCoordinate);
                    }
                }
            }
        }
    }

    // Initialize min-heap.
    QVector<MHeapElement> minHeap;

    MIntVector2D neighbours[9] =
    { MIntVector2D(-1,-1), MIntVector2D(-1, 0), MIntVector2D(-1, 1),
      MIntVector2D( 0,-1),                      MIntVector2D( 0, 1),
      MIntVector2D( 1,-1), MIntVector2D( 1, 0), MIntVector2D( 1, 1) };

    for (vertex.y = offset.y;
         vertex.y < std::min(northEastRegion.y, resolution.y);
         vertex.y++)
    {
        for (vertex.x = offset.x; vertex.x < northEastRegion.x;
             vertex.x++)
        {
            int iv = vertex.y * resolution.x + vertex.x % resolution.x;

            if (step[iv] == -1)
            {
                float minDistance = maxDistanceSquared;

                for (int d = 0;
                     d < (int)(sizeof(neighbours) / sizeof(neighbours[0]));
                     d++)
                {
                    MIntVector2D neighbour = vertex;
                    neighbour.x = neighbour.x % resolution.x;
                    neighbour = neighbour + neighbours[d];
                    int in = neighbour.y * resolution.x
                            + neighbour.x % resolution.x;

                    if (checkLonCoordinate(neighbour.x)
                            && offset.y <= neighbour.y
                            && neighbour.y < northEastRegion.y
                            && step[in] == 0)
                    {
                        QVector2D vertexFloat = QVector2D(vertex.x, vertex.y);
                        vertexFloat.setX(MMOD(vertexFloat.x(), resolution.x));
                        float distance =
                                (QVector2D(vertexFloat)
                                 - nearestContourPositions[in]).lengthSquared();
                        if (distance < minDistance)
                        {
                            minDistance = distance;
                            distanceField[iv] = distance;
                            nearestContourPositions[iv] =
                                    nearestContourPositions[in];
                            // Mark vertex as visited.
                            step[iv] = 1;
                        }
                    }
                }

                if (minDistance < maxDistanceSquared)
                {
                    minHeap.push_back(MHeapElement(iv, minDistance));
                }
            }
        }
    }

    std::make_heap(minHeap.begin(), minHeap.end(), std::greater<MHeapElement>());

    // Perform marching.
    while (minHeap.size() > 0)
    {
        // Get minimum element (smallest distance to closest contour) from the
        // heap.
        std::pop_heap(minHeap.begin(), minHeap.end(),
                      std::greater<MHeapElement>());
        MHeapElement minElement = minHeap.takeLast();
        int iv = minElement.vertexID;
        vertex.x = iv % resolution.x;
        vertex.y = iv / resolution.x;

        // Mark vertex as processed.
        step[iv] = 2;

        for (int d = 0; d < (int)(sizeof(neighbours) / sizeof(neighbours[0]));
             d++)
        {
            MIntVector2D neighbour = vertex;
            neighbour.x = neighbour.x % resolution.x;
            neighbour = neighbour + neighbours[d];

            if (checkLonCoordinate(neighbour.x)
                    && offset.y <= neighbour.y
                    && neighbour.y < northEastRegion.y)
            {
                int in = neighbour.y * resolution.x
                        + neighbour.x % resolution.x;
                // Neighbor is new vertex.
                if (step[in] == -1)
                {
                    float distance = (QVector2D(neighbour.x, neighbour.y)
                                      - nearestContourPositions[iv]).lengthSquared();
                    if (distance < maxDistanceSquared)
                    {
                        distanceField[in] = distance;
                        nearestContourPositions[in] = nearestContourPositions[iv];
                        // Mark vertex as visited.
                        step[in] = 1;
                        minHeap.push_back(MHeapElement(in, distance));
                        std::push_heap(minHeap.begin(), minHeap.end(),
                                       std::greater<MHeapElement>());
                    }
                }
                // Neighbor vertex is in queue.
                else if (step[in] == 1)
                {
                    float distance = (QVector2D(neighbour.x, neighbour.y)
                                      - nearestContourPositions[iv]).lengthSquared();
                    if (distance < distanceField[in])
                    {
                        // Find element in vector to be changed.
                        int hIndex = minHeap.indexOf(
                                    MHeapElement(in, distanceField[in]));
                        minHeap[hIndex].distance = distance;
                        // Since the distance is decreased, it is only necessary
                        // to resort the heap beginning with the element changed.
                        std::make_heap(minHeap.begin() + hIndex, minHeap.end(),
                                       std::greater<MHeapElement>());

                        distanceField[in] = distance;
                        nearestContourPositions[in] = nearestContourPositions[iv];
                    }
                }
            }
        }
    }

    int index = 0;
    int x = 0, y = 0;

    int domainSize = resolution.x * resolution.y;
    // Set sign of distances and take sqrt.
    for (int i = 0; i < (regionResolution.x * regionResolution.y); i++)
    {
        // Compute x and y coordinates from i.
        y = i / regionResolution.x;
        x = i - y * regionResolution.x;

        x += offset.x;
        y += offset.y;

        x = x % resolution.x;

        if (index < domainSize)
        {
            index = x + y * resolution.x;
            distanceField[index] = scalarField[index] < isoValue
                    ? -std::sqrt(distanceField[index])
                    :  std::sqrt(distanceField[index]);
        }
    }
}

} // namespace MFastMarch
} // namespace Met3D
