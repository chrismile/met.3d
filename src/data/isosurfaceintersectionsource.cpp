/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016 Marc Rautenhaus
**  Copyright 2016 Christoph Heidelmann
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

#include "isosurfaceintersectionsource.h"
#include "data/lines.h"
#include "util/metroutines.h"

#include <omp.h>
#include <chrono>

//#define TIME_MEASUREMENT

using namespace Met3D;

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MIsosurfaceIntersectionSource::MIsosurfaceIntersectionSource() :
        inputSources({nullptr, nullptr}),
        //inputSourceB(nullptr),
        currentSegmentFace(-1),
        nextCellInScanLoop(0)
{
}

/******************************************************************************
***                             PUBLIC METHODS                              ***
*******************************************************************************/

void MIsosurfaceIntersectionSource::setInputSourceFirstVar(
        MWeatherPredictionDataSource *s)
{
    inputSources[0] = s;
    registerInputSource(inputSources[0]);
    enablePassThrough(inputSources[0]);
}


void MIsosurfaceIntersectionSource::setInputSourceSecondVar(
        MWeatherPredictionDataSource *s)
{
    inputSources[1] = s;
    registerInputSource(inputSources[1]);
    enablePassThrough(inputSources[1]);
}


MTrajectorySelection *
MIsosurfaceIntersectionSource::produceData(MDataRequest request)
{
    assert(inputSources[0] != nullptr);
    assert(inputSources[1] != nullptr);
#ifdef TIME_MEASUREMENT
    auto startIsoX = std::chrono::system_clock::now();
#endif

    MDataRequestHelper rh(request);

    GLint scount = 0;
    int linesCounter = 0;
    QVector<QVector3D> points;
    QVector<GLint> *starts = new QVector<GLint>();
    QVector<GLsizei> *sizes = new QVector<GLsizei>();
    QVector<GLint> ensembleStartIndices;
    QVector<GLsizei> ensembleLengths;

    QStringList isovalues = rh.value("ISOX_VALUES").split("/");

    float isovalueA = isovalues[0].toFloat();
    float isovalueB = isovalues[1].toFloat();

    MIsosurfaceIntersectionLines *intersectionLines = new MIsosurfaceIntersectionLines();
    intersectionLines->lines = new QVector<QVector<QVector3D> *>();

    const uint8_t lowerLineThreshold = 1;

    QStringList members = rh.value("MEMBERS").split("/");
    rh.remove("MEMBERS");
    rh.remove("ENS_OPERATION");
    //rh.insert("LINES","");

    QStringList bboxList = rh.value("ISOX_BOUNDING_BOX").split("/");
    //QStringList bboxList = bbox.split("/");
    const float llcrnlon = bboxList[0].toFloat();
    const float llcrnlat = bboxList[1].toFloat();
    const float pBot_hPa = bboxList[2].toFloat();
    const float urcrnlon = bboxList[3].toFloat();
    const float urcrnlat = bboxList[4].toFloat();
    const float pTop_hPa = bboxList[5].toFloat();

    rh.remove("ISOX_BOUNDING_BOX");

    for (const auto &member : members)
    {
        int m = member.toInt();
        rh.insert("MEMBER", QString::number(m));

        Lines *linesStored = static_cast<Lines *>(memoryManager->getData(this,
                                                                         rh.request()));
        QVector<QVector<QVector3D> *> *lines = nullptr;

        // Obtain the scalar field of the first variable.
        MDataRequestHelper rhLocal(isoRequests[0]);
        rhLocal.insert("MEMBER", QString::number(m));
        MStructuredGrid *gridA = inputSources[0]->getData(rhLocal.request());

        // Obtain the scalar field of the second variable.
        rhLocal = MDataRequestHelper(isoRequests[1]);
        rhLocal.insert("MEMBER", QString::number(m));
        MStructuredGrid *gridB = inputSources[1]->getData(rhLocal.request());

        float dx = std::abs(gridA->getLons()[0] - gridA->getLons()[1]);
        //float dy = std::abs(gridA->getLats()[0] - gridA->getLats()[1]);

        if (linesStored == nullptr)
        {
            // Compute the intersection lines of the two grids for each
            // ensemble member.
            lines = getIntersectionLineForMember(gridA, isovalueA,
                                                 gridB, isovalueB);

            Lines *l = new Lines(lines);
            l->setGeneratingRequest(rh.request());
            if (!memoryManager->storeData(this, l))
            {
                delete l;
            }
        } else
        {
            // Append the stored lines to the current array of intersection
            // lines.
            lines = linesStored->lines;
            for (int lc = 0; lc < lines->size(); lc++)
            {
                if (lines->at(lc)->size() <= lowerLineThreshold)
                { continue; }

                intersectionLines->lines->append(lines->at(lc));
            }
        }

        // Concat lines to one array.
        uint actualLines = 0;
        uint startIndex = 0;
        uint newIndexCount = 0;

        for (int lc = 0; lc < lines->size(); lc++)
        {
            // this is the filter to remove small lines < threshold
            if (lines->at(lc)->size() <= lowerLineThreshold)
            { continue; }

            if (!linesStored)
            {
                intersectionLines->lines->append(lines->at(lc));
            }

            startIndex = scount;
            newIndexCount = 0;

            for (int llc = 0; llc < lines->at(lc)->size(); llc++)
            {
                QVector3D point = lines->at(lc)->at(llc);

                if (gridA->gridIsCyclicInLongitude())
                {
                    float mixI0 = MMOD(llcrnlon - gridA->lons[0], 360.) / dx;
                    float mixI = MMOD(point.x() - gridA->lons[0], 360.) / dx;
                    //int i0 = int(ceil(mixI0));
                    //int i = int(mixI);

                    float iprime = MMOD(mixI - mixI0, gridA->nlons);
                    point.setX(llcrnlon + iprime * dx);
                }

                points.append(point);

                if (point.x() >= llcrnlon && point.x() <= urcrnlon
                    && point.y() >= llcrnlat && point.y() <= urcrnlat
                    && point.z() >= pTop_hPa && point.z() <= pBot_hPa)
                {
                    newIndexCount++;
                } else
                {
                    if (newIndexCount > 0)
                    {
                        starts->append(startIndex);
                        sizes->append(newIndexCount);
                        actualLines++;
                    }

                    startIndex = scount + 1;
                    newIndexCount = 0;
                }

                scount++;
            }

            if (newIndexCount > 0)
            {
                starts->append((GLint) startIndex);
                sizes->append((GLsizei) newIndexCount);
                actualLines++;
            }
        }
        ensembleStartIndices.append(linesCounter);
        ensembleLengths.append(actualLines);
        linesCounter += actualLines;

        inputSources[0]->releaseData(gridA);
        inputSources[1]->releaseData(gridB);
    }


    // Build the instance of a intersections lines class,
    // with all its help arrays.
    intersectionLines->vertices = points;
    QVector<GLboolean> firstVerticesOfLines = QVector<GLboolean>(points.size());
    for (int i = 0; i < starts->size(); i++)
    {
        firstVerticesOfLines[starts->at(i)] = GL_TRUE;
    }
    intersectionLines->firstVerticesOfLines = firstVerticesOfLines;
    // ?
    if (intersectionLines->startIndices)
    { delete intersectionLines->startIndices; }
    intersectionLines->startIndices = starts->data();
    // ?
    if (intersectionLines->indexCount)
    { delete intersectionLines->indexCount; }
    intersectionLines->indexCount = sizes->data();
    intersectionLines->numTrajectories = linesCounter;
    intersectionLines->ensembleStartIndices = ensembleStartIndices;
    intersectionLines->ensembleIndexCount = ensembleLengths;
    intersectionLines->numEnsembleMembers = members.size();
    //if (this->lines) { delete this->lines; }
    this->lines = intersectionLines->lines;

#ifdef TIME_MEASUREMENT
    auto endIsoX = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endIsoX - startIsoX);
    LOG4CPLUS_DEBUG(mlog, "intersection total computation time: " << elapsed.count());
