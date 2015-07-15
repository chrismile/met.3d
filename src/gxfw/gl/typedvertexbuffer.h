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
#ifndef GENERIC_VERTEXBUFFER_H
#define GENERIC_VERTEXBUFFER_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include "QtCore"
#include "QGLWidget"
#include <log4cplus/loggingmacros.h>

// local application imports
#include "vertexbuffer.h"
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"


namespace GL
{

/**
  @brief MGenericVertexBuffer encapsulates OpenGL vertex buffers.

  Data = class or type of the data that is passed to the vertex buffer
  Num = number of components that form one vertex element
  Type = data type of one component in the vertex element
 */
template <class Data, typename Type, int Num>
class MTypedVertexBuffer : public MVertexBuffer
{
public:
    explicit MTypedVertexBuffer(Met3D::MDataRequest requestKey,
                                uint32_t numVertices_);

    void upload(const QVector<Data>& data, QGLWidget* currentGLContext = 0);

    void upload(const Data* data, const uint32_t elemCount,
                QGLWidget* currentGLContext = 0);

    void reallocate(const Data* data, const uint32_t elemCount,
                    GLsizei size = 0, bool force = false,
                    QGLWidget* currentGLContext = 0);

    void reallocate(const QVector<Data>& data,
                    GLsizei size = 0, bool force = false,
                    QGLWidget* currentGLContext = 0);

    void update(const Data* data, const uint32_t elemCount,
                GLint offset = 0, GLsizei size = 0,
                QGLWidget* currentGLContext = 0);

    void update(const QVector<Data>& data,
                GLint offset = 0, GLsizei size = 0,
                QGLWidget* currentGLContext = 0);

    void attachToVertexAttribute(GLuint attribute,
                                 GLint elemCount = -1,
                                 GLboolean normalized = false,
                                 GLsizei stride = 0,
                                 const GLvoid* offset = 0) override;

    GLuint getGPUMemorySize_kb();

protected:
    // number of vertices contained in the vertex buffer
    GLint numVertices;

    GLenum getGLEnumFromDataType();
};

typedef MTypedVertexBuffer<GLfloat,   GLfloat, 1> MFloatVertexBuffer;
typedef MTypedVertexBuffer<GLfloat,   GLfloat, 2> MFloat2VertexBuffer;
typedef MTypedVertexBuffer<GLfloat,   GLfloat, 3> MFloat3VertexBuffer;
typedef MTypedVertexBuffer<QVector3D, GLfloat, 3> MVector3DVertexBuffer;
typedef MTypedVertexBuffer<QVector2D, GLfloat, 2> MVector2DVertexBuffer;


// Implementation needs to be in the header file as methods are templated.
// See http://stackoverflow.com/questions/495021/why-can-templates-only-be-implemented-in-the-header-file

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

template <class Data, typename Type, int Num>
MTypedVertexBuffer<Data,Type,Num>::MTypedVertexBuffer(
        Met3D::MDataRequest requestKey, uint32_t numVertices_)
    : MVertexBuffer(requestKey),
      numVertices(numVertices_)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

template <class Data, typename Type, int Num>
GLuint MTypedVertexBuffer<Data, Type, Num>::getGPUMemorySize_kb()
{
    return static_cast<GLuint>((sizeof(Type) * numVertices * Num) / 1024.0f);
}


template <class Data, typename Type, int Num>
void MTypedVertexBuffer<Data,Type,Num>::attachToVertexAttribute(
        GLuint attribute,
        GLint elemCount,
        GLboolean normalized,
        GLsizei stride,
        const GLvoid* offset)
{
    bindToArrayBuffer();
    glVertexAttribPointer(attribute,
                          elemCount <= 0 ? Num : elemCount, // size
                          getGLEnumFromDataType(),          // type
                          normalized ? GL_TRUE : GL_FALSE,  // normalized
                          stride,                           // stride
                          offset); CHECK_GL_ERROR;

    glEnableVertexAttribArray(attribute); CHECK_GL_ERROR;
}


template <class Data, typename Type, int Num>
void MTypedVertexBuffer<Data,Type,Num>::upload(
        const QVector<Data>& data,
        QGLWidget* currentGLContext)
{
    upload(data.constData(), data.size(), currentGLContext);
}


template <class Data, typename Type, int Num>
void MTypedVertexBuffer<Data,Type,Num>::upload(
        const Data* data,
        const uint32_t elemCount,
        QGLWidget* currentGLContext)
{
    // if uploaded size does not equal desired size and data is not a nullptr
    // then throw an error
    if (elemCount != uint32_t(numVertices) && !data)
    {
        throw Met3D::MValueError(
                    "length of data vector needs to match the number of"
                    " vertices for which this vertex buffer was "
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
    glDeleteBuffers(1, &vertexBufferObject); CHECK_GL_ERROR;

    // Generate new VBO and upload geometry data to GPU.
    glGenBuffers(1, &vertexBufferObject); CHECK_GL_ERROR;

    LOG4CPLUS_TRACE(mlog, "uploading vertex buffer geometry to vbo #"
                            << vertexBufferObject << flush);

    //std::cout << "DEBUG: Vertex Buffer Object -> " << int(vertexBufferObject) << std::flush << std::endl;

    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject); CHECK_GL_ERROR;
    //std::cout << "DEBUG: " << sizeof(Type) << " " << Num << " " << numVertices << std::endl << std::flush;
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(Type) * Num * numVertices,
                 data,
                 GL_STATIC_DRAW); CHECK_GL_ERROR;
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

    // If a valid GL context has been specifed, make this current.
    if (currentGLContext) currentGLContext->makeCurrent();
}


template <class Data, typename Type, int Num>
void MTypedVertexBuffer<Data,Type,Num>::reallocate(
        const Data* data,
        const uint32_t elemCount,
        GLsizei size, bool force,
        QGLWidget* currentGLContext)
{
    Met3D::MGLResourcesManager *glRM = Met3D::MGLResourcesManager::getInstance();
    glRM->makeCurrent();

    const GLsizei vboSize = sizeof(Type) * Num * numVertices;
    const GLsizei upBufSize = (size > 0) ? size : sizeof(Type) * Num * elemCount;

    if (vboSize != upBufSize || force)
    {
        //std::cout << "DEBUG: buffer updated" << std::endl;

        numVertices = (size > 0) ? upBufSize / (sizeof(Type) * Num) : elemCount;

        // Delete the old VBO. If this is the first time the method has been called
        // (i.e. no VBO exists), "vbo" is set to 0 (constructor!); glDeleteBuffers
        // ignores 0 buffers.
        glDeleteBuffers(1, &vertexBufferObject); CHECK_GL_ERROR;

        // Generate new VBO and upload geometry data to GPU.
        glGenBuffers(1, &vertexBufferObject); CHECK_GL_ERROR;
        LOG4CPLUS_TRACE(mlog, "reallocating vertex buffer and create vbo #"
                                << vertexBufferObject);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject); CHECK_GL_ERROR;
        glBufferData(GL_ARRAY_BUFFER,
                     upBufSize,
                     data,
                     GL_DYNAMIC_DRAW); CHECK_GL_ERROR;

        if (glRM->isManagedGPUItem(this)) glRM->updateGPUItemSize(this);
    }

