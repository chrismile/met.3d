/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Bianca Tost
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

MMapProjectionSupportingActor::MMapProjectionSupportingActor()
    : MActor(),
      gridProjection(GRIDPROJECTION_CYLINDRICAL),
      enableGridRotation(false),
      rotateBBox(false),
      rotatedNorthPole(QPointF(-180., 90.)),
      enableProjLibraryProjection(false)
{
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Grid projection support enabled");

    // projection sub-group header
    gridProjectionPropertiesSubGroup = addProperty(
                GROUP_PROPERTY, "grid projection support", nullptr);

    // drop-down list for choosing type of projection
    QStringList gridProjectionNames;
    gridProjectionNames << gridProjectionToString(GRIDPROJECTION_CYLINDRICAL)
                        << gridProjectionToString(GRIDPROJECTION_ROTATEDLATLON)
                        << gridProjectionToString(GRIDPROJECTION_PROJ_LIBRARY);
    gridProjectionTypesProperty = addProperty(
                ENUM_PROPERTY, "type of projection", gridProjectionPropertiesSubGroup);
    properties->mEnum()->setEnumNames(gridProjectionTypesProperty, gridProjectionNames);
    properties->mEnum()->setValue(gridProjectionTypesProperty, gridProjection);

    // inputs for grid projection: rotated lat lon
    rotateBBoxProperty = addProperty(BOOL_PROPERTY, "rotate bounding box",
                                     gridProjectionPropertiesSubGroup);
    properties->mBool()->setValue(rotateBBoxProperty, rotateBBox);
    rotateBBoxProperty->setEnabled(false);

    rotatedNorthPoleProperty = addProperty(
                POINTF_LONLAT_PROPERTY, "rotated north pole",
                gridProjectionPropertiesSubGroup);
    properties->mPointF()->setValue(rotatedNorthPoleProperty, rotatedNorthPole);
    rotatedNorthPoleProperty->setEnabled(false);

    projLibraryStringProperty = addProperty(
                    STRING_PROPERTY, "Proj Library Settings String",
                    gridProjectionPropertiesSubGroup);
    properties->mString()->setValue(projLibraryStringProperty,
    "+proj=stere +a=6378273 +b=6356889.44891 +lat_0=90 +lat_ts=70 +lon_0=0");
    projLibraryStringProperty->setToolTip("https://proj.org/usage/projections.html");
    projLibraryStringProperty->setEnabled(false);

    // ToDo (MM, 05/2020): define stuff for southern hemisphere and possibly also other projection params

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
    settings->setValue("gridProjection",gridProjectionToString(gridProjection));
    settings->setValue("useRotation", enableGridRotation);
    settings->setValue("rotateBoundingBox", rotateBBox);
    settings->setValue("rotatedNorthPole", rotatedNorthPole);

    settings->setValue("enableProjLibraryProjection",enableProjLibraryProjection);
    settings->endGroup();
}


void MMapProjectionSupportingActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MMapProjectionSupportingActor::getSettingsID());

    properties->mEnum()->setValue(
                gridProjectionTypesProperty, stringToGridProjection(
                    (settings->value("gridProjection",
                                     gridProjectionToString(GRIDPROJECTION_CYLINDRICAL))
                     ).toString()));
    properties->mBool()->setValue(
                rotateBBoxProperty,
                settings->value("rotateBoundingBox", false).toBool());
    properties->mPointF()->setValue(
                rotatedNorthPoleProperty,
                settings->value("rotatedNorthPole",
                                QPointF(-180., 90.)).toPointF());
   properties->mString()->setValue(
                   projLibraryStringProperty,
                   settings->value("enableProjLibraryProjection","").toString());
    settings->endGroup();
}

MMapProjectionSupportingActor::gridProjectionTypes MMapProjectionSupportingActor::stringToGridProjection(
        QString gridProjectionName)
{
    if (gridProjectionName == "cylindrical")
    {
        return GRIDPROJECTION_CYLINDRICAL;
    }
    else if (gridProjectionName == "rotated lat.-lon.")
    {
        return GRIDPROJECTION_ROTATEDLATLON;
    }
    else if (gridProjectionName == "proj'-library supported projection")
    {
        return GRIDPROJECTION_PROJ_LIBRARY;
    }
    else
    {
        return GRIDPROJECTION_CYLINDRICAL;
    }
}


QString MMapProjectionSupportingActor::gridProjectionToString(
        gridProjectionTypes gridProjection)
{
    switch (gridProjection)
    {
        case GRIDPROJECTION_CYLINDRICAL: return "cylindrical";
        case GRIDPROJECTION_ROTATEDLATLON: return "rotated lat.-lon.";
        case GRIDPROJECTION_PROJ_LIBRARY: return "proj'-library supported projection";
    }
    return "cylindrical";
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/


} // namespace Met3D
