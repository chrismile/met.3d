/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2020 Marc Rautenhaus [1][2]
**  Copyright 2015-2017 Bianca Tost [1]
**  Copyright 2020      Andreas Beckert [2]
**
**  [1] Computer Graphics and Visualization Group
**      Technische Universitaet Muenchen, Garching, Germany
**
**  [2] Regional Computing Center, Visualization
**      Universitaet Hamburg, Hamburg, Germany
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
#include "nwpactorvariableproperties.h"

// standard library imports

// related third party imports
#include <QtCore>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/nwpactorvariable.h"
#include "gxfw/nwpmultivaractor.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                       MRequestPropertiesGroup                           ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MRequestProperties::MRequestProperties(MNWPActorVariable *actorVar)
    : actorVariable(actorVar)
{
}


/******************************************************************************
***                      MRequestPropertiesFactory                          ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MRequestPropertiesFactory::MRequestPropertiesFactory(MNWPActorVariable *actorVar)
    : actorVariable(actorVar)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MRequestPropertiesFactory::updateProperties(
        QList<MRequestProperties*> *propertiesList,
        const QStringList &keysRequiredByPipeline)
{
    // MVerticalRegridProperties
    updateTypedProperties<MVerticalRegridProperties>(
                QStringList() << "REGRID",
                propertiesList, keysRequiredByPipeline);

    // MTrajectoryFilterProperties
    updateTypedProperties<MTrajectoryFilterProperties>(
                QStringList() << "FILTER_PRESSURE_TIME" << "TRY_PRECOMPUTED"
                << "PWCB_ENSEMBLE_MEMBER",
                propertiesList, keysRequiredByPipeline);

    // MTrajectoryGriddingProperties
    updateTypedProperties<MTrajectoryGriddingProperties>(
                QStringList() << "GRID_GEOMETRY",
                propertiesList, keysRequiredByPipeline);

    // MTrajectoryThinOutProperties
    updateTypedProperties<MTrajectoryThinOutProperties>(
                QStringList() << "THINOUT_STRIDE",
                propertiesList, keysRequiredByPipeline);

    // MProbabilityRegionProperties
    updateTypedProperties<MProbabilityRegionProperties>(
                QStringList() << "PROBABILITY",
                propertiesList, keysRequiredByPipeline);

    // MSmoothProperties
    updateTypedProperties<MSmoothProperties>(
                QStringList() << "SMOOTH",
                propertiesList, keysRequiredByPipeline);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

template<typename T> void MRequestPropertiesFactory::updateTypedProperties(
        const QStringList& keysHandledByType,
        QList<MRequestProperties*> *propertiesList,
        const QStringList& keysRequiredByPipeline)
{
    // Does the list of keys required by the pipeline contain all keys
    // provided by property type T?
    for (int i = 0; i < keysHandledByType.size(); i++)
    {
        if (!keysRequiredByPipeline.contains(keysHandledByType[i]))
        {

            // Keys required by pipeline are not present. Check if properties
            // exist from earlier pipeline connection. If yes, delete.
            for (int i = 0; i < propertiesList->size(); i++)
            {
                if (dynamic_cast<T*>(propertiesList->at(i)))
                {
                    delete propertiesList->takeAt(i);
                    return;
                }
            }
            return;
        }
    }

    // All provided keys are contained. Check if an instance of T is already
    // contained in the actor variables' properties list.
    for (int i = 0; i < propertiesList->size(); i++)
    {
        if (dynamic_cast<T*>(propertiesList->at(i)))
        {
            return;
        }
    }

    // This is not the case, hence add a new one.
    propertiesList->append(new T(actorVariable));
}


/******************************************************************************
***                      SPECIFIC IMPLEMENTATIONS                           ***
*******************************************************************************/

