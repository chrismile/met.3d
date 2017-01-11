/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2016 Marc Rautenhaus
**  Copyright 2016 Bianca Tost
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
#include "spatial1dtransferfunction.h"

// standard library imports
#include <iostream>
#include <float.h>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/gl/typedvertexbuffer.h"
#include "util/mutil.h"
#include "gxfw/msceneviewglwidget.h"

using namespace std;


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MSpatial1DTransferFunction::MSpatial1DTransferFunction()
    : MTransferFunction(),
      tfTexture(nullptr),
      numLevels(2),
      useMirroredRepeat(false),
      minimumValue(0.0),
      maximumValue(100.0),
      clampMaximum(true),
      interpolationRange(1.0),
      alphaBlendingMode(AlphaBlendingMode::AlphaChannel),
      invertAlpha(false),
      useConstantColour(false),
      constantColour(QColor(0, 0, 0, 255)),
      textureScale(1.0),
      currentTextureWidth(0),
      currentTextureHeight(0)
{
    loadedImages.clear();
    pathsToLoadedImages.clear();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setName("Transfer function scalar to texture");

    // Properties related to labelling the colour bar.
    // ===============================================

    maxNumTicksProperty = addProperty(INT_PROPERTY, "num. ticks",
                                      labelPropertiesSupGroup);
    properties->mInt()->setValue(maxNumTicksProperty, 11);

    maxNumLabelsProperty = addProperty(INT_PROPERTY, "num. labels",
                                       labelPropertiesSupGroup);
    properties->mInt()->setValue(maxNumLabelsProperty, 6);

    tickWidthProperty = addProperty(DOUBLE_PROPERTY, "tick length",
                                    labelPropertiesSupGroup);
    properties->setDouble(tickWidthProperty, 0.015, 3, 0.001);

    labelSpacingProperty = addProperty(DOUBLE_PROPERTY, "space label-tick",
                                       labelPropertiesSupGroup);
    properties->setDouble(labelSpacingProperty, 0.01, 3, 0.001);

    // Properties related to texture levels.
    // =====================================

    levelsPropertiesSubGroup = addProperty(GROUP_PROPERTY, "texture levels",
                                          actorPropertiesSupGroup);

    loadLevelsImagesProperty = addProperty(CLICK_PROPERTY, "load levels",
                                           levelsPropertiesSubGroup);

    pathToLoadedImagesProperty = addProperty(STRING_PROPERTY, "level paths",
                                   levelsPropertiesSubGroup);
    pathsToLoadedImages.resize(numLevels);
    properties->mString()->setValue(pathToLoadedImagesProperty, pathsToLoadedImages.at(0));
    pathToLoadedImagesProperty->setEnabled(false);

    useMirroredRepeatProperty = addProperty(BOOL_PROPERTY, "use mirrored repeat",
                                   levelsPropertiesSubGroup);
    properties->mBool()->setValue(useMirroredRepeatProperty, useMirroredRepeat);

    // Properties related to data range.
    // =================================

    rangePropertiesSubGroup = addProperty(GROUP_PROPERTY, "range",
                                          actorPropertiesSupGroup);

    int decimals = 1;
    valueDecimalsProperty = addProperty(INT_PROPERTY, "decimals",
                                        rangePropertiesSubGroup);
    properties->setInt(valueDecimalsProperty, decimals, 0, 9);

    minimumValueProperty = addProperty(DOUBLE_PROPERTY, "minimum value",
                                       rangePropertiesSubGroup);
    properties->setDouble(minimumValueProperty, minimumValue,
                          decimals, pow(10., -decimals));

    maximumValueProperty = addProperty(DOUBLE_PROPERTY, "maximum value",
                                       rangePropertiesSubGroup);
    properties->setDouble(maximumValueProperty, maximumValue,
                          decimals, pow(10., -decimals));

    clampMaximumProperty = addProperty(BOOL_PROPERTY, "clamp maximum",
                                       rangePropertiesSubGroup);
    properties->mBool()->setValue(clampMaximumProperty, clampMaximum);

    interpolationRangeProperty = addProperty(DOUBLE_PROPERTY, "interpolation range",
                                       rangePropertiesSubGroup);
    properties->setDouble(interpolationRangeProperty, interpolationRange,
                          0.0, DBL_MAX, decimals, pow(10., -decimals));

    // Properties related to alpha blending.
    // =====================================

    alphaBlendingPropertiesSubGroup = addProperty(GROUP_PROPERTY,
                                                  "alpha blending",
                                                  actorPropertiesSupGroup);

    alphaBlendingModeProperty = addProperty(ENUM_PROPERTY, "mode",
                                            alphaBlendingPropertiesSubGroup);
    QStringList alphaModeNames = QStringList();
    alphaModeNames << "use alpha channel" << "use red channel"
                   << "use green channel" << "use blue channel"
                   << "use rgb average" << "use none";
    properties->mEnum()->setEnumNames(alphaBlendingModeProperty, alphaModeNames);
    properties->mEnum()->setValue(alphaBlendingModeProperty, alphaBlendingMode);

    invertAlphaProperty = addProperty(BOOL_PROPERTY, "invert alpha",
                                           alphaBlendingPropertiesSubGroup);
    properties->mBool()->setValue(invertAlphaProperty, invertAlpha);

    useConstantColourProperty = addProperty(BOOL_PROPERTY, "use constant colour",
                                           alphaBlendingPropertiesSubGroup);
    properties->mBool()->setValue(useConstantColourProperty, useConstantColour);

    constantColourProperty = addProperty(COLOR_PROPERTY, "constant colour",
                                        alphaBlendingPropertiesSubGroup);
    properties->mColor()->setValue(constantColourProperty, constantColour);


    // General properties.
    // ===================

    positionProperty = addProperty(RECTF_PROPERTY, "position",
                                   actorPropertiesSupGroup);
    properties->setRectF(positionProperty, QRectF(0.9, 0.9, 0.05, 0.5), 2);


    // Properties related to scale of texture.
    // =======================================

    textureScalePropertiesSubGroup = addProperty(GROUP_PROPERTY, "texture scale",
                                          actorPropertiesSupGroup);
    decimals = 1;
    textureScaleDecimalsProperty = addProperty(INT_PROPERTY, "decimals",
                                        textureScalePropertiesSubGroup);
    properties->setInt(textureScaleDecimalsProperty, decimals, 0, 9);

    textureScaleProperty = addProperty(DOUBLE_PROPERTY, "scale",
                                       textureScalePropertiesSubGroup);
    properties->setDouble(textureScaleProperty, textureScale, decimals,
                          pow(10., -decimals));
    properties->mDouble()->setMinimum(textureScaleProperty, pow(10., -decimals));
    textureScaleProperty->setToolTip("Scale of texture width in data resolution. \n"
                                     "Height is scaled according to aspect ratio.");

    endInitialiseQtProperties();
}


