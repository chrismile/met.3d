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
#include "framebuffer.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "renderbuffer.h"

using namespace std;

namespace GL
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MFramebuffer::MFramebuffer()
        : MAbstractGPUDataItem(""),
          width(0),
          height(0)
{
    Met3D::MGLResourcesManager *glRM = Met3D::MGLResourcesManager::getInstance();
    directStateAccessSupported = glRM->getIsOpenGLVersionAtLeast(4, 5);

    GLint oldFboId = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFboId);

    // Generate framebuffer object.
    glGenFramebuffers(1, &fbo); CHECK_GL_ERROR;
    hasColorAttachment = false;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo); CHECK_GL_ERROR;
    glBindFramebuffer(GL_FRAMEBUFFER, oldFboId); CHECK_GL_ERROR;
}


MFramebuffer::MFramebuffer( Met3D::MDataRequest requestKey )
        : MAbstractGPUDataItem(requestKey),
          width(0),
          height(0)
{
    Met3D::MGLResourcesManager *glRM = Met3D::MGLResourcesManager::getInstance();
    directStateAccessSupported = glRM->getIsOpenGLVersionAtLeast(4, 5);

    GLint oldFboId = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFboId);

    // Generate texture object.
    glGenFramebuffers(1, &fbo); CHECK_GL_ERROR;
    hasColorAttachment = false;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo); CHECK_GL_ERROR;
    glBindFramebuffer(GL_FRAMEBUFFER, oldFboId); CHECK_GL_ERROR;
}


MFramebuffer::~MFramebuffer()
{
    textures.clear();
    rbos.clear();
    glDeleteFramebuffers(1, &fbo);
    fbo = 0;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

bool MFramebuffer::bindTexture(MTexture* texture, FramebufferAttachment attachment)
{
    if (attachment >= COLOR_ATTACHMENT0 && attachment <= COLOR_ATTACHMENT15) {
        hasColorAttachment = true;
    }

    textures[attachment] = texture;

    GLint oldFbo = 0;

    GLuint oglTexture = texture->getTextureObject();
    width = texture->getWidth();
    height = texture->getHeight();
    if (directStateAccessSupported) {
        glNamedFramebufferTexture(fbo, attachment, oglTexture, 0); CHECK_GL_ERROR;
    } else {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture(GL_FRAMEBUFFER, attachment, oglTexture, 0); CHECK_GL_ERROR;
    }
    bool status = checkStatus();
    if (!directStateAccessSupported) {
        glBindFramebuffer(GL_FRAMEBUFFER, oldFbo); CHECK_GL_ERROR;
    }
    return status;
}


bool MFramebuffer::bindRenderbuffer(MRenderbuffer* renderbuffer, FramebufferAttachment attachment)
{
    GLint oldFbo = 0;

    rbos[attachment] = renderbuffer;
    if (directStateAccessSupported) {
        glNamedFramebufferRenderbuffer(fbo, attachment, GL_RENDERBUFFER, renderbuffer->getRenderbufferObject());
        CHECK_GL_ERROR;
    } else {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, renderbuffer->getRenderbufferObject());
        CHECK_GL_ERROR;
    }
    bool status = checkStatus();
    if (!directStateAccessSupported) {
        glBindFramebuffer(GL_FRAMEBUFFER, oldFbo); CHECK_GL_ERROR;
    }
    return status;
}


bool MFramebuffer::checkStatus()
{
    GLenum status;
    if (directStateAccessSupported) {
        status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER); CHECK_GL_ERROR;
    } else {
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER); CHECK_GL_ERROR;
    }
    switch(status) {
        case GL_FRAMEBUFFER_COMPLETE:
            break;
        default:
            LOG4CPLUS_ERROR(
                    mlog, std::string() + "Error: FramebufferObject::checkStatus(): "
                    + "Invalid FBO status " + std::to_string(status) + "!");
            return false;
    }
    return true;
}


unsigned int MFramebuffer::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); CHECK_GL_ERROR;

    if (!hasColorAttachment) {
        glDrawBuffer(GL_NONE); CHECK_GL_ERROR;
        glReadBuffer(GL_NONE); CHECK_GL_ERROR;
        hasColorAttachment = true; // Only call once
    }

    // More than one color attachment
    if (textures.size() > 1) {
        if (colorAttachments.empty()) {
            colorAttachments.reserve(textures.size());
            for (auto& texture : textures) {
                if (texture.first >= COLOR_ATTACHMENT0 && texture.first <= COLOR_ATTACHMENT15) {
                    colorAttachments.push_back(texture.first);
                }
            }
        }
        if (colorAttachments.size() > 1) {
            glDrawBuffers(GLsizei(colorAttachments.size()), &colorAttachments.front()); CHECK_GL_ERROR;
        }
    }

    return fbo;
}


unsigned int MFramebuffer::getGPUMemorySize_kb()
{
    return 0.0f;
}


} // namespace GL
