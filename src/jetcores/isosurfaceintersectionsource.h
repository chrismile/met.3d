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
#ifndef ISOSURFACEINTERSECTIONSOURCE_H
#define ISOSURFACEINTERSECTIONSOURCE_H

// standard library imports
#include <array>

// related third party imports

// local application imports
#include "data/weatherpredictiondatasource.h"
#include "data/structuredgrid.h"
#include "data/datarequest.h"
#include "data/trajectorydatasource.h"


namespace Met3D
{

class MIsosurfaceIntersectionSource : public MTrajectoryDataSource
{
public:
    explicit MIsosurfaceIntersectionSource();

    void setInputSourceFirstVar(MWeatherPredictionDataSource *s);

    void setInputSourceSecondVar(MWeatherPredictionDataSource *s);

    /**
      Overloads @ref MMemoryManagedDataSource::getData() to cast
      the returned @ref MAbstractDataItem to @ref MTrajectories
      that contains the isosurface intersection Lines
     */
    MIsosurfaceIntersectionLines *getData(MDataRequest request)
    {
        return static_cast<MIsosurfaceIntersectionLines *>
        (MTrajectoryDataSource::getData(request));
    }

    /**
      Produces the data item corresponding to @p request.

      @note This function needs to be implemented in a <em> thread-safe </em>
      manner, i.e. all access to shared data/resources within this class needs
      to be serialized @see
      https://qt-project.org/doc/qt-4.8/threads-reentrancy.html.
     */
    MTrajectorySelection *produceData(MDataRequest request);

    QVector<QVector<QVector3D> *> *getIntersectionLineForMember(
            MStructuredGrid *gridA, float isovalueA, MStructuredGrid *gridB,
            float isovalueB);

    MTask *createTaskGraph(MDataRequest request);

    /**
      Returns a @ref QList<QDateTime> containing the the available forecast
      initialisation times (base times).
      */
    QList<QDateTime> availableInitTimes() { return QList<QDateTime>(); }

    /**
      Valid times correspond to the trajectory start times available for the
      specified initialisation time @p initTime.
      */
    QList<QDateTime> availableValidTimes(const QDateTime &initTime)
    {
        Q_UNUSED(initTime);
        return QList<QDateTime>();
    }

    /**
      For a given init and valid time, returns the valid (=start) times of
      those trajectories that overlap with the given valid time.
     */
    QList<QDateTime> validTimeOverlap(const QDateTime &initTime,
                                      const QDateTime &validTime)
    {
        Q_UNUSED(initTime);
        Q_UNUSED(validTime);
        return QList<QDateTime>();
    }

    /**
      Returns the available ensemble members.
      */
    virtual QSet<unsigned int> availableEnsembleMembers()
    { return QSet<unsigned int>(); }

protected:
    friend class MLineGeometryFilter;

    friend class MRawLineGeometryFilter;

    friend class MVariableTrajectoryFilter;

    const QStringList locallyRequiredKeys();


private:
    /**
      @var lines

      @brief Stores the list of lines during the computation and also keeps the
      lines of the last computation.
      */
    QVector<QVector<QVector3D> *> *lines;

    /**
      @var inputSourceA
      @brief The input source for the first variable.
      */
    std::array<MWeatherPredictionDataSource *, 2> inputSources;
    std::array<QString, 2> isoRequests;

    /**
      @enum Faces
      @brief The enum make the face numbering human readable.
      */
    enum Faces : int
    {
        LEFT_FACE = 0,
        RIGHT_FACE = 1,
        FRONT_FACE = 2,
        BACK_FACE = 3,
        BOTTOM_FACE = 4,
        TOP_FACE = 5
    };

    /**
      @var currentSegmentFace
      @brief Stores the face that the last added segment ends in. Is used in
      the tracing method.
      */
    int currentSegmentFace;

    /**
      @var direction
      @brief Stores the vector direction of the last added segment.
      */
    QVector3D direction;

    /**
      @var faceTable
      @brief Faces table describes the relation betweed corner points and faces.

      The following table descibes which faces can have an intersection
      depending on which of the 8 corner points of the cube is under the
      regarding isolevel.

      If the facenumber bit is set the face can have an intersection.

      The numbering of the 6 faces is as the following:
      0 : Left
      1 : Right
      2 : Front
      3 : Back
      4 : Bottom
      5 : Top
      */

