/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Michael Kern
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
#ifndef MULTIVARPARTIALDERIVATIVEFILTER_H
#define MULTIVARPARTIALDERIVATIVEFILTER_H

// standard library imports

// related third party imports

// local application imports
#include "structuredgridensemblefilter.h"
#include "structuredgrid.h"
#include "datarequest.h"


namespace Met3D
{
/**
 * Represents the family of filters that requires more than one variable to
 * generate a result.
 */
class MMultiVarFilter : public MStructuredGridEnsembleFilter
{
public:
    explicit MMultiVarFilter();

    virtual MTask* createTaskGraph(MDataRequest request) override;

protected:
    const QStringList locallyRequiredKeys() override;
};


/**
 * Partially derives a variable by using the information of at least two other
 * variables. In example, derive each wind-component by the normal to the local
 * wind vector (u,v).
 */
class MMultiVarPartialDerivativeFilter : public MMultiVarFilter
{
public:
    explicit MMultiVarPartialDerivativeFilter();

    /**
     * Computes the partial derivatives of a variable based on the
     * user-specified request (d/dn, d/dz, d/dp, d²/dn², ...).
     */
    MStructuredGrid* produceData(MDataRequest request) override;
    MTask* createTaskGraph(MDataRequest request) override;

    void setGeoPotInputSource(MWeatherPredictionDataSource* s);

protected:
    const QStringList locallyRequiredKeys() override;
    /**
     * Lambda function to compute the velocity projected onto vector s at a
     * grid point.
     *
     * Computes the velocity projected onto the local wind vector direction s.
     */
    inline double computeVs(MStructuredGrid* gridU, MStructuredGrid* gridV,
                            int k, int j, int i, const QVector2D& s) const;

    MWeatherPredictionDataSource* geoPotSource;
};

class MBlurFilter : public MStructuredGridEnsembleFilter
{
public:
    explicit MBlurFilter();

    MStructuredGrid* produceData(MDataRequest request) override;
    MTask* createTaskGraph(MDataRequest request) override;

protected:
    const QStringList locallyRequiredKeys() override;
};


} // namespace Met3D

#endif //MULTIVARPARTIALDERIVATIVEFILTER_H
