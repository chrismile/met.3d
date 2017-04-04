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
#ifndef VALUEEXTRACTIONANALYSIS_H
#define VALUEEXTRACTIONANALYSIS_H

// standard library imports

// related third party imports
#include <QtCore>
#include <QTextBrowser>

// local application imports
#include "datarequest.h"
#include "abstractanalysis.h"

namespace Met3D
{

/**
  @brief Implements an analysis source that extracts the data values of all
  connected data sources at a specified position.
  */
class MValueExtractionAnalysis
        : public MAnalysisDataSource
{
public:
    MValueExtractionAnalysis();

    MAnalysisResult* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

protected:
    const QStringList locallyRequiredKeys();

};


/**
  @brief Control associated with @ref MValueExtractionAnalysis. Creates an
  @ref MValueExtractionAnalysis instance with the same data sources currently
  used by the attached actor's actor variables.
  */
class MValueExtractionAnalysisControl
        : public MAnalysisControl
{
public:
    MValueExtractionAnalysisControl(MNWPMultiVarActor *actor);
    ~MValueExtractionAnalysisControl();

    void displayResult(MAnalysisResult *result);

protected:
    MDataRequest prepareRequest(MDataRequest analysisRequest);

    MAnalysisDataSource* createAnalysisSource()
    { return new MValueExtractionAnalysis(); }

    void updateAnalysisSourceInputs();

    QTextBrowser *resultsTextBrowser;
};


} // namespace Met3D

#endif // VALUEEXTRACTIONANALYSIS_H
