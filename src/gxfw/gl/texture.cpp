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
#include "texture.h"

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

MTexture::MTexture( GLenum  target,
                    GLint   internalFormat,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth )
    : MAbstractGPUDataItem(""),
      target(target),
      internalFormat(internalFormat),
      width(width),
      height(height),
      depth(depth)
{
    // Generate texture object.
    glGenTextures(1, &textureObject); CHECK_GL_ERROR;

    // Get max. number of texture units.
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &lastTextureUnit);
    lastTextureUnit--;
}


MTexture::MTexture( Met3D::MDataRequest requestKey,
                    GLenum  target,
                    GLint   internalFormat,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth )
    : MAbstractGPUDataItem(requestKey),
      target(target),
      internalFormat(internalFormat),
      width(width),
      height(height),
      depth(depth)
{
    // Generate texture object.
    glGenTextures(1, &textureObject); CHECK_GL_ERROR;

    // Get max. number of texture units.
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &lastTextureUnit);
    lastTextureUnit--;
}


//Texture::Texture( GLenum  target,
//                  GLint   level,
//                  GLint   internalFormat,
//                  GLenum  format,
//                  GLenum  type,
//                  GLsizei width,
//                  GLsizei height,
//                  GLsizei depth )
//    : target(target),
//      level(level),
//      internalFormat(internalFormat),
//      format(format),
//      type(type),
//      width(width),
//      height(height),
//      depth(depth)
//{
//    // Generate texture object.
//    glGenTextures(1, &textureObject); CHECK_GL_ERROR;

//    bindToLastTextureUnit();

//    // Set wrap mode and filtering here..

//    // Set pixel store parameters here..

//    // Allocate video memory.
//    switch (target) {
//    case GL_TEXTURE_1D:
//        glTexImage1D(target, level, internalFormat,
//                     width, 0, format, type, NULL);
//        break;
//    case GL_TEXTURE_2D:
//        glTexImage2D(target, level, internalFormat,
//                     width, height, 0, format, type, NULL);
//        break;
//    case GL_TEXTURE_3D:
//        glTexImage3D(target, level, internalFormat,
//                     width, height, depth, 0, format, type, NULL);
//        break;
//    }
//}


MTexture::~MTexture()
{
    glDeleteTextures(1, &textureObject); CHECK_GL_ERROR;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

unsigned int MTexture::approxSizeInBytes()
{
    return approxSizeInBytes(internalFormat, width, height, depth);
}


unsigned int MTexture::getGPUMemorySize_kb()
{
    return approxSizeInBytes(internalFormat, width, height, depth) / 1024.;
}


void MTexture::bindToTextureUnit(GLuint unit)
{
    glActiveTexture(GL_TEXTURE0 + unit); CHECK_GL_ERROR;
    glBindTexture(target, textureObject); CHECK_GL_ERROR;
}


void MTexture::bindToLastTextureUnit()
{
    bindToTextureUnit(lastTextureUnit);
}


//Texture::subImage2D(GLint level,
//                    GLint xoffset,
//                    GLint yoffset,
//                    GLsizei width,
//                    GLsizei height,
//                    const GLvoid *data)
//{
//    // bind to last unit?
//    glTexSubImage2D(target, level, xoffset, yoffset, width, height,
//                    format, type, data);
//    // unbind?
//}


unsigned int MTexture::formatSizeInBytes(GLint internalFormat)
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
    case GL_RGBA8I:
    case GL_RGBA8UI:
    case GL_ALPHA32F_ARB:
    case GL_DEPTH_COMPONENT32:
        return 4;

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
                    "invalid internal format specified for texture",
                    __FILE__, __LINE__);

    }
}


unsigned int MTexture::approxSizeInBytes(GLint   internalFormat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth)
{
    // 1D textures.
    if (height < 0)
        return width * formatSizeInBytes(internalFormat);

    // 2D textures.
    if (depth < 0)
        return width * height * formatSizeInBytes(internalFormat);

    // 3D textures.
    return width * height * depth * formatSizeInBytes(internalFormat);
}


void MTexture::updateSize(GLsizei width, GLsizei height, GLsizei depth)
{
    this->width  = width;
    this->height = height;
    this->depth  = depth;

    Met3D::MGLResourcesManager *glRM = Met3D::MGLResourcesManager::getInstance();
    if (glRM->isManagedGPUItem(this)) glRM->updateGPUItemSize(this);
}

} // namespace GL
