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
#include <limits>
#include <cmath>

// related third party imports

// local application imports
#include "spring.h"

float distanceMetric(float x_t, float y_i) {
    float diff = (x_t - y_i);
    return std::sqrt(diff * diff);
}

std::vector<SpringMatch> spring(const std::vector<float>& X, const std::vector<float>& Y, float epsilon) {
    int n = int(X.size());
    int m = int(Y.size());
    std::vector<float> d(m + 1, 0.0f);
    std::vector<float> d_prime(m + 1, 0.0f);
    std::vector<int> s(m + 1, 0);
    std::vector<int> s_prime(m + 1, 0);
    int t_s = 1;
    int t_e = 1;

    std::vector<SpringMatch> matches;

    float d_min = std::numeric_limits<float>::max();
    for (int i = 1; i <= m; i++) {
        d_prime[i] = std::numeric_limits<float>::max();
        d[i] = std::numeric_limits<float>::max();
    }

    for (int t = 1; t <= n; t++) {
        s_prime[0] = t;
        s[0] = t;

        for (int i = 1; i <= m; i++) {
            float d_best;
            if (d[i - 1] < d_prime[i] && d[i - 1] < d_prime[i - 1]) {
                d_best = d[i - 1];
                s[i] = s[i - 1];
            } else if (d_prime[i] < d_prime[i - 1]) {
                d_best = d_prime[i];
                s[i] = s_prime[i];
            } else {
                d_best = d_prime[i - 1];
                s[i] = s_prime[i - 1];
            }
            d[i] = distanceMetric(X[t - 1], Y[i - 1]) + d_best;
        }

        if (d_min <= epsilon) {
            bool  report = true;
            for (int i = 1; i <= m; i++) {
                if (!(d[i] >= d_min || s[i] > t_e)) {
                    report = false;
                    break;
                }
            }
            if (report) {
                SpringMatch match{};
                match.t_s = t_s;
                match.t_e = t_e;
                match.d_min = d_min;
                matches.push_back(match);
                d_min = std::numeric_limits<float>::max();
                for (int i = 1; i <= m; i++) {
                    if (s[i] <= t_e) {
                        d[i] = std::numeric_limits<float>::max();
                    }
                }
            }
        }

        if (d[m] <= epsilon && d[m] < d_min) {
            d_min = d[m];
            t_s = s[m];
            t_e = t;
        }

        for (int i = 0; i <= m; i++) {
            d_prime[i] = d[i];
            s_prime[i] = s[i];
        }
    }

    return matches;
}
