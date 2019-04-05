/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Bianca Tost
**  Copyright 2015-2017 Marc Rautenhaus
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
#include "plotcollection.h"

// standard library imports
#include <iostream>
#include <functional>

// related third party imports
#include <QtCore>
#include <log4cplus/loggingmacros.h>
#include <omp.h>

// local application imports
#include "gxfw/nwpmultivaractor.h"
#include "actors/nwphorizontalsectionactor.h"
#include "util/fastmarch/fastmarch.h"

using namespace std;

namespace Met3D
{


/******************************************************************************
***                              MACROS                                     ***
*******************************************************************************/

// (member + index) index macro to access index of member m for contour
// boxplots.
#define cbpINDEXmi(m, i, size)  ((m) * (size) + (i))


/******************************************************************************
***                             CONSTANTS                                   ***
*******************************************************************************/

// Maximum number of members.
const int MAX_NUM_MEMBERS = 51;
// Maximum number of contour pairs one contour needs to be tested against.
const int MAX_NUM_PAIRS = ((MAX_NUM_MEMBERS - 1) * (MAX_NUM_MEMBERS - 2)) / 2;
// Maximum size of matrix containing all combinations of all members and the
// pairs of members not containing the member (matrix is needed to compute a
// default epsilon for contour boxplot).
const int MAX_EPSILON_MATRIX_SIZE = MAX_NUM_MEMBERS * MAX_NUM_PAIRS;

const float MISSING_VALUE    = -999.E9;

/******************************************************************************
***                             MPlotCollection                             ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MPlotCollection::MPlotCollection(MNWP2DSectionActorVariable *var)
    : gridDataStorage(nullptr),
      textureLineDrawing(nullptr),
      spaghettiPlot(nullptr),
      contourPlot(nullptr),
      textureContourBoxplot(nullptr),
      textureCBPBandDepth(nullptr),
      textureCBPEpsilonMatrix(nullptr),
      textureContourProbabilityPlot(nullptr),
      variabilityPlot(nullptr),
      textureScalarVariabilityPlot(nullptr),
      var(var),
      textureHandles(nullptr),
      gridTextureHandles(nullptr),
      contourSetIndex(0),
      numMembers(0),
      suppresssUpdate(false)
{
    actor = static_cast<MNWPHorizontalSectionActor*>(var->actor);

    // Set pointers to shader effects.
    setShaders();

    MQtProperties *properties = actor->getQtProperties();

    QtProperty *renderGroup = var->getPropertyGroup("rendering");
    assert(renderGroup != nullptr);

    // Add plotting techniques to render mode selection.
    // Corresponding enum is part of MNWP2DSectionActorVariable.
    QStringList renderModeNames =
            properties->getEnumItems(var->renderSettings.renderModeProperty);
    renderModeNames << "spaghetti plot"
                    << "contour boxplot"
                    << "contour probability plot"
                    << "distance variability plot"
                    << "scalar variability plot";
    properties->mEnum()->setEnumNames(
                var->renderSettings.renderModeProperty, renderModeNames);

    // Create and initialise QtProperties for the GUI.
    // ===============================================

    contourSetProperty =
            actor->addProperty(ENUM_PROPERTY, "contour set used by plots",
                           renderGroup);
    properties->mEnum()->setEnumNames(contourSetProperty,
                                      *var->getContourSetStringList());

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // Setup of spaghetti plots.
    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    spaghettiPlot = new SpaghettiPlot(actor, renderGroup);

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // Setup of contour plots.
    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    contourPlot = new ContourPlot(actor, renderGroup);

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // Setup of variability plots.
    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    variabilityPlot = new VariabilityPlot(actor, renderGroup);


    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // Setup of textures and arrays needed.
    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    binaryMapTextureContainer.clear();

    variabilityPlot->distanceStorage.clear();

    distanceTextureContainer.clear();
    variabilityPlot->distanceMean.clear();
}


MPlotCollection::~MPlotCollection()
{
    deleteTexturesAndArrays();

    delete spaghettiPlot;
    delete contourPlot;
    delete variabilityPlot;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MPlotCollection::saveConfiguration(QSettings *settings)
{
    settings->setValue("contourSetIndex", contourSetIndex);

    spaghettiPlot->saveConfiguration(settings);

    contourPlot->saveConfiguration(settings);

    variabilityPlot->saveConfiguration(settings);
}


void MPlotCollection::loadConfiguration(QSettings *settings)
{
    MQtProperties *properties = var->actor->getQtProperties();

    properties->mEnum()->setValue(contourSetProperty,
                                  settings->value("contourSetIndex", 0).toInt());

    spaghettiPlot->loadConfiguration(settings, properties);

    contourPlot->loadConfiguration(settings, properties);

    variabilityPlot->loadConfiguration(settings, properties);

}


bool MPlotCollection::onQtPropertyChanged(QtProperty *property)
{
    MQtProperties *properties = var->actor->getQtProperties();

    if (property == contourSetProperty)
    {
        contourSetIndex = properties->mEnum()->value(contourSetProperty);
        if (!suppresssUpdate)
        {
            if (var->gridAggregation == nullptr)
            {
                return false;
            }
            needsRecomputation();
        }
        return !suppresssUpdate;
    }

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // Spaghetti plot properties.
    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    else if (property == spaghettiPlot->colourProperty)
    {
        spaghettiPlot->colour = properties->mColor()->value(
                    spaghettiPlot->colourProperty);
        return true;
    }

    else if (property == spaghettiPlot->thicknessProperty)
    {
        spaghettiPlot->thickness = properties->mDouble()->value(
                    spaghettiPlot->thicknessProperty);
        return true;
    }

    else if (property == spaghettiPlot->prismaticColouredProperty)
    {
        spaghettiPlot->prismaticColoured = properties->mBool()->value(
                    spaghettiPlot->prismaticColouredProperty);
        // Disable colour property if prismatic colour is active and enable
        // the property otherwise.
        spaghettiPlot->colourProperty->setEnabled(
                    !spaghettiPlot->prismaticColoured);
        return true;
    }

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // Contour plot properties.
    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    else if (property == contourPlot->innerColourProperty)
    {
        contourPlot->innerColour = properties->mColor()->value(
                    contourPlot->innerColourProperty);
        return true;
    }

    else if (property == contourPlot->drawOuterProperty)
    {
        contourPlot->drawOuter = properties->mBool()->value(
                    contourPlot->drawOuterProperty);
        return true;
    }

    else if (property == contourPlot->outerColourProperty)
    {
        contourPlot->outerColour = properties->mColor()->value(
                    contourPlot->outerColourProperty);
        return true;
    }

    else if (property == contourPlot->drawMedianProperty)
    {
        contourPlot->drawMedian = properties->mBool()->value(
                    contourPlot->drawMedianProperty);
        return true;
    }

    else if (property == contourPlot->medianThicknessProperty)
    {
        contourPlot->medianThickness = properties->mDouble()->value(
                    contourPlot->medianThicknessProperty);
        return true;
    }

    else if (property == contourPlot->medianColourProperty)
    {
        contourPlot->medianColour = properties->mColor()->value(
                    contourPlot->medianColourProperty);
        return true;
    }

    // Contour boxplot properties.
    // ===========================
    else if (property == contourPlot->drawOutliersProperty)
    {
        contourPlot->drawOutliers = properties->mBool()->value(
                    contourPlot->drawOutliersProperty);
        return true;
    }

    else if (property == contourPlot->outlierThicknessProperty)
    {
        contourPlot->outlierThickness = properties->mDouble()->value(
                    contourPlot->outlierThicknessProperty);

        return true;
    }

    else if (property == contourPlot->outlierColourProperty)
    {
        contourPlot->outlierColour = properties->mColor()->value(
                    contourPlot->outlierColourProperty);
        return true;
    }

    else if (property == contourPlot->epsilonProperty)
    {
        double epsilon = properties->mDouble()->value(
                    contourPlot->epsilonProperty);
        if ((epsilon != contourPlot->epsilon))
        {
            contourPlot->epsilon = epsilon;
            // Avoid sending events if epsilon is changed during one event
            // (e.g. update to new default epsilon).
            if (contourPlot->boxplotNeedsRecompute
                    || contourPlot->epsilonChanged)
            {
                return false;
            }
            else
            {
                // Changing epsilon does not affect the binary maps nor the
                // default epsilon hence the only render texture needs to be
                // recomputed.
                contourPlot->epsilonChanged = true;
                return true;
            }
        }
        return false;
    }

    else if (property == contourPlot->useDefaultEpsilonProperty)
    {
        contourPlot->useDefaultEpsilon = properties->mBool()->value(
                    contourPlot->useDefaultEpsilonProperty);

        // Change epsilon/recompute render texture only if necessary (switching
        // from "not using" to "using" and value in epsilon differs from default
        // epsilon).
        if (contourPlot->useDefaultEpsilon
                && (contourPlot->defaultEpsilon != contourPlot->epsilon) )
        {
            properties->mDouble()->setValue(
                        contourPlot->epsilonProperty,
                        contourPlot->defaultEpsilon);
        }

        // Allow the user to change the epsilon manually only if the user does
        // not use the default epsilon.
        contourPlot->epsilonProperty->setEnabled(
                    !contourPlot->useDefaultEpsilon);
        return false;
    }

    // Contour probability plot properties.
    // ====================================
    else if (property == contourPlot->innerPercentageProperty)
    {
        contourPlot->innerPercentage = properties->mDDouble()->value(
                    contourPlot->innerPercentageProperty);

        // Adapt outermost width if necessary (i.e. new boundary of inner band
        // exceeds outer boundary of outer band thus outermost band width needs
        // to be reduced. We need to double the outer percentage value since it
        // represents only one half of the outermost band while the inner
        // percentage value represents the complete inner band.).
        if (contourPlot->innerPercentage
                > (100. - (2. * contourPlot->outerPercentage)))
        {
            properties->setDDouble(
                        contourPlot->outerPercentageProperty,
                        floor(50. - 0.5 * contourPlot->innerPercentage), 0.,
                        50., 0, 5., " %");
            return false;
        }

        contourPlot->probabilityNeedsRecompute = true;
        return true;
    }

    else if (property == contourPlot->outerPercentageProperty)
    {
        contourPlot->outerPercentage = properties->mDDouble()->value(
                    contourPlot->outerPercentageProperty);

        // Adapt inner width if necessary (i.e. new inner boundary of outermost
        // band exceeds boundary of inner band thus inner band width needs to be
        // reduced. We need to halve the inner percentage value since the outer
        // percentage value represents only one half of the outermost band while
        // the inner percentage value represents the complete inner band.).
        if (contourPlot->outerPercentage
                > (50. - (0.5 * contourPlot->innerPercentage)))
        {
            properties->setDDouble(
                        contourPlot->innerPercentageProperty,
                        floor(100. - (2. * contourPlot->outerPercentage)),
                        0., 100., 0, 5., " %");
            return false;
        }

        contourPlot->probabilityNeedsRecompute = true;
        return true;
    }

    else if (property == contourPlot->drawoutermostProperty)
    {
        contourPlot->drawOutermost = properties->mBool()->value(
                    contourPlot->drawoutermostProperty);
        return true;
    }

    else if (property == contourPlot->outermostColourProperty)
    {
        contourPlot->outermostColour = properties->mColor()->value(
                    contourPlot->outermostColourProperty);
        return true;
    }

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // Variability plot.
    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    else if (property == variabilityPlot->colourProperty)
    {
        variabilityPlot->colour = properties->mColor()->value(
                    variabilityPlot->colourProperty);
        return true;
    }

    else if (property == variabilityPlot->scaleProperty)
    {
        variabilityPlot->scale = properties->mDouble()->value(
                    variabilityPlot->scaleProperty);
        // Scale change does not affect distance fields thus only render
        // textures need to be recomputed.
        variabilityPlot->distanceScaleChanged = true;
        // Since the scalar variability plot does not have any "preprocessing"
        // step and thus the render texture is the only component to be
        // computed, changing the scale provokes the complete re-computation
        // of the scalar variability plot.
        variabilityPlot->scalarNeedsRecompute = true;
        return true;
    }

    else if (property == variabilityPlot->drawMeanProperty)
    {
        variabilityPlot->drawMean = properties->mBool()->value(
                    variabilityPlot->drawMeanProperty);
        return true;
    }

    else if (property == variabilityPlot->meanThicknessProperty)
    {
        variabilityPlot->meanThickness = properties->mDouble()->value(
                    variabilityPlot->meanThicknessProperty);

        return true;
    }

    else if (property == variabilityPlot->meanColourProperty)
    {
        variabilityPlot->meanColour = properties->mColor()->value(
                    variabilityPlot->meanColourProperty);
        return true;
    }

    return false;
}


void MPlotCollection::reset()
{
    deleteTexturesAndArrays();
    needsRecomputation();
}


void MPlotCollection::needsRecomputation()
{
    contourPlot->boxplotNeedsRecompute      = true;
    contourPlot->probabilityNeedsRecompute  = true;
    variabilityPlot->distanceNeedsRecompute = true;
    variabilityPlot->scalarNeedsRecompute   = true;
}


void MPlotCollection::recreateArrays()
{
    // Delete old array if necessary.
    if (gridDataStorage != nullptr)
    {
        delete gridDataStorage;
        gridDataStorage = nullptr;
    }
    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();
    // Use array of the size of the textures holding the scalar fields.
    unsigned int texturesize =
            grids.at(0)->getNumLons() * grids.at(0)->getNumLats();
    unsigned int size = static_cast<unsigned int>(grids.size()) * texturesize;
    // Set up new array to store data of the ensemble scalar fields in it.
    gridDataStorage = new float[size];

    return;
}


void MPlotCollection::compute()
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);

    // Compute requested plot only if needed components are available, i.e.
    // scalar fields to compute plot from (gridAggregation) and isovalue[s] to
    // compute plot for.
    if (var->gridAggregation != nullptr
            && !var->contourSetList.at(contourSetIndex).levels.empty())
    {
        // Get number of ensemble members currently used.
        numMembers = var->gridAggregation->getGrids().size();
        // Only compute the plot currently selected and only if necessary (i.e.
        // it is not computed yet).
        switch (var->renderSettings.renderMode)
        {
        case MNWP2DHorizontalActorVariable::RenderMode::ContourBoxplot:
        {
            // Contour boxplot needs at least three ensemble members to be
            // computed from.
            if (numMembers >= 3 && (contourPlot->epsilonChanged
                                    || contourPlot->boxplotNeedsRecompute))
            {
                if (contourPlot->boxplotNeedsRecompute)
                {
                    createContourBoxplotTextures();

                    computeContourBoxplotsBinaryMap();
                    computeContourBoxplotDefaultEpsilon();
                    contourPlot->boxplotNeedsRecompute = false;
                }
                computeContourBoxplot();
                contourPlot->epsilonChanged = false;
            }
            break;
        }
        case MNWP2DHorizontalActorVariable::RenderMode::ContourProbabilityPlot:
        {
            if (contourPlot->probabilityNeedsRecompute)
            {
                computeContourProbabilityPlot();
                contourPlot->probabilityNeedsRecompute = false;
            }
            break;
        }
        case MNWP2DHorizontalActorVariable::RenderMode::DistanceVariabilityPlot:
        {
            if (variabilityPlot->distanceScaleChanged
                    || variabilityPlot->distanceNeedsRecompute)
            {
                computeDistanceVariabilityPlot();
                variabilityPlot->distanceNeedsRecompute = false;
                variabilityPlot->distanceScaleChanged = false;
            }
            break;
        }
        case MNWP2DHorizontalActorVariable::RenderMode::ScalarVariabilityPlot:
        {
            if (variabilityPlot->scalarNeedsRecompute)
            {
                computeScalarVariabilityPlot();
                variabilityPlot->scalarNeedsRecompute = false;
            }
            break;
        }
        default:
            break;
        }
    }
}


void MPlotCollection::render(MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);

    // Creating textures might have changed the OpenGL context thus switch back.
    if (sceneView)
    {
        sceneView->makeCurrent();
    }

    // Render requested plot only if needed components are available, i.e.
    // scalar fields to compute plots from (gridAggregation) and isovalue[s] to
    // compute plot for.
    if (var->gridAggregation != nullptr
            && !var->contourSetList.at(contourSetIndex).levels.empty())
    {
        // Only render the plot currently selected and only if possible.
        switch (var->renderSettings.renderMode)
        {
        case MNWP2DHorizontalActorVariable::RenderMode::SpaghettiPlot:
            renderSpaghettiPlot(sceneView);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::ContourBoxplot:
            // Contour boxplot needs at least three ensemble members to be
            // computed from.
            if (numMembers >= 3)
            {
                renderContourBoxplotMedianLine(sceneView);
                renderContourBoxplotOutliers(sceneView);
                renderContourBoxplots(sceneView);
            }
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::ContourProbabilityPlot:
            renderContourProbabilityPlotMedianLine(sceneView);
            renderContourProbabilityPlots(sceneView);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::DistanceVariabilityPlot:
            renderMultiIsoVariabilityPlotMean(sceneView);
            renderMultiIsoVariabilityPlot(sceneView);
            break;

        case MNWP2DHorizontalActorVariable::RenderMode::ScalarVariabilityPlot:
            renderVariabilityPlotMean(sceneView);
            renderVariabilityPlot(sceneView);
            break;

        default:
            break;
        }
    }
}


void MPlotCollection::setShaders()
{
    glMarchingSquaresShader  = actor->getGlMarchingSquaresShader();
    glContourPlotsShader     = actor->getGlContourPlotsShader();
    glCSContourPlotsShader   = actor->getGlCSContourPlotsShader();
    glVariabilityPlotsShader = actor->getGlVariabilityPlotsShader();
}


void MPlotCollection::updateContourList(int index)
{
    MQtProperties *properties = var->actor->getQtProperties();
    int contourSetIndex = properties->mEnum()->value(contourSetProperty);
    // Contour set was deleted.
    if (index >= 0)
    {
        suppresssUpdate = true;
        properties->mEnum()->setEnumNames(contourSetProperty,
                                          *var->getContourSetStringList());
        suppresssUpdate = false;
        // Switch to index 0 if contour set selected was deleted.
        if (index == contourSetIndex)
        {
            // Don't suppress the update since we switch the contour set.
            contourSetIndex = 0;
        }
        // If the index of the contour set deleted is smaller than the index of
        // the currently selected contour set, we need to decrease
        // contourSetIndex to match the new index of the contour set selected.
        else if (index < contourSetIndex)
        {
            // Suppress the update since the contour set does not change.
            suppresssUpdate = true;
            contourSetIndex--;
        }
        // If the index of the contour set deleted is greater than the index of
        // the currently selected contour set, the index of the contour set
        // selected is not affected by the deletion hence we don't need to adapt
        // contourSetIndex.
        else
        {
            // Suppress the update since the contour set does not change.
            suppresssUpdate = true;
        }
        // Set the index of the enum property to contourSetIndex since changing
        // enum names of a enum property resets the current index to 0 and
        // deleting a contour set from the list also might change the list index
        // of the currently selected contour set.
        properties->mEnum()->setValue(contourSetProperty, contourSetIndex);
        suppresssUpdate = false;
    }
    // Contour set was added.
    else
    {
        suppresssUpdate = true;
        properties->mEnum()->setEnumNames(contourSetProperty,
                                          *var->getContourSetStringList());
        // Set the index of the enum property to contourSetIndex since changing
        // enum names of a enum property resets the current index to 0.
        properties->mEnum()->setValue(contourSetProperty, contourSetIndex);
        suppresssUpdate = false;
    }
}


/******************************************************************************
***                            PRIVATE METHODS                              ***
*******************************************************************************/

void MPlotCollection::textureDelete(GL::MTexture *texture)
{
    if (texture != nullptr)
    {
        delete texture;
        texture = nullptr;
    }
}


int MPlotCollection::getTextureIndex(int isovalue, int numMembers, int member)
{
    return isovalue * numMembers + member;
}


void MPlotCollection::createGeneralTextures()
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    if (var->gridAggregation == nullptr)
    {
        return;
    }

