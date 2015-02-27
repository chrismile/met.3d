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
#ifndef TEXTURE_H
#define TEXTURE_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include "QtCore"

// local application imports
#include "gxfw/gl/abstractgpudataitem.h"


namespace GL
{

/**
  @brief MTexture encapsulates OpenGL textures.
  */
class MTexture : public MAbstractGPUDataItem
{
public:
    MTexture(GLenum target, GLint internalFormat,
             GLsizei width, GLsizei height=-1, GLsizei depth=-1);

    MTexture(Met3D::MDataRequest requestKey, GLenum target, GLint internalFormat,
             GLsizei width, GLsizei height=-1, GLsizei depth=-1);

    virtual ~MTexture();

    /**
      Returns the approximate size of the texture in bytes, computed by
      width * height * depth * (bytes per value of internal format).
      */
    unsigned int approxSizeInBytes();

    unsigned int getGPUMemorySize_kb();

    void bindToTextureUnit(GLuint unit);

    void bindToLastTextureUnit();

    /**
      Returns the size of an texture element, given an internal format. See
      http://www.opengl.org/sdk/docs/man4/xhtml/glTexImage2D.xml.
      */
    static unsigned int formatSizeInBytes(GLint internalFormat);

    /**
      Returns the approximate size of the texture in bytes, computed by
      width * height * depth * (bytes per value of internal format).
      */
    static unsigned int approxSizeInBytes(GLint   internalFormat,
                                          GLsizei width,
                                          GLsizei height=-1,
                                          GLsizei depth=-1);

    /**
      Update this texture's size parameters. If the texture is memory managed,
      this method automatically tells the GLResourcesManager of the changed
      size.
     */
    void updateSize(GLsizei width, GLsizei height=-1, GLsizei depth=-1);

    GLuint getTextureObject() { return textureObject; }

    void    setIDKey(QString key) { idKey = key; }

    QString getIDKey()            { return idKey; }

    int getWidth() { return width; }

    int getHeight() { return height; }

    int getDepth() { return depth; }

private:

    GLuint  textureObject;
    GLint   lastTextureUnit;

    GLenum  target;
    GLint   level;
    GLint   internalFormat;
    GLenum  format;
    GLenum  type;
    GLsizei width;
    GLsizei height;
    GLsizei depth;

    QString idKey;

};

} // namespace GL

#endif // TEXTURE_H
