/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2021 Christoph Neuhauser
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
// standard library imports

// related third party imports

// local application imports
#include "aabb2.h"

bool AABB2::intersects(const AABB2& otherAABB)
{
    if (max.x() < otherAABB.min.x() || min.x() > otherAABB.max.x()
            || max.y() < otherAABB.min.y() || min.y() > otherAABB.max.y()) {
        return false;
    }
    return true;
}

void AABB2::combine(const AABB2& otherAABB)
{
    if (otherAABB.min.x() < min.x())
        min[0] = otherAABB.min.x();
    if (otherAABB.min.y() < min.y())
        min[1] = otherAABB.min.y();
    if (otherAABB.max.x() > max.x())
        max[0] = otherAABB.max.x();
    if (otherAABB.max.y() > max.y())
        max[1] = otherAABB.max.y();
}

void AABB2::combine(const QVector2D& pt)
{
    if (pt.x() < min.x())
        min[0] = pt.x();
    if (pt.y() < min.y())
        min[1] = pt.y();
    if (pt.x() > max.x())
        max[0] = pt.x();
    if (pt.y() > max.y())
        max[1] = pt.y();
}

bool AABB2::contains(const QVector2D& pt)
{
    return pt.x() >= min.x() && pt.y() >= min.y() && pt.x() <= max.x() && pt.y() <= max.y();
}
