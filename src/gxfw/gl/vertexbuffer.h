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
#ifndef VERTEXBUFFER_H
#define VERTEXBUFFER_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include "QtCore"
#include "QOpenGLWidget"

// local application imports
#include "gxfw/gl/abstractgpudataitem.h"


namespace GL
{

/**
  @brief Abstract base class for OpenGL vertex buffers.
 */
class MVertexBuffer : public MAbstractGPUDataItem
{
public:
    explicit MVertexBuffer(Met3D::MDataRequest requestKey);

    virtual ~MVertexBuffer();

    GLuint getVertexBufferObject() { return vertexBufferObject; }

    void bindToArrayBuffer();

    /**
      attribute = index of the currently used vertex attribute slot (1/2/3/4)
      elemCount = number of vertex components
      stride = byte offset between consecutive generic vertex attributes
      offset = byte offset of the first vertex attribute in data
      */
    virtual void attachToVertexAttribute(GLuint attribute,
                                         GLint elemCount = -1,
                                         GLboolean normalized = false,
                                         GLsizei stride = 0,
                                         const GLvoid* offset = 0) = 0;

protected:
    // the created vertex buffer object for given vertices
    GLuint  vertexBufferObject;
};

} // namespace GL


#endif // VERTEXBUFFER_H
