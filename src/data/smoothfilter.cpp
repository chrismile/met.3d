/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus
**  Copyright 2020      Andreas Beckert
**
**  Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
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
#include "smoothfilter.h"

// standard library imports
#include "assert.h"
#include<cmath>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "gxfw/nwpactorvariableproperties.h"
#include "util/mutil.h"
#include "util/mexception.h"
#include "util/metroutines.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSmoothFilter::MSmoothFilter()
    : MSingleInputProcessingWeatherPredictionDataSource()
{

}

/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/


MStructuredGrid *MSmoothFilter::produceData(MDataRequest request)
{
    assert(inputSource != nullptr);

    MDataRequestHelper rh(request);

    // Parse request.
    // "SMOOTH" = Filter type / std deviation (km) / std deviation (grid points)
    // filter type is uniform weights, std deviation is interpreted as radius.
    QStringList parameterList = rh.value("SMOOTH").split("/");
    rh.removeAll(locallyRequiredKeys());
    // The first parameter passes the filter type.
    MSmoothProperties::SmoothModeTypes filterType =
            static_cast<MSmoothProperties::SmoothModeTypes>(
                parameterList[0].toInt());

    MStructuredGrid *result = nullptr;

    MStructuredGrid* inputGrid = inputSource->getData(rh.request());
    result = createAndInitializeResultGrid(inputGrid);
    QString smoothModeName = MSmoothProperties::smoothModeToString(filterType);
    LOG4CPLUS_DEBUG(mlog, "Smooth filter: computing "
                    << smoothModeName.toUtf8().constData());

    switch (filterType)
    {
    // Original Gaussian blur filter with precomputed weights.
    case MSmoothProperties::GAUSS_DISTANCE:
    {
        float stdDev_km = parameterList[1].toFloat();
        computeHorizontalGaussianSmoothing_GCDistance(inputGrid, result,
                                                      stdDev_km);
        break;
    }
    // Box blur filter, where the box size is precalculated according to
    // the distance between grid points. Distance changes between longitudes
    // according to the latitude are considered.
    case MSmoothProperties::BOX_BLUR_DISTANCE_FAST:
    {
        float stdDev_km = parameterList[1].toFloat();
        computeHorizontalBoxBlurSmoothing_GCDistanceFast(
                    inputGrid, result, stdDev_km);
        break;
    }
    // Uniform weights of surrounding grid points.
    case MSmoothProperties::UNIFORM_WEIGHTED_GRIDPOINTS:
    {
        int radius_gp = parameterList[2].toInt();
        computeHorizontalUniformWeightedSmoothing_GCGridpoints(
                    inputGrid, result, radius_gp);
        break;
    }
    // Original Gaussian blur filter on grid points.
    case MSmoothProperties::GAUSS_GRIDPOINTS:
    {
        int stdDev_gp = parameterList[2].toInt();
        computeHorizontalGaussianSmoothing_GCGridpoints(
                    inputGrid, result, stdDev_gp);
        break;
    }
    // Box blur filter. This is the original implementation of the box blur
    // filter but very slow.
    case MSmoothProperties::BOX_BLUR_GRIDPOINTS_SLOW:
    {
        int stdDev_gp = parameterList[2].toInt();
        computeHorizontalBoxBlurSmoothing_GCGridpointsSlow(
                    inputGrid, result, stdDev_gp);
        break;
    }
    // Fastest box blur filter, with the same result as the slow box blur filter.
    case MSmoothProperties::BOX_BLUR_GRIDPOINTS_FAST:
    {
        int stdDev_gp = parameterList[2].toInt();
        computeHorizontalBoxBlurSmoothing_GCGridpointsFast(
                    inputGrid, result, stdDev_gp);
        break;
    }
    default:
        LOG4CPLUS_ERROR(mlog, "This smooth filter does not exists. "
                              "Return nullptr data field."
                        << smoothModeName.toUtf8().constData());
    }
    return result;
}


MTask *MSmoothFilter::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);
    MTask* task = new MTask(request, this);
    // Simply request the variable that was requested from this data source
    //(we're requesting the unsmoothed field and pass on the smoothed
    //version).
    MDataRequestHelper rh(request);
    rh.removeAll(locallyRequiredKeys());
    task->addParent(inputSource->getTaskGraph(rh.request()));
    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MSmoothFilter::locallyRequiredKeys()
{
    return (QStringList() << "SMOOTH");
}


