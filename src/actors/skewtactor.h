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
#ifndef SKEWTACTOR_H
#define SKEWTACTOR_H

// standard library imports
#include <memory>  // for use of shared_ptr

// related third party imports
#include <QObject>
#include <GL/glew.h>
#include <QGLShaderProgram>
#include <QtProperty>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// local application imports
#include "gxfw/nwpmultivaractor.h"
#include "gxfw/nwpactorvariable.h"
#include "gxfw/gl/shadereffect.h"
#include "actors/transferfunction1d.h"
#include "actors/graticuleactor.h"
#include "gxfw/textmanager.h"

class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief MSkewTActor renders a Skew-T diagram displaying the vertical profile
  over a choosen geographical position. The diagram can be drawn into the 3D
  view and additionally be displayed in full screen more in a separate view.
 */
class MSkewTActor : public MNWPMultiVarActor
{
    Q_OBJECT

public:
    MSkewTActor();
    ~MSkewTActor() override;

    static QString staticActorType() { return "Skew-T diagram"; }

    void reloadShaderEffects() override;

    void saveConfiguration(QSettings* settings) override;

    void loadConfiguration(QSettings* settings) override;

    QString getSettingsID() override { return "SkewTActor"; }

    const QList<MVerticalLevelType> supportedLevelTypes() override;

    MNWPActorVariable* createActorVariable(
            const MSelectableDataSource& dataSource) override;

    virtual int checkIntersectionWithHandle(
            MSceneViewGLWidget* sceneView, float clipX, float clipY) override;

    virtual void dragEvent(MSceneViewGLWidget*
                           sceneView, int handleID, float clipX,
                           float clipY) override;

    virtual bool supportsMultipleEnsembleMemberVisualization() override
    { return true; }

    virtual void onFullScreenModeSwitch(MSceneViewGLWidget *sceneView,
                                        bool fullScreenEnabled) override;

protected slots:
    // These two slots are used for downloading the data from the
    // U of Wyoming web service.
    void downloadOfObservationFromUWyomingFinished(QNetworkReply* reply);
    void downloadOfObservationListFromUWyomingFinished(QNetworkReply* reply);

protected:
    void initializeActorResources() override;

    void renderToCurrentContext(MSceneViewGLWidget* sceneView) override;

    void renderToCurrentFullScreenContext(MSceneViewGLWidget* sceneView) override;

    void dataFieldChangedEvent() override;

    void onQtPropertyChanged(QtProperty* property) override;

    void onDeleteActorVariable(MNWPActorVariable* var) override;

    void onAddActorVariable(MNWPActorVariable* var) override;

    void onChangeActorVariable(MNWPActorVariable *var) override;

private:
    /**
      @brief Stores the current variable index and color of a variable.
     */
    struct VariableConfig
    {
        QtProperty *property = nullptr;
        MNWPSkewTActorVariable *variable = nullptr;
        int index = 0;
        QColor color;
        double thickness;
    };

    /**
      @brief Stores the coordinates of the diagram outline.
     */
    struct Outline
    {
        float data[16];
    };

    /**
      @brief Stores a vertex range that has to be drawn by the draw
      calls for the diagram auxiliary lines.
     */
    struct VertexRange
    {
        int start = 0;
        int size  = 0;
    };

    /**
      @brief Stores the vertex ranges for every different type of auxiliary
      line.
     */
    struct VertexRanges
    {
        VertexRange temperatureMarks;
        VertexRange isotherms;
        VertexRange dryAdiabates;
        VertexRange moistAdiabates;
        VertexRange outline;
        VertexRange pressure;
    };

    struct VARIABLES
    {
        struct TEMPERATURE
        {
            int MEMBER = 0;
            int MEAN = 1;
            int MINIMUM = 2;
            int MAXIMUM = 3;
            int DEVIATION = 4;
            int SPAGHETTI = 5;
        };
        TEMPERATURE temperature;

        struct DEWPOINT
        {
            int MEMBER = 6;
            int MEAN = 7;
            int MINIMUM = 8;
            int MAXIMUM = 9;
            int DEVIATION = 10;
            int SPAGHETTI = 11;
        };
        DEWPOINT dewPoint;
    };
    VARIABLES variablesIndices;

