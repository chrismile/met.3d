/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017-2018 Marc Rautenhaus
**  Copyright 2017-2018 Bianca Tost
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
#include "transferfunction.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/gl/typedvertexbuffer.h"
#include "util/mutil.h"
#include "actors/transferfunction1d.h"
#include "actors/spatial1dtransferfunction.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTransferFunction::MTransferFunction(QObject *parent) :
    MActor(parent),
    tfTexture(nullptr),
    vertexBuffer(nullptr),
    minimumValue(0.f),
    maximumValue(100.f)
{    
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    // Properties related to labelling the colour bar.
    // ===============================================

    maxNumTicksProperty = addProperty(INT_PROPERTY, "num. ticks",
                                      labelPropertiesSupGroup);
    properties->mInt()->setValue(maxNumTicksProperty, 11);
    properties->mInt()->setMinimum(maxNumTicksProperty, 0);

    maxNumLabelsProperty = addProperty(INT_PROPERTY, "num. labels",
                                       labelPropertiesSupGroup);
    properties->mInt()->setValue(maxNumLabelsProperty, 6);
    properties->mInt()->setMinimum(maxNumLabelsProperty, 0);

    tickWidthProperty = addProperty(DOUBLE_PROPERTY, "tick length",
                                    labelPropertiesSupGroup);
    properties->setDouble(tickWidthProperty, 0.015, 3, 0.001);

    labelSpacingProperty = addProperty(DOUBLE_PROPERTY, "space label-tick",
                                       labelPropertiesSupGroup);
    properties->setDouble(labelSpacingProperty, 0.01, 3, 0.001);

    // Properties related to data range.
    // =================================

    rangePropertiesSubGroup = addProperty(GROUP_PROPERTY, "range",
                                          actorPropertiesSupGroup);

    int significantDigits = 3;
    double step = 1.;

    minimumValueProperty = addProperty(SCIENTIFICDOUBLE_PROPERTY, "minimum value",
                                       rangePropertiesSubGroup);
    properties->setSciDouble(minimumValueProperty, minimumValue,
                           significantDigits, step);

    maximumValueProperty = addProperty(SCIENTIFICDOUBLE_PROPERTY, "maximum value",
                                       rangePropertiesSubGroup);
    properties->setSciDouble(maximumValueProperty, maximumValue,
                           significantDigits, step);

    valueOptionsPropertiesSubGroup = addProperty(
                GROUP_PROPERTY, "min/max options", rangePropertiesSubGroup);

    valueSignificantDigitsProperty = addProperty(
                INT_PROPERTY, "significant digits", valueOptionsPropertiesSubGroup);
    properties->setInt(valueSignificantDigitsProperty, significantDigits, 0, 9);

    valueStepProperty = addProperty(SCIENTIFICDOUBLE_PROPERTY, "step",
                                    valueOptionsPropertiesSubGroup);
    properties->setSciDouble(valueStepProperty, step, significantDigits, 0.1);

    // General properties.
    // ===================

    positionProperty = addProperty(RECTF_CLIP_PROPERTY, "position",
                                   actorPropertiesSupGroup);
    properties->setRectF(positionProperty, QRectF(0.9, 0.9, 0.05, 0.5), 2);

    endInitialiseQtProperties();
}


MTransferFunction::~MTransferFunction()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MTransferFunction::saveConfiguration(QSettings *settings)
{
    settings->beginGroup(MTransferFunction::getSettingsID());

    // Properties related to labelling the colour bar.
    // ===============================================
    settings->setValue("maxNumTicks",
                       properties->mInt()->value(maxNumTicksProperty));
    settings->setValue("maxNumLabels",
                       properties->mInt()->value(maxNumLabelsProperty));
    settings->setValue("tickLength",
                       properties->mDouble()->value(tickWidthProperty));
    settings->setValue("labelSpacing",
                       properties->mDouble()->value(labelSpacingProperty));

    // Properties related to data range.
    // =================================
    settings->setValue("minimumValue",
                       properties->mSciDouble()->value(minimumValueProperty));
    settings->setValue("maximumValue",
                       properties->mSciDouble()->value(maximumValueProperty));
    settings->setValue("valueSignificantDigits",
                       properties->mInt()->value(valueSignificantDigitsProperty));
    settings->setValue("valueStep",
                       properties->mInt()->value(valueSignificantDigitsProperty));

    // General properties.
    // ===================
    settings->setValue("position",
                       properties->mRectF()->value(positionProperty));

    settings->endGroup();
}


