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
        if (varName.startsWith('d') && varName != "deposition")
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
                filteredTrajectory.attributes[0].push_back(std::numeric_limits<float>::quiet_NaN());
                filteredTrajectory.positions.push_back(QVector3D(
                        std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::quiet_NaN()));
            }
            else
            {
                // Use pressure as attribute if no auxiliary data is available.
                filteredTrajectory.attributes[0].push_back(point.z());

                point.setZ(MSceneViewGLWidget::worldZfromPressure(
                        point.z(), log_pBottom_hPa, deltaZ_deltaLogP));
                filteredTrajectory.positions.push_back(point);
            }
            //if (point.z() == M_INVALID_TRAJECTORY_POS)
            //{
            //    continue;
            //}
            //if ((point - prevPoint).length() < 1e-5)
            //{
            //    continue;
            //}

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

            //prevPoint = point;
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
                float maxSensitivity = 0.0f;
                for (int varIdx : sensitivityIndices)
                {
                    float sensitivityValue = filteredTrajectory.attributes.at(varIdx).at(timeStepIdx);
                    if (!std::isnan(sensitivityValue))
                    {
                        if (std::abs(sensitivityValue) > std::abs(maxSensitivity))
                        {
                            maxSensitivity = sensitivityValue;
                        }
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


    // 2) Compute min max values of all attributes across all trajectories
    if (numVariables <= 0)
    {
        LOG4CPLUS_DEBUG(mlog, "ERROR: No variable was found in trajectory file.");
        std::exit(-2);
    }

    QVector<QVector2D> attributesMinMax(
            numVariables,
            QVector2D(std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()));

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


    MBezierTrajectories* newTrajectories = new MBezierTrajectories(
            inTrajectories->getGeneratingRequest(),
            filteredTrajectories, indicesToFilteredIndicesMap, numVariables,
            auxDataVarNames);

    for (int32_t traj = 0; traj < int(filteredTrajectories.size()); ++traj)
    {
        const MFilteredTrajectory& filteredTrajectory = filteredTrajectories.at(traj);
        MBezierTrajectory newTrajectory;
        newTrajectory.lineID = traj;

        for (int i = 0; i < filteredTrajectory.positions.size(); i++)
        {
            newTrajectory.positions.push_back(filteredTrajectory.positions.at(i));
            newTrajectory.elementIDs.push_back(i);
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