    /**
      @brief Describes the drawing area.
     */
    struct Area
    {
        float top, bottom;
        float left, right;
        float width() const
        {
            return right - left;
        }
        float height() const
        {
            return top - bottom;
        }
        struct Outline outline() const
        {
            Outline outline;
            float data[16] = { left , top   , left , 0.0,
                               left , bottom, right, bottom,
                               left , top   , right, top,
                               right, bottom, right, top
                             };
            std::copy(std::begin(data), std::end(data),
                      std::begin(outline.data));
            return outline;
        }
    };

    struct Amplitude
    {
        float min, max;
        float amplitude() const
        {
            return std::abs(max) + std::abs(min);
        }
        float center() const
        {
            return amplitude() / 2.f;
        }
    };

    struct Size
    {
        float width, height;
    };

    /**
      @brief Stores the actual diagram configuration. This avoids using the
      QtProperty getters to retrieve configuration details.
     */
    struct DiagramConfiguration
    {
        DiagramConfiguration() {}
        void init();

        QVector<VariableConfig> varConfigs;
        QVector2D position;
        QVector2D clipPos;
        struct Area area;
        float layer;
        struct Amplitude pressure;
        struct Amplitude temperature;
        struct Size offscreenTextureSize;
        GLint maxTextureSize;
        bool pressureEqualsWorldPressure = false;
        bool overDragHandle = false;
        bool drawDryAdiabates = true;
        bool drawMoistAdiabates = true;
        bool regenerateAdiabates = true;
        bool fullscreen = false;
        bool drawInPerspective = false;
        VertexRanges vertexRanges;
        QVector4D diagramColor;
        float moistAdiabatInterval = 10.0;
        float dryAdiabatInterval = 10.0;
        float yOffset = 0;

        float skew(float x, float y) const;

        float clipTo2D(float v) const;

        float temperaturePosition() const;

        /**
          Scales the temperature value @p T (in Kelvin) to the diagram space.
         */
        float scaleTemperatureToDiagramSpace(float T) const;
        /**
          Returns @p z in pressure (hPa) coordinates depending on the actual
          configuration of the diagram @p dconfig.
         */
        double pressureFromWorldZ(double z);
        /**
          Returns @p p (hPa) in world coordinates depending on the actual
          configuration of the diagram.
         */
        float worldZfromPressure(float p) const;
        /**
          @brief Returns the pressureToWorldZParameters depending on the actual
          configuration of the diagram.
          @return QVector2D(log(maxPressure), slopeToZ)
         */
        QVector2D pressureToWorldZParameters() const;
    };

    struct ModeSpecificDiagramConfiguration
    {
        ModeSpecificDiagramConfiguration() {}
        void init(DiagramConfiguration *dconfig, QString bufferNameSuffix);
        float skew(float x, float y) const;
        float temperaturePosition() const;
        /**
          Returns @p z in pressure (hPa) coordinates depending on the actual
          configuration of the diagram @p dconfig.
         */
        double pressureFromWorldZ(double z, DiagramConfiguration dconfig);
        /**
          Returns @p p (hPa) in world coordinates depending on the actual
          configuration of the mode specific diagram.
         */
        float worldZfromPressure(float p) const;
        /**
          Returns @p z in pressure coordinates depending on the mode specific
          configuration of the diagram.
         */
        float worldZToPressure(float z) const;
        /**
          @brief Returns the pressureToWorldZParameters depending on the actual
          configuration of the diagram.
          @return QVector2D(log(maxPressure), slopeToZ)
         */
        QVector2D pressureToWorldZParameters() const;

        bool pressureEqualsWorldPressure = false;

        struct Area area;
        QVector2D clipPos;
        float layer;

        DiagramConfiguration *dconfig;

        bool regenerateAdiabates = true;
        VertexRanges vertexRanges;
        QVector<QVector2D> dryAdiabatesVertices;
        QVector<QVector2D> moistAdiabatesVertices;
        QString bufferNameSuffix;
    };

    ModeSpecificDiagramConfiguration normalscreenDiagrammConfiguration;
    ModeSpecificDiagramConfiguration fullscreenDiagrammConfiguration;

    QMap<MSceneViewGLWidget*, bool> sceneViewFullscreenEnabled;