    // Create texture to draw (median/mean) line of contour probability or
    // variability plot.
    if (!textureLineDrawing)
    {
        MStructuredGrid *grid = var->gridAggregation->getGrids().at(0);
        QString textureID = QString("linedraw%1").arg(
                    var->targetGrid2D->getID());
        textureLineDrawing =
                new GL::MTexture(textureID, GL_TEXTURE_2D, GL_R32F,
                                 grid->nlons, grid->nlats);

        // Create texture for drawing lines.
        if (!glRM->tryStoreGPUItem(textureLineDrawing))
        {
            textureDelete(textureLineDrawing);
            textureLineDrawing = nullptr;
        }

        if (textureLineDrawing)
        {
            textureLineDrawing->updateSize(grid->nlons, grid->nlats);

            glRM->makeCurrent();
            textureLineDrawing->bindToLastTextureUnit();

            // Set texture parameters: wrap mode and filtering.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            // Upload data array to GPU.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, grid->nlons,
                         grid->nlats, 0, GL_RED, GL_FLOAT,
                         NULL); CHECK_GL_ERROR;
        }
    }
}


void MPlotCollection::createContourBoxplotTextures()
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    if (var->gridAggregation == nullptr)
    {
        return;
    }

    MStructuredGrid *grid = var->gridAggregation->getGrids().at(0);

    // Create render texture of contour boxplot.
    if (textureContourBoxplot == nullptr)
    {
        QString textureID = QString("cboxplot%1").arg(
                    var->targetGrid2D->getID());
        textureContourBoxplot =
                new GL::MTexture(textureID, GL_TEXTURE_2D, GL_RGBA32F,
                                 grid->getNumLons(), grid->getNumLats());
        if (!glRM->tryStoreGPUItem(textureContourBoxplot))
        {
            textureDelete(textureContourBoxplot);
        }
        if (textureContourBoxplot)
        {
            textureContourBoxplot->updateSize(grid->getNumLons(),
                                              grid->getNumLats());
            glRM->makeCurrent();
            textureContourBoxplot->bindToLastTextureUnit();

            // Set texture parameters: wrap mode and filtering.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            // Upload data array to GPU.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, grid->getNumLons(),
                         grid->getNumLats(), 0, GL_RG, GL_FLOAT,
                         NULL); CHECK_GL_ERROR;
        }
    }

    // Create texture for computing and storing band depth values for
    // contour boxplot computation.
    if (textureCBPBandDepth == nullptr)
    {
        QString textureID_bd =
                QString("cboxplotbd%1").arg(var->targetGrid2D->getID());
        textureCBPBandDepth =
                new GL::MTexture(textureID_bd, GL_TEXTURE_1D, GL_R32F,
                                 MAX_NUM_MEMBERS);
        if (!glRM->tryStoreGPUItem(textureCBPBandDepth))
        {
            textureDelete(textureCBPBandDepth);
        }
        if (textureCBPBandDepth)
        {
            textureCBPBandDepth->updateSize(MAX_NUM_MEMBERS);
            // Initialise texture with 0 values.
            float* data = new float[MAX_NUM_MEMBERS];
            for (int i = 0; i < MAX_NUM_MEMBERS; i++)
            {
                data[i] = 0.0f;
            }

            glRM->makeCurrent();
            textureCBPBandDepth->bindToLastTextureUnit();

            // Set texture parameters: wrap mode and filtering.
            glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            // Upload data array to GPU.
            glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, MAX_NUM_MEMBERS, 0, GL_RED,
                         GL_FLOAT, data); CHECK_GL_ERROR;
        }
    }

    // Create texture storing matrix used to compute a default epsilon for
    // contour boxplot.
    if (textureCBPEpsilonMatrix == nullptr)
    {
        QString textureID_em =
                QString("cboxplotem%1").arg(var->targetGrid2D->getID());
        textureCBPEpsilonMatrix =
                new GL::MTexture(textureID_em, GL_TEXTURE_2D, GL_R32F,
                                 MAX_NUM_PAIRS, MAX_NUM_MEMBERS);
        if (!glRM->tryStoreGPUItem(textureCBPEpsilonMatrix))
        {
            textureDelete(textureCBPEpsilonMatrix);
        }
        if (textureCBPEpsilonMatrix)
        {
            textureCBPEpsilonMatrix->updateSize(MAX_NUM_PAIRS, MAX_NUM_MEMBERS);

            glRM->makeCurrent();
            textureCBPEpsilonMatrix->bindToLastTextureUnit();

            // Set texture parameters: wrap mode and filtering.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);


            // Upload data array to GPU.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, MAX_NUM_PAIRS,
                         MAX_NUM_MEMBERS, 0, GL_RED, GL_FLOAT,
                         NULL); CHECK_GL_ERROR;
        }
    }
    // Create textures storing binary maps of the ensemble member scalar fields
    // used to compute contour boxplot.
    createBinaryMapTextures(numMembers);
}


void MPlotCollection::createBinaryMapTextures(int amountOfTexturesNeeded)
{
    // It makes no sense to create textures if no ensemble scalar fields are
    // available.
    if (var->gridAggregation == nullptr)
    {
        return;
    }

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    int currentSize= binaryMapTextureContainer.size();

    if (amountOfTexturesNeeded <= currentSize)
    {
        // We have more than enough textures.
        return;
    }

    binaryMapTextureContainer.resize(amountOfTexturesNeeded);

    MStructuredGrid *grid = var->gridAggregation->getGrids().at(0);

    // Only create textures not created yet.
    for (int i = currentSize; i < amountOfTexturesNeeded; i++)
    {
        QString textureID = QString("cbp-binarymap%1-%2").arg(
                    var->targetGrid2D->getID(), QString::number (i));

        binaryMapTextureContainer[i] =
                new GL::MTexture(textureID, GL_TEXTURE_2D, GL_R8,
                                 grid->nlons, grid->nlats);
        if (!glRM->tryStoreGPUItem(binaryMapTextureContainer[i]))
        {
            delete binaryMapTextureContainer[i];
            binaryMapTextureContainer[i] = nullptr;
        }
        if (binaryMapTextureContainer[i] != nullptr)
        {
            binaryMapTextureContainer[i]->updateSize(grid->nlons, grid->nlats);

            glRM->makeCurrent();
            binaryMapTextureContainer[i]->bindToLastTextureUnit();

            // Set texture parameters: wrap mode and filtering.
            // Use repeat wrap mode for right access when computing band
            // depth and default epsilon for shifted regions.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

            // Upload data array to GPU.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, grid->nlons, grid->nlats,
                         0, GL_RED, GL_BYTE, NULL); CHECK_GL_ERROR;
        }
    }
}


void MPlotCollection::createContourProbabilityPlotTextures()
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    createGeneralTextures();

    // It makes no sense to create textures if no ensemble scalar fields are
    // available.
    if (var->gridAggregation == nullptr)
    {
        return;
    }

    MStructuredGrid *grid = var->gridAggregation->getGrids().at(0);
    unsigned int nlons = grid->getNumLons();
    unsigned int nlats = grid->getNumLats();

    // Create render texture of contour probability plot.
    if (textureContourProbabilityPlot == nullptr)
    {
        QString textureID_probability =
                QString("cprobplot%1").arg(var->targetGrid2D->getID());
        textureContourProbabilityPlot =
                new GL::MTexture(textureID_probability, GL_TEXTURE_2D, GL_RGBA32F,
                                 nlons, nlats * 2);
        if (!glRM->tryStoreGPUItem(textureContourProbabilityPlot))
        {
            textureDelete(textureContourProbabilityPlot);
        }
        if (textureContourProbabilityPlot)
        {
            textureContourProbabilityPlot->updateSize(nlons, nlats * 2);

            glRM->makeCurrent();
            textureContourProbabilityPlot->bindToLastTextureUnit();

            // Set texture parameters: wrap mode and filtering.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            // Upload data array to GPU.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nlons, nlats * 2, 0,
                         GL_RG, GL_FLOAT, NULL); CHECK_GL_ERROR;
        }
    }

    if (contourPlot->probabilityMedian == nullptr)
    {
        contourPlot->probabilityMedian = new float[nlons * nlats];
    }
}


void MPlotCollection::createScalarVariabilityPlotTexturesAndArrays()
{
    createGeneralTextures();

    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    // It makes no sense to create textures if no ensemble scalar fields are
    // available.
    if (var->gridAggregation == nullptr)
    {
        return;
    }

    MStructuredGrid *grid = var->gridAggregation->getGrids().at(0);
    unsigned int nlons = grid->getNumLons();
    unsigned int nlats = grid->getNumLats();

    // Create render texture of scalar variability plot.
    if (textureScalarVariabilityPlot == nullptr)
    {
        QString textureIDScalar =
                QString("scalarvariabilityplot%1").arg(var->targetGrid2D->getID());
        textureScalarVariabilityPlot =
                new GL::MTexture(textureIDScalar, GL_TEXTURE_2D, GL_RG32F,
                                 nlons, nlats);
        if (!glRM->tryStoreGPUItem(textureScalarVariabilityPlot))
        {
            textureDelete(textureScalarVariabilityPlot);
        }
        if (textureScalarVariabilityPlot)
        {
            textureScalarVariabilityPlot->updateSize(nlons, nlats);

            glRM->makeCurrent();
            textureScalarVariabilityPlot->bindToLastTextureUnit();

            // Set texture parameters: wrap mode and filtering.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            // Upload data array to GPU.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, nlons, nlats, 0, GL_RG,
                         GL_FLOAT, NULL); CHECK_GL_ERROR;
        }
    }

    // Create array storing scalar variability plot mean values.
    if (variabilityPlot->scalarMean == nullptr)
    {
        variabilityPlot->scalarMean = new float[nlons * nlats];
    }
}