// Kernel cannot be precomputed as distances between center point and
// surrounding points change depending on geographical position.
// Gauss is implemented as convolution of latitudinal and longitudinal Gauss
void MSmoothFilter::computeHorizontalGaussianSmoothing_GCDistance(
        const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
        float stdDev_km)
{
    MStructuredGrid *resultGridTemp = nullptr;
    resultGridTemp = createAndInitializeResultGrid(resultGrid);
    float totalWeight;
    float totalValue, currentValue;
    int nWeights;
    int nLons = inputGrid->getNumLons();
    int nLats = inputGrid->getNumLats();
    int nLev = inputGrid->getNumLevels();
    int iMin, iMax, jMin, jMax;
    QList<QList<float>> latDependentLonWeights =
            precomputeLatDependendDistanceWeightsOfLongitude(inputGrid,
                                                             stdDev_km);
    QList<float> weightsLat = precomputeDistanceWeightsOfLatitude(
                inputGrid, stdDev_km);
//#pragma omp parallel for
    for (int k = 0; k < nLev; k++)
    {
        // Longitudinal Gauss smoothing.
        for (int j = 0; j < nLats; j++)
        {
            nWeights = latDependentLonWeights[j].size();
            for (int i = 0; i < nLons; i++)
            {
                totalValue = 0;
                totalWeight = 0;
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(currentValue))
                {
                    resultGridTemp->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    iMin = std::max(i - nWeights + 1, 0);
                    iMax = std::min(i + nWeights, nLons);
                    for (int m = iMin; m < iMax; m++)
                    {
                        currentValue = inputGrid->getValue(k, j, m);
                        if (!IS_MISSING(currentValue))
                        {
                            totalValue += currentValue
                                    * latDependentLonWeights[j][abs(i - m)];
                            totalWeight += latDependentLonWeights[j][abs(i - m)];
                        }
                    }
                    resultGridTemp->setValue(k, j, i, totalValue / totalWeight);
                }
            }
        }
        // Latitudinal Gauss smoothing.
        nWeights = weightsLat.size();
        for (int i = 0; i < nLons; i++)
        {
            for (int j = 0; j < nLats; j++)
            {
                totalValue = 0;
                totalWeight = 0;
                currentValue = resultGridTemp->getValue(k, j, i);
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    jMin = std::max(j - nWeights + 1, 0);
                    jMax = std::min(j + nWeights, nLats);
                    for (int m = jMin; m < jMax; m++)
                    {
                        currentValue = resultGridTemp->getValue(k, m, i);
                        if (!IS_MISSING(currentValue))
                        {
                            totalValue += currentValue
                                    * weightsLat[abs(j - m)];
                            totalWeight += weightsLat[abs(j - m)];
                        }
                    }
                    resultGrid->setValue(k, j, i, totalValue / totalWeight);
                }
            }
        }
    }
}


// Longitudinal Box Blur smoothing.
void MSmoothFilter::computeHorizontalBoxBlurSmoothing_GCDistanceFast(
        const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
        float stdDev_km)
{
    QList<QList<int>> latDependentBoxRadii;
    int n = 3;
    latDependentBoxRadii = computeLatDependentBoxRadii(inputGrid, stdDev_km, n);
    // Distance between latitudes.
    // Distance between latitudes.
    //const float deltaGp_km = MetConstants::DELTA_LAT_km
    //         / (1. / inputGrid->getDeltaLat());
    const float deltaGp_km = inputGrid->getDeltaLatInKm();
    int distanceInGridpoints = static_cast<int>(round(stdDev_km / deltaGp_km));
    QVector<int> lonBoxRadii = computeBoxRadii(distanceInGridpoints, n);
    MStructuredGrid *resultGridTemp = nullptr;
    resultGridTemp = createAndInitializeResultGrid(resultGrid);
    boxBlurTotalFast(inputGrid, resultGrid, lonBoxRadii[0],
            latDependentBoxRadii[0]);
    boxBlurTotalFast(resultGrid, resultGridTemp, lonBoxRadii[1],
            latDependentBoxRadii[1]);
    boxBlurTotalFast(resultGridTemp, resultGrid, lonBoxRadii[2],
            latDependentBoxRadii[2]);
 };


