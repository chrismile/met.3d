/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Bianca Tost
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
#ifndef SINGLEVARIABLEANALYSIS_H
#define SINGLEVARIABLEANALYSIS_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "abstractanalysis.h"


namespace Met3D
{

class MNWPActorVariable;


/**
 @brief Abstract base class for modules acting as a broker between a variable
 and an @ref MAnalysisDataSource().
 */
class MSingleVariableAnalysisControl
        : public MAnalysisControl
{
    Q_OBJECT

public:
    MSingleVariableAnalysisControl(MNWPActorVariable *variable);
    ~MSingleVariableAnalysisControl();

    /**
      Implement this method with a suitable way to display the result of
      the analysis.
     */
    virtual void displayResult(MAnalysisResult *result) = 0;

protected:
    /**
      Implement this method to create a full pipeline request for
      @ref MAnalysisDataSource::requestData().
     */
    virtual MDataRequest prepareRequest(MDataRequest analysisRequest) = 0;

    /**
      Implement this method to create a new instance of the @ref
      MAnalysisDataSource attached to this control.
     */
    virtual MAnalysisDataSource* createAnalysisSource() = 0;

    /**
      Implement this method to update the @ref MAnalysisDataSource s data
      inputs from the actor variables.
     */
    virtual void updateAnalysisSourceInputs() = 0;

    MNWPActorVariable *variable;
};


} // namespace Met3D

#endif // SINGLEVARIABLEANALYSIS_H