#endif

    return intersectionLines;
}


MTask *MIsosurfaceIntersectionSource::createTaskGraph(MDataRequest request)
{
    assert(inputSources[0] != nullptr);
    assert(inputSources[1] != nullptr);

    MTask *task = new MTask(request, this);
    MDataRequestHelper rh(request);

    QStringList tempRequiredKeys;
    tempRequiredKeys << "MULTI_DERIVATIVE_SETTINGS" << "MULTI_GEOPOTENTIAL"
                     << "MULTI_GEOPOTENTIAL_TYPE";

    bool derivatives = rh.contains("MULTI_DERIVATIVE_SETTINGS");

    QStringList derivSettings;

    if (derivatives)
    {
        derivSettings = rh.value("MULTI_DERIVATIVE_SETTINGS").split("/");
        rh.remove("MULTI_DERIVATIVE_SETTINGS");
    }

    const QStringList vars = rh.value("ISOX_VARIABLES").split("/");
    const QString members = rh.value("MEMBERS");

    rh.removeAll(locallyRequiredKeys());

    QStringList memberList = members.split("/");

    foreach (const auto &member, memberList)
    {
        int m = member.toInt();
        rh.insert("MEMBER", QString::number(m));

        for (int i = 0; i < 2; ++i)
        {
            if (derivatives)
            {
                rh.insert("MULTI_VARIABLES", vars[0] + "/" + vars[1]);
                rh.insert("MULTI_DERIVATIVE_OPS", derivSettings[i]);
                rh.insert("VARIABLE", vars[i]);
            } else
            {
                rh.removeAll(tempRequiredKeys);
                rh.insert("VARIABLE", vars[i]);
            }

            isoRequests[i] = rh.request();
            task->addParent(inputSources[i]->getTaskGraph(rh.request()));
        }
    }

    return task;
}

/******************************************************************************
***                             PROTECTED METHODS                           ***
*******************************************************************************/

const QStringList MIsosurfaceIntersectionSource::locallyRequiredKeys()
{
    return (QStringList() << "ISOX_VARIABLES" << "ISOX_VALUES"
                          << "MEMBERS" << "ISOX_BOUNDING_BOX");
}