MSpatial1DTransferFunction::~MSpatial1DTransferFunction()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE  0
#define SHADER_TEXTURE_ATTRIBUTE 1

void MSpatial1DTransferFunction::reloadShaderEffects()
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


void MSpatial1DTransferFunction::saveConfiguration(QSettings *settings)
{
    MTransferFunction::saveConfiguration(settings);
    settings->beginGroup(MSpatial1DTransferFunction::getSettingsID());

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
    settings->setValue("valueDecimals",
                       properties->mInt()->value(valueDecimalsProperty));
    settings->setValue("minimumValue",
                       properties->mDouble()->value(minimumValueProperty));
    settings->setValue("maximumValue",
                       properties->mDouble()->value(maximumValueProperty));
    settings->setValue("clampMaximum",
                       properties->mBool()->value(clampMaximumProperty));
    settings->setValue("interpolationRange",
                       properties->mDouble()->value(interpolationRangeProperty));

    // Properties related to alpha blending.
    // =====================================
    settings->setValue("alphaBlendingMode",
                       properties->mEnum()->value(alphaBlendingModeProperty));
    settings->setValue("invertAlpha",
                       properties->mBool()->value(invertAlphaProperty));
    settings->setValue("useConstantColour",
                       properties->mBool()->value(useConstantColourProperty));
    settings->setValue("constantColour",
                       properties->mColor()->value(constantColourProperty));

    // General properties.
    // ===================
    settings->setValue("position",
                       properties->mRectF()->value(positionProperty));

    // Properties related to type of texture.
    // ======================================
    settings->setValue("pathsToLoadedImages", imagePathsString);
    settings->setValue("useMirroredRepeat",
                       properties->mBool()->value(useMirroredRepeatProperty));

    // Properties related to texture scale.
    // ====================================
    settings->setValue("textureScaleDecimals",
                       properties->mInt()->value(textureScaleDecimalsProperty));
    settings->setValue("textureScale",
                       properties->mDouble()->value(textureScaleProperty));

    settings->endGroup();
}


