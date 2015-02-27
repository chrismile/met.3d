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
#ifndef NWPACTORVARIABLEPROPERTIES_H
#define NWPACTORVARIABLEPROPERTIES_H

// standard library imports

// related third party imports
#include <QtCore>
#include <QtProperty>

// local application imports
#include "data/datarequest.h"


namespace Met3D
{

class MNWPActorVariable;

struct MPropertyType
{
    enum ChangeNotification { IsoValue = 0, VerticalRegrid = 1 };
};


/**
  @brief Abstract base class for group properties for pipeline parameters. This
  is the "GUI" part of pipeline modules (for a pipeline module that requires
  specific parameters, implement corresponding GUI properties in a derived
  class).

  Derived classes have to be registered in the @ref MRequestPropertiesFactory
  @ref updateProperties() method. A @ref MNWPActorVariable can pass its
  properties and the keys required by its current data source to this method.
  Properties corresponding to the required keys are then added to the actor
  variable's property list.
 */
class MRequestProperties
{
public:
    MRequestProperties(MNWPActorVariable *actorVar);

    /**
     Called by @ref MNWPActorVariable::onQtPropertyChanged(). Returns true if
     @p property was accepted.
     */
    virtual bool onQtPropertyChanged(QtProperty *property,
                                     bool *redrawWithoutDataRequest) = 0;

    /**
     Adds key/value pairs corresponding to the properties of this class.
     */
    virtual void addToRequest(MDataRequestHelper *rh) = 0;

    /**
      Implement this method to react to changes of actor properties.
      Compare to @ref MNWPActorVariable::actorPropertyChangeEvent().
     */
    virtual void actorPropertyChangeEvent(
            MPropertyType::ChangeNotification ptype, void *value)
    { Q_UNUSED(ptype); Q_UNUSED(value); }


protected:
    MNWPActorVariable *actorVariable;

};


/**
  Factory for @ref MRequestProperties. Each @ref MNWPActorVariable owns an
  instance of this factory to update its properties.

  @note New classes derived from @ref MRequestProperties need to be added
  to @ref updateProperties().
 */
class MRequestPropertiesFactory
{
public:
    MRequestPropertiesFactory(MNWPActorVariable *actorVar);

    void updateProperties(QList<MRequestProperties*> *propertiesList,
                          const QStringList& keysRequiredByPipeline);

protected:
    template<typename T> void updateTypedProperties(
            const QStringList& keysHandledByType,
            QList<MRequestProperties*> *propertiesList,
            const QStringList& keysRequiredByPipeline);

    MNWPActorVariable *actorVariable;
};


/******************************************************************************
***                      SPECIFIC IMPLEMENTATIONS                           ***
*******************************************************************************/

/**
  Vertical regridding.
 */
class MVerticalRegridProperties : public MRequestProperties
{
public:
    MVerticalRegridProperties(MNWPActorVariable *actorVar);
    bool onQtPropertyChanged(QtProperty *property,
                             bool *redrawWithoutDataRequest);
    void addToRequest(MDataRequestHelper *rh);

    void actorPropertyChangeEvent(
            MPropertyType::ChangeNotification ptype, void *value);

protected:
    QtProperty *regridModeProperty;
    QString     regridMode;
    QtProperty *enableBroadcastProperty;

    bool ignorePropertyChangeEvents;
};


/**
  Trajectory filtering.
 */
class MTrajectoryFilterProperties : public MRequestProperties
{
public:
    MTrajectoryFilterProperties(MNWPActorVariable *actorVar);
    bool onQtPropertyChanged(QtProperty *property,
                             bool *redrawWithoutDataRequest);
    void addToRequest(MDataRequestHelper *rh);

protected:
    QtProperty *enableFilterProperty;
    QtProperty *tryPrecomputedFilterProperty;
    QtProperty *deltaPressureProperty; // filter trajectories according to
    QtProperty *deltaTimeProperty;     // this criterion
    QtProperty *filterUsedMembersProperty;
};


/**
  Grid specification for BL trajectories.
 */
class MTrajectoryGriddingProperties : public MRequestProperties
{
public:
    MTrajectoryGriddingProperties(MNWPActorVariable *actorVar);
    bool onQtPropertyChanged(QtProperty *property,
                             bool *redrawWithoutDataRequest);
    void addToRequest(MDataRequestHelper *rh);

protected:
    QtProperty *applySettingsProperty;
    QtProperty *scaleParcelThicknessProperty;

    QtProperty *westernLonProperty;
    QtProperty *deltaLonProperty;
    QtProperty *numLonProperty;

    QtProperty *northerLatProperty;
    QtProperty *deltaLatProperty;
    QtProperty *numLatProperty;

    QtProperty *verticalGridTypeProperty;
    QtProperty *bottomPressureProperty;
    QtProperty *topPressureProperty;
    QtProperty *numPressureProperty;
};


/**
  Probability region contribution.
 */
class MTrajectoryThinOutProperties : public MRequestProperties
{
public:
    MTrajectoryThinOutProperties(MNWPActorVariable *actorVar);
    bool onQtPropertyChanged(QtProperty *property,
                             bool *redrawWithoutDataRequest);
    void addToRequest(MDataRequestHelper *rh);

protected:
    QtProperty *applySettingsProperty;

    QtProperty *enableThinOutProperty;
    QtProperty *strideLonProperty;
    QtProperty *strideLatProperty;
    QtProperty *strideLevProperty;
};


/**
  Probability region contribution.
 */
class MProbabilityRegionProperties : public MRequestProperties
{
public:
    MProbabilityRegionProperties(MNWPActorVariable *actorVar);
    bool onQtPropertyChanged(QtProperty *property,
                             bool *redrawWithoutDataRequest);
    void addToRequest(MDataRequestHelper *rh);

    void actorPropertyChangeEvent(
            MPropertyType::ChangeNotification ptype, void *value);

protected:
    QtProperty *probabilityRegionIsovalueProperty;
};

} // namespace Met3D

#endif // NWPACTORVARIABLEPROPERTIES_H
