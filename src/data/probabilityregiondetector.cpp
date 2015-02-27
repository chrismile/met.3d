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
#include "probabilityregiondetector.h"

// standard library imports
#include "assert.h"
//#include <bitset>
#include <unordered_map>
#include <fstream>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mstopwatch.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MProbabilityRegionDetectorFilter::MProbabilityRegionDetectorFilter()
    : MStructuredGridEnsembleFilter()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MStructuredGrid* MProbabilityRegionDetectorFilter::produceData(MDataRequest request)
{
    assert(inputSource != nullptr);

#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
#endif
    LOG4CPLUS_DEBUG(mlog, "detecting probability regions...");

    MDataRequestHelper rh(request);

    // Parse request.
    float probability = rh.floatValue("PROBABILITY");
    rh.removeAll(locallyRequiredKeys());

    MStructuredGrid* probGrid = inputSource->getData(rh.request());
    MStructuredGrid* result = createAndInitializeResultGrid(probGrid);
    result->setToZero();

    for (unsigned int k = 0; k < result->getNumLevels(); k++)
        for (unsigned int j = 0; j < result->getNumLats(); j++)
            for (unsigned int i = 0; i < result->getNumLons(); i++)
            {
                // Check if the result for this grid point has already been
                // computed.
                if (result->getValue(k, j, i) > 0.) continue;

                // If not, check if the grid point lies inside an isosurface
                // with isovalue "probability". If not, continue.
                if (probGrid->getValue(k, j, i) < probability) continue;

                // We have found a new start point. Start region growing to
                // detect all grid points that are inside the isosurface.
                LOG4CPLUS_DEBUG_FMT(mlog, "found probability region with "
                                    "value %.2f at index (k=%i/j=%i/i=%i)",
                                    probGrid->getValue(k, j, i), k, j, i);

                QVector<MIndex3D> indexList(0);
                indexList.append(MIndex3D(k, j, i));
                int currentIndex = 0;

                // Mark grid point as being in the queue.
                result->setValue(k, j, i, -1.);

                // Bitfield storing the members that contribute to the
                // isosurface.
                quint64 contributingMembers = 0;

                // Region growing.
                while (currentIndex < indexList.size())
                {
                    MIndex3D i3 = indexList.at(currentIndex++);

                    // Update contributing members.
                    contributingMembers |= probGrid->getFlags(i3.k, i3.j, i3.i);

                    // Check if adjacent grid points lie inside the isosurface.
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

                                // Continue if the adjacent grid point already
                                // is in the queue.
                                if (result->getValue(oidx.k, oidx.j, oidx.i)
                                        < 0.) continue;

                                // Continue if the grid point is not inside
                                // the isosurface.
                                if (probGrid->getValue(oidx.k, oidx.j, oidx.i)
                                        < probability) continue;

                                indexList.append(oidx);
                                result->setValue(oidx.k, oidx.j, oidx.i, -1.);
                            }
                }

                // Set result value for all visited grid points.
                int numContributingMembers = 0;
                for (unsigned char bit = 0; bit < 64; bit++)
                    if (contributingMembers & (Q_UINT64_C(1) << bit))
                        numContributingMembers += 1;

                float contributingProbability =
                        float(numContributingMembers) /
                        probGrid->getNumContributingMembers();

                // Map to check if a certain cell is within the detected
                // region.
                std::unordered_map<std::string, float> indexMap;
                indexMap.reserve(indexList.size());

                for (int n = 0; n < indexList.size(); n++)
                {
                    MIndex3D idx = indexList.at(n);
                    result->setValue(idx.k, idx.j, idx.i,
                                     contributingProbability);

                    char buffer[200] = "";
                    snprintf(buffer, 200, "%i,%i,%i",idx.k, idx.j, idx.i);
                    std::string mapEntry = buffer;
                    indexMap[mapEntry] = contributingProbability;
                }

                LOG4CPLUS_DEBUG_FMT(mlog, "%i members (%.2f) contributed to "
                                    "region of %i grid points",
                                    numContributingMembers,
                                    contributingProbability, indexList.size());
            }

    inputSource->releaseData(probGrid);

#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "probability regions detected in "
                    << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif
    return result;
}


MTask* MProbabilityRegionDetectorFilter::createTaskGraph(MDataRequest request)
{
    assert(inputSource != nullptr);

    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    rh.removeAll(locallyRequiredKeys());

    task->addParent(inputSource->getTaskGraph(rh.request()));

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MProbabilityRegionDetectorFilter::locallyRequiredKeys()
{
    return (QStringList() << "PROBABILITY");
}


} // namespace Met3D
