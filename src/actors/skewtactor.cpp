/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2019 Marc Rautenhaus [*, previously +]
**  Copyright 2015-2016 Christoph Heidelmann [+]
**  Copyright 2018      Bianca Tost [+]
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
**
**  + Computer Graphics and Visualization Group
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
#include "skewtactor.h"

// standard library imports
#include <iostream>
#include <math.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/selectdatasourcedialog.h"
#include "data/structuredgrid.h"
#include <log4cplus/loggingmacros.h>
#include "util/metroutines.h"
#include <QElapsedTimer>

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/
MSkewTActor::MSkewTActor() : MNWPMultiVarActor(),
    vbDiagramVertices(nullptr),
    vbHighlightVertices(nullptr),
    vbWyomingVertices(nullptr),
    wyomingVerticesCount(0),
    offsetPickPositionToHandleCentre(QVector2D(0., 0.)),
    dragEventActive(false)
{
    enablePicking(true);
    setActorSupportsFullScreenVisualisation(true);

    beginInitialiseQtProperties();

    setActorType(staticActorType());
    setName(getActorType());

    QString celsiusUnit = QString::fromUtf8(" \u00B0C");

    // General actor properties.
    // =========================
    appearanceGroupProperty = addProperty(
                GROUP_PROPERTY, "appearance", actorPropertiesSupGroup);

    alignWithCameraProperty = addProperty(
                BOOL_PROPERTY, "align with camera",
                appearanceGroupProperty);
    properties->mBool()->setValue(alignWithCameraProperty, true);

    diagramWidth3DProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "3D diagram width",
                appearanceGroupProperty);
    properties->setDDouble(diagramWidth3DProperty, 15., 0., 360., 2, 1., "");

    bottomPressureProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "pressure bottom",
                appearanceGroupProperty);
    properties->setDDouble(bottomPressureProperty, 1050., 0.1, 1050., 2, 10.,
                           " hPa");
    topPressureProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "pressure top",
                appearanceGroupProperty);
    properties->setDDouble(topPressureProperty, 20., 0.1, 1050., 2, 10.,
                           " hPa");
    temperatureMinProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "temperature min",
                appearanceGroupProperty);
    properties->setDDouble(temperatureMinProperty, -60., -100., 20., 0, 10.,
                           celsiusUnit);
    temperatureMaxProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "temperature max",
                appearanceGroupProperty);
    properties->setDDouble(temperatureMaxProperty, 60., 20., 100., 0, 10.,
                           celsiusUnit);
    isothermsSpacingProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "isotherms spacing",
                appearanceGroupProperty);
    properties->setDDouble(isothermsSpacingProperty, 10., 1., 100., 0, 1.,
                           celsiusUnit);
    skewFactorProperty= addProperty(
                DECORATEDDOUBLE_PROPERTY, "skew factor",
                appearanceGroupProperty);
    properties->setDDouble(skewFactorProperty, 1., 0., 2., 2, 0.1, " (0..2)");

    // Dry adiabates.
    // ==============
    drawDryAdiabatesProperty = addProperty(
                BOOL_PROPERTY, "draw dry adiabates", appearanceGroupProperty);
    properties->mBool()->setValue(drawDryAdiabatesProperty, true);
    dryAdiabatesSpacingProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "dry adiabates spacing",
                appearanceGroupProperty);
    properties->setDDouble(dryAdiabatesSpacingProperty, 10., 1., 100., 0, 1.,
                           celsiusUnit);

    // Moist adiabates.
    // ================
    drawMoistAdiabatesProperty = addProperty(
                BOOL_PROPERTY, "draw moist adiabates", appearanceGroupProperty);
    properties->mBool()->setValue(drawMoistAdiabatesProperty, true);
    moistAdiabatesSpcaingProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "moist adiabates spacing",
                appearanceGroupProperty);
    properties->setDDouble(moistAdiabatesSpcaingProperty, 10., 1., 100., 0, 1.,
                           celsiusUnit);


    // Geographical position of profile.
    // =================================

    QPointF initialPosition = QPointF(0., 0.);
    geoPositionProperty = addProperty(
                POINTF_LONLAT_PROPERTY, "position", actorPropertiesSupGroup);
    properties->mPointF()->setValue(geoPositionProperty, initialPosition);

    // Keep an instance of a MMovablePoleActor as a "subactor" to draw the
    // pole at which the vertical profile is taken.
    poleActor = new MMovablePoleActor();
    poleActor->setName("profile pole");
    poleActor->addPole(initialPosition);
    poleActor->enablePoleProperties(false); // only allow a single pole
    poleActor->setTubeRadius(0.2);
    poleActor->setVerticalExtent(1050., 20.);
    poleActor->setTicksOnRightSide(false);
    actorPropertiesSupGroup->addSubProperty(poleActor->getPropertyGroup());


// TODO (bt, 03Sep2018) Disabled for now, needs to be updated to work again.
//    // Request observational data from University of Wyoming.
//    // ======================================================
//    groupWyoming = addProperty(
//                GROUP_PROPERTY, "Obs Data UofWyoming", actorPropertiesSupGroup);
//    wyomingStationsProperty = addProperty(
//                ENUM_PROPERTY, "Wyoming Station", groupWyoming);
//    enableWyomingProperty = addProperty(
//                BOOL_PROPERTY, "enabled", groupWyoming);
//    properties->mBool()->setValue(enableWyomingProperty, false);
//    dewPointColorWyomingProperty = addProperty(
//                COLOR_PROPERTY, "dew-point colour", groupWyoming);
//    properties->mColor()->setValue(dewPointColorWyomingProperty,
//                                   QColor(255, 255, 0, 255));
//    temperatureColorWyomingProperty = addProperty(
//                COLOR_PROPERTY, "temperature colour", groupWyoming);
//    properties->mColor()->setValue(temperatureColorWyomingProperty,
//                                   QColor(255, 0, 80, 255));

    endInitialiseQtProperties();

    copyDiagramConfigurationFromQtProperties();

    loadListOfAvailableObservationsFromUWyoming();
}


MSkewTActor::~MSkewTActor()
{
    if (vbWyomingVertices)
    {
        delete vbWyomingVertices;
    }
    if (vbDiagramVertices)
    {
        delete vbDiagramVertices;
    }
    if (vbHighlightVertices)
    {
        delete vbHighlightVertices;
    }

//TODO (mr, 20Feb2019) -- Deleting the poleActor causes a segmentation fault.. why?
    // delete poleActor;

    removeAllSkewTLabels();
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MSkewTActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);

    beginCompileShaders(1);

    compileShadersFromFileWithProgressDialog(
                skewTShader, "src/glsl/skewtrendering.fx.glsl");

    endCompileShaders();
}