void MPlotCollection::createDistanceVariabilityPlotTexturesAndArrays()
{
    createGeneralTextures();

    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    createGeneralTextures();

    // It makes no sense to create textures if no ensemble scalar fields are
    // available.
    if (var->gridAggregation == nullptr)
    {
        return;
    }

    MStructuredGrid *grid = var->gridAggregation->getGrids().at(0);
    unsigned int nlons = grid->getNumLons();
    unsigned int nlats = grid->getNumLats();
    unsigned int texturesize = nlons * nlats;

    int currentSize = distanceTextureContainer.size();

    int amountNeeded = var->contourSetList.at(contourSetIndex).levels.size();

    if (amountNeeded > currentSize)
    {
        distanceTextureContainer.resize(amountNeeded);

        // Only create textures not created yet.
        for (int i = currentSize; i < amountNeeded; i++)
        {
            // Create render texture of distance variability plot.
            QString textureID = QString("distancevariabilityplot%1-%2").arg(
                        var->targetGrid2D->getID(), QString::number(i));

            distanceTextureContainer[i] =
                    new GL::MTexture(textureID, GL_TEXTURE_2D,
                                     GL_RG32F, nlons, nlats); CHECK_GL_ERROR;
            if (!glRM->tryStoreGPUItem(distanceTextureContainer[i]))
            {
                delete distanceTextureContainer[i];
                distanceTextureContainer[i] = nullptr;
            }
            if (distanceTextureContainer[i] != nullptr)
            {
                distanceTextureContainer[i]->updateSize(nlons, nlats);

                glRM->makeCurrent();
                distanceTextureContainer[i]->bindToLastTextureUnit();

                // Set texture parameters: wrap mode and filtering.
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

                // Upload data array to GPU.
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, nlons, nlats, 0, GL_RG,
                             GL_FLOAT, NULL); CHECK_GL_ERROR;
            }
        }

        // Create arrays storing mean fields of distance variability plot.
        variabilityPlot->distanceMean.resize(amountNeeded);
        for (int i = 0; i < variabilityPlot->distanceMean.size(); i++)
        {
            variabilityPlot->distanceMean[i] = new float[texturesize];
        }
    }

    amountNeeded = numMembers * amountNeeded;
    currentSize = variabilityPlot->distanceStorage.size();

    // Create arrays storing distance fields of distance variability plot.
    if (amountNeeded > currentSize)
    {
        variabilityPlot->distanceStorage.resize(amountNeeded);
        for (int i = currentSize; i < variabilityPlot->distanceStorage.size();
             i++)
        {
            variabilityPlot->distanceStorage[i] = new float[texturesize];
        }
    }
}


void MPlotCollection::residentBindlessMembers(
        std::shared_ptr<GL::MShaderEffect> shader)
{
    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();

    GL::MTexture* tex = nullptr;
    GLuint textureID = 0;
    GLuint64 textureHandle = 0;

    // Delete old texture handles if present.
    if (gridTextureHandles != nullptr)
    {
        delete gridTextureHandles;
    }
    gridTextureHandles = new GLuint64[numMembers];

    for (int i = 0; i < numMembers; i++)
    {
        tex = grids[i]->get2DFieldTexture();
        // Print error message if failed to get 2D field texture.
        if (tex == NULL)
        {
            switch (i - (i / 10) * 10)
            {
            case 1: LOG4CPLUS_ERROR(mlog, "ERROR: Failed to get "
                                    << i << "st 2D field texture, grids["
                                    << i << "]->get2DFieldTexture returned "
                                            "NULL."); break;
            case 2: LOG4CPLUS_ERROR(mlog, "ERROR: Failed to get "
                                    << i << "nd 2D field texture, grids["
                                    << i << "]->get2DFieldTexture returned "
                                            "NULL."); break;
            case 3: LOG4CPLUS_ERROR(mlog, "ERROR: Failed to get "
                                    << i << "rd 2D field texture, grids["
                                    << i << "]->get2DFieldTexture returned "
                                            "NULL."); break;
            default: LOG4CPLUS_ERROR(mlog, "ERROR: Failed to get "
                                     << i << "th 2D field texture, grids["
                                     << i << "]->get2DFieldTexture returned "
                                             "NULL."); break;
            }
        }

        textureID = tex->getTextureObject(); CHECK_GL_ERROR;

        textureHandle = glGetTextureHandleARB(textureID); CHECK_GL_ERROR;

        if (!glIsTextureHandleResidentARB(textureHandle))
        {
            // Do not call this multiple times.
            glMakeTextureHandleResidentARB(textureHandle); CHECK_GL_ERROR;
        }
        gridTextureHandles[i] = (textureHandle);
    }

    shader->setUniformValueArray("gridTextureHandles", &gridTextureHandles[0],
            numMembers); CHECK_GL_ERROR;
}


void MPlotCollection::nonResidentBindlessMembers()
{
    GLuint64 textureHandle = 0;

    for (int i = 0; i < numMembers; i++)
    {
        textureHandle = gridTextureHandles[i];

        if (glIsTextureHandleResidentARB(textureHandle))
        {
            // Do not call this multiple times.
            glMakeTextureHandleNonResidentARB(textureHandle); CHECK_GL_ERROR;
        }
    }

    // Delete texture handles not needed any longer if present.
    if (gridTextureHandles != nullptr)
    {
        delete gridTextureHandles;
        gridTextureHandles = nullptr;
    }
}


void MPlotCollection::residentBindlessTextures(
        std::shared_ptr<GL::MShaderEffect> shader,
        QVector<GL::MTexture*> *textureContainer)
{
    int numTextures = textureContainer->size();

    GL::MTexture* tex = nullptr;
    GLuint textureID = 0;
    GLuint64 textureHandle = 0;

    // Delete old texture handles if present.
    if (textureHandles != nullptr)
    {
        delete textureHandles;
    }
    textureHandles = new GLuint64[numTextures];

    for (int i = 0; i < numTextures; i++)
    {
        tex = textureContainer->at(i);
        // Print error message if failed to get texture.
        if (tex == NULL)
        {
            switch (i - (i / 10) * 10)
            {
            case 1:
                LOG4CPLUS_ERROR(mlog, "ERROR: Failed to get "
                                << i << "st texture in container.");
                break;
            case 2:
                LOG4CPLUS_ERROR(mlog, "ERROR: Failed to get "
                                << i << "nd texture in container.");
                break;
            case 3:
                LOG4CPLUS_ERROR(mlog, "ERROR: Failed to get "
                                << i << "rd texture in container.");
                break;
            default:
                LOG4CPLUS_ERROR(mlog, "ERROR: Failed to get "
                                << i << "th texture in container.");
                break;
            }
        }

        textureID = tex->getTextureObject(); CHECK_GL_ERROR;

        textureHandle = glGetTextureHandleARB(textureID); CHECK_GL_ERROR;

        if (!glIsTextureHandleResidentARB(textureHandle))
        {
            // Do not call this multiple times.
            glMakeTextureHandleResidentARB(textureHandle); CHECK_GL_ERROR;
        }
        textureHandles[i] = (textureHandle);
    }

    shader->setUniformValueArray("textureHandles", &textureHandles[0],
            numTextures); CHECK_GL_ERROR;
}


void MPlotCollection::nonResidentBindlessTextures(
        QVector<GL::MTexture*> *textureContainer)
{
    int numTextures = textureContainer->size();

    GLuint64 textureHandle = 0;

    for (int i = 0; i < numTextures; i++)
    {
        textureHandle = textureHandles[i];

        if (glIsTextureHandleResidentARB(textureHandle))
        {
            // Do not call this multiple times.
            glMakeTextureHandleNonResidentARB(textureHandle); CHECK_GL_ERROR;
        }
    }

    // Delete texture handles not needed any longer if present.
    if (textureHandles != nullptr)
    {
        delete textureHandles;
        textureHandles = nullptr;
    }
}


void MPlotCollection::computeContourBoxplotsBinaryMap()
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();

    glCSContourPlotsShader->bindProgram("ComputeBinaryMap");

    // Use bindless textures to access ensemble scalar field textures and binary
    // map textures.
    residentBindlessMembers(glCSContourPlotsShader);
    residentBindlessTextures(glCSContourPlotsShader,
                             &binaryMapTextureContainer);

    // Set shader variables.
    // =====================

    // Grid offsets to render only the requested subregion.
    glCSContourPlotsShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glCSContourPlotsShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;

    // Set Isovalue.
    glCSContourPlotsShader->setUniformValue(
                "isoValue", GLfloat(
                    var->contourSetList.at(contourSetIndex).levels.at(0)));

    // We don't want to compute entries twice thus we need to use at most
    // the width of the grid (var->nlons can be larger than the grid size).
    glDispatchCompute(min(var->nlons,
                          int(grids.at(0)->getNumLons() + 1)) * 2,
                      var->nlats - 1, numMembers);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    // Tell OpenGL we don't need the bindless textures any more at the moment.
    nonResidentBindlessMembers();
    nonResidentBindlessTextures(&binaryMapTextureContainer);
}


void MPlotCollection::computeContourBoxplotDefaultEpsilon()
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();

    int nlons = min(var->nlons, static_cast<int>(grids.at(0)->getNumLons()));

    int bBoxXBorder = static_cast<int>(var->i0) + nlons;
    int bBoxYBorder = static_cast<int>(var->j0) + var->nlats;

    int matsize = MAX_EPSILON_MATRIX_SIZE;

    // Begin: Compute Matrix.
    // ======================
    glCSContourPlotsShader->bindProgram("ComputeDefaultEpsilon");

    // Use bindless textures to access binary map textures.
    residentBindlessTextures(glCSContourPlotsShader,
                             &binaryMapTextureContainer);

    // Set shader variables.
    // =====================

    // Set nMembers.
    glCSContourPlotsShader->setUniformValue(
                "nMembers", GLint(numMembers)); CHECK_GL_ERROR;
    // Set nlons.
    glCSContourPlotsShader->setUniformValue(
                "nlons", GLint(nlons)); CHECK_GL_ERROR;
    // Set nlats.
    glCSContourPlotsShader->setUniformValue(
                "nlats", GLint(var->nlats)); CHECK_GL_ERROR;
    // Set offsets.
    glCSContourPlotsShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glCSContourPlotsShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;

    // Set borders of chosen area filled with scalar values.
    glCSContourPlotsShader->setUniformValue(
                "xBorder", GLint(bBoxXBorder)); CHECK_GL_ERROR;
    glCSContourPlotsShader->setUniformValue(
                "yBorder", GLint(bBoxYBorder)); CHECK_GL_ERROR;

    glCSContourPlotsShader->setUniformValue(
                "epsilonMatrix", GLint(var->imageUnitTargetGrid)); CHECK_GL_ERROR;
    glBindImageTexture(var->imageUnitTargetGrid, // image unit
                       textureCBPEpsilonMatrix->getTextureObject(),
                                                 // texture object
                       0,                        // level
                       GL_FALSE,                 // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       GL_R32F); CHECK_GL_ERROR; // format

    glDispatchCompute(numMembers, numMembers - 2, numMembers - 2);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);


    float *matrix = new float[matsize];
    textureCBPEpsilonMatrix->bindToTextureUnit(var->imageUnitTargetGrid);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, matrix); CHECK_GL_ERROR;
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

    // Tell OpenGL we don't need the bindless textures any more at the moment.
    nonResidentBindlessTextures(&binaryMapTextureContainer);
    // End: Compute Matrix.
    // ====================

    // nPairs: number of member pairs one member needs to be tested against.
    // formula: from n choose r; n = #members - 1 (member pairs are chosen from
    //          set without member to test against), r = 2 .
    int nPairs = ((numMembers - 1) * (numMembers - 2)) / 2;

    // Begin: Search for "default" epsilon.
    // ====================================
    float epsilon = 0.5f;
    float lastEpsilon = 0.f;
    float lowerBound = 0.f;
    float upperBound = 1.f;
    float sum = 0.f;
    // Maximum number of tries to find default epsilon used to avoid endless
    // loop (rather unlikely to occur but just in case).
    int maxNumIterations = 100000;
    int numIterations = 0;

    // Size of the matrix.
    float msize = static_cast<float>(nPairs * numMembers);

    // Initial test for border cases: All entries of the matrix are equal to 0
    // or all entries are equal to 1.
    for (int i = 0; i < numMembers; i++)
    {
        for (int j = 0; j < nPairs; j++)
        {
            sum += matrix[i * MAX_NUM_PAIRS + j];
        }
    }
    sum /= msize;

    // No band lies between two others.
    if (sum == 1.f)
    {
        epsilon = 1.f;
    }
    // All contours are the same.
    else if (sum == 0.f)
    {
        epsilon = 0.f;
    }
    // One or more contours lie between others.
    else
    {
        float oneSixth = 1.f / 6.f;
        // Binary search of the "optimal" epsilon.
        // Early break if epsilon does not change any more.
        while ((sum != oneSixth) && (epsilon != lastEpsilon)
               && (numIterations < maxNumIterations))
        {
            sum = 0.f; // Reset sum.
            lastEpsilon = epsilon;
            // We need two for-loops since the compute shader treats the matrix
            // as a 2D texture of size maximum needed
            // (maxNumMembers * maxNumPairs) and therefore for smaller matrices
            // not all entries of a row are filled with meaningful values.
            for (int i = 0; i < numMembers; i++)
            {
                for (int j = 0; j < nPairs; j++)
                {
                    if (matrix[i * MAX_NUM_PAIRS + j] < epsilon)
                    {
                        sum++;
                    }
                }
            }
            // Normalize sum.
            sum /= msize;
            if (sum > oneSixth)
            {
                upperBound = epsilon;
                epsilon = (lowerBound + epsilon) / 2.f;
            }
            else if (sum < oneSixth)
            {
                lowerBound = epsilon;
                epsilon = (upperBound + epsilon) / 2.f;
            }
            else
            {
                break;
            }
            numIterations++;
        }
        if (numIterations == maxNumIterations)
        {
            LOG4CPLUS_WARN(mlog,
                           (QString(
                               "WARNING: Default epsilon could not be computed"
                               " after %1 iterations. Stopped search to avoid"
                               " endless loop.").arg(maxNumIterations))
                           .toStdString());
        }
    }

    delete matrix;
    // End: Search for default epsilon.
    // ================================

    // Store value of default epsilon found.
    contourPlot->defaultEpsilon = static_cast<double>(epsilon);
    // Change epsilon if user has chosen to use the default epsilon.
    if (contourPlot->useDefaultEpsilon)
    {
        actor->getQtProperties()->mDouble()->setValue(
                    contourPlot->epsilonProperty,
                    contourPlot->defaultEpsilon);
    }
}