    QMap<MSceneViewGLWidget*, QList<MLabel*>> skewTLabels;

    float labelSize;
    QColor labelColour;
    bool labelBBox;
    QColor labelBBoxColour;

    QtProperty *geoPositionProperty,
               *bottomPressureProperty, *topPressureProperty,
               *alignWithWorldPressureProperty,
               *wyomingStationsProperty,
               *enableWyomingProperty,
               *dewPointColorWyomingProperty,
               *temperatureColorWyomingProperty, *groupWyoming,
               *temperatureMinProperty, *temperatureMaxProperty,
               *drawDryAdiabatesProperty,
               *drawMoistAdiabatesProperty,
               *moistAdiabatesDivProperty,
               *dryAdiabatesDivisionsProperty,
               *appearanceGroupProperty,
               *perspectiveRenderingProperty,
               *groupVariables, *temperatureGroupProperty,
               *temperatureMinMaxVariableColorProperty,
               *temperatureShowProbabilityTubeProperty,
               *temperatureShowDeviationTubeProperty,
               *humidityGroupProperty,
               *dewPointMinMaxVariableColorProperty,
               *dewPointShowProbabilityTubeProperty,
               *dewPointShowDeviationTubeProperty;

    /**
      @brief Stores the names of the variables.
     */
    QList<QString> varNameList;

    QHash<QString, int> varNamesIndices;

    std::shared_ptr<GL::MShaderEffect> skewTShader;
    std::shared_ptr<GL::MShaderEffect> positionSpheresShader;

    /**
      @brief Buffer used for the diagram auxiliary lines.
     */
    GL::MVertexBuffer* vbDiagramVertices;
    GL::MVertexBuffer* vbDiagramVerticesFS;

    /**
      @brief Buffer used for the actual wyoming station.
     */
    GL::MVertexBuffer* vbWyomingVertices;

    QVector<QVector2D> dryAdiabatesVertices;
    QVector<QVector2D> moistAdiabatesVertices;
    int wyomingVerticesCount;
    QVector<int> wyomingStations;

    DiagramConfiguration diagramConfiguration;

    void updateVariableEnums(MNWPActorVariable *deletedVar = nullptr);

    void drawDragPoint(MSceneViewGLWidget *sceneView);
    void setShaderGeneralVars(MSceneViewGLWidget *sceneView,
                              ModeSpecificDiagramConfiguration *config);
    void drawProbabilityTube(MNWPSkewTActorVariable *max,
                             MNWPSkewTActorVariable *min,
                             bool isHumidity, QColor color);
    void drawDeviation(MNWPSkewTActorVariable *mean,
                       MNWPSkewTActorVariable *deviation,
                       bool isHumidity, QColor deviationColor);
    void setDiagramConfiguration();

    void drawDiagramLabels(MSceneViewGLWidget*sceneView,
                           GL::MVertexBuffer *vbDiagramVertices,
                           ModeSpecificDiagramConfiguration *config);

    void generateDiagramVertices(GL::MVertexBuffer **vbDiagramVertices,
                                 ModeSpecificDiagramConfiguration *config);

    void drawDiagram(MSceneViewGLWidget* sceneView,
                     GL::MVertexBuffer *vbDiagramVertices,
                     ModeSpecificDiagramConfiguration *config);

    void loadObservationalDataFromUWyoming(int stationNum);
    void loadListOfAvailableObservationsFromUWyoming();

    void drawDiagram3DView(MSceneViewGLWidget* sceneView);

    void drawDiagramFullScreen(MSceneViewGLWidget* sceneView);

    void drawDiagramLabels3DView(MSceneViewGLWidget* sceneView);

    void drawDiagramLabelsFullScreen(MSceneViewGLWidget* sceneView);

    // If the user picks the handle not in its centre, we cannot move the handle
    // by setting the centre point to the mouse position so we need this offset
    // to place the handle relative to the mouse position.
    QVector2D offsetPickPositionToHandleCentre;
};


class MSkewTActorFactory : public MAbstractActorFactory
{
public:
    MSkewTActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override
    { displayWarningExperimentalStatus(); return new MSkewTActor(); }
};


}  // namespace Met3D

#endif  // SKEWTACTOR_H
