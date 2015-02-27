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
#include "regioncontributionanalysis.h"

// standard library imports
#include <iostream>
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/metroutines.h"
#include "gxfw/nwpmultivaractor.h"
#include "actors/nwpvolumeraycasteractor.h"

using namespace std;

namespace Met3D
{

MRegionContributionResult::MRegionContributionResult()
    : MAnalysisResult(),
      maxMemberFeatures(0),
      numProbabilityRegionGridpoints(0)
{
}


unsigned int MRegionContributionResult::getMemorySize_kb()
{
    int memberInfoSize = 0;
    for (int i = 0; i < memberInfo.size(); i++)
        for (int f = 0; f < memberInfo[i].numFeatureGridpoints.size(); f++)
            memberInfoSize += memberInfo[i].numFeatureGridpoints.size()
                    * 2 * sizeof(int);

    return MAnalysisResult::getMemorySize_kb()
            + ( sizeof(MRegionContributionResult)
                + memberInfoSize
                ) / 1024.;
}


/******************************************************************************
***                     MValueExtractionAnalysis                            ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MRegionContributionAnalysis::MRegionContributionAnalysis()
    : MAnalysisDataSource()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MAnalysisResult* MRegionContributionAnalysis::produceData(MDataRequest request)
{
    MRegionContributionResult *result = new MRegionContributionResult();

    MDataRequestHelper rh(request);
    QVector3D pos_lonLatP = rh.vec3Value("POS_LONLATP");

    // Obtain pointers to input data sources: the source that provides the
    // probability volume, and the one that provides the contribution volume.
    MDataRequestHelper probRH = rh.subRequest("PROB_");
    MWeatherPredictionDataSource *probSource =
            static_cast<MWeatherPredictionDataSource*>(
                getPrefixedDataSource("PROB_"));

    MDataRequestHelper contrRH = rh.subRequest("CONTR_");
    MWeatherPredictionDataSource *contrSource =
            static_cast<MWeatherPredictionDataSource*>(
                getPrefixedDataSource("CONTR_"));

    // Error checks.
    if (probSource == nullptr)
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: data source for probability field "
                        "is unavailable. Aborting region contribution.");
        if (contrSource) contrSource->releaseData(contrRH.request());
        return result; // return empty result
    }
    if (contrSource == nullptr)
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: data source for contribution data "
                        "is unavailable. Aborting region contribution.");
        if (probSource) probSource->releaseData(probRH.request());
        return result; // return empty result
    }

    MStructuredGrid *probGrid = probSource->getData(probRH.request());
    MStructuredGrid *contrGrid = contrSource->getData(contrRH.request());

    // Determine an initial grid point that is inside the probability region.
    MIndex3D gridPointInRegion = contrGrid->maxNeighbouringGridPoint(pos_lonLatP);

    // Extract a list of all points belonging to the selected feature.
    MIndexedGridRegion probRegion = extractProbabilityRegion(
                gridPointInRegion, contrGrid);

    result->memberInfo.resize(probGrid->getMaxAvailableMember()+1);

    result->textResult.append(
                QString("probability region at (%1/%2/%3), index %4:")
                .arg(pos_lonLatP.x()).arg(pos_lonLatP.y()).arg(pos_lonLatP.z())
                .arg(gridPointInRegion.toString()));

    result->textResult.append(
                QString("  probability surface hit at %1")
                .arg(probGrid->interpolateValue(pos_lonLatP)));

    result->textResult.append(
                QString("  %1\% of the ensemble members contribute to this "
                        "region of %2 grid points")
                .arg(contrGrid->getValue(gridPointInRegion) * 100.)
                .arg(probRegion.size()));

    result->numProbabilityRegionGridpoints = probRegion.size();
    result->probabilityRegionVolume = gridRegionVolume(probGrid, &probRegion);

    // Store visitation flags for region growing below.
    MStructuredGrid visitationGrid(probGrid->getLevelType(),
                                   probGrid->getNumLevels(),
                                   probGrid->getNumLats(),
                                   probGrid->getNumLons());
    visitationGrid.setToZero();
    visitationGrid.enableFlags();

    // Store the probability region in the visitation grid; used in
    // singleMemberRegionGrowing().
    for (int i = 0; i < probRegion.size(); i++)
        visitationGrid.setValue(probRegion[i], 1.);

    // For each member:
    for (unsigned char m = 0; m < result->memberInfo.size(); m++)
    {
        // Check if there is any contribution of member m to the probability
        // volume at all.
        if ( ! (probGrid->memberIsContributing(m)) ) continue;

        QList<MIndexedGridRegion> memberFeatures;
        QList<MIndexedGridRegion> memberFeaturesOverlap;

        for (int i = 0; i < probRegion.size(); i++)
        {
            MIndex3D idx = probRegion[i];

            // Is the current index already part of a detected structure?
            if (visitationGrid.getFlag(idx, m)) continue;

            // Did member m contribute to the probability value at index idx?
            if (probGrid->getFlag(idx, m))
            {
                // Yes: Start a region growing to detect the region of adjacent
                // grid points of member m that fulfil the probability
                // criterion.

                MIndexedGridRegion region;
                MIndexedGridRegion overlapRegion;

                singleMemberRegionGrowing(m, idx, probGrid, &visitationGrid,
                                          &region, &overlapRegion);

                // Find insertion index so that region lists remain sorted with
                // respect to size.
                int insertionIndex = 0;
                while (insertionIndex < memberFeatures.size())
                {
                    if (memberFeatures[insertionIndex].size() < region.size())
                        break;
                    insertionIndex++;
                }

                memberFeatures.insert(insertionIndex, region);
                memberFeaturesOverlap.insert(insertionIndex, overlapRegion);
                result->memberInfo[m].numFeatureGridpoints.insert(
                            insertionIndex, region.size());
                result->memberInfo[m].numOverlappingFeatureGridpoints.insert(
                            insertionIndex, overlapRegion.size());

                double volume_m3 = gridRegionVolume(probGrid, &region);
                double overlapVolume_m3 = gridRegionVolume(probGrid, &overlapRegion);

                result->memberInfo[m].featureVolume.insert(
                            insertionIndex, volume_m3);
                result->memberInfo[m].overlappingFeatureVolume.insert(
                            insertionIndex, overlapVolume_m3);

            }
        }

        result->textResult.append(
                    QString("\n  member %1 contributes with %2 disjunct features")
                    .arg(m).arg(memberFeatures.size()));

        result->maxMemberFeatures = max(result->maxMemberFeatures,
                                        memberFeatures.size());

        for (int i = 0; i < memberFeatures.size(); i++)
        {
            result->textResult.append(
                        QString("    feature %1: %2 grid points (%3 points, "
                                "i.e. %4\%, overlap with probability region)")
                        .arg(i).arg(memberFeatures[i].size())
                        .arg(memberFeaturesOverlap[i].size())
                        .arg(float(memberFeaturesOverlap[i].size()) /
                             memberFeatures[i].size() * 100.));

            double volume_m3 = result->memberInfo[m].featureVolume[i];
            double overlapVolume_m3 = result->memberInfo[m].overlappingFeatureVolume[i];

            result->textResult.append(
                        QString("             : %1 km^3 (%2 km^3, "
                                "i.e. %3\%, overlap with probability region)")
                        .arg(volume_m3 / 1.e9)
                        .arg(overlapVolume_m3 / 1.e9)
                        .arg(float(overlapVolume_m3) /
                             volume_m3 * 100.));
        }
    } // member m

    probSource->releaseData(probGrid);
    contrSource->releaseData(contrGrid);

    return result;
}


MTask* MRegionContributionAnalysis::createTaskGraph(MDataRequest request)
{
    MTask *task = new MTask(request, this);

    // Get task graphs for prob. and reg. contr. requests.
    MDataRequestHelper rh(request);
    MWeatherPredictionDataSource *source;

    MDataRequestHelper probRH = rh.subRequest("PROB_");
    source = static_cast<MWeatherPredictionDataSource*>(
                getPrefixedDataSource("PROB_"));
    task->addParent(source->getTaskGraph(probRH.request()));

    MDataRequestHelper contrRH = rh.subRequest("CONTR_");
    source = static_cast<MWeatherPredictionDataSource*>(
                getPrefixedDataSource("CONTR_"));
    task->addParent(source->getTaskGraph(contrRH.request()));

    // Create request for geop. height field.

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MRegionContributionAnalysis::locallyRequiredKeys()
{
    return QStringList() << "POS_LONLATP";
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

MIndexedGridRegion MRegionContributionAnalysis::extractProbabilityRegion(
        MIndex3D indexInRegion, MStructuredGrid *contrGrid)
{
    // Stores the probability region.
    MIndexedGridRegion region;

    // Grid to store which indices have been visited.
    MStructuredGrid visitationGrid(contrGrid->getLevelType(),
                                   contrGrid->getNumLevels(),
                                   contrGrid->getNumLats(),
                                   contrGrid->getNumLons());
    visitationGrid.setToZero();

    // Add initial point to prob. region and mark the point as visited.
    region.append(indexInRegion);
    visitationGrid.setValue(indexInRegion, 1);

    int currentIndex = 0;

    // Region growing.
    while (currentIndex < region.size())
    {
        MIndex3D i3 = region.at(currentIndex++);

        // Check if adjacent grid points belong to the region.
        // Add those that do to the queue.
        for (int ok = -1; ok <= 1; ok++)
            for (int oj = -1; oj <= 1; oj++)
                for (int oi = -1; oi <= 1; oi++)
                {
                    MIndex3D oidx(i3.k+ok, i3.j+oj, i3.i+oi);
                    if (oidx.k < 0) continue;
                    if (oidx.k >= int(contrGrid->getNumLevels())) continue;
                    if (oidx.j < 0) continue;
                    if (oidx.j >= int(contrGrid->getNumLats())) continue;
                    if (oidx.i < 0) continue;
                    if (oidx.i >= int(contrGrid->getNumLons())) continue;

                    // Has the adjacent grid point already been visited?
                    if (visitationGrid.getValue(oidx) > 0.) continue;

                    // Continue if the grid point does not belong to the region.
                    if (contrGrid->getValue(oidx) == 0.) continue;

                    region.append(oidx);
                    visitationGrid.setValue(oidx, 1.);
                }
    }

    return region;
}


void MRegionContributionAnalysis::singleMemberRegionGrowing(
        unsigned char member, MIndex3D startIndex,
        MStructuredGrid *probGrid, MStructuredGrid *visitationGrid,
        MIndexedGridRegion *memberRegion, MIndexedGridRegion *overlapRegion)
{
    memberRegion->clear();
    overlapRegion->clear();

    // Add initial point to prob. region and mark the point as visited.
    memberRegion->append(startIndex);
    visitationGrid->setFlag(startIndex, member);

    int currentIndex = 0;

    // Region growing.
    while (currentIndex < memberRegion->size())
    {
        MIndex3D i3 = memberRegion->at(currentIndex++);

        // Check if adjacent grid points belong to the memberRegion->
        // Add those that do to the queue.
        for (int ok = -1; ok <= 1; ok++)
            for (int oj = -1; oj <= 1; oj++)
                for (int oi = -1; oi <= 1; oi++)
                {
                    MIndex3D oidx(i3.k+ok, i3.j+oj, i3.i+oi);
                    if (oidx.k < 0) continue;
                    if (oidx.k >= int(probGrid->getNumLevels())) continue;
                    if (oidx.j < 0) continue;
                    if (oidx.j >= int(probGrid->getNumLats())) continue;
                    if (oidx.i < 0) continue;
                    if (oidx.i >= int(probGrid->getNumLons())) continue;

                    // Has the adjacent index already been visited?
                    if (visitationGrid->getFlag(oidx, member)) continue;

                    // If the adjacent index fulfils the probability
                    // criterion, it belongs to the memberRegion->
                    if (probGrid->getFlag(oidx, member))
                    {
                        memberRegion->append(oidx);
                        visitationGrid->setFlag(oidx, member);

                        // Does this index overlap with the probability region?
                        if (visitationGrid->getValue(oidx) > 0.)
                            overlapRegion->append(oidx);
                    }
                }
    }
}


double MRegionContributionAnalysis::gridRegionVolume(
        MStructuredGrid *grid, MIndexedGridRegion *region)
{
    double volume_m3 = 0.;

    for (int i = 0; i < region->size(); i++)
    {
        MIndex3D idx = region->at(i);

        double boxVolume = boxVolume_dry(
                    grid->getWestInterfaceLon(idx.i),
                    grid->getNorthInterfaceLat(idx.j),
                    grid->getEastInterfaceLon(idx.i),
                    grid->getSouthInterfaceLat(idx.j),
                    grid->getPressure(idx.k, idx.j, idx.i) * 100.,
                    grid->getBottomInterfacePressure(idx.k, idx.j, idx.i) * 100.,
                    grid->getTopInterfacePressure(idx.k, idx.j, idx.i) * 100.);

        volume_m3 += boxVolume;

        /*
        QString s = QString("boxVol=%1, box=%2,%3/%4,%5/%6-%7-%8")
                .arg(boxVolume)
                .arg(grid->getWestInterfaceLon(idx.i))
                .arg(grid->getNorthInterfaceLat(idx.j))
                .arg(grid->getEastInterfaceLon(idx.i))
                .arg(grid->getSouthInterfaceLat(idx.j))
                .arg(grid->getPressure(idx.k, idx.j, idx.i))
                .arg(grid->getBottomInterfacePressure(idx.k, idx.j, idx.i))
                .arg(grid->getTopInterfacePressure(idx.k, idx.j, idx.i));
        LOG4CPLUS_DEBUG(mlog, s.toStdString());
        */
    }

    return volume_m3;
}


