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
#ifndef ABSTRACTGPUDATAITEM_H
#define ABSTRACTGPUDATAITEM_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include "QtCore"

// local application imports
#include "data/datarequest.h"


namespace GL
{

/**
  @brief Abstract base class for all OpenGL resources managed by the @ref
  MGLResourcesManager. This currently includes textures and vertex buffers
  (more can be added).
  */
class MAbstractGPUDataItem
{
public:
    /**
      Create a new GPU data item that is memory managed under the request
      key @p requestKey.
     */
    MAbstractGPUDataItem(Met3D::MDataRequest requestKey);

    virtual ~MAbstractGPUDataItem();

    /**
      Override this method in derived classes. Needs to provide the (at least
      approximate) GPU memory consumed by the item.
      */
    virtual unsigned int getGPUMemorySize_kb() = 0;

    inline Met3D::MDataRequest getRequestKey() const { return requestKey; }

private:
    Met3D::MDataRequest requestKey;

};

} // namespace GL


#endif // ABSTRACTGPUDATAITEM_H
