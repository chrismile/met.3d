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

static std::vector<std::vector<QColor>> defaultTransferFunctionsSequential = {
        // reds
        { {228,218,218}, {228,192,192}, {228,161,161}, {228,118,119}, {228, 26, 28} },
        // blues
        { {176,179,184}, {157,168,184}, {134,156,184}, {104,142,184}, { 55,126,184} },
        // greens
        { {132,139,134}, {116,139,122}, { 96,139,108}, { 69,139, 91}, {  5,139, 69} },
        // purples
        { {129,123,129}, {129,108,127}, {129, 90,126}, {129, 65,125}, {129, 15,124} },
        // oranges
        { {217,208,207}, {217,186,182}, {217,159,152}, {217,125,110}, {217, 72,  1} },
        // pinks
        { {231,221,224}, {231,195,207}, {231,164,187}, {231,123,165}, {231, 41,138} },
        // golds
        { {254,248,243}, {254,233,217}, {254,217,185}, {254,199,144}, {254,178, 76} },
        // dark-blues
        { {243,243,255}, {214,214,255}, {179,179,255}, {130,131,255}, {  0,  7,255} },
};


static std::vector<std::vector<QColor>> defaultTransferFunctionsDiverging = {
        // Standard transfer function also used, e.g., in ParaView.
        { { 59, 76,192}, {144,178,254}, {220,220,220}, {245,156,125}, {180,  4, 38} },
};


MMultiVarTf::MMultiVarTf(
        QVector<QtProperty*>& transferFunctionProperties, QVector<MTransferFunction1D*>& transferFunctions)
        : transferFunctionProperties(transferFunctionProperties), transferFunctions(transferFunctions)
{
}


