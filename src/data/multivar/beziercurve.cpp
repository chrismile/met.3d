/******************************************************************************
**
** This file contains adapted code from the Embree library.
** The original code is covered by the Apache License, Version 2.0, and the
** changes are covered by the GNU General Public License (see below).
**
*******************************************************************************
**
** Copyright 2009-2020 Intel Corporation
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**     http://www.apache.org/licenses/LICENSE-2.0
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020-2021 Christoph Neuhauser
**  Copyright 2020      Michael Kern
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

#include "beziercurve.hpp"

namespace Met3D {

inline QVector3D lerp(const QVector3D& p0, const QVector3D& p1, float t)
{
    return (1.0f - t) * p0 + t * p1;
}

// Bezier Curve code --> quick evaluation
// https://github.com/embree/embree/blob/master/kernels/subdiv/bezier_curve.h

MBezierCurve::MBezierCurve(const std::array<QVector3D, 4> &points, const float _minT, const float _maxT)
        : controlPoints(points), minT(_minT), maxT(_maxT)
        {
    totalArcLength = evalArcLength(minT, maxT, 20);
}

void MBezierCurve::evaluate(const float t, QVector3D &P, QVector3D &dt) const {
    assert(minT <= t && t <= maxT);

    float tN = normalizeT(t);

    // De-Casteljau algorithm
    auto P01 = lerp(controlPoints[0], controlPoints[1], tN);
    auto P02 = lerp(controlPoints[1], controlPoints[2], tN);
    auto P03 = lerp(controlPoints[2], controlPoints[3], tN);
    auto P11 = lerp(P01, P02, tN);
    auto P12 = lerp(P02, P03, tN);
    P = lerp(P11, P12, tN);

//    auto testDT = derivative(t);

    dt = 3.0f * (P12 - P11);
}

QVector3D MBezierCurve::derivative(const float t) const {
    assert(minT <= t && t <= maxT);

    float tN = normalizeT(t);

    // De-Casteljau algorithm
    auto P01 = lerp(controlPoints[0], controlPoints[1], tN);
    auto P02 = lerp(controlPoints[1], controlPoints[2], tN);
    auto P03 = lerp(controlPoints[2], controlPoints[3], tN);
    auto P11 = lerp(P01, P02, tN);
    auto P12 = lerp(P02, P03, tN);

    auto dt = 3.0f * (P12 - P11);

    return dt;
}

QVector3D MBezierCurve::curvature(const float t) const {
    assert(minT <= t && t <= maxT);
    return QVector3D(0.0f, 0.0, 0.0f);
}


float MBezierCurve::evalArcLength(const float _minT, const float _maxT, const uint16_t numSteps) const {
    assert(minT <= _minT && _minT <= maxT);
    assert(minT <= _maxT && _maxT <= maxT);
    assert(_minT <= _maxT);

    // Normalize min and max borders
    float minTN = _minT;
    float maxTN = _maxT;

    // Trapezoidal rule for integration
    float h = (maxTN - minTN) / float(numSteps - 1);

    float L = 0.0f;

    for (auto i = 0; i < numSteps; ++i)
    {
        // Avoid overflow due to numerical error
        float curT = std::min(minTN + i * h, maxTN);
        float segmentL = derivative(curT).length();

        if (i > 0 || i < numSteps - 1)
        {
            segmentL *= 2.0f;
        }

        L += segmentL;
    }

    L *= (h / 2.0f);
    return L;
}


// See Eberly Paper: Moving along curve with constant speed
float MBezierCurve::solveTForArcLength(const float _arcLength) const {
    assert(_arcLength <= totalArcLength && _arcLength >= 0.0);

    // Initial guess
    float t = minT + _arcLength / totalArcLength * (maxT - minT);

    float delta = 1E-5;
    // We use bisection to get closer to our solution
    float lower = minT;
    float upper = maxT;

    const uint32_t numIterations = 20;
    // Use Newton-Raphson algorithm to find t at arc length along each curve
    for (uint32_t i = 0; i < numIterations; ++i)
    {
        float C = evalArcLength(minT, t, 20) - _arcLength;

        // Early termination if t was found
        if (std::abs(C) <= delta)
        {
            return t;
        }

        // Derivative should be >= 0
        float dCdt = derivative(t).length();

        // Newton-Raphson step
        float tCandidate = t - C / dCdt;

        // Bisection
        if (C > 0)
        {
            upper = t;
            if (tCandidate <= lower)
            {
                t = (upper + lower) / 2.0f;
            }
            else
            {
                t = tCandidate;
            }
        }
        else
        {
            lower = t;
            if (tCandidate >= upper)
            {
                t = (upper + lower) / 2.0f;
            }
            else
            {
                t = tCandidate;
            }
        }
    }

    return t;
}

}