void MPlotCollection::computeContourBoxplotBandDepth()
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();

    int nlons = min(var->nlons, static_cast<int>(grids.at(0)->getNumLons()));

    unsigned int bBoxXBorder = var->i0 + static_cast<uint>(nlons) + 1;
    unsigned int bBoxYBorder = var->j0 + static_cast<uint>(var->nlats) + 1;

    // Begin: Compute band depth array on GPU.
    // =======================================

    glCSContourPlotsShader->bindProgram("ComputeBandDepth");

    // Use bindless textures to access binary map textures.
    residentBindlessTextures(glCSContourPlotsShader,
                             &binaryMapTextureContainer);

    // Set shader variables.
    // =====================

    // nPairs: number of contour pairs one contour needs to be tested against.
    // formula: from n choose r; n = #members - 1 (contour pairs are chosen from
    //          set without contour to test against), r = 2 .
    float nPairs = static_cast<float>(((numMembers - 1) * (numMembers - 2)) / 2);
    // Set nMembers.
    glCSContourPlotsShader->setUniformValue(
                "nMembers", GLint(numMembers)); CHECK_GL_ERROR;
    // Set nlons.
    glCSContourPlotsShader->setUniformValue(
                "nlons", GLint(nlons)); CHECK_GL_ERROR;
    // Set nlats.
    glCSContourPlotsShader->setUniformValue(
                "nlats", GLint(var->nlats)); CHECK_GL_ERROR;
    // Set epsilon.
    glCSContourPlotsShader->setUniformValue(
                "epsilon", GLfloat(contourPlot->epsilon
                                   * double(nlons * var->nlats))); CHECK_GL_ERROR;
    // Set nPairs.
    glCSContourPlotsShader->setUniformValue("numPairsFloat",
                                            GLfloat(nPairs)); CHECK_GL_ERROR;
    // Set offsets.
    glCSContourPlotsShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glCSContourPlotsShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;

    // Set borders of chosen area filled with scalar values.
    glCSContourPlotsShader->setUniformValue(
                "xBorder", GLint(bBoxXBorder)); CHECK_GL_ERROR;
    glCSContourPlotsShader->setUniformValue(
                "yBorder", GLint(bBoxYBorder)); CHECK_GL_ERROR;

    glCSContourPlotsShader->setUniformValue(
                "bandDepth", GLint(var->imageUnitTargetGrid)); CHECK_GL_ERROR;
    glBindImageTexture(var->imageUnitTargetGrid, // image unit
                       textureCBPBandDepth->getTextureObject(),
                                                 // texture object
                       0,                        // level
                       GL_FALSE,                 // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       GL_R32F); CHECK_GL_ERROR; // format

    // Start one work group per member.
    glDispatchCompute(1, 1, numMembers);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

    // Tell OpenGL we don't need the bindless textures any more at the moment.
    nonResidentBindlessTextures(&binaryMapTextureContainer);

    // End: Compute band depth array on GPU.
    // =====================================

    // Sort band depth array on CPU.
    // =============================

    textureCBPBandDepth->bindToTextureUnit(GLuint(var->imageUnitTargetGrid));

    float *temp = contourPlot->bandDepth;

    glGetTexImage(GL_TEXTURE_1D, 0, GL_RED, GL_FLOAT, temp); CHECK_GL_ERROR;
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

    // Sort band depth array from greatest to smallest band depth value.
    std::stable_sort(temp, temp + numMembers, [](float a, float b)
    {
        return (b - (floor(b / 10) * 10)) < (a - (floor(a / 10) * 10));
    });

    // Re-upload the combined and sorted band depth array.
    textureCBPBandDepth->bindToLastTextureUnit();
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, GLsizei(numMembers), GL_RED,
                    GL_FLOAT, temp); CHECK_GL_ERROR;
}


void MPlotCollection::computeContourBoxplot()
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    // Compute band depth array for the contour boxplot computation.
    computeContourBoxplotBandDepth();

    // Get the sorted band depth array (One entry combines band depth value and
    // member-id: member-id * 10 + bandDepthValue).
    float *bandDepth = contourPlot->bandDepth;

    // Number of lines building the 50% band/inner band.
    int numInnerMembers = ceil(numMembers / 2.0);

    // isoValueTimes2: Used for calculating "mirrored" min: min_new = -min_old
    // + isoValue * 2 . (cf. plotcollection.h)
    float  isoValueTimes2 =
            2.0f * (var->contourSetList.at(contourSetIndex).levels.at(0));

    // Number of members with band depth value NOT equal to zero (i.e. members
    // forming the bands).
    int nonZeroMembers = 0;
    // Count number of members with band depth value unequal to zero.
    // bandDepth array stores band depth value and related member:
    // (member * 10 + bandDepth) thus extracting the band depth value is
    // necessary.
    while (nonZeroMembers < numMembers
           && ((bandDepth[nonZeroMembers]
                - floor(bandDepth[nonZeroMembers] / 10) * 10) != 0.0f))
    {
        nonZeroMembers++;
    }

    // Only compute contour boxplot band if it exists (<=> two or more non zero
    // band depth values exist).
    if (nonZeroMembers > 1)
    {
        numInnerMembers = ceil((float)nonZeroMembers / 2.0);

        glCSContourPlotsShader->bindProgram("ComputeContourBoxplot");

        // Use bindless textures to access ensemble scalar field textures.
        residentBindlessMembers(glCSContourPlotsShader);

        // Set shader variables.
        // =====================

        // Set numInnerMembers.
        glCSContourPlotsShader->setUniformValue(
                    "numInnerMembers", GLint(numInnerMembers)); CHECK_GL_ERROR;
        // Set nonZeroMembers.
        glCSContourPlotsShader->setUniformValue
                ("nonZeroMembers", GLint(nonZeroMembers)); CHECK_GL_ERROR;
        // Set isoValueTimes2.
        glCSContourPlotsShader->setUniformValue(
                    "isoValueTimes2", GLfloat(isoValueTimes2)); CHECK_GL_ERROR;

        // Grid offsets to render only the requested subregion.
        glCSContourPlotsShader->setUniformValue(
                    "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
        glCSContourPlotsShader->setUniformValue(
                    "jOffset", GLint(var->j0)); CHECK_GL_ERROR;

        // Texture bindings for band depth array (1D texture).
        textureCBPBandDepth->bindToTextureUnit(var->textureUnitDataField);
        glCSContourPlotsShader->setUniformValue("bandDepthSampler",
                                                   var->textureUnitDataField);

        glCSContourPlotsShader->setUniformValue(
                    "contourBoxplotTexture",
                    GLint(var->imageUnitTargetGrid)); CHECK_GL_ERROR;
        glBindImageTexture(var->imageUnitTargetGrid, // image unit
                           textureContourBoxplot->getTextureObject(),
                                                     // texture object
                           0,                        // level
                           GL_FALSE,                 // layered
                           0,                        // layer
                           GL_READ_WRITE,            // shader access
                           GL_RGBA32F); CHECK_GL_ERROR; // format

        // We don't want to compute entries twice thus we need to use at most
        // the width of the grid. (var->nlons can be larger than the grid size)
        glDispatchCompute(
                    min(static_cast<unsigned int>(var->nlons),
                        var->gridAggregation->getGrids().at(0)->getNumLons()),
                    var->nlats, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Tell OpenGL we don't need the bindless textures any more at the
        // moment.
        nonResidentBindlessMembers();
    }
}


void MPlotCollection::computeContourProbabilityPlot()
{
    createContourProbabilityPlotTextures();

    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);

    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();

    // Variables storing sizes of the grid.
    unsigned int nlons = grids.at(0)->getNumLons();
    unsigned int nlats = grids.at(0)->getNumLats();
    unsigned int textureSize = nlons * nlats;
    unsigned int textureSize4 = 4 * nlons * nlats;

    // We do not want to compute parts of the contour probability plot twice for
    // repeated regions but only for regions which are inside the bounding box
    // therefore we use the minimum of the dimensions of the bounding box and
    // the dimensions of the grid.
    unsigned int nlonsRegion = min(nlons, static_cast<unsigned int>(var->nlons));
    unsigned int regionSize = nlonsRegion
            * min(nlats, static_cast<unsigned int>(var->nlats));

    // Create array which is large enough to handle maximum number of threads.
    float* storage = new float[numMembers * omp_get_max_threads()];

    float innerPercentage = 0.5f - (float)((contourPlot->innerPercentage / 2.)
                                          / 100.);
    float outerPercentage = (float)(contourPlot->outerPercentage / 100.);
    
    // Precompute interpolation factors and indices of interpolation boundaries
    // for inner and outer band boundaries.
    // =========================================================================
    // Since we search for bands representing the demanded percentage boundaries
    // as close as possible we need to interpolate between values of two members
    // if the percentage boundary does not coincide with a member. (Since the 
    // plot is computed locally for each grid point separately, the concrete
    // member might vary from grid point to grid point.)
    // To interpolate the value we need the interpolation factor and the 
    // neighbouring member indices (lower/smaller and upper/greater neighbour).
    // Since the method is applied locally the indices represent the position of
    // the values needed stored in a sorted array.

    // Variables needed to calculate interpolated values to render the innermost
    // band from.
    // =========================================================================
    // Interpolation factor of the upper boundary of the inner band.
    float innerUpperFactor = (float)numMembers * (1.0f - innerPercentage)
            - floor((float)numMembers * (1.0f - innerPercentage));
    // Index of smaller neighbour of the upper boundary of the inner band.
    int indexUpperLow = numMembers - ceil(numMembers * innerPercentage) - 1;
    // Index of greater neighbour of the upper boundary of the inner band.
    int indexUpperUp  = numMembers - floor(numMembers * innerPercentage) - 1;

    // Interpolation factor of the lower boundary of the inner band.
    float innerLowerFactor = (float)numMembers * innerPercentage
            - floor((float)numMembers * innerPercentage);
    // Index of smaller neighbour of the lower boundary of the inner band.
    int indexLowerLow = floor(numMembers * innerPercentage);
    // Index of greater neighbour of the lower boundary of the inner band.
    int indexLowerUp  = ceil(numMembers * innerPercentage);
    // =========================================================================

    // Variables needed to calculate interpolated values to render the outer
    // band from.
    // =========================================================================
    // Interpolation factor of the upper boundary of the outer band.
    float outUpperFactor  = (float)numMembers * (1.0f - outerPercentage)
            - floor((float)numMembers * (1.0f - outerPercentage));
    // Index of smaller neighbour of the upper boundary of the outer band.
    int indexOutUpperLow = numMembers - ceil(numMembers * outerPercentage) - 1;
    // Index of greater neighbour of the upper boundary of the outer band.
    int indexOutUpperUp  = numMembers - floor(numMembers * outerPercentage) - 1;

    // Interpolation factor of the lower boundary of the outer band.
    float outLowerFactor  = (float)numMembers * outerPercentage
            - floor((float)numMembers * outerPercentage);
    // Index of smaller neighbour of the lower boundary of the outer band.
    int indexOutLowerLow = floor(numMembers * outerPercentage);
    // Index of greater neighbour of the lower boundary of the outer band.
    int indexOutLowerUp  = ceil(numMembers * outerPercentage);
    // =========================================================================

    // Index of greatest value in storage array. (Needed to compute values of 
    // outermost band.)
    int indexLast = numMembers - 1;

    // Indices to interpolate median values. (If the number of members is odd, 
    // only medianIndex is used and no interpolation is performed.)
    int medianIndex = numMembers / 2;
    int medianIndex1 = medianIndex - 1;

    // Variables needed to perform region check.
    unsigned int xBound = var->i0 + min(uint(var->nlons), nlons);
    unsigned int yBound = var->j0 + uint(var->nlats);
    unsigned int x = 0, y = 0;
    unsigned int xOffset = var->i0;
    unsigned int yOffset = var->j0;

    // Index variables used to iterate over the arrays separately.
    unsigned int index = 0; // render texture array
    unsigned int i; // scalar field

    // Stores index of first element in storage-array belonging to a thread.
    // (Each thread needs a own copy of this variable!)
    int threadStorageStartPos = 0;

    // isoValueTimes2: Used for calculating "mirrored" min: min_new = -min_old
    // + isoValue * 2 . (cf. plotcollection.h)
    float isoValueTimes2 =
            2.0f * ((float)var->contourSetList.at(contourSetIndex).levels.at(0));

    // textureSize * 4 * 2: 4 = number of channels, 2 = we store "outermost
    // values" in rg-channels after all "innermost and outer values",
    // therefore we need two time the 4-channel texture size.
    float* drawStorage = new float[textureSize * 4 * 2];

    // We define a function-variable to get the median value for both an odd and
    // an even number of ensemble members without using an if-clause in the 
    // for-loop, without unnecessary for-loop-code copies and without performing
    // an interpolation for odd number of members (since in this case a look-up
    // is sufficient enough).
    std::function<float (const int)> getMedianValue;
    // Even number of members means we need to interpolate the median value.
    if (numMembers % 2 == 0)
    {
        getMedianValue = [&storage, &medianIndex,
                &medianIndex1](const int threadID)
        {
            return 0.5f * (storage[medianIndex + threadID]
                    + storage[medianIndex1 + threadID]);
        };
    }
    // Odd number of members means we don't need to interpolate the median
    // value.
    else
    {
        getMedianValue = [&storage, &medianIndex](const int threadID)
        {
            return storage[medianIndex + threadID];
        };
    }

    // Lambda function to interpolate "lower bound"/"minimum" value of a band.
    std::function<float (float factor, int lowerIndex,
                         int upperIndex, int threadID)> interpolateMinimum =
            [&isoValueTimes2, &storage](float factor, int lowerIndex,
            int upperIndex, int threadID)
    {
        return (isoValueTimes2
                - ((1.0f - factor) * storage[lowerIndex + threadID]
                + factor * storage[upperIndex + threadID]));
    };

    // Lambda function to interpolate "upper bound"/"maximum" value of a band.
    std::function<float (float factor, int lowerIndex,
                         int upperIndex, int threadID)> interpolateMaximum =
            [&isoValueTimes2, &storage](float factor, int lowerIndex,
            int upperIndex, int threadID)
    {
        return ((1.0f - factor) * storage[lowerIndex + threadID]
                + factor * storage[upperIndex + threadID]);
    };

    // Fill contour probability plot render texture.
#pragma omp parallel for firstprivate(x, y, index, threadStorageStartPos)
    for (i = 0; i < regionSize; i++)
    {
        // Compute 2D x and y coordinates from 1D texture index i.
        y = uint(floor(i / nlonsRegion));
        x = uint(i - int(y * nlonsRegion));

        x += xOffset;
        y += yOffset;

        if ((x < xBound) && (y < yBound))
        {
            // Map x to its corresponding value in the range [0,nlons[ (domain 
            // of ensemble member scalar field as texture).
            x = MMOD(x, nlons);

            index = x + y * nlons;

            threadStorageStartPos = omp_get_thread_num() * numMembers;
            // Loop over all members and store their values in storage.
            for (int m = 0; m < numMembers; m++)
            {
                storage[m + threadStorageStartPos] =
                        gridDataStorage[cbpINDEXmi(m, index, textureSize)];
            }

            // Sort values in storage from smallest to greatest.
            std::stable_sort(storage + threadStorageStartPos,
                             storage + threadStorageStartPos + numMembers,
                             [](float a, float b)
            {
                return b > a;
            });

            // Get median value either by interpolation between the two
            // "innermost" values (for even number of members) or by a simple 
            // look-up of the "innermost" value (for odd number of members.)
            contourPlot->probabilityMedian[index] =
                    getMedianValue(threadStorageStartPos);

            // Increase the index of drawStorage by four as it contains four
            // channels of "texture size" to store min and max fields while i 
            // iterates over a field of one "texture size".
            index = 4 * index;
            // Get interpolated values for each band boundary (min, max;
            // innermost, middle, outermost band).
            // Innermost band min.
            drawStorage[index] = interpolateMinimum(
                        innerLowerFactor, indexLowerLow, indexLowerUp,
                        threadStorageStartPos);
            // Innermost band max.
            drawStorage[index + 1] = interpolateMaximum(
                        innerUpperFactor, indexUpperLow, indexUpperUp,
                        threadStorageStartPos);

            // Middle/outer band min.
            drawStorage[index + 2] = interpolateMinimum(
                        outLowerFactor, indexOutLowerLow, indexOutLowerUp,
                        threadStorageStartPos);
            // Middle/outer band max.
            drawStorage[index + 3] = interpolateMaximum(
                        outUpperFactor, indexOutUpperLow, indexOutUpperUp,
                        threadStorageStartPos);

            // Outermost band min.
            drawStorage[index + textureSize4] =
                    (float)(-storage[threadStorageStartPos] + isoValueTimes2);
            // Outermost band max.
            drawStorage[index + 1 + textureSize4] =
                    (float)storage[indexLast + threadStorageStartPos];

            // Padding not needed part of array with zeros.
            drawStorage[index + 2 + textureSize4] = 0.0f;
            drawStorage[index + 3 + textureSize4] = 0.0f;
        }
    }

    // Upload contour probability plot texture, store outermost values in
    // rg-channels "after" inner band min/max field.
    textureContourProbabilityPlot->bindToLastTextureUnit();

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, nlons, nlats * 2, GL_RGBA,
                    GL_FLOAT, drawStorage); CHECK_GL_ERROR;
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    delete drawStorage;
    delete storage;
}