MMultiVarTf::~MMultiVarTf()
{
    destroyTexture1DArray();
    releaseBuffers();
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

    if (minMaxList.size() != transferFunctions.size()) {
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
            /*if (numBytesPerColorMap != 0 && numBytesPerColorMap != colorValuesTf.size())
            {
                LOG4CPLUS_ERROR(mlog, "Inconsistent number of transfer function steps!");
                return;
            }
            else if (numBytesPerColorMap == 0)
            {
                numBytesPerColorMap = colorValuesTf.size();
            }*/
            numBytesPerColorMap = std::max(numBytesPerColorMap, colorValuesTf.size());
        }
        else
        {
            hasNullptr = true;
        }
    }

    if (allNullptr)
    {
        numBytesPerColorMap = 256;
    }
    size_t numEntriesPerColorMap = numBytesPerColorMap / 4;

    /*std::vector<unsigned char> standardColorMapBytes;
    if (hasNullptr)
    {
        standardColorMapBytes.reserve(numBytesPerColorMap);
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
    }*/

    standardSequentialColorMapsBytes.clear();
    standardSequentialColorMapsBytes.resize(defaultTransferFunctionsSequential.size());
    for (size_t colMapIdx = 0; colMapIdx < defaultTransferFunctionsSequential.size(); colMapIdx++)
    {
        std::vector<unsigned char>& colorMapBytes = standardSequentialColorMapsBytes.at(colMapIdx);
        std::vector<QColor>& defaultTransferFunction = defaultTransferFunctionsSequential.at(colMapIdx);
        const int VALUES_PER_MAP = int(defaultTransferFunction.size());
        colorMapBytes.reserve(numBytesPerColorMap);
        for (size_t i = 0; i < numEntriesPerColorMap; i++)
        {
            float pct = float(i) / float(numEntriesPerColorMap - 1);
            float arrayPosFlt = pct * float(VALUES_PER_MAP - 1);
            int lastIdx = std::min(int(arrayPosFlt), VALUES_PER_MAP - 1);
            int nextIdx = std::min(lastIdx + 1, VALUES_PER_MAP - 1);
            float f1 = arrayPosFlt - float(lastIdx);
            float f0 = 1.0f - f1;
            QColor c0 = defaultTransferFunction.at(lastIdx);
            QColor c1 = defaultTransferFunction.at(nextIdx);
            uint8_t red = clamp(int(std::round(float(c0.red()) * f0 + float(c1.red()) * f1)), 0, 255);
            uint8_t green = clamp(int(std::round(float(c0.green()) * f0 + float(c1.green()) * f1)), 0, 255);
            uint8_t blue = clamp(int(std::round(float(c0.blue()) * f0 + float(c1.blue()) * f1)), 0, 255);
            colorMapBytes.push_back(red);
            colorMapBytes.push_back(green);
            colorMapBytes.push_back(blue);
            colorMapBytes.push_back(0xff);
        }
    }

    standardDivergingColorMapsBytes.clear();
    standardDivergingColorMapsBytes.resize(defaultTransferFunctionsDiverging.size());
    for (size_t colMapIdx = 0; colMapIdx < defaultTransferFunctionsDiverging.size(); colMapIdx++)
    {
        std::vector<unsigned char>& colorMapBytes = standardDivergingColorMapsBytes.at(colMapIdx);
        std::vector<QColor>& defaultTransferFunction = defaultTransferFunctionsDiverging.at(colMapIdx);
        const int VALUES_PER_MAP = int(defaultTransferFunction.size());
        colorMapBytes.reserve(numBytesPerColorMap);
        for (size_t i = 0; i < numEntriesPerColorMap; i++)
        {
            float pct = float(i) / float(numEntriesPerColorMap - 1);
            float arrayPosFlt = pct * float(VALUES_PER_MAP - 1);
            int lastIdx = std::min(int(arrayPosFlt), VALUES_PER_MAP - 1);
            int nextIdx = std::min(lastIdx + 1, VALUES_PER_MAP - 1);
            float f1 = arrayPosFlt - float(lastIdx);
            float f0 = 1.0f - f1;
            QColor c0 = defaultTransferFunction.at(lastIdx);
            QColor c1 = defaultTransferFunction.at(nextIdx);
            uint8_t red = clamp(int(std::round(float(c0.red()) * f0 + float(c1.red()) * f1)), 0, 255);
            uint8_t green = clamp(int(std::round(float(c0.green()) * f0 + float(c1.green()) * f1)), 0, 255);
            uint8_t blue = clamp(int(std::round(float(c0.blue()) * f0 + float(c1.blue()) * f1)), 0, 255);
            colorMapBytes.push_back(red);
            colorMapBytes.push_back(green);
            colorMapBytes.push_back(blue);
            colorMapBytes.push_back(0xff);
        }
    }

    std::vector<unsigned char> colorValuesArray;
    colorValuesArray.reserve(transferFunctions.size() * numBytesPerColorMap);
    useLogScaleArray.resize(transferFunctions.size());
    int varIdx = 0;
    foreach(MTransferFunction1D *tf, transferFunctions)
    {
        bool useLogScale = false;
        if (tf)
        {
            useLogScale = tf->getUseLogScale();
        }
        useLogScaleArray[varIdx] = useLogScale;

        if (tf != nullptr && !tf->getColorValuesByteArray().empty())
        {
            std::vector<unsigned char> colorValuesTf = tf->getColorValuesByteArray();
            float minimumValue = tf->getMinimumValue();
            float maximumValue = tf->getMaximumValue();
            if (tf->getIsRangeReverse())
            {
                float tmp = minimumValue;
                minimumValue = maximumValue;
                maximumValue = tmp;

                uint32_t* data = reinterpret_cast<uint32_t*>(colorValuesTf.data());
                std::reverse(data, data + colorValuesTf.size() / sizeof(uint32_t));
            }

            if (colorValuesTf.size() != numBytesPerColorMap)
            {
                // Resampling using nearest neighbor interpolation.
                std::vector<unsigned char> resampledColorValuesTf;
                resampledColorValuesTf.resize(numBytesPerColorMap);
                const size_t numEntriesOld = colorValuesTf.size() / 4;
                for (size_t i = 0; i < numEntriesPerColorMap; i++)
                {
                    for (size_t channel = 0; channel < 4; channel++)
                    {
                        size_t iOld = (i * numEntriesOld) / numEntriesPerColorMap * 4 + channel;
                        size_t iNew = i * 4 + channel;
                        resampledColorValuesTf.at(iNew) = colorValuesTf.at(iOld);
                    }
                }
                colorValuesArray.insert(
                        colorValuesArray.end(), resampledColorValuesTf.begin(), resampledColorValuesTf.end());
            }
            else
            {
                colorValuesArray.insert(
                        colorValuesArray.end(), colorValuesTf.begin(), colorValuesTf.end());
            }
            minMaxList[varIdx] = QVector2D(minimumValue, maximumValue);
        }
        else
        {
            const QString& varName = variableNames.at(varIdx);
            bool isSensitivity = (varName.startsWith('d') && varName != "deposition") || varName == "sensitivity_max";
            bool isDiverging = isSensitivity;

            if (isDiverging)
            {
                std::vector<unsigned char>& colorMapBytes = standardDivergingColorMapsBytes.at(
                        varIdx % defaultTransferFunctionsDiverging.size());
                colorValuesArray.insert(
                        colorValuesArray.end(), colorMapBytes.begin(), colorMapBytes.end());
            }
            else
            {
                std::vector<unsigned char>& colorMapBytes = standardSequentialColorMapsBytes.at(
                        varIdx % defaultTransferFunctionsSequential.size());
                colorValuesArray.insert(
                        colorValuesArray.end(), colorMapBytes.begin(), colorMapBytes.end());
            }
            QVector2D range;
            if (varIdx < variableRanges.size())
            {
                if (isSensitivity)
                {
                    float maxValue = std::max(
                            std::abs(variableRanges.at(varIdx).x()),
                            std::abs(variableRanges.at(varIdx).y()));
                    range = QVector2D(-maxValue, maxValue);
                }
                else
                {
                    range = variableRanges.at(varIdx);
                }
            }
            else
            {
                range = QVector2D(0.0f, 1.0f);
            }
            minMaxList[varIdx] = range;
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

        if (!glRM->tryStoreGPUItem(textureTransferFunctionArray))
        {
            delete textureTransferFunctionArray;
            textureTransferFunctionArray = nullptr;
        }
    }

    if (textureTransferFunctionArray)
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

#ifdef USE_QOPENGLWIDGET
        glActiveTexture(GL_TEXTURE0);
        glRM->doneCurrent();
#endif
    }

    this->useLogScaleIsDirty = true;
    this->minMaxIsDirty = true;
}


