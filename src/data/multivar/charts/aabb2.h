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
#ifndef MET_3D_AABB2_H
#define MET_3D_AABB2_H
// standard library imports
#include <cfloat>

// related third party imports
#include <QVector2D>

// local application imports

class AABB2 {
public:
    QVector2D min, max;

    AABB2() : min(FLT_MAX, FLT_MAX), max(-FLT_MAX, -FLT_MAX) {}
    AABB2(const QVector2D& min, const QVector2D& max) : min(min), max(max) {}

    inline QVector2D getDimensions() const { return max - min; }
    inline QVector2D getExtent() const { return (max - min) / 2.0f; }
    inline QVector2D getCenter() const { return (max + min) / 2.0f; }
    inline const QVector2D& getMinimum() const { return min; }
    inline const QVector2D& getMaximum() const { return max; }
    inline float getWidth() const { return max.x() - min.x(); }
    inline float getHeight() const { return max.y() - min.y(); }

    //! Returns whether the two AABBs intersect.
    bool intersects(const AABB2& otherAABB);
    //! Merge the two AABBs.
    void combine(const AABB2& otherAABB);
    //! Merge AABB with a point.
    void combine(const QVector2D& pt);
    //! Returns whether the AABB contain the point.
    bool contains(const QVector2D& pt);
};

#endif //MET_3D_AABB2_H