void MSpatial1DTransferFunction::loadConfiguration(QSettings *settings)
{
    MTransferFunction::loadConfiguration(settings);

    settings->beginGroup(MSpatial1DTransferFunction::getSettingsID());

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
    setValueDecimals(settings->value("valueDecimals", 3).toInt());
    setMinimumValue(settings->value("minimumValue", 203.15f).toDouble());
    setMaximumValue(settings->value("maximumValue", 303.15f).toDouble());
    properties->mBool()->setValue(
                clampMaximumProperty,
                settings->value("clampMaximum", true).toBool());
    properties->mDouble()->setValue(
                interpolationRangeProperty,
                settings->value("interpolationRange", 1.0).toDouble());

    // Properties related to alpha blending.
    // =====================================
    properties->mEnum()->setValue(alphaBlendingModeProperty,
                                 settings->value("alphaBlendingMode", 0).toInt());
    properties->mBool()->setValue(invertAlphaProperty,
                                 settings->value("invertAlpha", false).toBool());
    properties->mBool()->setValue(useConstantColourProperty,
                                 settings->value("useConstantColour",
                                                 false).toBool());
    properties->mColor()->setValue(constantColourProperty,
                                 settings->value("constantColour",
                                                 QColor(0, 0, 0, 255))
                                   .value<QColor>());

    // General properties.
    // ===================
    setPosition(settings->value("position", QRectF(0.9, 0.9, 0.05, 0.5)).toRectF());

    // Properties related to type of texture.
    // ======================================
    QStringList paths = ((settings->value("pathsToLoadedImages", "").toString())
                         .split("; ", QString::SkipEmptyParts));

    loadImagesFromPaths(paths);

    properties->mBool()->setValue(
                useMirroredRepeatProperty,
                settings->value("useMirroredRepeat", false).toBool());

    // Properties related to texture scale.
    // ====================================
    properties->mInt()->setValue(
                textureScaleDecimalsProperty,
                settings->value("textureScaleDecimals", 1).toInt());
    properties->mDouble()->setValue(
                textureScaleProperty,
                settings->value("textureScale", 1.0).toDouble());

    settings->endGroup();

    if (isInitialized())
    {
        generateTransferTexture(0, true);
        for (int level = 1; level < loadedImages.size(); level++)
        {
            generateTransferTexture(level, false);
        }
        loadedImages.clear();
        generateTextureBarGeometry();
    }

}


void MSpatial1DTransferFunction::setMinimumValue(float value)
{
    properties->mDouble()->setValue(minimumValueProperty, value);
}


void MSpatial1DTransferFunction::setMaximumValue(float value)
{
    properties->mDouble()->setValue(maximumValueProperty, value);
}


void MSpatial1DTransferFunction::setValueDecimals(int decimals)
{
    properties->mInt()->setValue(valueDecimalsProperty, decimals);
    properties->mDouble()->setDecimals(minimumValueProperty, decimals);
    properties->mDouble()->setSingleStep(minimumValueProperty, pow(10.,-decimals));
    properties->mDouble()->setDecimals(maximumValueProperty, decimals);
    properties->mDouble()->setSingleStep(maximumValueProperty, pow(10.,-decimals));
}


void MSpatial1DTransferFunction::setPosition(QRectF position)
{
    properties->mRectF()->setValue(positionProperty, position);
}


QString MSpatial1DTransferFunction::transferFunctionName()
{
    return getName();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MSpatial1DTransferFunction::initializeActorResources()
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    textureUnit = assignTextureUnit();

    if (!loadedImages.empty())
    {
        generateTransferTexture(0, true);
        for (int level = 1; level < loadedImages.size(); level++)
        {
            generateTransferTexture(level, false);
        }
        loadedImages.clear();
    }

    // Load shader programs.
    bool loadShaders = false;

    loadShaders |= glRM->generateEffectProgram("transfer_colourbar",
                                               colourbarShader);
    loadShaders |= glRM->generateEffectProgram("transfer_geom",
                                               simpleGeometryShader);

    if (loadShaders) reloadShaderEffects();

    generateTextureBarGeometry();
}