/******************************************************************************
***                             PRIVATE METHODS                             ***
*******************************************************************************/

QVector<QVector<QVector3D> *> *
MIsosurfaceIntersectionSource::getIntersectionLineForMember(
        MStructuredGrid *gridA, float isovalueA, MStructuredGrid *gridB,
        float isovalueB)
{
    if (gridA == nullptr || gridB == nullptr) return nullptr;

    nextCellInScanLoop = 0;
    lines = new QVector<QVector<QVector3D> *>();

    const int numCells = (gridA->getNumLons() - 1) * (gridA->getNumLats() - 1)
                         * (gridA->getNumLevels() - 1);

    cells = QVector<CellInformation *>(numCells, nullptr);
    cellsVisited = QVector<bool>(numCells, false);

    QVector<float> pressures(gridA->getNumValues(), 0);

    int numLatLons = gridA->getNumLats() * gridA->getNumLons();
    int numLons = gridA->getNumLons();

    int numCellLons = numLons - 1;
    int numCellLatLons = (gridA->getNumLats() - 1) * (gridA->getNumLons() - 1);


    // Use the multi-threaded CPU to compute the intersection task in parallel.
#pragma omp parallel
    {
        // Obtain the pressure values at each grid point.
#pragma omp for schedule(static)
        for (uint c = 0; c < gridA->getNumValues(); ++c)
        {
            const int i = c % gridA->nlons;
            const int j = (c / numLons) % gridA->nlats;
            const int k = c / (numLatLons);

            pressures[c] = gridA->getPressure(k, j, i);
        }

        // 1) Compute the intersection line segments at each voxel in parallel.
#pragma omp for schedule(dynamic, 2)
        for (uint k = 0; k < gridA->getNumLevels() - 1; ++k)
        {
            for (uint j = 0; j < gridA->getNumLats() - 1; ++j)
            {
                for (uint i = 0; i < gridA->getNumLons() - 1; ++i)
                {
                    const int dataIndex = i + j * numLons
                                          + k * numLatLons;

                    const int index = i + j * numCellLons
                                      + k * numCellLatLons;

                    CellInfoInput cInfoInput(pressures);
                    cInfoInput.actCellIndex = index;
                    cInfoInput.actDataIndex = dataIndex;
                    cInfoInput.gridA = gridA;
                    cInfoInput.isovalueA = isovalueA;
                    cInfoInput.gridB = gridB;
                    cInfoInput.isovalueB = isovalueB;

                    CellInformation *cInfo = getCellInformation(cInfoInput);
                    cells[index] = cInfo;
                    cellsVisited[index] = cInfo->isEmpty;
                }
            }
        }
    }

    // 2) Collect all segments that belong to the same line and combine those
    // as one trajectory.
    for (uint k = 0; k < gridA->getNumLevels() - 1; ++k)
    {
        for (uint j = 0; j < gridA->getNumLats() - 1; ++j)
        {
            for (uint i = 0; i < gridA->getNumLons() - 1; ++i)
            {
                const int index = i + j * numCellLons
                                  + k * numCellLatLons;

                CellInformation *cInfo = cells[index];
                if (cellsVisited[index])
                { continue; }

                cellsVisited[index] = true;
                traceLine(cInfo, gridA, isovalueA, gridB, isovalueB);
            }
        }
    }

    // Free the memory of all temporary, dynamically allocated data.
#pragma omp parallel for schedule(static)
    for (uint k = 0; k < gridA->getNumLevels() - 1; ++k)
    {
        for (uint j = 0; j < gridA->getNumLats() - 1; ++j)
        {
            for (uint i = 0; i < gridA->getNumLons() - 1; ++i)
            {
                const int index = i + j * numCellLons
                                  + k * numCellLatLons;

                for (int l = 0; l < cells[index]->segments.size(); ++l)
                {
                    delete cells[index]->segments.at(l);
                }

                delete cells[index];
            }
        }
    }

    return lines;
}


QVector3D MIsosurfaceIntersectionSource::VertexInterp(
        float isolevel, const QVector3D &p1, const QVector3D &p2,
        float valp1, float valp2)
{
    const double mu = (isolevel - valp1) / (valp2 - valp1);
    return QVector3D(p1.x() + mu * (p2.x() - p1.x()),
                     p1.y() + mu * (p2.y() - p1.y()),
                     p1.z() + mu * (p2.z() - p1.z()));
}