    const char faceTable[256] = {
            0b000000, 0b010101, 0b010110, 0b010111, 0b011001, 0b011101,
            0b011111,
            0b011111, 0b011010, 0b011111, 0b011110, 0b011111, 0b011011,
            0b011111,
            0b011111, 0b001111, 0b100101, 0b110101, 0b110111, 0b110111,
            0b111101,
            0b111101, 0b111111, 0b111111, 0b111111, 0b111111, 0b111111,
            0b111111,
            0b111111, 0b111111, 0b111111, 0b101111, 0b100110, 0b110111,
            0b110110,
            0b110111, 0b111111, 0b111111, 0b111111, 0b111111, 0b111110,
            0b111111,
            0b111110, 0b111111, 0b111111, 0b111111, 0b111111, 0b101111,
            0b100111,
            0b110111, 0b110111, 0b110011, 0b111111, 0b111111, 0b111111,
            0b111011,
            0b111111, 0b111111, 0b111111, 0b111011, 0b111111, 0b111111,
            0b111111,
            0b101011, 0b101001, 0b111101, 0b111111, 0b111111, 0b111001,
            0b111101,
            0b111111, 0b111111, 0b111011, 0b111111, 0b111111, 0b111111,
            0b111011,
            0b111111, 0b111111, 0b101111, 0b101101, 0b111101, 0b111111,
            0b111111,
            0b111101, 0b111100, 0b111111, 0b111110, 0b111111, 0b111111,
            0b111111,
            0b111111, 0b111111, 0b111110, 0b111111, 0b101110, 0b101111,
            0b111111,
            0b111111, 0b111111, 0b111111, 0b111111, 0b111111, 0b111111,
            0b111111,
            0b111111, 0b111111, 0b111111, 0b111111, 0b111111, 0b111111,
            0b101111,
            0b101111, 0b111111, 0b111111, 0b111011, 0b111111, 0b111110,
            0b111111,
            0b111010, 0b111111, 0b111111, 0b111111, 0b111011, 0b111111,
            0b111110,
            0b111111, 0b101010, 0b101010, 0b111111, 0b111110, 0b111111,
            0b111011,
            0b111111, 0b111111, 0b111111, 0b111010, 0b111111, 0b111110,
            0b111111,
            0b111011, 0b111111, 0b111111, 0b101111, 0b101111, 0b111111,
            0b111111,
            0b111111, 0b111111, 0b111111, 0b111111, 0b111111, 0b111111,
            0b111111,
            0b111111, 0b111111, 0b111111, 0b111111, 0b111111, 0b101111,
            0b101110,
            0b111111, 0b111110, 0b111111, 0b111111, 0b111111, 0b111111,
            0b111111,
            0b111110, 0b111111, 0b111100, 0b111101, 0b111111, 0b111111,
            0b111101,
            0b101101, 0b101111, 0b111111, 0b111111, 0b111011, 0b111111,
            0b111111,
            0b111111, 0b111011, 0b111111, 0b111111, 0b111101, 0b111001,
            0b111111,
            0b111111, 0b111101, 0b101001, 0b101011, 0b111111, 0b111111,
            0b111111,
            0b111011, 0b111111, 0b111111, 0b111111, 0b111011, 0b111111,
            0b111111,
            0b111111, 0b110011, 0b110111, 0b110111, 0b100111, 0b101111,
            0b111111,
            0b111111, 0b111111, 0b111111, 0b111110, 0b111111, 0b111110,
            0b111111,
            0b111111, 0b111111, 0b111111, 0b110111, 0b110110, 0b110111,
            0b100110,
            0b101111, 0b111111, 0b111111, 0b111111, 0b111111, 0b111111,
            0b111111,
            0b111111, 0b111111, 0b111111, 0b111101, 0b111101, 0b110111,
            0b110111,
            0b110101, 0b100101, 0b001111, 0b011111, 0b011111, 0b011011,
            0b011111,
            0b011110, 0b011111, 0b011010, 0b011111, 0b011111, 0b011101,
            0b011001,
            0b010111, 0b010110, 0b010101, 0b000000
    };

    /**
      @var pow2
      @brief Precalculated power of two values.
      */
    const int pow2[8] = {1, 2, 4, 8, 16, 32, 64, 128};


    /**
      @var facePoints
      @brief This array stores the Relation between face of a cube and the
      corner points.

      This array describes which are the points of each face in the cube.

      You can access each facepoint by:
      {facenumber * 4 + facepoint} with facepoint e [0..3]

      They are stored in the order:
      2------3
      |      |
      |      |
      0------1
      */
    const int facePoints[24] = {
            2, 0, 6, 4,
            1, 3, 5, 7,
            0, 1, 4, 5,
            3, 2, 7, 6,
            2, 3, 0, 1,
            4, 5, 6, 7
    };

