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
#ifndef STRUCTUREDGRIDSTATISTICSANALYSISSOURCE_H
#define STRUCTUREDGRIDSTATISTICSANALYSISSOURCE_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "singlevariableanalysis.h"
#include "qcustomplot.h"


namespace Met3D
{


class MStructuredGridStatisticsResult : public MAnalysisResult
{
public:
    MStructuredGridStatisticsResult();

    unsigned int getMemorySize_kb();

    // Data drawn from grid.
    double minValue;
    double maxValue;
    double meanValue;
    QMap<double, double> histogramData; // distribution of data values

    // User input.
    double histogramAccuracyAdjustValue; // value used to adjust data values
                                         // to histogram accuracy
                                         // (= 10^(number of significant digits))
    int   histogramDisplayMode; // mode distribution was generated for
                                // (0: as relative frequencies;
                                //  1: as absolute grid point count)
};


/**
  @brief Implements an analysis source that analyses the statistics of the
  structed grid of a single variable.

  Part of the statistics are minimum, maximum and mean value and the
  distribution of the data values. To work correctly "histogram accuracy
  significant digits" and "histogram display mode" need to be specified.

  "Histogram significant digits" = number of significant digits used when
  computing the histogram. Might be negative if the user wishes to ignore
  digits in front of the decimal point.

  "Histogram display mode" defines whether the distribution should be computed
  as relative frequencies of the total amount of data values given (= 0) or as
  absolute grid point count (= 1).
  */
class MStructuredGridStatisticsAnalysis
        : public MAnalysisDataSource
{
    Q_OBJECT

public:
    MStructuredGridStatisticsAnalysis();

    MAnalysisResult* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

protected:
    const QStringList locallyRequiredKeys();

};


/**
 @brief Control associated with @ref MStructuredGridStatisticsAnalysis.

 It uses the results of @ref MStructuredGridStatisticsAnalysis and displays the
 distribution of the data values as a histogram and min, max and mean value as
 plain text in the result dock widget @ref dock.
 */
class MStructuredGridStatisticsAnalysisControl
        : public MSingleVariableAnalysisControl
{
    Q_OBJECT

public:
    MStructuredGridStatisticsAnalysisControl(MNWPActorVariable *variable);
    ~MStructuredGridStatisticsAnalysisControl();

    void displayResult(MAnalysisResult *result);

    enum HistogramDisplayMode
    {
        RELATIVE_FREQUENCY_DISTRIBUTION = 0,
        ABSOLUTE_COUNTS = 1
    };

public slots:
    void onReplot();

protected:
    MDataRequest prepareRequest(MDataRequest analysisRequest);

    MAnalysisDataSource* createAnalysisSource()
    { return new MStructuredGridStatisticsAnalysis(); }

    void updateAnalysisSourceInputs();

    void plotHistogram(QCustomPlot *plot,
                       MStructuredGridStatisticsResult *gsgresult);

    /**
     Adapts number of ticks and sub-ticks of x-axis of @ref histogram to
     @ref histogramAccuracy and value range (@ref upperBound - @ref lowerBound)
     to avoid overlapping labels but nevertheless show one tick per bar if
     possible.

     Wraps the text of the y-axis' label once if the y-axis' height falls below
     a certain threshold.

     Adapts bar width to x-axis' width but ensures that their width is at least
     of one pixel.
     */
    void adaptToPlotAxesChange();

    QCustomPlot *histogram;
    QLabel      *minMaxMeanLabel;

    double lowerBound;
    double upperBound;
    double histogramAccuracyAdjustValue;
    int histogramDisplayMode;
    // Indicates that the axes might have been changed and ticks, labels and
    // bars need to be adapted. Used to avoid endless loop when updating the
    // plot axes.
    bool   axesChanged;
};


} // namespace Met3D

#endif // STRUCTUREDGRIDSTATISTICSANALYSISSOURCE_H
