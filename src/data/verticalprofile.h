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
#ifndef VERTICALPROFILE_H
#define VERTICALPROFILE_H

// standard library imports

// related third party imports
#include <QVector>
#include <QVector2D>

// local application imports
#include "data/abstractdataitem.h"
#include "gxfw/gl/vertexbuffer.h"


namespace Met3D
{

/**
  @brief MVerticalProfile ...
 */
class MVerticalProfile : public MAbstractDataItem
{
public:
    MVerticalProfile();

    ~MVerticalProfile();

    unsigned int getMemorySize_kb();

    const QVector<QVector2D>& getScalarPressureData() { return profileData; }

    const QVector2D& getLonLatLocation() { return lonLatLocation; }

    /**
      Return a vertex buffer object that contains the profile data. The
      vertex buffer is created (and data uploaded) on the first call to this
      method.

      The @p currentGLContext argument is necessary as a GPU upload can switch
      the currently active OpenGL context. If this method is called
      from a render method, it should switch back to the current render context
      (given by @p currentGLContext).
     */
    GL::MVertexBuffer *getVertexBuffer(QOpenGLWidget *currentGLContext = 0);

    void releaseVertexBuffer();

    void updateData(QVector2D lonLatLocation, QVector<QVector2D> &profile);

protected:

private:
    QVector<QVector2D> profileData;
    QVector2D lonLatLocation;
};


} // namespace Met3D

#endif // VERTICALPROFILE_H
