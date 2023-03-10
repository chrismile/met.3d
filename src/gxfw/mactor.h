/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus [*, previously +]
**  Copyright 2016-2018 Bianca Tost [+]
**
**  + Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
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
#ifndef MACTOR_H
#define MACTOR_H

// standard library imports
#include <memory>

// related third party imports
#include <QtCore>
#include <QtProperty>
#include <QWidgetAction>
#include <QProgressDialog>

// local application imports
#include "gxfw/mtypes.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/vertexbuffer.h"
#include "system/qtproperties.h"
#include "gxfw/camera.h"

namespace Met3D
{

class MGLResourcesManager;
class MSceneViewGLWidget;
class MSceneControl;


/**
  @brief MActor is the abstract base class for all actors.

  It inherits from QObject to use Qt's signal/slot mechanism.
  */
class MActor : public QObject
{
    Q_OBJECT
    friend class MMultiVarData;

public:
    /**
      Constructs a new MActor with a unique identification number (@ref
      getID()) and initialises a property group for the new actor. The MActor
      property group handles properties common for all actors, derived actor
      classes should add their own properties in their constructor.

      @note This constructor NEEDS to be called from derived constructors.
      */
    MActor(QObject *parent = 0);

    /**
      */
    ~MActor();

    /**

     */
    void initialize();

    /**
      Initialise OpenGL resources of the actor that cannot be shared between
      OpenGL contexts. According to the OpenGL 4.2 core specification, Appendix
      D, this includes:

      "Objects which contain references to other objects include framebuffer,
      program pipeline, query, and vertex array objects. Such objects are
      called container objects and are not shared."

      @see initializeActorResources()
      */
    virtual void initializePerGLContextResources(MSceneViewGLWidget *sceneView)
    {
        Q_UNUSED(sceneView);
    }

    /**
      If the actor is enabled (property @ref enabled is set to true), render
      the actor in the current OpenGL context. Calls the virtual method @ref
      renderToCurrentContext() to perform the actual rendering in the derived
      classes. Do not overwrite this method, but reimplement
      @ref renderToCurrentContext() instead.
      */
    void render(MSceneViewGLWidget *sceneView);

    /**
      If the actor is enabled (property @ref enabled is set to true), render
      the actor in the current OpenGL context. Calls the virtual method @ref
      renderOverlayToCurrentContext() to perform the actual rendering in the
      derived classes. Do not overwrite this method, but reimplement
      @ref renderOverlayToCurrentContext() instead.
      */
    void renderOverlay(MSceneViewGLWidget *sceneView);

    /**
      If the actor is enabled (property @ref enabled is set to true), render
      the actor in the current OpenGL context in full-screen mode. Calls the
      virtual method @ref renderToCurrentContextFullScreen() to perform the
      actual rendering in the derived classes. Do not overwrite this method,
      but reimplement @ref renderToCurrentContextFullScreen() instead.
      */
    void renderToFullScreen(MSceneViewGLWidget *sceneView);

    /**
      Returns whether the actor has been initialised. Usually true after @ref
      initialize() has been called.
      */
    bool isInitialized();

    /**
      Returns true if the actor is enabled.
     */
    bool isEnabled();

    /**
      Recompiles the actor's GLSL shaders. Needs to call
      @ref beginCompileShaders() and @ref endCompileShaders().
      */
    virtual void reloadShaderEffects() = 0;

    /**
      Returns actor type.
    */
    static QString staticActorType() { return "Default actor"; }

    /**
      Returns actor type.

      Note: Actor type needs to be set in the constructor.
    */
    virtual QString getActorType() { return actorType; }

    /**
      Returns a unique number that identifies the actor.
      */
    unsigned int getID();

    /**
      Sets the name of the actor.
      */
    void setName(const QString name);

    /**
      Returns the name of the actor (set by @ref setName()).
      */
    QString getName();

    /**
      Returns a @ref QtProperty instance acting as parent to all the properties
      of this actor (i.e. representing the properties "group"). This instance
      can be added to a @ref QtAbstractPropertyBrowser.

      @see http://doc.qt.nokia.com/solutions/4/qtpropertybrowser/index.html
      */
    QtProperty* getPropertyGroup() { return propertyGroup; }

