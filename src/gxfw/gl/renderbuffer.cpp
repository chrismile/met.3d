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
#include "renderbuffer.h"

// standard library imports
#include <iostream>

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

MRenderbuffer::MRenderbuffer( GLenum  internalFormat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei samples )
        : MAbstractGPUDataItem(""),
          internalFormat(internalFormat),
          width(width),
          height(height),
          samples(samples)
{
    // Generate renderbuffer object.
    glGenRenderbuffers(1, &rbo); CHECK_GL_ERROR;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);

    if (samples > 0) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internalFormat, width, height);
    } else {
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
    }
}


MRenderbuffer::MRenderbuffer( Met3D::MDataRequest requestKey,
                              GLenum  internalFormat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei samples )
        : MAbstractGPUDataItem(requestKey),
          internalFormat(internalFormat),
          width(width),
          height(height),
          samples(samples)
{
    // Generate renderbuffer object.
    glGenRenderbuffers(1, &rbo); CHECK_GL_ERROR;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);

    if (samples > 0) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internalFormat, width, height);
    } else {
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
    }
}


MRenderbuffer::~MRenderbuffer()
{
    glDeleteRenderbuffers(1, &rbo); CHECK_GL_ERROR;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

unsigned int MRenderbuffer::approxSizeInBytes()
{
    return approxSizeInBytes(internalFormat, width, height, samples);
}


unsigned int MRenderbuffer::getGPUMemorySize_kb()
{
    return approxSizeInBytes(internalFormat, width, height, samples) / 1024.;
}


unsigned int MRenderbuffer::formatSizeInBytes(GLenum internalFormat)
{
    switch (internalFormat)
    {

        case GL_R8:
        case GL_R8I:
        case GL_R8UI:
        case GL_R8_SNORM:
        case GL_R3_G3_B2:
            return 1;

        case GL_R16:
        case GL_R16F:
        case GL_R16I:
        case GL_R16UI:
        case GL_R16_SNORM:
        case GL_RG8:
        case GL_RG8I:
        case GL_RG8UI:
        case GL_RG8_SNORM:
        case GL_DEPTH_COMPONENT16:
            return 2;

        case GL_RGB8:
        case GL_RGB8I:
        case GL_RGB8UI:
        case GL_RGB8_SNORM:
            return 3;

        case GL_R32F:
        case GL_R32I:
        case GL_R32UI:
        case GL_RG16:
        case GL_RG16F:
        case GL_RG16I:
        case GL_RG16UI:
        case GL_RG16_SNORM:
        case GL_RGBA8:
        case GL_RGBA8I:
        case GL_RGBA8UI:
        case GL_ALPHA32F_ARB:
        case GL_DEPTH_COMPONENT32:
        case GL_DEPTH24_STENCIL8:
            return 4;

        case GL_DEPTH32F_STENCIL8:
            return 5;

        case GL_RGB16:
        case GL_RGB16F:
        case GL_RGB16I:
        case GL_RGB16UI:
        case GL_RGB16_SNORM:
            return 6;

        case GL_RG32F:
        case GL_RG32I:
        case GL_RG32UI:
        case GL_RGBA16:
        case GL_RGBA16F:
        case GL_RGBA16I:
        case GL_RGBA16UI:
        case GL_RGBA16_SNORM:
            return 8;

        case GL_RGB32F:
        case GL_RGB32I:
        case GL_RGB32UI:
            return 12;

        case GL_RGBA32F:
        case GL_RGBA32I:
        case GL_RGBA32UI:
            return 16;

        default:
            throw Met3D::MValueError(
                    "invalid internal format specified for renderbuffer",
                    __FILE__, __LINE__);

    }
}


unsigned int MRenderbuffer::approxSizeInBytes(GLenum  internalFormat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLsizei samples)
{
    if (samples < 0)
        samples = 1;

    return width * height * samples * formatSizeInBytes(internalFormat);
}


void MRenderbuffer::updateSize(GLsizei width, GLsizei height, GLsizei samples)
{
    this->width   = width;
    this->height  = height;
    this->samples = samples;

    Met3D::MGLResourcesManager *glRM = Met3D::MGLResourcesManager::getInstance();
    if (glRM->isManagedGPUItem(this)) glRM->updateGPUItemSize(this);
}

} // namespace GL