void MSkewTActor::saveConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::saveConfiguration(settings);

    settings->beginGroup(MSkewTActor::getSettingsID());

    settings->setValue("bottomPressure",
                       properties->mDDouble()->value(bottomPressureProperty));
    settings->setValue("topPressure",
                       properties->mDDouble()->value(topPressureProperty));

    settings->setValue("minTemperature",
                       properties->mDDouble()->value(temperatureMinProperty));
    settings->setValue("maxTemperature",
                       properties->mDDouble()->value(temperatureMaxProperty));

    settings->setValue("skewFactor",
                       properties->mDDouble()->value(skewFactorProperty));

    settings->setValue("isothermsSpacing",
                       properties->mDDouble()->value(isothermsSpacingProperty));

    settings->setValue("alignWithCamera",
                       properties->mBool()->value(
                           alignWithCameraProperty));
    settings->setValue("diagramWidth3D",
                       properties->mDDouble()->value(
                           diagramWidth3DProperty));

    settings->setValue("position",
                       properties->mPointF()->value(geoPositionProperty));

    settings->setValue("moistAdiabatesSpacing",
                       properties->mDDouble()->value(moistAdiabatesSpcaingProperty));
    settings->setValue("moistAdiabatesEnabled",
                       properties->mBool()->value(drawMoistAdiabatesProperty));

    settings->setValue("dryAdiabatesSpacing",
                       properties->mDDouble()->value(
                           dryAdiabatesSpacingProperty));
    settings->setValue("dryAdiabatesEnabled",
                       properties->mBool()->value(drawDryAdiabatesProperty));

    for (int i = 0; i < diagramConfiguration.varConfigs.size(); i++)
    {
        VariableConfig var = diagramConfiguration.varConfigs.at(i);
        settings->setValue(QString("%1VariableIndex").arg(i), var.index);
        settings->setValue(QString("%1VariableColor").arg(i), var.color);
    }

    settings->setValue("temperatureShowMinMaxProperty",
                       properties->mBool()->value(
                       temperatureShowProbabilityTubeProperty));
    settings->setValue("temperatureShowDeviationProperty",
                       properties->mBool()->value(
                           temperatureShowDeviationTubeProperty));
    settings->setValue("temperatureMinMaxColourProperty",
                       properties->mColor()->value(
                           temperatureMinMaxVariableColorProperty));

    settings->setValue("humidityShowMinMaxProperty",
                       properties->mBool()->value(
                       dewPointShowProbabilityTubeProperty));
    settings->setValue("humidityShowDeviationProperty",
                       properties->mBool()->value(
                           dewPointShowDeviationTubeProperty));
    settings->setValue("humidityMinMaxColourProperty",
                       properties->mColor()->value(
                           dewPointMinMaxVariableColorProperty));

    poleActor->saveConfiguration(settings);
    settings->endGroup();
}


void MSkewTActor::loadConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::loadConfiguration(settings);

    enableActorUpdates(false);

    settings->beginGroup(MSkewTActor::getSettingsID());

    poleActor->loadConfiguration(settings);
    poleActor->enablePoleProperties(false); // only allow a single pole

    properties->mDDouble()->setValue(
        bottomPressureProperty,
        settings->value("bottomPressure", 1050.).toDouble());
    properties->mDDouble()->setValue(
        topPressureProperty,
        settings->value("topPressure", 20.).toDouble());


    properties->mDDouble()->setValue(
        temperatureMinProperty,
        settings->value("minTemperature", -60.).toDouble());
    properties->mDDouble()->setValue(
        temperatureMaxProperty,
        settings->value("maxTemperature", 60.).toDouble());

    properties->mDDouble()->setValue(
        skewFactorProperty,
        settings->value("skewFactor", 1.).toDouble());

    properties->mDDouble()->setValue(
                isothermsSpacingProperty,
                settings->value("isothermsSpacing", 10.).toDouble());

    properties->mBool()->setValue(
                alignWithCameraProperty,
                settings->value("alignWithCamera", true).toBool());
    properties->mDDouble()->setValue(
                diagramWidth3DProperty,
                settings->value("diagramWidth3D", 15.).toDouble());

    properties->mPointF()->setValue(
                geoPositionProperty,
                settings->value("position").toPointF());

    properties->mDDouble()->setValue(
        moistAdiabatesSpcaingProperty,
        settings->value("moistAdiabatesSpacing", 10.).toDouble());
    properties->mBool()->setValue(
        drawMoistAdiabatesProperty,
        settings->value("moistAdiabatesEnabled", true).toBool());

    properties->mDDouble()->setValue(
                dryAdiabatesSpacingProperty,
                settings->value("dryAdiabatesSpacing", 10.).toDouble());
    properties->mBool()->setValue(
                drawDryAdiabatesProperty,
                settings->value("dryAdiabatesEnabled", true).toBool());

    for (int i = 0; i < diagramConfiguration.varConfigs.size(); i++)
    {
        int index = settings->value(
                       QString("%1VariableIndex").arg(i)).toInt();
        QColor color = settings->value(QString("%1VariableColor").arg(i),
                                       QColor(0, 0, 0, 255)).value<QColor>();

        VariableConfig *var = &diagramConfiguration.varConfigs.data()[i];
        var->color = color;
        var->index = index;
        properties->mEnum()->setEnumNames(var->property, varNameList);
        properties->mEnum()->setValue(var->property, index);
        if (index > 0 && index <= variables.size())
        {
            var->variable = dynamic_cast<MNWPSkewTActorVariable*>(
                        variables.at(index - 1));
            properties->mColor()->setValue(var->variable->profileColourProperty, color);
        }
    }

    properties->mBool()->setValue(
                temperatureShowProbabilityTubeProperty,
                settings->value("temperatureShowMinMaxProperty", true).toBool());
    properties->mBool()->setValue(
                temperatureShowDeviationTubeProperty,
                settings->value("temperatureShowDeviationProperty",
                                true).toBool());
    properties->mColor()->setValue(
                temperatureMinMaxVariableColorProperty,
                settings->value("temperatureMinMaxColourProperty",
                                QColor(201, 10, 5, 255)).value<QColor>());

    properties->mBool()->setValue(
                dewPointShowProbabilityTubeProperty,
                settings->value("humidityShowMinMaxProperty", true).toBool());
    properties->mBool()->setValue(
                dewPointShowDeviationTubeProperty,
                settings->value("humidityShowDeviationProperty", true).toBool());
    properties->mColor()->setValue(
                dewPointMinMaxVariableColorProperty,
                settings->value("humidityMinMaxColourProperty",
                                QColor(17, 98, 208, 255)).value<QColor>());

    settings->endGroup();
    copyDiagramConfigurationFromQtProperties();
    updateVerticalProfilesAndLabels();
    enableActorUpdates(true);
}


const QList<MVerticalLevelType> MSkewTActor::supportedLevelTypes()
{
    return (QList<MVerticalLevelType>() <<
            HYBRID_SIGMA_PRESSURE_3D <<
            PRESSURE_LEVELS_3D);
}


MNWPActorVariable *MSkewTActor::createActorVariable(
        const MSelectableDataSource &dataSource)
{
    MNWPSkewTActorVariable *newVar = new MNWPSkewTActorVariable(this);

    newVar->dataSourceID = dataSource.dataSourceID;
    newVar->levelType    = dataSource.levelType;
    newVar->variableName = dataSource.variableName;

    return newVar;
}


int MSkewTActor::checkIntersectionWithHandle(
        MSceneViewGLWidget *sceneView, float clipX, float clipY)
{
    if (sceneViewFullscreenEnabled.value(sceneView))
    {
        // In fullscreen mode, this function implements a classical "mouse
        // move event" that determines the (T,p) coordinate at which the mouse
        // pointer currently hovers and generates some geometry (isobaric,
        // isothermal etc. lines) that highlight the current coordinate.

        // Compute (T,p) coordinate from clip space (x,y) coordinate.
        QVector2D clipSpaceCoordinate = QVector2D(clipX, clipY);
        QVector2D xyCoordinate = transformClipSpace2xy(clipSpaceCoordinate);
        QVector2D tpCoordinate = transformXY2Tp(xyCoordinate);

        // Generate highlight geometry.
        generateFullScreenHighlightGeometry(tpCoordinate,
                                            &vbHighlightVertices,
                                            &skewTDiagramConfiguration);
    }
    else
    {
        return poleActor->checkIntersectionWithHandle(sceneView, clipX, clipY);
    }
    return -1;
}


void MSkewTActor::addPositionLabel(
        MSceneViewGLWidget *sceneView, int handleID, float clipX, float clipY)
{
    if (!sceneViewFullscreenEnabled.value(sceneView))
    {
        poleActor->addPositionLabel(sceneView, handleID, clipX, clipY);
    }
}


void MSkewTActor::dragEvent(
     MSceneViewGLWidget *sceneView, int handleID, float clipX, float clipY)
{
    if (!sceneViewFullscreenEnabled.value(sceneView))
    {
        dragEventActive = true;

        poleActor->dragEvent(sceneView, handleID, clipX, clipY);

        properties->mPointF()->setValue(
                    geoPositionProperty,
                    poleActor->getPoleVertices().at(0).toPointF());

        dragEventActive = false;
    }
}