void MSpatial1DTransferFunction::onQtPropertyChanged(QtProperty *property)
{
    MTransferFunction::onQtPropertyChanged(property);

    if ( (property == minimumValueProperty)    ||
         (property == maximumValueProperty)    ||
         (property == maxNumTicksProperty)     ||
         (property == maxNumLabelsProperty)    ||
         (property == positionProperty)        ||
         (property == tickWidthProperty)       ||
         (property == labelSpacingProperty)    ||
         (property == labelSizeProperty)       ||
         (property == labelColourProperty)     ||
         (property == labelBBoxProperty)       ||
         (property == labelBBoxColourProperty)   )
    {
        if (suppressActorUpdates()) return;

        generateTextureBarGeometry();
        emitActorChangedSignal();
    }

    else if (property == loadLevelsImagesProperty)
    {
        QFileDialog dialog(nullptr);
        dialog.setFileMode(QFileDialog::ExistingFiles);
        dialog.setNameFilter(tr("Image Files (*.gif *.png *.jpg *.jpeg)"));
        dialog.setDirectory(pathsToLoadedImages.at(0));
        QStringList fileNames;
        if (dialog.exec())
        {
            fileNames = dialog.selectedFiles();
            if (fileNames.length() >= 2)
            {
                numLevels = fileNames.length();

                QDialog dialog(nullptr);
                dialog.setWindowTitle("Change order of textures:");
                QVBoxLayout *layout = new QVBoxLayout();
                QListWidget *listWidget = new QListWidget(nullptr);
                dialog.setLayout(layout);
                layout->addWidget(listWidget);
                listWidget->addItems(fileNames);
                listWidget->setDragEnabled(true);
                listWidget->setDropIndicatorShown(true);
                listWidget->setDragDropMode(QAbstractItemView::InternalMove);
                QPushButton *okButton = new QPushButton(nullptr);
                okButton->setText("OK");
                connect(okButton, SIGNAL(clicked()), &dialog, SLOT(accept()));
                layout->addWidget(okButton);

                dialog.exec();
                // Clear fileNames to store file names in user set order.
                fileNames.clear();

                for(int index = 0; index < listWidget->count(); index++)
                {
                  QListWidgetItem *item = listWidget->item(index);
                  fileNames.push_back(item->text());
                }

                listWidget->close();
                delete layout;
                delete listWidget;

                pathsToLoadedImages.resize(numLevels);
                loadImagesFromPaths(fileNames);
                loadedImages.clear();
            }
            else
            {
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setText(QString("Amount of selceted files is not enough!\n"
                                       "You have to select at least 2 image "
                                       "files!"));
                msgBox.exec();
                return;
            }
        }
    }

    else if (property == useMirroredRepeatProperty)
    {
        useMirroredRepeat =
                properties->mBool()->value(useMirroredRepeatProperty);

        if (tfTexture != nullptr)
        {
            MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
            glRM->makeCurrent();
            tfTexture->bindToTextureUnit(0);

            if (useMirroredRepeat)
            {
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,
                                GL_MIRRORED_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,
                                GL_MIRRORED_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R,
                                GL_MIRRORED_REPEAT); CHECK_GL_ERROR;
            }
            else
            {
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,
                                GL_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,
                                GL_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R,
                                GL_REPEAT); CHECK_GL_ERROR;
            }
            // Need to set these parameters as well, otherwise they are set back
            // to their default values.
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR); CHECK_GL_ERROR;
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR_MIPMAP_LINEAR); CHECK_GL_ERROR;

        }

        if (suppressActorUpdates()) return;

        emitActorChangedSignal();
    }

    else if (property == clampMaximumProperty)
    {
        clampMaximum = properties->mBool()->value(clampMaximumProperty);

        if (suppressActorUpdates()) return;

        emitActorChangedSignal();
    }

    else if (property == valueDecimalsProperty)
    {
        int decimals = properties->mInt()->value(valueDecimalsProperty);
        properties->mDouble()->setDecimals(minimumValueProperty, decimals);
        properties->mDouble()->setSingleStep(minimumValueProperty,
                                             pow(10.,-decimals));
        properties->mDouble()->setDecimals(maximumValueProperty, decimals);
        properties->mDouble()->setSingleStep(maximumValueProperty,
                                             pow(10.,-decimals));
        properties->mDouble()->setDecimals(interpolationRangeProperty, decimals);
        properties->mDouble()->setSingleStep(interpolationRangeProperty,
                                             pow(10.,-decimals));

        if (suppressActorUpdates()) return;

        // Texture remains unchanged; only geometry needs to be updated.
        generateTextureBarGeometry();
        emitActorChangedSignal();
    }

    else if (property == interpolationRangeProperty)
    {
        interpolationRange =
                properties->mDouble()->value(interpolationRangeProperty);

        if (suppressActorUpdates()) return;

        emitActorChangedSignal();
    }

    else if (property == alphaBlendingModeProperty)
    {
        alphaBlendingMode =
                AlphaBlendingMode(
                    properties->mEnum()->value(alphaBlendingModeProperty));

        // Only invert alpha if alphaBlendingMode is not "none".
        // Therefore disable invertAlpha-Property and set invertAlpha to false.
        if (alphaBlendingMode == AlphaBlendingMode::None)
        {
            enableActorUpdates(false);
            invertAlphaProperty->setEnabled(false);
            properties->mBool()->setValue(invertAlphaProperty, false);
            enableActorUpdates(true);
        }
        else
        {
            enableActorUpdates(false);
            invertAlphaProperty->setEnabled(true);
            enableActorUpdates(true);
        }

        if (suppressActorUpdates()) return;

        emitActorChangedSignal();
    }

    else if (property == invertAlphaProperty)
    {
        invertAlpha = properties->mBool()->value(invertAlphaProperty);

        if (suppressActorUpdates()) return;

        emitActorChangedSignal();
    }

    else if (property == useConstantColourProperty)
    {
        useConstantColour = properties->mBool()->value(useConstantColourProperty);

        if (suppressActorUpdates()) return;

        emitActorChangedSignal();
    }

    else if (property == constantColourProperty)
    {
        constantColour = properties->mColor()->value(constantColourProperty);

        if (suppressActorUpdates() || !useConstantColour) return;

        emitActorChangedSignal();
    }

    else if (property == textureScaleDecimalsProperty)
    {
        int decimals = properties->mInt()->value(textureScaleDecimalsProperty);
        double minStep = pow(10.,-decimals);

        properties->mDouble()->setDecimals(textureScaleProperty, decimals);
        properties->mDouble()->setSingleStep(textureScaleProperty, minStep);
        properties->mDouble()->setMinimum(textureScaleProperty, minStep);
    }

    else if (property == textureScaleProperty)
    {
        textureScale = properties->mDouble()->value(textureScaleProperty);
        emitActorChangedSignal();
    }

    return;
}


