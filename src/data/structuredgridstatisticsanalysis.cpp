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
#include "structuredgridstatisticsanalysis.h"

// standard library imports
#include <iostream>
#include "assert.h"
#include <limits>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "gxfw/msystemcontrol.h"
#include "mainwindow.h"
#include "gxfw/nwpmultivaractor.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                       MAnalysisDataSource                               ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MStructuredGridStatisticsResult::
MStructuredGridStatisticsResult()
    : MAnalysisResult(),
      minValue(std::numeric_limits<double>::max()),
      maxValue(-std::numeric_limits<double>::max()),
      meanValue(0.f)
{
    histogramData.clear();
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

unsigned int MStructuredGridStatisticsResult::getMemorySize_kb()
{
    int histogramDataSize =
            histogramData.size() * (sizeof(double) + sizeof(double));

    return MAnalysisResult::getMemorySize_kb()
            + ( sizeof(MStructuredGridStatisticsResult)
                + histogramDataSize
                ) / 1024.;
}


/******************************************************************************
***                    MStructuredGridStatisticsAnalysis                    ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MStructuredGridStatisticsAnalysis::MStructuredGridStatisticsAnalysis()
    : MAnalysisDataSource()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MAnalysisResult* MStructuredGridStatisticsAnalysis::produceData(
        MDataRequest request)
{
    MStructuredGridStatisticsResult *result =
            new MStructuredGridStatisticsResult();

    MDataRequestHelper rh(request);
    double significantDigits = rh.doubleValue("HISTOGRAM_SIGNIFICANT_DIGITS");
    double histogramAccuracyAdjustValue = pow(10., significantDigits);

    int histogramDisplayMode = rh.intValue("HISTOGRAM_DISPLAYMODE");

    // Obtain pointers to input data source.
    MDataRequestHelper varRH = rh.subRequest("VAR_DATA_");
    MWeatherPredictionDataSource *source =
            static_cast<MWeatherPredictionDataSource*>(
                getPrefixedDataSource("VAR_DATA_"));

    // Error checks.
    if (source == nullptr)
    {
        LOG4CPLUS_ERROR(mlog,
                        "ERROR: Request references unavailable datasource.");
        return result; // Return empty result.
    }

    MStructuredGrid *grid = source->getData(varRH.request());

    // Loop over all grid points and compute statistics.
    for (unsigned int i = 0; i < grid->getNumValues(); i++)
    {
        double value = double(grid->getValue(i));
        result->minValue = min(value, result->minValue);
        result->maxValue = max(value, result->maxValue);
        result->meanValue = result->meanValue
                + (1. / static_cast<double>(i + 1)) * (value - result->meanValue);
        value = round(value * histogramAccuracyAdjustValue);
        value = value / histogramAccuracyAdjustValue;
        result->histogramData.insert(
                    value,
                    result->histogramData.value(value, 0) + 1.);
    }

    double numGridValues = double(grid->getNumValues());

    if (histogramDisplayMode ==
            MStructuredGridStatisticsAnalysisControl::RELATIVE_FREQUENCY_DISTRIBUTION)
    {
        foreach (double value, result->histogramData.keys())
        {
            // Replace count value by percentage value.
            result->histogramData.insert(
                        value,
                        (result->histogramData.value(value, 0) / numGridValues)
                        * 100.);
        }
    }
    result->histogramAccuracyAdjustValue = histogramAccuracyAdjustValue;
    result->histogramDisplayMode = histogramDisplayMode;

    result->textResult.append(
                QString("Min: %1").arg(result->minValue));

    result->textResult.append(
                QString("Max: %1").arg(result->maxValue));

    result->textResult.append(
                QString("Mean: %1").arg(result->meanValue));

    source->releaseData(grid);

    return result;
}


MTask* MStructuredGridStatisticsAnalysis::createTaskGraph(MDataRequest request)
{
    MTask *task = new MTask(request, this);

    // Get task graphs for variable request.
    MDataRequestHelper rh(request);

    MDataRequestHelper varRH = rh.subRequest("VAR_DATA_");
    MWeatherPredictionDataSource *source =
            static_cast<MWeatherPredictionDataSource*>(
                getPrefixedDataSource("VAR_DATA_"));
    task->addParent(source->getTaskGraph(varRH.request()));

    return task;
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MStructuredGridStatisticsAnalysis::locallyRequiredKeys()
{
    return QStringList() << "HISTOGRAM_SIGNIFICANT_DIGITS"
                         << "HISTOGRAM_DISPLAYMODE";
}


/******************************************************************************
***                MStructuredGridStatisticsAnalysisControl                 ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MStructuredGridStatisticsAnalysisControl::
MStructuredGridStatisticsAnalysisControl(MNWPActorVariable *variable)
    : MSingleVariableAnalysisControl(variable),
      histogramAccuracyAdjustValue(1.),
      histogramDisplayMode(RELATIVE_FREQUENCY_DISTRIBUTION),
      axesChanged(true)
{
    QWidget *widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout();

    histogram = new QCustomPlot();
    layout->addWidget(histogram);
    histogram->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Expanding));

    minMaxMeanLabel = new QLabel();
    minMaxMeanLabel->setFrameShape(QFrame::Box);
    layout->addWidget(minMaxMeanLabel);

    widget->setLayout(layout);
    // Change background colour of widget to white to fit to the white
    // background of the histogram plot.
    QPalette pal = widget->palette();
    pal.setColor(widget->backgroundRole(), Qt::white);
    widget->setPalette(pal);
    widget->setAutoFillBackground(true);

    setDisplayWidget(widget);
    setDisplayTitle("Statistics of " + variable->variableName);

    // Connect afterReplot signal to onHistogramReploted slot to adapt x-axis
    // ticks on the fly while changing the size of the plot.
    connect(histogram, SIGNAL(afterReplot()), this, SLOT(onReplot()));
}


MStructuredGridStatisticsAnalysisControl::
~MStructuredGridStatisticsAnalysisControl()
{
    delete histogram;
    delete minMaxMeanLabel;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MStructuredGridStatisticsAnalysisControl::displayResult(
        MAnalysisResult *result)
{
    QString resultString;
    for (int i = 0; i < result->textResult.size() - 1; i++)
    {
        resultString += result->textResult[i] + "\n";
    }
    resultString += result->textResult[result->textResult.size() - 1];

    minMaxMeanLabel->setText(resultString);

    MStructuredGridStatisticsResult* sgsresult =
            dynamic_cast<MStructuredGridStatisticsResult*>(result);
    plotHistogram(histogram, sgsresult);
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MStructuredGridStatisticsAnalysisControl::onReplot()
{
    if (axesChanged)
    {
        adaptToPlotAxesChange();
        axesChanged = false;
        // Use singleShot with no delay instead of calling the function directly
        // since otherwise QCustomPlot will not execute the replot. (At least
        // in the case of qcustomplot library version 1.3, version 2.0 seems
        // okay.)
        QTimer::singleShot(0, histogram, SLOT(replot()));
        return;
    }
    axesChanged = true;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

MDataRequest MStructuredGridStatisticsAnalysisControl::
prepareRequest(MDataRequest analysisRequest)
{
    MDataRequestHelper rh(analysisRequest);

    MDataRequestHelper gridRH(variable->grid->getGeneratingRequest());
    gridRH.addKeyPrefix("VAR_DATA_");
    rh.unite(gridRH);

    return rh.request();
}


void MStructuredGridStatisticsAnalysisControl::updateAnalysisSourceInputs()
{
    analysisSource->clearDataSources();

    analysisSource->addDataSource(QString("VAR_DATA_"), variable->dataSource);
}


void MStructuredGridStatisticsAnalysisControl::plotHistogram(
        QCustomPlot *plot, MStructuredGridStatisticsResult *gsgresult)
{
    plot->clearPlottables();
    plot->setMinimumHeight(140);

    // Store accuracy adjust value and display mode since they are needed to
    // adapt the coordinate axes when the size of the widget changes.
    histogramAccuracyAdjustValue = gsgresult->histogramAccuracyAdjustValue;
    histogramDisplayMode = gsgresult->histogramDisplayMode;

    // Create bars that represent the data distribution.
    QCPBars *bars = new QCPBars(plot->xAxis, plot->yAxis);

// addPlottable method was removed from qcustomplot library with version 2.0.0.
#if QCPLOT_MAJOR_VERSION <= 1
    plot->addPlottable(bars);
#endif

    // Set data to create histogram from.
    bars->setData(gsgresult->histogramData.keys().toVector(),
                  gsgresult->histogramData.values().toVector());

    // Adapt bar width to avoid overlapping of bars. (Fit all values for the
    // given accuracy into the width of the axis).
    lowerBound =
            round(gsgresult->minValue * histogramAccuracyAdjustValue)
            / histogramAccuracyAdjustValue;
    upperBound =
            round(gsgresult->maxValue * histogramAccuracyAdjustValue)
            / histogramAccuracyAdjustValue;

    bars->setBaseValue(0.);

    // Use absolute pixel size as bar width to be able to set the width of a bar
    // to at least one pixel otherwise bars might disappear in cases of small
    // accuracy value, large range and sparse distribution.
    bars->setWidthType(QCPBars::WidthType::wtAbsolute);

    // Configure x/y axes.

    plot->xAxis->grid()->setVisible(true);
    plot->xAxis->setLabel(variable->variableName);
#if QCPLOT_MAJOR_VERSION <= 1
    plot->xAxis->setAutoSubTicks(false);
    plot->xAxis->setAutoTickStep(false);
#else
    QCPAxisTickerFixed *ticker = new QCPAxisTickerFixed();
    ticker->setScaleStrategy(QCPAxisTickerFixed::ScaleStrategy::ssPowers);
    ticker->setTickStepStrategy(
                QCPAxisTicker::TickStepStrategy::tssMeetTickCount);
    plot->xAxis->setTicker(QSharedPointer<QCPAxisTickerFixed>(ticker));
#endif
    // Rotate x-axis' labels by 90° clockwise to achieve a label's width
    // independent of number displayed. (Facilitates decision when to change
    // tick step size.)
    plot->xAxis->setTickLabelRotation(90.);

    plot->yAxis->grid()->setSubGridVisible(true);
    QPen gridPen;
    gridPen.setStyle(Qt::SolidLine);
    gridPen.setColor(QColor(0, 0, 0, 25));
    plot->yAxis->grid()->setPen(gridPen);
    gridPen.setStyle(Qt::DotLine);
    plot->yAxis->grid()->setSubGridPen(gridPen);
    plot->rescaleAxes();

    // Enable user interaction.
    plot->setInteractions(
                QCP::iRangeDrag | QCP::iRangeZoom);
    plot->axisRect()->setRangeDrag(Qt::Vertical);
    plot->axisRect()->setRangeZoom(Qt::Vertical);

    plot->replot();
}


void MStructuredGridStatisticsAnalysisControl::adaptToPlotAxesChange()
{
    if (histogram->plottableCount() == 0)
    {
        return;
    }
    // Scale histogram bars according to the width of the x-axis but always draw
    // them with width of at least one pixel.

    // Number of bars fitting in the range [lowerBound, upperBound] for the
    // accuracy specified by histogramAccuracyAdjustValue.
    double numBars =
            max(1., (upperBound - lowerBound) * histogramAccuracyAdjustValue);
    static_cast<QCPBars*>(histogram->plottable(0))
            ->setWidth(max(1, int((1. / (2. * numBars))
                                  * histogram->xAxis->axisRect()->width())));

    // Wrap text of label of y-Axis if the height is too small to fit the
    // text written in one line.
    if (histogramDisplayMode == RELATIVE_FREQUENCY_DISTRIBUTION)
    {
        QString axisLabel = "relative frequency (%)";
        QFontMetrics fontMetrics(histogram->yAxis->labelFont());
        int axisLabelWidth = fontMetrics.width(axisLabel);
        if (histogram->yAxis->axisRect()->height() >= axisLabelWidth)
        {
            histogram->yAxis->setLabel(axisLabel);
        }
        else
        {
            histogram->yAxis->setLabel("relative\nfrequency (%)");
        }
    }
    else
    {
        QString axisLabel = "absolute grid point count";
        histogram->yAxis->setLabel(axisLabel);
        QFontMetrics fontMetrics(histogram->yAxis->labelFont());
        int axisLabelWidth = fontMetrics.width(axisLabel);
        if (histogram->yAxis->axisRect()->height() >= axisLabelWidth)
        {
            histogram->yAxis->setLabel(axisLabel);
        }
        else
        {
            histogram->yAxis->setLabel("absolute grid\npoint count");
        }
    }

    // Get number of labels (equates number of ticks) that would be drawn
    // approximately for the given significant digits and data range. Set number
    // to at least one label since otherwise no labels are drawn for lower and
    // upper bound being equal.
    double numLabels =
            max(1.,
                ceil((upperBound - lowerBound) * histogramAccuracyAdjustValue));

    // Number of labels which can be placed without overlapping each other.
    // (Allow at least one label (for width = 0) to avoid division by zero.)
    double allowedNumLabels =
            max(1., ceil(histogram->xAxis->axisRect()->width() / 10.));

    // Compute number of labels with a distance of a power of ten in displayed
    // value which fits the given axis width:
    //  >> Solve for x: (numLabels / (10^x)) <= allowedNumLabels
    //  >> Solution   :  x <= log10(allowedNumLabels * numLabels)
    // x shall be integral: Use ceil to get the maximum number fulfiling the
    // inequation above.
    // Avoid negative values to get a distance of a power of ten and NOT of a
    // root of ten.
    numLabels =
            round(numLabels
                  / pow(10, max(0., ceil(log10(numLabels / allowedNumLabels)))));

    double tickStep = pow(10., -round(log10(histogramAccuracyAdjustValue)));
    if ((upperBound - lowerBound) != 0.)
    {
        tickStep = pow(10., round(log10((upperBound - lowerBound)/numLabels)));
    }

#if QCPLOT_MAJOR_VERSION <= 1
    // Set tick step to the power of 10 suiting the number of labels.
    histogram->xAxis->setTickStep(tickStep);

    // Set number of sub-ticks to 4 or 9 if tickstep isn't small enough to meet
    // the accuracy of the histogram. Use log10() and round() to get rid of
    // errors occurring due to computational inaccuracy. (Don't use more than 9
    // sub-ticks since otherwise the ticks will merge to one black bar.)
    if (round(log10(histogram->xAxis->tickStep()))
            > round(log10(1. / histogramAccuracyAdjustValue)))
    {
        // Only allow 9 sub-ticks if there is enough space for them.
        if (allowedNumLabels
                > 2 * ((upperBound - lowerBound) / histogram->xAxis->tickStep()))
        {
            histogram->xAxis->setSubTickCount(9);
        }
        // A number of 4 sub-ticks results in one sub-tick for every second bar
        // which would have a sub-tick if we would use a number of 9 sub-ticks.
        else
        {
            histogram->xAxis->setSubTickCount(4);
        }
    }
    else
    {
        histogram->xAxis->setSubTickCount(0);
    }
#else
    // Set tick step to the power of 10 suiting the number of labels. Use round
    // to get rid of computational inaccuracy.
    QCPAxisTickerFixed *ticker = static_cast<QCPAxisTickerFixed*>(
                histogram->xAxis->ticker().data());

    ticker->setTickStep(tickStep);

    // Show sub-ticks if tickstep isn't small enough to meet the accuracy
    // of the histogram. Use log10() and round() to get rid of errors occurring
    // due to computational inaccuracy.
    if (round(log10(ticker->tickStep()))
            > round(log10(1. / histogramAccuracyAdjustValue)))
    {
        histogram->xAxis->setSubTicks(true);
    }
    else
    {
        histogram->xAxis->setSubTicks(false);
    }
#endif
}

} // namespace Met3D