QList<MLabel*> MSkewTActor::getPositionLabelToRender()
{
    return poleActor->getPositionLabelToRender();
}


void MSkewTActor::removePositionLabel()
{
    poleActor->removePositionLabel();
}


void MSkewTActor::onFullScreenModeSwitch(MSceneViewGLWidget *sceneView,
                                         bool fullScreenEnabled)
{
    sceneViewFullscreenEnabled.insert(sceneView, fullScreenEnabled);
}


/******************************************************************************
***                             PROTECTED SLOTS                             ***
*******************************************************************************/

void MSkewTActor::downloadOfObservationFromUWyomingFinished(
        QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        LOG4CPLUS_DEBUG(mlog, "Error in " << reply->url().toString().toStdString()
                        << ":" << reply->errorString().toStdString() << flush);
        return;
    }

    QByteArray data = reply->readAll();
    std::string html(QString(data).toStdString());

    float lat = QString(html.substr(
                            html.find("Station latitude") +
                            strlen("Station latitude: "),
                            6).c_str()).toFloat();
    float lon = QString(html.substr(
                            html.find("Station longitude") +
                            strlen("Station longitude: "),
                            6).c_str()).toFloat();

    html = html.substr(html.find("<PRE>") + 5,
                       html.find("</PRE>") - html.find("<PRE>"));
    QStringList lines = QString(html.c_str()).split("\n");
    QVector<QVector3D> verticesForBuffer;
    int count = 0;
    for (int i = 5; i < lines.count(); i++)
    {
        QString cleaned     = lines.at(i);
        cleaned             = cleaned.replace(QRegExp("[ ]{1,}"), " ");
        cleaned             = cleaned.trimmed();
        QStringList values  = cleaned.split(" ");
        if (values.count() > 5)
        {
            count++;
        }
    }
    QVector3D vertices[count];

    for (int i = 5; i < lines.count(); i++)
    {
        QString cleaned     = lines.at(i);
        cleaned             = cleaned.replace(QRegExp("[ ]{1,}"), " ");
        cleaned             = cleaned.trimmed();
        QStringList values  = cleaned.split(" ");
        if (values.count() > 5)
        {
            // Temperature
            vertices[i - 5].setX(values.at(2).toFloat() + 273.15f);
            // Pressure
            vertices[i - 5].setY(values.at(0).toFloat());
            // DewPoint
            vertices[i - 5].setZ(values.at(3).toFloat() + 273.15f);
        }
    }
    for (int i = 0; i < count; i++)
    {
        verticesForBuffer.append(vertices[i]);
    }
    uploadVec3ToVertexBuffer(verticesForBuffer,
                             QString("wyomingVertices_actor#%1").arg(myID),
                             &vbWyomingVertices);
    wyomingVerticesCount = verticesForBuffer.count();
    properties->mPointF()->setValue(geoPositionProperty, QPointF(lon, lat));
}


void MSkewTActor::downloadOfObservationListFromUWyomingFinished(
        QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        LOG4CPLUS_DEBUG(mlog, "Error in "
                        << reply->url().toString().toStdString()
                        << ":" << reply->errorString().toStdString() << flush);
        return;
    }

    QByteArray data = reply->readAll();
    std::string html(QString(data).toStdString());

    int hasResponse =
        html.find("<MAP NAME=\"raob\">");
    if (hasResponse == -1)
    {
        return;
    }
    html = html.substr(
               html.find("<MAP NAME=\"raob\">") +
               strlen("<MAP NAME=\"raob\">"),
               html.find("</MAP>") -
               html.find("<MAP NAME=\"raob\">"));

    QStringList lines = QString(
                            html.c_str()).split("\n");
    QStringList names;
    for (int i = 0; i < lines.count(); i++)
    {
        QString cleaned = lines.at(i);
        cleaned = cleaned.trimmed();
        QStringList firstSplit =
            cleaned.split("return s('");
        if (firstSplit.count() > 1)
        {
            QString second = firstSplit.at(1);
            QStringList rest = second.split("  ");
            if (rest.count() > 1)
            {
                QString partName = rest.at(1);
                std::string name = partName.toStdString();
                name = name.substr(0, name.find("')"));
                wyomingStations.append(rest.at(0).toInt());
                names.append(QString(name.c_str()));
            }
        }
    }
    properties->mEnum()->setEnumNames(
        wyomingStationsProperty, names);
    properties->mEnum()->setValue(
        wyomingStationsProperty, 0);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MSkewTActor::initializeActorResources()
{
    MNWPMultiVarActor::initializeActorResources();

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    bool loadShaders = false;
    loadShaders |= glRM->generateEffectProgram("skewtrendering", skewTShader);

    if (loadShaders) reloadShaderEffects();

    poleActor->initialize();

    copyDiagramConfigurationFromQtProperties();
    generateDiagramGeometry(&vbDiagramVertices, &skewTDiagramConfiguration);

    LOG4CPLUS_DEBUG(mlog, "done");
}


void MSkewTActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    drawDiagram3DView(sceneView);
}


void MSkewTActor::renderToCurrentFullScreenContext(MSceneViewGLWidget *sceneView)
{
    drawDiagramFullScreen(sceneView);
}


void MSkewTActor::dataFieldChangedEvent()
{
    emitActorChangedSignal();
}


void MSkewTActor::onQtPropertyChanged(QtProperty *property)
{
    MNWPMultiVarActor::onQtPropertyChanged(property);

    if (suppressActorUpdates()) return;

    if (property == wyomingStationsProperty
             || property == enableWyomingProperty)
    {
        if (properties->mBool()->value(enableWyomingProperty))
        {
            int index = properties->mEnum()->value(wyomingStationsProperty);
            if (index != -1)
            {
                loadObservationalDataFromUWyoming(wyomingStations.at(index));
            }
        }
        else
        {
            wyomingVerticesCount = 0;
        }
        emitActorChangedSignal();
    }
    else if (property == diagramWidth3DProperty
             || property == alignWithCameraProperty)
    {
        copyDiagramConfigurationFromQtProperties();
        emitActorChangedSignal();
    }
    else if (property == temperatureMaxProperty
             || property == temperatureMinProperty
             || property == isothermsSpacingProperty
             || property == moistAdiabatesSpcaingProperty
             || property == dryAdiabatesSpacingProperty
             || property == bottomPressureProperty
             || property == topPressureProperty
             || property == skewFactorProperty)
    {
        diagramConfiguration.regenerateAdiabates = true;
        skewTDiagramConfiguration.recomputeAdiabateGeometries = true;

        copyDiagramConfigurationFromQtProperties();
        poleActor->setVerticalExtent(diagramConfiguration.vertical_p_hPa.max,
                                     diagramConfiguration.vertical_p_hPa.min);

        generateDiagramGeometry(&vbDiagramVertices,
                                &skewTDiagramConfiguration);
        emitActorChangedSignal();
    }
    else if (property == labelSizeProperty
             || property == labelColourProperty
             || property == labelBBoxProperty
             || property == labelBBoxColourProperty)
    {
        if (suppressActorUpdates()) return;
        generateDiagramGeometry(&vbDiagramVertices,
                                &skewTDiagramConfiguration);
        emitActorChangedSignal();
    }
    else if (property == geoPositionProperty)
    {
        diagramConfiguration.geoPosition = QVector2D(
                    float(properties->mPointF()->value(geoPositionProperty).x()),
                    float(properties->mPointF()->value(geoPositionProperty).y()));
        updateVerticalProfilesAndLabels();
        emitActorChangedSignal();
    }
    else if (property == drawDryAdiabatesProperty)
    {
        diagramConfiguration.drawDryAdiabates =
                properties->mBool()->value(drawDryAdiabatesProperty);

        if (diagramConfiguration.drawDryAdiabates)
        {
            // Regenerate dry adiabates only if necessary (first time, pressure
            // drawing type, temperature scale)
            if ((skewTDiagramConfiguration
                 .vertexArrayDrawRanges.dryAdiabates.indexCount == 0)
                    || skewTDiagramConfiguration.recomputeAdiabateGeometries)
            {
                generateDiagramGeometry(&vbDiagramVertices,
                                        &skewTDiagramConfiguration);
            }
        }
        emitActorChangedSignal();
    }
    else if (property == drawMoistAdiabatesProperty)
    {
        diagramConfiguration.drawMoistAdiabates =
                properties->mBool()->value(drawMoistAdiabatesProperty);

        if (diagramConfiguration.drawMoistAdiabates)
        {
            // Regenerate moist adiabates only if necessary (first time,
            // pressure drawing type, temperature scale)
            if ((skewTDiagramConfiguration
                 .vertexArrayDrawRanges.moistAdiabates.indexCount == 0)
                    || skewTDiagramConfiguration.recomputeAdiabateGeometries)
            {
                generateDiagramGeometry(&vbDiagramVertices,
                                        &skewTDiagramConfiguration);
            }
        }
        emitActorChangedSignal();
    }
    else
    {
        for (int i = 0; i < diagramConfiguration.varConfigs.size(); i++)
        {
            VariableConfig *var = &diagramConfiguration.varConfigs[i];
            int index = properties->mEnum()->value(var->property);
            var->index = index;
            if (var->index <= 0)
            {
                if (property == var->property)
                {
                    var->variable = nullptr;
                    emitActorChangedSignal();
                    return;
                }
                continue;
            }
            if (property == var->property
                    || property == var->variable->profileColourProperty
                    || property == var->variable->lineThicknessProperty)
            {
                var->variable = dynamic_cast<MNWPSkewTActorVariable*>(
                            variables.at(var->index - 1));
                var->color = var->variable->profileColour;
                var->thickness = var->variable->lineThickness;
                emitActorChangedSignal();
                return;
            }
        }
    }
}