void MSpatial1DTransferFunction::renderToCurrentContext(
        MSceneViewGLWidget *sceneView)
{
    Q_UNUSED(sceneView);

    if (tfTexture != nullptr)
    {
        tfTexture->bindToTextureUnit(textureUnit);
        int viewPortWidth = sceneView->getViewPortWidth();
        int viewPortHeight = sceneView->getViewPortHeight();

        glAlphaFunc(GL_GREATER, 0.1);
        glEnable(GL_ALPHA_TEST);

        QRectF positionRect = properties->mRectF()->value(positionProperty);
        float barWidth     = positionRect.width();
        float barHeight    = positionRect.height();

        // First draw the colourbar itself. glPolygonOffset is used to displace
        // the colourbar's z-value slightly to the back, to that the frame drawn
        // afterwards is rendered correctly.
        colourbarShader->bindProgram("spatialTF");

        colourbarShader->setUniformValue("distInterp",
                                         GLfloat(interpolationRange));

        colourbarShader->setUniformValue("minimumValue",
                                         GLfloat(minimumValue));
        colourbarShader->setUniformValue("maximumValue",
                                         GLfloat(maximumValue));

        colourbarShader->setUniformValue("viewPortWidth",
                                         GLint(viewPortWidth));
        colourbarShader->setUniformValue("viewPortHeight",
                                         GLint(viewPortHeight));
        colourbarShader->setUniformValue("textureWidth",
                                         GLint(currentTextureWidth));
        colourbarShader->setUniformValue("textureHeight",
                                         GLint(currentTextureHeight));
        colourbarShader->setUniformValue("barWidthF",
                                         GLfloat(barWidth));
        colourbarShader->setUniformValue("barHeightF",
                                         GLfloat(barHeight));

        colourbarShader->setUniformValue("numLevels",
                                         GLint(numLevels));

        colourbarShader->setUniformValue("alphaBlendingMode",
                                         GLenum(alphaBlendingMode));
        colourbarShader->setUniformValue("invertAlpha",
                                         GLboolean(invertAlpha));
        colourbarShader->setUniformValue("useConstantColour",
                                         GLboolean(useConstantColour));
        colourbarShader->setUniformValue("constantColour", constantColour);

        colourbarShader->setUniformValue("spatialTransferTexture", textureUnit);CHECK_GL_ERROR;

        vertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE, 3, false,
                                              5 * sizeof(float), 0 * sizeof(float));CHECK_GL_ERROR;
        vertexBuffer->attachToVertexAttribute(SHADER_TEXTURE_ATTRIBUTE, 2, false,
                                              5 * sizeof(float),
                                              (const GLvoid*)(3 * sizeof(float)));CHECK_GL_ERROR;

        glPolygonOffset(.01f, 1.0f);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, numVertices); CHECK_GL_ERROR;
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Next draw a black frame around the colourbar.
    simpleGeometryShader->bindProgram("Simple"); CHECK_GL_ERROR;
    simpleGeometryShader->setUniformValue("colour", QColor(0, 0, 0)); CHECK_GL_ERROR;
    vertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE, 3, false,
                                          5 * sizeof(float),
                                          (const GLvoid*)(10 * sizeof(float)));

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
    glLineWidth(1);
    glDrawArrays(GL_LINE_LOOP, 0, numVertices); CHECK_GL_ERROR;

    // Finally draw the tick marks.
    vertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE, 3, false,
                                          0,
                                          (const GLvoid*)(30 * sizeof(float)));

    glDrawArrays(GL_LINES, 0, 2 * numTicks); CHECK_GL_ERROR;

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MSpatial1DTransferFunction::generateTransferTexture(int level, bool recreate)
{
    if (!loadedImages.at(level).isNull())
    {
        MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

        // Upload the texture to GPU memory:
        if (tfTexture == nullptr)
        {
            // No texture exists. Create a new one and register with memory manager.
            QString textureID = QString("spatialTransferFunction_#%1").arg(getID());
            tfTexture = new GL::MTexture(textureID, GL_TEXTURE_2D_ARRAY,
                                         GL_RGBA32F,
                                         currentTextureWidth,
                                         currentTextureHeight,
                                         numLevels);

            if ( !glRM->tryStoreGPUItem(tfTexture) )
            {
                delete tfTexture;
                tfTexture = nullptr;
            }


            if (useMirroredRepeat)
            {
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,
                                GL_MIRRORED_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,
                                GL_MIRRORED_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R,
                                GL_MIRRORED_REPEAT); CHECK_GL_ERROR;
            }
            else
            {
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,
                                GL_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,
                                GL_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R,
                                GL_REPEAT); CHECK_GL_ERROR;
            }
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR); CHECK_GL_ERROR;
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR_MIPMAP_LINEAR); CHECK_GL_ERROR;

            glRM->makeCurrent();
            tfTexture->bindToTextureUnit(0);

            glTexImage3D(GL_TEXTURE_2D_ARRAY,            // target
                         0,                         // level of detail
                         GL_RGBA32F,                // internal format
                         currentTextureWidth,       // width
                         currentTextureHeight,      // height
                         numLevels,                 // depth
                         0,                         // border
                         GL_RGBA,                   // format
                         GL_UNSIGNED_BYTE,          // data type of the pixel data
                         NULL); CHECK_GL_ERROR;
        }

        if (recreate)
        {
            tfTexture->updateSize(currentTextureWidth, currentTextureHeight,
                                  numLevels);
            glRM->makeCurrent();
            tfTexture->bindToTextureUnit(0);

            if (useMirroredRepeat)
            {
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,
                                GL_MIRRORED_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,
                                GL_MIRRORED_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R,
                                GL_MIRRORED_REPEAT); CHECK_GL_ERROR;
            }
            else
            {
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,
                                GL_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,
                                GL_REPEAT); CHECK_GL_ERROR;
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R,
                                GL_REPEAT); CHECK_GL_ERROR;
            }
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR); CHECK_GL_ERROR;
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR_MIPMAP_LINEAR); CHECK_GL_ERROR;


            glTexImage3D(GL_TEXTURE_2D_ARRAY,            // target
                         0,                         // level of detail
                         GL_RGBA32F,                // internal format
                         currentTextureWidth,       // width
                         currentTextureHeight,      // height
                         numLevels,                 // depth
                         0,                         // border
                         GL_RGBA,                   // format
                         GL_UNSIGNED_BYTE,          // data type of the pixel data
                         NULL); CHECK_GL_ERROR;
        }

        if ( tfTexture )
        {
//            tfTexture->updateSize(loadedImage.width(), loadedImage.height());
            tfTexture->updateSize(currentTextureWidth, currentTextureHeight,
                                  numLevels);

            glRM->makeCurrent();
            tfTexture->bindToTextureUnit(0);

            // Upload data array to GPU.
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY,             // target
                            0,                         // level of detail
                            0,                         // xoffset
                            0,                         // yoffset
                            level,                     // zoffset
                            currentTextureWidth,       // width
                            currentTextureHeight,      // height
                            1,                         // depth
                            GL_RGBA,                   // format
                            GL_UNSIGNED_BYTE,          // data type of the pixel data
                            loadedImages.at(level).bits()); CHECK_GL_ERROR;

