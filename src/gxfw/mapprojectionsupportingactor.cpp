/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017-2020 Marc Rautenhaus [*, previously +]
**  Copyright 2020 Kameswarro Modali [*]
**  Copyright 2017 Bianca Tost [+]
**
**  + Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  * Regional Computing Center, Visual Data Analysis
**  Universitaet Hamburg, Hamburg, Germany
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
#include "mapprojectionsupportingactor.h"

// standard library imports
#include <iostream>
#include <cstdio>

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <gdal_priv.h>

// local application imports
#include "util/mstopwatch.h"
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"


using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MMapProjectionSupportingActor::MMapProjectionSupportingActor(QList<MapProjectionType> supportedProjections)
    : MActor(),
      mapProjection(MAPPROJECTION_CYLINDRICAL),
      rotateBBox(false),
      rotatedNorthPole(QPointF(-180., 90.)),
      projLibraryDefaultString("+proj=stere +a=6378273 +b=6356889.44891 +lat_0=90 +lat_ts=70 +lon_0=0"),
      projLibraryString(projLibraryDefaultString)
{
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Grid projection support enabled");

    mapProjectionPropertiesSubGroup = addProperty(
                GROUP_PROPERTY, "map projection support", nullptr);

    // Drop-down list for choosing type of projection.
    QStringList gridProjectionNames;
    for (MapProjectionType proj : supportedProjections)
    {
        gridProjectionNames << mapProjectionToString(proj);
    }
    mapProjectionTypesProperty = addProperty(
                ENUM_PROPERTY, "type of projection", mapProjectionPropertiesSubGroup);
    properties->mEnum()->setEnumNames(mapProjectionTypesProperty, gridProjectionNames);
    properties->mEnum()->setValue(mapProjectionTypesProperty, mapProjection);

    // Inputs for rotated lat-lon projection.
    rotateBBoxProperty = addProperty(BOOL_PROPERTY, "rotate bounding box",
                                     mapProjectionPropertiesSubGroup);
    properties->mBool()->setValue(rotateBBoxProperty, rotateBBox);
    rotateBBoxProperty->setEnabled(false);

    rotatedNorthPoleProperty = addProperty(
                POINTF_LONLAT_PROPERTY, "rotated north pole",
                mapProjectionPropertiesSubGroup);
    properties->mPointF()->setValue(rotatedNorthPoleProperty, rotatedNorthPole);
    rotatedNorthPoleProperty->setEnabled(false);

    // Proj.org string, see https://proj.org/operations/projections/index.html.
    projLibraryStringProperty = addProperty(
                STRING_PROPERTY, "proj-string",
                mapProjectionPropertiesSubGroup);
    properties->mString()->setValue(projLibraryStringProperty, projLibraryString);
    projLibraryStringProperty->setToolTip("Enter a valid proj-string, see: https://proj.org/operations/projections/index.html\n"
                                          "Note that this does NOT project the data - only this actor's geometry.");
    projLibraryStringProperty->setEnabled(false);

    projLibraryApplyProperty = addProperty(
                CLICK_PROPERTY, "apply projection",
                mapProjectionPropertiesSubGroup);
    projLibraryApplyProperty->setEnabled(false);

    endInitialiseQtProperties();
}


MMapProjectionSupportingActor::~MMapProjectionSupportingActor()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MMapProjectionSupportingActor::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MMapProjectionSupportingActor::getSettingsID());

    settings->setValue("mapProjection", mapProjectionToString(mapProjection));
    settings->setValue("rotateBoundingBox", rotateBBox);
    settings->setValue("rotatedNorthPole", rotatedNorthPole);
    settings->setValue("projString", projLibraryString);

    settings->endGroup();
}


void MMapProjectionSupportingActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MMapProjectionSupportingActor::getSettingsID());

    properties->mEnum()->setValue(
                mapProjectionTypesProperty, stringToMapProjection(
                    (settings->value("mapProjection",
                                     mapProjectionToString(MAPPROJECTION_CYLINDRICAL))
                     ).toString()));
    properties->mBool()->setValue(
                rotateBBoxProperty,
                settings->value("rotateBoundingBox", false).toBool());
    properties->mPointF()->setValue(
                rotatedNorthPoleProperty,
                settings->value("rotatedNorthPole",
                                QPointF(-180., 90.)).toPointF());
    // Explicitly load value into string variable here as changing the property
    // will not trigger a recomputation of graticule/map geometry.
    projLibraryString = settings->value("projString",
                                        projLibraryDefaultString).toString();
    properties->mString()->setValue(projLibraryStringProperty,
                                    projLibraryString);

    settings->endGroup();
}


void MMapProjectionSupportingActor::updateMapProjectionProperties()
{
    MapProjectionType projIndex = stringToMapProjection(
                properties->getEnumItem(mapProjectionTypesProperty));

    switch (projIndex)
    {
    case MAPPROJECTION_CYLINDRICAL:
        mapProjection = MAPPROJECTION_CYLINDRICAL;
        rotateBBoxProperty->setEnabled(false);
        rotatedNorthPoleProperty->setEnabled(false);
        projLibraryStringProperty->setEnabled(false);
        projLibraryApplyProperty->setEnabled(false);
        break;
    case MAPPROJECTION_ROTATEDLATLON:
        mapProjection = MAPPROJECTION_ROTATEDLATLON;
        rotateBBoxProperty->setEnabled(true);
        rotatedNorthPoleProperty->setEnabled(true);
        projLibraryStringProperty->setEnabled(false);
        projLibraryApplyProperty->setEnabled(false);
        break;
    case MAPPROJECTION_PROJ_LIBRARY:
        mapProjection = MAPPROJECTION_PROJ_LIBRARY;
        rotateBBoxProperty->setEnabled(false);
        rotatedNorthPoleProperty->setEnabled(false);
        projLibraryStringProperty->setEnabled(true);
        projLibraryApplyProperty->setEnabled(true);
        break;
    }
}


MMapProjectionSupportingActor::MapProjectionType
MMapProjectionSupportingActor::stringToMapProjection(QString gridProjectionName)
{
    if (gridProjectionName == "cylindrical")
    {
        return MAPPROJECTION_CYLINDRICAL;
    }
    else if (gridProjectionName == "rotated lat.-lon.")
    {
        return MAPPROJECTION_ROTATEDLATLON;
    }
    else if (gridProjectionName == "proj.org projection")
    {
        return MAPPROJECTION_PROJ_LIBRARY;
    }
    else
    {
        return MAPPROJECTION_CYLINDRICAL;
    }
}


QString MMapProjectionSupportingActor::mapProjectionToString(
        MapProjectionType gridProjection)
{
    switch (gridProjection)
    {
    case MAPPROJECTION_CYLINDRICAL: return "cylindrical";
    case MAPPROJECTION_ROTATEDLATLON: return "rotated lat.-lon.";
    case MAPPROJECTION_PROJ_LIBRARY: return "proj.org projection";
    default: return "cylindrical";
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/


} // namespace Met3D
