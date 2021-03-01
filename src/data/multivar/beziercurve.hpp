/******************************************************************************
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

#ifndef MET_3D_BEZIERCURVE_HPP
#define MET_3D_BEZIERCURVE_HPP

// standard library imports
#include <array>

// related third party imports
#include <QVector>
#include <QVector3D>

// local application imports

namespace Met3D {

class MBezierCurve {
public:
    explicit MBezierCurve(const std::array<QVector3D, 4>& points, const float _minT, const float _maxT);

    std::array<QVector3D, 4> controlPoints;
    float totalArcLength;
    float minT;
    float maxT;

    bool isInterval(const float t) { return (minT <= t && t <= maxT); }
    void evaluate(const float t, QVector3D& P, QVector3D& dt) const;
    QVector3D derivative(const float t) const;
    QVector3D curvature(const float t) const;
    float evalArcLength(const float _minT, const float _maxT, const uint16_t numSteps) const;
    float solveTForArcLength(const float _arcLength) const;
    float normalizeT(const float t) const { return (t - minT) / (maxT - minT); }

private:
    float denormalizeT(const float t) const { return t * (maxT - minT) + minT; }
};

}

#endif //MET_3D_BEZIERCURVE_HPP