//             glGenerateTextureMipmap(tfTexture->getTextureObject());
            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        }
    }
}


void MSpatial1DTransferFunction::generateTextureBarGeometry()
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    // Create geometry for a box filled with the colourbar texture.

    // Get user-defined upper left corner x, y, z and width, height in clip
    // space.
    QRectF positionRect = properties->mRectF()->value(positionProperty);
    float ulcrnr[3] = { float(positionRect.x()), float(positionRect.y()), -1. };
    float width     = positionRect.width();
    float height    = positionRect.height();

    // This array accomodates the geometry for two filled triangles showing the
    // actual colourbar (GL_TRIANGLE_STRIP). The 3rd, 4th, and the additional
    // 5th and 6th vertices are used to draw a box around the colourbar
    // (GL_LINE_LOOP).
    float coordinates[30] =
    {
        ulcrnr[0]        , ulcrnr[1]         , ulcrnr[2], 1, 0, // ul
        ulcrnr[0]        , ulcrnr[1] - height, ulcrnr[2], 0, 0, // ll
        ulcrnr[0] + width, ulcrnr[1]         , ulcrnr[2], 1, 1, // ur
        ulcrnr[0] + width, ulcrnr[1] - height, ulcrnr[2], 0, 1, // lr
        ulcrnr[0]        , ulcrnr[1] - height, ulcrnr[2], 0, 0, // ll for frame
        ulcrnr[0]        , ulcrnr[1]         , ulcrnr[2], 1, 0, // ul for frame
    };

    // ========================================================================
    // Next, generate the tickmarks. maxNumTicks tickmarks are drawn, but not
    // more than colour steps.
