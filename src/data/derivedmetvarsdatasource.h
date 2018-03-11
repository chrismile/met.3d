/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
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
#ifndef DERIVEDMETVARSDATASOURCE_H
#define DERIVEDMETVARSDATASOURCE_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "processingwpdatasource.h"
#include "structuredgrid.h"
#include "datarequest.h"

namespace Met3D
{

/**
 @brief The MDerivedDataFieldProcessor class is the abstract base class for all
 classes that derive a data field, e.g., wind speed or potential temperature.
 */
class MDerivedDataFieldProcessor
{
public:
    MDerivedDataFieldProcessor(QString standardName,
                               QStringList requiredInputVariables);

    virtual ~MDerivedDataFieldProcessor() {}

    QString getStandardName() { return standardName; }

    QStringList getRequiredInputVariables() { return requiredInputVariables; }

    /**
      This method computes the derived data field and needs to be implemented
      in any derived class.

      It is called from @ref MDerivedMetVarsDataSource::produceData() if the
      corresponding variable is requested.

      @p inputGrids contains the required input data fields in the order
      specified in @ref requiredInputVariables. @p derivedGrid contains
      a pre-initialized result grid that already contains lon/lat/lev etc.
      information copied from the first grid in @p inputGrids.
     */
    virtual void compute(QList<MStructuredGrid*>& inputGrids,
                         MStructuredGrid *derivedGrid) = 0;

private:
    QString standardName;
    QStringList requiredInputVariables;
};


/**
  @brief MDerivedMetVarsDataSource derives meteorological variables from basic
  forecast parameters.
  */
class MDerivedMetVarsDataSource
        : public MProcessingWeatherPredictionDataSource
{
public:
    MDerivedMetVarsDataSource();
    ~MDerivedMetVarsDataSource();

    MStructuredGrid* produceData(MDataRequest request);

    MTask* createTaskGraph(MDataRequest request);

    void setInputSource(MWeatherPredictionDataSource* s);

    /**
      Defines a mapping from a CF standard name to an input variable name,
      e.g., "eastward_wind" to "u (an)". This is required to obtain a
      unique mapping of which input variables are used to derive new variables.
      (Otherwise, a case can easily occur in which the input source provides
      two variables with identical standard name. Then, the variable that
      would be used would be random.)

      This function needs to be called for all input variables that shall
      be used.
     */
    void setInputVariable(QString standardName, QString inputVariableName);

    /**
      Registers a data field processor. Needs to be called for each variable
      that shall be derived.

      Ownership of the object that is passed is assumed to be handed over to
      this object; it is deleted when this object is deleted.

      @note Currently, data sources are registered in the constructor.
     */
    void registerDerivedDataFieldProcessor(MDerivedDataFieldProcessor *processor);

    QList<MVerticalLevelType> availableLevelTypes();

    QStringList availableVariables(MVerticalLevelType levelType);

    QSet<unsigned int> availableEnsembleMembers(MVerticalLevelType levelType,
                                                const QString& variableName);

    QList<QDateTime> availableInitTimes(MVerticalLevelType levelType,
                                        const QString& variableName);

    QList<QDateTime> availableValidTimes(MVerticalLevelType levelType,
                                         const QString& variableName,
                                         const QDateTime& initTime);

    QString variableLongName(MVerticalLevelType levelType,
                             const QString&     variableName);

    QString variableStandardName(MVerticalLevelType levelType,
                             const QString&     variableName);

    QString variableUnits(MVerticalLevelType levelType,
                             const QString&     variableName);


protected:
    const QStringList locallyRequiredKeys();

    /**
      Returns the defined input variable name for a given standard name,
      if this has been set with @ref setInputVariable(). Otherwise, returns
      an empty string.
     */
    QString getInputVariableNameFromStdName(QString stdName);

    /**
      Updates a passed standard name and level type according to an enforced
      level type being encoded in the standard name.

      Examples:
      Passing a standard name of "air_temperature" and a leveltype of
      "HYBRID_SIGMA_PRESSURE_3D" will not change anything.
      Passing a standard name of "surface_geopotential/SURFACE_2D" and a
      leveltype of "HYBRID_SIGMA_PRESSURE_3D" will result in a standard name
      of "surface_geopotential" and a leveltype of "SURFACE_2D".
     */
    bool updateStdNameAndLevelType(QString *stdName,
                                   MVerticalLevelType *levelType);

    MWeatherPredictionDataSource* inputSource;

    QMap<QString, MDerivedDataFieldProcessor*> registeredDerivedDataProcessors;
    QMap<QString, QStringList> requiredInputVariablesList;
    QMap<QString, QString> variableStandardNameToInputNameMapping;
};


/******************************************************************************
***                            DATA PROCESSORS                              ***
*******************************************************************************/

class MHorizontalWindSpeedProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MHorizontalWindSpeedProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MPotentialTemperatureProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MPotentialTemperatureProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


class MGeopotentialHeightProcessor
        : public MDerivedDataFieldProcessor
{
public:
    MGeopotentialHeightProcessor();

    void compute(QList<MStructuredGrid*>& inputGrids,
                 MStructuredGrid *derivedGrid);
};


} // namespace Met3D

#endif // DERIVEDMETVARSDATASOURCE_H
