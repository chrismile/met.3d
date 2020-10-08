/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020 Andreas Beckert
**  Copyright 2020 Marc Rautenhaus
**
**  Regional Computing Center, Visual Data Analysis Group
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
#ifndef SMOOTHFILTER_H
#define SMOOTHFILTER_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "processingwpdatasource.h"
#include "structuredgrid.h"
#include "datarequest.h"
#include "gxfw/nwpactorvariableproperties.h"

namespace Met3D
{

/**
  @brief MSmoothFilter implements smoothing operations for gridded data.
 */
class MSmoothFilter
        : public MSingleInputProcessingWeatherPredictionDataSource
{
public:
    MSmoothFilter();

    MStructuredGrid* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

protected:

    const QStringList locallyRequiredKeys();

private:

//*************************** GAUSSIAN SMOOTHING *******************************

    /**
     * @brief computeHorizontalGaussianSmoothing_GCDistance Original Gaussian
     * smoothing with distance weighted averages. The algorithm is implemented
     * as convolution of longitudinal and latitudinal Gaussian smoothing.
     * Weights are precomputed to save computation time. For longitudinal Gauss,
     * weights depend on standard deviation and latitude position. For
     * latitudinal Gauss, weights only depend on standard deviation.
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param stdDev_km standard deviation in km
     */
    void computeHorizontalGaussianSmoothing_GCDistance(
            const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
            float stdDev_km);

    /**
     * @brief computeHorizontalGaussianSmoothing_GCGridpoints Original Gaussian
     * smoothing with weights depending on the grid points, not the real
     * distance, since the horizontal distance between grid points depends
     * on the latitude.
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param stdDev_gp standard deviation as grid points
     */
    void computeHorizontalGaussianSmoothing_GCGridpoints(
            const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
            int stdDev_gp);

    /**
     * @brief precomputeLatDependendDistanceWeightsOfLongitude precompute weights for
     * distance weighted Gaussian blur within the significant radius.
     * @param inputGrid pointer to input grid
     * @param stdDev_km standard deviation in km
     * @return list of weights according to latitude and
     * distance [lat][Gaussian weights]
     */
    QList<QList<float>> precomputeLatDependendDistanceWeightsOfLongitude(
            const MStructuredGrid *inputGrid, float stdDev_km);

    /**
     * @brief precomputeDistanceWeightsOfLongitude precomputes weights for
     * distance weighted Gaussian blur within the significant radius.
     * @param inputGrid pointer to input grid
     * @param stdDev_km standard deviation in km
     * @return list of weights according to distance [Gaussian weights]
     */
    QList<float> precomputeDistanceWeightsOfLatitude(
            const MStructuredGrid *inputGrid, float stdDev_km);

    /**
     * @brief computeGaussWeight computes Gaussian weights according to chosen
     * standard deviation and distance to center point
     * @param stdDev_km standard deviation in km
     * @param distance_km distance to center point
     * @return Gaussian weight
     */
    float computeGaussWeight(float stdDev_km, float distance_km);


//*************************** BOX BLUR SMOOTHING *******************************

    /**
     * @brief computeHorizontalBoxBlurSmoothing_GCDistanceFast Convolution
     * of longitudinal and latitudinal box blur. For longitudinal smoothing
     * box size depends on latitude position and standard deviation.
     * For latitudinal smoothing box size depend only on standard deviation.
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param stdDev_km standard deviation in km
     */
    void computeHorizontalBoxBlurSmoothing_GCDistanceFast(
            const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
            float stdDev_km, MSmoothProperties::BoundaryModeTypes boundaryType);

    /**
     * @brief computeHorizontalBoxBlurSmoothing_GCGridpointsFast Convolution of
     * box blur. The result is identical with with
     * computeHorizontalBoxBlurSmoothing_GCGridpointsSlow() but much faster.
     * Idea by Wojciech Jarosz, see: ACM SIGGRAPH@UIUC ACM SIGGRAPH@UIUC
     * Fast Image Convolutions, https://web.archive.org/web/20060718054020/
     * http://www.acm.uiuc.edu/siggraph/workshops/wjarosz_convolution_2001.pdf
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param stdDev_gp standard deviation as grid points
     */
    void computeHorizontalBoxBlurSmoothing_GCGridpointsFast(
            const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
            int stdDev_gp, MSmoothProperties::BoundaryModeTypes boundaryType);

    /**
     * @brief computeHorizontalBoxBlurSmoothing_GCGridpointsSlow Convolution of
     * box blur. The result is the same as
     * computeHorizontalBoxBlurSmoothing_GCGridpoints, but the algorithm
     * implementing the box blur is slower. This version of the box blur should
     * be hidden in the GUI.
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param stdDev_gp standard deviation as grid points
     */
    void computeHorizontalBoxBlurSmoothing_GCGridpointsSlow(
            const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
            int stdDev_gp);

    /**
     * @brief computeLatDependentBoxRadii Calculates the size of the boxes for
     * box blur according to the given standard deviation and latitude.
     * For more details see:
     * https://www.peterkovesi.com/papers/FastGaussianSmoothing.pdf
     * "Fast Almost-Gaussian Filtering"
     * Peter Kovesi, Center for Exploration Targeting, School of Earth and
     * Environment, The University of Western Australia
     * @param inputGrid pointer to input grid
     * @param stdDev_km standard deviation in kilometer
     * @param n number of box blur passes
     * @return boxes for box blur of size [lat][n]
     */
    QList<QList<int>> computeLatDependentBoxRadii(
            const MStructuredGrid *inputGrid, float stdDev_km, int n);

    /**
     * @brief numGridpointsSpannedByDistance Converts given distance [km]
     * into grid points.
     * @param inputGrid pointer to input grid
     * @param jLat latitude
     * @param distance in kilometer
     * @return converted distance from km to grid points (nearest grid point)
     */
    int numGridpointsSpannedByDistance(
            const MStructuredGrid *inputGrid, int jLat, float distance_km);

    /**
     * @brief computeBoxRadii Calculates the size of the boxes for
     * box blur according to the given standard deviation in grid points.
     * last box (e.g. 3 passes) is slightly larger than first two
     * boxes (if you use 3 passes (n=3)). This reduces the absolute error of the
     * box blur compared to Gaussian blur.
     * For more details see:
     * https://www.peterkovesi.com/papers/FastGaussianSmoothing.pdf
     * "Fast Almost-Gaussian Filtering"
     * Peter Kovesi, Center for Exploration Targeting, School of Earth and
     * Environment, The University of Western Australia
     * @param stdDev_gp standard deviation in grid points
     * @param n number of box blur passes
     * @return QVector of integers with box size for each pass
     */
    QVector<int> computeBoxRadii(int stdDev_gp, int n);


    /**
     * @brief boxBlurTotalFast Function which calls longitudinal and latitudinal
     * box blur smoothing (helper function for convolution).
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param lonBoxRadius size of latitudinal box
     * @param latDependentBoxRadii sizes of latitude dependent longitude boxes
     */
    void boxBlurTotalFast(
            const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
            int lonBoxRadius, QList<int> latDependentBoxRadii,
            MSmoothProperties::BoundaryModeTypes boundaryType);

    /**
     * @brief boxBlurTotalFast Function which calls longitudinal and latitudinal
     * box blur (helper function for convolution).
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param boxRadius size of box (in grid points)
     */
    void boxBlurTotalFast(const MStructuredGrid *inputGrid,
                          MStructuredGrid *resultGrid, int boxRadius,
                          MSmoothProperties::BoundaryModeTypes boundaryType);

    /**
     * @brief boxBlurLongitudinalFast Implements the longitudinal box blur with a
     * very efficient algorithm and with lat dependent box size.
     * Idea by Wojciech Jarosz, see: ACM SIGGRAPH@UIUC ACM SIGGRAPH@UIUC
     * Fast Image Convolutions, https://web.archive.org/web/20060718054020/
     * http://www.acm.uiuc.edu/siggraph/workshops/wjarosz_convolution_2001.pdf
     * Includes three different implementations of boundary handling:
     * constant, symmetric and zero-padding. For details see:
     * A Survey of Gaussian Convolution Algorithms by Pascal Getreuer
     * (http://www.ipol.im/pub/art/2013/87/?utm_source=doi)
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param latBoxes QList of lat dependent box size for longitudinal box blur
     */
    void boxBlurLongitudinalFast(
            const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
            QList<int> latDependentBoxRadii,
            MSmoothProperties::BoundaryModeTypes boundaryType);

    /**
     * @brief boxBlurLongitudinalFast Implements the longitudinal box blur with a
     * very efficient algorithm.
     * Idea by Wojciech Jarosz, see: ACM SIGGRAPH@UIUC ACM SIGGRAPH@UIUC
     * Fast Image Convolutions, https://web.archive.org/web/20060718054020/
     * http://www.acm.uiuc.edu/siggraph/workshops/wjarosz_convolution_2001.pdf
     * Includes three different implementations of boundary handling:
     * constant, symmetric and zero-padding. For details see:
     * A Survey of Gaussian Convolution Algorithms by Pascal Getreuer
     * (http://www.ipol.im/pub/art/2013/87/?utm_source=doi)
     * @param inputGrid pointer to input grid.
     * @param resultGrid pointer to result grid.
     * @param boxRadius size of box.
     */
    void boxBlurLongitudinalFast(const MStructuredGrid *inputGrid,
                      MStructuredGrid *resultGrid, int boxRadius,
                      MSmoothProperties::BoundaryModeTypes boundaryType);

    /**
     * @brief boxBlurLatitudinalFast Implements the latitudinal box blur with a
     * very efficient algorithm and with lat dependent box size.
     * Idea by Wojciech Jarosz, see: ACM SIGGRAPH@UIUC ACM SIGGRAPH@UIUC
     * Fast Image Convolutions, https://web.archive.org/web/20060718054020/
     * http://www.acm.uiuc.edu/siggraph/workshops/wjarosz_convolution_2001.pdf
     * Includes three different implementations of boundary handling:
     * constant, symmetric and zero-padding. For details see:
     * A Survey of Gaussian Convolution Algorithms by Pascal Getreuer
     * (http://www.ipol.im/pub/art/2013/87/?utm_source=doi)
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param boxRadius box size
     */
    void boxBlurLatitudinalFast(
            const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
            int boxRadius, MSmoothProperties::BoundaryModeTypes boundaryType);

    /**
     * @brief createIndexList creates a list of indices for symmetric boundary
     * conditions. This index list includes the symmetric indices of the
     * boundaries.
     * @param inputGrid pointer to input grid.
     * @param boxRadius size of box.
     * @param dir direction of box blur convolution
     * @return QVector<int> with indices for smoothing with symmmetric boundary
     * conditions.
     */
    QVector<int> createIndexList(const MStructuredGrid *inputGrid, int boxRadius,
                         QString dir);

    /**
     * @brief boxBlurTotalSlow this is the easiest implementation of box blur
     * but with the same result as boxBlurTotalFast, but the implemented
     * algorithm is slower. This version of the box blur should be hidden in
     * the GUI.
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param boxRadius box size
     */
    void boxBlurTotalSlow(const MStructuredGrid *inputGrid,
                          MStructuredGrid *resultGrid, int boxRadius);


//*************************** UNIFORM WEIGHTED SMOOTHING ***********************

    /**
     * @brief computeHorizontalUniformWeightedSmoothing_GCGridpoints Simple
     * smoothing algorithm using uniform weights. All grid points within the
     * radius (standard deviation) are considered and equally weighted.
     * @param inputGrid pointer to input grid
     * @param resultGrid pointer to result grid
     * @param radius_gp radius in grid points (in GUI standard deviation)
     */
    void computeHorizontalUniformWeightedSmoothing_GCGridpoints(
            const MStructuredGrid *inputGrid, MStructuredGrid *resultGrid,
            int radius_gp);
};

} // namespace Met3D

#endif // SMOOTHFILTER_H