    /**
      Informs this actor that it has been added to the @ref MSceneControl @p
      scene. The scene is added to the protected property @ref scenes. If the
      scene is already contained in @ref scenes, it is not added a second time.

      <em> Do not call this function directly. </em> It is implicitely called
      from @ref MSceneControl::addActor(), which should be used to add an actor
      to a scene.

      @see MSceneControl::addActor()
      */
    virtual void registerScene(MSceneControl *scene);

    virtual void deregisterScene(MSceneControl *scene);

    void onSceneViewAdded() {}

    /**
     Provides information of how many elements (e.g. NWP variables) of this
     actor are connected to a MSyncControl. Needs to be reimplemented in
     derived classes and MSceneControl::variableSynchronizesWith() needs to be
     called accordingly.
     */
    virtual void provideSynchronizationInfoToScene(MSceneControl *scene)
    {
        Q_UNUSED(scene);
    }

    /**
      Returns a list of the scenes in which this actor has been registered.
     */
    const QList<MSceneControl*>& getScenes() { return scenes; }

    /**
      Clears list of the scenes in which this actor has been registered.
     */
    void clearScenes() { scenes.clear(); }

    /**
      Returns a list of the views in which this actor appears.
     */
    const QList<MSceneViewGLWidget*> getViews();

    /**
      Returns a list with the labels that shall be rendered for this actor.
      */
    QList<MLabel*> getLabelsToRender();

    /**
      Returns the position labe in a list if it shall be rendered for this actor.
      */
    virtual QList<MLabel*> getPositionLabelToRender();

    /**
      Returns @p true if the actor can be picked and dragged in modify mode.
      */
    bool isPickable() { return actorIsPickable; }

    /**
      Returns @p true if the actor has data that can be selected.
      */
    bool hasSelectableData() { return actorHasSelectableData; }

    /**
      Returns a handle ID if the clip space coordinates of a handle of this
      actor are within @p clipRadius of (@p clipX, @p clipY); or -1 otherwise.

      Called by a @ref MSceneViewGLWidget in modification mode.
      */
    virtual int checkIntersectionWithHandle(MSceneViewGLWidget *sceneView,
                                            float clipX, float clipY)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(clipX);
        Q_UNUSED(clipY);
        return -1;
    }

