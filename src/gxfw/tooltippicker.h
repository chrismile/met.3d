/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2016-2017 Bianca Tost
**  Copyright 2021-2022 Christoph Neuhauser
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
#ifndef TOOLTIPPICKER_H
#define TOOLTIPPICKER_H

// standard library imports

// related third party imports
#include <QString>
#include <QPoint>

// local application imports

namespace Met3D
{

/**
  Interface for picking objects to display tooltip text for.
  */
class MToolTipPicker
{
public:
    /**
     * Picks a 3D point in the scene using screen coordinates (assuming origin at upper left corner of viewport)
     * and returns the depth and tool tip text for the intersection.
     * @param position The position on the screen (usually the mouse position).
     * @param depth The depth of the intersection (set if an intersection occurred).
     * @param text The tool tip text for the intersection (set if an intersection occurred).
     * @return True if a point in the scene was hit.
     */
    virtual bool toolTipPick(MSceneViewGLWidget* sceneView, const QPoint &position, float &depth, QString &text)=0;
};

}

#endif //TOOLTIPPICKER_H
