/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2021 Christoph Neuhauser
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
#ifndef MET_3D_INDEXBUFFER_H
#define MET_3D_INDEXBUFFER_H

// standard library imports

// related third party imports
#include <GL/glew.h>
#include <QtCore>
#ifdef USE_QOPENGLWIDGET
#include <QOpenGLWidget>
#else
#include <QGLWidget>
#endif

// local application imports
#include "gxfw/gl/abstractgpudataitem.h"


namespace GL
{

/**
  @brief Abstract base class for OpenGL vertex buffers.
 */
class MIndexBuffer : public MAbstractGPUDataItem
{
public:
    explicit MIndexBuffer(Met3D::MDataRequest requestKey);

    virtual ~MIndexBuffer();

    GLuint getVertexBufferObject() { return indexBufferObject; }

    void bindToElementArrayBuffer();

    /**
     * 'count' parameter for, e.g., glDrawElements.
     * See: https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawElements.xhtml
     * @return The number of vertices.
     */
    inline GLsizei getCount()
    {
        return numIndices;
    }

    /**
     * 'type' parameter for, e.g., glDrawElements.
     * See: https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawElements.xhtml
     * @return The number of vertices.
     */
    inline GLenum getType()
    {
        return type;
    }

protected:
    // The created index buffer object for given vertices.
    GLuint indexBufferObject;
    // The number of index entries.
    GLsizei numIndices = 0;
    // GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT or GL_UNSIGNED_INT.
    GLenum type = 0;
};

} // namespace GL


#endif //MET_3D_INDEXBUFFER_H
