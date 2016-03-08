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
#ifndef MSCENEVIEWGLWIDGET_H
#define MSCENEVIEWGLWIDGET_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include <QtCore>
#include <QtGui>
#include <QGLWidget>
#include <QGLShaderProgram>
#include <QtProperty>

// local application imports
#include "util/mutil.h"
#include "util/mstopwatch.h"
#include "gxfw/camera.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/mscenecontrol.h"
#include "gxfw/msystemcontrol.h"
#include "gxfw/mtypes.h"

namespace Met3D
{

typedef QVector<QVector3D> QVectorOfQVector3D;

/**
  Stores the reference to a currently picked actor and the ID of the picked
  handle.
  */
struct PickActor {
    MActor *actor;
    int    handleID;
};


/**
  @brief MSceneViewGLWidget implements a view on a given scene (which is
  represented by an @ref MSceneControl instance).
  */
class MSceneViewGLWidget : public QGLWidget
{
    Q_OBJECT

public:
    enum CameraActions {
        NorthUp = 0,
        TopView = 1,
        RememberCurrentView = 2,
        RestoreRememberedView = 3,
        SaveViewToFile = 4,
        RestoreFromFile = 5
    };

    enum SceneNavigationMode {
        MOVE_CAMERA  = 0,
        ROTATE_SCENE = 1
    };

    enum LightDirection {
        WORLD_NORTHWEST = 0,
        SCENE_NORTHWEST = 1,
        VIEWDIRECTION   = 2,
        TOP             = 3
    };

    MSceneViewGLWidget();

    ~MSceneViewGLWidget();

    /**
      Sets the @ref MSceneControl instance @p scene that shall be rendered.
      */
    void setScene(MSceneControl *scene);

    void removeCurrentScene();

    QSize minimumSizeHint() const;

    QSize sizeHint() const;

    void setClearColor(const QColor &color);

    MSceneControl* getScene() { return scene; } // implicit inline

    MCamera* getCamera() { return &camera; }

    /**
      Returns the current model-view-projection matrix.
      */
    QMatrix4x4* getModelViewProjectionMatrix()
    { return &modelViewProjectionMatrix; }

    /**
      Compute a world z-coordinate from a pressure value @p p_hPa (in hPa).
      */
    double worldZfromPressure(double p_hPa);

    static double worldZfromPressure(double p_hPa, double log_pBottom_hPa,
                                     double deltaZ_deltaLogP);

    double pressureFromWorldZ(double z);

    /**
      Returns a 2D vector containing [ ln (p_bot), dz / dln(p) ]. These two
      values are needed to convert pressure (in hPa) to this scene view's
      world z coordinate.
      */
    QVector2D pressureToWorldZParameters();

    /**
      Converts a vector whose components are given as lat/lon/pressure to clip
      space (i.e. converts pressure to world z and applies the
      model-view-projection matrix.).
      */
    QVector3D lonLatPToClipSpace(const QVector3D& lonlatp);

    QVector3D clipSpaceToLonLatWorldZ(const QVector3D& clipPos);

    QVector3D clipSpaceToLonLatP(const QVector3D& clipPos);

    /**
      Returns true if visualisation parameters have changed that might make the
      recomputation of actor properties necessary. Example: Vertical sections
      need to recompute the "targetGrid" if the view's vertical scaling has
      changed.
      */
    bool visualisationParametersHaveChanged()
    { return visualizationParameterChange; }

    /**
      Returns the scene's light direction. (Use this in actors that use
      shading.)
      */
    QVector3D getLightDirection();

    /**
      Returns a unique number that identifies the scene view.
      */
    unsigned int getID() { return myID; }

    /**
      Returns a @ref QtProperty instance acting as parent to all the properties
      of this view (i.e. representing the properties "group"). This instance
      can be added to a @ref QtAbstractPropertyBrowser.

      @see http://doc.qt.nokia.com/solutions/4/qtpropertybrowser/index.html
      */
    QtProperty* getPropertyGroup() { return propertyGroup; }

    int getViewPortWidth() { return viewPortWidth; }

    int getViewPortHeight() { return viewPortHeight; }

    void setVerticalScaling(float scaling);

    void setInteractionMode(bool enabled);

    void setAnalysisMode(bool enabled);

    void setFreeze(bool enabled);

    bool interactionModeEnabled() { return actorInteractionMode; }

    bool analysisModeEnabled() { return analysisMode; }

    bool userIsInteractingWithScene() { return userIsInteracting; }

    bool userIsScrollingWithMouse();

    bool isViewPortResized();

    void addCameraSync(MSceneViewGLWidget *otherSceneView)
    { syncCameras.insert(otherSceneView); }

    void removeCameraSync(MSceneViewGLWidget *otherSceneView)
    { syncCameras.remove(otherSceneView); }

    /**
      Updates the label that displays the camera position in the scene view
      control's property browser.
      */
    void updateCameraPositionDisplay();

    /**
      If the scene view is in "actor interaction mode": make @p actor the only
      actor in the scene with which the user can interact. (Used, e.g., to
      display a pole to select a scene rotation centre).
     */
    void setSingleInteractionActor(MActor *actor);

    void setSceneNavigationMode(SceneNavigationMode mode);

