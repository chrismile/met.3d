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
#include "nwpsurfacetopographyactor.h"

// standard library imports
#include <iostream>
#include <math.h>

// related third party imports
#include <QObject>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/selectdatasourcedialog.h"
#include "data/structuredgrid.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNWPSurfaceTopographyActor::MNWPSurfaceTopographyActor()
    : MNWPMultiVarActor(),
      topographyVariableIndex(0),
      shadingVariableIndex(0)
{
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Surface topography");

    topographyVariableIndexProp = addProperty(ENUM_PROPERTY, "topography variable",
                                            actorPropertiesSupGroup);

    shadingVariableIndexProp = addProperty(ENUM_PROPERTY, "shading variable",
                                    actorPropertiesSupGroup);

    endInitialiseQtProperties();
}


MNWPSurfaceTopographyActor::~MNWPSurfaceTopographyActor()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MNWPSurfaceTopographyActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);
    shaderProgram->compileFromFile_Met3DHome("src/glsl/surface_topography.fx.glsl");
}


const QList<MVerticalLevelType> MNWPSurfaceTopographyActor::supportedLevelTypes()
{
    return (QList<MVerticalLevelType>() << SURFACE_2D);
}


MNWPActorVariable* MNWPSurfaceTopographyActor::createActorVariable(
        const MSelectableDataSource& dataSource)
{
    MNWPActorVariable* newVar = new MNWPActorVariable(this);

    newVar->dataSourceID = dataSource.dataSourceID;
    newVar->levelType = dataSource.levelType;
    newVar->variableName = dataSource.variableName;

    return newVar;
}


void MNWPSurfaceTopographyActor::saveConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::saveConfiguration(settings);

    settings->beginGroup(MNWPSurfaceTopographyActor::getSettingsID());

    settings->setValue("topographyVariableIndex", topographyVariableIndex);
    settings->setValue("shadingVariableIndex", shadingVariableIndex);

    settings->endGroup();
}


