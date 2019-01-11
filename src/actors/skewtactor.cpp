/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2016 Christoph Heidelmann [+]
**  Copyright 2018      Bianca Tost [+]
**  Copyright 2015-2019 Marc Rautenhaus [*, previously +]
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
    vbDiagramVerticesFS(nullptr),
    vbWyomingVertices(nullptr),
    wyomingVerticesCount(0),
    offsetPickPositionToHandleCentre(QVector2D(0., 0.))
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

    perspectiveRenderingProperty = addProperty(
                BOOL_PROPERTY, "perspective depiction", appearanceGroupProperty);
    properties->mBool()->setValue(perspectiveRenderingProperty, false);

    alignWithWorldPressureProperty = addProperty(
                BOOL_PROPERTY, "align with pressure axis",
                appearanceGroupProperty);
    properties->mBool()->setValue(alignWithWorldPressureProperty, true);

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
    skewFactorProperty= addProperty(
                DECORATEDDOUBLE_PROPERTY, "skew factor",
                appearanceGroupProperty);
    properties->setDDouble(skewFactorProperty, 1., 0., 2., 2, 0.1, " (0..2)");
    isothermsSpacingProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "isotherms spacing",
                appearanceGroupProperty);
    properties->setDDouble(isothermsSpacingProperty, 10., 0.1, 100., 1, 1.,
                           celsiusUnit);

    // Dry adiabates.
    // ==============
    drawDryAdiabatesProperty = addProperty(
                BOOL_PROPERTY, "draw dry adiabates", appearanceGroupProperty);
    properties->mBool()->setValue(drawDryAdiabatesProperty, true);
    dryAdiabatesSpacingProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "dry adiabates spacing",
                appearanceGroupProperty);
    properties->setDDouble(dryAdiabatesSpacingProperty, 10., 0.1, 100., 1, 1.,
                           celsiusUnit);

    // Moist adiabates.
    // ================
    drawMoistAdiabatesProperty = addProperty(
                BOOL_PROPERTY, "draw moist adiabates", appearanceGroupProperty);
    properties->mBool()->setValue(drawMoistAdiabatesProperty, true);
    moistAdiabatesSpcaingProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "moist adiabates spacing",
                appearanceGroupProperty);
    properties->setDDouble(moistAdiabatesSpcaingProperty, 10., 0.1, 100., 1, 1.,
                           celsiusUnit);

    geoPositionProperty = addProperty(
                POINTF_LONLAT_PROPERTY, "position", actorPropertiesSupGroup);
    properties->mPointF()->setValue(geoPositionProperty, QPointF());


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

    // Variables.
    // ==========
    diagramConfiguration.varConfigs = QVector<VariableConfig>(12);
    groupVariables = addProperty(
           GROUP_PROPERTY, "data variables", actorPropertiesSupGroup);

    // Temperature variables.
    // ======================
    temperatureGroupProperty = addProperty(
                GROUP_PROPERTY, "air temperature", groupVariables);
    diagramConfiguration.varConfigs[variablesIndices.temperature.MEMBER]
            .property = addProperty(ENUM_PROPERTY, "single member", temperatureGroupProperty);
    diagramConfiguration.varConfigs[variablesIndices.temperature.MEAN]
            .property = addProperty( ENUM_PROPERTY, "ensemble mean", temperatureGroupProperty);
    diagramConfiguration.varConfigs[variablesIndices.temperature.MAXIMUM].property
            = addProperty(ENUM_PROPERTY, "ensemble max", temperatureGroupProperty);
    diagramConfiguration.varConfigs[variablesIndices.temperature.MINIMUM].property
            = addProperty(ENUM_PROPERTY, "ensemble min", temperatureGroupProperty);
    diagramConfiguration.varConfigs[variablesIndices.temperature.DEVIATION]
            .property = addProperty(ENUM_PROPERTY, "ensemble std.dev.", temperatureGroupProperty);
    diagramConfiguration.varConfigs[variablesIndices.temperature.SPAGHETTI]
            .property = addProperty(ENUM_PROPERTY, "multiple members", temperatureGroupProperty);
    temperatureShowProbabilityTubeProperty = addProperty(
                BOOL_PROPERTY, "draw min-max shaded", temperatureGroupProperty);
    properties->mBool()->setValue(temperatureShowProbabilityTubeProperty, true);
    temperatureShowDeviationTubeProperty = addProperty(
                BOOL_PROPERTY, "draw std.dev. shaded", temperatureGroupProperty);
    properties->mBool()->setValue(temperatureShowDeviationTubeProperty, true);
    temperatureMinMaxVariableColorProperty = addProperty(
                COLOR_PROPERTY, "min-max shade colour", temperatureGroupProperty);
    properties->mColor()->setValue(temperatureMinMaxVariableColorProperty,
                                   QColor(201, 10, 5, 255));

    // Dewpoint variables.
    // ===================
    humidityGroupProperty = addProperty(
                GROUP_PROPERTY, "specific humidity", groupVariables);
    diagramConfiguration.varConfigs[variablesIndices.dewPoint.MEMBER].property =
            addProperty(ENUM_PROPERTY, "single member", humidityGroupProperty);
    diagramConfiguration.varConfigs[variablesIndices.dewPoint.MEAN].property =
            addProperty(ENUM_PROPERTY, "ensemble mean", humidityGroupProperty);
    diagramConfiguration.varConfigs[variablesIndices.dewPoint.MAXIMUM].property =
            addProperty(ENUM_PROPERTY, "ensemble max", humidityGroupProperty);
    diagramConfiguration.varConfigs[variablesIndices.dewPoint.MINIMUM].property =
            addProperty(ENUM_PROPERTY, "ensemble min", humidityGroupProperty);
    diagramConfiguration.varConfigs[variablesIndices.dewPoint.DEVIATION]
            .property = addProperty(ENUM_PROPERTY, "ensemble std.dev.", humidityGroupProperty);
    diagramConfiguration.varConfigs[variablesIndices.dewPoint.SPAGHETTI]
            .property = addProperty(ENUM_PROPERTY, "multiple members", humidityGroupProperty);
    dewPointShowProbabilityTubeProperty = addProperty(
                BOOL_PROPERTY, "draw min-max shaded", humidityGroupProperty);
    properties->mBool()->setValue(dewPointShowProbabilityTubeProperty, true);
    dewPointShowDeviationTubeProperty = addProperty(
                BOOL_PROPERTY, "draw std.dev. shaded", humidityGroupProperty);
    properties->mBool()->setValue(dewPointShowDeviationTubeProperty, true);
    dewPointMinMaxVariableColorProperty = addProperty(
                COLOR_PROPERTY, "min-max shade colour", humidityGroupProperty);
    properties->mColor()->setValue(dewPointMinMaxVariableColorProperty,
                                   QColor(17, 98, 208, 255));

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
    if (vbDiagramVerticesFS)
    {
        delete vbDiagramVerticesFS;
    }
    MTextManager *tm = MGLResourcesManager::getInstance()->getTextManager();
    if (tm != nullptr)
    {
        for (QList<MLabel*> labels : skewTLabels)
        {
            // Remove all labels from text manager.
            const int numLabels = labels.size();

            for (int i = 0; i < numLabels; ++i)
            {
                tm->removeText(labels.takeLast());
            }
        }
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MSkewTActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);

    beginCompileShaders(2);

    compileShadersFromFileWithProgressDialog(
                skewTShader, "src/glsl/skewtrendering.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                positionSpheresShader, "src/glsl/trajectory_positions.fx.glsl");

    endCompileShaders();
}


void MSkewTActor::saveConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::saveConfiguration(settings);

    settings->beginGroup(MSkewTActor::getSettingsID());

    settings->setValue("upright",
                       properties->mBool()->value(perspectiveRenderingProperty));

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

    settings->setValue("pressureEqualsWorldPressure",
                       properties->mBool()->value(
                           alignWithWorldPressureProperty));
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

    settings->endGroup();
}


void MSkewTActor::loadConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::loadConfiguration(settings);

    enableActorUpdates(false);

    settings->beginGroup(MSkewTActor::getSettingsID());

    properties->mBool()->setValue(perspectiveRenderingProperty,
                                  settings->value("upright", false).toBool());

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
                alignWithWorldPressureProperty,
                settings->value("pressureEqualsWorldPressure", true).toBool());

    properties->mPointF()->setValue(
                geoPositionProperty,
                settings->value("position").toPointF());

    properties->mDDouble()->setValue(
        moistAdiabatesSpcaingProperty,
        settings->value("moistAdiabatesSpacing", 10.).toDouble());
    properties->mBool()->setValue(
        alignWithWorldPressureProperty,
        settings->value("moistAdiabatesEnabled", true).toBool());

    properties->mDDouble()->setValue(
                dryAdiabatesSpacingProperty,
                settings->value("dryAdiabatesSpacing", 10.).toDouble());
    properties->mBool()->setValue(
                alignWithWorldPressureProperty,
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
            properties->mColor()->setValue(var->variable->colorProperty, color);
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
        fullscreenDiagrammConfiguration.clipPos.setX(clipX);
        fullscreenDiagrammConfiguration.clipPos.setY(clipY);
    }
    else
    {
        float clipRadius = MSystemManagerAndControl::getInstance()
                ->getHandleSize();

        QMatrix4x4 *mvpMatrix = sceneView->getModelViewProjectionMatrix();
        QVector3D p = QVector3D(diagramConfiguration.geoPosition.x(),
                                diagramConfiguration.geoPosition.y(), 0.f);
        QVector3D pClip = *(mvpMatrix) * p; // projection into clipspace
        float dx = pClip.x() - clipX;
        float dy = pClip.y() - clipY;
        offsetPickPositionToHandleCentre = QVector2D(dx, dy);

        // Obtain the camera position and the view direction
        const QVector3D& cameraPos = sceneView->getCamera()->getOrigin();
        QVector3D viewDir = p - cameraPos;

        // Scale the radius (in world space) with respect to the viewer distance
        float radius = static_cast<float>(clipRadius * viewDir.length() / 100.f);

        // Compute the world position of the current mouse position
        QVector3D mouseWorldPos = mvpMatrix->inverted()
                * QVector3D(clipX, clipY, 1.f);

        // Get the ray direction from the camera to the mouse position
        QVector3D l = mouseWorldPos - cameraPos;
        l.normalize();

        // Compute (o - c) // ray origin (o) - sphere center (c)
        QVector3D oc = cameraPos - p;
        // Length of (o - c) = || o - c ||
        float lenOC = static_cast<float>(oc.length());
        // Compute l * (o - c)
        float loc = static_cast<float>(QVector3D::dotProduct(l, oc));

        // Solve equation:
        // d = - (l * (o - c) +- sqrt( (l * (o - c))² - || o - c ||² + r² )
        // Since the equation can be solved only if root discriminant is >= 0
        // just compute the discriminant
        float root = loc * loc - lenOC * lenOC + radius * radius;

        // If root discriminant is positive or zero, there's an intersection
        if (root >= 0.f)
        {
            mvpMatrix = sceneView->getModelViewProjectionMatrix();
            QVector3D posCentreClip = *mvpMatrix * p;
            offsetPickPositionToHandleCentre =
                    QVector2D(posCentreClip.x() - clipX,
                              posCentreClip.y() - clipY);
            diagramConfiguration.overDragHandle = true;
            return 1;
        }

        diagramConfiguration.overDragHandle = false;
    }
    return -1;
}


