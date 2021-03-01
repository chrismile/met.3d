/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2021 Christoph Neuhauser
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
#ifndef MET_3D_TYPEDINDEXBUFFER_H
#define MET_3D_TYPEDINDEXBUFFER_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include "QtCore"
#include "QGLWidget"
#include <log4cplus/loggingmacros.h>

// local application imports
#include "indexbuffer.h"
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"


namespace GL
{

/**
  @brief MTypedIndexBuffer encapsulates OpenGL index buffers.

  Type = data type of one index element (either unsigned byte, unsigned short, or unsigned int).
 */
template <typename Type>
class MTypedIndexBuffer : public MIndexBuffer
{
public:
    explicit MTypedIndexBuffer(Met3D::MDataRequest requestKey,
                               uint32_t numIndices_);

    void upload(const QVector<Type>& data, QGLWidget* currentGLContext = 0);

    void upload(const Type* data, const uint32_t elemCount,
                QGLWidget* currentGLContext = 0);

    void reallocate(const Type* data, const uint32_t elemCount,
                    GLsizei size = 0, bool force = false,
                    QGLWidget* currentGLContext = 0);

    void reallocate(const QVector<Type>& data,
                    GLsizei size = 0, bool force = false,
                    QGLWidget* currentGLContext = 0);

    void update(const Type* data, const uint32_t elemCount,
                GLint offset = 0, GLsizei size = 0,
                QGLWidget* currentGLContext = 0);

    void update(const QVector<Type>& data,
                GLint offset = 0, GLsizei size = 0,
                QGLWidget* currentGLContext = 0);

    GLuint getGPUMemorySize_kb();
};

typedef MTypedIndexBuffer<GLubyte> MUbyteIndexBuffer;
typedef MTypedIndexBuffer<GLushort> MUshortIndexBuffer;
typedef MTypedIndexBuffer<GLuint> MUintIndexBuffer;


// Implementation needs to be in the header file as methods are templated.
// See http://stackoverflow.com/questions/495021/why-can-templates-only-be-implemented-in-the-header-file

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

template <class Type>
MTypedIndexBuffer<Type>::MTypedIndexBuffer(
        Met3D::MDataRequest requestKey, uint32_t numIndices_)
        : MIndexBuffer(requestKey)
{
    numIndices = numIndices_;
    if (sizeof(Type) == 1) {
        type = GL_UNSIGNED_BYTE;
    } else if (sizeof(Type) == 2) {
        type = GL_UNSIGNED_SHORT;
    } else if (sizeof(Type) == 4) {
        type = GL_UNSIGNED_INT;
    } else {
        LOG4CPLUS_ERROR(mlog, "Error in MTypedIndexBuffer<Type>::MTypedIndexBuffer: Invalid type size.");
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

template <class Type>
GLuint MTypedIndexBuffer<Type>::getGPUMemorySize_kb()
{
    return static_cast<GLuint>((sizeof(Type) * numIndices) / 1024.0f);
}


template <class Type>
void MTypedIndexBuffer<Type>::upload(
        const QVector<Type>& data,
        QGLWidget* currentGLContext)
{
    upload(data.constData(), data.size(), currentGLContext);
}


template <class Type>
void MTypedIndexBuffer<Type>::upload(
        const Type* data,
        const uint32_t elemCount,
        QGLWidget* currentGLContext)
{
    // if uploaded size does not equal desired size and data is not a nullptr
    // then throw an error
    if (elemCount != uint32_t(numIndices) && !data)
    {
        throw Met3D::MValueError(
                "length of data vector needs to match the number of"
                " indices for which this index buffer was "
                "initialized", __FILE__, __LINE__);
    }

    // Make sure that the GLResourcesManager context is the currently active
    // context, otherwise glDrawArrays on the VBO generated here will fail in
    // any other context than the currently active. The GLResourcesManager
    // context is shared with all visible contexts.
    Met3D::MGLResourcesManager *glRM = Met3D::MGLResourcesManager::getInstance();
    glRM->makeCurrent();

    // Delete the old VBO. If this is the first time the method has been called
    // (i.e. no VBO exists), "vbo" is set to 0 (constructor!); glDeleteBuffers
    // ignores 0 buffers.
    glDeleteBuffers(1, &indexBufferObject); CHECK_GL_ERROR;

    // Generate new VBO and upload geometry data to GPU.
    glGenBuffers(1, &indexBufferObject); CHECK_GL_ERROR;

    LOG4CPLUS_TRACE(mlog, "uploading index buffer geometry to vbo #"
            << indexBufferObject << flush);

    //std::cout << "DEBUG: Index Buffer Object -> " << int(indexBufferObject) << std::flush << std::endl;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject); CHECK_GL_ERROR;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(Type) * numIndices,
                 data,
                 GL_STATIC_DRAW); CHECK_GL_ERROR;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

    // If a valid GL context has been specifed, make this current.
    if (currentGLContext) currentGLContext->makeCurrent();
}


template <class Type>
void MTypedIndexBuffer<Type>::reallocate(
        const Type* data,
        const uint32_t elemCount,
        GLsizei size, bool force,
        QGLWidget* currentGLContext)
{
    Met3D::MGLResourcesManager *glRM = Met3D::MGLResourcesManager::getInstance();
    glRM->makeCurrent();

    const GLsizei vboSize = sizeof(Type) * numIndices;
    const GLsizei upBufSize = (size > 0) ? size : sizeof(Type) * elemCount;

    if (vboSize != upBufSize || force)
    {
        //std::cout << "DEBUG: buffer updated" << std::endl;

        numIndices = (size > 0) ? upBufSize / (sizeof(Type)) : elemCount;

        // Delete the old VBO. If this is the first time the method has been called
        // (i.e. no VBO exists), "vbo" is set to 0 (constructor!); glDeleteBuffers
        // ignores 0 buffers.
        glDeleteBuffers(1, &indexBufferObject); CHECK_GL_ERROR;

        // Generate new VBO and upload geometry data to GPU.
        glGenBuffers(1, &indexBufferObject); CHECK_GL_ERROR;
        LOG4CPLUS_TRACE(mlog, "reallocating index buffer and create vbo #"
                << indexBufferObject);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject); CHECK_GL_ERROR;
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     upBufSize,
                     data,
                     GL_DYNAMIC_DRAW); CHECK_GL_ERROR;

        if (glRM->isManagedGPUItem(this)) glRM->updateGPUItemSize(this);
    }

