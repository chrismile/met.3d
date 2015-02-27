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
#ifndef MTYPES_H
#define MTYPES_H

// standard library imports

// related third party imports
#include <GL/glew.h>
#include <QtCore>
#include <QVector3D>
#include <QColor>

// local application imports

namespace Met3D
{

struct MLabel
{
    uint      id;
    GLuint    vbo;
    QVector3D anchor;
    QVector3D anchorOffset;     // offset the actual label position by this
                                // vector from the anchor
    int       coordinateSystem;
    QColor    textColour;
    uint      numCharacters;
    float     size;
    float     width;            // width of label
    bool      drawBBox;
    QColor    bboxColour;
};

} // namespace Met3D

#endif // MTYPES_H
