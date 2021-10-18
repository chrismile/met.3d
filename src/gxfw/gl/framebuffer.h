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
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include "QtCore"

// local application imports
#include "gxfw/gl/abstractgpudataitem.h"


namespace GL
{

class MTexture;
class MRenderbuffer;

enum FramebufferAttachment {
    DEPTH_ATTACHMENT = 0x8D00, STENCIL_ATTACHMENT = 0x8D20,
    DEPTH_STENCIL_ATTACHMENT = 0x821A, COLOR_ATTACHMENT = 0x8CE0,
    COLOR_ATTACHMENT0 = 0x8CE0, COLOR_ATTACHMENT1 = 0x8CE1,
    COLOR_ATTACHMENT2 = 0x8CE2, COLOR_ATTACHMENT3 = 0x8CE3,
    COLOR_ATTACHMENT4 = 0x8CE4, COLOR_ATTACHMENT5 = 0x8CE5,
    COLOR_ATTACHMENT6 = 0x8CE6, COLOR_ATTACHMENT7 = 0x8CE7,
    COLOR_ATTACHMENT8 = 0x8CE8, COLOR_ATTACHMENT9 = 0x8CE9,
    COLOR_ATTACHMENT10 = 0x8CEA, COLOR_ATTACHMENT11 = 0x8CEB,
    COLOR_ATTACHMENT12 = 0x8CEC, COLOR_ATTACHMENT13 = 0x8CED,
    COLOR_ATTACHMENT14 = 0x8CEE, COLOR_ATTACHMENT15 = 0x8CEF
};

/**
  @brief MFramebuffer encapsulates OpenGL framebuffer objects.
  */
class MFramebuffer : public MAbstractGPUDataItem
{
public:
    MFramebuffer();

    MFramebuffer(Met3D::MDataRequest requestKey);

    virtual ~MFramebuffer();

    bool bindTexture(MTexture* texture, FramebufferAttachment attachment = COLOR_ATTACHMENT);

    bool bindRenderbuffer(MRenderbuffer* renderbuffer, FramebufferAttachment attachment = DEPTH_ATTACHMENT);

    unsigned int bind();

    unsigned int getGPUMemorySize_kb() override;

    GLuint getFramebufferObject() { return fbo; }

    void    setIDKey(QString key) { idKey = key; }

    QString getIDKey()            { return idKey; }

    int getWidth() { return width; }

    int getHeight() { return height; }

private:

    bool checkStatus();

    bool directStateAccessSupported;

    GLuint  fbo;

    GLsizei width;
    GLsizei height;
    bool hasColorAttachment;
    std::map<FramebufferAttachment, MTexture*> textures;
    std::map<FramebufferAttachment, MRenderbuffer*> rbos;
    std::vector<GLuint> colorAttachments;

    QString idKey;

};

} // namespace GL

#endif // FRAMEBUFFER_H