void MTransferFunction::loadConfiguration(QSettings *settings)
{
    // For compatibilty with versions < 1.2.
    QStringList versionString = readConfigVersionID(settings);
    if (versionString[0].toInt() <= 1  && versionString[1].toInt() < 2)
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(this))
        {
            settings->beginGroup(tf->getSettingsID());
        }
        else if(MSpatial1DTransferFunction *tf =
                dynamic_cast<MSpatial1DTransferFunction*>(this))
        {
            settings->beginGroup(tf->getSettingsID());
        }
    }
    else
    {
        settings->beginGroup(MTransferFunction::getSettingsID());
    }

    // Properties related to labelling the colour bar.
    // ===============================================
    properties->mInt()->setValue(maxNumTicksProperty,
                                 settings->value("maxNumTicks", 11).toInt());
    properties->mInt()->setValue(maxNumLabelsProperty,
                                 settings->value("maxNumLabels", 6).toInt());

    properties->mDouble()->setValue(
                tickWidthProperty,
                settings->value("tickLength", 0.015).toDouble());
    properties->mDouble()->setValue(
                labelSpacingProperty,
                settings->value("labelSpacing", 0.010).toDouble());

    // Properties related to data range.
    // =================================
    int significantDigits = 3;

    // Support of old configuration files.
    if (settings->contains("valueDecimals"))
    {
        significantDigits = settings->value("valueDecimals", 3).toInt();
    }
    else
    {
        significantDigits = settings->value("valueSignificantDigits", 3).toInt();
    }

    setValueSignificantDigits(significantDigits);
    setValueStep(settings->value("valueStep", 1.).toDouble());
    setMinimumValue(settings->value("minimumValue", 0.).toDouble());
    setMaximumValue(settings->value("maximumValue", 100.).toDouble());

    // General properties.
    // ===================
    setPosition(settings->value("position",
                                QRectF(0.9, 0.9, 0.05, 0.5)).toRectF());

    settings->endGroup();
}


void MTransferFunction::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);

    beginCompileShaders(2);

    compileShadersFromFileWithProgressDialog(
                simpleGeometryShader,
                "src/glsl/simple_coloured_geometry.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                colourbarShader,
                "src/glsl/colourbar.fx.glsl");

    endCompileShaders();
}


void MTransferFunction::setMinimumValue(float value)
{
    properties->mSciDouble()->setValue(minimumValueProperty, value);
}


void MTransferFunction::setMaximumValue(float value)
{
    properties->mSciDouble()->setValue(maximumValueProperty, value);
}


void MTransferFunction::setValueSignificantDigits(int significantDigits)
{
    properties->mInt()->setValue(valueSignificantDigitsProperty,
                                 significantDigits);
    properties->mSciDouble()->setSignificantDigits(minimumValueProperty,
                                                 significantDigits);
    properties->mSciDouble()->setSignificantDigits(maximumValueProperty,
                                                 significantDigits);
    properties->mSciDouble()->setSignificantDigits(valueStepProperty,
                                                 significantDigits);
}


void MTransferFunction::setValueStep(double step)
{
    properties->mSciDouble()->setValue(valueSignificantDigitsProperty, step);
    properties->mSciDouble()->setSingleStep(minimumValueProperty, step);
    properties->mSciDouble()->setSingleStep(maximumValueProperty, step);
}


void MTransferFunction::setPosition(QRectF position)
{
    properties->mRectF()->setValue(positionProperty, position);
}


void MTransferFunction::setNumTicks(int num)
{
    properties->mInt()->setValue(maxNumTicksProperty, num);
}


void MTransferFunction::setNumLabels(int num)
{
    properties->mInt()->setValue(maxNumLabelsProperty, num);
}


QString MTransferFunction::transferFunctionName()
{
    return getName();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTransferFunction::initializeActorResources()
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    textureUnit = assignTextureUnit();

    generateTransferTexture();

    // Load shader programs.
    bool loadShaders = false;

    loadShaders |= glRM->generateEffectProgram("transfer_colourbar",
                                               colourbarShader);
    loadShaders |= glRM->generateEffectProgram("transfer_geom",
                                               simpleGeometryShader);

    if (loadShaders) reloadShaderEffects();

    generateBarGeometry();
}

} // namespace Met3D
