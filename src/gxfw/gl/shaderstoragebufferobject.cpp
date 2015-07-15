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
#include "shaderstoragebufferobject.h"

#include "texture.h"

// standard library imports
#include <iostream>

// related third party imports

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"

namespace GL
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MShaderStorageBufferObject::MShaderStorageBufferObject(const QString requestID,
                                                       const GLuint elementSize,
                                                       const GLuint numElements)
    : MAbstractGPUDataItem(requestID),
      elementByteSize(elementSize),
      numberElements(numElements)
{
    shaderStorageBufferObject = 0;
    glGenBuffers(1, &shaderStorageBufferObject);
}


MShaderStorageBufferObject::~MShaderStorageBufferObject()
{
    glDeleteBuffers(1, &shaderStorageBufferObject); CHECK_GL_ERROR;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MShaderStorageBufferObject::updateSize(const GLuint numElements)
{
    numberElements = numElements;

    Met3D::MGLResourcesManager *glRM = Met3D::MGLResourcesManager::getInstance();
    if (glRM->isManagedGPUItem(this)) glRM->updateGPUItemSize(this);
}


unsigned int MShaderStorageBufferObject::getGPUMemorySize_kb()
{
    return elementByteSize * numberElements / 1024.;
}


void MShaderStorageBufferObject::upload(const GLvoid* data, const GLenum usage)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, shaderStorageBufferObject);
    // DYNAMIC = data is modified often
    // COPY = read data from OpenGL and use as source for rendering
    const GLuint byteSize = elementByteSize * numberElements;
    glBufferData(GL_SHADER_STORAGE_BUFFER, byteSize, data, usage); CHECK_GL_ERROR;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}


void MShaderStorageBufferObject::bindToIndex(const GLuint index)
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, shaderStorageBufferObject);
}


} // namespace GL
