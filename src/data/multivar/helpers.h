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
#ifndef MET_3D_HELPERS_H
#define MET_3D_HELPERS_H

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
        QGLWidget *currentGLContext, const QString& vbID, const QVector<T>& data)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a buffer with this item's data already exists in GPU memory.
    GL::MVertexBuffer *vb = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(vbID));
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
        QGLWidget *currentGLContext, const QString& ibID, const QVector<uint32_t>& data)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a buffer with this item's data already exists in GPU memory.
    GL::MIndexBuffer *ib = static_cast<GL::MIndexBuffer*>(glRM->getGPUItem(ibID));
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
        QGLWidget *currentGLContext, const QString& vbID, const QVector<T>& data)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a buffer with this item's data already exists in GPU memory.
    GL::MShaderStorageBufferObject *vb = static_cast<GL::MShaderStorageBufferObject*>(glRM->getGPUItem(vbID));
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

}

#endif //MET_3D_HELPERS_H
