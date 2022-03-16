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
    MMultiVarTf(QVector<QtProperty*>& transferFunctionProperties, QVector<MTransferFunction1D*>& transferFunctions);
    ~MMultiVarTf() override;

    void createTexture1DArray();
    void destroyTexture1DArray();
    void generateTexture1DArray();
    void bindTexture1DArray(int textureUnitTransferFunction);
    void setVariableNames(const QVector<QString>& names);
    void setVariableRanges(const QVector<QVector2D>& variableRangesNew);

    GL::MShaderStorageBufferObject *getUseLogScaleBuffer(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget* currentGLContext = nullptr);
#else
            QGLWidget* currentGLContext = nullptr);
#endif
    GL::MShaderStorageBufferObject *getMinMaxBuffer(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget* currentGLContext = nullptr);
#else
            QGLWidget* currentGLContext = nullptr);
#endif
    void releaseBuffers();

private:
    QVector<QtProperty*> &transferFunctionProperties;
    /** Pointer to transfer function object and corresponding texture unit. */
    QVector<MTransferFunction1D*> &transferFunctions;
    GL::MTexture *textureTransferFunctionArray = nullptr;
    GL::MShaderStorageBufferObject *useLogScaleBuffer = nullptr;
    GL::MShaderStorageBufferObject *minMaxBuffer = nullptr;
    const QString useLogScaleBufferID =
            QString("multivardata_use_log_scale_buffer_#%1").arg(getID());
    const QString minMaxBufferID =
            QString("multivardata_minmax_buffer_#%1").arg(getID());
    QVector<QVector2D> minMaxList;
    bool useLogScaleIsDirty = true;
    bool minMaxIsDirty = true;
    QVector<QString> variableNames;
    QVector<QVector2D> variableRanges;
    QVector<uint32_t> useLogScaleArray;
    std::vector<std::vector<unsigned char>> standardSequentialColorMapsBytes;
    std::vector<std::vector<unsigned char>> standardDivergingColorMapsBytes;
};

} // namespace Met3D

#endif //MET_3D_MULTIVARTF_H
