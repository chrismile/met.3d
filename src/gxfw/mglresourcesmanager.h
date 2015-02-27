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
#ifndef MGLRESOURCESMANAGER_H
#define MGLRESOURCESMANAGER_H

// standard library imports
#include <memory>

// related third party imports
#include "GL/glew.h"
#include <QtGui>
#include <QGLWidget>
#include <QGLShaderProgram>
#include "qtpropertymanager.h"

// local application imports
#include "util/mutil.h"
#include "util/mstopwatch.h"
#include "gxfw/mscenecontrol.h"
#include "gxfw/mactor.h"
#include "gxfw/textmanager.h"
#include "gxfw/gl/texture.h"
#include "gxfw/msystemcontrol.h"
#include "gxfw/gl/abstractgpudataitem.h"
#include "data/datarequest.h"


namespace Met3D
{

/**
  @brief MGLResourcesManager manages the graphics resources of the application.
  This includes OpenGL resources (textures, shaders, etc.) and graphics related
  objects of the Met.3D system (actors, scenes, related GUI elements).

  Internally, the class implements a hidden OpenGL context that shares its
  shader programs, textures, etc. with the visible contexts (@ref
  MSceneViewGLWidget).

  There is only one instance of MGLResourcesManager in the system (singleton
  pattern).
  */
class MGLResourcesManager : public QGLWidget
{
    Q_OBJECT

public:
    /**
     */
    static void initialize(const QGLFormat &format, QWidget *parent = 0,
                           QGLWidget *shareWidget = 0);

    ~MGLResourcesManager();

    /**
     Returns the (singleton) instance of the system control. If getInstance()
     is called for the first time an optional parent widget can be passed as
     argument.
     */
    static MGLResourcesManager* getInstance();

    /**
      Place a scene in a managed pool of scenes. All registered scenes are
      deleted by the MGLResourcesManager destructor.
      */
    void registerScene(MSceneControl *scene);

    MSceneControl* getScene(QString name);

    QList<MSceneControl*>& getScenes();

    void deleteScene(QString name);

    /**
      Place an actor in a managed pool of actors. All registered actors are
      deleted by the MGLResourcesManager destructor.
      */
    void registerActor(MActor *actor);

    /**
      Returns a shared pointer to a new shader effect program if no program
      with the given name has been created so far, or a pointer to the
      existing shader program.
      */
    bool generateEffectProgram(
            QString name,
            std::shared_ptr<GL::MShaderEffect>& effectProgram);

    /**
      @deprecated

      @note Use the @ref tryStoreGPUItem() mechanism instead.

      Returns true if a new texture object was created and texture data needs
      to be uploaded to GPU. False if a dataset with the specified name already
      exists. In both cases texture object name and texture unit number are
      returned to which the corresponding OpenGL context using the actor should
      bind.
      */
    bool generateTexture(QString name, GLuint *textureObjectName);

    /**
      @deprecated

      @note Use the @ref tryStoreGPUItem() mechanism instead.

      Create a new texture while managing video memory (via active/released
      textures). An error is generated if no video memory is available.

      Created textures have to be released by calling @ref releaseTexture()
      once they are not required anymore.

      @see videoMemoryLimit_kb
      */
    GL::MTexture* createTexture(QString key,
                               bool*   existed,
                               GLenum  target,
                               GLint   internalFormat,
                               GLsizei width,
                               GLsizei height=-1,
                               GLsizei depth=-1);

    /**
      @deprecated

      @note Use the @ref tryStoreGPUItem() mechanism instead.

      Check if a texture is associated with the given key and if it is in
      video memory. Returns a NULL pointer if no texture with the given key
      is in video memory.
      */
    GL::MTexture* checkTexture(QString  key);

    /**
      @deprecated

      @note Use the @ref tryStoreGPUItem() mechanism instead.

      Releases the given texture. The texture is cached and queued for deletion,
      which occurs if video memory is required by another texture.

      Deletion is implemented in @ref createTexture().
      */
    void releaseTexture(GL::MTexture *texture);

    /**
      @deprecated

      @note Use the @ref tryStoreGPUItem() mechanism instead.

      Overload for releaseTexture(GL::Texture *texture).
      */
    void releaseTexture(QString key);

    /**
      @deprecated

      @note Use the @ref tryStoreGPUItem() mechanism instead.

      Explicitly deletes the released texture associated with "key". If the key
      doesn't exist, it is silently ignored.
      */
    void deleteReleasedTexturesOfBaseKey(QString baseKey);

    /**
      Explicitly deletes the actor with given name, if any actor exists.
     */
    void deleteActor(const QString name);

    /**
      Releases the given actor from the resource manager
     */
    void deleteActor(MActor* actor);

    /**
      Put the @ref GL::MAbstractGPUDataItem @p item into memory management.
      Returns @p true on success; @p false if there is not enough memory to
      store the given item, or if another item is already stored using the
      item's request key.
     */
    bool tryStoreGPUItem(GL::MAbstractGPUDataItem *item);

    /**
      Returns the @ref GL::MAbstractGPUDataItem memory managed under the given
      @p key. Returns a null pointer is no item with the given request key is
      available.

      Acquired items must be released by a call to @ref releaseGPUItem() to
      ensure correct memory management.
     */
    GL::MAbstractGPUDataItem* getGPUItem(Met3D::MDataRequest key);

