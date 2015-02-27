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
#include "vertexbuffer.h"

// standard library imports

// related third party imports

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"

using namespace std;

namespace GL
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MVertexBuffer::MVertexBuffer(Met3D::MDataRequest requestKey)
    : MAbstractGPUDataItem(requestKey),
      vertexBufferObject(0)
{
}


MVertexBuffer::~MVertexBuffer()
{
    // Delete the vertex buffer object. If the VBO has never been created,
    // "vertexBufferObject" takes the value 0 (constructor!)); glDeleteBuffers
    // ignores 0 buffers.
    glDeleteBuffers(1, &vertexBufferObject); CHECK_GL_ERROR;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MVertexBuffer::bindToArrayBuffer()
{
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject); CHECK_GL_ERROR;
}


} // namespace GL
