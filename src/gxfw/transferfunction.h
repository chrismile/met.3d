/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Bianca Tost
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
#ifndef TRANSFERFUNCTION_H
#define TRANSFERFUNCTION_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <QtProperty>

// local application imports
#include "gxfw/mactor.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/texture.h"

namespace Met3D
{

/**
  @brief Base class for transfer function actors.
 */
class MTransferFunction : public MActor
{
    Q_OBJECT
public:
    MTransferFunction(QObject *parent = 0);
    ~MTransferFunction();

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

    QString getSettingsID() override { return "TransferFunction"; }

    void reloadShaderEffects();

    /**
      Returns the minimum scalar value of the current transfer function
      settings.
      */
    float getMinimumValue() { return minimumValue; }

    /**
      Returns the maximum scalar value of the current transfer function
      settings.
      */
    float getMaximumValue() { return maximumValue; }

    /**
      Returns the texture that represents the transfer function texture.
      */
    GL::MTexture* getTexture() { return tfTexture; }

    void setMinimumValue(float value);

    void setMaximumValue(float value);

    void setValueDecimals(int decimals);

    void setPosition(QRectF position);

    void setNumTicks(int num);

    void setNumLabels(int num);

    QString transferFunctionName();
    
signals:
    
public slots:

protected:
    void initializeActorResources();

    virtual void generateTransferTexture() {}

    virtual void generateBarGeometry() {}

    GL::MTexture *tfTexture;
    GLint textureUnit;

    GL::MVertexBuffer *vertexBuffer;
    uint numVertices;

    std::shared_ptr<GL::MShaderEffect> simpleGeometryShader;
    std::shared_ptr<GL::MShaderEffect> colourbarShader;

    // General properties.
    QtProperty *positionProperty;

    // Properties related to ticks and labels.
    QtProperty *maxNumTicksProperty;
    uint        numTicks;
    QtProperty *maxNumLabelsProperty;
    QtProperty *tickWidthProperty;
    QtProperty *labelSpacingProperty;

    // Properties related to value range.
    QtProperty *rangePropertiesSubGroup;
    QtProperty *valueDecimalsProperty;
    QtProperty *minimumValueProperty;
    QtProperty *maximumValueProperty;
    float       minimumValue;
    float       maximumValue;
    
};

} // namespace Met3D

#endif // TRANSFERFUNCTION_H