    /**
      Release a data item previously obtained with @ref getGPUItem().
     */
    void releaseGPUItem(GL::MAbstractGPUDataItem *item);

    void releaseGPUItem(Met3D::MDataRequest key);

    void releaseAllGPUItemReferences(Met3D::MDataRequest key);

    void updateGPUItemSize(GL::MAbstractGPUDataItem *item);

    bool isManagedGPUItem(GL::MAbstractGPUDataItem *item);

    /**
      Trigger a reload of the GLSL shader effects of all registered actors.
     */
    void reloadActorShaders();

    /**
      Returns the total and available GPU memory in kB. Currently only
      implemented for NVIDIA cards, uses the GL_NVX_gpu_memory_info extension.
      */
    void gpuMemoryInfo_kb(uint &total, uint &available);

    /**
      Returns the total and available CPU memory in kB. Currently
      Linux-specific implementation, information is read from /proc/meminfo.
      */
    void systemMemoryInfo_kb(uint &total, uint &available);

    void initializeTextManager();

    /**
      Returns a pointer to the text manager.
      */
    MTextManager* getTextManager();

    /**
      Reference to the list of registered actors.
      */
    const QVector<MActor*>& getActors() const { return actorPool; }

    MActor* getActorByName(const QString& name) const;

    /**
      Registers a factory for an actor that can be created at program runtime.

      @see MActorCreationDialog
      @see MApplicationConfigurationManager::registerActorFactories()
     */
    void registerActorFactory(MAbstractActorFactory* factory);

    /**
      Remove a registered actor factory method from the list of factories
      displayed to the user.
     */
    void deleteActorFactory(const QString& name);

    const QList<QString> getActorFactoryNames() const;

    MAbstractActorFactory* getActorFactory(const QString& name);

    QList<MAbstractActorFactory*> getActorFactories();

public slots:
    /**
      Updates the property labels that display the current memory usage. The
      slot is called by a timer to update every two seconds.
      */
    void displayMemoryUsage();

    void dumpMemoryContent(QtProperty *property);

signals:
    /**
     This signal is emitted when a new actor has been created and added to the
     actor pool.
     */
    void actorCreated(MActor* actor);

    /**
     This signal is emitted when an actor has been deleted and removed from the
     actor pool.
     */
    void actorDeleted(MActor* actor);

protected:
    /**
      Initialize the OpenGL resources. Called once on program start.
     */
    void initializeGL();

private:
    /**
     Constructor is private, as it should only be called from getInstance().
     See https://en.wikipedia.org/wiki/Singleton_pattern#Lazy_initialization.
     */
    MGLResourcesManager(const QGLFormat &format, QWidget *parent = 0,
                        QGLWidget *shareWidget = 0);

    /** Single instance of the GL resources manager. */
    static MGLResourcesManager* instance;

    /** Scenes */
    QList<MSceneControl*> scenePool;

    /** Actors and actor factories */
    QVector<MActor*> actorPool;
    QMap<QString, MAbstractActorFactory*> actorFactoryPool;

    /** Shaders */
    QMap<QString, std::shared_ptr<GL::MShaderEffect> > effectPool;

    /** Textures */
    QMap<QString, GLuint> textureObjectPool;

    // Dictionary of active (=used) textures.
    QHash<QString, GL::MTexture*> activeTexturesPool;
    // Reference counter for each texture, needs to be 0 to release texture.
    QHash<QString, int         > referenceCounter;
    // Released (=cached) textures, can be deleted at any time.
    QHash<QString, GL::MTexture*> releasedTexturesPool;
    // Queue giving the order in which released textures are deleted.
    QList<QString>               releasedTexturesQueue;
    // Dictionary that keeps lists of texture keys that belong to a given
    // base key (used for deleteReleasedTexturesOfBaseKey).
    QHash<QString, QStringList  > baseKeyToTextureKeysDict;
    // Amount of video memory in kb the application is allowed to consume.
    uint videoMemoryLimit_kb;
    // Amount of currently used video memory.
    uint videoMemoryUsage_kb;

    // Dictionary of active (=used) textures.
    QHash<Met3D::MDataRequest, GL::MAbstractGPUDataItem*> activeGPUItems;
    // Reference counter for each texture, needs to be 0 to release texture.
    QHash<Met3D::MDataRequest, int> gpuItemsReferenceCounter;
    // Released (=cached) textures, can be deleted at any time.
    QHash<Met3D::MDataRequest, GL::MAbstractGPUDataItem*> releasedGPUItems;
    // Queue giving the order in which released textures are deleted.
    QList<Met3D::MDataRequest> releasedGPUItemsQueue;
    // Stores the memory usage in kb for each item.
    QHash<Met3D::MDataRequest, unsigned int> gpuItemsMemoryUsage_kb;

    MTextManager *textManager;
    MSystemManagerAndControl *systemControl;

    QtProperty *propertyGroup;
    QtProperty *totalVideoMemoryProperty;
    QtProperty *met3DVideoMemoryProperty;
    QtProperty *totalSystemMemoryProperty;
    QtProperty *dumpMemoryContentProperty;
};

} // namespace Met3D

#endif // MGLRESOURCESMANAGER_H