    /**
      Checks whether selectable data of this actor is at
      @p mousePositionX, @p mousePositionY.

      Called by a @ref MSceneViewGLWidget.
      */
    virtual bool checkIntersectionWithSelectableData(
            MSceneViewGLWidget *sceneView, QMouseEvent *event)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(event);
        return false;
    }

    /**
      Checks whether a virtual (i.e., drawn using OpenGL) window is below
      the mouse cursor at @p mousePositionX, @p mousePositionY.

      Called by a @ref MSceneViewGLWidget.
      */
    virtual bool checkVirtualWindowBelowMouse(
            MSceneViewGLWidget *sceneView, int mousePositionX, int mousePositionY)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(mousePositionX);
        Q_UNUSED(mousePositionY);
        return false;
    }

    virtual void mouseMoveEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(event);
    }

    /*
     * This function is called when the mouse is moved in the parent widget.
     *
     * Called by a @ref MSceneViewGLWidget.
     */
    virtual void mouseMoveEventParent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(event);
    }

    virtual void mousePressEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(event);
    }

    virtual void mouseReleaseEvent(MSceneViewGLWidget *sceneView, QMouseEvent *event)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(event);
    }

    virtual void wheelEvent(MSceneViewGLWidget *sceneView, QWheelEvent *event)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(event);
    }


    /**
      Creates a label representing the position of handle with ID @p handleID
      near the handle.

      Called by a @ref MSceneViewGLWidget in modification mode.
      */
    virtual void addPositionLabel(MSceneViewGLWidget *sceneView, int handleID,
                                  float clipX, float clipY)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(handleID);
        Q_UNUSED(clipX);
        Q_UNUSED(clipY);
    }

    /**
      Removes label displaying the position if present.

      Called by a @ref MSceneViewGLWidget in modification mode.
      */
    virtual void removePositionLabel();

    /**
      Drags the handle with ID @p handleID to the clip space point (@p clipX,
      @p clipY).

      Called by a @ref MSceneViewGLWidget in modification mode.
      */
    virtual void dragEvent(MSceneViewGLWidget *sceneView,
                           int handleID, float clipX, float clipY)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(clipX);
        Q_UNUSED(clipY);
        Q_UNUSED(handleID);
    }

    /**
      Triggers the analysis of a potential object at the clip space position @p
      clipX, @p clipY. If the trigger is successful, true shall be returned. If
      no object can be identified at the given position, the method should
      return false.

      Called by a @ref MSceneViewGLWidget in analysis mode.
     */
    virtual bool triggerAnalysisOfObjectAtPos(MSceneViewGLWidget *sceneView,
                                              float clipX, float clipY,
                                              float clipRadius)
    {
        Q_UNUSED(sceneView);
        Q_UNUSED(clipX);
        Q_UNUSED(clipY);
        Q_UNUSED(clipRadius);
        return false;
    }

    void setEnabled(bool enabled);

    void setLabelsEnabled(bool enabled);

    /**
      Returns an ID unique to the actor type. For example, this ID is used to
      ifentify the actor type in configuration files and for runtime actor
      creation.

      @note This function NEEDS to be overridden in derived classes.
     */
    virtual QString getSettingsID() { return "MActor"; }

    static QString getStaticSettingsID() { return "MActor"; }

    /**
     Save the actor configuration to the file @p filename. Calls @ref
     saveConfiguration() to store the actor-specific settings of derived
     classes.
     */
    void saveConfigurationToFile(QString filename = "");

    void loadConfigurationFromFile(QString filename = "");

    /**
     Save actor-specific configuration to the @ref QSettings object @p
     settings.

     @note Override this function if your derived class needs to store
     configuration.
     */
    void saveActorConfiguration(QSettings *settings);

    /**
     Load actor-specific configuration from the @ref QSettings object @p
     settings.
     */
    void loadActorConfiguration(QSettings *settings);

    /**
     Save actor-specific configuration to the @ref QSettings object @p
     settings.

     @note Override this function if your derived class needs to store
     configuration.
     */
    virtual void saveConfiguration(QSettings *settings) { Q_UNUSED(settings); }

    /**
     Load actor-specific configuration from the @ref QSettings object @p
     settings.
     */
    virtual void loadConfiguration(QSettings *settings) { Q_UNUSED(settings); }

    /**
      Get a reference to the property managers responsible for the QtProperties
      used to control this actor.
     */
    MQtProperties* getQtProperties();

    /**
      Call this method before any new properties are added to the actor
      via @ref addProperty().

      @note Must be followed by a call to @ref endInitialiseQtProperties().
     */
    void beginInitialiseQtProperties();

    /**
     Call this method to finish a section started with @ref
     beginInitialiseQtProperties().
     */
    void endInitialiseQtProperties();

    /**
      Adds a GUI QtProperty to this actor's properties. Put calls to this
      method between @ref beginInitialiseQtProperties() and @ref
      endInitialiseQtProperties().

      If @p group is specified, the new property is added to this property
      group.

      @p addProperty() automatically connects to the signals of the property
      manager used for the new property.
     */
    QtProperty* addProperty(MQtPropertyType type, const QString &name,
                            QtProperty *group = nullptr);

    /**
      Removes a GUI QtProperty to this actor's properties.

      If @p group is specified, the new property is removed from this property
      group.

      @p removeProperty() automatically disconnects from the signals of the
      property manager used for the new property.
     */
    void removeProperty(QtProperty* property, QtProperty *group = nullptr);

    void collapseActorPropertyTree();

    /**
      Set to @p false if the actor should not be deleted by the user.
      Useful for actors that exist only as a single instance in the system,
      e.g. text labels.
     */
    void setActorIsUserDeletable(bool b) { actorIsUserDeletable = b; }

    bool getActorIsUserDeletable() { return actorIsUserDeletable; }

    /**
      Returns @p true if this actor is in some way connected to the argument
      @p actor.

      @note Override this method in your derived class.
     */
    virtual bool isConnectedTo(MActor *actor) { Q_UNUSED(actor); return false; }

    /**
      Returns @p true if this actor supports visualisation of multiple ensemble
      members (e.g. spaghetti plots).

      @note Override this method in your derived class.
     */
    virtual bool supportsMultipleEnsembleMemberVisualization()
    { return actorSupportsMultipleEnsembleMemberVisualization; }

    /**
      Returns @p true if this actor supports visualisation in full-screen mode.
     */
    virtual bool supportsFullScreenVisualisation()
    { return actorSupportsFullScreenVisualisation; }

    /**
      Called if full-screen mode is switched.

      @p fullScreenEnabled is true if full-screen mode is switched on and to
      false if switched off.

     Needs to be reimplemented in derived classes which support full-screen
     visualisation and need to react when full-screen mode is changed.
     */
    virtual void onFullScreenModeSwitch(MSceneViewGLWidget *sceneView,
                                        bool fullScreenEnabled)
    { Q_UNUSED(sceneView); Q_UNUSED(fullScreenEnabled); }

