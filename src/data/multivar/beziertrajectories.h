/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020-2021 Christoph Neuhauser
**  Copyright 2020      Michael Kern
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
#ifndef MET_3D_BEZIERTRAJECTORIES_H
#define MET_3D_BEZIERTRAJECTORIES_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include <QVector2D>
#include <QGLWidget>

// local application imports
#include "../trajectories.h"
#include "gxfw/gl/indexbuffer.h"
#include "gxfw/gl/shaderstoragebufferobject.h"

namespace Met3D {

/**
 * Data for one single trajectory for @see MBezierTrajectories.
 */
class MBezierTrajectory {
public:
    unsigned int getMemorySize_kb() const;

    // Describes the position of variables in the buffer and the total number of variable values for the entire line
    struct LineDesc
    {
        float startIndex; // pointer to index in array
        float numValues; // number of variables along line after Bezier curve transformation
    };

    // Describes the range of values for each variable and the offset within each line
    struct VarDesc
    {
        float startIndex;
        QVector2D minMax;
        float dummy;
    };

    // Per Bezier point data.
    QVector<QVector3D> positions;
    QVector<QVector<float>> attributes;

    // Packed array of base trajectory attributes.
    QVector<float> multiVarData;

    // Information about this line/trajectory.
    LineDesc lineDesc;
    // Information about all variables.
    QVector<VarDesc> multiVarDescs;
};

struct MBezierTrajectoriesRenderData
{
    // IBO
    GL::MIndexBuffer* indexBuffer = nullptr;
    // VBOs
    GL::MVertexBuffer* vertexPositionBuffer = nullptr;
    GL::MVertexBuffer* vertexNormalBuffer = nullptr;
    GL::MVertexBuffer* vertexTangentBuffer = nullptr;
    GL::MVertexBuffer* vertexMultiVariableBuffer = nullptr;
    GL::MVertexBuffer* vertexVariableDescBuffer = nullptr;
    // SSBOs
    GL::MShaderStorageBufferObject* variableArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* lineDescArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* varDescArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* lineVarDescArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* varSelectedArrayBuffer = nullptr;
};

/**
 * Flow line data with multiple variables being displayed at once.
 * The lines are smoothed using Bezier curves.
 */
class MBezierTrajectories : public MSupplementalTrajectoryData
{
public:
    MBezierTrajectories(MDataRequest requestToReferTo, unsigned int numTrajectories, unsigned int numVariables);
    ~MBezierTrajectories();

    unsigned int getMemorySize_kb();

    inline int size() const { return bezierTrajectories.size(); }
    inline MBezierTrajectory& operator[](std::size_t idx) { return bezierTrajectories[idx]; }
    inline const MBezierTrajectory& operator[](std::size_t idx) const { return bezierTrajectories[idx]; }

    MBezierTrajectoriesRenderData getRenderData(QGLWidget *currentGLContext = 0);
    void releaseRenderData();
    void updateSelectedVariables(const QVector<uint32_t>& varSelected);

private:
    QVector<MBezierTrajectory> bezierTrajectories;
    MBezierTrajectoriesRenderData bezierTrajectoriesRenderData;
    QVector<uint32_t> varSelected;

    const QString indexBufferID =
            QString("beziertrajectories_index_buffer_#%1").arg(getID());
    const QString vertexPositionBufferID =
            QString("beziertrajectories_vertex_position_buffer_#%1").arg(getID());
    const QString vertexNormalBufferID =
            QString("beziertrajectories_vertex_normal_buffer_#%1").arg(getID());
    const QString vertexTangentBufferID =
            QString("beziertrajectories_vertex_tangent_buffer_#%1").arg(getID());
    const QString vertexMultiVariableBufferID =
            QString("beziertrajectories_vertex_multi_variable_buffer_#%1").arg(getID());
    const QString vertexVariableDescBufferID =
            QString("beziertrajectories_vertex_variable_desc_buffer_#%1").arg(getID());
    const QString variableArrayBufferID =
            QString("beziertrajectories_variable_array_buffer_#%1").arg(getID());
    const QString lineDescArrayBufferID =
            QString("beziertrajectories_line_desc_array_buffer_#%1").arg(getID());
    const QString varDescArrayBufferID =
            QString("beziertrajectories_var_desc_array_buffer_#%1").arg(getID());
    const QString lineVarDescArrayBufferID =
            QString("beziertrajectories_line_var_desc_array_buffer_#%1").arg(getID());
    const QString varSelectedArrayBufferID =
            QString("beziertrajectories_var_selected_array_buffer_#%1").arg(getID());
};

}

#endif //MET_3D_BEZIERTRAJECTORIES_H