void MSkewTActor::printDebugOutputOnUserRequest()
{
    // Debug output to verify Tp->xy transformation matrix.
    QString str = "\nDEBUG output for verification of (T,p)->(x,y) "
                  "transformation:\n\n";
    for (float t = 243.15; t <= 313.15; t += 10.)
    {
        for (float p = 1050.; p >= 100.; p -= 50.)
        {
            QVector2D tp(t, p);
            QVector2D xp = transformTp2xy(tp);
            str += QString("T=%1, p=%2 --> x=%3, p=%4\n")
                    .arg(tp.x()).arg(tp.y()).arg(xp.x()).arg(xp.y());
        }
    }

    // Print (T, p) profile at current (lon, lat) position.
    float lon = diagramConfiguration.geoPosition.x();
    float lat = diagramConfiguration.geoPosition.y();
    str += "\n\n\nDEBUG output, vertical (T, p) profile at current location: ";
    str += QString("(%1, %2).\n").arg(lon).arg(lat);

    for (int vi = 0; vi < variables.size(); vi++)
    {
        MNWPSkewTActorVariable* var =
                static_cast<MNWPSkewTActorVariable*> (variables.at(vi));

        if ( !var->hasData() ) continue;

        QVector<QVector2D> profile = var->grid->extractVerticalProfile(lon, lat);

        str += QString("\nGRID: %1\n").arg(var->grid->getGeneratingRequest());
        for (QVector2D tpCoordinate : profile)
        {
            str += QString("T=%1, p=%2\n")
                    .arg(tpCoordinate.x()).arg(tpCoordinate.y());
        }
    }

    str += "DEBUG output end.\n";
    LOG4CPLUS_DEBUG(mlog, str.toStdString());
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MSkewTActor::copyDiagramConfigurationFromQtProperties()
{
    diagramConfiguration.alignWithCamera =
            properties->mBool()->value(alignWithCameraProperty);
    diagramConfiguration.diagramWidth3D =
            properties->mDDouble()->value(diagramWidth3DProperty);
    diagramConfiguration.geoPosition = QVector2D(
                properties->mPointF()->value(geoPositionProperty).x(),
                properties->mPointF()->value(geoPositionProperty).y());
    diagramConfiguration.temperature_degC.min =
            properties->mDDouble()->value(temperatureMinProperty);
    diagramConfiguration.temperature_degC.max =
            properties->mDDouble()->value(temperatureMaxProperty);
    diagramConfiguration.vertical_p_hPa.min =
            properties->mDDouble()->value(topPressureProperty);
    diagramConfiguration.vertical_p_hPa.max =
            properties->mDDouble()->value(bottomPressureProperty);
    diagramConfiguration.skewFactor =
            properties->mDDouble()->value(skewFactorProperty);
    diagramConfiguration.isothermSpacing =
            properties->mDDouble()->value(isothermsSpacingProperty);
    diagramConfiguration.drawDryAdiabates =
            properties->mBool()->value(drawDryAdiabatesProperty);
    diagramConfiguration.drawMoistAdiabates =
            properties->mBool()->value(drawMoistAdiabatesProperty);
    diagramConfiguration.moistAdiabatSpacing =
            properties->mDDouble()->value(moistAdiabatesSpcaingProperty);
    diagramConfiguration.dryAdiabatSpacing =
            properties->mDDouble()->value(dryAdiabatesSpacingProperty);

    skewTDiagramConfiguration.pressureEqualsWorldPressure =
            diagramConfiguration.alignWithCamera;

    diagramConfiguration.init();
    skewTDiagramConfiguration.init(&diagramConfiguration, "_normal");

    // After the configuration has been copied from the properties, recompute
    // the (T, log(p)) to (x, y) transformation matrix to transform (T, p)
    // coordinates into (x, y) coordinates.
    computeTlogp2xyTransformationMatrix();
}


void MSkewTActor::generateDiagramGeometry(
        GL::MVertexBuffer** vbDiagramVertices,
        ModeSpecificDiagramConfiguration *config)
{
    // Clear list of existing labels and get settings for label display.
    removeAllSkewTLabels();

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    MTextManager* tm = glRM->getTextManager();
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);

    // Array with vertex data that will be uploaded to a vertex buffer at the
    // end of the method. Contains line segments to be rendered with GL_LINES.
    // NOTE: All geometry stored in this array needs to be mapped to 2D diagram
    // coordinates (0..1) x (0..1)!
    QVector<QVector2D> vertexArray;

//TODO (mr, 11Jan2019) -- most geometry generated in this method could make
//    use of line strips (whould make rendering more efficient).

    // Temporary variables for start and end vertices for a line segment,
    // reused throughout the method.
    QVector2D vStart, vEnd;

    // Generate vertices for diagram frame.
    // ====================================
    config->vertexArrayDrawRanges.frame.startIndex = vertexArray.size();

    vertexArray << QVector2D(0.0, 0.0) << QVector2D(0.0, 1.0);
    vertexArray << QVector2D(1.0, 0.0) << QVector2D(1.0, 1.0);
    vertexArray << QVector2D(0.0, 0.0) << QVector2D(1.0, 0.0);
    vertexArray << QVector2D(0.0, 1.0) << QVector2D(1.0, 1.0);

    config->vertexArrayDrawRanges.frame.indexCount =
            vertexArray.size() - config->vertexArrayDrawRanges.frame.startIndex;


    // Generate vertices for isobars.
    // ==============================
    config->vertexArrayDrawRanges.isobars.startIndex = vertexArray.size();

    QList<int> pressureLevels;
    for (QVector3D axisTick : poleActor->getAxisTicks())
    {
        // Draw isobaric lines at tick positions of associated pole actor.
        pressureLevels.append(axisTick.z());
    }
    for (int pLevel_hPa : pressureLevels)
    {
        if ((pLevel_hPa < diagramConfiguration.vertical_p_hPa.max) &&
                (pLevel_hPa > diagramConfiguration.vertical_p_hPa.min))
        {
            // We need some temperature for the transformation, not used
            // any further.
            QVector2D tpCoordinate(273.15, pLevel_hPa);
            QVector2D xyCoordinate = transformTp2xy(tpCoordinate);
            vStart = QVector2D(0., xyCoordinate.y());
            vEnd = QVector2D(1., xyCoordinate.y());
            vertexArray << vStart << vEnd;

            // Add label for this isobaric line.
            QVector2D clipSpaceCoordinate = transformXY2ClipSpace(vStart);
            labelsFullScreen.append(
                        tm->addText(
                            QString("%1").arg(pLevel_hPa),
                            MTextManager::CLIPSPACE,
                            clipSpaceCoordinate.x(), clipSpaceCoordinate.y(),
                            -0.99,
                            labelsize, labelColour, MTextManager::MIDDLECENTRE,
                            labelbbox, labelBBoxColour)
                        );
            // We could generate labels for the 3D scene at this place;
            // currently the labels provided by the pole are used.
            // labels3D.append(
            //             tm->addText(
            //                 QString("%1").arg(pLevel_hPa),
            //                 MTextManager::LONLATP,
            //                 diagramConfiguration.geoPosition.x(),
            //                 diagramConfiguration.geoPosition.y(),
            //                 pLevel_hPa,
            //                 labelsize, labelColour, MTextManager::MIDDLERIGHT,
            //                 labelbbox, labelBBoxColour)
            //             );
        }
    }

    config->vertexArrayDrawRanges.isobars.indexCount =
            vertexArray.size() - config->vertexArrayDrawRanges.isobars.startIndex;


    // Generate vertices for isotherms.
    // ================================
    config->vertexArrayDrawRanges.isotherms.startIndex = vertexArray.size();

    float diagramTmin_K = config->dconfig->temperature_degC.min + 273.15;
    float diagramTmax_K = config->dconfig->temperature_degC.max + 273.15;
    float diagramTRange_K = diagramTmax_K - diagramTmin_K;
    float skewFactor = config->dconfig->skewFactor;

    // If the diagram is drawn skewed, the isotherms need to continue over the
    // minimum temperature limit of the diagram. The used factor is a heuristic,
    // might need to be adjusted later. Note that the offset needs to be a
    // multiple of the isotherm spacing value so that the temperature values
    // of the isotherms are not affected by the scaling factor.
    float tMinOffset = ceil((skewFactor * diagramTRange_K) /
                            config->dconfig->isothermSpacing) *
            config->dconfig->isothermSpacing;
    for (float isothermTemperature_K = diagramTmin_K - tMinOffset;
         isothermTemperature_K <= diagramTmax_K;
         isothermTemperature_K += config->dconfig->isothermSpacing)
    {
        // Generate vertex at (isotherm temperature, bottom pressure).
        QVector2D tpCoordinate_K_hPa = QVector2D(
                    isothermTemperature_K, config->dconfig->vertical_p_hPa.min);
        vStart = transformTp2xy(tpCoordinate_K_hPa);
        // Generate vertex at (isotherm temperature, top pressure).
        tpCoordinate_K_hPa.setY(config->dconfig->vertical_p_hPa.max);
        vEnd = transformTp2xy(tpCoordinate_K_hPa);
        vertexArray << vStart << vEnd;

        // Add label for this isotherm at bottom and top of diagram.
        QVector2D clipSpaceCoordinate = transformXY2ClipSpace(vEnd);
        labelsFullScreen.append(
                    tm->addText(
                        QString("%1").arg(round(isothermTemperature_K - 273.15)),
                        MTextManager::CLIPSPACE,
                        clipSpaceCoordinate.x(), clipSpaceCoordinate.y(),
                        -0.99,
                        labelsize, labelColour, MTextManager::UPPERCENTRE,
                        labelbbox, labelBBoxColour)
                    );
        clipSpaceCoordinate = transformXY2ClipSpace(vStart);
        labelsFullScreen.append(
                    tm->addText(
                        QString("%1").arg(round(isothermTemperature_K - 273.15)),
                        MTextManager::CLIPSPACE,
                        clipSpaceCoordinate.x(), clipSpaceCoordinate.y(),
                        -0.99,
                        labelsize, labelColour, MTextManager::LOWERCENTRE,
                        labelbbox, labelBBoxColour)
                    );
    }

    config->vertexArrayDrawRanges.isotherms.indexCount =
            vertexArray.size() - config->vertexArrayDrawRanges.isotherms.startIndex;


    // Generate vertices for dry adiabates.
    // ====================================

    float log_pBot = log(config->dconfig->vertical_p_hPa.max);
    float log_pTop = log(config->dconfig->vertical_p_hPa.min);
    int nAdiabatPoints = 100; // number of discrete points to plot adiabat
    float deltaLogP = (log_pBot - log_pTop) / float(nAdiabatPoints);

    config->vertexArrayDrawRanges.dryAdiabates.startIndex = vertexArray.size();

    if (diagramConfiguration.drawDryAdiabates)
    {
        // Create dry adiabates only if necessary (first time, pressure
        // drawing type, temperature scale, top and bottom pressure changed).
        if (config->vertexArrayDrawRanges.dryAdiabates.indexCount == 0
                || config->recomputeAdiabateGeometries)
        {
            config->dryAdiabatesVertices.clear();

            // To fill the diagram with dry adiabates, we need to continue over
            // the maximimum temperature limit. Again the used factor is a
            // heuristic value.
            for (float adiabatTemperature = diagramTmin_K;
                 adiabatTemperature <= diagramTmax_K + (
                     2 - skewFactor + 3) * diagramTRange_K;
                 adiabatTemperature += config->dconfig->dryAdiabatSpacing)
            {
                // First vertex of adiabat.
                float p_hPa = exp(log_pBot);
                float potT_K = ambientTemperatureOfPotentialTemperature_K(
                            adiabatTemperature, p_hPa * 100.);

                QVector2D tpCoordinate_K_hPa = QVector2D(potT_K, p_hPa);
                vStart = transformTp2xy(tpCoordinate_K_hPa);

                // Remaining vertices.
                for (float log_p_hPa = log_pBot; log_p_hPa > log_pTop;
                     log_p_hPa -= deltaLogP)
                {
                    float p_hPa = exp(log_p_hPa);
                    float potT_K = ambientTemperatureOfPotentialTemperature_K(
                                adiabatTemperature, p_hPa * 100.);

                    QVector2D tpCoordinate_K_hPa = QVector2D(potT_K, p_hPa);
                    vEnd = transformTp2xy(tpCoordinate_K_hPa);

                    config->dryAdiabatesVertices << vStart << vEnd;
                    vStart = vEnd;
                }
            }
        }
    }

    vertexArray << config->dryAdiabatesVertices;
    config->vertexArrayDrawRanges.dryAdiabates.indexCount = vertexArray.size()
            - config->vertexArrayDrawRanges.dryAdiabates.startIndex;


    // Generate moist adiabates vertices.
    // ==================================
    config->vertexArrayDrawRanges.moistAdiabates.startIndex = vertexArray.size();

    if (diagramConfiguration.drawMoistAdiabates)
    {
        // Regenerate moist adiabates only if necessary (first time, pressure
        // drawing type, temperature scale, top and bottom pressure changed).
        if (config->vertexArrayDrawRanges.moistAdiabates.indexCount == 0
                || config->recomputeAdiabateGeometries)
        {
            config->moistAdiabatesVertices.clear();

            for (float adiabatTemperature = diagramTmin_K;
                 adiabatTemperature <= diagramTmax_K;
                 adiabatTemperature += config->dconfig->moistAdiabatSpacing)
            {
                // NOTE that the Moisseeva & Stull (2017) implementation for
                // saturated adiabats is only valid for a thetaW range of
                // -70 degC to +40 degC. Hence limit to this range.
                if (adiabatTemperature < 203.15 || adiabatTemperature > 313.15)
                {
                    continue;
                }

                // First vertex of adiabat.
                float p_hPa = exp(log_pBot);
                float potT_K = temperatureAlongSaturatedAdiabat_K_MoisseevaStull(
                            adiabatTemperature, p_hPa * 100.);

                QVector2D tpCoordinate_K_hPa = QVector2D(potT_K, p_hPa);
                vStart = transformTp2xy(tpCoordinate_K_hPa);

                // Remaining vertices.
                for (float log_p_hPa = log_pBot; log_p_hPa > log_pTop;
                     log_p_hPa -= deltaLogP)
                {
                    float p_hPa = exp(log_p_hPa);
                    float potT_K = temperatureAlongSaturatedAdiabat_K_MoisseevaStull(
                                adiabatTemperature, p_hPa * 100.);

                    QVector2D tpCoordinate_K_hPa = QVector2D(potT_K, p_hPa);
                    vEnd = transformTp2xy(tpCoordinate_K_hPa);

                    config->moistAdiabatesVertices << vStart << vEnd;
                    vStart = vEnd;
                }
            }
        }
    }

    vertexArray << config->moistAdiabatesVertices;

    config->vertexArrayDrawRanges.moistAdiabates.indexCount = vertexArray.size()
            - config->vertexArrayDrawRanges.moistAdiabates.startIndex;

    // Upload geometry to vertex buffer.
    config->recomputeAdiabateGeometries = false;
    uploadVec2ToVertexBuffer(
                vertexArray, QString("skewTDiagramVertices%1_actor#%2")
                .arg(config->bufferNameSuffix).arg(myID),
                vbDiagramVertices);

    LOG4CPLUS_DEBUG(mlog, "Generation of Skew-T diagram geometry finished.");
}