QList<QList<int>> MSmoothFilter::computeLatDependentBoxRadii(
        const MStructuredGrid *inputGrid, float stdDev_km,
        int n)
{
    int stdDev_gp;
    QList<QList<int>> boxes;
    QList<int> t;
    for (int i = 0; i < n; i++)
    {
        boxes.append((QList<int>) t);
    }
    for (unsigned int i = 0; i < inputGrid->getNumLats(); i++)
    {
        stdDev_gp = numGridpointsSpannedByDistance(inputGrid, i, stdDev_km);
        // Ideal averaging filter width.
        float widthIdeal = sqrt((12. * pow(stdDev_gp, 2) / n) + 1.);
        // Filter width rounded to nearest odd integer less than widthIdeal.
        int widthIdealLess = floor(widthIdeal);
        // Check if widthIdealLess is odd.
        if ((widthIdealLess % 2) == 0)
        {
            widthIdealLess--;
        }
        // Nearest odd integer higher than ideal width.
        int widthIdealUp = widthIdealLess + 2;
        // mIdeal determines at which pass the nearest odd integer width higher
        // than the width ideal should be used. This should compensate the
        // rounding in the first place to the nearest integer less than
        // the ideal width.
        float mIdeal = (12. * pow(stdDev_gp, 2.) - n *
                        pow(widthIdealLess, 2.)
                        - 4. * float(n) * float(widthIdealLess) - 3. * n)
                /(-4. * float(widthIdealLess) - 4.);
        // m has to be rounded to nearest integer.
        int m = round(mIdeal);
        for (int j = 0; j < n; j++)
        {
            if (j < m)
            {
                boxes[j].append((widthIdealLess - 1) / 2);
            }
            else
            {
                boxes[j].append((widthIdealUp - 1) / 2);
            }
        }
    }
    return boxes;
};


int MSmoothFilter::numGridpointsSpannedByDistance(
        const MStructuredGrid *inputGrid, int iLat, float distance_km)
{
    float distanceInDeg;
    int distanceInGridpoints;
    float phi = abs(inputGrid->getLats()[iLat]) * M_PI / 180.;
    float latitudeCircleInKm = cos(phi) * 2. * M_PI
            * MetConstants::EARTH_RADIUS_km;
    // This should prevent an division by zero when phi is 90°.
    // TODO(ab, 13Feb2020) --: Rethink this error query, probably there are
    // smarter ways.
    // distanceInDegree should not be larger than 360°.
    if (latitudeCircleInKm > 0)
    {
        distanceInDeg = distance_km/latitudeCircleInKm * 360.;
        if (distanceInDeg > 360.) {distanceInDeg = 360.;}
    }
    else
    {
        distanceInDeg = 360.;
    }
    distanceInGridpoints = round(distanceInDeg / inputGrid->getDeltaLon());
    return distanceInGridpoints;
}


QList<QList<float>> MSmoothFilter::precomputeLatDependendDistanceWeightsOfLongitude(
        const MStructuredGrid *inputGrid, float stdDev_km)
{
    //Significant radius, all grid points within the 99% quantile
    //of a Gaussian distribution are considered. The 99% quantile is the
    //result of the standard deviation multiplied by 2.576. For more
    //information see: https://en.wikipedia.org/wiki/Normal_distribution
    float sigRadius = stdDev_km * 2.576;
    int sigRadius_gridpoints;
    float deltaGridpoint_km;
    float distance_km;
    QList<float> weights;
    QList<QList<float>> latDependendWeights;
    for (unsigned int i = 0; i<inputGrid->getNumLats(); i++)
    {
        //deltaGridpoint_km = inputGrid->getGeometricDeltaLat_km(i);
        deltaGridpoint_km = inputGrid->getDeltaLonInKm(i);
        sigRadius_gridpoints = round(sigRadius/deltaGridpoint_km);
        for (int j = 0; j <= sigRadius_gridpoints; j++)
        {
            distance_km = j *  deltaGridpoint_km;
            weights.append(computeGaussWeight(stdDev_km, distance_km));
        }
        latDependendWeights.append((QList<float>) weights);
        weights.clear();
    }
    return latDependendWeights;
};