//    int numSteps = properties->mInt()->value(numStepsProperty);
//    int maxNumTicks = properties->mInt()->value(maxNumTicksProperty);
//    numTicks = min(numSteps+1, maxNumTicks);
    int maxNumTicks = properties->mInt()->value(maxNumTicksProperty);
    numTicks = maxNumTicks;

    // This array accomodates the tickmark geometry.
    float tickmarks[6 * numTicks];

    // Width of the tickmarks in clip space.
    float tickwidth = properties->mDouble()->value(tickWidthProperty);

    int n = 0;
    for (uint i = 0; i < numTicks; i++)
    {
        tickmarks[n++] = ulcrnr[0];
        tickmarks[n++] = ulcrnr[1] - i * (height / (numTicks-1));
        tickmarks[n++] = ulcrnr[2];
        tickmarks[n++] = ulcrnr[0] - tickwidth;
        tickmarks[n++] = ulcrnr[1] - i * (height / (numTicks-1));
        tickmarks[n++] = ulcrnr[2];
    }

    // ========================================================================
    // Now we can upload the two geometry arrays to the GPU. Switch to the
    // shared background context so all views can access the VBO generated
    // here.
    glRM->makeCurrent();

    QString requestKey = QString("vbo_transfer_function_actor_%1").arg(getID());

    GL::MVertexBuffer* vb =
            static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(requestKey));


    if (vb)
    {
        vertexBuffer = vb;
        GL::MFloatVertexBuffer* buf = dynamic_cast<GL::MFloatVertexBuffer*>(vb);
        // reallocate buffer if size has changed
//        buf->reallocate(nullptr, 30);
//        buf->update(coordinates, 0, 0, sizeof(coordinates));
        buf->reallocate(nullptr, 30 + numTicks * 6);
        buf->update(coordinates, 0, 0, sizeof(coordinates));
        buf->update(tickmarks, 0, sizeof(coordinates), sizeof(tickmarks));

    } else
    {
//        GL::MFloatVertexBuffer* newVB = nullptr;
//        newVB = new GL::MFloatVertexBuffer(requestKey, 30);
//        if (glRM->tryStoreGPUItem(newVB))
//        {
//            newVB->reallocate(nullptr, 30, 0, true);
//            newVB->update(coordinates, 0, 0, sizeof(coordinates));

//        } else { delete newVB; }
//        vertexBuffer = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(requestKey));

        GL::MFloatVertexBuffer* newVB = nullptr;
        newVB = new GL::MFloatVertexBuffer(requestKey, 30 + numTicks * 6);
        if (glRM->tryStoreGPUItem(newVB))
        {
            newVB->reallocate(nullptr, 30 + numTicks * 6, 0, true);
            newVB->update(coordinates, 0, 0, sizeof(coordinates));
            newVB->update(tickmarks, 0, sizeof(coordinates), sizeof(tickmarks));

        } else { delete newVB; }
        vertexBuffer = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(requestKey));
    }

    // Required for the glDrawArrays() call in renderToCurrentContext().
    numVertices = 4;

    minimumValue = properties->mDouble()->value(minimumValueProperty);
    maximumValue = properties->mDouble()->value(maximumValueProperty);

    // ========================================================================
    // Finally, place labels at the tickmarks:

    int maxNumLabels = properties->mInt()->value(maxNumLabelsProperty);

    // Obtain a shortcut to the application's text manager to register the
    // labels generated in the loops below.
    MTextManager* tm = glRM->getTextManager();

    // Remove all text labels of the old geometry.
    while (!labels.isEmpty()) tm->removeText(labels.takeLast());

    // A maximum of maxNumLabels are placed. The approach taken here is to
    // compute a "tick step size" from the number of ticks drawn and the
    // maximum number of labels to be drawn. A label will then be placed
    // every tickStep-th tick. The formula tries to place a label at the
    // lower and upper end of the colourbar, if possible.
    int tickStep = ceil(double(numTicks-1) / double(maxNumLabels-1));

    // The (clip-space) distance between the ends of the tick marks and the
    // labels.
    float labelSpacing = properties->mDouble()->value(labelSpacingProperty);

    // Number of label decimals to be printed.
    int decimals = properties->mInt()->value(valueDecimalsProperty);

    // Label font size and colour.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);

    // Label bounding box.
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);

    // Scale factor for labels.
    float scaleFactor = 1.0f; //properties->mDouble()->value(scaleFactorProperty);

    // Register the labels with the text manager.
    for (uint i = 0; i < numTicks; i += tickStep) {
        float value = maximumValue - double(i) / double(numTicks-1)
                * (maximumValue - minimumValue);
        labels.append(tm->addText(
                          QString("%1").arg(value*scaleFactor, 0, 'f', decimals),
                          MTextManager::CLIPSPACE,
                          tickmarks[6*i + 3] - labelSpacing,
                          tickmarks[6*i + 4],
                          tickmarks[6*i + 5],
                          labelsize,
                          labelColour, MTextManager::MIDDLERIGHT,
                          labelbbox, labelBBoxColour)
                      );
    }
}