void MSkewTActor::generateFullScreenHighlightGeometry(
        QVector2D tpCoordinate,
        GL::MVertexBuffer** vbDiagramVertices,
        ModeSpecificDiagramConfiguration *config)
{
    config->highlightGeometryDrawRanges.lineWidth = 3;

    // Array with vertex data that will be uploaded to a vertex buffer at the
    // end of the method. Contains line segments to be rendered with GL_LINES.
    // NOTE: All geometry stored in this array needs to be mapped to 2D diagram
    // coordinates (0..1) x (0..1)!
    QVector<QVector2D> vertexArray;

//TODO (mr, 11Jan2019) -- most geometry generated in this method could make
//    use of line strips (whould make rendering more efficient).

    // Temporary variables for start and end vertices for a line segment,
    // reused throughout the method.
    QVector2D vStart, vEnd;

    // Omit diagram frame.
    // ====================================
    config->highlightGeometryDrawRanges.frame.startIndex = 0;
    config->highlightGeometryDrawRanges.frame.indexCount = 0;


    // Generate vertices for isobar.
    // =============================
    config->highlightGeometryDrawRanges.isobars.startIndex = vertexArray.size();

    float pLevel_hPa = tpCoordinate.y();
    if ((pLevel_hPa < diagramConfiguration.vertical_p_hPa.max) &&
            (pLevel_hPa > diagramConfiguration.vertical_p_hPa.min))
    {
        // We need some temperature for the transformation, not used
        // any further.
        QVector2D tpCoordinate(273.15, pLevel_hPa);
        QVector2D xyCoordinate = transformTp2xy(tpCoordinate);
        vStart = QVector2D(0., xyCoordinate.y());
        vEnd = QVector2D(1., xyCoordinate.y());
        vertexArray << vStart << vEnd;
    }

    config->highlightGeometryDrawRanges.isobars.indexCount =
            vertexArray.size() - config->highlightGeometryDrawRanges.isobars.startIndex;


    // Generate vertices for isotherm.
    // ===============================
    config->highlightGeometryDrawRanges.isotherms.startIndex = vertexArray.size();

    float isothermTemperature_K = tpCoordinate.x();
    // Generate vertex at (isotherm temperature, bottom pressure).
    QVector2D tpCoordinate_K_hPa = QVector2D(
                isothermTemperature_K, config->dconfig->vertical_p_hPa.min);
    vStart = transformTp2xy(tpCoordinate_K_hPa);
    // Generate vertex at (isotherm temperature, top pressure).
    tpCoordinate_K_hPa.setY(config->dconfig->vertical_p_hPa.max);
    vEnd = transformTp2xy(tpCoordinate_K_hPa);
    vertexArray << vStart << vEnd;

    config->highlightGeometryDrawRanges.isotherms.indexCount =
            vertexArray.size() - config->highlightGeometryDrawRanges.isotherms.startIndex;


    // Generate vertices for dry adiabate.
    // ===================================

    float log_pBot = log(config->dconfig->vertical_p_hPa.max);
    float log_pTop = log(config->dconfig->vertical_p_hPa.min);
    int nAdiabatPoints = 100; // number of discrete points to plot adiabat
    float deltaLogP = (log_pBot - log_pTop) / float(nAdiabatPoints);

    config->highlightGeometryDrawRanges.dryAdiabates.startIndex = vertexArray.size();

    if (diagramConfiguration.drawDryAdiabates)
    {
        float adiabatTemperature = potentialTemperature_K(
                    tpCoordinate.x(), tpCoordinate.y() * 100.);

        // First vertex of adiabat.
        float p_hPa = exp(log_pBot);
        float potT_K = ambientTemperatureOfPotentialTemperature_K(
                    adiabatTemperature, p_hPa * 100.);

        QVector2D tpCoordinate_K_hPa = QVector2D(potT_K, p_hPa);
        vStart = transformTp2xy(tpCoordinate_K_hPa);

        // Remaining vertices.
        for (float log_p_hPa = log_pBot; log_p_hPa > log_pTop;
             log_p_hPa -= deltaLogP)
        {
            float p_hPa = exp(log_p_hPa);
            float potT_K = ambientTemperatureOfPotentialTemperature_K(
                        adiabatTemperature, p_hPa * 100.);

            QVector2D tpCoordinate_K_hPa = QVector2D(potT_K, p_hPa);
            vEnd = transformTp2xy(tpCoordinate_K_hPa);

            vertexArray << vStart << vEnd;
            vStart = vEnd;
        }
    }

    config->highlightGeometryDrawRanges.dryAdiabates.indexCount = vertexArray.size()
            - config->highlightGeometryDrawRanges.dryAdiabates.startIndex;


    // Generate moist adiabate vertices.
    // =================================
    config->highlightGeometryDrawRanges.moistAdiabates.startIndex = vertexArray.size();

    if (diagramConfiguration.drawMoistAdiabates)
    {
            // NOTE that the Moisseeva & Stull (2017) implementation for
            // saturated adiabats is only valid for a temperature range of
            // -100 degC to +40 degC. Hence limit to this range.
            if (tpCoordinate.x() >= 173.15 && tpCoordinate.x() < 313.15)
            {
                float adiabatTemperature =
                        wetBulbPotentialTemperatureOfSaturatedAdiabat_K_MoisseevaStull(
                            tpCoordinate.x(), tpCoordinate.y() * 100.);

                // First vertex of adiabat.
                float p_hPa = exp(log_pBot);
                float moistPotT_K =
                        temperatureAlongSaturatedAdiabat_K_MoisseevaStull(
                            adiabatTemperature, p_hPa * 100.);

                QVector2D tpCoordinate_K_hPa = QVector2D(moistPotT_K, p_hPa);
                vStart = transformTp2xy(tpCoordinate_K_hPa);

                // Remaining vertices.
                for (float log_p_hPa = log_pBot; log_p_hPa > log_pTop;
                     log_p_hPa -= deltaLogP)
                {
                    float p_hPa = exp(log_p_hPa);
                    float potT_K = temperatureAlongSaturatedAdiabat_K_MoisseevaStull(
                                adiabatTemperature, p_hPa * 100.);

                    QVector2D tpCoordinate_K_hPa = QVector2D(potT_K, p_hPa);
                    vEnd = transformTp2xy(tpCoordinate_K_hPa);

                    vertexArray << vStart << vEnd;
                    vStart = vEnd;
                }
            }
    }

    config->highlightGeometryDrawRanges.moistAdiabates.indexCount = vertexArray.size()
            - config->highlightGeometryDrawRanges.moistAdiabates.startIndex;

    // Upload geometry to vertex buffer.
    uploadVec2ToVertexBuffer(
                vertexArray, QString("skewTHighlightVertices%1_actor#%2")
                .arg(config->bufferNameSuffix).arg(myID),
                vbDiagramVertices);
}