void MSkewTActor::dragEvent(
     MSceneViewGLWidget *sceneView, int handleID, float clipX, float clipY)
{
    Q_UNUSED(handleID);
    if (!sceneViewFullscreenEnabled.value(sceneView))
    {
        // Select an arbitrary z-value to construct a point in clip space that,
        // transformed to world space, lies on the ray passing through the camera
        // and the location on the worldZ==0 plane "picked" by the mouse.
        // (See notes 22-23Feb2012).
        QVector3D mousePosClipSpace =
                QVector3D(clipX + offsetPickPositionToHandleCentre.x(),
                          clipY + offsetPickPositionToHandleCentre.y(),
                          0.f);

        // The point p at which the ray intersects the worldZ==0 plane is found
        // by computing the value d in p = d * l + l0, where l0 is a point on
        // the ray and l is a vector in the direction of the ray. d can be
        // found with
        //        (p0 - l0) * n
        //   d = ----------------
        //            l * n
        // where p0 is a point on the worldZ==0 plane and n is the normal vector
        // of the plane.
        //       http://en.wikipedia.org/wiki/Line-plane_intersection

        // To compute l0, the MVP matrix has to be inverted.
        QMatrix4x4 *mvpMatrix =
            sceneView->getModelViewProjectionMatrix();
        QVector3D l0 = mvpMatrix->inverted() *
                       mousePosClipSpace;

        // Compute l as the vector from l0 to the camera origin.
        QVector3D cameraPosWorldSpace =
            sceneView->getCamera()->getOrigin();
        QVector3D l = (l0 - cameraPosWorldSpace);

        // The plane's normal vector simply points upward, the origin in world
        // space is located on the plane.
        QVector3D n = QVector3D(0.f, 0.f, 1.f);
        QVector3D p0 = QVector3D(0.f, 0.f, 0.f);

        // Compute the mouse position in world space.
        float d = QVector3D::dotProduct(p0 - l0, n) / QVector3D::dotProduct(l, n);

        QVector3D mousePosWorldSpace = l0 + d * l;
        QPointF p = QPointF(mousePosWorldSpace.x(), mousePosWorldSpace.y());
        properties->mPointF()->setValue(geoPositionProperty, p);
    }
}


void MSkewTActor::onFullScreenModeSwitch(MSceneViewGLWidget *sceneView,
                                         bool fullScreenEnabled)
{
    sceneViewFullscreenEnabled.insert(sceneView, fullScreenEnabled);
//    if (fullScreenEnabled)
//    {
//        diagramConfiguration.fullscreen = true;
//    }
//    else
//    {
//        diagramConfiguration.fullscreen = false;
//    }
    copyDiagramConfigurationFromQtProperties();
    diagramConfiguration.regenerateAdiabates = true;
    normalscreenDiagrammConfiguration.recomputeAdiabateGeometries = true;
    fullscreenDiagrammConfiguration.recomputeAdiabateGeometries = true;
    if (isInitialized())
    {
        generateDiagramGeometry(&vbDiagramVertices, &normalscreenDiagrammConfiguration);
        generateDiagramGeometry(&vbDiagramVerticesFS, &fullscreenDiagrammConfiguration);
    }
    emitActorChangedSignal();
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

    loadShaders |= glRM->generateEffectProgram("skewt_spheres",
                                               positionSpheresShader);

    if (loadShaders) reloadShaderEffects();

    copyDiagramConfigurationFromQtProperties();

    generateDiagramGeometry(&vbDiagramVertices, &normalscreenDiagrammConfiguration);
    generateDiagramGeometry(&vbDiagramVerticesFS, &fullscreenDiagrammConfiguration);

    labelSize       = properties->mInt()->value(labelSizeProperty);
    labelColour     = properties->mColor()->value(labelColourProperty);
    labelBBox       = properties->mBool()->value(labelBBoxProperty);
    labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);

    LOG4CPLUS_DEBUG(mlog, "done");
}


void MSkewTActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    sceneViewFullscreenEnabled.insert(sceneView, false);
    drawDiagram3DView(sceneView);
    if (sceneView->interactionModeEnabled())
    {
        drawDiagramHandle(sceneView);
    }
}


void MSkewTActor::renderToCurrentFullScreenContext(MSceneViewGLWidget *sceneView)
{
    sceneViewFullscreenEnabled.insert(sceneView, true);
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
    else if (property == temperatureMaxProperty
             || property == temperatureMinProperty
             || property == alignWithWorldPressureProperty
             || property == isothermsSpacingProperty
             || property == moistAdiabatesSpcaingProperty
             || property == dryAdiabatesSpacingProperty
             || property == bottomPressureProperty
             || property == topPressureProperty
             || property == skewFactorProperty)
    {
        diagramConfiguration.regenerateAdiabates = true;
        normalscreenDiagrammConfiguration.recomputeAdiabateGeometries = true;
        fullscreenDiagrammConfiguration.recomputeAdiabateGeometries = true;

        copyDiagramConfigurationFromQtProperties();

        generateDiagramGeometry(&vbDiagramVertices,
                                &normalscreenDiagrammConfiguration);
        generateDiagramGeometry(&vbDiagramVerticesFS,
                                &fullscreenDiagrammConfiguration);
        emitActorChangedSignal();
    }
    else if (property == labelSizeProperty
             || property == labelColourProperty
             || property == labelBBoxProperty
             || property == labelBBoxColourProperty)
    {
        labelSize       = properties->mInt()->value(labelSizeProperty);
        labelColour     = properties->mColor()->value(labelColourProperty);
        labelBBox       = properties->mBool()->value(labelBBoxProperty);
        labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);
        emitActorChangedSignal();
    }
    else if (property == geoPositionProperty
             || property == perspectiveRenderingProperty)
    {
        diagramConfiguration.drawInPerspective
                = properties->mBool()->value(perspectiveRenderingProperty);
        diagramConfiguration.geoPosition = QVector2D(
                    float(properties->mPointF()->value(geoPositionProperty).x()),
                    float(properties->mPointF()->value(geoPositionProperty).y()));
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
            if ((normalscreenDiagrammConfiguration
                 .vertexArrayDrawRanges.dryAdiabates.indexCount == 0)
                    || normalscreenDiagrammConfiguration.recomputeAdiabateGeometries)
            {
                generateDiagramGeometry(&vbDiagramVertices,
                                        &normalscreenDiagrammConfiguration);
            }
            // Regenerate dry adiabates only if necessary (first time, pressure
            // drawing type, temperature scale)
            if ((fullscreenDiagrammConfiguration
                 .vertexArrayDrawRanges.dryAdiabates.indexCount == 0)
                    || fullscreenDiagrammConfiguration.recomputeAdiabateGeometries)
            {
                generateDiagramGeometry(&vbDiagramVertices,
                                        &fullscreenDiagrammConfiguration);
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
            if ((normalscreenDiagrammConfiguration
                 .vertexArrayDrawRanges.moistAdiabates.indexCount == 0)
                    || normalscreenDiagrammConfiguration.recomputeAdiabateGeometries)
            {
                generateDiagramGeometry(&vbDiagramVertices,
                                        &normalscreenDiagrammConfiguration);
            }
            // Regenerate moist adiabates only if necessary (first time,
            // pressure drawing type, temperature scale)
            if ((fullscreenDiagrammConfiguration
                 .vertexArrayDrawRanges.moistAdiabates.indexCount == 0)
                    || fullscreenDiagrammConfiguration.recomputeAdiabateGeometries)
            {
                generateDiagramGeometry(&vbDiagramVertices,
                                        &fullscreenDiagrammConfiguration);
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
                    || property == var->variable->colorProperty
                    || property == var->variable->thicknessProperty)
            {
                var->variable = dynamic_cast<MNWPSkewTActorVariable*>(
                            variables.at(var->index - 1));
                var->color = var->variable->color;
                var->thickness = var->variable->thickness;
                emitActorChangedSignal();
                return;
            }
        }
        // A variable might have changed its ensemble mode thus update the variable
        // enum property names.
        for (MNWPActorVariable *var : variables)
        {
            if (property == var->ensembleModeProperty)
            {
                updateVariableEnums();
                emitActorChangedSignal();
            }
        }
    }
}


