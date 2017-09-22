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
#ifndef ROTATEDGRIDSUPPORTINGACTOR_H
#define ROTATEDGRIDSUPPORTINGACTOR_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <QtProperty>
#include <ogr_geometry.h>

// local application imports
#include "gxfw/mactor.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/texture.h"


class MGLResourcesManager;
class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief MRotatedGridSupportingActor is a abstract base class for actors
  supporting rotated grids.

  It adds properties to define rotated north pole coordinates and to choose
  between whether to use rotation or not and if rotation is used whether to
  rotate the bounding box or to treat the bounding box coordinates as regular
  coordinates.

  It is used by @ref MBaseMapActor and @ref MGraticuleActor .
  */
class MRotatedGridSupportingActor : public MActor
{
public:
    MRotatedGridSupportingActor();
    ~MRotatedGridSupportingActor();

    QString getSettingsID() override { return "RotatedGridEnablingActor"; }

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

protected:
    QtProperty *rotatedGridPropertiesSubGroup;
    bool enableGridRotation;
    QtProperty *enableGridRotationProperty;
    bool rotateBBox;
    QtProperty *rotateBBoxProperty;
    QPointF rotatedNorthPole;
    QtProperty *rotatedNorthPoleProperty;
};

} // namespace Met3D

#endif // ROTATEDGRIDSUPPORTINGACTOR_H