#define SHADER_VERTEX_ATTRIBUTE 0
#define SHADER_TEXTURE_ATTRIBUTE 1

void MSkewTActor::drawDiagram2(MSceneViewGLWidget *sceneView)
{
    skewTShader->bindProgram("DiagramGeometry");
    skewTShader->setUniformValue("fullscreen",
                                 sceneViewFullscreenEnabled.value(sceneView));
    skewTShader->setUniformValue("mvpMatrix",
                                 *(sceneView->getModelViewProjectionMatrix()));
    skewTShader->setUniformValue("verticesInTPSpace", GLboolean(true));
    skewTShader->setUniformValue("tlogp2xyMatrix", transformationMatrixTlogp2xy);

    for (MNWPActorVariable* avar : variables)
    {
        MNWPSkewTActorVariable* var =
                static_cast<MNWPSkewTActorVariable*> (avar);

        if ( !var->hasData() ) continue;

        var->profileVertexBuffer->attachToVertexAttribute(
                    SHADER_VERTEX_ATTRIBUTE, 2,
                    GL_FALSE, 0, (const GLvoid *)(0 * sizeof(float)));

        skewTShader->setUniformValue("colour", var->profileColour);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(float(var->lineThickness));
        glDrawArrays(GL_LINE_STRIP, 0, var->profile.getScalarPressureData().size());

        // Unbind VBO.
        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    }
}