void MPlotCollection::computeDistanceVariabilityPlot()
{
    createDistanceVariabilityPlotTexturesAndArrays();

    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);

    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();
    int nlons = grids.at(0)->getNumLons();
    int nlats = grids.at(0)->getNumLats();
    unsigned int textureSize = nlons * nlats;

    // We do not want to compute parts of the distance variability plot twice
    // for repeated regions but only for regions which are inside the bounding
    // box therefore we use the minimum of the dimensions of the bounding box
    // and the dimensions of the grid.
    int nlonsRegion = min(nlons, var->nlons);
    int nlatsRegion = min(nlats, var->nlats);
    unsigned int regionSize = static_cast<unsigned int>(
                nlonsRegion * nlatsRegion);
    unsigned int xOffsetInt = static_cast<int>(var->i0);
    unsigned int yOffsetInt = static_cast<int>(var->j0);


    // Recompute distance only if necessary.
    if (variabilityPlot->distanceNeedsRecompute)
    {
        int i = 0;
        MFastMarch::MIntVector2D resolution(nlons, nlats);
        MFastMarch::MIntVector2D offset(xOffsetInt, yOffsetInt);
        const float maxDistance = nlatsRegion * nlatsRegion
                + nlonsRegion * nlonsRegion;
        MFastMarch::MIntVector2D regionResolution(nlonsRegion, nlatsRegion);

        // Compute distance fields for every iso value.
        for (int isoValueIndex = 0;
             isoValueIndex < var->contourSetList.at(
                 contourSetIndex).levels.size();
             isoValueIndex++)
        {
            // Compute distance field for every ensemble member.
//#pragma omp parallel for
            for (i = 0; i < this->numMembers; ++i)
            {
                // Use fast-march method to compute distance field for every
                // ensemble member.
                MFastMarch::fastMarch2D(
                            gridDataStorage + i * textureSize,
                            var->contourSetList.at(contourSetIndex)
                            .levels.at(isoValueIndex), resolution,
                            offset, maxDistance, regionResolution,
                            grids.at(0)->gridIsCyclicInLongitude(),
                            variabilityPlot->distanceStorage[
                            getTextureIndex(isoValueIndex, this->numMembers, i)]);
            }
        }
    }

    // Array storing min and max field for distance variability plot.
    float* storage = new float[textureSize * 2];
    float numMembers = static_cast<float>(this->numMembers);
    // Standard deviation.
    float sigma = 0.0f;
    float mean = 0.0f;
    float value = 0.0f;

    // Variables needed to perform region check.
    unsigned int xBound = var->i0 + uint(min(var->nlons, nlons));
    unsigned int yBound = var->j0 + uint(var->nlats);
    unsigned int x = 0, y = 0;
    unsigned int xOffset = var->i0;
    unsigned int yOffset = var->j0;

    unsigned int index = 0;

    // Fill one render texture per iso value.
    for (int isoValueIndex = 0; isoValueIndex < var->contourSetList.at(
             contourSetIndex).levels.size(); isoValueIndex++)
    {
        unsigned int i;
        // Compute mean, min and max field for distance variability plot.
#pragma omp parallel for firstprivate(x, y, index, sigma, mean, value)
        // Loop over each grid point.
        for (i = 0; i < regionSize; i++)
        {
            // Compute 2D x and y coordinates from 1D texture index i.
            y = uint(floor(i / nlonsRegion));
            x = uint(i - y * nlonsRegion);

            x += xOffset;
            y += yOffset;

            if ( (x < xBound) && (y < yBound) )
            {
                // Map x to its corresponding value in the range [0,nlons[
                // (domain  of ensemble member scalar field).
                x = MMOD(x, nlons);
                index = x + y * nlons;

                // Initialise/reset mean and standard deviation with value of
                // first member.
                mean = variabilityPlot->distanceStorage[
                        getTextureIndex(isoValueIndex, this->numMembers,
                                        0)][index];
                sigma = mean * mean;
                // Loop over remaining members.
                for (int m = 1; m < this->numMembers; m++)
                {
                    // Store value to avoid reading it more often.
                    value = variabilityPlot->distanceStorage[
                            getTextureIndex(isoValueIndex, this->numMembers,
                                            m)][index];
                    sigma += value * value;
                    mean += value;
                }
                mean /= numMembers;
                // Calculate standard deviation = sqrt(E[X²] - (E[X])²) .
                sigma = sqrt((sigma / numMembers) - (mean * mean));
                // Scale sigma.
                sigma *= variabilityPlot->scale;

                // Storing results.
                storage[2 * index]   = mean - sigma;
                storage[2 * index + 1] = mean + sigma;
                variabilityPlot->distanceMean[
                        getTextureIndex(isoValueIndex, 1, 0)][index] =
                        mean;
            }
        }

        // Upload distance variability plot texture.
        distanceTextureContainer[isoValueIndex]->bindToTextureUnit(0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, nlons, nlats,
                        GL_RG, GL_FLOAT, storage); CHECK_GL_ERROR;
    }

    delete storage;
}


void MPlotCollection::computeScalarVariabilityPlot()
{
    createScalarVariabilityPlotTexturesAndArrays();

    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);

    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();
    unsigned int textureSize =
            grids.at(0)->getNumLons() * grids.at(0)->getNumLats();

    float* storage = new float[textureSize * 2];
    float numMembers = static_cast<float>(this->numMembers);
    // Standard deviation.
    float sigma = 0.0f;
    float mean = 0.0f;
    float value = 0.0f;

    // Variables for region check.
    unsigned int nlons = grids.at(0)->getNumLons();
    // Variables for region check.
    unsigned int xBound = var->i0 + min(uint(var->nlons), nlons);
    unsigned int yBound = var->j0 + uint(var->nlats);
    unsigned int x = 0, y = 0;
    unsigned int xOffset = var->i0;
    unsigned int yOffset = var->j0;

    // We do not want to compute parts of the scalar variability plot twice for
    // repeated regions but only for regions which are inside the bounding box
    // therefore we use the minimum of the dimensions of the bounding box and
    // the dimensions of the grid.
    unsigned int nlonsRegion = min(nlons, static_cast<unsigned int>(var->nlons));
    unsigned int regionSize = nlonsRegion
            * min(grids.at(0)->getNumLats(),
                  static_cast<unsigned int>(var->nlats));

    unsigned int index = 0;
    unsigned int i;
#pragma omp parallel for firstprivate(x, y, index, sigma, mean, value)
    for (i = 0; i < regionSize; i++)
    {
        // Compute x and y coordinates from i.
        y = uint(floor(i / nlonsRegion));
        x = uint(i - y * nlonsRegion);

        x += xOffset;
        y += yOffset;

        if ( (x < xBound) && (y < yBound) )
        {
            // Map x to its corresponding value in the range [0,nlons[ (domain 
            // of ensemble member scalar field).
            x = MMOD(x, nlons);
            index = x + y * nlons;

            // Initialise/reset mean and standard deviation with value of first
            // member.
            mean = gridDataStorage[index];
            sigma = mean * mean;
            // Loop over remaining members.
            for (int m = 1; m < this->numMembers; m++)
            {
                // Store value to avoid reading it more often.
                value = gridDataStorage[m * textureSize + index];
                sigma += value * value;
                mean += value;
            }
            mean /= numMembers;
            // Calculate standard deviation = sqrt(E[X²] - (E[X])²) .
            sigma = sqrt(sigma / numMembers - mean * mean);
            // Scale sigma.
            sigma *= variabilityPlot->scale;

            // Storing results.
            storage[index * 2]   =
                    mean - sigma - (float)(var->contourSetList.at(
                                               contourSetIndex).levels.at(0));
            storage[index * 2 + 1] =
                    mean + sigma - (float)(var->contourSetList.at(
                                               contourSetIndex).levels.at(0));
            variabilityPlot->scalarMean[index] = mean;
        }
    }

    // Upload scalar variability plot texture.
    textureScalarVariabilityPlot->bindToLastTextureUnit();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, grids.at(0)->getNumLons(),
                    grids.at(0)->getNumLats(), GL_RG, GL_FLOAT,
                    storage); CHECK_GL_ERROR;

    delete storage;
}


void MPlotCollection::renderSpaghettiPlot(MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();

    MStructuredGrid *grid = grids.at(0);
    var->textureLonLatLevAxes = grid->getLonLatLevTexture();

    glMarchingSquaresShader->bind();

    // Model-view-projection matrix from the current scene view.
    glMarchingSquaresShader->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

    // Texture bindings for Lat/Lon axes (1D textures).
    var->textureLonLatLevAxes->bindToTextureUnit(
                var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "latLonAxesData", var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "latOffset", GLint(grid->nlons)); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "isCyclicGrid",
                GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "eastGridLon",
                GLfloat(var->grid->lons[var->grid->nlons - 1])); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "shiftForWesternLon",
                GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;
    glMarchingSquaresShader->setUniformValue(
                "worldZ", GLfloat(sceneView->worldZfromPressure(
                                      actor->getSlicePosition_hPa())));

    // Variables for prismatic coloured plot.
    QColor colour = QColor(255, 0, 0, 255);
    double hue = 0.;
    // Divide colour spectrum by number of members.
    double stepsize = 6. / static_cast<double>(numMembers);

    GL::MTexture* texture2D = nullptr;

    // Loop over all ensemble members forming the spaghetti plots.
    for (int i = 0; i < numMembers; i++)
    {
        texture2D = grids.at(i)->get2DFieldTexture();
        texture2D->bindToTextureUnit(var->imageUnitTargetGrid);
        // The 2D data grid that the contouring algorithm processes.
        glBindImageTexture(var->imageUnitTargetGrid,      // image unit
                           texture2D->getTextureObject(),  // texture object
                           0,                        // level
                           GL_FALSE,                 // layered
                           0,                        // layer
                           GL_READ_WRITE,            // shader access
                           GL_R32F); CHECK_GL_ERROR; // format

        glMarchingSquaresShader->setUniformValue("sectionGrid",
                                                 var->imageUnitTargetGrid);

        // Draw individual line segments as output by the geometry shader (no
        // connected polygon is created).
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;

        glLineWidth(spaghettiPlot->thickness); CHECK_GL_ERROR;
        // Use prismatic colours to assign contours to members.
        if (spaghettiPlot->prismaticColoured)
        {
            switch ((int)floor(hue))
            {
            case 0:
                colour.setRgb(255, (int)( 255 * (hue - floor(hue)) ), 0);
                break;
            case 1:
                colour.setRgb((int)( 255 * (1.0 - (hue - floor(hue))) ), 255, 0);
                break;
            case 2:
                colour.setRgb(0, 255, (int)( 255 * (hue - floor(hue)) ));
                break;
            case 3:
                colour.setRgb(0, (int)( 255 * (1.0 - (hue - floor(hue)) )), 255);
                break;
            case 4:
                colour.setRgb((int)( 255 * (hue - floor(hue)) ), 0, 255);
                break;
            case 5:
                colour.setRgb(255, 0, (int)( 255 * (1.0 - (hue - floor(hue))) ));
                break;
            default:
                break;
            }
            hue += stepsize;

            glMarchingSquaresShader->setUniformValue(
                        "colour", colour);
        }
        // Use one colour for all members.
        else
        {
            glMarchingSquaresShader->setUniformValue(
                        "colour", spaghettiPlot->colour);
        }

        // Loop over all iso values a spaghetti plot should be rendered for.
        for (int j = 0; j < var->contourSetList.at(
                 contourSetIndex).levels.size(); j++)
        {
            glMarchingSquaresShader->setUniformValue(
                        "isoValue",
                        GLfloat(var->contourSetList.at(
                                    contourSetIndex).levels.at(j)));
            glDrawArraysInstanced(GL_POINTS,
                                  0,
                                  var->nlons - 1,
                                  var->nlats - 1); CHECK_GL_ERROR;
        } // for iso values
        grids.at(i)->release2DFieldTexture();
    } // for ensemble members
}


