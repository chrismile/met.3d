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
#ifndef HELPERS_H
#define HELPERS_H

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/gl/typedvertexbuffer.h"
#include "gxfw/gl/typedindexbuffer.h"
#include "gxfw/gl/shaderstoragebufferobject.h"

namespace Met3D
{

template<class T>
GL::MVertexBuffer* createVertexBuffer(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext,
#else
        QGLWidget *currentGLContext,
#endif
        const QString& vbID, const QVector<T>& data)
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    // Check if a buffer with this item's data already exists in GPU memory.
    GL::MVertexBuffer* vb = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(vbID));
    if (vb) return vb;

    // No buffer with this item's data exists. Create a new one.
    GL::MTypedVertexBuffer<T, GLfloat, sizeof(T) / 4> *newVB = new GL::MTypedVertexBuffer<T, GLfloat, sizeof(T) / 4>(
            vbID, data.size());

    if (glRM->tryStoreGPUItem(newVB))
        newVB->upload(data, currentGLContext);
    else
        delete newVB;

    return static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(vbID));
}


inline GL::MIndexBuffer* createIndexBuffer(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext,
#else
        QGLWidget *currentGLContext,
#endif
        const QString& ibID, const QVector<uint32_t>& data)
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    // Check if a buffer with this item's data already exists in GPU memory.
    GL::MIndexBuffer* ib = static_cast<GL::MIndexBuffer*>(glRM->getGPUItem(ibID));
    if (ib) return ib;

    // No buffer with this item's data exists. Create a new one.
    GL::MTypedIndexBuffer<GLuint> *newVB = new GL::MTypedIndexBuffer<GLuint>(ibID, data.size());

    if (glRM->tryStoreGPUItem(newVB))
        newVB->upload(data, currentGLContext);
    else
        delete newVB;

    return static_cast<GL::MIndexBuffer*>(glRM->getGPUItem(ibID));
}


template<class T>
GL::MShaderStorageBufferObject* createShaderStorageBuffer(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext,
#else
        QGLWidget *currentGLContext,
#endif
        const QString& vbID, const QVector<T>& data)
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    // Check if a buffer with this item's data already exists in GPU memory.
    GL::MShaderStorageBufferObject* vb = static_cast<GL::MShaderStorageBufferObject*>(glRM->getGPUItem(vbID));
    if (vb) return vb;

    // No buffer with this item's data exists. Create a new one.
    GL::MShaderStorageBufferObject *newVB = new GL::MShaderStorageBufferObject(
            vbID, sizeof(T), data.size());

    if (glRM->tryStoreGPUItem(newVB))
        newVB->upload(data.constData(), GL_STATIC_DRAW);
    else
        delete newVB;

    return static_cast<GL::MShaderStorageBufferObject*>(glRM->getGPUItem(vbID));
}

inline QMatrix4x4 matrixOrthogonalProjection(float left, float right, float bottom, float top, float near, float far)
{
    return QMatrix4x4(
            2.0f / (right - left), 0.0f, 0.0f, - (right + left) / (right - left),
            0.0f, 2.0f / (top - bottom), 0.0f, - (top + bottom) / (top - bottom),
            0.0f, 0.0f, -2.0f / (far - near), - (far + near) / (far - near),
            0.0f, 0.0f, 0.0f, 1.0f);
    //return QMatrix4x4(
    //        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
    //        0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
    //        0.0f, 0.0f, -2.0f / (far - near), -(far + near) / (far - near),
    //        -(right + left) / (right - left), -(top + bottom) / (top - bottom), 0.0f, 1.0f);
}

inline float mix(float x, float y, float a)
{
    return x * (1.0f - a) + y * a;
}

inline QVector3D mix(const QVector3D &x, const QVector3D &y, float a)
{
    return (1.0f - a) * x + a * y;
}

inline float fract(float x)
{
    return x - std::floor(x);
}

inline int sign(float x)
{
    return x > 0.0f ? 1 : (x < 0.0f ? -1 : 0);
}

inline int sign(int x)
{
    return x > 0 ? 1 : (x < 0 ? -1 : 0);
}

inline float remap(float x, float srcStart, float srcStop, float dstStart, float dstStop)
{
    float t = (x - srcStart) / (srcStop - srcStart);
    return dstStart + t * (dstStop - dstStart);
}

}

#endif // HELPERS_H