void MIsosurfaceIntersectionSource::traceLine(
        CellInformation *startingCell, MStructuredGrid *gridA,
        float isovalueA, MStructuredGrid *gridB, float isovalueB)
{
    // Take last cell line segment and build new line from it
    lines->append(startingCell->segments.last());

    // Face of first point of starting segment => will be used when tracing backwards
    int currentSegmentFaceWhenTracingBackwards = startingCell->pointFaceRelation.at(
            startingCell->segments.size() / 2);
    // Face of second point of starting segment
    currentSegmentFace = startingCell->pointFaceRelation.at(
            startingCell->segments.size() / 2 + 1);
    // current 3D direction of the line
    direction = getDirection(secondLastPointOfLastLine(),
                             lastPointOfLastLine());
    // Remove last segment from cell
    startingCell->removeLastSegment();

    // tracing forwards
    CellInformation *next = addCellToLastLine(
            getNextCell(startingCell, gridA, isovalueA, gridB, isovalueB));
    while (next != nullptr)
    {
        next = addCellToLastLine(
                getNextCell(next, gridA, isovalueA, gridB, isovalueB));
    }

    // tracing backwards
    currentSegmentFace = currentSegmentFaceWhenTracingBackwards;
    direction = getDirection(firstPointOfLastLine(), secondPointOfLastLine());
    next = prependCellToLastLine(
            getNextCell(startingCell, gridA, isovalueA, gridB, isovalueB));
    while (next != nullptr)
    {
        next = prependCellToLastLine(
                getNextCell(next, gridA, isovalueA, gridB, isovalueB));
    }

    // if the line is closable, close it
    if (lines->last()->size() > 3 &&
        isClose(lastPointOfLastLine(), firstPointOfLastLine()))
    {
        lines->last()->append(lines->last()->at(1));
        lines->last()->append(lines->last()->at(2));
    }

    // if the line is shorter then 2 remove it, because otherwise the normals can not be calculated
    if (lines->last()->size() < 2)
    {
        lines->pop_back();
    }
}


MIsosurfaceIntersectionSource::CellInformation *
MIsosurfaceIntersectionSource::getNextCell(
        CellInformation *actCell, MStructuredGrid *gridA,
        float isovalueA, MStructuredGrid *gridB, float isovalueB)
{
    const int numCellsLatLon =
            (gridA->getNumLons() - 1) * (gridA->getNumLats() - 1);
    const int numCells = numCellsLatLon * (gridA->getNumLevels() - 1);

    if (currentSegmentFace == LEFT_FACE)
    {
        int posIndex = actCell->index - 1;
        if (posIndex < 0)
        { return nullptr; }

        CellInformation *cell = cells[posIndex];
        if (((cell->faces1 & cell->faces2) & pow2[RIGHT_FACE]) ==
            pow2[RIGHT_FACE])
        {
            cellsVisited[posIndex] = true;
            return cell;
        }
    } else if (currentSegmentFace == BACK_FACE)
    {
        int posIndex = actCell->index + (gridA->getNumLons() - 1);
        if (posIndex < 0)
        { return nullptr; }
        if (posIndex > numCells - 1)
        { return nullptr; }

        CellInformation *cell = cells[posIndex];
        if (((cell->faces1 & cell->faces2) & pow2[FRONT_FACE]) ==
            pow2[FRONT_FACE])
        {
            cellsVisited[posIndex] = true;
            return cell;
        }
    } else if (currentSegmentFace == BOTTOM_FACE)
    {
        int posIndex = actCell->index - numCellsLatLon;
        if (posIndex < 0)
        { return nullptr; }
        CellInformation *cell = cells[posIndex];
        if (((cell->faces1 & cell->faces2) & pow2[TOP_FACE]) == pow2[TOP_FACE])
        {
            cellsVisited[posIndex] = true;
            return cell;
        }
    } else if (currentSegmentFace == RIGHT_FACE)
    {
        int posIndex = actCell->index + 1;
        if (posIndex > numCells - 1)
        { return nullptr; }
        if (posIndex < 0)
        { return nullptr; }
        CellInformation *cell = cells[posIndex];
        if (((cell->faces1 & cell->faces2) & pow2[LEFT_FACE]) ==
            pow2[LEFT_FACE])
        {
            cellsVisited[posIndex] = true;
            return cell;
        }
    } else if (currentSegmentFace == FRONT_FACE)
    {
        int posIndex = actCell->index - (gridA->getNumLons() - 1);
        if (posIndex > numCells - 1)
        { return nullptr; }
        if (posIndex < 0)
        { return nullptr; }
        CellInformation *cell = cells[posIndex];
        if (((cell->faces1 & cell->faces2) & pow2[BACK_FACE]) ==
            pow2[BACK_FACE])
        {
            cellsVisited[posIndex] = true;
            return cell;
        }
    } else if (currentSegmentFace == TOP_FACE)
    {
        int posIndex = actCell->index + numCellsLatLon;
        if (posIndex > numCells - 1)
        { return nullptr; }
        CellInformation *cell = cells[posIndex];
        if (((cell->faces1 & cell->faces2) & pow2[BOTTOM_FACE]) ==
            pow2[BOTTOM_FACE])
        {
            cellsVisited[posIndex] = true;
            return cell;
        }
    }
    return nullptr;
}