    /**
      @var edgePoints

      @brief This array stores the relation between the faces and the face
      edges.

      In the following we abbreviate the eight cornerpoints of the current cell
      with p[0..7] and the corresponding values with v[0..7].

      This array describes which are the edges of each face in the cube.

      You can access each edgepoints by:
      {facenumber * 4 + edgenum * 2 + (0 | 1)}
      */

    const int edgePoints[48] = {
            //left
            2, 0,
            0, 4,
            4, 6,
            6, 2,
            //right
            1, 3,
            3, 7,
            7, 5,
            5, 1,
            //front
            0, 1,
            1, 5,
            5, 4,
            4, 0,
            //back
            3, 2,
            2, 6,
            6, 7,
            7, 3,
            //Bottom
            2, 3,
            3, 1,
            1, 0,
            0, 2,
            //Top
            4, 5,
            5, 7,
            7, 6,
            6, 4
    };

    /**
      @var edgeTable

      This array describes which combination of corner points that are under
      the isovalue results into which edges get cut.
      */
    const char edgeTable[16] = {
            0b0000, 0b1001, 0b0011, 0b1010,
            0b1100, 0b0101, 0b1111, 0b0110,
            0b0110, 0b1111, 0b0101, 0b1100,
            0b1010, 0b0011, 0b1001, 0b0000
    };

    /**
      @enum CellInformation
      @brief The struct stores all necessary information of one cell.
     */
    struct CellInformation
    {
        /**
          @enum values1
          @brief The values of the 8 corner points for variable 1.
          */
        float values1[8];

        /**
          @enum values2
          @brief The values of the 8 corner points for variable 2.
          */
        float values2[8];

        /**
          @enum cellPoints
          @brief The positions of the 8 corner points.
          */
        QVector3D cellPoints[8];

        /**
          @enum faces1
          @brief The cut faces of variable 1.
          */
        int faces1;

        /**
          @enum faces2
          @brief The cut faces of variable 2.
          */
        int faces2;

        /**
          @enum index
          @brief The index that this cell has in the cache array.
          */
        int index = -1;

        bool isEmpty = false;

        /**
          @enum segments
          @brief The cell segments found in the cell.
          */
        QVector<QVector<QVector3D> *> segments;

        /**
          @enum pointFaceRelation
          @brief Describes to which faces one point of the segments belongs to.
          */
        QVector<int> pointFaceRelation;

        /**
          @enum facePointRelation
          @brief Describes which points belong to which face.
          */
        QVector<int> facePointRelation;

        int lon;
        int lat;
        int lev;

        CellInformation();

        CellInformation(MStructuredGrid *gridA, MStructuredGrid *gridB,
                        int cellIndex, int dataIndex,
                        const QVector<float> &pressures);

        /**
          @brief Computes the cube index of variable 1.

          The cubeindex describes which values of the cube are below the
          isovalue. It stores this by switching on/off bytes in an integer
          value.
          */
        int getCubeIndexes1(float isovalue);

        /**
          @brief Computes the cubeindex of variable 2.

          The cubeindex describes which values of the cube are below the
          isovalue. It stores this by switching on/off bytes in an integer
          value.
          */
        int getCubeIndexes2(float isovalue);


        void removeSegment(int i);

        void removeLastSegment() { removeSegment(segments.size() - 1); }

    private:

        /**
          @brief Fills this cells corner points with their correct positions.
          */
        void fillCellPoints(MStructuredGrid *grid,
                            const QVector<float> &pressures);

        /**
          @brief Fills these cells corner points with the correct values.
          @param cellValues the cellValues to be filled (values1 or values2).
          @param grid the grid where the values will be taken from.
          */
        void fillCellValues(float cellValues[], MStructuredGrid *grid);
    };

    int nextCellInScanLoop = 0;

    /**
      @var computedCells
      @brief Stores the CellInformations that are already calculated.
      Uses the index of the cell to index it in the array.
      */
    QVector<CellInformation *> cells;
    QVector<bool> cellsVisited;
    //QVector<CellInformation*> computedCells;

