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
#include "multivartf.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "helpers.h"

namespace Met3D
{

MMultiVarTf::~MMultiVarTf()
{
    destroyTexture1DArray();
    releaseMinMaxBuffer();
}

void MMultiVarTf::createTexture1DArray()
{
    generateTexture1DArray();
}


void MMultiVarTf::destroyTexture1DArray()
{
    ;
}


void MMultiVarTf::generateTexture1DArray()
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    if (minMaxList.empty()) {
        minMaxList.resize(transferFunctions.size());
    }

    bool allNullptr = true;
    bool hasNullptr = false;
    size_t numBytesPerColorMap = 0;
    foreach(MTransferFunction1D *tf, transferFunctions)
    {
        if (tf != nullptr)
        {
            allNullptr = false;
            const std::vector<unsigned char> &colorValuesTf = tf->getColorValuesByteArray();
            if (numBytesPerColorMap != 0 && numBytesPerColorMap != colorValuesTf.size())
            {
                LOG4CPLUS_ERROR(mlog, "Inconsistent number of transfer function steps!");
                return;
            }
            else if (numBytesPerColorMap == 0)
            {
                numBytesPerColorMap = colorValuesTf.size();
            }
        } else {
            hasNullptr = true;
        }
    }

    if (allNullptr)
    {
        numBytesPerColorMap = 256;
    }
    size_t numEntriesPerColorMap = numBytesPerColorMap / 4;

    std::vector<unsigned char> standardColorMapBytes;
    if (hasNullptr)
    {
        for (size_t i = 0; i < numEntriesPerColorMap; i++)
        {
            float pct = float(i) / float(numEntriesPerColorMap - 1);
            uint32_t green = std::min(int(std::round(pct * 255)), 255);
            uint32_t blue = std::min(int(std::round((1.0f - pct) * 255)), 255);
            standardColorMapBytes.push_back(0);
            standardColorMapBytes.push_back(green);
            standardColorMapBytes.push_back(blue);
            standardColorMapBytes.push_back(0xff);
        }
    }

    std::vector<unsigned char> colorValuesArray;
    colorValuesArray.resize(transferFunctions.size() * numBytesPerColorMap);
    int varIdx = 0;
    foreach(MTransferFunction1D *tf, transferFunctions)
    {
        if (tf != nullptr)
        {
            const std::vector<unsigned char> &colorValuesTf = tf->getColorValuesByteArray();
            colorValuesArray.insert(
                    colorValuesArray.begin(), colorValuesTf.begin(), colorValuesTf.end());
            minMaxList[varIdx] = QVector2D(tf->getMinimumValue(), tf->getMaximumValue());
        }
        else
        {
            colorValuesArray.insert(
                    colorValuesArray.begin(), standardColorMapBytes.begin(), standardColorMapBytes.end());
            minMaxList[varIdx] = QVector2D(0.0f, 1.0f);
        }
        varIdx++;
    }

    if (textureTransferFunctionArray == nullptr)
    {
        // No texture exists. Create a new one and register with memory manager.
        QString textureID = QString("transferFunctionArray_#%1").arg(getID());
        textureTransferFunctionArray = new GL::MTexture(
                textureID, GL_TEXTURE_1D_ARRAY, GL_RGBA8,
                numEntriesPerColorMap, transferFunctions.size());

        if ( !glRM->tryStoreGPUItem(textureTransferFunctionArray) )
        {
            delete textureTransferFunctionArray;
            textureTransferFunctionArray = nullptr;
        }
    }

    if ( textureTransferFunctionArray )
    {
        textureTransferFunctionArray->updateSize(numBytesPerColorMap / 4, transferFunctions.size());

        glRM->makeCurrent();
        textureTransferFunctionArray->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering.
        // NOTE: GL_NEAREST is required here to avoid interpolation between
        // discrete colour levels -- the colour bar should reflect the actual
        // number of colour levels in the texture.
        glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // Upload data array to GPU.
        glTexImage2D(GL_TEXTURE_1D_ARRAY,       // target
                     0,                         // level of detail
                     GL_RGBA,                   // internal format
                     numBytesPerColorMap / 4,   // width
                     transferFunctions.size(),  // height
                     0,                         // border
                     GL_RGBA,                   // format
                     GL_UNSIGNED_BYTE,          // data type of the pixel data
                     colorValuesArray.data()); CHECK_GL_ERROR;
    }

    this->minMaxIsDirty = true;
}


void MMultiVarTf::bindTexture1DArray(int textureUnitTransferFunction)
{
    textureTransferFunctionArray->bindToTextureUnit(textureUnitTransferFunction);
}


GL::MShaderStorageBufferObject *MMultiVarTf::getMinMaxBuffer(QGLWidget *currentGLContext)
{
    if (minMaxBuffer == nullptr)
    {
        minMaxBuffer = createShaderStorageBuffer(currentGLContext, minMaxBufferID, minMaxList);
        minMaxIsDirty = false;
    }
    else if (minMaxIsDirty)
    {
        minMaxBuffer->upload(minMaxList.constData(), GL_STATIC_DRAW);
        minMaxIsDirty = false;
    }
    return minMaxBuffer;
}


void MMultiVarTf::releaseMinMaxBuffer()
{
    if (minMaxBuffer)
    {
        MGLResourcesManager::getInstance()->releaseGPUItem(minMaxBufferID);
    }
}

} // namespace Met3D