void MSkewTActor::drawDiagramGeometryAndLabels(
        MSceneViewGLWidget *sceneView, GL::MVertexBuffer *vbDiagramVertices,
        VertexRanges *vertexRanges)
{
    // Bind vertex array and shader for diagram geometry.
    // ==================================================
    glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);
    vbDiagramVertices->attachToVertexAttribute(
        SHADER_VERTEX_ATTRIBUTE, 2,
        GL_FALSE, 0, (const GLvoid *)(0 * sizeof(float)));

    skewTShader->bindProgram("DiagramGeometry");

    skewTShader->setUniformValue("fullscreen",
                                 sceneViewFullscreenEnabled.value(sceneView));
    skewTShader->setUniformValue("mvpMatrix",
                                 *(sceneView->getModelViewProjectionMatrix()));
    skewTShader->setUniformValue("verticesInTPSpace", GLboolean(false));
    skewTShader->setUniformValue("xy2worldMatrix",
                                 computeXY2WorldSpaceTransformationMatrix(sceneView));

    // Draw diagram background and frame.
    // ==================================

    // The first four vertices of the frame, drawn as a triangle strip,
    // represent the diagram background.
    skewTShader->setUniformValue("colour", QVector4D(1.f, 1.f, 1.f, 1.f));
    skewTShader->setUniformValue("depthOffset", 0.001f); // draw in background
    glLineWidth(1);
    glPolygonMode(GL_FRONT_AND_BACK, renderAsWireFrame ? GL_LINE : GL_FILL);
    glDrawArrays(GL_TRIANGLE_STRIP,
                 vertexRanges->frame.startIndex, 4);

    // Render the frame as thick lines.
    glLineWidth(3);
    skewTShader->setUniformValue("colour", diagramConfiguration.diagramColor);
    skewTShader->setUniformValue("depthOffset", 0.f);
    glDrawArrays(GL_LINES,
                 vertexRanges->frame.startIndex,
                 vertexRanges->frame.indexCount);

    glLineWidth(vertexRanges->lineWidth);

    // Draw dry adiabates.
    // ===================
    if (diagramConfiguration.drawDryAdiabates)
    {
        skewTShader->setUniformValue("colour", QVector4D(0.8f, 0.8f, 0.f, 1.f));
        glDrawArrays(GL_LINES,
                     vertexRanges->dryAdiabates.startIndex,
                     vertexRanges->dryAdiabates.indexCount);
    }

    // Draw isobars.
    // =============
    skewTShader->setUniformValue("colour", diagramConfiguration.diagramColor);
    glDrawArrays(GL_LINES,
                 vertexRanges->isobars.startIndex,
                 vertexRanges->isobars.indexCount);

    // Draw isotherms.
    // ===============
    skewTShader->setUniformValue("colour", QVector4D(1.f, 0.f, 0.f, 1.f));
    glDrawArrays(GL_LINES,
                 vertexRanges->isotherms.startIndex,
                 vertexRanges->isotherms.indexCount);

    // Draw moist adiabates.
    // =====================
    if (diagramConfiguration.drawMoistAdiabates)
    {
        skewTShader->setUniformValue("colour", QVector4D(0.f, 0.8f, 0.f, 1.f));
        glDrawArrays(GL_LINES,
                     vertexRanges->moistAdiabates.startIndex,
                     vertexRanges->moistAdiabates.indexCount);
    }

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;

    // Copy render-mode-dependent labels (3D vs. fullscreen).
    // ======================================================
    if (sceneViewFullscreenEnabled.value(sceneView))
    {
        labels = labelsFullScreen;
    }
    else
    {
        labels = labels3D;
        labels.append(poleActor->getLabelsToRender());
//        poleActor->render(sceneView);
    }
}


void MSkewTActor::loadListOfAvailableObservationsFromUWyoming()
{
    QUrl url = QUrl(QString("http://weather.uwyo.edu/upperair/europe.html"));
    QNetworkAccessManager *m_netwManager = new
    QNetworkAccessManager(this);
    connect(m_netwManager,
            SIGNAL(finished(QNetworkReply *)), this,
            SLOT(downloadOfObservationListFromUWyomingFinished(QNetworkReply *)));
    QNetworkRequest request(url);
    m_netwManager->get(request);
}


void MSkewTActor::loadObservationalDataFromUWyoming(int stationNum)
{
    MSystemManagerAndControl *sysMC =
        MSystemManagerAndControl::getInstance();
    MSyncControl *sync =
        sysMC->getSyncControl("Synchronization");
    QUrl url = QUrl(
                   QString(
                    "http://weather.uwyo.edu/cgi-bin/sounding?region=europe"
                    "&TYPE=TEXT:LIST&YEAR=%1&MONTH=%2&FROM=1712&TO=1712&STNM=%3")
                   .arg(sync->validDateTime().date().year())
                   .arg(sync->validDateTime().date().month())
                   .arg(stationNum));
    QNetworkAccessManager *m_netwManager = new
    QNetworkAccessManager(this);
    connect(m_netwManager,
            SIGNAL(finished(QNetworkReply *)), this,
            SLOT(downloadOfObservationFromUWyomingFinished(QNetworkReply *)));
    QNetworkRequest request(url);
    m_netwManager->get(request);
}


void MSkewTActor::DiagramConfiguration::init()
{
    diagramColor = QVector4D(0, 0, 0, 1);
}


void MSkewTActor::ModeSpecificDiagramConfiguration::init(
        DiagramConfiguration *dconfig, QString bufferNameSuffix)
{
    this->dconfig = dconfig;
    this->bufferNameSuffix = bufferNameSuffix;
}


void MSkewTActor::drawDiagram3DView(MSceneViewGLWidget* sceneView)
{
    skewTDiagramConfiguration.layer = -0.005f;
    drawDiagramGeometryAndLabels(
                sceneView, vbDiagramVertices,
                &skewTDiagramConfiguration.vertexArrayDrawRanges);
    skewTDiagramConfiguration.layer = -.1f;
    drawDiagram2(sceneView);
    poleActor->render(sceneView);
}


void MSkewTActor::drawDiagramFullScreen(MSceneViewGLWidget* sceneView)
{
    glClear(GL_DEPTH_BUFFER_BIT);
    drawDiagramGeometryAndLabelsFullScreen(sceneView);
    drawDiagram2(sceneView);
}


