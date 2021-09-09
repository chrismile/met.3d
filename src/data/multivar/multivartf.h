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
#ifndef MET_3D_MULTIVARTF_H
#define MET_3D_MULTIVARTF_H

// standard library imports

// related third party imports
#include <QtCore>
#include <QtProperty>

// local application imports
#include "data/abstractdataitem.h"
#include "actors/transferfunction1d.h"
#include "gxfw/gl/shaderstoragebufferobject.h"


namespace Met3D
{

class MTrajectoryActor;

class MMultiVarTf : public MMemoryManagementUsingObject
{
public:
    MMultiVarTf(QVector<QtProperty*>& transferFunctionProperties, QVector<MTransferFunction1D*>& transferFunctions)
        : transferFunctionProperties(transferFunctionProperties), transferFunctions(transferFunctions) {}
    ~MMultiVarTf();

    void createTexture1DArray();
    void destroyTexture1DArray();
    void generateTexture1DArray();
    void bindTexture1DArray(int textureUnitTransferFunction);

    GL::MShaderStorageBufferObject *getMinMaxBuffer(QOpenGLWidget *currentGLContext = 0);
    void releaseMinMaxBuffer();

private:
    QVector<QtProperty*> &transferFunctionProperties;
    /** Pointer to transfer function object and corresponding texture unit. */
    QVector<MTransferFunction1D*> &transferFunctions;
    GL::MTexture *textureTransferFunctionArray = nullptr;

    GL::MShaderStorageBufferObject *minMaxBuffer = nullptr;
    const QString minMaxBufferID =
            QString("multivardata_minmax_buffer_#%1").arg(getID());
    QVector<QVector2D> minMaxList;
    bool minMaxIsDirty = true;
};

} // namespace Met3D

#endif //MET_3D_MULTIVARTF_H