    // If a valid GL context has been specifed, make this current.
    if (currentGLContext) currentGLContext->makeCurrent();
}


template <class Type>
void MTypedIndexBuffer<Type>::reallocate(
        const QVector<Type>& data,
        GLsizei size,
        bool force,
        QGLWidget* currentGLContext)
{
    reallocate(data, data.size(), 0, size, force, currentGLContext);
}


template <class Type>
void MTypedIndexBuffer<Type>::update(
        const Type* data,
        const uint32_t elemCount,
        GLint offset, GLsizei size,
        QGLWidget* currentGLContext)
{
    const GLsizei vboSize = sizeof(Type) * numIndices;
    const GLsizei upBufSize = (size > 0) ? size : sizeof(Type) * elemCount;

    if (upBufSize + offset > vboSize)
    {
        throw Met3D::MValueError(
                "size of sub data needs to be less or equal the number of"
                " indices for which this index buffer was "
                "initialized", __FILE__, __LINE__);
    }

    // Make sure that the GLResourcesManager context is the currently active
    // context, otherwise glDrawArrays on the VBO generated here will fail in
    // any other context than the currently active. The GLResourcesManager
    // context is shared with all visible contexts.
    Met3D::MGLResourcesManager *glRM = Met3D::MGLResourcesManager::getInstance();
    glRM->makeCurrent();

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject); CHECK_GL_ERROR;
    LOG4CPLUS_TRACE(mlog, "updating index buffer object #"
            << indexBufferObject);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, // buffer target
                    offset,  // offset in buffer
                    upBufSize, // size of buffer region
                    data); // new buffer data
    CHECK_GL_ERROR;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

    // If a valid GL context has been specifed, make this current.
    if (currentGLContext) currentGLContext->makeCurrent();
}


template <class Type>
void MTypedIndexBuffer<Type>::update(
        const QVector<Type>& data,
        GLint offset, GLsizei size,
        QGLWidget* currentGLContext)
{
    update(data.constData(), data.size(), offset, size, currentGLContext);
}


/******************************************************************************
***                         PROTECTED METHODS                               ***
*******************************************************************************/


} // namespace GL


#endif //MET_3D_TYPEDINDEXBUFFER_H