MIsosurfaceIntersectionSource::CellInformation *
MIsosurfaceIntersectionSource::addCellToLastLine(
        CellInformation *cell)
{
    if (cell == nullptr)
    { return nullptr; }
    const QVector3D *toAdd = nullptr;
    bool matched = false;
    int indexToAdd = -1;
    int pointToAdd = -1;
    for (int i = 0; i < cell->segments.size(); i++)
    {
        const QVector3D &firstOuterPoint = cell->segments.at(i)->at(0);
        const QVector3D &lastOuterPoint = cell->segments.at(i)->at(1);
        if (cell->pointFaceRelation[(i * 2) + 1] ==
            opposite(currentSegmentFace))
        {
            toAdd = &firstOuterPoint;
            pointToAdd = 0;
            matched = true;
            indexToAdd = i;
        } else if (cell->pointFaceRelation[(i * 2)] ==
                   opposite(currentSegmentFace))
        {
            toAdd = &lastOuterPoint;
            pointToAdd = 1;
            matched = true;
            indexToAdd = i;
        }
    }

    if (matched)
    {
        lines->last()->append(*toAdd);
        direction = getDirection(lastPointOfLastLine(),
                                 secondLastPointOfLastLine());
        currentSegmentFace = cell->pointFaceRelation.at(
                indexToAdd / 2 + pointToAdd);
        cell->removeSegment(indexToAdd);
        return cell;
    }
    return nullptr;
}


MIsosurfaceIntersectionSource::CellInformation *
MIsosurfaceIntersectionSource::prependCellToLastLine(
        CellInformation *cell)
{
    if (cell == nullptr)
    { return nullptr; }
    const QVector3D *toAdd = nullptr;
    bool matched = false;
    int indexToAdd = -1;
    int pointToAdd = -1;
    for (int i = 0; i < cell->segments.size(); i++)
    {
        const QVector3D &firstOuterPoint = cell->segments.at(i)->at(0);
        const QVector3D &lastOuterPoint = cell->segments.at(i)->at(1);
        if (cell->pointFaceRelation[(i * 2)] == opposite(currentSegmentFace))
        {
            toAdd = &lastOuterPoint;
            matched = true;
            indexToAdd = i;
            pointToAdd = 1;
        } else if (cell->pointFaceRelation[(i * 2) + 1] ==
                   opposite(currentSegmentFace))
        {
            toAdd = &firstOuterPoint;
            matched = true;
            indexToAdd = i;
            pointToAdd = 0;
        }
    }

    if (matched)
    {
        lines->last()->prepend(*toAdd);
        direction = getDirection(firstPointOfLastLine(),
                                 secondPointOfLastLine());
        currentSegmentFace = cell->pointFaceRelation.at(
                indexToAdd / 2 + pointToAdd);
        cell->removeSegment(indexToAdd);
        return cell;
    }
    return nullptr;
}


int MIsosurfaceIntersectionSource::dequeueNextCellIndex(MStructuredGrid *grid)
{
    if (nextCellInScanLoop++ >= static_cast<int>(grid->getNumValues()))
    { return -1; }
    return nextCellInScanLoop;
}


MIsosurfaceIntersectionSource::CellInformation *
MIsosurfaceIntersectionSource::getCellInformation(
        MIsosurfaceIntersectionSource::CellInfoInput &input)
{
    CellInformation *cellInformation = new CellInformation(input.gridA,
                                                           input.gridB,
                                                           input.actCellIndex,
                                                           input.actDataIndex,
                                                           input.pressures);

    /*
     * Algoritmus for marching cubes is taken from
     * http://paulbourke.net/geometry/polygonise/
     */

    // Get the the faces that have a surface line in it.
    cellInformation->faces1 = faceTable[cellInformation->getCubeIndexes1(
            input.isovalueA)];
    // When the cube is entirely out or in surface skip to next cube.
    if (cellInformation->faces1 == 0)
    {
        cellInformation->isEmpty = true;
    }
    // Same as above but with the second variable.
    cellInformation->faces2 = faceTable[cellInformation->getCubeIndexes2(
            input.isovalueB)];
    if (cellInformation->faces2 == 0)
    {
        cellInformation->isEmpty = true;
    }

    // In the following the intersection points found in one block
    // are abbreviated by isP.
    getCellSegments(input.isovalueA, input.isovalueB, cellInformation);
    if (cellInformation->segments.size() == 0)
    { cellInformation->isEmpty = true; }

    return cellInformation;
}