QList<float> MSmoothFilter::precomputeDistanceWeightsOfLatitude(
        const MStructuredGrid *inputGrid, float stdDev_km)
{
    //Significant radius, all grid points within the 99% quantile
    //of a Gaussian distribution are considered. The 99% quantile is the
    //result of the standard deviation multiplied by 2.576. For more
    //information see: https://en.wikipedia.org/wiki/Normal_distribution
    float significantRadius = stdDev_km * 2.576;
    int significantRadius_gridpoints;
    //float deltaGridpoints_km = inputGrid->getGeometricDeltaLat_km();
    float deltaGridpoints_km = inputGrid->getDeltaLatInKm();
    float distance_km;
    QList<float> weights;
    significantRadius_gridpoints =
            round(significantRadius / deltaGridpoints_km);

    for (int j = 0; j <= significantRadius_gridpoints; j++)
    {
        distance_km = j * deltaGridpoints_km;
        weights.append(computeGaussWeight(stdDev_km, distance_km));
    }
    return weights;
};


float MSmoothFilter::computeGaussWeight(float stdDev_km, float distance_km)
{
    float weight = exp(-(pow(distance_km, 2.) / (2. * pow(stdDev_km, 2.))))
            / sqrt(1. / (2. * M_PI * pow(stdDev_km, 2.)));
    return weight;
};


void MSmoothFilter::boxBlurTotalFast(
        const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
        int lonBoxRadius, QList<int> latDependentBoxRadii)
{
    MStructuredGrid *resultGridTemp = nullptr;
    resultGridTemp = createAndInitializeResultGrid(resultGrid);
    boxBlurLongitudinalFast(inputGrid, resultGridTemp, latDependentBoxRadii);
    boxBlurLatitudinalFast(resultGridTemp, resultGrid, lonBoxRadius);
    delete resultGridTemp;

};


void MSmoothFilter::boxBlurLongitudinalFast(
        const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
        QList<int> latDependentBoxRadii)
{
    float iarr, value, currentValue, minusValue = 0.0, plusValue = 0.0;;
    int boxRadius;
    int nLon = inputGrid->getNumLons();
    int nGridPoints;
//#pragma omp parallel for
    for (unsigned int k = 0; k < inputGrid->getNumLevels(); k++)
    {
        for (unsigned int j = 0; j < inputGrid->getNumLats(); j++)
        {
            boxRadius = latDependentBoxRadii[j];
            value = 0;
            nGridPoints = 0;
            // Adds the values until radius is reached.
            // This is then the first value to be set on the result grid.
            for (int i = 0; i < boxRadius; i++)
            {
                plusValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(plusValue))
                {
                    plusValue = 0;
                    nGridPoints--;
                }
                value += plusValue;
                nGridPoints++;
            }
            for(int i = 0; i < boxRadius + 1; i++)
            {
                plusValue = inputGrid->getValue(k, j, i + boxRadius);
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(plusValue))
                {
                    plusValue = 0;
                    nGridPoints--;
                }
                value += plusValue;
                nGridPoints++;
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    iarr = 1. / float(nGridPoints);
                    resultGrid->setValue(k, j, i, value * iarr);
                }
            }

            for(int i = boxRadius + 1; i < (nLon - boxRadius); i++)
            {
                plusValue = inputGrid->getValue(k, j, i + boxRadius);
                minusValue = inputGrid->getValue(k, j, i - boxRadius - 1);
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(plusValue))
                {
                    plusValue = 0;
                    nGridPoints--;
                }
                if (IS_MISSING(minusValue))
                {
                    minusValue = 0;
                    nGridPoints++;
                }
                value += plusValue - minusValue;
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    iarr = 1. / float(nGridPoints);
                    resultGrid->setValue(k, j, i, value * iarr);
                }
            }

            for(int i = (nLon - boxRadius); i < nLon; i++)
            {
                minusValue = inputGrid->getValue(k, j, i - boxRadius - 1);
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(minusValue))
                {
                    minusValue = 0;
                    nGridPoints ++;
                }
                value -= minusValue;
                nGridPoints--;
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    iarr = 1. / float(nGridPoints);
                    resultGrid->setValue(k, j, i, value * iarr);
                }
            }
        }
    }
};


