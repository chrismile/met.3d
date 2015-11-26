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
#ifndef TRAJECTORYREADER_H
#define TRAJECTORYREADER_H

// standard library imports

// related third party imports
#include <QtCore>
#include <netcdf>

// local application imports
#include "data/trajectorydatasource.h"
#include "data/abstractdatareader.h"
#include "data/trajectories.h"
#include "data/structuredgrid.h"


namespace Met3D
{

// Define a hierarchy of dictionaries that provide fast access to where the
// trajectories for a given init and valid (or start) time are stored.

struct MTrajectoryStartTimeInfo
{
    MTrajectoryStartTimeInfo() : isStartTime(false) {}

    // For each start (= in this context valid) time, store a list of other
    // start times whose trajectories overlap at some timestep with this start
    // time.
    QList<QDateTime> validTimeOverlap;

    // Each start time is stored in a separate file.
    QString filename;
    bool isStartTime;
};

// Stores a filename for each start time.
typedef QMap<QDateTime, MTrajectoryStartTimeInfo> MTrajectoryStartTimeMap;

// Stores which trajectory start times are available for a given init time.
typedef QMap<QDateTime, MTrajectoryStartTimeMap> MTrajectoryInitTimeMap;


/**
  Data that for each trajectory file have to be read only once and that can
  be cached as long as the file is open.
 */
struct MTrajectoryFileInfo
{
    MTrajectoryFileInfo()
        : ncFile(nullptr),
          numTimeSteps(0), numTrajectories(0), numEnsembleMembers(0)
    { }

    ~MTrajectoryFileInfo()
    { if (ncFile) delete ncFile; }

    netCDF::NcFile *ncFile;

    unsigned int numTimeSteps, numTrajectories, numEnsembleMembers;

    netCDF::NcVar lonVar, latVar, prsVar;

    QVector<QDateTime> times;

    // Start grid geometry stored in the file.
    std::shared_ptr<MStructuredGrid> startGrid;

    // Mutex to lock access to the file.
    QMutex accessMutex;
};

typedef QHash<QString, MTrajectoryFileInfo*> MOpenTrajectoryFileMap;


/**
  @brief MTrajectoryReader reads particle trajectories from CF-similar LAGRANTO
  NetCDF files.
  */
class MTrajectoryReader :
        public MTrajectoryDataSource, public MAbstractDataReader
{
public:
    MTrajectoryReader(QString identifier);

    ~MTrajectoryReader();

    /**
      Returns a @ref QList<QDateTime> containing the the available forecast
      initialisation times (base times).
      */
    QList<QDateTime> availableInitTimes();

    /**
      Returns a @ref QList<QDateTime> containing the the trajectory start times
      available for the specified initialisation time @p initTime.
      */
    QList<QDateTime> availableValidTimes(const QDateTime& initTime);

    /**
      For a given init and valid time, returns the valid (=start) times of
      those trajectories that overlap with the given valid time.
     */
    QList<QDateTime> validTimeOverlap(const QDateTime& initTime,
                                      const QDateTime& validTime);

    QSet<unsigned int> availableEnsembleMembers();

    /**
      Reads a data item from disk.
     */
    MTrajectories* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

    /**
      Reads a trajectory supplement from disk. This method is not part of the
      task graph framework and needs to be called explicitely. It is located
      in the reader class to have all file-related knowledge in one class
      (cf notes 14Jan2014, mr).

      Returns a nullptr if the requested information cannot be found.
     */
    MFloatPerTrajectorySupplement* readFloatPerTrajectorySupplement(
            MDataRequest trajectoryRequest, MDataRequest supplementRequest);

protected:

    /**
      Scans the root data directory to determine the available data sets.
      */
    void scanDataRoot();

    /**
      Define the request keys required by this reader.
     */
    const QStringList locallyRequiredKeys();

    /**
      Check if the file @p filename is open. If not, open it and create a new
      entry in @ref openFiles for this file (read coordinate variables etc.).
     */
    void checkFileOpen(QString filename);

    QString fileFilter;

    // Dictionaries of available trajectory data. Access needs to be proteced
    // by the provided read/write lock.
    MTrajectoryInitTimeMap availableTrajectories;
    QSet<unsigned int>     availableMembers;
    QReadWriteLock         availableItemsLock;

    // Dictionary of open file information. Read/write access to this
    // dictionary must be protected by the provided mutex.
    MOpenTrajectoryFileMap openFiles;
    QMutex                 openFilesMutex;
};


} // namespace Met3D

#endif // TRAJECTORYREADER_H
