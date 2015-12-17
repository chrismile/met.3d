/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Michael Kern
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
******************************************************************************/

// requires volume_global_structs_utils.glsl
// requires volume_<datatype>_utils.glsl
// requires volume_sample_utils.glsl

uniform DataVolumeExtent dataExtentShV;


// sample shading data at given world position
float sampleShadingDataAtPos(in vec3 pos)
{
    // case PRESSURE_LEVEL_3D
    if (dataExtentShV.levelType == 0)
    {
        return samplePressureLevelVolumeAtPos(dataVolumeShV, dataExtentShV,
                                              pressureTableShV, pos);
    }
    // case HYBRID_SIGMA_PRESSURE_3D
    else if (dataExtentShV.levelType == 1)
    {
        return sampleHybridSigmaVolumeAtPos(dataVolumeShV, dataExtentShV,
                                            surfacePressureShV, hybridCoefficientsShV,
                                            pos);
    }

    return 0;
}


// sample shading data at given world position
float sampleShadingDataAtPos_maxNeighbour(in vec3 pos)
{
    // case PRESSURE_LEVEL_3D
    if (dataExtentShV.levelType == 0)
    {
        return samplePressureLevelVolumeAtPos_maxNeighbour(
                                        dataVolumeShV, dataExtentShV, pos);
    }
    // case HYBRID_SIGMA_PRESSURE_3D
    else if (dataExtentShV.levelType == 1)
    {
        return sampleHybridSigmaVolumeAtPos_maxNeighbour(
                                        dataVolumeShV, dataExtentShV,
                                        surfacePressureShV, hybridCoefficientsShV,
                                        pos);
    }

    return 0;
}