void MSmoothFilter::boxBlurLatitudinalFast(
        const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
        int boxRadius)
{
    const unsigned int nlat = inputGrid->getNumLats();
    int nGridPoints;
    float value, currentValue, iarr, minusValue = 0.0, plusValue = 0.0;
//#pragma omp parallel for
    for (unsigned int k = 0; k < inputGrid->getNumLevels(); k++)
    {
        for (unsigned int i = 0; i < inputGrid->getNumLons(); i++)
        {
            value = 0;
            nGridPoints = 0;
            // Adds the values until radius is reached.
            // This is then the first value to be set on the result grid.
            for (int j = 0; j < boxRadius; j++)
            {
                plusValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(plusValue))
                {
                    plusValue = 0;
                    nGridPoints --;
                }
                value += plusValue;
                nGridPoints++;
            }

            for(int j = 0; j < boxRadius + 1; j++)
            {
                plusValue = inputGrid->getValue(k, j + boxRadius, i);
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(plusValue))
                {
                    plusValue = 0;
                    nGridPoints --;
                }
                value += plusValue;
                nGridPoints++;
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    iarr = 1. / float(nGridPoints);
                    resultGrid->setValue(k, j, i, value * iarr);
                }
            }
            for(unsigned int j = boxRadius + 1; j < (nlat - boxRadius); j++)
            {
                plusValue = inputGrid->getValue(k, j + boxRadius, i);
                minusValue = inputGrid->getValue(k, j - boxRadius - 1, i);
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(plusValue))
                {
                    plusValue = 0;
                    nGridPoints --;
                }
                if (IS_MISSING(minusValue))
                {
                    minusValue = 0;
                    nGridPoints ++;
                }
                value += plusValue - minusValue;
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    iarr = 1. / float(nGridPoints);
                    resultGrid->setValue(k, j, i, value * iarr);
                }
            }
            for(unsigned int j = (nlat - boxRadius); j < nlat; j++)
            {
                minusValue = inputGrid->getValue(k, j - boxRadius - 1, i);
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(minusValue))
                {
                    minusValue = 0;
                    nGridPoints ++;
                }
                value -= minusValue;
                nGridPoints--;
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    iarr = 1. / float(nGridPoints);
                    resultGrid->setValue(k, j, i, value * iarr);
                }
            }
        }
    }
};


void MSmoothFilter::computeHorizontalUniformWeightedSmoothing_GCGridpoints(
        const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
        int radius_gp)
{
    const unsigned int nLon = inputGrid->getNumLons();
    const unsigned int nlat = inputGrid->getNumLats();
    int nSmoothPoints;
    float totalValue, addValue, currentValue;
//#pragma omp parallel for
    for (unsigned int k = 0; k < inputGrid->getNumLevels(); ++k)
    {
        for (unsigned int j = 0; j < nlat; ++j)
        {
            for (unsigned int i = 0; i < nLon; ++i)
            {
                // Implementation of the simplest smoothing filter.
                // Take the sdtDev_gp parameter as grid distance and smooth
                // without accounting any weights.

                int iStart = max(0, static_cast<int>(i - radius_gp));
                int iEnd = min(static_cast<int>(nLon),
                               static_cast<int>(i + radius_gp));
                int jStart = max(0, static_cast<int>(j - radius_gp));
                int jEnd = min(static_cast<int>(nlat),
                               static_cast<int>(j + radius_gp));

                totalValue = 0;
                nSmoothPoints = 0;
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i,
                                         M_MISSING_VALUE);
                }
                else
                {
                    for (int js = jStart; js < jEnd; js++)
                    {
                        for (int is = iStart; is < iEnd; is++)
                        {
                            addValue = inputGrid->getValue(k, js, is);
                            if (!IS_MISSING(addValue))
                            {
                                totalValue += addValue;
                                nSmoothPoints ++;
                            }
                        }
                    }
                    resultGrid->setValue(k, j, i,
                                         totalValue/float(nSmoothPoints));
                 }
            }
        }
    }
 };