void MPlotCollection::renderContourBoxplots(MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    // Extracting first two band depth values.
    float value0 = contourPlot->bandDepth[0];
    value0 = value0 - floor(value0 / 10.0f) * 10.0f;
    float value1 = contourPlot->bandDepth[1];
    value1 = value1 - floor(value1 / 10.0f) * 10.0f;
    // Only render contour boxplot band if it exists (<=> two or more non zero
    // band depth values exist).
    if (value0 != 0.0f && value1 != 0.0f)
    {
        glContourPlotsShader->bindProgram("Standard");

        // Model-view-projection matrix from the current scene view.
        glContourPlotsShader->setUniformValue(
                    "mvpMatrix",
                    *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;

        // Texture bindings for Lat/Lon axes (1D textures).
        var->textureLonLatLevAxes->bindToTextureUnit(
                    var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "latLonAxesData",
                    var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "latOffset",
                    var->gridAggregation->getGrids().at(0)->nlons); CHECK_GL_ERROR;

        glContourPlotsShader->setUniformValue(
                    "innerColour", contourPlot->innerColour);  CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "outerColour", contourPlot->outerColour);  CHECK_GL_ERROR;

        glContourPlotsShader->setUniformValue(
                    "isoValue", GLfloat(
                        var->contourSetList.at(
                            contourSetIndex).levels.at(0))); CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "renderOuter",
                    GLboolean(contourPlot->drawOuter)); CHECK_GL_ERROR;
        // Work around: Shift z-position of plot depending on which variable is
        // drawn to render the variables added later to the actor above the
        // variables added earlier to the actor.
        glContourPlotsShader->setUniformValue(
                    "worldZ",
                    GLfloat(sceneView->worldZfromPressure(
                                actor->getSlicePosition_hPa()))); CHECK_GL_ERROR;

        glContourPlotsShader->setUniformValue(
                    "contourPlotTexture",
                    GLint(var->imageUnitTargetGrid)); CHECK_GL_ERROR;
        glBindImageTexture(var->imageUnitTargetGrid, // image unit
                           textureContourBoxplot->getTextureObject(),
                                                     // texture object
                           0,                        // level
                           GL_FALSE,                 // layered
                           0,                        // layer
                           GL_READ_WRITE,            // shader access
                           GL_RGBA32F); CHECK_GL_ERROR; // format

        // Grid offsets to render only the requested subregion.
        glContourPlotsShader->setUniformValue(
                    "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "bboxLons", QVector2D(actor->getLlcrnrlon(),
                                          actor->getUrcrnrlon())); CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "isCyclicGrid",
                    GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "eastGridLon",
                    GLfloat(var->grid->lons[var->grid->nlons - 1])); CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "shiftForWesternLon",
                    GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

        // Use instanced rendering to avoid geometry upload (see notes 09Feb2012).
        glPolygonOffset(.8f, 1.0f); CHECK_GL_ERROR;
        glEnable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
        // Used to draw contour boxplots of later variables above plot of
        // earlier variables.
        glDepthFunc(GL_LEQUAL);
        glPolygonMode(GL_FRONT_AND_BACK,
                      actor->getRenderAsWireFrame() ? GL_LINE : GL_FILL);
        CHECK_GL_ERROR;
        glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                              0, var->nlons * 2, var->nlats - 1); CHECK_GL_ERROR;

        glDisable(GL_POLYGON_OFFSET_FILL);
        // Reset depth test function.
        glDepthFunc(GL_LESS);
    }
}


void MPlotCollection::renderContourBoxplotMedianLine(MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    // Index of member representing the median contour.
    int medianIndex = floor(contourPlot->bandDepth[0] / 10);
    // Draw median line only if the user wants it to be drawn and if it exists
    // (i.e. at least one band depth value needs to be unequal to zero).
    if (contourPlot->drawMedian &&
            ((contourPlot->bandDepth[0]
              - (static_cast<float>(medianIndex) * 10.f)) > 0.f) )
    {
        QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();
        MStructuredGrid *grid = grids.at(medianIndex);
        var->textureLonLatLevAxes = grid->getLonLatLevTexture();

        glMarchingSquaresShader->bind();

        // Model-view-projection matrix from the current scene view.
        glMarchingSquaresShader->setUniformValue(
                    "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

        // Texture bindings for Lat/Lon axes (1D textures).
        var->textureLonLatLevAxes->bindToTextureUnit(
                    var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "latLonAxesData", var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "latOffset", GLint(grid->nlons)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "isCyclicGrid",
                    GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "eastGridLon",
                    GLfloat(var->grid->lons[var->grid->nlons - 1])); CHECK_GL_ERROR;
        CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "shiftForWesternLon",
                    GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

        glMarchingSquaresShader->setUniformValue(
                    "worldZ", GLfloat(sceneView->worldZfromPressure(
                                          actor->getSlicePosition_hPa())));

        // The 2D data grid that the contouring algorithm processes.
        glBindImageTexture(var->imageUnitTargetGrid,      // image unit
                           grid->get2DFieldTexture()->getTextureObject(),
                                                     // texture object
                           0,                        // level
                           GL_FALSE,                 // layered
                           0,                        // layer
                           GL_READ_WRITE,            // shader access
                           GL_R32F); CHECK_GL_ERROR; // format
        glMarchingSquaresShader->setUniformValue("sectionGrid",
                                                 var->imageUnitTargetGrid);

        // Draw individual line segments as output by the geometry shader (no
        // connected polygon is created).
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;

        // Draw the median contour.
        glLineWidth(contourPlot->medianThickness); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "colour", contourPlot->medianColour);
        glMarchingSquaresShader->setUniformValue(
                    "isoValue", GLfloat(
                        var->contourSetList.at(contourSetIndex).levels.at(0)));
        glDrawArraysInstanced(GL_POINTS,
                              0,
                              var->nlons - 1,
                              var->nlats - 1); CHECK_GL_ERROR;

        grid->release2DFieldTexture();
    } // if
}


void MPlotCollection::renderContourBoxplotOutliers(MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();
    // Index of member being outlier, initialise with index of last member in
    // bandDepth array.
    int outlierIndex = floor(contourPlot->bandDepth[numMembers - 1] / 10);
    // Band depth value, initialise with value of last member in bandDepth array.
    float bandDepthValue =
            (contourPlot->bandDepth[numMembers - 1] - outlierIndex * 10.f);

    // Only draw outliers if the user wants them to be drawn and only if at
    // least one exists (i.e. at least one ensemble member has a band depth
    // value equal to zero).
    if (contourPlot->drawOutliers && bandDepthValue == 0.f)
    {
        MStructuredGrid *grid = grids.at(outlierIndex);
        var->textureLonLatLevAxes = grid->getLonLatLevTexture();

        glMarchingSquaresShader->bind();

        // Model-view-projection matrix from the current scene view.
        glMarchingSquaresShader->setUniformValue(
                    "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

        // Texture bindings for Lat/Lon axes (1D textures).
        var->textureLonLatLevAxes->bindToTextureUnit(
                    var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "latLonAxesData", var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "latOffset", GLint(grid->nlons)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "isCyclicGrid",
                    GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "eastGridLon",
                    GLfloat(var->grid->lons[var->grid->nlons - 1])); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "shiftForWesternLon",
                    GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

        glMarchingSquaresShader->setUniformValue(
                    "worldZ",
                    GLfloat(sceneView->worldZfromPressure(
                                actor->getSlicePosition_hPa()) ) ); CHECK_GL_ERROR;

        // Pointer to 2D scalar field texture of an ensemble member.
        GL::MTexture* texture2D = nullptr;

        // Loop over all ensemble members identified as outliers and perform one
        // render pass per outlier.
        for (int i = numMembers - 1; (i >= 0) && (bandDepthValue == 0.f); i--)
        {
            // Grids index of member identified as outlier.
            outlierIndex = static_cast<int>(contourPlot->bandDepth[i] / 10);

            // Get scalar field of the outlier to be drawn in this render pass.
            texture2D = grids.at(outlierIndex)->get2DFieldTexture();
            texture2D->bindToTextureUnit(var->imageUnitTargetGrid);
            // The 2D data grid that the contouring algorithm processes.
            glBindImageTexture(var->imageUnitTargetGrid,      // image unit
                               texture2D->getTextureObject(),  // texture object
                               0,                        // level
                               GL_FALSE,                 // layered
                               0,                        // layer
                               GL_READ_WRITE,            // shader access
                               // GL_WRITE_ONLY,            // shader access
                               GL_R32F); CHECK_GL_ERROR; // format
            glMarchingSquaresShader->setUniformValue("sectionGrid",
                                                     var->imageUnitTargetGrid);

            // Draw individual line segments as output by the geometry shader
            // (no connected polygon is created).
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;

            glLineWidth(contourPlot->outlierThickness); CHECK_GL_ERROR;
            glMarchingSquaresShader->setUniformValue(
                        "colour", contourPlot->outlierColour);

            glMarchingSquaresShader->setUniformValue(
                        "isoValue", GLfloat(var->contourSetList.at(
                                                contourSetIndex).levels.at(0)));
            glDrawArraysInstanced(GL_POINTS,
                                  0,
                                  var->nlons - 1,
                                  var->nlats - 1); CHECK_GL_ERROR;

            // Calculate band depth value of next member.
            bandDepthValue = (contourPlot->bandDepth[i - 1]
                    - static_cast<float>(floor(contourPlot->bandDepth[i - 1]
                                         / 10.f)) * 10.f);
            grids.at(outlierIndex)->release2DFieldTexture();
        } // for
    } // if
}


void MPlotCollection::renderContourProbabilityPlots(MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();
    // Choose program to render contour probability plot according to whether
    // the outermost band should be drawn or not.
    if (contourPlot->drawOutermost)
    {
        glContourPlotsShader->bindProgram("RenderProbabilityPlot");
        glContourPlotsShader->setUniformValue(
                    "outermostColour",
                    contourPlot->outermostColour); CHECK_GL_ERROR;
        glContourPlotsShader->setUniformValue(
                    "nlats", GLint(grids.at(0)->getNumLats())); CHECK_GL_ERROR;
    }
    else
    {
        // If the outermost band should not be drawn, contour probability plot
        // can be treated like a contour boxplot to render the bands.
        glContourPlotsShader->bindProgram("Standard");
    }

    // Model-view-projection matrix from the current scene view.
    glContourPlotsShader->setUniformValue(
                "mvpMatrix",
                *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;

    // Texture bindings for Lat/Lon axes (1D textures).
    var->textureLonLatLevAxes->bindToTextureUnit(
                var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "latLonAxesData", var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "latOffset", grids.at(0)->nlons); CHECK_GL_ERROR;

    glContourPlotsShader->setUniformValue(
                "innerColour", contourPlot->innerColour); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "outerColour", contourPlot->outerColour); CHECK_GL_ERROR;

    glContourPlotsShader->setUniformValue(
                "isoValue",
                GLfloat(var->contourSetList.at(
                            contourSetIndex).levels.at(0))); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "renderOuter",
                GLboolean(contourPlot->drawOuter)); CHECK_GL_ERROR;

    glContourPlotsShader->setUniformValue(
                "worldZ",
                GLfloat(sceneView->worldZfromPressure(
                            actor->getSlicePosition_hPa()))); CHECK_GL_ERROR;

    glContourPlotsShader->setUniformValue(
                "contourPlotTexture",
                GLint(var->imageUnitTargetGrid)); CHECK_GL_ERROR;
    glBindImageTexture(var->imageUnitTargetGrid, // image unit
                       textureContourProbabilityPlot->getTextureObject(),
                                                 // texture object
                       0,                        // level
                       GL_FALSE,                 // layered
                       0,                        // layer
                       GL_READ_WRITE,            // shader access
                       GL_RGBA32F); CHECK_GL_ERROR; // format

    // Grid offsets to render only the requested subregion.
    glContourPlotsShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "bboxLons", QVector2D(actor->getLlcrnrlon(),
                                      actor->getUrcrnrlon())); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "isCyclicGrid",
                GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "eastGridLon",
                GLfloat(var->grid->lons[var->grid->nlons - 1])); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "shiftForWesternLon",
                GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;
    glContourPlotsShader->setUniformValue(
                "worldZ", GLfloat(sceneView->worldZfromPressure(
                                      actor->getSlicePosition_hPa())));

    // Use instanced rendering to avoid geometry upload (see notes 09Feb2012).
    glPolygonOffset(.8f, 1.0f); CHECK_GL_ERROR;
    glEnable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
    glPolygonMode(GL_FRONT_AND_BACK,
                  actor->getRenderAsWireFrame() ? GL_LINE : GL_FILL);
    CHECK_GL_ERROR;
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                          0, var->nlons * 2, var->nlats - 1); CHECK_GL_ERROR;

    glDisable(GL_POLYGON_OFFSET_FILL);
}


void MPlotCollection::renderContourProbabilityPlotMedianLine(
        MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    // Only draw median contour if the user wants it to be drawn.
    if (contourPlot->drawMedian)
    {
        QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();
        MStructuredGrid *grid = grids.at(0);
        var->textureLonLatLevAxes = grid->getLonLatLevTexture();

        textureLineDrawing->bindToTextureUnit(var->imageUnitTargetGrid);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D,             // target
                     0,                         // level of detail
                     GL_R32F,                   // internal format
                     grids.at(0)->getNumLons(), // width
                     grids.at(0)->getNumLats(), // height
                     0,                         // border
                     GL_RED,                    // format
                     GL_FLOAT,                  // data type of the pixel data
                     contourPlot->probabilityMedian); CHECK_GL_ERROR;

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glMarchingSquaresShader->bind();

        // Model-view-projection matrix from the current scene view.
        glMarchingSquaresShader->setUniformValue(
                    "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

        // Texture bindings for Lat/Lon axes (1D textures).
        var->textureLonLatLevAxes->bindToTextureUnit(
                    var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "latLonAxesData",
                    var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "latOffset", GLint(grid->nlons)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "isCyclicGrid",
                    GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "eastGridLon",
                    GLfloat(var->grid->lons[var->grid->nlons - 1])); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "shiftForWesternLon",
                    GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

        glMarchingSquaresShader->setUniformValue(
                    "worldZ", GLfloat(sceneView->worldZfromPressure(
                                          actor->getSlicePosition_hPa())));

        // The 2D data grid that the contouring algorithm processes.
        glBindImageTexture(var->imageUnitTargetGrid, // image unit
                           textureLineDrawing->getTextureObject(),
                                                     // texture object
                           0,                        // level
                           GL_FALSE,                 // layered
                           0,                        // layer
                           GL_READ_WRITE,            // shader access
                           GL_R32F); CHECK_GL_ERROR; // format
        glMarchingSquaresShader->setUniformValue("sectionGrid",
                                                 var->imageUnitTargetGrid);

        // Draw individual line segments as output by the geometry shader (no
        // connected polygon is created).
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;

        // Draw the median contour.
        glLineWidth(contourPlot->medianThickness); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "colour", contourPlot->medianColour);
        glMarchingSquaresShader->setUniformValue(
                    "isoValue", GLfloat(var->contourSetList.at(
                                            contourSetIndex).levels.at(0)));
        glDrawArraysInstanced(GL_POINTS,
                              0,
                              var->nlons - 1,
                              var->nlats - 1); CHECK_GL_ERROR;
    } // if
}


