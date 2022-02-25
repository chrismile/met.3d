/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020-2021 Christoph Neuhauser
**  Copyright 2020      Michael Kern
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
#include "beziertrajectoriessource.h"

// standard library imports
#include <algorithm>
#include <cassert>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "gxfw/msceneviewglwidget.h"
#include "beziercurve.hpp"

using namespace std;


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MBezierTrajectoriesSource::MBezierTrajectoriesSource()
        : MScheduledDataSource(),
          trajectorySource(nullptr)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

double distance(const QVector<float>& v1, const QVector<float>& v2)
{
    int numVariables = std::min(v1.size(), v2.size());
    double differenceSum = 0.0f;
    for (int varIdx = 0; varIdx < numVariables; varIdx++)
    {
        float valueOld = v1[varIdx];
        float valueNew = v2[varIdx];
        differenceSum += double(valueNew - valueOld) * double(valueNew - valueOld);
    }
    return std::sqrt(differenceSum);
}

MBezierTrajectories *MBezierTrajectoriesSource::produceData(MDataRequest request)
{
    assert(trajectorySource != nullptr);

    LOG4CPLUS_DEBUG(mlog, "computing bezier trajectories..");

    MDataRequestHelper rh(request);

    QStringList args = rh.value("BEZIERTRAJECTORIES_LOGP_SCALED").split("/");
    double log_pBottom_hPa  = args[0].toDouble();
    double deltaZ_deltaLogP = args[1].toDouble();

    rh.remove("BEZIERTRAJECTORIES_LOGP_SCALED");
    MTrajectories *inTrajectories = trajectorySource->getData(rh.request());

    MFilteredTrajectories filteredTrajectories;
    filteredTrajectories.reserve(inTrajectories->getNumTrajectories());

    QVector<int> indicesToFilteredIndicesMap;
    indicesToFilteredIndicesMap.reserve(inTrajectories->getNumTrajectories());

    // Loop over all trajectories and compute normals for each of their vertices.
    int numTrajectories = inTrajectories->getNumTrajectories();
    int numTimeStepsPerTrajectory = inTrajectories->getNumTimeStepsPerTrajectory();
    QVector<QVector3D> vertices = inTrajectories->getVertices();
    const uint32_t numVariablesReal = inTrajectories->getAuxDataVarNames().size();
    uint32_t numVariables = numVariablesReal + 1;
    //const uint32_t numVariables = std::max(numVariablesReal, uint32_t(1));
    this->numVariables = numVariables;

    const QStringList& auxDataVarNames = inTrajectories->getAuxDataVarNames();
    bool hasSensitivityData = false;
    QVector<int> sensitivityIndices;
    int auxVarIdx = 0;
    for (const QString& varName : auxDataVarNames)
    {
        if (varName.startsWith('d') && varName == "deposition")
        {
            hasSensitivityData = true;
            sensitivityIndices.push_back(auxVarIdx + 1);
        }
        auxVarIdx++;
    }

    for (int i = 0; i < numTrajectories; i++)
    {
        int baseIndex = i * numTimeStepsPerTrajectory;

        // Prevent "out of bound exception" ("vertices" are accessed at "baseIndex+1").
        if (baseIndex+1 >= vertices.size())
        {
            continue;
        }

        MFilteredTrajectory filteredTrajectory;
        filteredTrajectory.attributes.resize(int(numVariables) + (hasSensitivityData ? 1 : 0));

        QVector3D prevPoint(M_INVALID_TRAJECTORY_POS, M_INVALID_TRAJECTORY_POS, M_INVALID_TRAJECTORY_POS);
        for (int t = 0; t < numTimeStepsPerTrajectory; t++)
        {
            QVector3D point = vertices.at(baseIndex+t);

            if (point.z() == M_INVALID_TRAJECTORY_POS)
            {
                continue;
            }
            if ((point - prevPoint).length() < 1e-5)
            {
                continue;
            }

            // Use pressure as attribute if no auxiliary data is available.
            filteredTrajectory.attributes[0].push_back(point.z());
            if (numVariablesReal > 0)
            {
                QVector<float> vertexAttributes = inTrajectories->getAuxDataAtVertex(baseIndex+t);
                for (uint32_t j = 0; j < numVariablesReal; j++)
                {
                    filteredTrajectory.attributes[j + 1].push_back(vertexAttributes.at(j));
                }
            }
            //else
            //{
            //    // Use pressure as attribute if no auxiliary data is available.
            //    for (uint32_t j = 0; j < numVariables; j++)
            //    {
            //        filteredTrajectory.attributes[j].push_back(point.z());
            //    }
            //}

            prevPoint = point;
            point.setZ(MSceneViewGLWidget::worldZfromPressure(
                    point.z(), log_pBottom_hPa, deltaZ_deltaLogP));
            filteredTrajectory.positions.push_back(point);
        }

        if (filteredTrajectory.positions.size() >= 2)
        {
            indicesToFilteredIndicesMap.push_back(filteredTrajectories.size());
            filteredTrajectories.push_back(filteredTrajectory);
        }
        else
        {
            indicesToFilteredIndicesMap.push_back(-1);
        }
    }


    if (hasSensitivityData)
    {
        for (int trajectoryIdx = 0; trajectoryIdx < filteredTrajectories.size(); trajectoryIdx++)
        {
            MFilteredTrajectory& filteredTrajectory = filteredTrajectories[trajectoryIdx];
            QVector<float>& maxSensitivityAttributes = filteredTrajectory.attributes.back();
            maxSensitivityAttributes.reserve(numTimeStepsPerTrajectory);
            for (int timeStepIdx = 0; timeStepIdx < numTimeStepsPerTrajectory; timeStepIdx++)
            {
                bool hasValidData = false;
                float maxSensitivity = std::numeric_limits<float>::lowest();
                for (int varIdx : sensitivityIndices)
                {
                    float sensitivityValue = filteredTrajectory.attributes.at(varIdx).at(timeStepIdx);
                    if (!std::isnan(sensitivityValue))
                    {
                        maxSensitivity = std::max(maxSensitivity, sensitivityValue);
                        hasValidData = true;
                    }
                }
                if (!hasValidData) {
                    maxSensitivity = std::numeric_limits<float>::quiet_NaN();
                }
                maxSensitivityAttributes.push_back(maxSensitivity);
            }
        }
    }
    numVariables++;


    // k-means clustering.
    /*const int K = 2;
    const int maxIterations = 1000;
    QVector<QVector<float>> clusterCenters;
    QVector<QVector<double>> clusterElementsMeans;
    QVector<int> clustersNumElements;
    clusterCenters.resize(K);
    clusterElementsMeans.resize(K);
    clustersNumElements.resize(K);
    std::mt19937 generator(2);
    std::uniform_real_distribution<> dis(0, 1);
    for (int clusterIdx = 0; clusterIdx < K; clusterIdx++)
    {
        QVector<float>& clusterCenter = clusterCenters[clusterIdx];
        clusterCenter.reserve(numVariables);
        clusterElementsMeans[clusterIdx].resize(numVariables);
        for (int varIdx = 0; varIdx < int(numVariables); varIdx++)
        {
            clusterCenter.push_back(dis(generator));
        }
    }
    for (int timeStepIdx = 0; timeStepIdx < numTimeStepsPerTrajectory; timeStepIdx++)
    {
        QVector<QVector<float>> parameterVectors;
        parameterVectors.resize(filteredTrajectories.size());
        for (int trajectoryIdx = 0; trajectoryIdx < filteredTrajectories.size(); trajectoryIdx++)
        {
            const MFilteredTrajectory& filteredTrajectory = filteredTrajectories[trajectoryIdx];
            QVector<float>& parameterVector = parameterVectors[trajectoryIdx];
            parameterVector.reserve(numVariables);
            for (int varIdx = 0; varIdx < int(numVariables); varIdx++)
            {
                parameterVector.push_back(filteredTrajectory.attributes.at(varIdx).at(timeStepIdx));
            }
        }

        QVector<int> trajectoryClusterIndices;
        trajectoryClusterIndices.resize(filteredTrajectories.size());
        QVector<QVector<float>> clusterCentersOld = clusterCenters;
        for (int iteration = 0; iteration < maxIterations; iteration++) {
            // Get the cluster index for each trajectory.
            for (int trajectoryIdx = 0; trajectoryIdx < filteredTrajectories.size(); trajectoryIdx++)
            {
                int& trajectoryClusterIndex = trajectoryClusterIndices[trajectoryIdx];
                const QVector<float>& parameterVector = parameterVectors[trajectoryIdx];
                double clusterDistanceMin = std::numeric_limits<double>::max();
                for (int clusterIdx = 0; clusterIdx < K; clusterIdx++)
                {
                    QVector<float>& clusterCenter = clusterCenters[clusterIdx];
                    double clusterDistance = distance(parameterVector, clusterCenter);
                    if (clusterDistance < clusterDistanceMin)
                    {
                        clusterDistanceMin = clusterDistance;
                        trajectoryClusterIndex = clusterIdx;
                    }
                }
            }

            // Update the cluster center positions.
            clusterCentersOld = clusterCenters;
            for (int clusterIdx = 0; clusterIdx < K; clusterIdx++)
            {
                clustersNumElements[clusterIdx] = 0;
                QVector<double>& clusterElementsMean = clusterElementsMeans[clusterIdx];
                for (int varIdx = 0; varIdx < int(numVariables); varIdx++)
                {
                    clusterElementsMean[varIdx] = 0.0;
                }
            }
            for (int trajectoryIdx = 0; trajectoryIdx < filteredTrajectories.size(); trajectoryIdx++)
            {
                QVector<float>& parameterVector = parameterVectors[trajectoryIdx];
                int& trajectoryClusterIndex = trajectoryClusterIndices[trajectoryIdx];
                clustersNumElements[trajectoryClusterIndex]++;
                QVector<float>& clusterCenter = clusterCenters[trajectoryClusterIndex];
                QVector<double>& clusterElementsMean = clusterElementsMeans[trajectoryClusterIndex];
                for (int varIdx = 0; varIdx < int(numVariables); varIdx++)
                {
                    clusterElementsMean[varIdx] += parameterVector[varIdx];
                }
            }
            for (int clusterIdx = 0; clusterIdx < K; clusterIdx++)
            {
                QVector<float>& clusterCenter = clusterCenters[clusterIdx];
                if (clustersNumElements[clusterIdx] == 0)
                {
                    for (int varIdx = 0; varIdx < int(numVariables); varIdx++)
                    {
                        clusterCenter[varIdx] = dis(generator);
                    }
                }
                else
                {
                    double numElements = double(clustersNumElements[clusterIdx]);
                    QVector<double>& clusterElementsMean = clusterElementsMeans[clusterIdx];
                    for (int varIdx = 0; varIdx < int(numVariables); varIdx++)
                    {
                        clusterCenter[varIdx] = float(clusterElementsMean[varIdx] / numElements);
                    }
                }
            }

            // Check whether the cluster centers no longer sufficiently changed.
            double differenceClusters = 0.0f;
            for (int clusterIdx = 0; clusterIdx < K; clusterIdx++)
            {
                QVector<float>& clusterCenterOld = clusterCentersOld[clusterIdx];
                QVector<float>& clusterCenter = clusterCenters[clusterIdx];
                differenceClusters += distance(clusterCenterOld, clusterCenter) / float(numVariables * K);
            }

            if (differenceClusters < 1e-5) {
                break;
            }
        }

        for (int trajectoryIdx = 0; trajectoryIdx < filteredTrajectories.size(); trajectoryIdx++)
        {
            MFilteredTrajectory& filteredTrajectory = filteredTrajectories[trajectoryIdx];
            filteredTrajectory.attributes.back().push_back(float(trajectoryClusterIndices.at(trajectoryIdx)));
        }
    }
    numVariables++;*/


    // 1) Determine Bezier segments
    std::vector<std::vector<MBezierCurve>> curves(filteredTrajectories.size());
    // Store the arclength of all segments along a curve
    std::vector<float> curveArcLengths(filteredTrajectories.size(), 0.0f);

    // Average segment length;
    float avgSegLength = 0.0f;
    float minSegLength = std::numeric_limits<float>::max();
    float numSegments = 0;

    int32_t trajCounter = 0;
    for (const auto& trajectory : filteredTrajectories)
    {
        std::vector<MBezierCurve> &curveSet = curves[trajCounter];

        const int maxVertices = trajectory.positions.size();

        float minT = 0.0f;
        float maxT = 1.0f;

        for (int v = 0; v < maxVertices - 1; ++v)
        {
            const QVector3D& pos0 = trajectory.positions[std::max(0, v - 1)];
            const QVector3D& pos1 = trajectory.positions[v];
            const QVector3D& pos2 = trajectory.positions[v + 1];
            const QVector3D& pos3 = trajectory.positions[std::min(v + 2, maxVertices - 1)];

            const QVector3D cotangent1 = (pos2 - pos0).normalized();
            const QVector3D cotangent2 = (pos3 - pos1).normalized();
            const QVector3D tangent = pos2 - pos1;
            float lenTangent = tangent.length();

            if (std::isnan(lenTangent))
            {
                LOG4CPLUS_DEBUG(mlog, "ERROR: NaN encountered in trajectory file.");
                lenTangent = 0.0f;
            }

            avgSegLength += lenTangent;
            minSegLength = std::min(minSegLength, lenTangent);
            numSegments++;

            QVector3D C0 = pos1;
            QVector3D C1 = pos1 + cotangent1 * lenTangent * 0.5f;
            QVector3D C2 = pos2 - cotangent2 * lenTangent * 0.5f;
            QVector3D C3 = pos2;

            MBezierCurve BCurve({{C0, C1, C2, C3}}, minT, maxT);
            if (std::isnan(BCurve.totalArcLength))
            {
                BCurve.totalArcLength = 0.0f;
            }

            curveSet.push_back(BCurve);
            curveArcLengths[trajCounter] += BCurve.totalArcLength;

            minT += 1.0f;
            maxT += 1.0f;
        }

        trajCounter++;
    }

    avgSegLength /= numSegments;

    // 2) Compute min max values of all attributes across all trajectories
    if (numVariables <= 0)
    {
        LOG4CPLUS_DEBUG(mlog, "ERROR: No variable was found in trajectory file.");
        std::exit(-2);
    }

    QVector<QVector2D> attributesMinMax(
            numVariables, QVector2D(std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()));

    const uint32_t numLines = filteredTrajectories.size();

    // 2.5) Create buffer array with all variables and statistics
    QVector<QVector<float>> multiVarData(numLines);
    QVector<MBezierTrajectory::LineDesc> lineDescs(numLines);
    QVector<QVector<MBezierTrajectory::VarDesc>> lineMultiVarDescs(numLines);

    uint32_t lineOffset = 0;
    uint32_t lineID = 0;

    for (const auto& trajectory : filteredTrajectories)
    {
        uint32_t varOffsetPerLine = 0;

        for (uint32_t v = 0; v < numVariables; ++v)
        {
            MBezierTrajectory::VarDesc varDescPerLine = {0};
            varDescPerLine.minMax = QVector2D(
                    std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest());
            varDescPerLine.startIndex = float(varOffsetPerLine);
            varDescPerLine.dummy = 0.0f;

            const QVector<float>& variableArray = trajectory.attributes[v];

            for (const auto &variable : variableArray)
            {
                attributesMinMax[v].setX(std::min(attributesMinMax[v].x(), variable));
                attributesMinMax[v].setY(std::max(attributesMinMax[v].y(), variable));

                varDescPerLine.minMax.setX(std::min(varDescPerLine.minMax.x(), variable));
                varDescPerLine.minMax.setY(std::max(varDescPerLine.minMax.y(), variable));

                multiVarData[lineID].push_back(variable);
            }

            lineMultiVarDescs[lineID].push_back(varDescPerLine);
            varOffsetPerLine += variableArray.size();
        }
        lineDescs[lineID].startIndex = lineOffset;
        lineDescs[lineID].numValues = varOffsetPerLine;

        lineOffset += varOffsetPerLine;
        lineID++;
    }


    // 3) Compute several equally-distributed / equi-distant points along Bezier curves.
    // Store these points in a new trajectory
    bool resetPerSegMode;
    const float minAvgSegLength = 0.1f;
    if (avgSegLength < minAvgSegLength)
    {
        resetPerSegMode = !needsSubdiv;
        if (needsSubdiv)
        {
            avgSegLength = minAvgSegLength;
        }
    }
    else
    {
        resetPerSegMode = true;
    }
    float rollSegLength = avgSegLength / std::min(numVariables, uint32_t(8));// avgSegLength * 0.2f;

    MBezierTrajectories* newTrajectories = new MBezierTrajectories(
            inTrajectories->getGeneratingRequest(),
            filteredTrajectories, indicesToFilteredIndicesMap, numVariables,
            auxDataVarNames);

    for (int32_t traj = 0; traj < int(filteredTrajectories.size()); ++traj)
    {
        float curArcLength = 0.0f;
        MBezierTrajectory newTrajectory;

        // Obtain set of Bezier Curves
        std::vector<MBezierCurve> &BCurves = curves[traj];
        // Obtain total arc length
        const float totalArcLength = curveArcLengths[traj];

        QVector3D pos;
        QVector3D tangent;
        uint32_t lineID = 0;
        uint32_t varIDPerLine = 0;
        // Start with first segment
        BCurves[0].evaluate(0, pos, tangent);

        newTrajectory.positions.push_back(pos);

        // Now we store variable, min, and max, and var ID per vertex as new attributes
        newTrajectory.attributes.resize(9);
        float varValue = filteredTrajectories[traj].attributes[varIDPerLine][lineID];
        newTrajectory.attributes[0].push_back(varValue);
        newTrajectory.attributes[1].push_back(attributesMinMax[varIDPerLine].x());
        newTrajectory.attributes[2].push_back(attributesMinMax[varIDPerLine].y());
        // var ID
        newTrajectory.attributes[3].push_back(static_cast<float>(varIDPerLine));
        // var element index
        newTrajectory.attributes[4].push_back(static_cast<float>(lineID));
        // line ID
        newTrajectory.attributes[5].push_back(static_cast<float>(traj));
        // next line ID
        newTrajectory.attributes[6].push_back(static_cast<float>(std::min(lineID, uint32_t(BCurves.size() - 1))));
        // interpolant t
        newTrajectory.attributes[7].push_back(0.0f);
        // timestep index
        newTrajectory.attributes[8].push_back(0.0f);

        curArcLength += rollSegLength;

        float sumArcLengths = 0.0f;
        float sumArcLengthsNext = BCurves[0].totalArcLength;

        varIDPerLine++;

        while (curArcLength <= totalArcLength)
        {
            // Obtain current Bezier segment index based on arc length
            while (sumArcLengthsNext <= curArcLength)
            {
                if (resetPerSegMode)
                {
                    varIDPerLine = 0;
                }
                lineID++;
                if (lineID >= BCurves.size()) {
                    break;
                }
                sumArcLengths = sumArcLengthsNext;
                sumArcLengthsNext += BCurves[lineID].totalArcLength;
                if (!resetPerSegMode)
                {
                    break;
                }
            }
            if (lineID >= BCurves.size())
            {
                break;
            }

            const auto &BCurve = BCurves[lineID];

            const float _arcLength = curArcLength - sumArcLengths;
            if (_arcLength > BCurve.totalArcLength)
            {
                continue;
            }
            float t = BCurve.solveTForArcLength(_arcLength);
            t = clamp(t, BCurve.minT, BCurve.maxT);

            BCurves[lineID].evaluate(t, pos, tangent);

            newTrajectory.positions.push_back(pos);

            if (varIDPerLine < numVariables)
            {
                float varValue = filteredTrajectories[traj].attributes[varIDPerLine][lineID];
                newTrajectory.attributes[0].push_back(varValue);
                newTrajectory.attributes[1].push_back(attributesMinMax[varIDPerLine].x());
                newTrajectory.attributes[2].push_back(attributesMinMax[varIDPerLine].y());
            }
            else
            {
                newTrajectory.attributes[0].push_back(0.0);
                newTrajectory.attributes[1].push_back(0.0);
                newTrajectory.attributes[2].push_back(0.0);
            }
            if (varIDPerLine < numVariables || !resetPerSegMode)
            {
                newTrajectory.attributes[3].push_back(static_cast<float>(varIDPerLine));
            }
            else
            {
                newTrajectory.attributes[3].push_back(-1.0);
            }

            // var element index
            newTrajectory.attributes[4].push_back(static_cast<float>(lineID));
            // line ID
            newTrajectory.attributes[5].push_back(static_cast<float>(traj));
            // next line ID
            newTrajectory.attributes[6].push_back(
                    static_cast<float>(std::min(lineID + 1, uint32_t(BCurves.size() - 1))));
            // interpolant t
            float normalizedT = BCurve.normalizeT(t);
            newTrajectory.attributes[7].push_back(normalizedT);
            // timestep index
            newTrajectory.attributes[8].push_back(float(lineID) + normalizedT);

            curArcLength += rollSegLength;
            varIDPerLine++;
        }

        newTrajectory.lineDesc = lineDescs[traj];
        newTrajectory.multiVarData = multiVarData[traj];
        newTrajectory.multiVarDescs = lineMultiVarDescs[traj];

        (*newTrajectories)[traj] = newTrajectory;
    }

    trajectorySource->releaseData(inTrajectories);
    LOG4CPLUS_DEBUG(mlog, ".. bezier trajectories done.");

    return newTrajectories;
}


MTask* MBezierTrajectoriesSource::createTaskGraph(MDataRequest request)
{
    assert(trajectorySource != nullptr);

    MTask *task = new MTask(request, this);

    // Add dependency: the trajectories.
    MDataRequestHelper rh(request);
    rh.remove("BEZIERTRAJECTORIES_LOGP_SCALED");
    task->addParent(trajectorySource->getTaskGraph(rh.request()));

    return task;
}


void MBezierTrajectoriesSource::setTrajectorySource(MTrajectoryDataSource* s)
{
    trajectorySource = s;
    registerInputSource(trajectorySource);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MBezierTrajectoriesSource::locallyRequiredKeys()
{
    return (QStringList() << "BEZIERTRAJECTORIES_LOGP_SCALED");
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

} // namespace Met3D