    // If a valid GL context has been specifed, make this current.
    if (currentGLContext) currentGLContext->makeCurrent();
}


template <class Data, typename Type, int Num>
void MTypedVertexBuffer<Data,Type,Num>::reallocate(
        const QVector<Data>& data,
        GLsizei size,
        bool force,
        QGLWidget* currentGLContext)
{
    reallocate(data, data.size(), 0, size, force, currentGLContext);
}


template <class Data, typename Type, int Num>
void MTypedVertexBuffer<Data,Type,Num>::update(
        const Data* data,
        const uint32_t elemCount,
        GLint offset, GLsizei size,
        QGLWidget* currentGLContext)
{
    const GLsizei vboSize = sizeof(Type) * Num * numVertices;
    const GLsizei upBufSize = (size > 0) ? size : sizeof(Type) * Num * elemCount;

    if (upBufSize + offset > vboSize)
    {
        throw Met3D::MValueError(
                    "size of sub data needs to be less or equal the number of"
                    " vertices for which this vertex buffer was "
                    "initialized", __FILE__, __LINE__);
    }

    // Make sure that the GLResourcesManager context is the currently active
    // context, otherwise glDrawArrays on the VBO generated here will fail in
    // any other context than the currently active. The GLResourcesManager
    // context is shared with all visible contexts.
    Met3D::MGLResourcesManager *glRM = Met3D::MGLResourcesManager::getInstance();
    glRM->makeCurrent();

    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject); CHECK_GL_ERROR;
    LOG4CPLUS_TRACE(mlog, "updating vertex buffer object #"
                            << vertexBufferObject);
    glBufferSubData(GL_ARRAY_BUFFER, // buffer target
                    offset,  // offset in buffer
                    upBufSize, // size of buffer region
                    data); // new buffer data
    CHECK_GL_ERROR;

    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

    // If a valid GL context has been specifed, make this current.
    if (currentGLContext) currentGLContext->makeCurrent();
}


template <class Data, typename Type, int Num>
void MTypedVertexBuffer<Data,Type,Num>::update(
        const QVector<Data>& data,
        GLint offset, GLsizei size,
        QGLWidget* currentGLContext)
{
    update(data.constData(), data.size(), offset, size, currentGLContext);
}


/******************************************************************************
***                         PROTECTED METHODS                               ***
*******************************************************************************/

template <class Data, typename Type, int Num>
GLenum MTypedVertexBuffer<Data,Type,Num>::getGLEnumFromDataType()
{
    if (typeid(Type) == typeid(float)) { return GL_FLOAT; }
    if (typeid(Type) == typeid(int)) { return GL_INT; }
    if (typeid(Type) == typeid(char)) { return GL_BYTE; }

    std::cerr << "No enum information for this type" << std::endl << std::flush;
    return GL_NONE;
}


} // namespace GL

#endif
