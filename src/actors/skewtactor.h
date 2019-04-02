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
#include <QMatrix4x4>

// local application imports
#include "gxfw/nwpmultivaractor.h"
#include "gxfw/nwpactorvariable.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/textmanager.h"
#include "actors/movablepoleactor.h"

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

    int checkIntersectionWithHandle(
            MSceneViewGLWidget* sceneView, float clipX, float clipY) override;

    void addPositionLabel(MSceneViewGLWidget *sceneView, int handleID,
                          float clipX, float clipY);

    void dragEvent(MSceneViewGLWidget*
                   sceneView, int handleID, float clipX,
                   float clipY) override;

    QList<MLabel*> getPositionLabelToRender() override;

    void removePositionLabel() override;

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

    void printDebugOutputOnUserRequest();

private:
    // Start index and index count for vertices to be drawn from a vertex array.
    struct VertexRange
    {
        int startIndex = 0;
        int indexCount  = 0;
    };

    // Vertex ranges for coordinate lines of the diagram; all vertices are
    // stored in a shared vertex buffer.
    struct SkewTCoordinateLinesVertexRanges
    {
        int lineWidth = 1;
        VertexRange diagramFrame;
        VertexRange isobars;
        VertexRange isotherms;
        VertexRange dryAdiabates;
        VertexRange moistAdiabates;
    };

    struct CoordinateRange
    {
        float min, max;
    };

    struct SkewTDiagramConfiguration
    {
        SkewTDiagramConfiguration() {}

        QVector4D backgroundColor;
        QVector2D lonLatPosition;
        struct CoordinateRange verticalRange_p_hPa;
        struct CoordinateRange temperatureRange_degC;
        float skewFactor = 1.;
        bool alignWithCamera = false;
        double diagramWidth3D = 15.;
        bool drawDryAdiabates = true;
        bool drawMoistAdiabates = true;
        bool regenerateAdiabates = true;
        bool fullscreen = false;
        float isothermSpacing = 10.;
        float moistAdiabatSpacing = 10.0;
        float dryAdiabatSpacing = 10.0;
        bool recomputeAdiabateGeometries = true;
        QVector<QVector2D> dryAdiabatesVertices;
        QVector<QVector2D> moistAdiabatesVertices;
        SkewTCoordinateLinesVertexRanges coordinateGeometryDrawRanges;
        SkewTCoordinateLinesVertexRanges highlightGeometryDrawRanges;
    };

    SkewTDiagramConfiguration diagramConfig;

    QMap<MSceneViewGLWidget*, bool> sceneViewFullscreenEnabled;

    QList<MLabel*> labelsFullScreen;
    QList<MLabel*> labels3D;

    QtProperty *geoPositionProperty,
               *appearanceGroupProperty, *alignWithCameraProperty,
               *bottomPressureProperty, *topPressureProperty,
               *temperatureMinProperty, *temperatureMaxProperty,
               *isothermsSpacingProperty, *skewFactorProperty,
               *drawDryAdiabatesProperty, *dryAdiabatesSpacingProperty,
               *drawMoistAdiabatesProperty, *moistAdiabatesSpcaingProperty,
               *diagramWidth3DProperty,
               *temperatureColorWyomingProperty, *groupWyoming,
               *wyomingStationsProperty, *enableWyomingProperty,
               *dewPointColorWyomingProperty;

    std::shared_ptr<GL::MShaderEffect> skewTShader;

    // Vertex buffers for diagram geometry and for "highlight geometry" in
    // fullscreen interaction mode.
    GL::MVertexBuffer* vbDiagramGeometry;
    GL::MVertexBuffer* vbFullscreenMouseOverGeometry;
    QVector<QVector2D> dryAdiabatesVertices;
    QVector<QVector2D> moistAdiabatesVertices;

    // Vertex buffer for profiles retrieved from the UoWyoming webservice.
    GL::MVertexBuffer* vbWyomingProfiles;
    int wyomingVerticesCount;
    QVector<int> wyomingStations;

    /**
      Copies the current user-defined diagram configuration from the
      GUI properties into the @ref SkewTDiagramConfiguration struct.
     */
    void copyDiagramConfigurationFromQtProperties();

    void drawDiagramGeometryAndLabels(
            MSceneViewGLWidget*sceneView,
            GL::MVertexBuffer *vbDiagramGeometry,
            SkewTCoordinateLinesVertexRanges *vertexRanges);

    void generateDiagramGeometry(
            GL::MVertexBuffer **vbDiagramGeometry,
            SkewTDiagramConfiguration *config);

    void generateFullScreenHighlightGeometry(
            QVector2D tpCoordinate,
            GL::MVertexBuffer** vbDiagramGeometry,
            SkewTDiagramConfiguration *config);

    void drawDiagram(MSceneViewGLWidget* sceneView);

    void loadObservationalDataFromUWyoming(int stationNum);

    void loadListOfAvailableObservationsFromUWyoming();

    void drawDiagramGeometryAndLabelsFullScreen(MSceneViewGLWidget* sceneView);

    // Stores the transformation matrix that transforms (T, log(p)) coordinates
    // into the diagram's (x, y) coordinates. Computed by
    // computeTlogp2xyTransformationMatrix().
    QMatrix4x4 transformationMatrixTlogp2xy;
    QMatrix4x4 transformationMatrixXY2Tlogp; // inverse matrix

    /**
      Compute the @ref transformationMatrixTlogp2xy from user settings
      (temperature and pressure range to be displayed by the diagram).

      @see transformTp2xy()
     */
    void computeTlogp2xyTransformationMatrix();

    /**
      Transform a (temperature, pressure) coordinate into the Skew-T diagram's
      (x, y) coordinates. Temperature is in K, pressure in hPa, the (x, y)
      coordinates are in the (0..1) range both. The transformation matrix
      used in this method needs to be computed first using
      @ref computeTlogp2xyTransformationMatrix().
     */
    QVector2D transformTp2xy(QVector2D tpCoordinate_K_hPa);

    /**
      Inverse of @ref transformTp2xy().
     */
    QVector2D transformXY2Tp(QVector2D xyCoordinate);

    /**
      Transform a Skew-T diagram's (x, y) coordinate into full-screen clip
      space coordinates.

      @note There is an equivalent function with the same name in the GLSL
      shader; if this function is modified the shader function needs to be
      modified as well.
     */
    QVector2D transformXY2ClipSpace(QVector2D xyCoordinate);

    /**
      Inverse of transformXY2ClipSpace
     */
    QVector2D transformClipSpace2xy(QVector2D clipCoordinate);

    /**
      Computes a view-dependent transformation matrix to transform coordinates
      in the diagram's (x, y) space into a scene view's world space.
      Coordinates are mapped to an upright plane anchored at the vertical
      profile's location.
     */
    QMatrix4x4 computeXY2WorldSpaceTransformationMatrix(
            MSceneViewGLWidget* sceneView);

    /**
      Updates (recomputes) the vertical profiles of the actor variables, e.g.
      after the position of the diagram handle has been changed.
     */
    void updateVerticalProfilesAndLabels();

    void removeAllSkewTLabels();

    MMovablePoleActor *poleActor;
    bool dragEventActive;
};


class MSkewTActorFactory : public MAbstractActorFactory
{
public:
    MSkewTActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override { return new MSkewTActor(); }
};


}  // namespace Met3D

#endif  // SKEWTACTOR_H
