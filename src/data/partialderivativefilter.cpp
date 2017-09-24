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
#include "partialderivativefilter.h"

// standard library imports
#include <iostream>
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MPartialDerivativeFilter::MPartialDerivativeFilter()
    : MStructuredGridEnsembleFilter()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MStructuredGrid* MPartialDerivativeFilter::produceData(MDataRequest request)
{
    assert(inputSource != nullptr);

    MDataRequestHelper rh(request);
    std::cout << request.toStdString() << std::endl << std::flush;

    // Parse request.
    // Examples: DERIVATIVE=D/DLON, =D2/DLAT2
    QString derivative = rh.value("DERIVATIVE");
    QStringList params = derivative.split("/");
    rh.removeAll(locallyRequiredKeys());

    MLonLatHybridSigmaPressureGrid *inputGrid =
            dynamic_cast<MLonLatHybridSigmaPressureGrid*>(
                inputSource->getData(rh.request()));

    if (!inputGrid) return nullptr;

    // Create new grid with the same grid topology as the input grid.
    MStructuredGrid* result = createAndInitializeResultGrid(inputGrid);

    if (params[0] == "D")
    {
        // Compute the first partial derivative.
        // =====================================

        // Loop over all grid points: Compute first partial derivative
        // ith central differences.

        if (params[1] == "DLON")
        {
            for (unsigned int k = 0; k < result->getNumLevels(); k++)
            {
                for (unsigned int j = 0; j < result->getNumLats(); j++)
                {
                    // Forward/backward differences for boundary grid points.
                    float dinput_dlon = (inputGrid->getValue(k, j, 1) - inputGrid->getValue(k, j, 0)) /
                            (inputGrid->getLons()[1] - inputGrid->getLons()[0]);
                    result->setValue(k, j, 0, dinput_dlon);

                    unsigned int nlons = result->getNumLons();
                    dinput_dlon = (inputGrid->getValue(k, j, nlons - 1) - inputGrid->getValue(k, j, nlons - 2)) /
                            (inputGrid->getLons()[nlons - 1] - inputGrid->getLons()[nlons - 2]);
                    result->setValue(k, j, nlons - 1, dinput_dlon);

                    // Central differences for all other grid points.
                    for (unsigned int i = 1; i < nlons - 1; i++)
                    {
                        dinput_dlon = (inputGrid->getValue(k, j, i + 1) - inputGrid->getValue(k, j, i - 1)) /
                                (inputGrid->getLons()[i + 1] - inputGrid->getLons()[i - 1]);
                        result->setValue(k, j, i, dinput_dlon);
                    }
                }
            }
        }

        else if (params[1] == "DLAT")
        {
            for (unsigned int k = 0; k < result->getNumLevels(); k++)
            {
                for (unsigned int i = 0; i < result->getNumLons(); i++)
                {
                    // Forward/backward differences for boundary grid points.
                    float dinput_dlat = (inputGrid->getValue(k, 1, i) - inputGrid->getValue(k, 0, i)) /
                            (inputGrid->getLats()[1] - inputGrid->getLats()[0]);
                    result->setValue(k, 0, i, dinput_dlat);

                    unsigned int nlats = result->getNumLats();
                    dinput_dlat = (inputGrid->getValue(k, nlats - 1, i) - inputGrid->getValue(k, nlats - 2, i)) /
                            (inputGrid->getLats()[nlats - 1] - inputGrid->getLats()[nlats - 2]);
                    result->setValue(k, nlats - 1, i, dinput_dlat);

                    // Central differences for all other grid points.
                    for (unsigned int j = 1; j < nlats - 1; j++)
                    {
                        dinput_dlat = (inputGrid->getValue(k, j + 1, i) - inputGrid->getValue(k, j - 1, i)) /
                                (inputGrid->getLats()[j + 1] - inputGrid->getLats()[j - 1]);
                        result->setValue(k, j, i, dinput_dlat);
                    }
                }
            }
        }

        else if (params[1] == "DP")
        {
            for (unsigned int j = 0; j < result->getNumLats(); j++)
            {
                for (unsigned int i = 0; i < result->getNumLons(); i++)
                {
                    // Forward/backward differences for boundary grid points.
                    float dinput_dp = (inputGrid->getValue(1, j, i) - inputGrid->getValue(0, j, i)) /
                            (inputGrid->getPressure(1, j, i) - inputGrid->getPressure(0, j, i));
                    result->setValue(0, j, i, dinput_dp);

                    unsigned int nlevs = result->getNumLevels();
                    dinput_dp = (inputGrid->getValue(nlevs - 1, j, i) - inputGrid->getValue(nlevs - 2, j, i)) /
                            (inputGrid->getPressure(nlevs - 1, j, i) - inputGrid->getPressure(nlevs - 2, j, i));
                    result->setValue(nlevs - 1, j, i, dinput_dp);

                    // Central differences for all other grid points.
                    for (unsigned int k = 1; k < nlevs - 1; k++)
                    {
                        dinput_dp = (inputGrid->getValue(k + 1, j, i) - inputGrid->getValue(k - 1, j, i)) /
                                (inputGrid->getPressure(k + 1, j, i) - inputGrid->getPressure(k - 1, j, i));
                        result->setValue(k, j, i, dinput_dp);
                    }
                }
            }
        }
    }

    else if (params[0] == "D2")
    {
        // Compute the second partial derivative.
        // ======================================

        if (params[1] == "DLON2")
        {
            for (unsigned int k = 0; k < result->getNumLevels(); k++)
                for (unsigned int j = 0; j < result->getNumLats(); j++)
                {
                    // Central differences for all grid points except boundary
                    // grid points.
                    for (unsigned int i = 1; i < result->getNumLons()-1; i++)
                    {
                        float dlon = (inputGrid->getLons()[i+1] - inputGrid->getLons()[i]);
                        float dinput2_dlon2 = (inputGrid->getValue(k, j, i+1) - 2. * inputGrid->getValue(k, j, i)
                                 + inputGrid->getValue(k, j, i-1)) / (dlon*dlon);
                        result->setValue(k, j, i, dinput2_dlon2);
                    }

                    // Boundary conditions:
//TODO (mr, 23Nov2014) -- which bc should we use?
                    result->setValue(k, j, 0,
                                     result->getValue(k, j, 1));
                    int nlons = result->getNumLons();
                    result->setValue(k, j, nlons-1,
                                     result->getValue(k, j, nlons-2));
                }
        }

        else if (params[1] == "DLAT2")
        {
            for (unsigned int k = 0; k < result->getNumLevels(); k++)
                for (unsigned int i = 0; i < result->getNumLons(); i++)
                {
                    // Central differences for all grid points except boundary
                    // grid points.
                    for (unsigned int j = 1; j < result->getNumLats()-1; j++)
                    {
                        float dlat = (inputGrid->getLats()[j+1]
                                - inputGrid->getLats()[j]);
                        float dinput2_dlat2 = (inputGrid->getValue(k, j+1, i) - 2. * inputGrid->getValue(k, j, i)
                                 + inputGrid->getValue(k, j-1, i)) / (dlat*dlat);
                        result->setValue(k, j, i, dinput2_dlat2);
                    }

                    // Boundary conditions:
//TODO (mr, 23Nov2014) -- which bc should we use?
                    result->setValue(k, 0, i,
                                     result->getValue(k, 1, i));
                    int nlats = result->getNumLats();
                    result->setValue(k, nlats-1, i,
                                     result->getValue(k, nlats-2, i));
                }
        }

        else if (params[1] == "DP2")
        {
            for (unsigned int j = 0; j < result->getNumLats(); j++)
                for (unsigned int i = 0; i < result->getNumLons(); i++)
                {
                    // Central differences for all grid points except boundary
                    // grid points.
                    for (unsigned int k = 1; k < result->getNumLevels()-1; k++)
                    {
                        float dp = (inputGrid->getPressure(k+1, j, i)
                                - inputGrid->getPressure(k, j, i));
                        float dinput2_dp2 = (inputGrid->getValue(k+1, j, i) - 2. * inputGrid->getValue(k, j, i)
                                 + inputGrid->getValue(k-1, j, i)) / (dp*dp);
                        result->setValue(k, j, i, dinput2_dp2);
                    }

                    // Boundary conditions:
//TODO (mr, 23Nov2014) -- which bc should we use?
                    result->setValue(0, j, i,
                                     result->getValue(1, j, i));
                    int nlevs = result->getNumLevels();
                    result->setValue(nlevs-1, j, i,
                                     result->getValue(nlevs-2, j, i));
                }
        }

        else if (params[1] == "DLONLAT")
        {
            // http://www.holoborodko.com/pavel/numerical-methods/numerical-derivative/central-differences/#central_differences
        }
    }

    else
    {
        result->setToZero();
    }

    inputSource->releaseData(inputGrid);

    return result;
}


MTask* MPartialDerivativeFilter::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    rh.removeAll(locallyRequiredKeys());
    task->addParent(inputSource->getTaskGraph(rh.request()));

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MPartialDerivativeFilter::locallyRequiredKeys()
{
    return (QStringList() << "DERIVATIVE");
}

} // namespace Met3D
