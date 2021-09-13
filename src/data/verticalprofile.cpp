/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2019 Marc Rautenhaus
**
**  Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
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
#include "verticalprofile.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/gl/typedvertexbuffer.h"


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MVerticalProfile::MVerticalProfile()
    : MAbstractDataItem()
{
}


MVerticalProfile::~MVerticalProfile()
{
    // Make sure the corresponding data is removed from GPU memory as well.
    MGLResourcesManager::getInstance()->releaseAllGPUItemReferences(getID());
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

unsigned int MVerticalProfile::getMemorySize_kb()
{
    return (profileData.size() * sizeof(QVector2D));
}


GL::MVertexBuffer *MVerticalProfile::getVertexBuffer(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // Check if a vertex buffer already exists in GPU memory.
    GL::MVertexBuffer *vb = static_cast<GL::MVertexBuffer*>(
                glRM->getGPUItem(getID()));
    if (vb) return vb;

    // No vertex buffer exists. Create a new one.
    GL::MVector2DVertexBuffer *newVB = new GL::MVector2DVertexBuffer(
                getID(), profileData.size());

    if (glRM->tryStoreGPUItem(newVB))
    {
        newVB->upload(profileData, currentGLContext);
    }
    else
    {
        delete newVB;
    }

    return static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(getID()));
}


void MVerticalProfile::releaseVertexBuffer()
{
    MGLResourcesManager::getInstance()->releaseGPUItem(getID());
}


void MVerticalProfile::updateData(
        QVector2D lonLatLocation, QVector<QVector2D> &profile)
{
    // Update CPU-side memory.
    this->lonLatLocation = lonLatLocation;
    this->profileData = profile;

    // If a vertex buffer exists, update that as well.
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    GL::MVertexBuffer* vb = static_cast<GL::MVertexBuffer*>(
                glRM->getGPUItem(getID()));

    if (vb)
    {
        GL::MVector2DVertexBuffer* vb2D =
                dynamic_cast<GL::MVector2DVertexBuffer*>(vb);
        // Reallocate buffer if size has changed.
        vb2D->reallocate(nullptr, profile.size(), 0, false);
        vb2D->update(profile, 0, 0);
    }
}


} // namespace Met3D
