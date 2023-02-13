/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2016-2017 Bianca Tost
**  Copyright 2021-2022 Christoph Neuhauser
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
#ifdef USE_QOPENGLWIDGET
#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#else
#include <QGLWidget>
#include <QGLShaderProgram>
#endif
#include <QtProperty>

// local application imports
#include "util/mutil.h"
#include "util/mstopwatch.h"
#include "gxfw/camera.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/mscenecontrol.h"
#include "gxfw/msystemcontrol.h"
#include "gxfw/mtypes.h"
#include "gxfw/mresizewindowdialog.h"
#include "gxfw/tooltippicker.h"

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
class MSceneViewGLWidget
#ifdef USE_QOPENGLWIDGET
        : public QOpenGLWidget, public MSynchronizedObject
#else
        : public QGLWidget, public MSynchronizedObject
#endif
{
    Q_OBJECT

public:
    enum CameraActions {
        CAMERA_NORTHUP = 0,
        CAMERA_UPRIGHT = 1,
        CAMERA_TOPVIEW = 2,
        CAMERA_SAVETOFILE = 3,
        CAMERA_LOADFROMFILE = 4
    };

    enum SceneNavigationMode {
        MOVE_CAMERA             = 0,
        ROTATE_SCENE            = 1,
        TOPVIEW_2D              = 2,
        SINGLE_FULLSCREEN_ACTOR = 3
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

    void setBackgroundColour(const QColor &color);

    MSceneControl* getScene() { return scene; } // implicit inline

    MCamera* getCamera() { return &camera; }

    /**
      Returns the current model-view-projection matrix.
      */
    QMatrix4x4* getModelViewProjectionMatrix()
    { return &modelViewProjectionMatrix; }

    /**
      Returns the current inverted model-view-projection matrix.
      */
    QMatrix4x4* getModelViewProjectionMatrixInverted()
    { return &modelViewProjectionMatrixInverted; }

    /**
      Returns the vertical field of view angle.
      */
    float getVerticalAngle() const
    { return verticalAngle; }

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

    inline void setLightDirection(LightDirection lightDirection)
    {
        this->lightDirection = lightDirection;
        MSystemManagerAndControl *systemControl = MSystemManagerAndControl::getInstance();
        systemControl->getEnumPropertyManager()->setValue(lightingProperty, lightDirection);
    }


    inline LightDirection getLightDirectionEnum() const
    {
        return this->lightDirection;
    }

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

    void setAutoRotationMode(bool enabled);

    void setFreeze(bool enabled);

    bool interactionModeEnabled() { return actorInteractionMode; }

    bool analysisModeEnabled() { return analysisMode; }

    bool userIsInteractingWithScene() { return userIsInteracting; }

    bool userIsScrollingWithMouse();

    bool orthographicModeEnabled() { return sceneNavigationMode == TOPVIEW_2D; }

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

    /**
     Save the scene view configuration to the file @p filename.
     */
    void saveConfigurationToFile(QString filename = "");
    /**
     Load the scene view configuration from the file @p filename.
     */
    void loadConfigurationFromFile(QString filename = "");

    /**
     Save scene view-specific configuration to the @ref QSettings object @p
     settings.
     */
    void saveConfiguration(QSettings *settings);
    /**
     Load scene view-specific configuration from the @ref QSettings object @p
     settings.
     */
    void loadConfiguration(QSettings *settings);

    void forceOverwriteImageSequence(bool overwrite)
    { overwriteImageSequence = overwrite; }

    void onHandleSizeChanged();

    virtual void synchronizeWith(
            MSyncControl *sync, bool updateGUIProperties=true);

    virtual bool synchronizationEvent(MSynchronizationType, QVector<QVariant>);

    void addToolTipPicker(MToolTipPicker* picker);
    void removeToolTipPicker(MToolTipPicker* picker);

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

    /**
      Sets the valid and init time displayed in the upper left corner of the
      view.
      */
    void updateDisplayTime();

    /**
      */
    void updateFPSTimer();

    void stopFPSMeasurement();

    void updateToolTipTimer();

    void updateSceneLabel();

    void saveTimeAnimationImage(QString path, QString filename);

    void updateSyncControlProperty();

    /**
     Connects to the MGLResourcesManager::actorCreated() signal. If the new
     actor supports full-screen mode, it is added to the list of full-screen
     actors displayed by the fullScreenActorProperty.
     */
    void onActorCreated(MActor *actor);

    /**
     Connects to the MGLResourcesManager::actorDeleted() signal. If the deleted
     actor supports full-screen mode, update the list of full-screen actors
     displayed by the fullScreenActorProperty, possibly disconnect from the
     full-screen actor.
     */
    void onActorDeleted(MActor *actor);

    /**
     Connects to the MGLResourcesManager::actorRenamed() signal. If the renamed
     actor supports full-screen mode, it is renamed in the list of full-screen
     actors displayed by the fullScreenActorProperty.
     */
    void onActorRenamed(MActor *actor, QString oldName);

    /**
      Directly connect change signal of full-screen actor to @ref update/updateGL() to
      allow the user to select actors as full-screen actors which are not
      connected to the scene view's scene.
     */
    void onFullScreenActorUpdate();

protected:
    void initializeGL();

#ifdef USE_QOPENGLWIDGET
    void update();
#else
    void updateGL();
#endif

    void paintGL();

    void resizeGL(int width, int height);

    void mouseDoubleClickEvent(QMouseEvent *event);

    void mousePressEvent(QMouseEvent *event) override;

    /**
      Overloads QOpenGLWidget/QGLWidget::mouseMoveEvent().

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

    void keyReleaseEvent(QKeyEvent *event);

    void leaveEvent(QEvent *event) override;

    //bool event(QEvent *event) override;

    void updateSynchronizedCameras();

    QList<MLabel*> staticLabels;

protected slots:

    /**
      Checks every 100 milliseconds if the user is scrolling with the mouse.
     */
    void checkUserScrolling();

    /**
     Called by the auto-rotation timer to rotate the camera in auto-rotation
     mode.
     */
    void autoRotateCamera();

    /**
     Called by the camera path timer to rotate the camera in camera path mode (CTRL+Y).
     mode.
     */
    void updateCameraPath();

private:
    /**
      Handles opening of file dialog and letting the user choose where to save
      the screenshot, how to call it and as which image file type.
     */
    void saveScreenshot();

    /**
      Handles taking and saving a screenshot of the scene to @p filename.
     */
    void saveScreenshotToFileName(QString filename);

    void resizeView();

    MSceneControl *scene;

    // Background colour.
    QColor backgroundColour;

    // Stores the position of the mourse cursor, used in the mouse??Event()
    // methods.
    QPoint lastPos;
    QVector3D lastPoint;

    MCamera camera;
    float verticalAngle = 45.0f;
    QMatrix4x4 modelViewProjectionMatrix;
    QMatrix4x4 modelViewProjectionMatrixInverted;
    QMatrix4x4 sceneRotationMatrix;
    SceneNavigationMode sceneNavigationMode;
    QVector3D sceneRotationCentre;
    QTimer *cameraAutoRotationTimer;
    QVector3D cameraAutoRotationAxis;
    float cameraAutoRotationAngle;

    float cameraPathTimeMs = 0.0f;
    float cameraPathDuration = 10.0f * 1e3f;
    QTimer *cameraPathTimer = nullptr;
    int cameraPathInterval = 16;
    QVector3D cameraPathStartOrigin;
    QVector3D cameraPathStartZAxis;
    QVector3D cameraPathStartYAxis;

    // Status variables.
    bool renderLabelsWithDepthTest;
    bool actorInteractionMode;
    bool cameraAutorotationMode;
    bool analysisMode;
    int freezeMode;
    bool viewIsInitialised;
    bool userIsInteracting; // Is the user currently moving the camera?
    bool userIsScrolling;   // Is user currently scrolling with the mouse?
    bool viewportResized;   // Was the viewport resized?
    bool isDraggingVirtualWindow = false;

    // For displaying tool tips for objects in the widget.
    QVector<MToolTipPicker*> toolTipPickers;
    QLabel *toolTipLabel;
    QTimer *toolTipUpdateTimer;
    QPoint toolTipMousePosition;
    QPoint toolTipMousePositionGlobal;
    bool toolTipMouseInParent = false;
    int toolTipWaitTimeMs = 400;

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

#ifdef USE_QOPENGLWIDGET
    QOpenGLShaderProgram *focusShader;
#else
    QGLShaderProgram *focusShader;
#endif
    std::shared_ptr<GL::MShaderEffect> northArrowShader;

    // In modification mode, stores the currently picked actor.
    PickActor pickedActor;

    // Identification number.
    static unsigned int idCounter;
    unsigned int myID;

    QtProperty *propertyGroup;

    QtProperty *configurationSupGroup;
    QtProperty *saveConfigProperty;
    QtProperty *loadConfigProperty;

    QtProperty *cameraPositionProperty;
    QtProperty *cameraGroupProperty;
    QtProperty *cameraSetNorthUpProperty;
    QtProperty *cameraSetUprightProperty;
    QtProperty *cameraSetTopViewProperty;
    QtProperty *cameraSaveToFileProperty;
    QtProperty *cameraLoadFromFileProperty;

    QtProperty *interactionGroupProperty;
    QtProperty *resizeProperty;
    QtProperty *sceneSaveToImageProperty;
    QtProperty *sceneNavigationModeProperty;
    QtProperty *sceneRotationCenterProperty;
    QtProperty *sceneRotationCentreLatProperty;
    QtProperty *sceneRotationCentreLonProperty;
    QtProperty *sceneRotationCentreElevationProperty;
    QtProperty *selectSceneRotationCentreProperty;
    MActor     *fullScreenActor;
    QtProperty *fullScreenActorProperty;
    float       sceneNavigationSensitivity;
    QtProperty *sceneNavigationSensitivityProperty;
    QtProperty *syncCameraWithViewProperty;
    QtProperty *actorInteractionProperty;
    QtProperty *analysisModeProperty;
    QtProperty *cameraAutoRotationModeProperty;    
    QtProperty *posLabelEnableProperty;
    bool        posLabelIsEnabled; /* Whether position label will be rendered. */

    QtProperty *renderingGroupProperty;
    QtProperty *backgroundColourProperty;
    QtProperty *farPlaneDistanceProperty;
    float       farPlaneDistance;
    QtProperty *fieldOfViewProperty;
    QtProperty *multisamplingProperty;
    bool        multisamplingEnabled;
    QtProperty *antialiasingProperty;
    bool        antialiasingEnabled;
    QtProperty *labelDepthTestProperty;
    QtProperty *lightingProperty;
    QtProperty *verticalScalingProperty;

    QtProperty *annotationsGroupProperty;

    QtProperty *displayDateTimeGroupProperty;
    QtProperty *displayDateTimeSyncControlProperty;
    QtProperty *displayDateTimeFontSizeProperty;
    QtProperty *displayDateTimeColourProperty;
    QtProperty *displayDateTimeBBoxProperty;
    QtProperty *displayDateTimeBBoxColourProperty;
    QtProperty *displayDateTimePositionProperty;

    /** Synchronisation with MSyncControl. */
    MSyncControl *synchronizationControl;

    MLabel       *displayDateTimeLabel;
    float         displayDateTimeFontSize;
    QColor        displayDateTimeColour;

    struct NorthArrow
    {
      NorthArrow() {}

      QtProperty *groupProperty;
      QtProperty *enabledProperty;
      QtProperty *colourProperty;
      QtProperty *horizontalScaleProperty;
      QtProperty *verticalScaleProperty;
      QtProperty *lonPositionProperty;
      QtProperty *latPositionProperty;
      QtProperty *worldZPositionProperty;

      bool enabled;
      QColor colour;
      double horizontalScale;
      double verticalScale;
      double lon;
      double lat;
      double worldZ;
    };
    NorthArrow northArrow;

    QtProperty *sceneViewLabelGroupProperty;
    QtProperty *sceneViewLabelEnableProperty;

    /**
      List of static labels to display in this view.
     */
    MLabel *sceneNameLabel;

    bool measureFPS;
    uint measureFPSFrameCount;
    QtProperty *measureFPSProperty;

    bool visualizationParameterChange;

    QSet<MSceneViewGLWidget*> syncCameras;
    MSceneViewGLWidget *cameraSyncronizedWith;

    MActor *singleInteractionActor;

    /**
      If this variable is set to false, the qt property changed events will be
      ignored for the instance of the scene.
     */
    bool enablePropertyEvents;

    MResizeWindowDialog *resizeViewDialog;

    // Overwrite already existing image files of a time series without
    // asking the user.
    bool overwriteImageSequence;
};

} // namespace Met3D

#endif
