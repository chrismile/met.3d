/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Michael Kern
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

#ifndef SHADERSTORAGEBUFFEROBJECT_H
#define SHADERSTORAGEBUFFEROBJECT_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include "QtCore"

// local application imports
#include "gxfw/gl/abstractgpudataitem.h"

namespace GL
{

/**
  @brief MShaderStorageBufferObject encapsulates OpenGL shader storage buffer
  objects.
  */
class MShaderStorageBufferObject : public MAbstractGPUDataItem
{
public:
    MShaderStorageBufferObject(const QString requestID,
                               const GLuint elementSize,
                               const GLuint numElements);

    virtual ~MShaderStorageBufferObject();

    unsigned int getGPUMemorySize_kb() override;

    void updateSize(const GLuint numElements);

    void upload(const GLvoid* data, const GLenum usage);

    void bindToIndex(const GLuint index);

    GLuint getBufferObject() { return shaderStorageBufferObject; }

private:
    GLuint shaderStorageBufferObject;
    GLuint elementByteSize;
    GLuint numberElements;
};

}

#endif // SHADERSTORAGEBUFFEROBJECT_H
