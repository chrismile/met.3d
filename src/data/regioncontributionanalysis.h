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
#ifndef REGIONCONTRIBUTIONANALYSIS_H
#define REGIONCONTRIBUTIONANALYSIS_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "datarequest.h"
#include "abstractanalysis.h"
#include "qcustomplot.h"

namespace Met3D
{


class MRegionContributionResult : public MAnalysisResult
{
public:
    MRegionContributionResult();

    unsigned int getMemorySize_kb();

    struct MemberInfo
    {
        QList<int> numFeatureGridpoints;
        QList<int> numOverlappingFeatureGridpoints;
        QList<double> featureVolume;
        QList<double> overlappingFeatureVolume;
    };

    QVector<MemberInfo> memberInfo;
    int maxMemberFeatures;
    int numProbabilityRegionGridpoints;
    double probabilityRegionVolume;

};


/**
  @brief Implements an analysis source that analyses the ensemble contribution
  to a selected probability region (= probability isosurface).
  */
class MRegionContributionAnalysis : public MAnalysisDataSource
{
public:
    MRegionContributionAnalysis();

    MAnalysisResult* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

protected:
    const QStringList locallyRequiredKeys();

private:
    MIndexedGridRegion extractProbabilityRegion(
            MIndex3D indexInRegion, MStructuredGrid *contrGrid);

    void singleMemberRegionGrowing(
            unsigned char member, MIndex3D startIndex,
            MStructuredGrid *probGrid, MStructuredGrid *visitationGrid,
            MIndexedGridRegion *memberRegion,
            MIndexedGridRegion *overlapRegion);

    double gridRegionVolume(
            MStructuredGrid *grid, MIndexedGridRegion *region);

};


/**
  @brief Control associated with @ref MRegionContributionAnalysis. Creates an
  @ref MRegionContributionAnalysis instance with the same data sources currently
  used by the attached actor's actor variables.
  */
class MRegionContributionAnalysisControl
        : public MAnalysisControl
{
public:
    MRegionContributionAnalysisControl(MNWPMultiVarActor *actor);
    ~MRegionContributionAnalysisControl();

    void displayResult(MAnalysisResult *result);

protected:
    MDataRequest prepareRequest(MDataRequest analysisRequest);

    MAnalysisDataSource* createAnalysisSource()
    { return new MRegionContributionAnalysis(); }

    void updateAnalysisSourceInputs();

    void plotHistogram(QCustomPlot* plot, MRegionContributionResult* cresult,
                       bool volume);

    QTextBrowser *resultsTextBrowser;
    QCustomPlot  *sizeHistogram;
    QCustomPlot  *volumeHistogram;
};


} // namespace Met3D

#endif // REGIONCONTRIBUTIONANALYSIS_H