public slots:
    /**
      Handles change events of the properties in the property browser. Calls
      @ref onQtPropertyChanged(), in which derived classes can handle
      property change events.
     */
    void actOnQtPropertyChanged(QtProperty *property);

    /**
      Called when another actor (not this one) in the global actor pool has been
      created. Calls @ref onOtherActorCreated(), in which derived classes can
      handle actor deletion events.
     */
    void actOnOtherActorCreated(MActor *actor);

    /**
      Called when another actor (not this one) in the global actor pool has been
      deleted. Allows this actor to take actions if any connection exists
      between this actor and the deleted actor. Calls @ref onOtherActorDeleted(),
      in which derived classes can handle actor deletion events.
     */
    void actOnOtherActorDeleted(MActor *actor);

    /**
      Called when another actor (not this one) in the global actor pool has been
      renamed. Calls @ref onOtherActorRenamed(), in which derived classes can
      handle actor renaming events.
     */
    void actOnOtherActorRenamed(MActor *actor, QString oldName);

signals:
    /**
      Emitted when a property of the actor has changed. Should also be emitted
      in derived classes when a property added in the derived class has
      changed.

      @note Never emit directly, always use @ref signalActorChanged()!
      */
    void actorChanged();

    /**
      Emitted when the name of the actor has been changed via @ref setName().
     */
    void actorNameChanged(MActor *actor, QString oldName);