/******************************************************************************
***                      MVerticalRegridProperties                          ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MVerticalRegridProperties::MVerticalRegridProperties(
        MNWPActorVariable *actorVar)
    : MRequestProperties(actorVar),
      regridMode(""),
      ignorePropertyChangeEvents(false)
{
    MNWPMultiVarActor *a = actorVar->getActor();
    MQtProperties *properties = a->getQtProperties();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    a->beginInitialiseQtProperties();

    QtProperty *groupProperty = actorVar->getPropertyGroup(
                "vertical regrid");

    QStringList regridModeNames;
    regridModeNames << "no regrid"
                    << "to hybrid/mean psfc"
                    << "to hybrid/min psfc"
                    << "to hybrid/const 1013.25 hPa"
                    << "to pressure levels/ECMWF standard"
                    << "to pressure levels/const 1013.25 hPa";
    regridModeProperty = a->addProperty(ENUM_PROPERTY, "regrid mode",
                                        groupProperty);
    properties->mEnum()->setEnumNames(regridModeProperty, regridModeNames);

//TODO (mr, 01Feb2015) -- find a more elegant way to sync properties;
//                        possibly with Qt signals?
    enableBroadcastProperty = a->addProperty(
                BOOL_PROPERTY, "broadcast to all variables",
                groupProperty);
    properties->mBool()->setValue(enableBroadcastProperty, false);

    a->endInitialiseQtProperties();
}


MVerticalRegridProperties::~MVerticalRegridProperties()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

bool MVerticalRegridProperties::onQtPropertyChanged(
        QtProperty *property, bool *redrawWithoutDataRequest)
{
    Q_UNUSED(redrawWithoutDataRequest);

    if (property == regridModeProperty)
    {
        MQtProperties *properties = actorVariable->getActor()->getQtProperties();
        int index = properties->mEnum()->value(regridModeProperty);

        switch (index)
        {
        case 0: // no regrid
            regridMode = "";
            break;
        case 1: // mean sfc pressure
            regridMode = "ML/MEAN";
            break;
        case 2: // min sfc pressure
            regridMode = "ML/MIN";
            break;
        case 3: // const sfc pressure
            regridMode = "ML/CONST_STANDARD_PSFC";
            break;
        case 4: // PL grid
            regridMode = "PL/HPA/10/50/100/200/250/300/400/500/700/850/925/1000";
            break;
        case 5: // PL grid, const sfc pressure
            regridMode = "PL/CONST_STANDARD_PSFC";
            break;
        }

        if (actorVariable->getActor()->suppressActorUpdates()) return false;
        actorVariable->triggerAsynchronousDataRequest(true);

        // If enabled, broadcast change to other actor variables.
        if (properties->mBool()->value(enableBroadcastProperty))
        {
            ignorePropertyChangeEvents = true;
            actorVariable->getActor()->broadcastPropertyChangedEvent(
                        MPropertyType::VerticalRegrid, &index);
            ignorePropertyChangeEvents = false;
        }
    }

    return false;
}


void MVerticalRegridProperties::addToRequest(MDataRequestHelper *rh)
{
    if (regridMode != "")
    {
        if (regridMode == "ML/MEAN")
            rh->insert("REGRID", QString("ML/MEAN/%1").arg(
                           MDataRequestHelper::uintSetToString(
                               actorVariable->selectedEnsembleMembers)));
        else if (regridMode == "ML/MIN")
            rh->insert("REGRID", QString("ML/MIN/%1").arg(
                           MDataRequestHelper::uintSetToString(
                               actorVariable->selectedEnsembleMembers)));
        else
            rh->insert("REGRID", regridMode);
    }
}


void MVerticalRegridProperties::actorPropertyChangeEvent(
        MPropertyType::ChangeNotification ptype, void *value)
{
    if (ignorePropertyChangeEvents) return;

    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    if (ptype == MPropertyType::VerticalRegrid)
    {
        // Prevent further broadcasts.
        properties->mBool()->setValue(enableBroadcastProperty, false);

        int index = *(static_cast<int*>(value));
        properties->mEnum()->setValue(regridModeProperty, index);
    }
}


void MVerticalRegridProperties::saveConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();
    settings->beginGroup("VerticalRegrid");
    settings->setValue("regridMode",
                       properties->getEnumItem(regridModeProperty));
    settings->setValue("enableBroadcast",
                       properties->mBool()->value(enableBroadcastProperty));
    settings->endGroup();
}


void MVerticalRegridProperties::loadConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();
    settings->beginGroup("VerticalRegrid");
    properties->setEnumItem(
                regridModeProperty,
                settings->value("regridMode", "no regrid").toString());
    properties->mBool()->setValue(
                enableBroadcastProperty,
                settings->value("enableBroadcast", false).toBool());
    settings->endGroup();
}


/******************************************************************************
***                     MTrajectoryFilterProperties                         ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryFilterProperties::MTrajectoryFilterProperties(
        MNWPActorVariable *actorVar)
    : MRequestProperties(actorVar)
{
    MNWPMultiVarActor *a = actorVar->getActor();
    MQtProperties *properties = a->getQtProperties();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    a->beginInitialiseQtProperties();

    QtProperty *groupProperty = actorVar->getPropertyGroup(
                "trajectory filter settings");

    enableFilterProperty = a->addProperty(BOOL_PROPERTY, "filter trajectories",
                                          groupProperty);
    properties->mBool()->setValue(enableFilterProperty, true);

    tryPrecomputedFilterProperty = a->addProperty(BOOL_PROPERTY,
                                                  "try precomputed filter",
                                                  groupProperty);
    properties->mBool()->setValue(tryPrecomputedFilterProperty, true);

    deltaPressureProperty = a->addProperty(DECORATEDDOUBLE_PROPERTY,
                                           "pressure difference", groupProperty);
    properties->setDDouble(deltaPressureProperty, 500., 1., 1050., 2, 5., " hPa");

    deltaTimeProperty = a->addProperty(DECORATEDDOUBLE_PROPERTY,
                                       "time interval", groupProperty);
    properties->setDDouble(deltaTimeProperty, 48, 1, 48, 0, 1, " hrs");

    filterUsedMembersProperty = a->addProperty(INT_PROPERTY,
                                               "num. used members", groupProperty);
    properties->setInt(filterUsedMembersProperty, 51, 1, 51, 1);

    a->endInitialiseQtProperties();
}


MTrajectoryFilterProperties::~MTrajectoryFilterProperties()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

bool MTrajectoryFilterProperties::onQtPropertyChanged(
        QtProperty *property, bool *redrawWithoutDataRequest)
{
    Q_UNUSED(redrawWithoutDataRequest);

    if ( property == enableFilterProperty
         || property == tryPrecomputedFilterProperty
         || property == deltaPressureProperty
         || property == deltaTimeProperty
         || property == filterUsedMembersProperty )
    {
        actorVariable->triggerAsynchronousDataRequest(false);
        return true;
    }

    return false;
}


void MTrajectoryFilterProperties::addToRequest(MDataRequestHelper *rh)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    // Specify trajectory filter.
    bool filteringEnabled = properties->mBool()->value(enableFilterProperty);
    if (filteringEnabled)
    {
        float deltaPressure_hPa = properties->mDDouble()->value(
                    deltaPressureProperty);
        int deltaTime_hrs = properties->mDDouble()->value(deltaTimeProperty);
        // Request is e.g. 500/48 for 500 hPa in 48 hours.
        rh->insert("FILTER_PRESSURE_TIME",
                  QString("%1/%2").arg(deltaPressure_hPa).arg(deltaTime_hrs));
    }
    else
    {
        rh->insert("FILTER_PRESSURE_TIME", "ALL");
    }

    bool tryPrecomputedFiltering = properties->mBool()->value(
                tryPrecomputedFilterProperty);
    rh->insert("TRY_PRECOMPUTED", tryPrecomputedFiltering ? 1 : 0);

    // Specify the members that will be used for computing the probabilities.
    unsigned int usedMembers = properties->mInt()->value(filterUsedMembersProperty);
    if (usedMembers > 1)
    {
        rh->insert("PWCB_ENSEMBLE_MEMBER", QString("0/%1").arg(usedMembers-1));
    }
    else
    {
        // Special case if only one member shall be used: Use the member from
        // the global member setting.
        unsigned int member = actorVariable->getEnsembleMember();
        rh->insert("PWCB_ENSEMBLE_MEMBER", QString("%1/%1").arg(member));
    }
}


void MTrajectoryFilterProperties::saveConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    settings->setValue("filteringEnabled",
                       properties->mBool()->value(enableFilterProperty));

    settings->setValue("deltaPressure_hPa",
                       properties->mDDouble()->value(deltaPressureProperty));
    settings->setValue("deltaTime_hrs",
                       properties->mDDouble()->value(deltaTimeProperty));

    settings->setValue("tryPrecomputedFiltering",
                       properties->mBool()->value(tryPrecomputedFilterProperty));
    settings->setValue("usedMembers",
                       properties->mInt()->value(filterUsedMembersProperty));
}


void MTrajectoryFilterProperties::loadConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    properties->mBool()->setValue(
                enableFilterProperty,
                settings->value("filteringEnabled", true).toBool());

    properties->mDDouble()->setValue(
                deltaPressureProperty,
                settings->value("deltaPressure_hPa", 500.).toDouble());
    properties->mDDouble()->setValue(
                deltaTimeProperty,
                settings->value("deltaTime_hrs", 48.).toDouble());

    properties->mBool()->setValue(
                tryPrecomputedFilterProperty,
                settings->value("tryPrecomputedFiltering", true).toBool());

    properties->mInt()->setValue(
                filterUsedMembersProperty,
                settings->value("usedMembers", 51).toInt());
}


/******************************************************************************
***                    MTrajectoryGriddingProperties                        ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryGriddingProperties::MTrajectoryGriddingProperties(
        MNWPActorVariable *actorVar)
    : MRequestProperties(actorVar)
{
    MNWPMultiVarActor *a = actorVar->getActor();
    MQtProperties *properties = a->getQtProperties();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    a->beginInitialiseQtProperties();

    QtProperty *groupProperty = actorVar->getPropertyGroup(
                "trajectory gridding settings");

    applySettingsProperty = a->addProperty(
                CLICK_PROPERTY, "apply settings", groupProperty);

    scaleParcelThicknessProperty = a->addProperty(
                BOOL_PROPERTY, "scale air parcel thickness", groupProperty);
    properties->mBool()->setValue(scaleParcelThicknessProperty, false);

    westernLonProperty = a->addProperty(
                DECORATEDDOUBLE_PROPERTY, "western lon", groupProperty);
    properties->setDDouble(
                westernLonProperty, -100., -360, 360, 2, 1., " degrees");

    deltaLonProperty = a->addProperty(
                DECORATEDDOUBLE_PROPERTY, "delta lon", groupProperty);
    properties->setDDouble(
                deltaLonProperty, 1,  0.01, 90, 2, 1, " degrees");

    numLonProperty = a->addProperty(
                INT_PROPERTY, "num. longitudes", groupProperty);
    properties->mInt()->setMinimum(numLonProperty, 1);
    properties->mInt()->setValue(numLonProperty, 131);

    northerLatProperty = a->addProperty(
                DECORATEDDOUBLE_PROPERTY, "norther lat", groupProperty);
    properties->setDDouble(northerLatProperty, 85., -90, 90, 2, 1, " degrees");

    deltaLatProperty = a->addProperty(
                DECORATEDDOUBLE_PROPERTY, "delta lon", groupProperty);
    properties->setDDouble(deltaLatProperty, 1, 0.01, 90, 2, 1, " degrees");

    numLatProperty = a->addProperty(
                INT_PROPERTY, "num. latitudes", groupProperty);
    properties->mInt()->setMinimum(numLatProperty, 1);
    properties->mInt()->setValue(numLatProperty, 66);

    QStringList gridType; gridType << "regular";
    verticalGridTypeProperty = a->addProperty(
                ENUM_PROPERTY, "vertical grid type", groupProperty);
    properties->mEnum()->setEnumNames(verticalGridTypeProperty, gridType);

    bottomPressureProperty = a->addProperty(
                DECORATEDDOUBLE_PROPERTY, "bottom pressure", groupProperty);
    properties->setDDouble(
                bottomPressureProperty, 1050., 20, 1050, 2, 5., " hPa");

    topPressureProperty = a->addProperty(
                DECORATEDDOUBLE_PROPERTY, "top pressure", groupProperty);
    properties->setDDouble(topPressureProperty, 100., 20, 1050, 2, 5., " hPa");

    numPressureProperty = a->addProperty(
                INT_PROPERTY, "num. vertical levels", groupProperty);
    properties->mInt()->setMinimum(numPressureProperty, 1);
    properties->mInt()->setValue(numPressureProperty, 20);

    a->endInitialiseQtProperties();
}


MTrajectoryGriddingProperties::~MTrajectoryGriddingProperties()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

bool MTrajectoryGriddingProperties::onQtPropertyChanged(
        QtProperty *property, bool *redrawWithoutDataRequest)
{
    Q_UNUSED(redrawWithoutDataRequest);

    if (property == applySettingsProperty)
    {
        // These properties can change the size of the data grid; hence notify
        // the actor variable (set gridTopologyMayHaveChanged to true).
        actorVariable->triggerAsynchronousDataRequest(true);
        return true;
    }

    return false;
}


void MTrajectoryGriddingProperties::addToRequest(MDataRequestHelper *rh)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    // Gridding settings.
    int vertGridTypeIndex = properties->mEnum()->value(verticalGridTypeProperty);
    QString vertGridType = (vertGridTypeIndex == 0) ? "REGULAR" : "STRETCHED";

    float westernlon = properties->mDDouble()->value(westernLonProperty);
    float dlon       = properties->mDDouble()->value(deltaLonProperty);
    int   nlon       = properties->mInt()->value(numLonProperty);

    float northernlat = properties->mDDouble()->value(northerLatProperty);
    float dlat        = properties->mDDouble()->value(deltaLatProperty);
    int   nlat        = properties->mInt()->value(numLatProperty);

    float pbot = properties->mDDouble()->value(bottomPressureProperty);
    float ptop = properties->mDDouble()->value(topPressureProperty);
    int   np   = properties->mInt()->value(numPressureProperty);

    bool scaleParcelThickness = properties->mBool()->value(scaleParcelThicknessProperty);
    rh->insert("GRID_GEOMETRY", QString("%1/%2/%3/%4/%5/%6/%7/%8/%9/%10/%11")
               .arg(vertGridType)
               .arg(westernlon).arg(dlon).arg(nlon).arg(northernlat)
               .arg(dlat).arg(nlat).arg(pbot).arg(ptop).arg(np)
               .arg(scaleParcelThickness ? "1" : "0"));
}


void MTrajectoryGriddingProperties::saveConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    settings->setValue("scaleParcelThickness",
                       properties->mBool()->value(scaleParcelThicknessProperty));

    settings->setValue("westernlon",
                       properties->mDDouble()->value(westernLonProperty));
    settings->setValue("dlon",
                       properties->mDDouble()->value(deltaLonProperty));
    settings->setValue("nlon",
                       properties->mInt()->value(numLonProperty));

    settings->setValue("northernlat",
                       properties->mDDouble()->value(northerLatProperty));
    settings->setValue("dlat",
                       properties->mDDouble()->value(deltaLatProperty));
    settings->setValue("nlat",
                       properties->mInt()->value(numLatProperty));

    settings->setValue("pbot",
                       properties->mDDouble()->value(bottomPressureProperty));
    settings->setValue("ptop",
                       properties->mDDouble()->value(topPressureProperty));
    settings->setValue("np",
                       properties->mInt()->value(numPressureProperty));
}


void MTrajectoryGriddingProperties::loadConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    properties->mBool()->setValue(
                scaleParcelThicknessProperty,
                settings->value("scaleParcelThickness", true).toBool());

    properties->mDDouble()->setValue(
                westernLonProperty,
                settings->value("westernlon", -90.).toDouble());
    properties->mDDouble()->setValue(
                deltaLonProperty,
                settings->value("dlon", 1.).toDouble());
    properties->mInt()->setValue(
                numLonProperty,
                settings->value("nlon", 180).toInt());

    properties->mDDouble()->setValue(
                northerLatProperty,
                settings->value("northernlat", -90.).toDouble());
    properties->mDDouble()->setValue(
                deltaLatProperty,
                settings->value("dlat", 1.).toDouble());
    properties->mInt()->setValue(
                numLatProperty,
                settings->value("nlat", 180).toInt());

    properties->mDDouble()->setValue(
                bottomPressureProperty,
                settings->value("pbot", 1000.).toDouble());
    properties->mDDouble()->setValue(
                topPressureProperty,
                settings->value("ptop", 100.).toDouble());
    properties->mInt()->setValue(
                numPressureProperty,
                settings->value("np", 20).toInt());
}


/******************************************************************************
***                     MTrajectoryThinOutProperties                        ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryThinOutProperties::MTrajectoryThinOutProperties(
        MNWPActorVariable *actorVar)
    : MRequestProperties(actorVar)
{
    MNWPMultiVarActor *a = actorVar->getActor();
    MQtProperties *properties = a->getQtProperties();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    a->beginInitialiseQtProperties();

    QtProperty *groupProperty = actorVariable->getPropertyGroup(
                "trajectory thinout settings");

    applySettingsProperty = a->addProperty(
                CLICK_PROPERTY, "apply settings", groupProperty);

    enableThinOutProperty = a->addProperty(
                BOOL_PROPERTY, "enable thin out", groupProperty);
    properties->mBool()->setValue(enableThinOutProperty, false);

    strideLonProperty = a->addProperty(
                INT_PROPERTY, "lon stride", groupProperty);
    properties->mInt()->setMinimum(strideLonProperty, 1);
    properties->mInt()->setValue(strideLonProperty, 1);

    strideLatProperty = a->addProperty(
                INT_PROPERTY, "lat stride", groupProperty);
    properties->mInt()->setMinimum(strideLatProperty, 1);
    properties->mInt()->setValue(strideLatProperty, 1);

    strideLevProperty = a->addProperty(
                INT_PROPERTY, "lev stride", groupProperty);
    properties->mInt()->setMinimum(strideLevProperty, 1);
    properties->mInt()->setValue(strideLevProperty, 1);

    a->endInitialiseQtProperties();
}


MTrajectoryThinOutProperties::~MTrajectoryThinOutProperties()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

bool MTrajectoryThinOutProperties::onQtPropertyChanged(
        QtProperty *property, bool *redrawWithoutDataRequest)
{
    Q_UNUSED(redrawWithoutDataRequest);

    if (property == applySettingsProperty)
    {
        // These properties can change the size of the data grid; hence notify
        // the actor variable (set gridTopologyMayHaveChanged to true).
        actorVariable->triggerAsynchronousDataRequest(true);
        return true;
    }

    return false;
}


void MTrajectoryThinOutProperties::addToRequest(MDataRequestHelper *rh)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    bool enableThinOut = properties->mBool()->value(enableThinOutProperty);
    if (enableThinOut)
    {
        int strideLon = properties->mInt()->value(strideLonProperty);
        int strideLat = properties->mInt()->value(strideLatProperty);
        int strideLev = properties->mInt()->value(strideLevProperty);
        rh->insert("THINOUT_STRIDE", QString("%1/%2/%3").arg(strideLon)
                   .arg(strideLat).arg(strideLev));
    }
}


void MTrajectoryThinOutProperties::saveConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    settings->setValue("enableThinOut",
                       properties->mBool()->value(enableThinOutProperty));

    settings->setValue("strideLon",
                       properties->mInt()->value(strideLonProperty));
    settings->setValue("strideLat",
                       properties->mInt()->value(strideLatProperty));
    settings->setValue("strideLev",
                       properties->mInt()->value(strideLevProperty));
}


void MTrajectoryThinOutProperties::loadConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    properties->mBool()->setValue(
                enableThinOutProperty,
                settings->value("enableThinOut", true).toBool());

    properties->mInt()->setValue(
                strideLonProperty,
                settings->value("strideLon", 1).toInt());
    properties->mInt()->setValue(
                strideLatProperty,
                settings->value("strideLat", 1).toInt());
    properties->mInt()->setValue(
                strideLevProperty,
                settings->value("strideLev", 1).toInt());
}


/******************************************************************************
***                     MProbabilityRegionProperties                        ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MProbabilityRegionProperties::MProbabilityRegionProperties(
        MNWPActorVariable *actorVar)
    : MRequestProperties(actorVar)
{
    MNWPMultiVarActor *a = actorVar->getActor();
    MQtProperties *properties = a->getQtProperties();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    a->beginInitialiseQtProperties();

    QtProperty *groupProperty = actorVariable->getPropertyGroup(
                "region contribution");

    probabilityRegionIsovalueProperty = a->addProperty(
                DECORATEDDOUBLE_PROPERTY, "prob. region. isoval", groupProperty);
    properties->setDDouble(
                probabilityRegionIsovalueProperty, 0.3, 0, 1, 3, 0.1, " (0-1)");

    a->endInitialiseQtProperties();
}


MProbabilityRegionProperties::~MProbabilityRegionProperties()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

bool MProbabilityRegionProperties::onQtPropertyChanged(
        QtProperty *property, bool *redrawWithoutDataRequest)
{
    Q_UNUSED(redrawWithoutDataRequest);

    if (property == probabilityRegionIsovalueProperty)
    {
        actorVariable->triggerAsynchronousDataRequest(false);
        return true;
    }

    return false;
}


void MProbabilityRegionProperties::addToRequest(MDataRequestHelper *rh)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    if (rh->contains("ENS_OPERATION"))
        if ( ! rh->value("ENS_OPERATION").startsWith("P")) return;

    float probabilityRegionDetectionIsovalue =
            properties->mDDouble()->value(probabilityRegionIsovalueProperty);
    rh->insert("PROBABILITY",
               QString("%1").arg(probabilityRegionDetectionIsovalue));
}


void MProbabilityRegionProperties::actorPropertyChangeEvent(
        MPropertyType::ChangeNotification ptype, void *value)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    if (ptype == MPropertyType::IsoValue)
    {
        float v = *(static_cast<float*>(value));
        properties->mDDouble()->setValue(probabilityRegionIsovalueProperty, v);
    }
}


void MProbabilityRegionProperties::saveConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    settings->setValue("probabilityRegionDetectionIsovalue",
                       properties->mDDouble()->value(probabilityRegionIsovalueProperty));
}


void MProbabilityRegionProperties::loadConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();

    properties->mDDouble()->setValue(
                probabilityRegionIsovalueProperty,
                settings->value("probabilityRegionDetectionIsovalue", 0.).toDouble());
}


/******************************************************************************
***                     MSmoothingProperties                        ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSmoothProperties::MSmoothProperties(
        MNWPActorVariable *actorVar)
    : MRequestProperties(actorVar),
      smoothMode(DISABLE_FILTER)
{
    MNWPMultiVarActor *a = actorVar->getActor();
    MQtProperties *properties = a->getQtProperties();

    // Create and initialize QtProperties for the GUI.
    // ===============================================
    a->beginInitialiseQtProperties();
    groupProperty = actorVar->getPropertyGroup("horizontal smoothing");

    recomputeOnPropertyChange = a->addProperty(
                BOOL_PROPERTY, "recompute on property change", groupProperty);
    applySettingsProperty = a->addProperty(
                CLICK_PROPERTY, "compute", groupProperty);
    QStringList smoothModeNames;
    // Comment the smoothModeName if you want that it does not show up in the
    // drop down menu of the GUI.
    smoothModeNames << smoothModeToString(DISABLE_FILTER)
                    << smoothModeToString(GAUSS_DISTANCE)
                    << smoothModeToString(BOX_BLUR_DISTANCE_FAST)
                    << smoothModeToString(UNIFORM_WEIGHTED_GRIDPOINTS)
                    << smoothModeToString(GAUSS_GRIDPOINTS)
//                    << smoothModeToString(BOX_BLUR_GRIDPOINTS_SLOW)
                    << smoothModeToString(BOX_BLUR_GRIDPOINTS_FAST);
    smoothModeProperty = a->addProperty(
                ENUM_PROPERTY, "smooth mode", groupProperty);
    properties->mEnum()->setEnumNames(smoothModeProperty, smoothModeNames);
    properties->mEnum()->setValue(smoothModeProperty, smoothMode);

    smoothStDevKmProperty = a->addProperty(
                DOUBLE_PROPERTY, "standard deviation (km)", groupProperty);
    properties->setDouble(
                smoothStDevKmProperty, 10, 0.5, 1000.0, 1, 0.5);
    smoothStDevKmProperty->setEnabled(false);
    smoothStDevGridboxProperty = a->addProperty(
                INT_PROPERTY, "standard deviation (grid cells)", groupProperty);
    properties->setInt(
                smoothStDevGridboxProperty, 3, 1, 500, 1);
    smoothStDevGridboxProperty->setEnabled(false);
    a->endInitialiseQtProperties();
}


MSmoothProperties::~MSmoothProperties()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

bool MSmoothProperties::onQtPropertyChanged(
        QtProperty *property, bool *redrawWithoutDataRequest)
{
    Q_UNUSED(redrawWithoutDataRequest);

    if ((property == applySettingsProperty)
            || (property == smoothModeProperty)
            || (property == smoothStDevKmProperty)
            || (property == smoothStDevGridboxProperty))
    {
        MQtProperties *properties
                = actorVariable->getActor()->getQtProperties();
        SmoothModeTypes index = stringToSmoothMode(properties->getEnumItem(
                                                       smoothModeProperty));
        switch (index)
        {
        case DISABLE_FILTER: // no smoothing
            smoothMode = DISABLE_FILTER;
            smoothStDevKmProperty->setEnabled(false);
            smoothStDevGridboxProperty->setEnabled(false);
            break;
        case GAUSS_DISTANCE:
            smoothMode = GAUSS_DISTANCE;
            smoothStDevKmProperty->setEnabled(true);
            smoothStDevGridboxProperty->setEnabled(false);
            break;
        case BOX_BLUR_DISTANCE_FAST:
            smoothMode = BOX_BLUR_DISTANCE_FAST;
            smoothStDevKmProperty->setEnabled(true);
            smoothStDevGridboxProperty->setEnabled(false);
            break;
        case UNIFORM_WEIGHTED_GRIDPOINTS:
            smoothMode = UNIFORM_WEIGHTED_GRIDPOINTS;
            smoothStDevKmProperty->setEnabled(false);
            smoothStDevGridboxProperty->setEnabled(true);
            break;
        case GAUSS_GRIDPOINTS:
            smoothMode =  GAUSS_GRIDPOINTS;
            smoothStDevKmProperty->setEnabled(false);
            smoothStDevGridboxProperty->setEnabled(true);
            break;
        case BOX_BLUR_GRIDPOINTS_SLOW:
            smoothMode = BOX_BLUR_GRIDPOINTS_SLOW;
            smoothStDevKmProperty->setEnabled(false);
            smoothStDevGridboxProperty->setEnabled(true);
            break;
        case BOX_BLUR_GRIDPOINTS_FAST:
            smoothMode = BOX_BLUR_GRIDPOINTS_FAST;
            smoothStDevKmProperty->setEnabled(false);
            smoothStDevGridboxProperty->setEnabled(true);
            break;
        }
        if (properties->mBool()->value(recomputeOnPropertyChange))
        {
            actorVariable->triggerAsynchronousDataRequest(true);
            return true;
        }
        else if (property == applySettingsProperty)
        {
            actorVariable->triggerAsynchronousDataRequest(true);
            return true;
        }
        else
        {
            return false;
        }
    }
    return false;
}


void MSmoothProperties::addToRequest(MDataRequestHelper *rh)
{
    if (smoothMode != DISABLE_FILTER)
    {
        MQtProperties *properties =
                actorVariable->getActor()->getQtProperties();
        float smoothStDev_km =
                properties->mDouble()->value(smoothStDevKmProperty);
        float smoothStdGridbox =
                properties->mInt()->value(smoothStDevGridboxProperty);
        rh->insert("SMOOTH", QString("%1/%2/%3")
                   .arg(smoothMode).arg(smoothStDev_km).arg(smoothStdGridbox));
    }
}


void MSmoothProperties::saveConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();
    settings->beginGroup("SmoothFilter");
    settings->setValue("smoothMode", smoothModeToString(smoothMode));
    settings->setValue("standardDeviation_km",
                       properties->mDouble()->value(smoothStDevKmProperty));
    settings->setValue("standardDeviation_gridcells",
                       properties->mInt()->value(smoothStDevGridboxProperty));
    settings->endGroup();
}


void MSmoothProperties::loadConfiguration(QSettings *settings)
{
    MQtProperties *properties = actorVariable->getActor()->getQtProperties();
    settings->beginGroup("SmoothFilter");
    properties->mEnum()->setValue(
                smoothModeProperty, stringToSmoothMode(
                    (settings->value("smoothMode",
                                     smoothModeToString(DISABLE_FILTER))
                     ).toString()));
    properties->mDouble()->setValue(
                smoothStDevKmProperty,
                settings->value("standardDeviation_km", 10.0).toDouble());
    properties->mInt()->setValue(
                smoothStDevGridboxProperty,
                settings->value("standardDeviation_gridcells", 3).toInt());
    settings->endGroup();
}


MSmoothProperties::SmoothModeTypes MSmoothProperties::stringToSmoothMode(
        QString smoothModeName)
{
    if (smoothModeName == "disabled")
    {
        return DISABLE_FILTER;
    }
    else if (smoothModeName == "horizontalGauss_distance")
    {
        return GAUSS_DISTANCE;
    }
    else if (smoothModeName == "horizontalBoxBlur_distance")
    {
        return BOX_BLUR_DISTANCE_FAST;
    }
    else if (smoothModeName == "horizontalUniformWeights_gridcells")
    {
        return UNIFORM_WEIGHTED_GRIDPOINTS;
    }
    else if (smoothModeName == "horizontalGauss_gridcells")
    {
        return GAUSS_GRIDPOINTS;
    }
    else if (smoothModeName == "horizontalBoxBlurSlow_gridcells")
    {
        return BOX_BLUR_GRIDPOINTS_SLOW;
    }
    else if (smoothModeName == "horizontalBoxBlur_gridcells")
    {
        return BOX_BLUR_GRIDPOINTS_FAST;
    }
    else
    {
        return DISABLE_FILTER;
    }
}


QString MSmoothProperties::smoothModeToString(
        SmoothModeTypes smoothModeType)
{
    switch (smoothModeType)
    {
        case DISABLE_FILTER: return "disabled";
        case GAUSS_DISTANCE: return "horizontalGauss_distance";
        case BOX_BLUR_DISTANCE_FAST: return "horizontalBoxBlur_distance";
        case UNIFORM_WEIGHTED_GRIDPOINTS: return
                "horizontalUniformWeights_gridcells";
        case GAUSS_GRIDPOINTS: return "horizontalGauss_gridcells";
        case BOX_BLUR_GRIDPOINTS_SLOW: return "horizontalBoxBlurSlow_gridcells";
        case BOX_BLUR_GRIDPOINTS_FAST: return "horizontalBoxBlur_gridcells";
    }
    return "disable filter";
}


} // namespace Met3D