    void setSceneRotationCentre(QVector3D centre);

signals:
    /**
      Emitted when a mouse button is released on the GL canvas.
      */
    void clicked();

public slots:
    /**
      Modifies the camera position and view direction according to the camera
      action @p action and updates the widget.
      */
    void executeCameraAction(int action, bool ignoreWithoutFocus=true);

    /**
      Updates the widget when property @p property changes.
      */
    void onPropertyChanged(QtProperty *property);

//    /**
//      Sets the valid and init time displayed in the upper left corner of the
//      view.
//      */
//    void updateDisplayTime();

    /**
      */
    void updateFPSTimer();

    void stopFPSMeasurement();

    void updateSceneLabel();

protected:
    void initializeGL();

    void paintGL();

    void resizeGL(int width, int height);

    void mousePressEvent(QMouseEvent *event);

    /**
      Overloads QGLWidget::mouseMoveEvent().

      In interaction mode (actor elements can be changed interactively, e.g.
      the waypoints of a vertical section) this method handles pick&drag
      events. Otherwise it handles camera moves. In interaction mode, Qt mouse
      tracking is enabled (in @ref propertyChange()); mouseMoveEvent() is hence
      called on any mouse move on the canvas. This allows the implementation of
      "hovering" effects when the mouse cursor moves over a pickable
      element. For camera moves, mouse tracking is disabled and mouseMoveEvent()
      is only called when a mouse button has been pressed.
      */
    void mouseMoveEvent(QMouseEvent *event);

    void mouseReleaseEvent(QMouseEvent *event);

    void wheelEvent(QWheelEvent *event);

    void keyPressEvent(QKeyEvent *event);

    void updateSynchronizedCameras();

    QList<MLabel*> staticLabels;

protected slots:

    /**
      Checks every 100 milliseconds if the user is scrolling with the mouse.
     */
    void checkUserScrolling();

private:
    MSceneControl *scene;

    // Background colour.
    QColor clearColor;

    // Stores the position of the mourse cursor, used in the mouse??Event()
    // methods.
    QPoint lastPos;
    QVector3D lastPoint;

    MCamera camera;
    QMatrix4x4 modelViewProjectionMatrix;
    QMatrix4x4 worldRotationMatrix;
    QMatrix4x4 worldPositionMatrix;
    SceneNavigationMode sceneNavigationMode;
    QVector3D sceneRotationCentre;

    // Status variables.
    bool renderLabelsWithDepthTest;
    bool actorInteractionMode;
    bool analysisMode;
    int freezeMode;
    bool viewIsInitialised;
    bool userIsInteracting; // Is the user currently moving the camera?
    bool userIsScrolling;   // Is user currently scrolling with the mouse?
    bool viewportResized;   // Was the viewport resized?

    QElapsedTimer scrollTimer;
    QElapsedTimer resizeTimer;
    QTimer        checkScrollTimer;

    // Framerate measurements:
    QTimer     *fpsTimer;      // Timer to take a measurement every xx seconds.
    MStopwatch *fpsStopwatch;  // Stopwatch to get the elapsed time.
    short  frameCount;         // Count how many frames have been rendered.
    bool   splitNextFrame;     // Status var to indicate when to measure.
    float *fpsTimeseries;      // Field to store an fps time series.
    int    fpsTimeseriesSize;  // Length of the time series.
    int    fpsTimeseriesIndex; // Current index within the time series.

    // Size of the OpenGL viewport belonging to this widget.
    int viewPortWidth;
    int viewPortHeight;

    // Vertical axis limits: Bottom and top pressure (in hPa) and corresponding
    // z coordinates.
    double pbot;
    double ptop;
    double logpbot;
    double zbot;
    double ztop;
    double slopePtoZ; // dZ / dlnP

    // Light direction for actors that use shading.
    LightDirection lightDirection;
    QMatrix4x4 sceneNorthWestRotationMatrix;

    QGLShaderProgram *focusShader;

    // In modification mode, stores the currently picked actor.
    PickActor pickedActor;

    // Identification number.
    static unsigned int idCounter;
    unsigned int myID;

    QtProperty *propertyGroup;

    QtProperty *cameraPositionProperty;
    QtProperty *interactionGroupProperty;
    QtProperty *sceneNavigationModeProperty;
    QtProperty *sceneRotationCenterProperty;
    QtProperty *sceneRotationCentreLatProperty;
    QtProperty *sceneRotationCentreLonProperty;
    QtProperty *sceneRotationCentreElevationProperty;
    QtProperty *selectSceneRotationCentreProperty;
    QtProperty *syncCameraWithViewProperty;
    QtProperty *interactionModeProperty;
    QtProperty *analysisModeProperty;

    QtProperty *renderingGroupProperty;
    QtProperty *multisamplingProperty;
    QtProperty *antialiasingProperty;
    QtProperty *labelDepthTestProperty;
    QtProperty *lightingProperty;
    QtProperty *verticalScalingProperty;

    bool measureFPS;
    uint measureFPSFrameCount;
    QtProperty *measureFPSProperty;

    /** List of static labels to display in this view. */
    MLabel* sceneNameLabel;

    bool visualizationParameterChange;

    QSet<MSceneViewGLWidget*> syncCameras;
    MSceneViewGLWidget* cameraSyncronizedWith;
    MCamera rememberCamera;

    MActor *singleInteractionActor;

    /**
     * When this variable is set to false the qt property changed events will be
     * ignored for the instance of the scene.
     */
    bool enablePropertyEvents;
};

} // namespace Met3D

#endif
