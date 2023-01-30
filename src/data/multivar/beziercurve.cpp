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

// standard library imports
#include <cmath>
#include <limits>
#include <algorithm>
#include <exception>

// related third party imports
#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "beziercurve.h"

namespace Met3D
{

inline float pow2(float x) { return x * x; }
inline float pow3(float x) { return x * x * x; }

/**
 * Finds a root of the cubic polynomial with the passed coefficients.
 * @param x The x coordinate of the Bezier curve.
 * @param b0 Cubic polynomial coefficient 0.
 * @param b1 Cubic polynomial coefficient 1.
 * @param b2 Cubic polynomial coefficient 2.
 * @param b3 Cubic polynomial coefficient 3.
 * @return The t parameter of the Bezier curve at x.
 */
float findRoot(float x, float b0, float b1, float b2, float b3)
{
    // Use Newton's method to find the root of the polynomial: https://en.wikipedia.org/wiki/Newton%27s_method
    float epsilon = 1e-6f;
    float epsilon2 = 1e-7f;
    float t = x;
    for (int i = 0; i < 8; i++) {
        float tSq = t * t;
        float x_new = b0 + b1 * t + b2 * tSq + b3 * t * tSq;
        if (std::abs(x_new) < epsilon) {
            return t;
        }
        float xprime_new = b1 + 2.0f * b2 * t + 3.0f * b3 * tSq;
        if (std::abs(xprime_new) < epsilon2) {
            break;
        }
        t = t - x_new / xprime_new;
    }

    // If we found no root after 8 iterations, use the Bisection method: https://en.wikipedia.org/wiki/Bisection_method
    float t0 = 0.0f;
    float t1 = 1.0f;
    t = x;
    while (t0 < t1) {
        float tSq = t * t;
        float x_new = b0 + b1 * t + b2 * tSq + b3 * t * tSq;
        if (abs(x_new) < epsilon) {
            return t;
        }
        if (x_new < 0.0f) {
            t0 = t;
        } else {
            t1 = t;
        }
        t = 0.5f * (t1 - t0) + t0;
    }

    // Could not find any root of the polynomial.
    return std::numeric_limits<float>::quiet_NaN();
}

float evaluateCubicBezier(float x, QVector2D p0, QVector2D p1, QVector2D p2, QVector2D p3)
{
    float b0 = p0[0] - x;
    float b1 = -3 * p0[0] + 3 * p1[0];
    float b2 = 3 * p0[0] - 6 * p1[0] + 3 * p2[0];
    float b3 = -p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0];
    float t = findRoot(x, b0, b1, b2, b3);

    if (std::isnan(t)) {
        throw std::runtime_error("Error: Could not find a root of the polynomial.");
    }

    float y = pow3(1.0f - t) * p0[1] + 3.0f * pow2(1.0f - t) * t * p1[1] + 3.0f * (1.0f - t) * pow2(t) * p2[1] + pow3(t) * p3[1];
    return y;
}

const float PI = 3.1415926535897932f;

void cameraPathCircle(
        float timePercentage, float acceleration,
        QVector3D center, float radius,
        float angleStart, float angleEnd, float pitch,
        QVector3D& cameraPosition, float& yaw)
{
    QVector2D p0(0.0f, 0.0f);
    QVector2D p1(0.0f + acceleration, 0.0f);
    QVector2D p2(1.0f - acceleration, 1.0f);
    QVector2D p3(1.0f, 1.0f);
    float t = evaluateCubicBezier(timePercentage, p0, p1, p2, p3);
    float angle = angleStart + t * (angleEnd - angleStart);
    yaw = angle;

    cameraPosition = QVector3D(
            -std::cos(angle) * radius * std::cos(pitch) + center[0],
            -std::sin(angle) * radius * std::cos(pitch) + center[1],
            std::sin(pitch) * radius + center[2]);
}

}
