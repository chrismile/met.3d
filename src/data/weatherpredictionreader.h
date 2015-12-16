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
#ifndef WEATHERPREDICIONREADER_H
#define WEATHERPREDICIONREADER_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "data/weatherpredictiondatasource.h"
#include "data/abstractdatareader.h"
#include "data/structuredgrid.h"


namespace Met3D
{

/** Information specific to a variable and timestep. */
struct MDatafieldInfo
{
    QString filename;     // file in which the variable is stored
};


// Define a hierarchy of dictionaries that provide fast access to where a
// specific datafield is stored.
// NOTE: QMap provides (log n) access time, QHash provides constant access
// time. However, there is no qHash function (required by QHash) implemented
// for QDateTime. Hence the question of what is faster: Using QMap (performing
// "<" comparisons of QDateTime -- with ~ 100 elements ~ 7 comparisons would be
// required), or using QDateTime.toISOString() as key (resulting in the
// overhead of creating the string and computing the hash function from it).
// See http://doc.qt.nokia.com/4.7/containers.html#algorithmic-complexity.
typedef QMap<QDateTime, MDatafieldInfo> MValidTimeMap;
typedef QMap<QDateTime, MValidTimeMap> MInitTimeMap;

struct MVariableInfo
{
    // This struct stores variable-specific information, the hierarchy of
    // maps is continued in the field "timeMap".
    MInitTimeMap timeMap;
    QString      variablename;        // NetCDF variable name
    QString      longname;            // CF-conventions
    QString      standardname;        // CF-conventions
    QString      units;               // CF-conventions
    QString      surfacePressureName; // for variables on hybrid model levels
                                      // the name of the var containing the
                                      // corresponding sfc pressure field
    QSet<unsigned int> availableMembers; // list of available ensemble members;
                                         // if the variable is not part of a
                                         // multimember ensemble, the list will
                                         // contain a single "0" member
};

typedef QMap<QString, MVariableInfo*> MVariableNameMap;
typedef QMap<MVerticalLevelType, MVariableNameMap> MLevelTypeMap;


/**
  @brief Base class to readers that read weather prediction data.
  */
class MWeatherPredictionReader :
        public MWeatherPredictionDataSource, public MAbstractDataReader
{
public:
    MWeatherPredictionReader(QString identifier);

    MStructuredGrid* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

protected:
    /**
      Only applicable for model level variables. Returns the name of the
      surface pressure variable that is associated with this variable (to
      compute the pressure field). If this method is called for a non model
      level variable, an exception is raised.
      */
    virtual QString variableSurfacePressureName(
            MVerticalLevelType levelType,
            const QString&     variableName) = 0;

    /**
      Reads the requested data field from disk. The returned @ref
      MStructuredGrid pointer needs to be deleted by the caller.
      */
    virtual MStructuredGrid* readGrid(MVerticalLevelType levelType,
                                      const QString&     variableName,
                                      const QDateTime&   initTime,
                                      const QDateTime&   validTime,
                                      unsigned int       ensembleMember) = 0;

    const QStringList locallyRequiredKeys();
};


} // namespace Met3D

#endif // WEATHERPREDICIONREADER_H
