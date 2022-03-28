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
#ifndef MET_3D_SPRING_H
#define MET_3D_SPRING_H

// standard library imports
#include <vector>

// related third party imports

// local application imports

struct SpringMatch {
    int t_s; ///< Match start.
    int t_e; ///< Match end.
    float d_min;
};

enum class SubsequenceMatchingTechnique {
    SPRING, NSPRING
};
const char* const SUBSEQUENCE_MATCHING_TECHNIQUE_NAMES[] = {
        "SPRING", "NSPRING"
};

/**
 * Returns ranges of subsequences in the sequence X similar to the query sequence Y using Dynamic Time Warping (DTW).
 * The implemented algorithm, SPRING, is much faster and more memory efficient than a naive search and was first
 * presented in the following paper.
 * Sakurai, Y., Faloutsos, C., Yamamuro, M.: Stream monitoring under the time warping distance. In: Proceedings of IEEE
 * 23rd International Conference on Data Engineering (ICDE 2007), Istanbul, Turkey, April 15-20, pp. 1046–1055 (2007).
 * @param X The data sequence X.
 * @param Y The query sequence Y.
 * @param epsilon The
 * @return
 */
std::vector<SpringMatch> spring(const std::vector<float>& X, const std::vector<float>& Y, float epsilon);

/**
 * Returns ranges of subsequences in the sequence X similar to the query sequence Y using Dynamic Time Warping (DTW).
 * The implemented algorithm, NSPRING, is much faster and more memory efficient than a naive search and was first
 * presented in the following paper.
 *
 * X. Gong, S. Fong, J. H. Chan, and S. Mohammed. NSPRING: The spring extension for subsequence matching of time series
 * supporting normalization. J. Supercomput., 72(10):3801-3825, Oct. 2016.
 *
 * It is based on the algorithm SPRING (reference below), but additionally supports data normalization.
 *
 * Sakurai, Y., Faloutsos, C., Yamamuro, M.: Stream monitoring under the time warping distance. In: Proceedings of IEEE
 * 23rd International Conference on Data Engineering (ICDE 2007), Istanbul, Turkey, April 15-20, pp. 1046–1055 (2007).
 *
 * @param X The data sequence X.
 * @param Y The query sequence Y.
 * @param epsilon The similarity threshold for a sub-sequence to accept.
 * @return The list of sub-sequence matches.
 */
std::vector<SpringMatch> nspring(const std::vector<float>& X, const std::vector<float>& Y, float epsilon);

#endif //MET_3D_SPRING_H