void MSmoothFilter::computeHorizontalGaussianSmoothing_GCGridpoints(
        const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
        int stdDev_gp)
{
    const unsigned int nlon = inputGrid->getNumLons();
    const unsigned int nlat = inputGrid->getNumLats();
    const float radius = 2. * pow(float(stdDev_gp), 2.);
    //Significant radius, all grid points within the 99% quantile
    //of a Gaussian distribution are considered. The 99% quantile is the
    //result of the standard deviation multiplied by 2.576. For more
    //information see: https://en.wikipedia.org/wiki/Normal_distribution
    const int sigRadius = ceil(float(stdDev_gp) * 2.576);
    int squaredDistance;
    float weight, currentValue, totalValue, addValue, weightSum;
//#pragma omp parallel for
    for (unsigned int k = 0; k < inputGrid->getNumLevels(); ++k)
        {
        for (unsigned int j = 0; j < nlat; ++j)
        {
            for (unsigned int i = 0; i < nlon; ++i)
            {
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    totalValue = 0;
                    weightSum = 0;
                    int west = max(0, static_cast<int>(i) - sigRadius);
                    int east = min(static_cast<int>(nlon), static_cast<int>(i)
                                    + sigRadius);
                    int south = max(0, static_cast<int>(j) - sigRadius);
                    int north = min(static_cast<int>(nlat), static_cast<int>(j)
                                    + sigRadius);
                    for(int n = south; n < north; n++)
                    {
                        for(int m = west; m < east; m++)
                        {
                            addValue = inputGrid->getValue(k, n, m);
                            if (!IS_MISSING(addValue))
                            {
                                squaredDistance = (n - j) * (n - j) + (m - i) * (m - i);
                                weight = exp(-squaredDistance / radius)
                                        / (M_PI * radius);
                                totalValue += addValue * weight;
                                weightSum += weight;
                            }
                        }
                    }
                    resultGrid->setValue(k, j, i, (totalValue / weightSum));
                }
            }
        }
    }
};


QVector<int> MSmoothFilter::computeBoxRadii(int stdDev_gp, int n)
{
    QVector<int> boxRadii(n);
    // Ideal averaging filter width.
    float widthIdeal = sqrt((12. * pow(stdDev_gp, 2.) / float(n)) + 1.);
    // Filter width rounded to nearest odd integer less than widthIdeal.
    int widthIdealLess = floor(widthIdeal);
    if ((widthIdealLess % 2) == 0)
    {
        widthIdealLess--;
    }
    // Check if widthIdealLess is odd.
    int widthIdealUp = widthIdealLess + 2;
    // mIdeal determines at which pass the nearest odd integer width is higher
    // than the ideal width should be used. This should compensate the
    // rounding in the first place to the nearest integer less than
    // the ideal width.
    float mIdeal = (12. * pow(stdDev_gp, 2.) - n
                    * pow(float(widthIdealLess), 2.) - 4.
                    * float(n) * widthIdealLess - 3. * float(n))
            / (-4. * float(widthIdealLess) - 4.);
    int m = round(mIdeal);
    for (int i = 0; i < n; i++)
    {
        if (i < m)
        {
            boxRadii[i] = (widthIdealLess - 1) / 2;
        }
        else
        {
            boxRadii[i] = (widthIdealUp - 1) / 2;
        }
    }
    return boxRadii;
};


void MSmoothFilter::boxBlurTotalSlow(const MStructuredGrid *inputGrid,
                                     MStructuredGrid *resultGrid, int boxRadius)
{
    // no missing value handling!
    // same result as boxBlurTotalFast, but with missing value handling
    int nLat = inputGrid->getNumLats();
    int nLon = inputGrid->getNumLons();
//#pragma omp parallel for
    for (unsigned int k = 0; k < inputGrid->getNumLevels(); k++)
    {
        for (int j = 0; j < nLat; j++)
        {
            for (int i = 0; i < nLon; i++)
            {
                float value = 0;
                for(int iy = j - boxRadius; iy <= j + boxRadius; iy++)
                {
                    for(int ix = i - boxRadius; ix <= i + boxRadius; ix++)
                    {
                        int x = min(nLon - 1, max(0, ix));
                        int y = min(nLat - 1, max(0, iy));
                        value += inputGrid->getValue(k, y, x);
                    }
                }
                resultGrid->setValue(k, j, i,
                                     (value / ((2 * float(boxRadius) + 1)
                                               * (2 * float(boxRadius) + 1))));
            }
        }
    }
};