    /**
      @brief Calculates 3D interpolation between the two points p1 and p2.
      The params @p valp1 and @p valp2 are the values of the grid at the points
      @p p1 and @p p2. The param @p isolevel is the value that represents the
      value at the returned point.
      */
    QVector3D VertexInterp(float isolevel,
                           const QVector3D &p1,
                           const QVector3D &p2,
                           float valp1,
                           float valp2);

    /**
      @brief Calculates the face of the opposite side.
      */
    inline int opposite(int face)
    {
        switch (face)
        {
            case Faces::LEFT_FACE:
                return static_cast<int>(Faces::RIGHT_FACE);
            case Faces::RIGHT_FACE:
                return static_cast<int>(Faces::LEFT_FACE);
            case Faces::FRONT_FACE:
                return static_cast<int>(Faces::BACK_FACE);
            case Faces::BACK_FACE:
                return static_cast<int>(Faces::FRONT_FACE);
            case Faces::BOTTOM_FACE:
                return static_cast<int>(Faces::TOP_FACE);
            case Faces::TOP_FACE:
                return static_cast<int>(Faces::BOTTOM_FACE);
            default:
                return -1;
        }
    }

    /**
      @brief Calculates if two points are close.
      Is uesd to decide whether to close a cell or not.
      */
    bool isClose(const QVector3D &a, const QVector3D &b);

    // int getCubeIndexes(float cellValues[], float isovalue);

    // int getFaces(float cellValues[], float isovalue);

    /**
      @brief Calculates the cell segments for one cell.
      */
    void getCellSegments(
            const float isovalueA, const float isovalueB,
            CellInformation *cell);

    struct CellInfoInput
    {
        CellInfoInput(QVector<float> &pressureArray) : pressures(
                pressureArray)
        {
        }

        int actCellIndex;
        int actDataIndex;
        MStructuredGrid *gridA;
        float isovalueA;
        MStructuredGrid *gridB;
        float isovalueB;
        const QVector<float> &pressures;
    };

    /**
      @brief Get all the cell information for one cell.
      */
    CellInformation *getCellInformation(CellInfoInput &input);

    /**
      @brief Adds a cell to the current processed line.
      */
    CellInformation *addCellToLastLine(CellInformation *cell);

    /**
      @brief Prepends a cell to the current processed line.
      */
    CellInformation *prependCellToLastLine(CellInformation *cell);

    /**
      @brief Gets the next cell index.
      */
    signed int dequeueNextCellIndex(MStructuredGrid *grid);

    /**
      @brief Traces a line starting at actCell.
      */
    void traceLine(CellInformation *actCell, MStructuredGrid *gridA,
                   float isovalueA, MStructuredGrid *gridB,
                   float isovalueB);

    /**
      @brief Computes the next cell by using the startingCell and the
      instance variable currentSegmentFace.
      */
    CellInformation *getNextCell(CellInformation *startingCell,
                                 MStructuredGrid *gridA);

    inline QVector3D getDirection(const QVector3D &a, const QVector3D &b)
    {
        return (b - a).normalized();
    }

    inline QVector3D lastPointOfLastLine()
    {
        return lines->last()->last();
    }

    inline QVector3D secondLastPointOfLastLine()
    {
        return lines->last()->at(lines->last()->size() - 2);
    }

    inline QVector3D firstPointOfLastLine()
    {
        return lines->last()->first();
    }

    inline QVector3D secondPointOfLastLine()
    {
        return lines->last()->at(1);
    }

    static inline QVector3D flatIndexToVector3D(int i, MStructuredGrid *grid)
    {
        return QVector3D(i % grid->nlons,
                         (i / grid->nlons) % grid->nlats,
                         i / (grid->nlats * grid->nlons));
    }

    static inline bool cellIsOutOfBounds(int actCellIndex,
                                         MStructuredGrid *grid)
    {
        QVector3D cellPosition = flatIndexToVector3D(actCellIndex, grid);
        return cellPosition.x() > grid->nlons - 2 ||
               cellPosition.x() < 2 ||
               cellPosition.y() > grid->nlats - 2 ||
               cellPosition.y() < 2 ||
               cellPosition.z() > grid->nlevs - 2 ||
               cellPosition.z() < 2;
    }

    static inline int Vector3DtoFlatIndex(const QVector3D &vector,
                                          MStructuredGrid *grid)
    {
        return INDEX3zyx(vector.z(), vector.y(), vector.x(), grid->nlats,
                         grid->nlons);
    }

};


} // namespace Met3D

#endif // ISOSURFACEINTERSECTIONSOURCE_H