void MPlotCollection::renderVariabilityPlot(MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    glVariabilityPlotsShader->bindProgram("Standard");

    // Model-view-projection matrix from the current scene view.
    glVariabilityPlotsShader->setUniformValue(
                "mvpMatrix",
                *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;

    // Texture bindings for Lat/Lon axes (1D textures).
    var->textureLonLatLevAxes->bindToTextureUnit(
                var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "latLonAxesData", var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "latOffset",
                var->gridAggregation->getGrids().at(0)->nlons); CHECK_GL_ERROR;

    glVariabilityPlotsShader->setUniformValue("colour", variabilityPlot->colour);

    glVariabilityPlotsShader->setUniformValue(
                "worldZ",
                GLfloat(sceneView->worldZfromPressure(
                            actor->getSlicePosition_hPa()))); CHECK_GL_ERROR;

    glVariabilityPlotsShader->setUniformValue(
                "variabilityPlotTexture",
                GLint(var->imageUnitTargetGrid)); CHECK_GL_ERROR;

    // Choose texture according to which variability plot should be drawn.
    if (var->renderSettings.renderMode
            == MNWP2DHorizontalActorVariable::RenderMode::DistanceVariabilityPlot)
    {
        // The computation and rendering for distance variability plots were changed
        // to work for multiple iso values. Because of that this method does not
        // work for distance variability plots any more. (This method should be
        // replaced by the multiple iso value rendering method as soon multiple
        // iso value support is implemented for scalar variability plots as well.)
        // Draw distance variability plot.
//        glBindImageTexture(var->imageUnitTargetGrid, // image unit
//                           textureDistanceVariabilityPlot->getTextureObject(),
//                                                     // texture object
//                           0,                        // level
//                           GL_FALSE,                 // layered
//                           0,                        // layer
//                           GL_READ_WRITE,            // shader access
//                           GL_RG32F); CHECK_GL_ERROR; // format
    }
    else
    {
        // draw scalar variability plot
        glBindImageTexture(var->imageUnitTargetGrid, // image unit
                           textureScalarVariabilityPlot->getTextureObject(),
                                                     // texture object
                           0,                        // level
                           GL_FALSE,                 // layered
                           0,                        // layer
                           GL_READ_WRITE,            // shader access
                           GL_RG32F); CHECK_GL_ERROR; // format
    }

    // Grid offsets to render only the requested subregion.
    glVariabilityPlotsShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "bboxLons", QVector2D(actor->getLlcrnrlon(),
                                      actor->getUrcrnrlon())); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "isCyclicGrid",
                GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "eastGridLon",
                GLfloat(var->grid->lons[var->grid->nlons - 1])); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "shiftForWesternLon",
                GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

    // Use instanced rendering to avoid geometry upload (see notes 09Feb2012).
    glPolygonOffset(.8f, 1.f); CHECK_GL_ERROR;
    glEnable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
    glPolygonMode(GL_FRONT_AND_BACK,
                  actor->getRenderAsWireFrame() ? GL_LINE : GL_FILL);
    CHECK_GL_ERROR;
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                          0, var->nlons * 2, var->nlats - 1); CHECK_GL_ERROR;

    glDisable(GL_POLYGON_OFFSET_FILL);
    return;
}


void MPlotCollection::renderMultiIsoVariabilityPlot(MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);
    glVariabilityPlotsShader->bindProgram("Standard");

    // Model-view-projection matrix from the current scene view.
    glVariabilityPlotsShader->setUniformValue(
                "mvpMatrix",
                *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;

    // Texture bindings for Lat/Lon axes (1D textures).
    var->textureLonLatLevAxes->bindToTextureUnit(
                var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "latLonAxesData", var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "latOffset",
                var->gridAggregation->getGrids().at(0)->nlons); CHECK_GL_ERROR;

    glVariabilityPlotsShader->setUniformValue("colour", variabilityPlot->colour);

    glVariabilityPlotsShader->setUniformValue(
                "worldZ",
                GLfloat(sceneView->worldZfromPressure(
                            actor->getSlicePosition_hPa()))); CHECK_GL_ERROR;

    glVariabilityPlotsShader->setUniformValue(
                "variabilityPlotTexture",
                GLint(var->imageUnitTargetGrid)); CHECK_GL_ERROR;

    // Grid offsets to render only the requested subregion.
    glVariabilityPlotsShader->setUniformValue(
                "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "bboxLons", QVector2D(actor->getLlcrnrlon(),
                                      actor->getUrcrnrlon())); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "isCyclicGrid",
                GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "eastGridLon",
                GLfloat(var->grid->lons[var->grid->nlons - 1])); CHECK_GL_ERROR;
    glVariabilityPlotsShader->setUniformValue(
                "shiftForWesternLon",
                GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

    // Loop over all iso values the variability plot should be drawn for.
    for (int isoValueIndex = 0; isoValueIndex < var->contourSetList.at(
             contourSetIndex).levels.size();
         isoValueIndex++)
    {
        // Draw distance variability plot.
        glBindImageTexture(var->imageUnitTargetGrid, // image unit
                           distanceTextureContainer[isoValueIndex]
                           ->getTextureObject(),
                                                     // texture object
                           0,                        // level
                           GL_FALSE,                 // layered
                           0,                        // layer
                           GL_READ_WRITE,            // shader access
                           GL_RG32F); CHECK_GL_ERROR; // format

        // Use instanced rendering to avoid geometry upload (see notes 09Feb2012).
        glPolygonOffset(.8f, 1.f); CHECK_GL_ERROR;
        glEnable(GL_POLYGON_OFFSET_FILL); CHECK_GL_ERROR;
        glPolygonMode(GL_FRONT_AND_BACK,
                      actor->getRenderAsWireFrame() ? GL_LINE : GL_FILL);
        CHECK_GL_ERROR;
        glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                              0, var->nlons * 2, var->nlats - 1);
        CHECK_GL_ERROR;
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    return;
}


void MPlotCollection::renderVariabilityPlotMean(MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);

    // Only draw mean line if the user wants it to be drawn.
    if (variabilityPlot->drawMean)
    {
        QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();

        MStructuredGrid* grid = grids.at(0);
        var->textureLonLatLevAxes = grid->getLonLatLevTexture();

        glMarchingSquaresShader->bind();

        // Model-view-projection matrix from the current scene view.
        glMarchingSquaresShader->setUniformValue(
                    "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

        // Texture bindings for Lat/Lon axes (1D textures).
        var->textureLonLatLevAxes->bindToTextureUnit(
                    var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "latLonAxesData",
                    var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "latOffset", GLint(grid->nlons)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "isCyclicGrid",
                    GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "eastGridLon",
                    GLfloat(var->grid->lons[var->grid->nlons - 1])); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "shiftForWesternLon",
                    GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

        glMarchingSquaresShader->setUniformValue(
                    "worldZ",
                    GLfloat(sceneView->worldZfromPressure(
                                actor->getSlicePosition_hPa())));CHECK_GL_ERROR;

        textureLineDrawing->bindToTextureUnit(var->imageUnitTargetGrid);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // Choose texture and iso value according to which variability plot mean
        // should be drawn.
        if (var->renderSettings.renderMode
                == MNWP2DHorizontalActorVariable::RenderMode::DistanceVariabilityPlot)
        {
        // The computation and rendering for distance variability plots were changed
        // to work for multiple iso values. Because of that this method does not
        // work for distance variability plots any more. (This method should be
        // replaced by the multiple iso value rendering method as soon multiple
        // iso value support is implemented for scalar variability plots as well.)
            //            // Draw distance variability mean.
            //            glTexImage2D(GL_TEXTURE_2D,             // target
            //                         0,                         // level of detail
            //                         GL_RG32F,                  // internal format
            //                         grid->getNumLons(),        // width
            //                         grid->getNumLats(),        // height
            //                         0,                         // border
            //                         GL_RED,                      // format
            //                         GL_FLOAT,                  // data type of the pixel data
            //                         variabilityPlot->distanceMean); CHECK_GL_ERROR;

            //            glMarchingSquaresShader->setUniformValue(
            //                        "isoValue", GLfloat(0.0f));
        }
        else
        {
            // Draw scalar variability mean.
            glTexImage2D(GL_TEXTURE_2D,             // target
                         0,                         // level of detail
                         GL_R32F,                   // internal format
                         grid->getNumLons(),        // width
                         grid->getNumLats(),        // height
                         0,                         // border
                         GL_RED,                    // format
                         GL_FLOAT,                  // data type of the pixel data
                         variabilityPlot->scalarMean); CHECK_GL_ERROR;

            glMarchingSquaresShader->setUniformValue(
                        "isoValue", GLfloat(var->contourSetList.at(
                                                contourSetIndex).levels.at(0)));
        }
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // The 2D data grid that the contouring algorithm processes.
        glBindImageTexture(var->imageUnitTargetGrid, // image unit
                           textureLineDrawing->getTextureObject(),
                                                     // texture object
                           0,                        // level
                           GL_FALSE,                 // layered
                           0,                        // layer
                           GL_READ_WRITE,            // shader access
                           // GL_WRITE_ONLY,            // shader access
                           GL_R32F); CHECK_GL_ERROR; // format
        glMarchingSquaresShader->setUniformValue("sectionGrid",
                                                 var->imageUnitTargetGrid);

        // Draw individual line segments as output by the geometry shader (no
        // connected polygon is created).
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;

        glLineWidth(variabilityPlot->meanThickness); CHECK_GL_ERROR;

        glMarchingSquaresShader->setUniformValue(
                    "colour", variabilityPlot->meanColour);
        glDrawArraysInstanced(GL_POINTS,
                              0,
                              var->nlons - 1,
                              var->nlats - 1); CHECK_GL_ERROR;
    } // if
}


void MPlotCollection::renderMultiIsoVariabilityPlotMean(MSceneViewGLWidget *sceneView)
{
    MNWP2DHorizontalActorVariable *var =
            static_cast<MNWP2DHorizontalActorVariable*>(this->var);

    // Only draw mean line if the user wants it to be drawn.
    if (variabilityPlot->drawMean)
    {
        QList<MStructuredGrid*> grids = var->gridAggregation->getGrids();

        MStructuredGrid* grid = grids.at(0);
        var->textureLonLatLevAxes = grid->getLonLatLevTexture();

        glMarchingSquaresShader->bind();

        // Model-view-projection matrix from the current scene view.
        glMarchingSquaresShader->setUniformValue(
                    "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

        // Texture bindings for Lat/Lon axes (1D textures).
        var->textureLonLatLevAxes->bindToTextureUnit(
                    var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "latLonAxesData",
                    var->textureUnitLonLatLevAxes); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "latOffset", GLint(grid->nlons)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "iOffset", GLint(var->i0)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "jOffset", GLint(var->j0)); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "isCyclicGrid",
                    GLboolean(var->grid->gridIsCyclicInLongitude())); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "leftGridLon", GLfloat(var->grid->lons[0])); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "eastGridLon",
                    GLfloat(var->grid->lons[var->grid->nlons - 1])); CHECK_GL_ERROR;
        glMarchingSquaresShader->setUniformValue(
                    "shiftForWesternLon",
                    GLfloat(var->shiftForWesternLon)); CHECK_GL_ERROR;

        glMarchingSquaresShader->setUniformValue(
                    "worldZ",
                    GLfloat(sceneView->worldZfromPressure(
                                actor->getSlicePosition_hPa()))); CHECK_GL_ERROR;

        textureLineDrawing->bindToTextureUnit(var->imageUnitTargetGrid);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glLineWidth(variabilityPlot->meanThickness); CHECK_GL_ERROR;

        glMarchingSquaresShader->setUniformValue("colour",
                                                 variabilityPlot->meanColour);

        glMarchingSquaresShader->setUniformValue(
                    "isoValue", GLfloat(0.f)); CHECK_GL_ERROR;

        glMarchingSquaresShader->setUniformValue(
                    "worldZ",
                    GLfloat(sceneView->worldZfromPressure(
                                actor->getSlicePosition_hPa()))); CHECK_GL_ERROR;

        // Loop over all iso values mean contour should be drawn for.
        for (int isoValueIndex = 0; isoValueIndex < var->contourSetList.at(
                 contourSetIndex).levels.size(); isoValueIndex++)
        {
            // Draw distance variability mean.
            glTexImage2D(GL_TEXTURE_2D,             // target
                         0,                         // level of detail
                         GL_R32F,                   // internal format
                         grid->getNumLons(),        // width
                         grid->getNumLats(),        // height
                         0,                         // border
                         GL_RED,                    // format
                         GL_FLOAT,                  // data type of the pixel data
                         variabilityPlot->distanceMean[isoValueIndex]);
            CHECK_GL_ERROR;
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            // The 2D data grid that the contouring algorithm processes.
            glBindImageTexture(var->imageUnitTargetGrid, // image unit
                               textureLineDrawing->getTextureObject(),
                                                         // texture object
                               0,                        // level
                               GL_FALSE,                 // layered
                               0,                        // layer
                               GL_READ_WRITE,            // shader access
                               // GL_WRITE_ONLY,            // shader access
                               GL_R32F); CHECK_GL_ERROR; // format
            glMarchingSquaresShader->setUniformValue("sectionGrid",
                                                     var->imageUnitTargetGrid);

            // Draw individual line segments as output by the geometry shader
            // (no connected polygon is created).
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;

            glDrawArraysInstanced(GL_POINTS,
                                  0,
                                  var->nlons - 1,
                                  var->nlats - 1); CHECK_GL_ERROR;
        } // for
    } // if
}


void MPlotCollection::deleteTexturesAndArrays()
{
    if (textureHandles)
    {
        delete textureHandles;
        textureHandles = nullptr;
    }

    if (gridTextureHandles)
    {
        delete gridTextureHandles;
        gridTextureHandles = nullptr;
    }

    textureDelete(textureLineDrawing);

    textureDelete(textureContourBoxplot);
    textureDelete(textureCBPBandDepth);
    textureDelete(textureCBPEpsilonMatrix);
    textureDelete(textureContourProbabilityPlot);

    for (int i = 0; i < binaryMapTextureContainer.size(); i++)
    {
        textureDelete(binaryMapTextureContainer.at(i));
    }
    binaryMapTextureContainer.clear();

    if (contourPlot->bandDepth)
    {
        delete contourPlot->bandDepth;
        contourPlot->bandDepth = nullptr;
    }
    if (gridDataStorage != nullptr)
    {
        delete gridDataStorage;
        gridDataStorage = nullptr;
    }
    if (contourPlot->probabilityMedian)
    {
        delete contourPlot->probabilityMedian;
        contourPlot->probabilityMedian = nullptr;
    }

    textureDelete(textureScalarVariabilityPlot);

    for (int i = 0; i < distanceTextureContainer.size(); i++)
    {
        textureDelete(distanceTextureContainer.at(i));
    }
    distanceTextureContainer.clear();

    for (int i = 0; i < variabilityPlot->distanceMean.size(); i++)
    {
        if (variabilityPlot->distanceMean[i] != nullptr)
        {
            delete[] variabilityPlot->distanceMean[i];
        }
        variabilityPlot->distanceMean[i] = nullptr;

    }
    variabilityPlot->distanceMean.clear();

    if (variabilityPlot->scalarMean != nullptr)
    {
        delete variabilityPlot->scalarMean;
        variabilityPlot->scalarMean = nullptr;
    }
}


