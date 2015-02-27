/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
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
#ifndef PROBABILITYREGIONDETECTOR_H
#define PROBABILITYREGIONDETECTOR_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "structuredgridensemblefilter.h"
#include "structuredgrid.h"
#include "datarequest.h"

namespace Met3D
{

/**
  @brief Implements the region contribution algorithm. Computes the
  "contribution volume" to colour probability isosurfaces according to
  the number of members that contributed.
  */
class MProbabilityRegionDetectorFilter
        : public MStructuredGridEnsembleFilter
{
public:
    MProbabilityRegionDetectorFilter();

    MStructuredGrid* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

protected:
    const QStringList locallyRequiredKeys();
};

} // namespace Met3D

#endif // PROBABILITYREGIONDETECTOR_H