void MNWPSurfaceTopographyActor::loadConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::loadConfiguration(settings);

    settings->beginGroup(MNWPSurfaceTopographyActor::getSettingsID());

    int tmpTopographyVariableIndex = settings->value(
                "topographyVariableIndex").toInt();
    // Check index bounds in case any actor variable was not loaded correctly.
    tmpTopographyVariableIndex = max(0, tmpTopographyVariableIndex);
    tmpTopographyVariableIndex = min(variables.size()-1, tmpTopographyVariableIndex);

    int tmpShadingVariableIndex = settings->value(
                "shadingVariableIndex").toInt();
    tmpShadingVariableIndex = max(0, tmpShadingVariableIndex);
    tmpShadingVariableIndex = min(variables.size()-1, tmpShadingVariableIndex);

    properties->mEnum()->setValue(topographyVariableIndexProp,
                                  tmpTopographyVariableIndex);
    properties->mEnum()->setValue(shadingVariableIndexProp,
                                  tmpShadingVariableIndex);

    settings->endGroup();
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MNWPSurfaceTopographyActor::onQtPropertyChanged(QtProperty* property)
{
    // Parent signal processing.
    MNWPMultiVarActor::onQtPropertyChanged(property);

    if (property == topographyVariableIndexProp ||
        property == shadingVariableIndexProp)
    {
        topographyVariableIndex = properties->mEnum()->value(
                    topographyVariableIndexProp);
        shadingVariableIndex = properties->mEnum()->value(
                    shadingVariableIndexProp);

        emitActorChangedSignal();
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MNWPSurfaceTopographyActor::initializeActorResources()
{
    // Parent initialisation.
    MNWPMultiVarActor::initializeActorResources();

    // Build list with NWPActorVariable names for GUI enum properties.
    varNames.clear();
    for (int i = 0; i < variables.size(); ++i)
    {
        varNames << variables.at(i)->variableName;
    }

    properties->mEnum()->setEnumNames(topographyVariableIndexProp, varNames);
    properties->mEnum()->setEnumNames(shadingVariableIndexProp, varNames);
    properties->mEnum()->setValue(topographyVariableIndexProp,
                                  topographyVariableIndex);
    properties->mEnum()->setValue(shadingVariableIndexProp,
                                  shadingVariableIndex);

    // Load shader.
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    bool loadShaders = false;
    loadShaders |= glRM->generateEffectProgram("surfacetopography_shader",
                                               shaderProgram);

    if (loadShaders) reloadShaderEffects();
}


void MNWPSurfaceTopographyActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    if (variables.size() == 0) return;

    // Shortcuts to the variable's properties.
    MNWPActorVariable* varTopo = variables.at(topographyVariableIndex);
    MNWPActorVariable* var     = variables.at(shadingVariableIndex);

    if ( !var->hasData() || !varTopo->hasData() ) return;

    // Bind shader program that renders the volume slice.
    shaderProgram->bind();

    // Texture bindings for transfer function for data field (1D texture from
    // transfer function class).
    if (var->transferFunction)
    {
        var->transferFunction->getTexture()->bindToTextureUnit(
                    var->textureUnitTransferFunction);
        shaderProgram->setUniformValue(
                    "transferFunction", var->textureUnitTransferFunction);
        shaderProgram->setUniformValue(
                    "scalarMinimum", var->transferFunction->getMinimumValue());
        shaderProgram->setUniformValue(
                    "scalarMaximum", var->transferFunction->getMaximumValue());
    }
    else
    {
        // Render only if transfer function is defined.
        return;
    }

    // Model-view-projection matrix from the current scene view.
    shaderProgram->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

    // Texture bindings for Lat/Lon axes (1D textures).
    var->textureLonLatLevAxes->bindToTextureUnit(var->textureUnitLonLatLevAxes);
    shaderProgram->setUniformValue("latLonAxesData", var->textureUnitLonLatLevAxes);
    shaderProgram->setUniformValue("latOffset", GLint(var->grid->nlons));

    // Texture bindings for data field (2D texture).
    var->textureDataField->bindToTextureUnit(var->textureUnitDataField);
    shaderProgram->setUniformValue("dataField", var->textureUnitDataField);

    // Texture bindings for surface topography (2D texture).
    varTopo->textureDataField->bindToTextureUnit(varTopo->textureUnitDataField);
    shaderProgram->setUniformValue(
                "surfaceTopography", varTopo->textureUnitDataField);
    shaderProgram->setUniformValue(
                "pToWorldZParams", sceneView->pressureToWorldZParameters());

    // Lighting direction from scene view.
    shaderProgram->setUniformValue(
                "lightDirection", sceneView->getLightDirection());

    // Use instanced rendering to avoid geometry upload (see notes 09Feb2012).
    glPolygonMode(GL_FRONT_AND_BACK,
                  renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                          0,
                          var->grid->nlons * 2,
                          var->grid->nlats - 1); CHECK_GL_ERROR;
}


void MNWPSurfaceTopographyActor::dataFieldChangedEvent()
{
    emitActorChangedSignal();
}


void MNWPSurfaceTopographyActor::onDeleteActorVariable(
        MNWPActorVariable *var)
{
    // Update lists of variable names.

    // Get index of variable that is about to be removed.
    int i = variables.indexOf(var);

    // Update variableIndex and topographyVariableIndex if these point to
    // the removed variable or to one with a lower index.
    if (i <= shadingVariableIndex)
    {
        shadingVariableIndex = std::max(0, shadingVariableIndex - 1);
    }

    if (i <= topographyVariableIndex)
    {
        topographyVariableIndex = std::max(0, topographyVariableIndex - 1);
    }

    // Temporarily save variable indices.
    int tmpShadingVariableIndex = shadingVariableIndex;
    int tmpTopographyVariableIndex = topographyVariableIndex;

    // Remove the variable name from the enum lists.
    varNames.removeAt(i);

    // Update enum lists.
    properties->mEnum()->setEnumNames(topographyVariableIndexProp, varNames);
    properties->mEnum()->setEnumNames(shadingVariableIndexProp, varNames);

    properties->mEnum()->setValue(topographyVariableIndexProp,
                                  tmpTopographyVariableIndex);
    properties->mEnum()->setValue(shadingVariableIndexProp,
                                  tmpShadingVariableIndex);

}


void MNWPSurfaceTopographyActor::onAddActorVariable(
        MNWPActorVariable *var)
{
    // Update lists of variable names.
    varNames << var->variableName;

    // Temporarily save variable indices.
    int tmpShadingVariableIndex = shadingVariableIndex;
    int tmpTopographyVariableIndex = topographyVariableIndex;

    properties->mEnum()->setEnumNames(topographyVariableIndexProp, varNames);
    properties->mEnum()->setEnumNames(shadingVariableIndexProp, varNames);

    properties->mEnum()->setValue(topographyVariableIndexProp,
                                  tmpTopographyVariableIndex);
    properties->mEnum()->setValue(shadingVariableIndexProp,
                                  tmpShadingVariableIndex);
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


} // namespace Met3D
