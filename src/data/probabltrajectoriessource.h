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
#ifndef PROBSURFACETRAJECTORIESSOURCE_H
#define PROBSURFACETRAJECTORIESSOURCE_H

// standard library imports

// related third party imports
#include <QtCore>
#include <gsl/gsl_interp.h>

// local application imports
#include "weatherpredictiondatasource.h"
#include "trajectoryselectionsource.h"
#include "trajectorydatasource.h"
#include "structuredgrid.h"
#include "datarequest.h"

namespace Met3D
{

/**
  @brief Data source that computes probabilities from ABL-started, filtered
  trajectories (e.g. for probability of warm conveyor belt occurrence).
  */
class MProbABLTrajectoriesSource
        : public MWeatherPredictionDataSource
{
public:
    MProbABLTrajectoriesSource();
    ~MProbABLTrajectoriesSource();

    MStructuredGrid* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

    void setTrajectorySource(MTrajectoryDataSource* s);

    void setInputSelectionSource(MTrajectorySelectionSource* s);

    QList<MVerticalLevelType> availableLevelTypes();

    QStringList availableVariables(MVerticalLevelType levelType);

    QSet<unsigned int> availableEnsembleMembers(MVerticalLevelType levelType,
                                                const QString& variableName);

    QList<QDateTime> availableInitTimes(MVerticalLevelType levelType,
                                        const QString& variableName);

    QList<QDateTime> availableValidTimes(MVerticalLevelType levelType,
                                         const QString& variableName,
                                         const QDateTime& initTime);

protected:
    const QStringList locallyRequiredKeys();

    MTrajectoryDataSource* trajectorySource;
    MTrajectorySelectionSource* inputSelectionSource;

private:
    gsl_interp       *gslLUTAirParcelThickness;
    gsl_interp_accel *gslLUTAirParcelThicknessAcc;
    double           *pressureValuesLUT;
    double           *airParcelThicknessValuesLUT;

    // Experimental -- rather don't use...
    void initializeDeltaPressureLUT();
    void cleanUpDeltaPressureLUT();
    float airParcelThickness(float p);
};

} // namespace Met3D

#endif // PROBSURFACETRAJECTORIESSOURCE_H
