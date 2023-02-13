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
#ifndef RENDERBUFFER_H
#define RENDERBUFFER_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include "QtCore"

// local application imports
#include "gxfw/gl/abstractgpudataitem.h"


namespace GL
{

/**
  @brief MRenderbuffer encapsulates OpenGL renderbuffer objects.
  */
class MRenderbuffer : public MAbstractGPUDataItem
{
public:
    MRenderbuffer(GLenum internalFormat, GLsizei width, GLsizei height, GLsizei samples=0);

    MRenderbuffer(Met3D::MDataRequest requestKey, GLenum internalFormat,
             GLsizei width, GLsizei height, GLsizei samples=0);

    virtual ~MRenderbuffer();

    /**
      Returns the approximate size of the renderbuffer in bytes, computed by
      width * height * depth * (bytes per value of internal format).
      */
    unsigned int approxSizeInBytes();

    unsigned int getGPUMemorySize_kb() override;

    /**
      Returns the size of an renderbuffer element, given an internal format. See
      https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glRenderbufferStorage.xhtml.
      */
    static unsigned int formatSizeInBytes(GLenum internalFormat);

    /**
      Returns the approximate size of the renderbuffer in bytes, computed by
      width * height * depth * (bytes per value of internal format).
      */
    static unsigned int approxSizeInBytes(GLenum  internalFormat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei samples=0);

    /**
      Update the size parameters of this renderbuffer. If the renderbuffer
      is memory managed, this method automatically tells the GLResourcesManager
      of the changed size.
     */
    void updateSize(GLsizei width, GLsizei height, GLsizei samples=0);

    GLuint getRenderbufferObject() { return rbo; }

    void    setIDKey(QString key) { idKey = key; }

    QString getIDKey()            { return idKey; }

    int getWidth() { return width; }

    int getHeight() { return height; }

    int getSamples() { return samples; }

private:

    GLuint  rbo;

    GLenum  internalFormat;
    GLsizei width;
    GLsizei height;
    GLsizei samples;

    QString idKey;

};

} // namespace GL

#endif // RENDERBUFFER_H