void MIsosurfaceIntersectionSource::getCellSegments(
        const float isovalueA, const float isovalueB, CellInformation *cell)
{
    // isP = intersection points
    QVector<QVector3D> isP;
    QVector<int> pointFaceRelation;
    QVector<int> facePointRelation = QVector<int>({-1, -1, -1, -1, -1, -1});

    // These two variables are used when
    // three intersections where found.
    // The description of this is written under the face loop.
    int faceWithTwoIntersects = -1, faceWithOneIntersect = -1;

    QVector<QVector3D> interp1, interp2;
    interp1.reserve(8);
    interp2.reserve(8);

//        auto start = std::chrono::system_clock::now();

    // Loop over the faces of the cell.
    for (int i = 0; i < 6; i++)
    {
        // These two variables store the
        // interpolated points for each variable
        interp1.resize(0); // trick to shrink QVector without releasing memory
        interp2.resize(0);

        // When both cubes have an intersecting face?
        if ((cell->faces1 & pow2[i]) == pow2[i] &&
            (cell->faces2 & pow2[i]) == pow2[i])
        {

            // Get the points in that face that are smaller then the
            // isolevel, then get the cutten edges with lineArrangements[]
            const int facePoint0 = facePoints[i * 4];
            const int facePoint1 = facePoints[i * 4 + 1];
            const int facePoint2 = facePoints[i * 4 + 2];
            const int facePoint3 = facePoints[i * 4 + 3];

            int points1 = 0;
            points1 += (cell->values1[facePoint0] < isovalueA) ? 1 : 0;
            points1 += (cell->values1[facePoint1] < isovalueA) ? 2 : 0;
            points1 += (cell->values1[facePoint2] < isovalueA) ? 4 : 0;
            points1 += (cell->values1[facePoint3] < isovalueA) ? 8 : 0;
            int lines1 = edgeTable[points1];

            int points2 = 0;
            points2 += (cell->values2[facePoint0] < isovalueB) ? 1 : 0;
            points2 += (cell->values2[facePoint1] < isovalueB) ? 2 : 0;
            points2 += (cell->values2[facePoint2] < isovalueB) ? 4 : 0;
            points2 += (cell->values2[facePoint3] < isovalueB) ? 8 : 0;
            int lines2 = edgeTable[points2];

            // When the lines lay like this in the face, intersection is not
            // possible (with rotated cases).
            //     2---x--3
            //     x      *
            //     |      |
            //     0---*--1
            if ((lines1 == 3 && lines2 == 12) ||
                (lines1 == 6 && lines2 == 9) ||
                (lines1 == 9 && lines2 == 6) ||
                (lines1 == 12 && lines2 == 3))
            { continue; }

//                auto start = std::chrono::system_clock::now();

            // Loop over the face edges.
            for (int l = 0; l < 4; l++)
            {
                // Is the edge cut in the cube of the first variable?
                if ((lines1 & pow2[l]) == pow2[l])
                {
                    const int edgePointPrev = edgePoints[(i * 8 + l * 2)];
                    const int edgePointNext = edgePoints[(i * 8 + l * 2) + 1];

                    // Interpolate the point on the current edge.
                    float val1 = cell->values1[edgePointPrev];
                    float val2 = cell->values1[edgePointNext];
                    const QVector3D &point1 = cell->cellPoints[edgePointPrev];
                    const QVector3D &point2 = cell->cellPoints[edgePointNext];
                    interp1.append(VertexInterp(isovalueA, point1, point2, val1,
                                                val2));
                }
                // Is the edge cut in the cube of the second variable?
                if ((lines2 & pow2[l]) == pow2[l])
                {
                    const int edgePointPrev = edgePoints[(i * 8 + l * 2)];
                    const int edgePointNext = edgePoints[(i * 8 + l * 2) + 1];

                    // Interpolate the point on the current edge.
                    float val1 = cell->values2[edgePointPrev];
                    float val2 = cell->values2[edgePointNext];
                    const QVector3D &point1 = cell->cellPoints[edgePointPrev];
                    const QVector3D &point2 = cell->cellPoints[edgePointNext];
                    interp2.append(VertexInterp(isovalueB, point1, point2, val1,
                                                val2));
                }
            }

            // When we have a bifurcation this counter is used to detect the face with two points
            int countOfIntersectsFace = 0;

            // When there are four points in one face two ambiguous cases occur.
            // The decision between these cases is done by the isovalue of one point of the face.
            if (interp1.size() == 4)
            {
                if (cell->values1[1] < isovalueA)
                {
                    QVector3D p1 = interp1[0];
                    for (int i = 0; i < 3; ++i)
                    {
                        interp1[i] = interp1[i + 1];
                    }
                    interp1[3] = p1;
                }
            }
            if (interp2.size() == 4)
            {
                if (cell->values2[1] < isovalueB)
                {
                    QVector3D p1 = interp2[0];
                    for (int i = 0; i < 3; ++i)
                    {
                        interp2[i] = interp2[i + 1];
                    }
                    interp2[3] = p1;
                }
            }

            // Test the found lines for intersection and
            // store the intersection points in the variable isP
            for (int lc1 = 0; lc1 < interp1.size() - 1; lc1 += 2)
            {
                const QVector3D &a1 = interp1.at(lc1);
                const QVector3D &a2 = interp1.at(lc1 + 1);

                for (int lc2 = 0; lc2 < interp2.size() - 1; lc2 += 2)
                {
                    const QVector3D &b1 = interp2.at(lc2);
                    const QVector3D &b2 = interp2.at(lc2 + 1);
                    QVector3D intersection(0, 0, 0);
                    // Due to the fact that intersection in 3D is heavy to compute,
                    // we use the information of the current face to calculate the
                    // intersection in the 2D space.
                    if (i == TOP_FACE || i == BOTTOM_FACE)
                    {
                        QVector2D p(a1.x(), a1.y());
                        QVector2D p2(a2.x(), a2.y());
                        QVector2D q(b1.x(), b1.y());
                        QVector2D q2(b2.x(), b2.y());
                        QVector2D out = getLineSegmentsIntersectionPoint(p, p2,
                                                                         q, q2);
                        if (out != QVector2D(0, 0))
                        {
                            intersection = QVector3D(out.x(), out.y(), a1.z());
                        }
                    } else if (i == LEFT_FACE || i == RIGHT_FACE)
                    {
                        QVector2D p(a1.y(), a1.z());
                        QVector2D p2(a2.y(), a2.z());
                        QVector2D q(b1.y(), b1.z());
                        QVector2D q2(b2.y(), b2.z());
                        QVector2D out = getLineSegmentsIntersectionPoint(p, p2,
                                                                         q, q2);
                        if (out != QVector2D(0, 0))
                        {
                            intersection = QVector3D(a1.x(), out.x(), out.y());
                        }
                    } else if (i == BACK_FACE || i == FRONT_FACE)
                    {
                        QVector2D p(a1.x(), a1.z());
                        QVector2D p2(a2.x(), a2.z());
                        QVector2D q(b1.x(), b1.z());
                        QVector2D q2(b2.x(), b2.z());
                        QVector2D out = getLineSegmentsIntersectionPoint(p, p2,
                                                                         q, q2);
                        if (out != QVector2D(0, 0))
                        {
                            intersection = QVector3D(out.x(), a1.y(), out.y());
                        }
                    }

                    // if an intersection was found
                    if (intersection != QVector3D(0, 0, 0))
                    {
                        pointFaceRelation.append(i);
                        facePointRelation.replace(i, isP.size());
                        isP.append(intersection);
                        countOfIntersectsFace++;
                    }
                }
            }

            // The following lines checking if the last face or
            // the first face have an intersection this is used
            // for detection of the direction of a bifurcation.
            if (countOfIntersectsFace == 1)
            {
                faceWithOneIntersect++;
            }
            if (countOfIntersectsFace == 2)
            {
                faceWithTwoIntersects++;
            }
        }
    }

    // We decide which points get connected regarding on the
    // point count and if the first or the last face
    // has two intersects.
    // This variable stores the found lines.
    //QVector<QVector<QVector3D>*> newSegments;
    QVector<int> retPointFaceRelation;
    QVector<int> retFacePointRelation;

    // Easy case: two points -> one line
    if (isP.size() == 2)
    {
        cell->segments.append(new QVector<QVector3D>());
        cell->segments.at(0)->reserve(2);

        // When the two points at the same position,
        // move one a little to not to break the calculation
        // of the normals.
        if (isP.at(0) != isP.at(1))
        {
            cell->segments.at(0)->append(isP.at(0));
        } else
        {
            cell->segments.at(0)->append(isP.at(0) + QVector3D(0, 0, 0.1));
        }
        cell->segments.at(0)->append(isP.at(1));
        cell->pointFaceRelation = pointFaceRelation;
        cell->facePointRelation = facePointRelation;


    }
        // When three points are found, this cube is
        // a bifurcation.
        // Then we have to decide how to build the lines.
        // What we do is build two lines from the face that
        // has one intersection to the one with two intersections.
    else if (isP.size() == 3)
    {
        retFacePointRelation = QVector<int>({-1, -1, -1, -1, -1, -1});
        cell->segments.reserve(2);
        cell->segments.append(new QVector<QVector3D>());
        cell->segments.append(new QVector<QVector3D>());
        cell->segments.at(0)->reserve(2);
        cell->segments.at(1)->reserve(2);
        retPointFaceRelation.reserve(4);

        if (faceWithOneIntersect < faceWithTwoIntersects)
        {
            cell->segments.at(0)->append(isP.first());
            retPointFaceRelation.append(pointFaceRelation.first());
            cell->segments.at(0)->append(isP.at(1));
            retPointFaceRelation.append(pointFaceRelation.at(1));
            cell->segments.at(1)->append(isP.first());
            retPointFaceRelation.append(pointFaceRelation.first());
            cell->segments.at(1)->append(isP.at(2));
            retPointFaceRelation.append(pointFaceRelation.at(2));
        } else
        {
            cell->segments.at(0)->append(isP.last());
            retPointFaceRelation.append(pointFaceRelation.last());
            cell->segments.at(0)->append(isP.at(1));
            retPointFaceRelation.append(pointFaceRelation.at(0));
            cell->segments.at(1)->append(isP.last());
            retPointFaceRelation.append(pointFaceRelation.last());
            cell->segments.at(1)->append(isP.at(1));
            retPointFaceRelation.append(pointFaceRelation.at(1));
        }

        retFacePointRelation.replace(retPointFaceRelation.first(), 0);
        retFacePointRelation.replace(retPointFaceRelation.at(1), 1);
        retFacePointRelation.replace(retPointFaceRelation.at(2), 2);
        retFacePointRelation.replace(retPointFaceRelation.last(), 3);

        cell->pointFaceRelation = retPointFaceRelation;
        cell->facePointRelation = retFacePointRelation;
    }
        // In this case 4 intersection points are found.
        // We resort the the points to build the lines.
    else if (isP.size() == 4)
    {
        retFacePointRelation = QVector<int>({-1, -1, -1, -1, -1, -1});
        cell->segments.reserve(2);
        cell->segments.append(new QVector<QVector3D>());
        cell->segments.append(new QVector<QVector3D>());
        cell->segments.at(0)->reserve(2);
        cell->segments.at(1)->reserve(2);
        retPointFaceRelation.reserve(4);

        // if we have a next face we use the current direction of the line to get the
        // the fitting line segment, if the line segments then cross, swap the segments end points
        if (currentSegmentFace != -1)
        {
            float closestDistance = 180;
            int closestIndex = facePointRelation.at(
                    opposite(currentSegmentFace));
            if (closestIndex == -1) closestIndex = 0;
            const QVector3D &closestPoint = isP.at(closestIndex);
            int closestIndex2 = -1;
            // Find the segment with the least agle between current direction and it
            for (int l = 0; l < 4; l++)
            {
                if (closestIndex != l)
                {
                    QVector3D directionTemp = (isP.at(l) -
                                               closestPoint).normalized();
                    float angle = acos(
                            QVector3D::dotProduct(directionTemp, direction));
                    if (abs(angle) < closestDistance)
                    {
                        closestDistance = abs(angle);
                        closestIndex2 = l;
                    }
                }
            }
            int next1 = -1;
            int next2 = -1;
            for (int l = 0; l < 4; l++)
            {
                if (l != closestIndex && l != closestIndex2)
                {
                    next1 = l;
                }
            }
            for (int l = 0; l < 4; l++)
            {
                if (l != closestIndex && l != closestIndex2 && l != next1)
                {
                    next2 = l;
                }
            }

            // if the line segments then intersect, switch the order
            if (getLineSegmentsIntersectionPoint(
                    isP.at(closestIndex).toVector2D(),
                    isP.at(closestIndex2).toVector2D(),
                    isP.at(next1).toVector2D(),
                    isP.at(next2).toVector2D())
                != QVector2D(0, 0))
            {
                cell->segments.at(0)->append(isP.at(next1));
                retPointFaceRelation.append(pointFaceRelation.at(next1));
                cell->segments.at(0)->append(isP.at(next2));
                retPointFaceRelation.append(pointFaceRelation.at(next2));

                cell->segments.at(1)->append(isP.at(closestIndex));
                retPointFaceRelation.append(pointFaceRelation.at(closestIndex));
                cell->segments.at(1)->append(isP.at(closestIndex2));
                retPointFaceRelation.append(
                        pointFaceRelation.at(closestIndex2));
            } else
            {
                cell->segments.at(0)->append(isP.at(closestIndex));
                retPointFaceRelation.append(pointFaceRelation.at(closestIndex));
                cell->segments.at(0)->append(isP.at(closestIndex2));
                retPointFaceRelation.append(
                        pointFaceRelation.at(closestIndex2));

                cell->segments.at(1)->append(isP.at(next1));
                retPointFaceRelation.append(pointFaceRelation.at(next1));
                cell->segments.at(1)->append(isP.at(next2));
                retPointFaceRelation.append(pointFaceRelation.at(next2));
            }
        } else
        {
            if (getLineSegmentsIntersectionPoint(isP.at(0).toVector2D(),
                                                 isP.at(1).toVector2D(),
                                                 isP.at(2).toVector2D(),
                                                 isP.at(3).toVector2D())
                != QVector2D(0, 0))
            {

                cell->segments.at(0)->append(isP.at(1));
                retPointFaceRelation.append(pointFaceRelation.at(1));
                cell->segments.at(0)->append(isP.at(2));
                retPointFaceRelation.append(pointFaceRelation.at(2));

                cell->segments.at(1)->append(isP.at(0));
                retPointFaceRelation.append(pointFaceRelation.at(0));
                cell->segments.at(1)->append(isP.at(3));
                retPointFaceRelation.append(pointFaceRelation.at(3));
            } else
            {
                cell->segments.at(0)->append(isP.at(0));
                retPointFaceRelation.append(pointFaceRelation.at(0));
                cell->segments.at(0)->append(isP.at(1));
                retPointFaceRelation.append(pointFaceRelation.at(1));

                cell->segments.at(1)->append(isP.at(2));
                retPointFaceRelation.append(pointFaceRelation.at(2));
                cell->segments.at(1)->append(isP.at(3));
                retPointFaceRelation.append(pointFaceRelation.at(3));
            }
        }

        retFacePointRelation.replace(retPointFaceRelation.first(), 0);
        retFacePointRelation.replace(retPointFaceRelation.at(1), 1);
        retFacePointRelation.replace(retPointFaceRelation.at(2), 2);
        retFacePointRelation.replace(retPointFaceRelation.last(), 3);

        cell->pointFaceRelation = retPointFaceRelation;
        cell->facePointRelation = retFacePointRelation;
    }
}


bool
MIsosurfaceIntersectionSource::isClose(const QVector3D &a, const QVector3D &b)
{
    // Arbitrary value chosen by Christoph! Think about a generic value to decide
    // whether two points are closely located
    if (abs(a.x() - b.x()) <= 5 &&
        abs(a.y() - b.y()) <= 5 &&
        abs(a.z() - b.z()) <= 5)
        return true;
    return false;
}