/******************************************************************************
***                  MValueExtractionAnalysisControl                        ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MRegionContributionAnalysisControl::MRegionContributionAnalysisControl(
        MNWPMultiVarActor *actor)
    : MAnalysisControl(actor)
{
    QSplitter *splitter = new QSplitter(Qt::Vertical);

    sizeHistogram = new QCustomPlot();
    splitter->addWidget(sizeHistogram);

    volumeHistogram = new QCustomPlot();
    splitter->addWidget(volumeHistogram);

    resultsTextBrowser = new QTextBrowser();
    resultsTextBrowser->setLineWrapMode(QTextBrowser::NoWrap);
    splitter->addWidget(resultsTextBrowser);

    setDisplayWidget(splitter);
    setDisplayTitle("Region Contribution Analysis");
}


MRegionContributionAnalysisControl::~MRegionContributionAnalysisControl()
{
    delete sizeHistogram;
    delete volumeHistogram;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MRegionContributionAnalysisControl::displayResult(MAnalysisResult *result)
{
    //LOG4CPLUS_DEBUG(mlog, "result of probability region contribution analysis:");

    QString s;
    for (int i = 0; i < result->textResult.size(); i++)
    {
        //LOG4CPLUS_DEBUG(mlog, result->textResult[i].toStdString());
        s += result->textResult[i] + "\n";
    }
    resultsTextBrowser->setPlainText(s);

    MRegionContributionResult* cresult =
            dynamic_cast<MRegionContributionResult*>(result);
    if (!cresult) return;

    plotHistogram(sizeHistogram, cresult, false);
    plotHistogram(volumeHistogram, cresult, true);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

MDataRequest MRegionContributionAnalysisControl::prepareRequest(
        MDataRequest analysisRequest)
{
    MDataRequestHelper rh(analysisRequest);

    MNWPVolumeRaycasterActor *volActor =
            dynamic_cast<MNWPVolumeRaycasterActor*>(actor);

    if (volActor)
    {
        // Get pointers to probability data source and region contribution data
        // source. How do I do this if the actor is not a volume actor?
        const MNWPActorVariable* probVar  = volActor->getCurrentRenderVariable();
        const MNWPActorVariable* contrVar = volActor->getCurrentShadingVariable();

        // Prefix both requests with "PROB_" and "CONTR_", respectively.
        MDataRequestHelper probRH(probVar->grid->getGeneratingRequest());
        probRH.addKeyPrefix("PROB_");
        rh.unite(probRH);

        MDataRequestHelper contrRH(contrVar->grid->getGeneratingRequest());
        contrRH.addKeyPrefix("CONTR_");
        rh.unite(contrRH);
    }

    return rh.request();
}


void MRegionContributionAnalysisControl::updateAnalysisSourceInputs()
{
    analysisSource->clearDataSources();

    // Make sure correct prob. and reg. contr. data sources are connected.
    MNWPVolumeRaycasterActor *volActor =
            dynamic_cast<MNWPVolumeRaycasterActor*>(actor);

    if (volActor)
    {
        analysisSource->addDataSource(
                    "PROB_", volActor->getCurrentRenderVariable()->dataSource);
        analysisSource->addDataSource(
                    "CONTR_", volActor->getCurrentShadingVariable()->dataSource);
    }
}


void MRegionContributionAnalysisControl::plotHistogram(
        QCustomPlot *plot, MRegionContributionResult *cresult, bool volume)
{
    plot->clearPlottables();

    // Colours used for the bars that display the features. The colours
    // are used in alternating order.
    QList<QColor> featureColours;
//    featureColours << QColor(255, 131, 0) << QColor(1, 92, 191)
//                   << QColor(150, 222, 0);
    featureColours << QColor(255, 131, 0) << QColor(10, 10, 10)
                   << QColor(150, 222, 0);

    // A pen for the lines that outline the bars.
    // NOTE: The pen is set invisible here. Otherwise, bars that have
    // zero size will still be drawn as a line (with no fill), which
    // distorts the plot.
    QPen linePen;
    linePen.setWidthF(1.2);
    linePen.setColor(QColor(0,0,0,0));

    // x axis with member labels.
    QVector<double> xticks;
    for (int m = 0; m < cresult->memberInfo.size(); m++) xticks << m;

    QList<QCPBars*> featureBars;
    QList<QCPBars*> featureOverlapBars;
    for (int i = 0; i < cresult->maxMemberFeatures; i++)
    {
        // Crate bars that represent the features' size.
        QCPBars *bars = new QCPBars(plot->xAxis, plot->yAxis);
        featureBars.append(bars);
        plot->addPlottable(bars);

        // Create bars that represent those parts of the features that
        // overlap with the probability region.
        QCPBars *overlapBars = new QCPBars(plot->xAxis, plot->yAxis);
        featureOverlapBars.append(overlapBars);
        plot->addPlottable(overlapBars);

        // Set the colour of the bars; overlapping bars are rendered with
        // full opacity; for the full feature size alpha is set to 120.
        QColor barColour = featureColours[i % featureColours.size()];
        overlapBars->setPen(linePen);
        overlapBars->setBrush(barColour);
        barColour.setAlpha(150);
        bars->setPen(linePen);
        bars->setBrush(barColour);

        // If this is not the first feature move the bars on top of the
        // already existing ones.
        if (i > 0)
        {
            bars->moveAbove(featureBars[i-1]);
            overlapBars->moveAbove(featureBars[i-1]);
        }

        // Get data for feature size and overlapping size.
        QVector<double> data(cresult->memberInfo.size());
        QVector<double> odata(cresult->memberInfo.size());
        for (int m = 0; m < cresult->memberInfo.size(); m++)
        {
            if (volume)
            {
                if (i < cresult->memberInfo[m].featureVolume.size())
                    data[m] = cresult->memberInfo[m].featureVolume[i] / 1.e12;
                else
                    data[m] = 0.;

                if (i < cresult->memberInfo[m].overlappingFeatureVolume.size())
                    odata[m] = cresult->memberInfo[m].overlappingFeatureVolume[i] / 1.e12;
                else
                    odata[m] = 0.;
            }
            else
            {
                if (i < cresult->memberInfo[m].numFeatureGridpoints.size())
                    data[m] = cresult->memberInfo[m].numFeatureGridpoints[i];
                else
                    data[m] = 0.;

                if (i < cresult->memberInfo[m].numOverlappingFeatureGridpoints.size())
                    odata[m] = cresult->memberInfo[m].numOverlappingFeatureGridpoints[i];
                else
                    odata[m] = 0.;
            }
        }

        bars->setData(xticks, data);
        overlapBars->setData(xticks, odata);
    }

    // Configure x/y axes.
    plot->xAxis->grid()->setVisible(true);
    plot->xAxis->setLabel("ensemble member");
    if (volume)
        plot->yAxis->setLabel("feature volume (10^3 * km^3)");
    else
        plot->yAxis->setLabel("feature size (grid cells)");
    plot->yAxis->grid()->setSubGridVisible(true);
    QPen gridPen;
    gridPen.setStyle(Qt::SolidLine);
    gridPen.setColor(QColor(0, 0, 0, 25));
    plot->yAxis->grid()->setPen(gridPen);
    gridPen.setStyle(Qt::DotLine);
    plot->yAxis->grid()->setSubGridPen(gridPen);
    plot->rescaleAxes();

    // Draw horizontal line that shows the size of the probability region.
    QVector<double> x(2), y(2);
    x[0] = 0.; x[1] = cresult->memberInfo.size();
    if (volume)
        y[0] = y[1] = cresult->probabilityRegionVolume / 1.e12;
    else
        y[0] = y[1] = cresult->numProbabilityRegionGridpoints;

    plot->addGraph();
    plot->graph(0)->setData(x, y);
    linePen.setColor(Qt::red);
    linePen.setWidthF(3.);
    plot->graph(0)->setPen(linePen);

    plot->setInteractions(
                QCP::iRangeDrag | QCP::iRangeZoom);
    plot->axisRect()->setRangeDrag(Qt::Vertical);
    plot->axisRect()->setRangeZoom(Qt::Vertical);
    plot->replot();
}


} // namespace Met3D
