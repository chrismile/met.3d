/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2022 Christoph Neuhauser
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
#ifndef MET_3D_MULTISAMPLING_HPP
#define MET_3D_MULTISAMPLING_HPP

// standard library imports

// related third party imports

// local application imports

namespace Met3D
{

/**
 * Query the numbers of multisample samples possible.
 * @param desiredSamples Maximum number of desired samples.
 * @return The number of multisamples (min(desiredSamples, maxSamples)).
 */
int getMaxSamplesGLImpl(int desiredSamples);

}


#endif //MET_3D_MULTISAMPLING_HPP
