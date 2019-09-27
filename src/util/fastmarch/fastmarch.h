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
#ifndef FASTMARCH_H
#define FASTMARCH_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include <QVector>

namespace Met3D
{
namespace MFastMarch
{

struct MHeapElement
{
public:
    MHeapElement()
    { this->vertexID = 0; this->distance = 0.f; }
    MHeapElement(int vertexID, float distance)
    { this->vertexID = vertexID; this->distance = distance; }
    friend bool operator>(const MHeapElement a, const MHeapElement b)
    {
        return a.distance > b.distance;
    }
    friend bool operator==(const MHeapElement a, const MHeapElement b)
    {
        return (a.vertexID == b.vertexID) && (a.distance == b.distance);
    }

    int vertexID;
    float distance;
};

struct MIntVector2D
{
public:
    MIntVector2D() { this->x = 0; this->y = 0; }

    MIntVector2D(int v) { this->x = v; this->y = v; }

    MIntVector2D(int x, int y) { this->x = x; this->y = y; }

    friend MIntVector2D operator+(const MIntVector2D a, const MIntVector2D b)
    {
        return MIntVector2D(a.x + b.x, a.y + b.y);
    }

    int x;
    int y;
};

/**
 * @brief Performs the fast marching method using a minimum heap to compute the
 * distamce field from @p scalarField for @isoValue.
 *
 * @param scalarField Input matrix, a 2D scalar field the distance field is
 * compute from. Dimensions should fit @p resolution.
 * @param isoValue Iso value the distance field is computed for.
 * @param resolution Size of the complete scalar field.
 * @param offset Index offset needed to compute the distance field from the
 * [sub] region the distance field defined by the bounding box.
 * @param maxDistance maximum distance a vertex can have to a contour. Needs to
 *  be set correctly since it influences the output (i.e. distance variability
 *  plot can look different for different maxDistance values)!
 * @param distanceField Array the distance field is stored to. Needs to be of
 *  the same size as scalarField.
 * @param regionResolution Size of the sub region the distance field should be
 *  computed from. Must not exceed @p resolution.
 * @param gridIsCyclicInLongitude Indicator whether the grid is cyclic or not.
 */
void fastMarch2D(const float *scalarField, float isoValue,
                 MIntVector2D &resolution, MIntVector2D &offset,
                 const float &maxDistanceSquared, MIntVector2D &regionResolution,
                 bool gridIsCyclicInLongitude, float *distanceField);

} // namespace MFastMarch
} // namespace Met3D

#endif // FASTMARCH_H
