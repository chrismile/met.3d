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
      enableGridRotation(false),
      rotateBBox(false),
      rotatedNorthPole(QPointF(-180., 90.))
{
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Rotated grid support enabled");

    rotatedGridPropertiesSubGroup = addProperty(
                GROUP_PROPERTY, "rotated grid support", nullptr);

    enableGridRotationProperty = addProperty(BOOL_PROPERTY, "enable rotation",
                                             rotatedGridPropertiesSubGroup);
    properties->mBool()->setValue(enableGridRotationProperty,
                                  enableGridRotation);

    rotateBBoxProperty = addProperty(BOOL_PROPERTY, "rotate bounding box",
                                     rotatedGridPropertiesSubGroup);
    properties->mBool()->setValue(rotateBBoxProperty, rotateBBox);

    rotatedNorthPoleProperty = addProperty(
                POINTF_LONLAT_PROPERTY, "rotated north pole",
                rotatedGridPropertiesSubGroup);
    properties->mPointF()->setValue(rotatedNorthPoleProperty, rotatedNorthPole);

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

    settings->setValue("useRotation", enableGridRotation);
    settings->setValue("rotateBoundingBox", rotateBBox);
    settings->setValue("rotatedNorthPole", rotatedNorthPole);

    settings->endGroup();
}


void MRotatedGridSupportingActor::loadConfiguration(QSettings *settings)
{
    settings->beginGroup(MRotatedGridSupportingActor::getSettingsID());

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

    settings->endGroup();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

} // namespace Met3D