void MSpatial1DTransferFunction::loadImagesFromPaths(QStringList pathList)
{
    if (!pathList.empty())
    {
        int newWidth  = 0;
        int newHeight = 0;
        int level = 0;
        bool setSize = true;

        pathsToLoadedImages.clear();
        loadedImages.clear();

        foreach (QString path, pathList)
        {
            if (path != "")
            {
                QImage image(path);
                if (!image.isNull())
                {
                    if (setSize)
                    {
                        newWidth  = image.width();
                        newHeight = image.height();
                        setSize = false;
                    }

                    if (image.width() != newWidth || image.height() != newHeight)
                    {
                        QMessageBox msgBox;
                        msgBox.setIcon(QMessageBox::Warning);
                        msgBox.setText(
                                    QString("Selected images don't have the "
                                            "same size.\n"
                                            "Aborting texture generation!"));
                        msgBox.exec();
                        loadedImages.clear();
                        return;
                    }

                    loadedImages.append(QGLWidget::convertToGLFormat(image));

                    pathsToLoadedImages.insert(level, path);
                    level++;
                }
                else
                {
                    QMessageBox msgBox;
                    msgBox.setIcon(QMessageBox::Warning);
                    msgBox.setText(QString("Image '%1':\n does not exist.\n"
                                           "No image was loaded.").arg(path));
                    msgBox.exec();
                }
            }

        }

        if (loadedImages.size() >= 2)
        {
            numLevels = loadedImages.size();
            currentTextureWidth  = newWidth;
            currentTextureHeight = newHeight;

            QString text("");
            imagePathsString = "";

            // Construct String to store all paths in one property.
            for (int i = 0; i < (numLevels - 1); i++)
            {
                text.append(QString("%1: ").arg(i) + pathsToLoadedImages.at(i)
                            + "; \n");
                imagePathsString.append(pathsToLoadedImages.at(i)
                                        + "; ");
            }
            text.append(QString("%1: ").arg(numLevels - 1)
                        + pathsToLoadedImages.at(numLevels - 1));
            imagePathsString.append(pathsToLoadedImages.at(numLevels - 1));


            properties->mString()->setValue(pathToLoadedImagesProperty, text);
            pathToLoadedImagesProperty->setToolTip(text);

            if (suppressActorUpdates()) return;

            // Initialize texture with width and height of images.
            generateTransferTexture(0, true);
            // Fill texture with all given images.
            for (int level = 1; level < numLevels; level++)
            {
                generateTransferTexture(level, false);
            }
            emitActorChangedSignal();
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(QString("Could not create enough images!\n"
                                   "Therefore no texture was created!"));
            msgBox.exec();
        }
    }
}

} // namespace Met3D
