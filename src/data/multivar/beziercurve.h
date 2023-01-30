/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2023 Christoph Neuhauser
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
#ifndef BEZIERCURVE_H
#define BEZIERCURVE_H

// standard library imports
#include <QVector2D>
#include <QVector3D>

// related third party imports

// local application imports

namespace Met3D
{

/**
 * Returns the y value of a cubic bezier curve for the given x coordinate.
 * @param x The x coordinate of the bezier curve.
 * @param p0 Control point 0.
 * @param p1 Control point 1.
 * @param p2 Control point 2.
 * @param p3 Control point 3.
 * @return The y coordinate of the bezier curve at x.
 */
float evaluateCubicBezier(float x, QVector2D p0, QVector2D p1, QVector2D p2, QVector2D p3);

void cameraPathCircle(
        float timePercentage, float acceleration,
        QVector3D center, float radius,
        float angleStart, float angleEnd, float pitch,
        QVector3D& cameraPosition, float& yaw);

}

#endif //BEZIERCURVE_H