void MMultiVarTf::bindTexture1DArray(int textureUnitTransferFunction)
{
    textureTransferFunctionArray->bindToTextureUnit(textureUnitTransferFunction);
}


void MMultiVarTf::setVariableNames(const QVector<QString>& names)
{
    variableNames = names;
}


void MMultiVarTf::setVariableRanges(const QVector<QVector2D>& variableRangesNew)
{
    variableRanges = variableRangesNew;

    int varIdx = 0;
    foreach(MTransferFunction1D *tf, transferFunctions)
    {
        if (tf != nullptr && !tf->getColorValuesByteArray().empty())
        {
            float minimumValue = tf->getMinimumValue();
            float maximumValue = tf->getMaximumValue();
            if (tf->getIsRangeReverse())
            {
                float tmp = minimumValue;
                minimumValue = maximumValue;
                maximumValue = tmp;
            }

            minMaxList[varIdx] = QVector2D(minimumValue, maximumValue);
        }
        else
        {
            const QString& varName = variableNames.at(varIdx);
            bool isSensitivity = (varName.startsWith('d') && varName != "deposition") || varName == "sensitivity_max";

            QVector2D range;
            if (varIdx < variableRanges.size())
            {
                if (isSensitivity)
                {
                    float maxValue = std::max(
                            std::abs(variableRanges.at(varIdx).x()),
                            std::abs(variableRanges.at(varIdx).y()));
                    range = QVector2D(-maxValue, maxValue);
                }
                else
                {
                    range = variableRanges.at(varIdx);
                }
            }
            else
            {
                range = QVector2D(0.0f, 1.0f);
            }
            minMaxList[varIdx] = range;
        }
        varIdx++;
    }

    this->minMaxIsDirty = true;
}


GL::MShaderStorageBufferObject *MMultiVarTf::getUseLogScaleBuffer(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
{
    if (useLogScaleBuffer == nullptr)
    {
        useLogScaleBuffer = createShaderStorageBuffer(
                currentGLContext, useLogScaleBufferID, useLogScaleArray);
        useLogScaleIsDirty = false;
    }
    else if (useLogScaleIsDirty)
    {
        useLogScaleBuffer->upload(useLogScaleArray.constData(), GL_STATIC_DRAW);
        useLogScaleIsDirty = false;
    }
    return useLogScaleBuffer;
}


GL::MShaderStorageBufferObject *MMultiVarTf::getMinMaxBuffer(
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *currentGLContext)
#else
        QGLWidget *currentGLContext)
#endif
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


void MMultiVarTf::releaseBuffers()
{
    if (useLogScaleBuffer)
    {
        MGLResourcesManager::getInstance()->releaseGPUItem(useLogScaleBufferID);
    }
    if (minMaxBuffer)
    {
        MGLResourcesManager::getInstance()->releaseGPUItem(minMaxBufferID);
    }
}

} // namespace Met3D