/******************************************************************************
***                              Spaghettiplot                              ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MPlotCollection::SpaghettiPlot::SpaghettiPlot(MActor *actor,
                                              QtProperty *renderGroup)
    : colour(QColor(227, 64, 64, 255)),
      thickness(1.2),
      prismaticColoured(false)

{
    MQtProperties *properties = actor->getQtProperties();

    groupProperty = actor->addProperty(GROUP_PROPERTY, "spaghetti plot",
                                       renderGroup);

    colourProperty = actor->addProperty(COLOR_PROPERTY, "colour", groupProperty);
    properties->mColor()->setValue(colourProperty, colour);

    thicknessProperty = actor->addProperty(DOUBLE_PROPERTY, "thickness",
                                           groupProperty);
    properties->setDouble(thicknessProperty, thickness, 0.1, 10., 1, 0.1);

    prismaticColouredProperty = actor->addProperty(BOOL_PROPERTY,
                                                   "prismatic colours",
                                                   groupProperty);
    properties->mBool()->setValue(prismaticColouredProperty, prismaticColoured);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MPlotCollection::SpaghettiPlot::saveConfiguration(QSettings *settings)
{
    settings->setValue("spaghettiPlotColour", colour);
    settings->setValue("spaghettiPlotThickness", thickness);
    settings->setValue("spaghettiPlotPrismaticColoured", prismaticColoured);
}


void MPlotCollection::SpaghettiPlot::loadConfiguration(
        QSettings *settings, MQtProperties *properties)
{
    properties->mColor()->setValue(
                colourProperty,
                settings->value("spaghettiPlotColour", QColor(227, 64, 64, 255))
                .value<QColor>());
    thickness = settings->value("spaghettiPlotThickness", 1.2).toDouble();
    properties->mDouble()->setValue(thicknessProperty, thickness);
    prismaticColoured = settings->value("spaghettiPlotPrismaticColoured",
                                        false).toBool();
    properties->mBool()->setValue(prismaticColouredProperty, prismaticColoured);
}


/******************************************************************************
***                               ContourPlot                               ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MPlotCollection::ContourPlot::ContourPlot(MActor *actor,
                                          QtProperty *renderGroup)
    : // Colour schema from paper.
      innerColour(QColor(176, 176, 218, 255)),
      drawOuter(true),
      outerColour(QColor(220, 220, 253 , 255)),
      drawMedian(true),
      medianThickness(2.),
      medianColour(QColor(245, 245, 70, 255)),
      // Boxplot.
      boxplotNeedsRecompute(true),
      drawOutliers(true),
      outlierThickness(1.2),
      outlierColour(QColor(227, 64, 64, 255)),
      epsilon(0.029),
      defaultEpsilon(0.029),
      useDefaultEpsilon(true),
      epsilonChanged(true),
      // Probability plot.
      probabilityNeedsRecompute(true),
      innerPercentage(50.),
      outerPercentage(10.),
      drawOutermost(true),
      outermostColour(QColor(240, 240, 255, 255)),
      // Arrays.
      bandDepth(new float[MAX_NUM_MEMBERS]),
      probabilityMedian(nullptr)
{
    MQtProperties *properties = actor->getQtProperties();

    groupProperty = actor->addProperty(GROUP_PROPERTY, "contour plot",
                                       renderGroup);

    innerColourProperty = actor->addProperty(COLOR_PROPERTY, "inner colour",
                                             groupProperty);
    properties->mColor()->setValue(innerColourProperty, innerColour);

    drawOuterProperty = actor->addProperty(BOOL_PROPERTY, "draw outer",
                                           groupProperty);
    properties->mBool()->setValue(drawOuterProperty, drawOuter);

    outerColourProperty = actor->addProperty(COLOR_PROPERTY, "outer colour",
                                             groupProperty);
    properties->mColor()->setValue(outerColourProperty, outerColour);

    drawMedianProperty = actor->addProperty(BOOL_PROPERTY, "draw median",
                                            groupProperty);
    properties->mBool()->setValue(drawMedianProperty, drawMedian);

    medianThicknessProperty = actor->addProperty(
                DOUBLE_PROPERTY, "median thickness", groupProperty);
    properties->setDouble(medianThicknessProperty, medianThickness, 0.1, 10.,
                          1, 0.1);

    medianColourProperty = actor->addProperty(COLOR_PROPERTY, "median colour",
                                              groupProperty);
    properties->mColor()->setValue(medianColourProperty, medianColour);

    // Properties for contour boxplot only.
    BoxplotGroupProperty = actor->addProperty(GROUP_PROPERTY, "contour boxplot",
                                             groupProperty);

    drawOutliersProperty = actor->addProperty(BOOL_PROPERTY, "draw outliers",
                                              BoxplotGroupProperty);
    properties->mBool()->setValue(drawOutliersProperty, drawOutliers);

    outlierThicknessProperty = actor->addProperty(
                DOUBLE_PROPERTY, "outlier thickness", BoxplotGroupProperty);
    properties->setDouble(outlierThicknessProperty, outlierThickness, 0.1, 10.,
                          1, 0.1);

    outlierColourProperty = actor->addProperty(COLOR_PROPERTY, "outlier colour",
                                               BoxplotGroupProperty);
    properties->mColor()->setValue(outlierColourProperty, outlierColour);

    epsilonProperty = actor->addProperty(DOUBLE_PROPERTY, "epsilon",
                                         BoxplotGroupProperty);
    properties->setDouble(epsilonProperty, epsilon, 0., 1., 5, 0.001);
    epsilonProperty->setEnabled(!useDefaultEpsilon);

    useDefaultEpsilonProperty = actor->addProperty(
                BOOL_PROPERTY, "use default epsilon", BoxplotGroupProperty);
    properties->mBool()->setValue(useDefaultEpsilonProperty, useDefaultEpsilon);

    // Properties for contour probability plot only.
    probabilityPlotGroupProperty =
            actor->addProperty(GROUP_PROPERTY, "contour probability plot",
                               groupProperty);

    innerPercentageProperty = actor->addProperty(
                DECORATEDDOUBLE_PROPERTY, "inner width",
                probabilityPlotGroupProperty);
    properties->setDDouble(innerPercentageProperty, innerPercentage, 0., 100.,
                           0, 5., " %");

    outerPercentageProperty =  actor->addProperty(
                DECORATEDDOUBLE_PROPERTY, "outermost width",
                probabilityPlotGroupProperty);
    properties->setDDouble(outerPercentageProperty, outerPercentage, 0., 50.,
                           0, 5., " %");

    drawoutermostProperty = actor->addProperty(BOOL_PROPERTY, "draw outermost",
                                               probabilityPlotGroupProperty);
    properties->mBool()->setValue(drawoutermostProperty, drawOutermost);

    outermostColourProperty = actor->addProperty(
                COLOR_PROPERTY, "outermost colour",
                probabilityPlotGroupProperty);
    properties->mColor()->setValue(outermostColourProperty, outermostColour);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MPlotCollection::ContourPlot::saveConfiguration(QSettings *settings)
{
    // Save general contour plot settings.
    settings->setValue("contourPlotInnerColour", innerColour);
    settings->setValue("contourPlotDrawOuter", drawOuter);
    settings->setValue("contourPlotOuterColour", outerColour);
    settings->setValue("contourPlotDrawMedian", drawMedian);
    settings->setValue("contourPlotMedianThickness", medianThickness);
    settings->setValue("contourPlotMedianColour", medianColour);
    // Save contour boxplot settings.
    settings->setValue("contourBoxplotDrawOutliers", drawOutliers);
    settings->setValue("contourBoxplotOutlierThickness", outlierThickness);
    settings->setValue("contourBoxplotOutlierColour", outlierColour);
    settings->setValue("contourBoxplotEpsilon", epsilon);
    settings->setValue("contourBoxplotUseDefaultEpsilon", useDefaultEpsilon);
    // Save contour probability plot settings.
    settings->setValue("contourProbabilityPlotInnerPercentage", innerPercentage);
    settings->setValue("contourProbabilityPlotOuterPercentage", outerPercentage);
    settings->setValue("contourProbabilityPlotDrawOutermost", drawOutermost);
    settings->setValue("contourProbabilityPlotOutermostColour", outermostColour);
}


void MPlotCollection::ContourPlot::loadConfiguration(
        QSettings *settings, MQtProperties *properties)
{
    // Load general contour plot settings.
    properties->mColor()->setValue(
                innerColourProperty,
                settings->value("contourPlotInnerColour",
                                QColor(176, 176, 218, 255)).value<QColor>());
    drawOuter = settings->value("contourPlotDrawOuter", true).toBool();
    properties->mBool()->setValue(drawOuterProperty, drawOuter);
    properties->mColor()->setValue(
                outerColourProperty,
                settings->value("contourPlotOuterColour",
                                QColor(220, 220, 253 , 255)).value<QColor>());
    drawMedian =
            settings->value("contourPlotDrawMedian", true).toBool();
    properties->mBool()->setValue(drawMedianProperty, drawMedian);
    medianThickness = settings->value("contourPlotMedianThickness",
                                      2.).toDouble();
    properties->mDouble()->setValue(medianThicknessProperty, medianThickness);
    properties->mColor()->setValue(
                medianColourProperty,
                settings->value("contourPlotMedianColour",
                                QColor(245, 245, 70, 255)).value<QColor>());
    // Load contour boxplot settings.
    drawOutliers = settings->value("contourBoxplotDrawOutliers", true).toBool();
    properties->mBool()->setValue(drawOutliersProperty, drawOutliers);
    outlierThickness = settings->value("contourBoxplotOutlierThickness",
                                       1.2).toDouble();
    properties->mDouble()->setValue(outlierThicknessProperty, outlierThickness);
    properties->mColor()->setValue(
                outlierColourProperty,
                settings->value("contourBoxplotOutlierColour",
                                QColor(227, 64, 64, 255)).value<QColor>());
    epsilon = settings->value("contourBoxplotEpsilon", 0.029).toDouble();
    properties->mDouble()->setValue(epsilonProperty, epsilon);
    useDefaultEpsilon = settings->value("contourBoxplotUseDefaultEpsilon",
                                        true).toBool();
    properties->mBool()->setValue(useDefaultEpsilonProperty, useDefaultEpsilon);
    // Load contour probability plot settings.
    innerPercentage = settings->value("contourProbabilityPlotInnerPercentage",
                                      50.).toDouble();
    properties->mDouble()->setValue(innerPercentageProperty, innerPercentage);
    outerPercentage = settings->value("contourProbabilityPlotOuterPercentage",
                                      10.).toDouble();
    properties->mDouble()->setValue(outerPercentageProperty, outerPercentage);
    drawOutermost = settings->value("contourProbabilityPlotDrawOutermost",
                                    true).toBool();
    properties->mBool()->setValue(drawoutermostProperty,
                                  drawOutermost);
    properties->mColor()->setValue(
                outermostColourProperty,
                settings->value("contourProbabilityPlotOutermostColour",
                                QColor(240, 240, 255, 255)).value<QColor>());
}


/******************************************************************************
***                             VariabilityPlot                             ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MPlotCollection::VariabilityPlot::VariabilityPlot(MActor *actor,
                                                  QtProperty *renderGroup)
    : colour(QColor(255, 130, 55, 255)),
      scale(1.0),
      drawMean(true),
      meanThickness(2.0),
      meanColour(QColor(187, 11, 14, 255)),
      // Distance variability plot.
      distanceNeedsRecompute(true),
      distanceScaleChanged(true),
      // Scalar variability plot.
      scalarNeedsRecompute(true),
      scalarMean(nullptr)
{
    MQtProperties *properties = actor->getQtProperties();

    groupProperty = actor->addProperty(GROUP_PROPERTY, "variability plot",
                                       renderGroup);

    colourProperty = actor->addProperty(COLOR_PROPERTY, "colour", groupProperty);
    properties->mColor()->setValue(colourProperty, colour);

    scaleProperty = actor->addProperty(DOUBLE_PROPERTY, "scale", groupProperty);
    properties->setDouble(scaleProperty, scale, 0.001, 1000., 3, 0.1);

    drawMeanProperty = actor->addProperty(
                BOOL_PROPERTY, "draw mean", groupProperty);
    properties->mBool()->setValue(drawMeanProperty, drawMean);

    meanThicknessProperty = actor->addProperty(
                DOUBLE_PROPERTY, "mean thickness", groupProperty);
    properties->setDouble(meanThicknessProperty, meanThickness, 0.1, 10., 1,
                          0.1);

    meanColourProperty = actor->addProperty(COLOR_PROPERTY, "mean colour",
                                            groupProperty);
    properties->mColor()->setValue(meanColourProperty, meanColour);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MPlotCollection::VariabilityPlot::saveConfiguration(QSettings *settings)
{
    settings->setValue("variabilityPlotColour", colour);
    settings->setValue("variabilityPlotScale", scale);
    settings->setValue("variabilityPlotShowMean", drawMean);
    settings->setValue("variabilityPlotMeanThickness", meanThickness);
    settings->setValue("variabilityPlotMeanColour", meanColour);
}


void MPlotCollection::VariabilityPlot::loadConfiguration(
        QSettings *settings, MQtProperties *properties)
{
    properties->mColor()->setValue(
                colourProperty,
                settings->value("variabilityPlotColour",
                                QColor(255, 130, 55, 255)).value<QColor>());
    scale = settings->value("variabilityPlotScale", 1.).toDouble();
    properties->mDouble()->setValue(scaleProperty, scale);
    drawMean = settings->value("variabilityPlotDrawMean", true).toBool();
    properties->mBool()->setValue(drawMeanProperty, drawMean);
    meanThickness = settings->value("variabilityPlotMeanThickness",
                                    2.).toDouble();
    properties->mDouble()->setValue(meanThicknessProperty, meanThickness);
    properties->mColor()->setValue(
                meanColourProperty,
                settings->value("variabilityPlotMeanColour",
                                QColor(187, 11, 14, 255)).value<QColor>());
}


} // namespace Met3D