void MSmoothFilter::computeHorizontalBoxBlurSmoothing_GCGridpointsSlow(
        const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
        int stdDev_gp)
{
    QVector<int> boxRadii = computeBoxRadii(stdDev_gp, 3);
    MStructuredGrid *resultGridTemp = nullptr;
    resultGridTemp = createAndInitializeResultGrid(resultGrid);
    boxBlurTotalSlow(inputGrid, resultGrid, boxRadii[0]);
    boxBlurTotalSlow(resultGrid, resultGridTemp, boxRadii[1]);
    boxBlurTotalSlow(resultGridTemp, resultGrid, boxRadii[2]);
    delete resultGridTemp;
 };


void MSmoothFilter::computeHorizontalBoxBlurSmoothing_GCGridpointsFast(
        const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
        int stdDev_gp)
{
    QVector<int> boxRadii = computeBoxRadii(stdDev_gp, 3);
    MStructuredGrid *resultGridTemp = nullptr;
    resultGridTemp = createAndInitializeResultGrid(resultGrid);
    boxBlurTotalFast(inputGrid, resultGrid, boxRadii[0]);
    boxBlurTotalFast(resultGrid, resultGridTemp, boxRadii[1]);
    boxBlurTotalFast(resultGridTemp, resultGrid, boxRadii[2]);
    delete resultGridTemp;
 };


void MSmoothFilter::boxBlurTotalFast(const MStructuredGrid *inputGrid,
                                     MStructuredGrid *resultGrid, int boxRadius)
{
    MStructuredGrid *resultGridTemp = nullptr;
    resultGridTemp = createAndInitializeResultGrid(resultGrid);
    boxBlurLongitudinalFast(inputGrid, resultGridTemp, boxRadius);
    boxBlurLatitudinalFast(resultGridTemp, resultGrid, boxRadius);
    delete resultGridTemp;
};


void MSmoothFilter::boxBlurLongitudinalFast(const MStructuredGrid *inputGrid,
                                 MStructuredGrid *resultGrid, int boxRadius)
{
    int nGridPoints;;
    int nLon = inputGrid->getNumLons();
    float iarr, value, currentValue, minusValue = 0.0, plusValue = 0.0;
//#pragma omp parallel for
    for (unsigned int k = 0; k < inputGrid->getNumLevels(); k++)
    {
        for (unsigned int j = 0; j < inputGrid->getNumLats(); j++)
        {
            value = 0;
            nGridPoints = 0;
            // Adds the values until radius is reached.
            // This is then the first value to be set on the result grid.
            for (int i = 0; i < boxRadius; i++)
            {
                plusValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(plusValue))
                {
                    plusValue = 0;
                    nGridPoints --;
                }
                value += plusValue;
                nGridPoints++;
            }

            for(int i = 0; i < boxRadius + 1; i++)
            {
                plusValue = inputGrid->getValue(k, j, i + boxRadius);
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(plusValue))
                {
                    plusValue = 0;
                    nGridPoints --;
                }
                value += plusValue;
                nGridPoints++;
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    iarr = 1. / float(nGridPoints);
                    resultGrid->setValue(k, j, i, value * iarr);
                }
            }

            for(int i = boxRadius + 1; i < (nLon - boxRadius); i++)
            {
                plusValue = inputGrid->getValue(k, j, i + boxRadius);
                minusValue = inputGrid->getValue(k, j, i - boxRadius - 1);
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(plusValue))
                {
                    plusValue = 0;
                    nGridPoints --;
                }
                if (IS_MISSING(minusValue))
                {
                    minusValue = 0;
                    nGridPoints ++;
                }
                value += plusValue - minusValue;
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    iarr = 1. / float(nGridPoints);
                    resultGrid->setValue(k, j, i, value * iarr);
                }
            }

            for(int i = (nLon - boxRadius); i < nLon; i++)
            {
                minusValue = inputGrid->getValue(k, j, i - boxRadius - 1);
                currentValue = inputGrid->getValue(k, j, i);
                if (IS_MISSING(minusValue))
                {
                    minusValue = 0;
                    nGridPoints ++;
                }
                value -= minusValue;
                nGridPoints--;
                if (IS_MISSING(currentValue))
                {
                    resultGrid->setValue(k, j, i, M_MISSING_VALUE);
                }
                else
                {
                    iarr = 1. / float(nGridPoints);
                    resultGrid->setValue(k, j, i, value * iarr);
                }
            }
        }
    }
};

}  // namespace Met3D