protected:
    void setActorType(QString actorType) { this->actorType = actorType; }

    /**
      Initialise the actor, e.g. load GLSL shader programs, create GPU OpenGL
      resources that can be shared between OpenGL contexts.

      @note Please check the OpenGL 4.2 core specification, Appendix D, to make
      sure the created resources can be shared.

      @see initializePerGLContextResources()
      */
    virtual void initializeActorResources() = 0;

    /**
      Implement this function in derived classes to handle property change
      events. @see actOnQtPropertyChanged().
     */
    virtual void onQtPropertyChanged(QtProperty *property)
    { Q_UNUSED(property); }


    /**
      Implement this function in derived classes to handle actor creation
      events. @see actOnOtherActorCreated().
     */
    virtual void onOtherActorCreated(MActor *actor)
    { Q_UNUSED(actor); }

    /**
      Implement this function in derived classes to handle actor deletion
      events. @see actOnOtherActorDeleted().
     */
    virtual void onOtherActorDeleted(MActor *actor)
    { Q_UNUSED(actor); }

    /**
      Implement this function in derived classes to handle actor renaming
      events. @see actOnOtherActorRenamed().
     */
    virtual void onOtherActorRenamed(MActor *actor, QString oldName)
    { Q_UNUSED(actor); Q_UNUSED(oldName); }

    // Define friends for suppressActorUpdates().
    friend class MVerticalRegridProperties;
    friend class MSessionManagerDialog;

    /**
      Query this method in implementations of @ref onQtPropertyChanged() to
      know whether updates to any part of the actor (e.g. loading of new data,
      upload of resources to the GPU) should be ommited.

      Returns @p false if either the actor has not been initialized yet
      (@see initialize()) or if updated are suppressed by a call to
      @ref enableActorUpdates()).
     */
    bool suppressActorUpdates();

    // Define friends for enableEmissionOfActorChangedSignal().
    friend class MNWPHorizontalSectionActor;

    /**
      Enables/disables the emission of the @ref actorChanged() signal. Use this
      to change actor properties without triggering a redraw of the scene.
      */
    void enableEmissionOfActorChangedSignal(bool enabled);

    /**
      Render the actor in the current OpenGL context.

      Needs to be implemented in derived classes.
      */
    virtual void renderToCurrentContext(MSceneViewGLWidget *sceneView) = 0;

    /**
      Render the actor's overlays in the current OpenGL context.

      Can be implemented in derived classes.
      */
    virtual void renderOverlayToCurrentContext(MSceneViewGLWidget *sceneView) {}

    /**
      Render the actor in the current OpenGL context in full screen mode.

      Needs to be implemented in derived classes supporting full-screen mode.
      */
    virtual void renderToCurrentFullScreenContext(
            MSceneViewGLWidget *sceneView)
    { Q_UNUSED(sceneView); }

    void enablePicking(bool p) { actorIsPickable = p; }

    void setHasSelectableData(bool p) { actorHasSelectableData = p; }

    /**
      This method needs to be called at the beginning of each actor's
      @ref reloadShaderEffects() method, indicating the number of shaders
      that will be compiled. It needs to be followed by a call to
      @ref endCompileShaders() after all shaders have been compiled.

      The method sets up the "compile shaders" progress dialog. It resets
      @ref shaderCompilationProgress to @p 0.
      */
    void beginCompileShaders(int numberOfShaders);

    /**
      See @ref beginCompileShaders().
     */
    void endCompileShaders();

    /**
      Compiles a GLSL shader from @p filename and updates the "compile shaders"
      progress dialog. Uses @ref shaderCompilationProgress to update number of
      already compiled shaders.
      */
    void compileShadersFromFileWithProgressDialog(
            std::shared_ptr<GL::MShaderEffect> shader,
            const QString filename);

    /**
      Compiles a GLSL shader from @p filename and updates the "compile shaders"
      progress dialog. Uses @ref shaderCompilationProgress to update number of
      already compiled shaders.
      */
    void compileShadersFromFileWithProgressDialog(
            std::shared_ptr<GL::MShaderEffect> shader,
            const QString filename,
            const QMap<QString, QString>& defines);

    /**
      Emit the @ref actorChanged() signal, but only if the signal is enabled,
      the actor is enabled and the actor is initialized.

      @see enableEmissionOfActorChangedSignal()
     */
    void emitActorChangedSignal();

    void listenToPropertyManager(const QObject *sender);

    /**
      Disables/enables actor updates. Usage:

      enableActorUpdates(false);
      [code that doesn't allow actor updates, e.g. load-from-file]
      enableActorUpdates(true);

      Uses a counter to allow nested calls.

      @see suppressActorUpdates()
     */
    void enableActorUpdates(bool enable);

    // Define friends for request/release texture and image units.
    friend class MNWP2DSectionActorVariable;
    friend class MNWP2DHorizontalActorVariable;
    friend class MNWP3DVolumeActorVariable;

    GLint assignTextureUnit();

    void releaseTextureUnit(GLint unit);

    GLint assignImageUnit();

    void releaseImageUnit(GLint unit);

    double computePositionLabelDistanceWeight(MCamera *camera,
                                              QVector3D mousePosWorldSpace);

    /**
      Call this method from a derived actor's constructor to enable full-screen
      rendering support for this actor. If set to @p true, the actor will be
      listed as available in a scene view's full-screen rendering property.

      Compare to @ref MSceneViewGLWidget::onActorCreated(MActor *actor) .
     */
    void setActorSupportsFullScreenVisualisation(bool b)
    { actorSupportsFullScreenVisualisation = b; }

    /**
      Call this method from a derived actor's constructor to enable multiple
      ensemble member visualisation support for this actor. If set to @p true,
      actor variables will list the "multiple members" option in their ensemble
      mode list and will be connected to a grid aggregation data source.

      Compare to @ref MNWPActorVariable .
     */
    void setActorSupportsMultipleEnsembleMemberVisualization(bool b)
    { actorSupportsMultipleEnsembleMemberVisualization = b; }

    /**
      Override this method in derived actors to print debug output upon user
      request.
     */
    virtual void printDebugOutputOnUserRequest();


    // Properties common to all actors.
    //=================================

    MQtProperties *properties;

    QtProperty *propertyGroup;            /* Property group for this actor. */
    QtProperty *actorPropertiesSupGroup;  /* Subgroup for actor properties;
                                             derived actor classes may define
                                             additional subgroups for, e.g.,
                                             variables. */
    QtProperty *actorConfigurationSupGroup;
    QtProperty *actorDevelopmentSupGroup;

    bool        actorIsEnabled;           /* Whether actor will be rendered. */
    QtProperty *actorEnabledProperty;
    bool        labelsAreEnabled;         /* Whether labels will be rendered. */
    QtProperty *labelsEnabledProperty;
    bool        renderAsWireFrame;        /* True if the actor should be
                                             rendered in wireframe mode. */
    QtProperty *wireFrameProperty;

    QtProperty *saveConfigProperty;
    QtProperty *loadConfigProperty;

    QtProperty *reloadShaderProperty;
    QtProperty *printDebugOutputProperty;

    QtProperty *labelPropertiesSupGroup;  /* Subgroup for label properties. */
    QtProperty *labelColourProperty;
    QtProperty *labelSizeProperty;
    QtProperty *labelBBoxProperty;
    QtProperty *labelBBoxColourProperty;

    bool actorIsPickable;
    bool actorHasSelectableData;

    /** Counter to monitor the progress of shader loading process. */
    QProgressDialog *shaderCompilationProgressDialog;
    int  shaderCompilationProgress;

    /** List of scenes to which this actor has been added. */
    QList<MSceneControl*> scenes;

    /** List of labels that belong to this actor. */
    QList<MLabel*> labels;

    MLabel* positionLabel;

    /**
     Removes all labels of this actor. Calls @ref MTextManager::removeText()
     for each label in @ref labels and deletes the label from the list.
     */
    void removeAllLabels();

    /**
     @deprecated Uploads a QVector<QVector3D> data field to the vertex buffer
     @p vbo.

     @note Use @ref MTypedVertexBuffer functionality instead.
     */
    void uploadVec3ToVertexBuffer(const QVector<QVector3D> *data, GLuint *vbo);

    void uploadVec3ToVertexBuffer(
            const QVector<QVector3D>& data,
            const QString requestKey,
            GL::MVertexBuffer** vbo,
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif

    void uploadVec2ToVertexBuffer(
            const QVector<QVector2D>& data,
            const QString requestKey,
            GL::MVertexBuffer** vbo,
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif

    /** Unique integer identifying this actor, assigned in the constructor. */
    unsigned int myID;

    bool multipleEnsembleMembersEnabled;

private:
    static unsigned int idCounter;

    QString actorName;
    QString actorType;

    int addPropertiesCounter;
    QSet<QtAbstractPropertyManager*> connectedPropertyManagers;

    bool actorIsInitialized;
    int actorChangedSignalDisabledCounter;
    int actorUpdatesDisabledCounter;

    // "true" (default) if the actor can be deleted from the GUI.
    bool actorIsUserDeletable;
    // "true" if the actor supports full-screen visualisation.
    // (default = "false")
    bool actorSupportsFullScreenVisualisation;
    // "true" if the actor supports multiple ensemble member visualisation.
    // (default = "false")
    bool actorSupportsMultipleEnsembleMemberVisualization;

    QList<GLint> availableTextureUnits;
    QList<GLint> assignedTextureUnits;
    QList<GLint> availableImageUnits;
    QList<GLint> assignedImageUnits;
};


/**
 @brief Base class for actor factories. Actors that can be instantiated during
 runtime require an actor factory. Derive from this class and override the @ref
 createInstance() method to create an instance of the derived actor class.

 Actor factories need to be registered with the @ref MGLResourcesManager,
 @see MApplicationConfigurationManager::registerActorFactories()
 */
class MAbstractActorFactory
{
public:
    MAbstractActorFactory();
    virtual ~MAbstractActorFactory();

    void initialize();

    QString getName();

    MActor* create(const QString& configfile = QString());

    bool acceptSettings(QSettings *settings);

    bool acceptSettings(const QString& configfile);

protected:
    virtual MActor* createInstance() = 0;

    /**
      Call this function in the derived @ref createInstance() method to display
      a warning that the actor is still in an "experimental" state -- useful
      for development versions.
     */
    void displayWarningExperimentalStatus();

    QString name;
    QString settingsID;
    bool isInitialized;
};

} // namespace Met3D

#endif // MACTOR_H
