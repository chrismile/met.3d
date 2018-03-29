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
#include "multivarpartialderivativefilter.h"

// standard library imports
#include <bitset>
#include <fstream>

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <omp.h>

// local application imports


namespace Met3D
{

/******************************************************************************
***                             MMultiVarFilter                             ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MMultiVarFilter::MMultiVarFilter()
    : MStructuredGridEnsembleFilter()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MTask* MMultiVarFilter::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    const QStringList vars = rh.value("MULTI_VARIABLES").split("/");

    rh.removeAll(locallyRequiredKeys());

    // Before proceeding with this task, obtain all required variables.
    foreach (const QString var, vars)
    {
        rh.insert("VARIABLE", var);
        task->addParent(inputSource->getTaskGraph(rh.request()));
    }

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/


const QStringList MMultiVarFilter::locallyRequiredKeys()
{
    return (QStringList() << "MULTI_VARIABLES");
}


/******************************************************************************
***                    MMultiVarPartialDerivativeFilter                     ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MMultiVarPartialDerivativeFilter::MMultiVarPartialDerivativeFilter()
    : MMultiVarFilter()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/


void MMultiVarPartialDerivativeFilter::setGeoPotInputSource(
            MWeatherPredictionDataSource* s)
{
    geoPotSource = s;
    registerInputSource(geoPotSource);
    enablePassThrough(s);
}


MStructuredGrid* MMultiVarPartialDerivativeFilter::produceData(
        MDataRequest request)
{
    assert (inputSource != nullptr);
    assert (geoPotSource != nullptr);

    MStructuredGrid *result = nullptr;

    MDataRequestHelper rh(request);
    // !TODO: We assume that only two variables are passed using the same
    // !TODO: init/valid time, member, and leveltype

    // Obtain the type of derivative for the given input source.
    const QString derivOps = rh.value("MULTI_DERIVATIVE_OPS");
    const QStringList ops = derivOps.split("#");

    // Obtain the name of variables that should be differentiated.
    const QStringList vars = rh.value("MULTI_VARIABLES").split("/");

    //const int levelType = rh.value("LEVELTYPE").toInt();

    // Obtain the name of the variable representing the actual geometric height
    // (z).
    QString varGeoP = rh.value("MULTI_GEOPOTENTIAL");

    // Geopotential height: type = 0 | geopotential: type = 1 (requires
    // dividing by 9.81)
    double geoPotScale =
            (rh.value("MULTI_GEOPOTENTIAL_TYPE").toInt() == 1) ? 9.81 : 1;

    // Remove all keys required for this filter.
    rh.removeAll(locallyRequiredKeys());

    MDataRequestHelper rhVar(rh);
    // Get the first data (u-component).
    rhVar.insert("VARIABLE", vars[0]);
    MStructuredGrid *gridU = inputSource->getData(rhVar.request());

    // And the second data (v-component).
    rhVar.insert("VARIABLE", vars[1]);
    MStructuredGrid *gridV = inputSource->getData(rhVar.request());

    MStructuredGrid* gridGeoP = nullptr;

    if (derivOps.contains("dz") && varGeoP != "")
    {
        // And the geopotential data (geopotential, ...).
        rhVar.insert("VARIABLE", varGeoP);
        gridGeoP = geoPotSource->getData(rhVar.request());
    }

    // If the grids are not available, return a nullptr.
    if (!gridU || !gridV)
    {
        return nullptr;
    }

    // Create a new grid with the same topology as the input grid.
    result = createAndInitializeResultGrid(gridU);
    //result->setGeneratingRequest(request);

    // Compute the distances between two grid points in longitude and latitude
    // direction.
    const float dx =
            static_cast<float>(gridU->getLons()[1] - gridU->getLons()[0]);
    const float dy =
            static_cast<float>(gridU->getLats()[1] - gridU->getLats()[0]);

    // Actual geometric distance between two grids in latitudinal direction
    // ~111km.
    const double deltaLat = 1.112e5;

    // Derive the input grid by its wind vector normal.
    const auto derivativeWrtNormal = [&](const QString& method,
                                         MStructuredGrid* inputGrid)
    {
#pragma omp parallel for
        for (int k = 0; k < static_cast<int>(result->getNumLevels()); ++k)
        {
            for (int j = 0; j < static_cast<int>(result->getNumLats()); ++j)
            {
                for (int i = 0; i < static_cast<int>(result->getNumLons()); ++i)
                {
                    // Current wind vector at sample grid point (k, j, i).
                    QVector2D V;

                    // Obtain the tangent at the grid point.
                    V.setX(gridU->getValue(k, j, i));
                    V.setY(gridV->getValue(k, j, i));

                    // The current velocity.
                    float Vs = V.length();

                    // The vector normal to V = s.
                    QVector2D n;

                    // Create the 2D-normal perpendicular to the tangent.
                    n.setX(-V.y());
                    n.setY(V.x());
                    n.normalize();

                    // Set vector s parallel to the tangent (V).
                    QVector2D s = V;
                    s.normalize();

                    // Compute the indices of the surrounding grid points.
                    int iPrev = std::max(i - 1, 0);
                    int iNext = std::min(i + 1, int(result->getNumLons() - 1));
                    int jPrev = std::max(j - 1, 0);
                    int jNext = std::min(j + 1, int(result->getNumLats() - 1));

                    // Get the velocities of the 4 surrounding points and
                    // resolve the velocities into the positive direction of s.
                    // Compute V_s at all surrounding points.
                    double VsPrevLon = computeVs(gridU, gridV, k, j, iPrev, s);
                    double VsNextLon = computeVs(gridU, gridV, k, j, iNext, s);
                    double VsPrevLat = computeVs(gridU, gridV, k, jPrev, i, s);
                    double VsNextLat = computeVs(gridU, gridV, k, jNext, i, s);

                    // Compute the first derivatives d/dx and d/dy.
                    double deltaX = dx * (iNext - iPrev)
                                    * cos(gridU->getLats()[j] / 180.0 * M_PI)
                            * deltaLat;
                    double deltaY = dy * (jNext - jPrev) * deltaLat;

                    double dVsdx = (VsNextLon - VsPrevLon) / deltaX;
                    double dVsdy = (VsNextLat - VsPrevLat) / deltaY;

                    // Contains the value of the current derivative.
                    double deriv = 0;

                    if (method == "ddn")
                    {
                        // Compute the first derivative in direction n.
                        deriv = (n.x() * dVsdx + n.y() * dVsdy);

                    }

                    else if (method == "d2dn2")
                    {
                        // Get the velocities of the 4 surrounding points along
                        // the diagonal (X-neighbours) and resolve the
                        // velocities into the positive direction of s.
                        double VsINJN =
                                computeVs(gridU, gridV, k, jNext, iNext, s);
                        double VsIPJN =
                                computeVs(gridU, gridV, k, jNext, iPrev, s);
                        double VsINJP =
                                computeVs(gridU, gridV, k, jPrev, iNext, s);
                        double VsIPJP =
                                computeVs(gridU, gridV, k, jPrev, iPrev, s);

                        double dVs2dx2 = (VsNextLon - 2 * Vs + VsPrevLon)
                                / (deltaX * deltaX / 4);
                        double dVs2dy2 = (VsNextLat - 2 * Vs + VsPrevLat)
                                / (deltaY * deltaY / 4);
                        double dVs2dxdy = (VsINJN - VsIPJN - VsINJP + VsIPJP)
                                / (deltaX * deltaY);

                        // Compute the second derivative in direction n.
                        deriv = n.x() * n.x() * dVs2dx2
                                + 2 * n.x() * n.y() * dVs2dxdy
                                + n.y() * n.y() * dVs2dy2;
                    }
                    // Mixed-partial derivative.
                    else if (method == "d2dndp" || method == "d2dndz")
                    {
                        int kNext = std::min(k + 1,
                                             int(gridU->getNumLevels() - 1));
                        int kPrev = std::max(k - 1, 0);

                        double dp = 0;

                        if (gridGeoP && method == "d2dndz")
                        {
                            const double geoHeightPrev = gridGeoP->getValue(
                                        kPrev, j, i) / geoPotScale;
                            const double geoHeightNext = gridGeoP->getValue(
                                        kNext, j, i) / geoPotScale;

                            dp = geoHeightNext - geoHeightPrev;
                        }
                        else
                        {
                            dp = gridU->getPressure(kNext, j, i)
                                    - gridU->getPressure(kPrev, j, i);
                        }

                        // Get the velocities of the 8 surrounding points along
                        // the diagonal (X-neighbours) for the adjacent model
                        // level and resolve the velocities into the positive
                        // direction of s.
                        double vsKPJIP =
                                computeVs(gridU, gridV, kPrev, j, iPrev, s);
                        double vsKNJIP =
                                computeVs(gridU, gridV, kNext, j, iPrev, s);

                        double vsKPJIN =
                                computeVs(gridU, gridV, kPrev, j, iNext, s);
                        double vsKNJIN =
                                computeVs(gridU, gridV, kNext, j, iNext, s);

                        double vsKPJPI =
                                computeVs(gridU, gridV, kPrev, jPrev, i, s);
                        double vsKNJPI =
                                computeVs(gridU, gridV, kNext, jPrev, i, s);

                        double vsKPJNI =
                                computeVs(gridU, gridV, kPrev, jNext, i, s);
                        double vsKNJNI =
                                computeVs(gridU, gridV, kNext, jNext, i, s);

                        // Compute the mixed partial derivative.
                        double d2Vsdpdx =
                                (vsKNJIN - vsKPJIN - vsKNJIP + vsKPJIP)
                                / (deltaX * dp);
                        double d2Vsdpdy =
                                (vsKNJNI - vsKPJNI - vsKNJPI + vsKPJPI)
                                / (deltaY * dp);

                        deriv = n.x() * d2Vsdpdx + n.y() * d2Vsdpdy;
                    }

                    result->setValue(k, j, i, static_cast<float>(deriv));
                }
            }
        }
    };

    // Derive the input grid by the geometric height / z.
    const auto derivativeWrtHeight = [&](const QString& method,
                                         MStructuredGrid* inputGrid)
    {
        assert(gridGeoP != nullptr);

        double deriv = 0;

#pragma omp parallel for
        for (unsigned int j = 0; j < result->getNumLats(); ++j)
        {
            for (unsigned int i = 0; i < result->getNumLons(); ++i)
            {
                for (unsigned int k = 0; k < result->getNumLevels(); ++k)
                {
                    const unsigned int kNext =
                            std::min(k + 1, result->getNumLevels() - 1);
                    const unsigned int kPrev =
                            static_cast<unsigned int>(
                                std::max(static_cast<int>(k - 1), 0));

                    // Current wind vector at sample grid point (k, j, i).
                    QVector2D V;

                    // Obtain the tangent at the grid point.
                    V.setX(gridU->getValue(k, j, i));
                    V.setY(gridV->getValue(k, j, i));

                    // Compute the current velocity.
                    float Vs = V.length();

                    // Set vector s parallel to the tangent (V).
                    QVector2D s = V;
                    s.normalize();

                    // Get the velocities of the 2 surrounding points along the
                    // model levels and resolve the velocities into the
                    // positive direction of s.
                    double VsPrevLev = computeVs(gridU, gridV, kPrev, j, i, s);
                    double VsNextLev = computeVs(gridU, gridV, kNext, j, i, s);

                    // Obtain geopotential height in m/s for every grid point.
                    double geoHeightPrev =
                            gridGeoP->getValue(kPrev, j, i) / geoPotScale;
                    double geoHeightNext =
                            gridGeoP->getValue(kNext, j, i) / geoPotScale;

                    // Compute the height distance between the two grid points.
                    float deltaZ = geoHeightNext - geoHeightPrev;

                    if (method == "ddz")
                    {
                        // Compute the first derivative with central differences.
                        deriv = (VsNextLev -  VsPrevLev) / (deltaZ);
                    }
                    else if (method == "d2dz2")
                    {
                        // Compute the second derivative.
                        deriv = (VsNextLev - 2 * Vs + VsPrevLev)
                                / (deltaZ * deltaZ / 4);
                    }

                    result->setValue(k, j, i, deriv);
                }
            }
        }
    };

    // Derive the input grid by pressure.
    const auto derivativeWrtPressure = [&](const QString& method,
                                           MStructuredGrid* inputGrid)
    {
        double deriv = 0;

#pragma omp parallel for
        for (unsigned int j = 0; j < result->getNumLats(); ++j)
        {
            for (unsigned int i = 0; i < result->getNumLons(); ++i)
            {
                for (unsigned int k = 0; k < result->getNumLevels(); ++k)
                {
                    const unsigned int kNext =
                            std::min(k + 1, result->getNumLevels() - 1);
                    const unsigned int kPrev =
                            static_cast<unsigned int>(
                                std::max(static_cast<int>(k - 1), 0));

                    // Current wind vector at sample grid point (k, j, i).
                    QVector2D V;

                    // Obtain the tangent at the grid point.
                    V.setX(gridU->getValue(k, j, i));
                    V.setY(gridV->getValue(k, j, i));

                    // Compute the current velocity.
                    double Vs = V.length();

                    // Set vector s parallel to the tangent (V).
                    QVector2D s = V;
                    s.normalize();

                    // Get the velocities of the 2 surrounding points along the
                    // model levels and resolve the velocities into the positive
                    // direction of s.
                    double VsPrevLev = computeVs(gridU, gridV, kPrev, j, i, s);
                    double VsNextLev = computeVs(gridU, gridV, kNext, j, i, s);

                    // Set deltaZ according to boundary conditions.
                    const float dp = gridU->getPressure(kNext, j, i)
                                     - gridU->getPressure(kPrev, j, i);

                    if (method == "ddp")
                    {
                        deriv = (VsNextLev - VsPrevLev) / dp;
                    }
                    // Assume that deltaPressure = h / 2
                    else if (method == "d2dp2")
                    {
                        deriv = (VsNextLev - 2 * Vs + VsPrevLev) / (dp * dp / 4);
                    }

                    result->setValue(k, j, i, deriv);
                }
            }
        }
    };

    result->setToZero();

    const QString& op = ops[0];

    if (op.contains("dn"))
    {
        derivativeWrtNormal(op, result);
    }
    else if (op.contains("dz"))
    {
        derivativeWrtHeight(op, result);
    }
    else if (op.contains("dp"))
    {
        derivativeWrtPressure(op, result);
    }

    // Release the recently created grids to reduce memory consumption.
    inputSource->releaseData(gridU);
    inputSource->releaseData(gridV);

    if (gridGeoP) { geoPotSource->releaseData(gridGeoP); }

    return result;
}


MTask* MMultiVarPartialDerivativeFilter::createTaskGraph(MDataRequest request)
{
    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);

    const QString derivOps = rh.value("MULTI_DERIVATIVE_OPS");
    const QStringList ops = derivOps.split("#");

    const QStringList vars = rh.value("MULTI_VARIABLES").split("/");

    QString varGeoP = rh.value("MULTI_GEOPOTENTIAL");

    rh.removeAll(locallyRequiredKeys());

    if (derivOps.contains("dz") && varGeoP != "")
    {
        rh.insert("VARIABLE", varGeoP);
        task->addParent(geoPotSource->getTaskGraph(rh.request()));
    }

    // Before proceeding with this task, obtain all required variables.
    for (const auto& var : vars)
    {
        rh.insert("VARIABLE", var);
        task->addParent(inputSource->getTaskGraph(rh.request()));
    }

    return task;
}


double MMultiVarPartialDerivativeFilter::computeVs(
        MStructuredGrid* gridU,
        MStructuredGrid* gridV,
        int k, int j, int i, const QVector2D& s) const
{
    QVector2D v(gridU->getValue(k, j, i),
                gridV->getValue(k, j, i));

    return QVector2D::dotProduct(v, s);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MMultiVarPartialDerivativeFilter::locallyRequiredKeys()
{
    return (QStringList() << "MULTI_VARIABLES"
                          << "MULTI_DERIVATIVE_OPS"
                          << "MULTI_GEOPOTENTIAL"
                          << "MULTI_GEOPOTENTIAL_TYPE");
}


/******************************************************************************
***                               MBlurFilter                               ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MBlurFilter::MBlurFilter()
        : MStructuredGridEnsembleFilter()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MStructuredGrid* MBlurFilter::produceData(MDataRequest request)
{
    MDataRequestHelper rh(request);

    const QString filterType = rh.value("BLUR_FILTERTYPE");
    const unsigned int kernelSize = rh.value("BLUR_KERNEL_SIZE").toUInt();
    const float sigma = rh.value("BLUR_SIGMA").toFloat();

    // Remove all keys required for this filter.
    rh.removeAll(locallyRequiredKeys());

    MStructuredGrid* inputGrid = inputSource->getData(rh.request());

    // If we haven't found the variables, then return a nullptr.
    if (!inputGrid)
    {
        return nullptr;
    }

    // Create a new grid with the same topology as the input grid.
    MStructuredGrid* result = createAndInitializeResultGrid(inputGrid);
    QVector<float> intermediate(result->getNumValues());

    const auto setIntermediateValue =
            [&](const int k, const int j, const int i, const float value)
    {
        intermediate[k * result->getNumLats() * result->getNumLons()
                     + j * result->getNumLons() + i] = value;
    };

    const auto getIntermediateValue = [&](const int k, const int j, const int i)
    {
        return intermediate[k * result->getNumLats() * result->getNumLons()
                            + j * result->getNumLons() + i];
    };

    const auto computeKernel1D = [](const double sigma,
                                    const unsigned int kernelSize)
    {
        QVector<double> kernel(kernelSize);

        const int offset = kernelSize / 2;
        const double s = 2 * sigma * sigma;
        const double sqrtPISigma = std::sqrt(2 * M_PI) * sigma;

        double minGauss = std::numeric_limits<double>::max();

        for (int x = -offset; x <= offset; ++x)
        {
            int i = x + offset;
            const double r = x * x;

            const double gauss = std::exp(-r / s) / sqrtPISigma;
            kernel[i] = gauss;
            minGauss = std::min(gauss, minGauss);
        }

        const double scaleFactor = 1.0 / minGauss;

        for (int i = 0; i < kernel.size(); ++i)
        {
            kernel[i] = std::floor(kernel[i] * scaleFactor);
        }

        return kernel;
    };

    QVector<double> kernel = computeKernel1D(sigma, kernelSize);

    const int offset = kernelSize / 2;

    for (unsigned int k = 0; k < inputGrid->getNumLevels(); ++k)
    {
        for (unsigned int j = 0; j < inputGrid->getNumLats(); ++j)
        {
            // Since the gauss filter is separable, first apply the filter in
            // x-direction (in 1D)...
            for (unsigned int i = 0; i < inputGrid->getNumLons(); ++i)
            {
                double kernelSum = 0;
                double value = 0;

                for (int offsetX = -offset; offsetX <= offset; ++offsetX)
                {
                    int iN = i + offsetX;

                    if (iN < 0
                            || iN >= static_cast<int>(inputGrid->getNumLons()))
                    {
                        continue;
                    }

                    double weight = kernel[offsetX + offset];
                    value += weight * inputGrid->getValue(k, j, iN);
                    kernelSum += weight;
                }

                setIntermediateValue(k, j, i,
                                     static_cast<float>(value / kernelSum));
            }
        }

        // ...and then apply the filter in y-direction to the intermediate
        // result.
        for (unsigned int j = 0; j < inputGrid->getNumLats(); ++j)
        {
            for (unsigned int i = 0; i < inputGrid->getNumLons(); ++i)
            {
                double kernelSum = 0;
                double value = 0;

                for (int offsetY = -offset; offsetY <= offset; ++offsetY)
                {
                    int jN = j + offsetY;

                    if (jN < 0
                            || jN >= static_cast<int>(inputGrid->getNumLats()))
                    { continue; }

                    double weight = kernel[offsetY + offset];
                    value += weight * getIntermediateValue(k, jN, i);
                    kernelSum += weight;
                }

                result->setValue(k, j, i,
                                 static_cast<float>(value / kernelSum));
            }
        }
    }

    inputSource->releaseData(inputGrid);

    return result;
}


MTask* MBlurFilter::createTaskGraph(MDataRequest request)
{
    MTask* task = new MTask(request, this);

    MDataRequestHelper rh(request);
    rh.removeAll(locallyRequiredKeys());

    task->addParent(inputSource->getTaskGraph(rh.request()));

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MBlurFilter::locallyRequiredKeys()
{
    return (QStringList() << "BLUR_FILTERTYPE" << "BLUR_KERNEL_SIZE"
                          << "BLUR_SIGMA");
}

} // namespace Met3D