void MSkewTActor::onDeleteActorVariable(MNWPActorVariable *var)
{
    for (int i = 0; i < diagramConfiguration.varConfigs.size(); i++)
    {
        VariableConfig *varConfig = &diagramConfiguration.varConfigs[i];
        if (varConfig->variable == var)
        {
            varConfig->index = -1;
            varConfig->variable = nullptr;
            varConfig->color = QColor(0, 0, 0, 255);
            break;
        }
    }
    updateVariableEnums(var);
}


void MSkewTActor::onAddActorVariable(MNWPActorVariable *var)
{
    Q_UNUSED(var);
    updateVariableEnums();
}


void MSkewTActor::onChangeActorVariable(MNWPActorVariable *var)
{
    Q_UNUSED(var);
    updateVariableEnums();
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
    LOG4CPLUS_DEBUG(mlog, str.toStdString());
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MSkewTActor::updateVariableEnums(MNWPActorVariable *deletedVar)
{
    // In the following the list of variable names will be updated.
    varNameList.clear();
    varNameList << "-";
    int deletedVarIndex = variables.size();
    for (MNWPActorVariable *var : variables)
    {
        if (var == deletedVar)
        {
            deletedVarIndex = varNameList.size();
            continue;
        }
        QStringList ensembleModes =
                properties->mEnum()->enumNames(var->ensembleModeProperty);
        int ensembleMode =
                properties->mEnum()->value(var->ensembleModeProperty);
        varNameList << var->variableName + "(" +
                       ensembleModes.at(ensembleMode) + ")";
    }

    // In the following the variable list properties are updated.
    enableActorUpdates(false);
    for (int i = 0; i < diagramConfiguration.varConfigs.size(); i++)
    {
        VariableConfig var = diagramConfiguration.varConfigs.at(i);

        // When deleting a variable, the indices of all variables which were
        // listed below that variable will decrease by one to fill the "free
        // space".
        if (var.index > deletedVarIndex)
        {
            var.index--;
        }
        properties->mEnum()->setEnumNames(var.property, varNameList);
        properties->mEnum()->setValue(var.property, var.index);
    }
    enableActorUpdates(true);
}


void MSkewTActor::drawDiagramHandle(MSceneViewGLWidget *sceneView)
{
    // Bind shader program.
    positionSpheresShader->bindProgram("UsePosition");

    // Set MVP-matrix and parameters to map pressure to world space in the
    // vertex shader.
    positionSpheresShader->setUniformValue("mvpMatrix",
                           *(sceneView->getModelViewProjectionMatrix()));
    positionSpheresShader->setUniformValue("pToWorldZParams",
                           sceneView->pressureToWorldZParameters());
    positionSpheresShader->setUniformValue("lightDirection",
                           sceneView->getLightDirection());
    positionSpheresShader->setUniformValue("cameraPosition",
                           sceneView->getCamera()->getOrigin());
    positionSpheresShader->setUniformValue("cameraUpDir",
                           sceneView->getCamera()->getYAxis());
    positionSpheresShader->setUniformValue(
                "radius", GLfloat(MSystemManagerAndControl::getInstance()
                                  ->getHandleSize()));
    positionSpheresShader->setUniformValue("scaleRadius", GLboolean(true));

    positionSpheresShader->setUniformValue("position",
                                           diagramConfiguration.geoPosition);

    // Texture bindings for transfer function for data scalar (1D texture from
    // transfer function class). The data scalar is stored in the vertex.w
    // component passed to the vertex shader.
    positionSpheresShader->setUniformValue("useTransferFunction",
                                           GLboolean(false));

    positionSpheresShader->setUniformValue("constColour", QColor(Qt::white));
    if (diagramConfiguration.overDragHandle)
    {
        positionSpheresShader->setUniformValue("constColour", QColor(Qt::red));
    }

    glPolygonMode(GL_FRONT_AND_BACK, renderAsWireFrame ? GL_LINE : GL_FILL);
    glLineWidth(1);
    glDrawArrays(GL_POINTS, 0, 1);
    // Unbind VBO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void MSkewTActor::copyDiagramConfigurationFromQtProperties()
{
    diagramConfiguration.pressureEqualsWorldPressure =
            properties->mBool()->value(alignWithWorldPressureProperty);
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

    normalscreenDiagrammConfiguration.pressureEqualsWorldPressure =
            diagramConfiguration.pressureEqualsWorldPressure;
    fullscreenDiagrammConfiguration.pressureEqualsWorldPressure = false;

    diagramConfiguration.drawInPerspective
            = properties->mBool()->value(perspectiveRenderingProperty);
    diagramConfiguration.init();
    normalscreenDiagrammConfiguration.init(&diagramConfiguration, "_normal");
    fullscreenDiagrammConfiguration.init(&diagramConfiguration, "_fullscreen");

    // After the configuration has been copied from the properties, recompute
    // the (T, log(p)) to (x, y) transformation matrix to transform (T, p)
    // coordinates into (x, y) coordinates.
    computeTlogp2xyTransformationMatrix();
}


void MSkewTActor::generateDiagramGeometry(
        GL::MVertexBuffer** vbDiagramVertices,
        ModeSpecificDiagramConfiguration *config)
{
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

//TODO (mr, 09Jan2019) -- make levels user-customizable.
    QList<int> pressureLevels;
    pressureLevels << 1 << 10 << 50 << 100 << 200 << 300 << 400 << 500
                   << 600 << 700 << 800 << 900 << 1000;
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
    // might need to be adjusted later.
    for (float isothermTemperature = diagramTmin_K - skewFactor * diagramTRange_K;
         isothermTemperature <= diagramTmax_K;
         isothermTemperature += config->dconfig->isothermSpacing)
    {
        // Generate vertex at (isotherm temperature, bottom pressure).
        QVector2D tpCoordinate_K_hPa = QVector2D(
                    isothermTemperature, config->dconfig->vertical_p_hPa.min);
        vStart = transformTp2xy(tpCoordinate_K_hPa);
        // Generate vertex at (isotherm temperature, top pressure).
        tpCoordinate_K_hPa.setY(config->dconfig->vertical_p_hPa.max);
        vEnd = transformTp2xy(tpCoordinate_K_hPa);
        vertexArray << vStart << vEnd;
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


#define SHADER_VERTEX_ATTRIBUTE 0
#define SHADER_TEXTURE_ATTRIBUTE 1

void MSkewTActor::drawDiagram(MSceneViewGLWidget *sceneView,
                              GL::MVertexBuffer* vbDiagramVertices,
                              ModeSpecificDiagramConfiguration *config)
{
    glLineWidth(2.f);
    skewTShader->bindProgram("DiagramTubes");
    setShaderGeneralVars(sceneView, config);
    if (properties->mBool()->value(dewPointShowProbabilityTubeProperty))
    {
        if ((diagramConfiguration.varConfigs[variablesIndices.dewPoint.MINIMUM]
             .index > 0)
                && (diagramConfiguration.varConfigs[
                    variablesIndices.dewPoint.MAXIMUM].index > 0))
        {
            QColor color = properties->mColor()->value(
                               dewPointMinMaxVariableColorProperty);
            drawProbabilityTube(
                        diagramConfiguration.varConfigs[
                        variablesIndices.dewPoint.MAXIMUM].variable,
                    diagramConfiguration.varConfigs[
                    variablesIndices.dewPoint.MINIMUM].variable,
                    true, color);
        }
    }
    config->layer -= 0.001f;
    skewTShader->setUniformValue("layer", GLfloat(config->layer));
    if (properties->mBool()->value(temperatureShowProbabilityTubeProperty))
    {
        if ((diagramConfiguration.varConfigs[
             variablesIndices.temperature.MINIMUM].index > 0 )
                && (diagramConfiguration.varConfigs[
                    variablesIndices.temperature.MAXIMUM].index > 0))
        {
            QColor color = properties->mColor()->value(
                        temperatureMinMaxVariableColorProperty);
            drawProbabilityTube(
                        diagramConfiguration.varConfigs[
                        variablesIndices.temperature.MAXIMUM].variable,
                    diagramConfiguration.varConfigs[
                    variablesIndices.temperature.MINIMUM].variable,
                    false, color);
        }
    }
    skewTShader->bindProgram("DiagramDeviation");
    setShaderGeneralVars(sceneView, config);
    if (properties->mBool()->value(
                dewPointShowDeviationTubeProperty))
    {
        if ((diagramConfiguration.varConfigs[variablesIndices.dewPoint.DEVIATION]
             .index > 0)
                && (diagramConfiguration.varConfigs[
                    variablesIndices.dewPoint.MEAN].index > 0))
        {
            QColor cDeviation = diagramConfiguration.varConfigs[
                    variablesIndices.dewPoint.DEVIATION].color;
            drawDeviation(diagramConfiguration.varConfigs[
                           variablesIndices.dewPoint.MEAN].variable,
                          diagramConfiguration.varConfigs[
                           variablesIndices.dewPoint.DEVIATION].variable,
                          true, cDeviation);
        }
    }
    config->layer -= 0.001f;
    skewTShader->setUniformValue("layer", GLfloat(config->layer));
    if (properties->mBool()->value(temperatureShowDeviationTubeProperty))
    {
        if ((diagramConfiguration.varConfigs[
             variablesIndices.temperature.DEVIATION].index > 0)
                && (diagramConfiguration.varConfigs[
                    variablesIndices.temperature.MEAN].index > 0))
        {
            QColor cDeviation = diagramConfiguration.varConfigs[
                    variablesIndices.temperature.DEVIATION].color;
            drawDeviation(diagramConfiguration.varConfigs[
                           variablesIndices.temperature.MEAN].variable,
                          diagramConfiguration.varConfigs[
                           variablesIndices.temperature.DEVIATION].variable,
                          false, cDeviation);
        }
    }

    skewTShader->bindProgram("DiagramVariables");
    setShaderGeneralVars(sceneView, config);
    skewTShader->setUniformValue("drawHumidity"   , false);
    skewTShader->setUniformValue("drawTemperature", false);
    // Refering the structured grid differs for spaghetti plots and all other
    // plots thus a variable is used to store the pointer to the grid used.
    MStructuredGrid *grid;
    // List of ensemble member grids (needed to draw spaghetti plots).
    QList<MStructuredGrid*> grids;
    for (int vi = 0; vi < diagramConfiguration.varConfigs.size(); vi++)
    {
        VariableConfig vc = diagramConfiguration.varConfigs.at(vi);
        int variableIndex = vc.index;
        if (variableIndex <= 0) continue;
        if (vi != variablesIndices.dewPoint.DEVIATION
                && vi != variablesIndices.temperature.DEVIATION)
        {
            MNWPSkewTActorVariable *var = vc.variable;
            if (var != nullptr)
            {
                if ((vi == variablesIndices.dewPoint.SPAGHETTI
                        || vi == variablesIndices.temperature.SPAGHETTI))
                {
                    if (var->gridAggregation == nullptr)
                    {
                        continue;
                    }
                    else
                    {
                        grids = var->gridAggregation->getGrids();
                        // Use first grid as reference for everything needed
                        // and which is the same for all members.
                        grid = grids.at(0);
                    }
                }
                else
                {
                    grid = var->grid;
                }

                if (grid == nullptr)
                {
                    continue;
                }

                if (vi >= variablesIndices.dewPoint.MEMBER
                        && vi <= variablesIndices.dewPoint.SPAGHETTI)
                {
                    skewTShader->setUniformValue("drawHumidity"   , true);
                    skewTShader->setUniformValue("drawTemperature", false);
                }
                if (vi >= variablesIndices.temperature.MEMBER
                        && vi <= variablesIndices.temperature.SPAGHETTI)
                {
                    skewTShader->setUniformValue("drawHumidity"   , false);
                    skewTShader->setUniformValue("drawTemperature", true);
                }
                skewTShader->setUniformValue("colour", var->color);
                glLineWidth(float(var->thickness));
                config->layer -= 0.001f;
                skewTShader->setUniformValue("layer",
                                             config->layer);
                skewTShader->setUniformValue("levelType",
                                             int(grid->getLevelType()));
                // Texture bindings for coordinate axes (1D texture).
                var->textureLonLatLevAxes->bindToTextureUnit(
                            var->textureUnitLonLatLevAxes);

                skewTShader->setUniformValue(
                            "lonLatLevAxes", var->textureUnitLonLatLevAxes);

                if (grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
                {
                    // Texture bindings for surface pressure (2D texture) and
                    // model level coefficients (1D texture).
                    var->textureSurfacePressure->bindToTextureUnit(
                                var->textureUnitSurfacePressure);
                    skewTShader->setUniformValue(
                                "surfacePressure",
                                var->textureUnitSurfacePressure);
                    var->textureHybridCoefficients->bindToTextureUnit(
                                var->textureUnitHybridCoefficients);
                    skewTShader->setUniformValue(
                                "hybridCoefficients",
                                var->textureUnitHybridCoefficients);
                }
                if ((vi != variablesIndices.dewPoint.SPAGHETTI
                        && vi != variablesIndices.temperature.SPAGHETTI))
                {
                    if (grid->getLevelType() == SURFACE_2D)
                    {
                        // Texture bindings for data field (2D texture).
                        var->textureDataField->bindToTextureUnit(
                                    var->textureUnitDataField);
                        skewTShader->setUniformValue(
                                    "dataField2D", var->textureUnitDataField);

                    }
                    else
                    {
                        glEnable(GL_LINE_SMOOTH);
                        glHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
                        // Texture bindings for data field (3D texture).
                        var->textureDataField->bindToTextureUnit(
                                    var->textureUnitDataField);
                        skewTShader->setUniformValue(
                                    "dataField", var->textureUnitDataField);
                    }
                }
                if (vi == variablesIndices.dewPoint.SPAGHETTI
                        || vi == variablesIndices.temperature.SPAGHETTI)
                {
                    skewTShader->setUniformValue(
                                "numberOfLevels", grid->getNumLevels());
                    skewTShader->setUniformValue(
                                "numberOfLats"  , grid->getNumLats());
                    glLineWidth(float(var->thickness));

                    if (var->transferFunction != nullptr)
                    {
                        var->transferFunction->getTexture()->bindToTextureUnit(
                                    var->textureUnitTransferFunction);
                        skewTShader->setUniformValue(
                                    "useTransferFunction", true);
                        skewTShader->setUniformValue(
                                    "transferFunction",
                                    var->textureUnitTransferFunction);
                        skewTShader->setUniformValue(
                                    "scalarMinimum",
                                    var->transferFunction->getMinimumValue());
                        skewTShader->setUniformValue(
                                    "scalarMaximum",
                                    var->transferFunction->getMaximumValue());
                    }
                    else
                    {
                        skewTShader->setUniformValue(
                                    "useTransferFunction", true);
                         skewTShader->setUniformValue("scalarMinimum", 0.f);
                         skewTShader->setUniformValue("scalarMaximum", 0.f);
                         skewTShader->setUniformValue("colour", var->color);
                    }
                    // To avoid z fighting first render all spaghetti contours
                    // into the stencil buffer and updating the depth buffer
                    // but without changing the colour buffer. In a second
                    // render pass update the colour buffer using stencil test
                    // but without performing depth test and writes.
                    // (as mentioned here: https://stackoverflow.com/questions/14842808/preventing-z-fighting-on-coplanar-polygons#14843885)
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    for (int i = 0; i < grids.size(); i++)
                    {
                        if (grid->getLevelType() == SURFACE_2D)
                        {
                            // Texture bindings for data field (2D texture).
                            grids.at(i)->getTexture()->bindToTextureUnit(
                                        var->textureUnitDataField);
                            skewTShader->setUniformValue(
                                        "dataField2D", var->textureUnitDataField);

                        }
                        else
                        {
                            // Texture bindings for data field (3D texture).
                            grids.at(i)->getTexture()->bindToTextureUnit(
                                        var->textureUnitDataField);
                            skewTShader->setUniformValue(
                                        "dataField", var->textureUnitDataField);
                        }
                        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                        skewTShader->setUniformValue("ensemble", i);
                        glDrawArrays(GL_LINE_STRIP, 0, grid->getNumLevels());
                        grids.at(i)->releaseTexture();
                    }
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glEnable(GL_STENCIL_TEST);
                    glDisable(GL_DEPTH_TEST);
                    glDepthMask(GL_FALSE);
                    for (int i = 0; i < grids.size(); i++)
                    {
                        if (grid->getLevelType() == SURFACE_2D)
                        {
                            // Texture bindings for data field (2D texture).
                            grids.at(i)->getTexture()->bindToTextureUnit(
                                        var->textureUnitDataField);
                            skewTShader->setUniformValue(
                                        "dataField2D", var->textureUnitDataField);

                        }
                        else
                        {
                            // Texture bindings for data field (3D texture).
                            grids.at(i)->getTexture()->bindToTextureUnit(
                                        var->textureUnitDataField);
                            skewTShader->setUniformValue(
                                        "dataField", var->textureUnitDataField);
                        }
                        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                        skewTShader->setUniformValue("ensemble", i);
                        glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
                        glDrawArrays(GL_LINE_STRIP, 0, grid->getNumLevels());
                        glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
                        grids.at(i)->releaseTexture();
                    }
                    glEnable(GL_DEPTH_TEST);
                    glDepthMask(GL_TRUE);
                    glDisable(GL_STENCIL_TEST);
                }
                else
                {

                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glDrawArrays(GL_LINE_STRIP, 0,
                                 grid->getNumLevels()); CHECK_GL_ERROR;
                }
            }
        }
    }

    // Draw observational data from U of Wyoming web service.
    // ======================================================
    if (wyomingVerticesCount > 0)
    {
        glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);

        vbWyomingVertices->attachToVertexAttribute(
            SHADER_VERTEX_ATTRIBUTE, 3,
            GL_FALSE, 0, (const GLvoid *)(0 * sizeof(float)));
        skewTShader->bindProgram("WyomingTestData");
        setShaderGeneralVars(sceneView, config);
        skewTShader->setUniformValue("colour", QVector4D(0.f, 128.f, 0.f, 1.f));
        skewTShader->setUniformValue("drawHumidity"   , false);
        skewTShader->setUniformValue("drawTemperature", true);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawArrays(GL_LINE_STRIP, 0, wyomingVerticesCount);

        skewTShader->setUniformValue("colour",
                                     QVector4D(128.f, 128.f, 0.f, 1.f));
        skewTShader->setUniformValue("drawTemperature", false);
        skewTShader->setUniformValue("drawHumidity"   , true);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawArrays(GL_LINE_STRIP, 0, wyomingVerticesCount);
    }
}


void MSkewTActor::drawProbabilityTube(
    MNWPSkewTActorVariable *max, MNWPSkewTActorVariable *min, bool isHumidity,
    QColor color)
{
    if (min->grid == nullptr || max->grid == nullptr)
    {
        return;
    }
    skewTShader->setUniformValue("ensemble", -1);

    if (isHumidity)
    {
        skewTShader->setUniformValue("drawHumidity"   , true);
        skewTShader->setUniformValue("drawTemperature", false);
    }
    else
    {
        skewTShader->setUniformValue("drawHumidity"   , false);
        skewTShader->setUniformValue("drawTemperature", true);
    }

    skewTShader->setUniformValue("colour", color);

    skewTShader->setUniformValue("levelTypeMax", int(max->grid->getLevelType()));
    skewTShader->setUniformValue("levelTypeMin", int(min->grid->getLevelType()));

    max->textureLonLatLevAxes->bindToTextureUnit( max->textureUnitLonLatLevAxes);
    min->textureLonLatLevAxes->bindToTextureUnit( min->textureUnitLonLatLevAxes);

    skewTShader->setUniformValue("lonLatLevAxesMax",
                                 max->textureUnitLonLatLevAxes);
    skewTShader->setUniformValue("lonLatLevAxesMin",
                                 max->textureUnitLonLatLevAxes);
    if (max->grid->getLevelType() == SURFACE_2D)
    {
        // Texture bindings for data field (2D texture).
        max->textureDataField->bindToTextureUnit(max->textureUnitDataField);
        skewTShader->setUniformValue("dataField2DMax", max->textureUnitDataField);
    }
    else
    {
        // Texture bindings for data field (3D texture).
        max->textureDataField->bindToTextureUnit(max->textureUnitDataField);
        skewTShader->setUniformValue("dataFieldMax", max->textureUnitDataField);
    }

    if (min->grid->getLevelType() == SURFACE_2D)
    {
        // Texture bindings for data field (2D texture).
        min->textureDataField->bindToTextureUnit(min->textureUnitDataField);
        skewTShader->setUniformValue("dataField2DMin", min->textureUnitDataField);
    }
    else
    {
        // Texture bindings for data field (3D texture).
        min->textureDataField->bindToTextureUnit(min->textureUnitDataField);
        skewTShader->setUniformValue("dataFieldMin", min->textureUnitDataField);
    }

    if (max->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Texture bindings for surface pressure (2D texture) and model
        // level coefficients (1D texture).
        max->textureSurfacePressure->bindToTextureUnit(
                                max->textureUnitSurfacePressure);
        max->textureHybridCoefficients->bindToTextureUnit(
                                max->textureUnitHybridCoefficients);
        skewTShader->setUniformValue("surfacePressureMax",
                                max->textureUnitSurfacePressure);
        skewTShader->setUniformValue("hybridCoefficientsMax",
                                max->textureUnitHybridCoefficients);
    }

    if (min->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Texture bindings for surface pressure (2D texture) and model
        // level coefficients (1D texture).
        min->textureSurfacePressure->bindToTextureUnit(
                                min->textureUnitSurfacePressure);
        min->textureHybridCoefficients->bindToTextureUnit(
                                min->textureUnitHybridCoefficients);
        skewTShader->setUniformValue("surfacePressureMin",
                                min->textureUnitSurfacePressure);
        skewTShader->setUniformValue("hybridCoefficientsMin",
                                min->textureUnitHybridCoefficients);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLE_STRIP, 0,
                 min->grid->getNumLevels() * 2); CHECK_GL_ERROR;
}


void MSkewTActor::drawDeviation(
    MNWPSkewTActorVariable *mean, MNWPSkewTActorVariable *deviation,
    bool isHumidity, QColor deviationColor)
{
    skewTShader->setUniformValue("ensemble", -1);

    if (isHumidity)
    {
        skewTShader->setUniformValue("drawHumidity"   , true);
        skewTShader->setUniformValue("drawTemperature", false);
    }
    else
    {
        skewTShader->setUniformValue("drawHumidity"   , false);
        skewTShader->setUniformValue("drawTemperature", true);
    }

    skewTShader->setUniformValue("colour", deviationColor);

    skewTShader->setUniformValue("levelTypeMean",
                                 int(mean->grid->getLevelType()));
    skewTShader->setUniformValue("levelTypeDeviation",
                                 int(deviation->grid->getLevelType()));
    mean->textureLonLatLevAxes->bindToTextureUnit(
                mean->textureUnitLonLatLevAxes);
    deviation->textureLonLatLevAxes->bindToTextureUnit(
                deviation->textureUnitLonLatLevAxes);
    skewTShader->setUniformValue("lonLatLevAxesMean",
                                 mean->textureUnitLonLatLevAxes);
    skewTShader->setUniformValue("lonLatLevAxesDeviation",
                                 deviation->textureUnitLonLatLevAxes);
    if (mean->grid->getLevelType() == SURFACE_2D)
    {
        // Texture bindings for data field (2D texture).
        mean->textureDataField->bindToTextureUnit(
                    mean->textureUnitDataField);
        skewTShader->setUniformValue(
                    "dataField2DMean", mean->textureUnitDataField);
    }
    else
    {
        // Texture bindings for data field (3D texture).
        mean->textureDataField->bindToTextureUnit(
                    mean->textureUnitDataField);
        skewTShader->setUniformValue(
                    "dataFieldMean", mean->textureUnitDataField);
    }

    if (deviation->grid->getLevelType() == SURFACE_2D)
    {
        // Texture bindings for data field (2D texture).
        deviation->textureDataField->bindToTextureUnit(
                    deviation->textureUnitDataField);
        skewTShader->setUniformValue(
                    "dataField2DDeviation", deviation->textureUnitDataField);
    }
    else
    {
        // Texture bindings for data field (3D texture).
        deviation->textureDataField->bindToTextureUnit(
                    deviation->textureUnitDataField);
        skewTShader->setUniformValue(
                    "dataFieldDeviation", deviation->textureUnitDataField);
    }

    if (mean->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Texture bindings for surface pressure (2D texture) and model
        // level coefficients (1D texture).
        mean->textureSurfacePressure->bindToTextureUnit(
                    mean->textureUnitSurfacePressure);
        mean->textureHybridCoefficients->bindToTextureUnit(
                    mean->textureUnitHybridCoefficients);
        skewTShader->setUniformValue("surfacePressureMean",
                                     mean->textureUnitSurfacePressure);
        skewTShader->setUniformValue("hybridCoefficientsMean",
                                     mean->textureUnitHybridCoefficients);
    }

    if (deviation->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
    {
        // Texture bindings for surface pressure (2D texture) and model
        // level coefficients (1D texture).
        deviation->textureSurfacePressure->bindToTextureUnit(
                    deviation->textureUnitSurfacePressure);
        deviation->textureHybridCoefficients->bindToTextureUnit(
                    deviation->textureUnitHybridCoefficients);
        skewTShader->setUniformValue("surfacePressureDeviation",
                                     deviation->textureUnitSurfacePressure);
        skewTShader->setUniformValue("hybridCoefficientsDeviation",
                                     deviation->textureUnitHybridCoefficients);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, mean->grid->getNumLevels() * 2);
}


void MSkewTActor::drawDiagramGeometryAndLabels(
        MSceneViewGLWidget *sceneView, GL::MVertexBuffer *vbDiagramVertices,
        ModeSpecificDiagramConfiguration *config)
{
    removeAllLabels();
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    MTextManager *tm = glRM->getTextManager();

    // Changing the member variable "labels" during a render call seems to
    // cause a segmentation fault during the call of renderLabelList()
    // sometimes. Thus use own label-lists for skew-t actors (one per scene
    // view, since every scene view might place the labels differently).
    QList<MLabel*> labels = skewTLabels[sceneView];
    // Remove all labels from text manager.
    const int numLabels = labels.size();

    for (int i = 0; i < numLabels; ++i)
    {
        tm->removeText(labels.takeLast());
    }

    if (!properties->mBool()->value(labelsEnabledProperty))
    {
        return;
    }

    // Somehow when using the member variables directly, some labels are missing.
    int labelSize   = this->labelSize;
    QColor labelColour = this->labelColour;
    bool labelBBox       = this->labelBBox;
    QColor labelBBoxColour = this->labelBBoxColour;

    glEnableVertexAttribArray(SHADER_VERTEX_ATTRIBUTE);
    vbDiagramVertices->attachToVertexAttribute(
        SHADER_VERTEX_ATTRIBUTE, 2,
        GL_FALSE, 0, (const GLvoid *)(0 * sizeof(float)));

    float diagramWorldZOffset = 0.f;
    float yOffset = 0.f;

    if (!config->pressureEqualsWorldPressure)
    {
        yOffset = 0.1f;
    }

    // Draw background of diagram.
    // ===========================
    if (!renderAsWireFrame)
    {
        skewTShader->bindProgram("DiagramBackground");
        skewTShader->setUniformValue("yOffset", (GLfloat)yOffset);
        setShaderGeneralVars(sceneView, config);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_POINTS, 0, 1);
    }

    // Bind shader for diagram geometry.
    // =================================
    skewTShader->bindProgram("DiagramVertices");
    setShaderGeneralVars(sceneView, config);

    // Draw diagram frame.
    // ===================
    glLineWidth(3);
    skewTShader->setUniformValue("colour", diagramConfiguration.diagramColor);
    glDrawArrays(GL_LINES,
                 config->vertexArrayDrawRanges.frame.startIndex,
                 config->vertexArrayDrawRanges.frame.indexCount);

    glLineWidth(1.f);

    // Draw dry adiabates.
    // ===================
    if (diagramConfiguration.drawDryAdiabates)
    {
        skewTShader->setUniformValue("colour", QVector4D(0.8f, 0.8f, 0.f, 1.f));
        glDrawArrays(GL_LINES,
                     config->vertexArrayDrawRanges.dryAdiabates.startIndex,
                     config->vertexArrayDrawRanges.dryAdiabates.indexCount);
    }

    // Draw isobars.
    // =============
    skewTShader->setUniformValue("colour", diagramConfiguration.diagramColor);
    glDrawArrays(GL_LINES,
                 config->vertexArrayDrawRanges.isobars.startIndex,
                 config->vertexArrayDrawRanges.isobars.indexCount);

    // Draw isotherms.
    // ===============
    skewTShader->setUniformValue("colour", QVector4D(1.f, 0.f, 0.f, 1.f));
    glDrawArrays(GL_LINES,
                 config->vertexArrayDrawRanges.isotherms.startIndex,
                 config->vertexArrayDrawRanges.isotherms.indexCount);

    // Draw moist adiabates.
    // =====================
    if (diagramConfiguration.drawMoistAdiabates)
    {
        skewTShader->setUniformValue("colour", QVector4D(0.f, 0.8f, 0.f, 1.f));
        glDrawArrays(GL_LINES,
                     config->vertexArrayDrawRanges.moistAdiabates.startIndex,
                     config->vertexArrayDrawRanges.moistAdiabates.indexCount);
    }


    // Draw mouse cross and legend in fullscreen mode.
    // ===============================================
    if (sceneViewFullscreenEnabled.value(sceneView)
            && sceneView->interactionModeEnabled()
            && (diagramConfiguration.clipTo2D(config->clipPos.x())
                >= config->drawingRegionClipSpace.left)
            && (diagramConfiguration.clipTo2D(config->clipPos.x())
                <= config->drawingRegionClipSpace.right)
            && (diagramConfiguration.clipTo2D(config->clipPos.y())
                >= config->drawingRegionClipSpace.bottom)
            && (diagramConfiguration.clipTo2D(config->clipPos.y())
                <= config->drawingRegionClipSpace.top))
    {
        skewTShader->bindProgram("LegendBackground");
        setShaderGeneralVars(sceneView, config);
        glDrawArrays(GL_POINTS, 0, 1);
        float realZ = ((config->clipPos.y() + 1.f) / 2.f -
                       config->drawingRegionClipSpace.bottom) * 36.f;
        float pressure = config->pressureFromWorldZ(realZ, diagramConfiguration);
        float temperature = (((config->clipPos.x()) + 1.f) / 2.f
                             + config->drawingRegionClipSpace.left
                             - (config->clipPos.y() + 1.f) / 2.f
                             - config->drawingRegionClipSpace.bottom) /
                            (config->drawingRegionClipSpace.right
                             - config->drawingRegionClipSpace.left) *
                            diagramConfiguration.temperature_degC.amplitude() -
                            diagramConfiguration.temperature_degC.center();
        glLineWidth(1.f);
        skewTShader->bindProgram("MarkingCircles");
        skewTShader->setUniformValue("clipPos", config->clipPos);
        setShaderGeneralVars(sceneView, config);
        int humidityVar       = diagramConfiguration.varConfigs[
                variablesIndices.dewPoint.MEMBER].index;
        int tempertureVar     = diagramConfiguration.varConfigs[
                variablesIndices.temperature.MEMBER].index;
        int humidityMeanVar   = diagramConfiguration.varConfigs[
                variablesIndices.dewPoint.MEAN].index;
        int tempertureMeanVar = diagramConfiguration.varConfigs[
                variablesIndices.temperature.MEAN].index;
//        float tempX = 0.f      , humidityX = 0.f;
        float tempVal = M_MISSING_VALUE, humidityVal = M_MISSING_VALUE;
//        float tempMeanX = 0.f  , humidityMeanX = 0.f;
        float tempMeanVal = M_MISSING_VALUE, humidityMeanVal = M_MISSING_VALUE;
        for (int vi = 0; vi < diagramConfiguration.varConfigs.size(); vi++)
        {
            VariableConfig var = diagramConfiguration.varConfigs.at(vi);
            if (var.variable == nullptr)
            {
                continue;
            }
            if (vi == variablesIndices.dewPoint.MEMBER
                    || vi == variablesIndices.dewPoint.MEAN)
            {
                float q = var.variable->grid->interpolateValue(
                            diagramConfiguration.geoPosition.x(),
                            diagramConfiguration.geoPosition.y(), pressure);
                // Mixing ratio.
                float w = q / (1.f - q);
                // Compute vapour pressure from pressure and mixing ratio
                // (Wallace and Hobbs 2nd ed. eq. 3.59).
                // (p_hPa * 100) = conversion to pascal
                float e_q = w / (w + 0.622f) * (pressure * 100.f);
                // Method is Bolton.
                float td = 243.5f / (17.67f / log(e_q / 100.f / 6.112f) - 1.f);
//                float x = diagramConfiguration.scaleTemperatureToDiagramSpace(
//                            td + 273.5f) * config->area.width();
                if (vi == variablesIndices.dewPoint.MEAN)
                {
                    humidityMeanVal = td;
//                    humidityMeanX = (x * 2.f) - 0.1f;
                }
                if (vi == variablesIndices.dewPoint.MEMBER)
                {
                    humidityVal = td;
//                    humidityX = (x * 2.f) - 0.1f;
                }
                skewTShader->setUniformValue("humidityColour" , var.color);
                skewTShader->setUniformValue("humidityVal"    , td);
                skewTShader->setUniformValue("drawHumidity"   , true);
                skewTShader->setUniformValue("drawTemperature", false);
                glDrawArrays(GL_POINTS, 0, 1);
            }
            if (vi == variablesIndices.temperature.MEAN
                    || vi == variablesIndices.temperature.MEMBER)
            {
                float val = var.variable->grid->interpolateValue(
                            diagramConfiguration.geoPosition.x(),
                            diagramConfiguration.geoPosition.y(), pressure);
//                float x = ((val - 273.15f
//                            + diagramConfiguration.temperature.center())
//                           / diagramConfiguration.temperature.amplitude())
//                        * (config->area.right
//                           - config->area.left);
                if (vi == variablesIndices.temperature.MEAN)
                {
                    tempMeanVal = val;
//                    tempMeanX = (x * 2.f) - 0.1f;
                }
                if (vi == variablesIndices.temperature.MEMBER)
                {
                    tempVal = val;
//                    tempX = (x * 2.f) - 0.1f;
                }
                skewTShader->setUniformValue("temperatureVal"   , val);
                skewTShader->setUniformValue("drawHumidity"     , false);
                skewTShader->setUniformValue("drawTemperature"  , true);
                skewTShader->setUniformValue("temperatureColour", var.color);
                glDrawArrays(GL_POINTS, 0, 1);
            }
        }

        skewTShader->bindProgram("MouseOverCross");
        setShaderGeneralVars(sceneView, config);
        skewTShader->setUniformValue("clipPos", config->clipPos);
        skewTShader->setUniformValue("colour" , QVector4D(0.f, 1.f, 0.f, 1.f));
        glDrawArrays(GL_LINES, 0, 2);

        float topShift = 0.f;
        if (tempertureVar != -1)
        {
            QColor tempColor = diagramConfiguration.varConfigs[
                    variablesIndices.temperature.MEMBER].color;
            labels.append(tm->addText(
                              QString("Temperature (single member): %1 deg C").arg(
                                  round((tempVal - 273.15f) * 100.f) / 100.f),
                              MTextManager::CLIPSPACE,
                              config->drawingRegionClipSpace.left - 0.9f,
                              config->drawingRegionClipSpace.top - 0.15f + topShift,
                              -0.99f, 16.f, tempColor, MTextManager::BASELINELEFT,
                              true, QColor(255, 255, 255, 255)));
            topShift = topShift - 0.1f;
        }

        if (humidityVar != -1)
        {
            QColor humidityColor = diagramConfiguration.varConfigs[
                    variablesIndices.temperature.MEAN].color;
            labels.append(tm->addText(
                              QString("Dew point (single member): %1 deg C").arg(
                                  round((humidityVal) * 100.f) / 100.f),
                              MTextManager::CLIPSPACE,
                              config->drawingRegionClipSpace.left - 0.9f,
                              config->drawingRegionClipSpace.top - 0.15f + topShift,
                              -0.99f, 16.f, humidityColor,
                              MTextManager::BASELINELEFT, true,
                              QColor(255, 255, 255, 255)));
            topShift = topShift - 0.1f;
        }

        if (tempertureMeanVar != -1)
        {
            QColor tempMeanColor = diagramConfiguration.varConfigs[
                    variablesIndices.dewPoint.MEMBER].color;
            labels.append(tm->addText(
                              QString("Temperature (ensemble mean): %1 deg C").arg(
                                  round((tempMeanVal - 273.15f) * 100.f) / 100.f),
                              MTextManager::CLIPSPACE,
                              config->drawingRegionClipSpace.left - 0.9f,
                              config->drawingRegionClipSpace.top - 0.15f + topShift,
                              -0.99f, 16.f, tempMeanColor,
                              MTextManager::BASELINELEFT, true,
                              QColor(255, 255, 255, 255)));
            topShift = topShift - 0.1f;
        }

        if (humidityMeanVar != -1)
        {
            QColor humidityMeanColor = diagramConfiguration.varConfigs[
                    variablesIndices.dewPoint.MEAN].color;
            labels.append(tm->addText(
                              QString("Dew point (ensemble mean): %1 deg C").arg(
                                  round((humidityMeanVal) * 100.f) / 100.f),
                              MTextManager::CLIPSPACE,
                              config->drawingRegionClipSpace.left - 0.9f,
                              config->drawingRegionClipSpace.top - 0.15f + topShift,
                              -0.99f, 16.f, humidityMeanColor,
                              MTextManager::BASELINELEFT, true,
                              QColor(255, 255, 255, 255)));
            topShift = topShift - 0.1f;
        }

        labels.append(tm->addText(
                          QString("p=%1").arg(
                              round((pressure) * 10.f) / 10.f),
                          MTextManager::CLIPSPACE,
                          config->drawingRegionClipSpace.left - 1.05f,
                          config->clipPos.y(), -0.99f, 16.f,
                          QColor(170, 0, 0, 255), MTextManager::BASELINELEFT,
                          true, QColor(255, 255, 255, 255)));

        labels.append(tm->addText(
                          QString("T=%1").arg(
                              round((temperature) * 10.f) / 10.f),
                          MTextManager::CLIPSPACE,
                          config->clipPos.x()
                          - config->clipPos.y()
                          - 1.f + config->drawingRegionClipSpace.left,
                          config->drawingRegionClipSpace.bottom - 1.035f, -0.99f,
                          16.f, QColor(170, 0, 0, 255), MTextManager::BASELINELEFT,
                          true, QColor(255, 255, 255, 255)));
    }

    if (config->pressureEqualsWorldPressure)
    {
        diagramWorldZOffset = -0.01f;
    }
    else
    {
        diagramWorldZOffset = 0.05f;
    }

    QMatrix4x4 view = sceneView->getCamera()->getViewMatrix();
    QVector3D cameraUp;

    if (!diagramConfiguration.drawInPerspective)
    {
        cameraUp = QVector3D(view.row(1).x(),
                             view.row(1).y(),
                             view.row(1).z());
    }
    else
    {
        cameraUp = QVector3D(0.f, 0.f, 1.f);
    }

    QVector3D cameraRight = QVector3D(view.row(0).x(),
                                      view.row(0).y(),
                                      view.row(0).z());
    QVector3D cameraFront = QVector3D(view.row(2).x(),
                                      view.row(2).y(),
                                      view.row(2).z());

    // Draw pressure labels.
    // =====================
    float worldZOfPressureLabel = config->worldZfromPressure(
                diagramConfiguration.vertical_p_hPa.min) + diagramWorldZOffset;
    QVector3D position = cameraUp * worldZOfPressureLabel * 36.f +
                         QVector3D(diagramConfiguration.geoPosition, 0.f)
                        - cameraRight * 0.02f * 36.f
                        + cameraFront * 0.05f;

//    if (sceneViewFullscreenEnabled.value(sceneView))
//    {
//        labels.append(tm->addText(QString("%1").arg(
//                                      (int) diagramConfiguration.pressure.min),
//                                  MTextManager::CLIPSPACE, -0.96f,
//                                  (act - 0.5f) * 2.f, -0.99f, labelSize,
//                                  labelColour, MTextManager::BASELINELEFT,
//                                  labelBBox, labelBBoxColour));
//    }
//    else
//    {
//        labels.append(tm->addText(QString("%1").arg(
//                                      (int) diagramConfiguration.pressure.min),
//                                  MTextManager::WORLDSPACE, position.x(),
//                                  position.y(), position.z(), labelSize,
//                                  labelColour, MTextManager::BASELINERIGHT,
//                                  labelBBox, labelBBoxColour));
//    }

    float bottom = sceneView->worldZfromPressure(
                diagramConfiguration.vertical_p_hPa.max) / 36.f;
    float top = sceneView->worldZfromPressure(
                diagramConfiguration.vertical_p_hPa.min) / 36.f;
    QList<float> pressureLevels;
    QString filler = "  ";
    pressureLevels << diagramConfiguration.vertical_p_hPa.min
                   << 1 << 10 << 50 << 100 << 200 << 300 << 400 << 500
                   << 600 << 700 << 800 << 900 << 1000;
    for (float pressureCount : pressureLevels)
    {
        if (pressureCount > diagramConfiguration.vertical_p_hPa.max)
        {
            break;
        }
        if (pressureCount < diagramConfiguration.vertical_p_hPa.min)
        {
            continue;
        }


        if (config->pressureEqualsWorldPressure)
        {
            worldZOfPressureLabel = sceneView->worldZfromPressure((double)pressureCount) / 36.;
        }
        else
        {
            worldZOfPressureLabel = config->worldZfromPressure((double)pressureCount);
        }
        worldZOfPressureLabel += diagramWorldZOffset;
        if (sceneViewFullscreenEnabled.value(sceneView))
        {
            if (pressureCount < 10.f)
            {
                filler = "      ";
            } else if (pressureCount < 100.f)
            {
                filler = "   ";
            }
            else
            {
                filler = "";
            }
            labels.append(tm->addText(filler + QString("%1").arg(pressureCount),
                                      MTextManager::CLIPSPACE,
                                      -0.98f, (worldZOfPressureLabel - 0.5f) * 2.f, -0.99f,
                                      labelSize, labelColour,
                                      MTextManager::BASELINELEFT,
                                      labelBBox, labelBBoxColour));
        }
        else
        {
            position = cameraUp * worldZOfPressureLabel * 36.f
                    + QVector3D(diagramConfiguration.geoPosition, 0.f)
                    - cameraRight * 0.02f * 36.f
                    + cameraFront * 0.05f;
            labels.append(tm->addText(QString("%1").arg(pressureCount),
                                      MTextManager::WORLDSPACE,
                                      position.x(), position.y(),
                                      position.z(), labelSize, labelColour,
                                      MTextManager::BASELINERIGHT,
                                      labelBBox, labelBBoxColour));
        }
    }

    // Draw temperature labels.
    // ========================
    float displayedTemperature = diagramConfiguration.temperature_degC.max;
    float isothermSpacingClipSpace = config->drawingRegionClipSpace.width() / 12.f;
    for (int i = 48; i > 0; i -= 2)
    {
        float x, y;
        if (config->pressureEqualsWorldPressure)
        {
            x = config->drawingRegionClipSpace.right - 0.01f;
            y = sceneView->worldZfromPressure(
                        config->worldZToPressure(
                            isothermSpacingClipSpace * (i / 2.f)
                            - config->drawingRegionClipSpace.width() + 0.045f));
            if (y >= bottom && y <= top)
            {
                if (sceneViewFullscreenEnabled.value(sceneView))
                {
                    labels.append(tm->addText(
                                      QString("%1").arg((int) displayedTemperature),
                                      MTextManager::CLIPSPACE,
                                      (x - 0.46f) * 2.f, (y - 0.5f) * 2.f,
                                      -0.99f, labelSize, labelColour,
                                      MTextManager::BASELINELEFT,
                                      labelBBox, labelBBoxColour));
                }
                else
                {
                    position = cameraUp * y * 36.f
                            + QVector3D(diagramConfiguration.geoPosition, 0.f)
                            + cameraRight * x * 36.f
                            + cameraFront * 0.05f;
                    labels.append(tm->addText(
                                      QString("%1").arg(-(int) displayedTemperature),
                                      MTextManager::WORLDSPACE,
                                      position.x(), position.y(), position.z(),
                                      labelSize, labelColour,
                                      MTextManager::BASELINELEFT,
                                      labelBBox, labelBBoxColour));
                }
            }
        }
        else
        {
            x = isothermSpacingClipSpace * (i / 2.f) - config->drawingRegionClipSpace.width()
                    + config->drawingRegionClipSpace.bottom;
            if (x <= config->drawingRegionClipSpace.left - 0.05f)
            {
                break;
            }
            if (x >= config->drawingRegionClipSpace.left - 0.05f
                    && x < config->drawingRegionClipSpace.right + 0.05f)
            {
                if (sceneViewFullscreenEnabled.value(sceneView))
                {
                    labels.append(tm->addText(
                                      QString("%1").arg((int) displayedTemperature),
                                      MTextManager::CLIPSPACE,
                                      (x - 0.5f) * 2.f, (y - 0.5f + 0.0085f)
                                      * 2.f, -0.99f, labelSize, labelColour,
                                      MTextManager::BASELINELEFT,
                                      labelBBox, labelBBoxColour));
                }
                else
                {
                    position = cameraRight * (x - 0.06f) * 36.f
                            + QVector3D(diagramConfiguration.geoPosition, 0.f)
                            + cameraUp
                            * (config->drawingRegionClipSpace.bottom - 0.05f) * 36.f
                            + cameraFront * 0.05f;
                    labels.append(tm->addText(
                                      QString("%1").arg((int) displayedTemperature),
                                      MTextManager::WORLDSPACE,
                                      position.x(), position.y(), position.z(),
                                      labelSize, labelColour,
                                      MTextManager::BASELINELEFT,
                                      labelBBox, labelBBoxColour));
                }
            }
        }

        displayedTemperature -= diagramConfiguration.temperature_degC.amplitude() / 12.f;
    }
    sceneView->makeCurrent();

    skewTLabels.insert(sceneView, labels);
    tm->renderLabelList(sceneView, labels);
}


void MSkewTActor::setShaderGeneralVars(
    MSceneViewGLWidget *sceneView, ModeSpecificDiagramConfiguration *config)
{
    skewTShader->setUniformValue("fullscreen",
                                 sceneViewFullscreenEnabled.value(sceneView));
    skewTShader->setUniformValue("uprightOrientation",
                                 diagramConfiguration.drawInPerspective);

    skewTShader->setUniformValue("area.left", config->drawingRegionClipSpace.left);
    skewTShader->setUniformValue("area.right", config->drawingRegionClipSpace.right);
    if (config->pressureEqualsWorldPressure)
    {
        float p = diagramConfiguration.vertical_p_hPa.min;
        skewTShader->setUniformValue("area.top",
                                     GLfloat(sceneView->worldZfromPressure(p) / 36.0));
        p = diagramConfiguration.vertical_p_hPa.max;
        skewTShader->setUniformValue("area.bottom",
                                     GLfloat(sceneView->worldZfromPressure(p) / 36.0));
    }
    else
    {
        skewTShader->setUniformValue("area.top", config->drawingRegionClipSpace.top);
        skewTShader->setUniformValue("area.bottom", config->drawingRegionClipSpace.bottom);
    }
    skewTShader->setUniformValue("position",
                                 diagramConfiguration.geoPosition);
    skewTShader->setUniformValue("bottomPressure",
                                 diagramConfiguration.vertical_p_hPa.max);
    skewTShader->setUniformValue("topPressure",
                                 diagramConfiguration.vertical_p_hPa.min);
    skewTShader->setUniformValue(
                "pressureEqualsWorldPressure",
                config->pressureEqualsWorldPressure);
    skewTShader->setUniformValue(
                "temperatureAmplitude",
                diagramConfiguration.temperature_degC.amplitude());
    skewTShader->setUniformValue(
                "temperatureCenter", diagramConfiguration.temperature_degC.center());
    if (config->pressureEqualsWorldPressure)
    {
        skewTShader->setUniformValue(
                    "pToWorldZParams2", sceneView->pressureToWorldZParameters());
    }
    else
    {
        skewTShader->setUniformValue(
                    "pToWorldZParams2", config->pressureToWorldZParameters());
    }
    skewTShader->setUniformValue(
                "pToWorldZParams", config->pressureToWorldZParameters());

    skewTShader->setUniformValue("mvpMatrix",
                                 *(sceneView->getModelViewProjectionMatrix()));
    skewTShader->setUniformValue("viewMatrix",
                                 sceneView->getCamera()->getViewMatrix());
    skewTShader->setUniformValue("numberOfLevels", -1);
    skewTShader->setUniformValue("numberOfLats"  , -1);
    skewTShader->setUniformValue("ensemble"      , -1);
    skewTShader->setUniformValue("scalarMinimum" ,  0.f);
    skewTShader->setUniformValue("scalarMaximum" ,  0.f);
    config->layer -= 0.001f;
    skewTShader->setUniformValue("layer"         , config->layer);
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
    area.left = 0.05;
    area.right = 0.95;
    area.bottom = worldZfromPressure(vertical_p_hPa.max);
    area.top = worldZfromPressure(vertical_p_hPa.min);
    if (!pressureEqualsWorldPressure)
    {
        area.bottom = area.bottom + 0.05;
        area.top = area.top + 0.05;
    }
    diagramColor = QVector4D(0, 0, 0, 1);
    offscreenTextureSize.width = 512;
    offscreenTextureSize.height = 512;
    layer = 0;
}


float MSkewTActor::DiagramConfiguration::skew(float x, float y) const
{
    return area.left + (x + y) * area.width();
}


float MSkewTActor::DiagramConfiguration::clipTo2D(float v) const
{
    return (v + 1.0) / 2.0;
}


float MSkewTActor::DiagramConfiguration::temperaturePosition() const
{
    if (pressureEqualsWorldPressure)
    {
        return area.top;
    }
    return area.bottom;
}


float MSkewTActor::DiagramConfiguration::scaleTemperatureToDiagramSpace(float T)
const
{
    return (((T - 273.5) + temperature_degC.center()) /
            temperature_degC.amplitude());
}


double MSkewTActor::DiagramConfiguration::pressureFromWorldZ(double z)
{
    float slopePtoZ = (area.top * 36.0 - area.bottom *36.0) /
            (log(20.) - log(1050.));
    return exp(z / slopePtoZ + log(vertical_p_hPa.max));
}


float MSkewTActor::DiagramConfiguration::worldZfromPressure(float p) const
{
    if (!pressureEqualsWorldPressure)
    {
        float slopePtoZ = (36.0f - 0.1f * 36.0f) / (log(
                              20.) - log(1050.));
        return ((log(p) - log(1050.)) * slopePtoZ) / 36.0;
    }
    else
    {
        float slopePtoZ = 36.0f / (log(20.) - log(1050.));
        return ((log(p) - log(1050.)) * slopePtoZ) / 36.0;
    }
}


QVector2D MSkewTActor::DiagramConfiguration::pressureToWorldZParameters() const
{
    if (!pressureEqualsWorldPressure)
    {
        float slopePtoZ = (36.0f - 0.1f * 36.0f) / (log(
                              20.) - log(1050.));
        return QVector2D(log(1050.), slopePtoZ);
    }
    else
    {
        float slopePtoZ = 36.0f / (log(20.) - log(1050.));
        return QVector2D(log(1050.), slopePtoZ);
    }
}


void MSkewTActor::ModeSpecificDiagramConfiguration::init(
        DiagramConfiguration *dconfig, QString bufferNameSuffix)
{
    this->dconfig = dconfig;

    drawingRegionClipSpace.left = 0.05f;
    drawingRegionClipSpace.right = 0.95f;
    drawingRegionClipSpace.bottom = worldZfromPressure(dconfig->vertical_p_hPa.max);
    drawingRegionClipSpace.top = worldZfromPressure(dconfig->vertical_p_hPa.min);
    if (!pressureEqualsWorldPressure)
    {
        drawingRegionClipSpace.bottom = drawingRegionClipSpace.bottom + 0.05f;
        drawingRegionClipSpace.top = drawingRegionClipSpace.top + 0.05f;
    }
    layer = 0;
    this->bufferNameSuffix = bufferNameSuffix;
}


float MSkewTActor::ModeSpecificDiagramConfiguration::skew(float x, float y) const
{
    return drawingRegionClipSpace.left + (x + y) * drawingRegionClipSpace.width();
}


float MSkewTActor::ModeSpecificDiagramConfiguration::temperatureReferenceZCoord() const
{
    if (pressureEqualsWorldPressure)
    {
        return drawingRegionClipSpace.top;
    }
    return drawingRegionClipSpace.bottom;
}


double MSkewTActor::ModeSpecificDiagramConfiguration::pressureFromWorldZ(
        double z, DiagramConfiguration dconfig)
{
    float slopePtoZ = (drawingRegionClipSpace.top * 36.0 - drawingRegionClipSpace.bottom * 36.0) /
            (log(dconfig.vertical_p_hPa.min) - log(dconfig.vertical_p_hPa.max));
    return exp(z / slopePtoZ + log(dconfig.vertical_p_hPa.max));
}


float MSkewTActor::ModeSpecificDiagramConfiguration::worldZfromPressure(
        float p) const
{
    if (!pressureEqualsWorldPressure)
    {
        float slopePtoZ = (36.0f - 0.1f * 36.0f)
                / (log(dconfig->vertical_p_hPa.min) - log(dconfig->vertical_p_hPa.max));
        return ((log(p) - log(dconfig->vertical_p_hPa.max)) * slopePtoZ) / 36.f;
    }
    else
    {
        float slopePtoZ = 36.f / (log(20.f) - log(1050.f));
        return ((log(p) - log(1050.)) * slopePtoZ) / 36.f;
    }
}


float MSkewTActor::ModeSpecificDiagramConfiguration::worldZToPressure(float z) const
{
    if (!pressureEqualsWorldPressure)
    {
        float slopePtoZ = (36.0f - 0.1f * 36.0f)
                / (log(dconfig->vertical_p_hPa.min) - log(dconfig->vertical_p_hPa.max));
        return exp(((z) / slopePtoZ) + log(dconfig->vertical_p_hPa.max));
    }
    else
    {
        float slopePtoZ = 36.0f / (log(20.) - log(1050.));
        return exp(((z) / slopePtoZ) + log(1050.));
    }
}


QVector2D
MSkewTActor::ModeSpecificDiagramConfiguration::pressureToWorldZParameters() const
{    
    if (!pressureEqualsWorldPressure)
    {
        float slopePtoZ = (36.0f - 0.1f * 36.0f)
                / (log(dconfig->vertical_p_hPa.min) - log(dconfig->vertical_p_hPa.max));
        return QVector2D(log(dconfig->vertical_p_hPa.max), slopePtoZ);
    }
    else
    {
        float slopePtoZ = 36.0f / (log(20.) - log(1050.));
        return QVector2D(log(1050.), slopePtoZ);
    }
}


void MSkewTActor::drawDiagram3DView(MSceneViewGLWidget* sceneView)
{
    normalscreenDiagrammConfiguration.layer = -0.005f;
    drawDiagramGeometryAndLabels(sceneView, vbDiagramVertices, &normalscreenDiagrammConfiguration);
    normalscreenDiagrammConfiguration.layer = -.1f;
    drawDiagram(sceneView, vbDiagramVertices, &normalscreenDiagrammConfiguration);
}


void MSkewTActor::drawDiagramFullScreen(MSceneViewGLWidget* sceneView)
{
    glClear(GL_DEPTH_BUFFER_BIT);
    fullscreenDiagrammConfiguration.layer = -0.005f;
    drawDiagramGeometryAndLabelsFullScreen(sceneView);
    drawDiagram(sceneView, vbDiagramVerticesFS, &fullscreenDiagrammConfiguration);
}


void MSkewTActor::drawDiagramGeometryAndLabelsFullScreen(MSceneViewGLWidget* sceneView)
{
    drawDiagramGeometryAndLabels(sceneView, vbDiagramVerticesFS,
                                 &fullscreenDiagrammConfiguration);
}


void MSkewTActor::drawDiagramGeometryAndLabels3DView(MSceneViewGLWidget* sceneView)
{
    drawDiagramGeometryAndLabels(sceneView, vbDiagramVertices,
                                 &normalscreenDiagrammConfiguration);
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

    // Skew temperature axis through shear in x (shear factor 0..1).
    QTransform shear;
    shear.shear(xShear, 0.);
    QMatrix4x4 shearMatrix(shear);

    // Construct transformation matrix that performs all steps above at once.
    transformationMatrixTlogp2xy = shearMatrix * scaleMatrix * translationMatrix;
}


QVector2D MSkewTActor::transformTp2xy(QVector2D tpCoordinate_K_hPa)
{
    QPointF tlogpCoordinate = QPointF(tpCoordinate_K_hPa.x(),
                                      log(tpCoordinate_K_hPa.y()));
    QPointF xyCoordinate = transformationMatrixTlogp2xy * tlogpCoordinate;

    return QVector2D(xyCoordinate);
}


}  // namespace Met3D
