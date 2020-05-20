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
#include "rotatedgridsupportingactor.h"

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

MRotatedGridSupportingActor::MRotatedGridSupportingActor()
    : MActor(),
      gridProjection(GRIDPROJECTION_DISABLED),
      enableGridRotation(false),
      rotateBBox(false),
      rotatedNorthPole(QPointF(-180., 90.)),
      enableStereographicGrid(false),
      stereoBBox(false),
      stereoStraightLon(0),
      stereoStandardLat(70)
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
    gridProjectionNames << gridProjectionToString(GRIDPROJECTION_DISABLED)
                        << gridProjectionToString(GRIDPROJECTION_ROTATEDLATLON)
                        << gridProjectionToString(GRIDPROJECTION_STEREOGRAPHIC);
    gridProjectionTypesProperty = addProperty(
                ENUM_PROPERTY, "type of projection", gridProjectionPropertiesSubGroup);
    properties->mEnum()->setEnumNames(gridProjectionTypesProperty, gridProjectionNames);
    properties->mEnum()->setValue(gridProjectionTypesProperty, gridProjection);

    // inputs for grid projection: rotated lat lon
    enableGridRotationProperty = addProperty(BOOL_PROPERTY, "enable rotation",
                                             gridProjectionPropertiesSubGroup);
    properties->mBool()->setValue(enableGridRotationProperty,
                                  enableGridRotation);
    enableGridRotationProperty->setEnabled(false);

    rotateBBoxProperty = addProperty(BOOL_PROPERTY, "rotate bounding box",
                                     gridProjectionPropertiesSubGroup);
    properties->mBool()->setValue(rotateBBoxProperty, rotateBBox);
    rotateBBoxProperty->setEnabled(false);

    rotatedNorthPoleProperty = addProperty(
                POINTF_LONLAT_PROPERTY, "rotated north pole",
                gridProjectionPropertiesSubGroup);
    properties->mPointF()->setValue(rotatedNorthPoleProperty, rotatedNorthPole);
    rotatedNorthPoleProperty->setEnabled(false);

    // inputs for grid projection: polar stereographic
    enableStereographicGridProperty = addProperty(BOOL_PROPERTY, "polar stereographic graticule",
                                                 gridProjectionPropertiesSubGroup);
    properties->mBool()->setValue(enableStereographicGridProperty,
                                    enableStereographicGrid);
    enableStereographicGridProperty->setEnabled(false);

    stereoBBoxProperty = addProperty(BOOL_PROPERTY, "polar stereographic bounding box",
                                     gridProjectionPropertiesSubGroup);
    properties->mBool()->setValue(stereoBBoxProperty, stereoBBox);
    stereoBBoxProperty->setEnabled(false);

    stereoProjLonProperty = addProperty(
                DOUBLE_PROPERTY, "stereo. proj.: vertical longitude from pole",
                gridProjectionPropertiesSubGroup);
    properties->setDouble(stereoProjLonProperty,stereoStraightLon,-180,180,1,1);
    stereoProjLonProperty->setEnabled(false);

    stereoProjLatProperty = addProperty(
                DOUBLE_PROPERTY, "stereo. proj.: standard parallel latitude",
                gridProjectionPropertiesSubGroup);
    properties->setDouble(stereoProjLatProperty,stereoStandardLat,-90,90,1,1);
    stereoProjLatProperty->setEnabled(false);

    // ToDo (MM, 05/2020): define stuff for southern hemisphere and possibly also other projection params

    endInitialiseQtProperties();
}


MRotatedGridSupportingActor::~MRotatedGridSupportingActor()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MRotatedGridSupportingActor::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MRotatedGridSupportingActor::getSettingsID());
    settings->setValue("gridProjection",gridProjectionToString(gridProjection));
    settings->setValue("useRotation", enableGridRotation);
    settings->setValue("rotateBoundingBox", rotateBBox);
    settings->setValue("rotatedNorthPole", rotatedNorthPole);
    settings->setValue("useStereographic", enableStereographicGrid);
    settings->setValue("stereoBoundingBox", stereoBBox);
    settings->setValue("stereoStraightLon", stereoStraightLon);
    settings->setValue("stereoStandardLat", stereoStandardLat);
    settings->endGroup();
}


void MRotatedGridSupportingActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MRotatedGridSupportingActor::getSettingsID());

    properties->mEnum()->setValue(
                gridProjectionTypesProperty, stringToGridProjection(
                    (settings->value("gridProjection",
                                     gridProjectionToString(GRIDPROJECTION_DISABLED))
                     ).toString()));
    properties->mBool()->setValue(
                enableGridRotationProperty,
                settings->value("useRotation", false).toBool());
    properties->mBool()->setValue(
                rotateBBoxProperty,
                settings->value("rotateBoundingBox", false).toBool());
    properties->mPointF()->setValue(
                rotatedNorthPoleProperty,
                settings->value("rotatedNorthPole",
                                QPointF(-180., 90.)).toPointF());
   properties->mBool()->setValue(
               enableStereographicGridProperty,
               settings->value("useStereographic", false).toBool());
   properties->mBool()->setValue(
               stereoBBoxProperty,
               settings->value("stereoBoundingBox", false).toBool());
   properties->mDouble()->setValue(
               stereoProjLonProperty,
               settings->value("stereoStraightLon",
                               0.).toDouble());
   properties->mDouble()->setValue(
               stereoProjLatProperty,
               settings->value("stereoStandardLat",
                               0.).toDouble());
    settings->endGroup();
}

MRotatedGridSupportingActor::gridProjectionTypes MRotatedGridSupportingActor::stringToGridProjection(
        QString gridProjectionName)
{
    if (gridProjectionName == "disabled")
    {
        return GRIDPROJECTION_DISABLED;
    }
    else if (gridProjectionName == "rotated lat.-lon.")
    {
        return GRIDPROJECTION_ROTATEDLATLON;
    }
    else if (gridProjectionName == "polar stereographic")
    {
        return GRIDPROJECTION_STEREOGRAPHIC;
    }
    else
    {
        return GRIDPROJECTION_DISABLED;
    }
}


QString MRotatedGridSupportingActor::gridProjectionToString(
        gridProjectionTypes gridProjection)
{
    switch (gridProjection)
    {
        case GRIDPROJECTION_DISABLED: return "disabled";
        case GRIDPROJECTION_ROTATEDLATLON: return "rotated lat.-lon.";
        case GRIDPROJECTION_STEREOGRAPHIC: return "polar stereographic";
    }
    return "disabled";
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/


} // namespace Met3D
