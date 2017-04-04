/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2017      Bianca Tost
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
#ifndef MSCENECONTROL_H
#define MSCENECONTROL_H

// standard library imports

// related third party imports
#include "qttreepropertybrowser.h"

// local application imports
#include "gxfw/mactor.h" // import required for correct render queue return type
#include "gxfw/synccontrol.h"


namespace Ui {
class MSceneControl;
}

namespace Met3D
{

class MSceneViewGLWidget;

/**
  @brief MSceneControl represents a scene (a group of actors and their
  properties) that can be viewed with an @ref MSceneViewGLWidget.

  The class keeps a list of @ref MActor s that belong to the scene and provides
  a GUI "control" for the user to view and modify properties of the actors. The
  scene can be connected to different @ref MSceneViewGLWidget s, so that
  views from different viewpoints are possible.
  */
class MSceneControl : public QWidget
{
    Q_OBJECT

public:
    MSceneControl(QString name, QWidget *parent = 0);
    ~MSceneControl();

    /**
      Adds an actor to the scene. If successful, returns an ID number for the
      actor (can be used with @ref removeActor() to remove the actor from the
      scene). If unsuccessful, -1 is returned. If the argument @p index is
      specified, the actor is placed at position @p index in this scenes render
      queue.
      */
    int addActor(MActor *actor, int index=-1);

    /**
      Removes the actor with the specified @p id from the scene. The ID of
      an actor is returned by @ref addActor().
      */
    void removeActor(int id);

    /**
      Removes the actor with the specified @p name from the scene.
      */
    void removeActorByName(const QString& actorName);

    /**
      Obtains the current render queue id of the actor with specified @p name
      from the scene.
     */
    int getActorRenderID(const QString& actorName) const;

    /**
      Returns the list of actors in this scene that need to be rendered on
      frame updates.
      */
    QVector<MActor*>& getRenderQueue() { return renderQueue; }

    /**
      Sets the name of the scene.
      */
    void setName(const QString& sceneName) { name = sceneName; }

    /**
      Returns the name of the scene.
      */
    const QString getName() const { return name; }

    /**
     Sets the background colour of the given @p property to @p colour.
     */
    void setPropertyColour(QtProperty *property, QColor colour);

    /**
     Reset the background colour of the @ref QtProperty @p property to the
     default value.
     */
    void resetPropertyColour(QtProperty *property);

    void variableSynchronizesWith(MSyncControl *sync);

    void variableDeletesSynchronizationWith(MSyncControl *sync);

    /**
      Let the scene know that it is rendered in the scene view @p view.
     */
    void registerSceneView(MSceneViewGLWidget *view);

    /**
      The scene isn't rendered in @p view any longer.
     */
    void unregisterSceneView(MSceneViewGLWidget *view);

    const QSet<MSceneViewGLWidget*>& getRegisteredSceneViews() const
    { return registeredSceneViews; }

    static MQtProperties *getQtProperties();

    QtTreePropertyBrowser* getActorPropertyBrowser()
    { return actorPropertiesBrowser; }

    void collapsePropertySubTree(QtProperty *property);

    void collapseActorPropertyTree(MActor *actor);

    /**
      If the scene view is in "actor interaction mode": make @p actor the only
      actor in the scene with which the user can interact. (Used, e.g., to
      display a pole to select a scene rotation centre).
     */
    void setSingleInteractionActor(MActor *actor);

public slots:
    /**
      This slot collects the @ref MActor::actorChanged() signals of all
      registered actors. It emits a @ref sceneChanged() signal. The purpose is
      to avoid having to connect the @ref MActor::actorChanged() signals to all
      scene views. Instead, a scene view has to observe only the @ref
      sceneChanged() signal for updates.
      */
    void onActorChanged();

    /**
      Instructs each actor of the scene to reload its shaders. Mainly intended
      for shader development purposes (modify the shader without restarting the
      entire application.
      */
    void reloadActorShaders();

    /**
     Blocks all redraw operations (emissions of @ref sceneChanged()) until @ref
     endBlockingRedraws() has been called.
     */
    void startBlockingRedraws();

    /**
     Enables the emission of @ref sceneChanged() after a call to @ref
     startBlockingRedraws.
     */
    void endBlockingRedraws();

signals:
    /**
      Emitted when the scene changes and a redraw in the viewport is required.
      Connect all @ref MSceneViewGLWidget instances that display this scene to
      this signal.
      */
    void sceneChanged();

    void sceneViewAdded();

protected:
    QVector<MActor*> renderQueue;
    QVector<QtBrowserItem*> browserItems;

private:
    Ui::MSceneControl *ui;
    QString name;

    QtTreePropertyBrowser *actorPropertiesBrowser;

    int  blockRedrawRequests;
    bool actorChangeDuringBlock;
    QMap<MSyncControl*, int> syncControlCounter;

    QSet<MSceneViewGLWidget*> registeredSceneViews;

    static MQtProperties *qtProperties;
    QList<QtAbstractEditorFactoryBase*> factories;
    static QMutex staticSceneControlAccessMutex;
};

} // namespace Met3D

#endif // MSCENECONTROL_H