void MSkewTActor::drawDiagramGeometryAndLabelsFullScreen(
        MSceneViewGLWidget* sceneView)
{
    drawDiagramGeometryAndLabels(
                sceneView, vbDiagramVertices,
                &skewTDiagramConfiguration.vertexArrayDrawRanges);

    // In interaction mode, draw highlighting coordinate axes and adiabates
    // for the current mouse position.
    if (sceneView->interactionModeEnabled() && vbHighlightVertices)
    {
        drawDiagramGeometryAndLabels(
                    sceneView, vbHighlightVertices,
                    &skewTDiagramConfiguration.highlightGeometryDrawRanges);
    }
}


void MSkewTActor::drawDiagramGeometryAndLabels3DView(
        MSceneViewGLWidget* sceneView)
{
    drawDiagramGeometryAndLabels(
                sceneView, vbDiagramVertices,
                &skewTDiagramConfiguration.vertexArrayDrawRanges);
}


void MSkewTActor::computeTlogp2xyTransformationMatrix()
{
    // Construct a transformation matrix that transforms (temperature,
    // log(pressure)) coordinates into a (skewed) (x, y) coordinate system in
    // the range (0..1).

    float Tmin_K = degCToKelvin(diagramConfiguration.temperature_degC.min);
    float Tmax_K = degCToKelvin(diagramConfiguration.temperature_degC.max);
    float logpbot_hPa = log(diagramConfiguration.vertical_p_hPa.max);
    float logptop_hPa = log(diagramConfiguration.vertical_p_hPa.min);
    float xShear = diagramConfiguration.skewFactor; // 0..1, where 1 == 45 deg

    // Translate (-Tmin_K, -logpbot_hPa) to the origin.
    QMatrix4x4 translationMatrix;
    translationMatrix.translate(-Tmin_K, -logpbot_hPa);

    // Scale so that both T and p become x and y in (0..1) each.
    QMatrix4x4 scaleMatrix;
    scaleMatrix.scale(1./(Tmax_K-Tmin_K), 1./(logptop_hPa-logpbot_hPa));

    // Skew temperature axis through shear in x (shear factor 0..2 works well).
    QTransform shear;
    shear.shear(xShear, 0.);
    QMatrix4x4 shearMatrix(shear);

    // Construct transformation matrix that performs all steps above at once.
    transformationMatrixTlogp2xy = shearMatrix * scaleMatrix * translationMatrix;
    transformationMatrixXY2Tlogp = transformationMatrixTlogp2xy.inverted();
}


QVector2D MSkewTActor::transformTp2xy(QVector2D tpCoordinate_K_hPa)
{
    QPointF tlogpCoordinate = QPointF(tpCoordinate_K_hPa.x(),
                                      log(tpCoordinate_K_hPa.y()));
    QPointF xyCoordinate = transformationMatrixTlogp2xy * tlogpCoordinate;

    return QVector2D(xyCoordinate);
}


QVector2D MSkewTActor::transformXY2Tp(QVector2D xyCoordinate)
{
    QPointF tlogpCoordinate =
            transformationMatrixXY2Tlogp * xyCoordinate.toPointF();
    return QVector2D(tlogpCoordinate.x(), exp(tlogpCoordinate.y()));
}


QVector2D MSkewTActor::transformXY2ClipSpace(QVector2D xyCoordinate)
{
    // NOTE: There is an equivalent function with the same name in the GLSL
    // shader; if this function is modified the shader function needs to be
    // modified as well.

//TODO (mr, 10Jan2019) -- replace by matrix multiplication.
    float hPad = 0.1;
    float vPad = 0.1;
    return QVector2D(xyCoordinate.x() * (2.-2.*hPad) - 1.+hPad,
                     xyCoordinate.y() * (2.-2.*vPad) - 1.+vPad);
}


QVector2D MSkewTActor::transformClipSpace2xy(QVector2D clipCoordinate)
{
    // NOTE: see transformXY2ClipSpace().
    float hPad = 0.1;
    float vPad = 0.1;
    return QVector2D((clipCoordinate.x() + 1.-hPad) / (2.-2.*hPad),
                     (clipCoordinate.y() + 1.-vPad) / (2.-2.*vPad));
}


QMatrix4x4 MSkewTActor::computeXY2WorldSpaceTransformationMatrix(
        MSceneViewGLWidget* sceneView)
{
    // Input are (x, y) coordinates ("diagram space") in 0..1 each.
    // Transformation to a scene view's world space requires:
    // 0. Interpret (x, y) as (lon, z) in world space.
    // 1. Scale x(lon) to a user-defined width in "degrees", and scale
    //    z to the world-Z range given by pbot and ptop.
    // 2. If the diagram plane should be oriented with the camera (so that
    //    the user is always looking straight at the diagram, rotate by
    //    using the camera's "up" and "right" vectors (build a new
    //    orthonormal coordinate system and simply put that into a matrix).
    // 3. Translate the result to the location of the diagram probe.

    float worldZbot = sceneView->worldZfromPressure(
                diagramConfiguration.vertical_p_hPa.max);
    float worldZtop = sceneView->worldZfromPressure(
                diagramConfiguration.vertical_p_hPa.min);

    QMatrix4x4 scaleMatrix;
    scaleMatrix.scale(diagramConfiguration.diagramWidth3D,
                      1.,
                      worldZtop-worldZbot);

    QMatrix4x4 rotationMatrix;
    rotationMatrix.setToIdentity();
    if (diagramConfiguration.alignWithCamera)
    {
        QVector3D up = QVector3D(0., 0., 1);
        QVector3D right = sceneView->getCamera()->getXAxis();
        QVector3D front = QVector3D::crossProduct(up, right);
        rotationMatrix.setColumn(0, QVector4D(right));
        rotationMatrix.setColumn(1, QVector4D(front));
        rotationMatrix.setColumn(2, QVector4D(up));
    }

    QMatrix4x4 translationMatrix;
    translationMatrix.translate(diagramConfiguration.geoPosition.x(),
                                diagramConfiguration.geoPosition.y(),
                                worldZbot);

    return translationMatrix * rotationMatrix * scaleMatrix;
}


void MSkewTActor::updateVerticalProfilesAndLabels()
{
    // Tell all actor variables to update their vertical profile data to the
    // current geographical location.
    for (MNWPActorVariable* avar : variables)
    {
        MNWPSkewTActorVariable* var =
                static_cast<MNWPSkewTActorVariable*> (avar);

        var->updateProfile(diagramConfiguration.geoPosition);
    }

    // Update the geographical locations of the labels in the 3D view.
    for (MLabel* label : labels3D)
    {
        label->anchor.setX(diagramConfiguration.geoPosition.x());
        label->anchor.setY(diagramConfiguration.geoPosition.y());
    }

    // Update vertical position of the attached pole actor.
    if (!dragEventActive)
    {
        poleActor->setPolePosition(
                    0, diagramConfiguration.geoPosition.toPointF());
    }
}


void MSkewTActor::removeAllSkewTLabels()
{
    // Remove all text labels.
    MTextManager* tm = MGLResourcesManager::getInstance()->getTextManager();
    if (tm == nullptr)
    {
        // When Met.3D is closed, the text manager may be deleted before this
        // actor is deleted. In that case labels will be freed by the text
        // manager's destructor.
        return;
    }

    while ( !labelsFullScreen.isEmpty() )
    {
        tm->removeText(labelsFullScreen.takeLast());
    }
    while ( !labels3D.isEmpty() )
    {
        tm->removeText(labels3D.takeLast());
    }

    //NOTE: The inherited list "labels" should only contain labels copied from
    // one of the two above lists in "drawDiagramGeometryAndLabels()". Since
    // all labels are deleted in the loops above we need to clear the "labels"
    // list. **THIS ASSUMES THAT NO OTHER LABELS ARE CONTAINED IN THIS LIST!**
    labels.clear();
}


}  // namespace Met3D
