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
#include "mglresourcesmanager.h"

// standard library imports
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <limits>

// related third party imports
#include <log4cplus/loggingmacros.h>
#ifdef USE_QOPENGLWIDGET
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#endif

// local application imports
#include "gxfw/textmanager.h"
#include "util/mexception.h"
#include "util/mutil.h"

using namespace std;

namespace Met3D
{

// Nvidia video memory info extension, see
// http://developer.download.nvidia.com/opengl/specs/GL_NVX_gpu_memory_info.txt
#define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX       0x9047
#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX    0x9048
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX  0x9049
#define GPU_MEMORY_INFO_EVICTION_COUNT_NVX         0x904A
#define GPU_MEMORY_INFO_EVICTED_MEMORY_NVX         0x904B

// Initially set the single GL resources manager pointer to null.
MGLResourcesManager* MGLResourcesManager::instance = 0;

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

// Constructor is private.
MGLResourcesManager::MGLResourcesManager(
#ifdef USE_QOPENGLWIDGET
        const QSurfaceFormat &format,
#else
        const QGLFormat &format,
#endif
        QWidget *parent,
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *shareWidget)
#else
        QGLWidget *shareWidget)
#endif
#ifdef USE_QOPENGLWIDGET
    : QOpenGLWidget(parent),
#else
    : QGLWidget(format, parent, shareWidget),
#endif
      globalMouseButtonRotate(Qt::LeftButton),
      globalMouseButtonPan(Qt::RightButton),
      globalMouseButtonZoom(Qt::MiddleButton),
      isReverseCameraZoom(false),
      isReverseCameraPan(false),
      videoMemoryUsage_kb(0),
      selectSceneCentreActor(nullptr),
      selectSceneCentreText(nullptr)
{
#ifdef USE_QOPENGLWIDGET
    requestedFormat = format;
    this->setFormat(format);
    create();
#endif

    // Get the system control instance.
    systemControl = MSystemManagerAndControl::getInstance();

    // Properties that are displayed in the system control.
    propertyGroup = systemControl->getGroupPropertyManager()
            ->addProperty("OpenGL resources");

    totalVideoMemoryProperty = systemControl->getStringPropertyManager()
            ->addProperty("available video memory");
    propertyGroup->addSubProperty(totalVideoMemoryProperty);

    met3DVideoMemoryProperty = systemControl->getStringPropertyManager()
            ->addProperty("NWP video memory usage");
    propertyGroup->addSubProperty(met3DVideoMemoryProperty);

    totalSystemMemoryProperty = systemControl->getStringPropertyManager()
            ->addProperty("available system memory");
    propertyGroup->addSubProperty(totalSystemMemoryProperty);

    dumpMemoryContentProperty = systemControl->getClickPropertyManager()
            ->addProperty("dump memory content");
    propertyGroup->addSubProperty(dumpMemoryContentProperty);

    // Place "OpenGL resources" property above all other properties of the
    // system control.
    QList<QtProperty*> propetiesList =
            systemControl->getSystemPropertiesBrowser()->properties();
    foreach (QtProperty *property, propetiesList)
    {
        systemControl->getSystemPropertiesBrowser()->removeProperty(property);
    }
    systemControl->addProperty(propertyGroup);
    foreach (QtProperty *property, propetiesList)
    {
        systemControl->addProperty(property);
    }

    connect(systemControl->getClickPropertyManager(),
            SIGNAL(propertyChanged(QtProperty*)),
            SLOT(dumpMemoryContent(QtProperty*)));
}


MGLResourcesManager::~MGLResourcesManager()
{
    LOG4CPLUS_DEBUG(mlog, "Freeing graphics resources.." << flush);

    // Free memory of the various pools.

    deleteActors();

    LOG4CPLUS_DEBUG(mlog, "\tactor factory pool" << flush);
    foreach(MAbstractActorFactory* factory, actorFactoryPool)
    {
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting actor factory "
                        << factory->getName().toStdString() << flush);
        delete factory;
    }

    LOG4CPLUS_DEBUG(mlog, "\tscene pool" << flush);
    for (int i = 0; i < scenePool.size(); i++)
    {
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting scene "
                        << scenePool[i]->getName().toStdString());
        delete scenePool[i];
    }

    LOG4CPLUS_DEBUG(mlog, "done");
}


void MGLResourcesManager::deleteActors()
{
    LOG4CPLUS_DEBUG(mlog, "\tactor pool" << flush);
    for (int i = 0; i < actorPool.size(); i++)
    {
        LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting actor " << actorPool[i]->getID()
                                                        << " (" << actorPool[i]->getName().toStdString() << ")"
                                                        << flush);
        delete actorPool[i];
        if (actorPool[i] == textManager)
        {
            textManager = nullptr;
        }
    }
    actorPool.clear();
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MGLResourcesManager::initialize(
#ifdef USE_QOPENGLWIDGET
        const QSurfaceFormat &format,
#else
        const QGLFormat &format,
#endif
        QWidget *parent,
#ifdef USE_QOPENGLWIDGET
        QOpenGLWidget *shareWidget)
#else
        QGLWidget *shareWidget)
#endif
{
    // If the instance is already initialized, this method won't do anything.
    if (MGLResourcesManager::instance == 0)
    {
        MGLResourcesManager::instance = new MGLResourcesManager(
#ifdef USE_QOPENGLWIDGET
                format, parent);
#else
                format, parent, shareWidget);
#endif
        MGLResourcesManager::instance->initializeTextManager();
    }
}


#ifdef USE_QOPENGLWIDGET
void MGLResourcesManager::initializeExternal()
{
    if (!isExternalDataInitialized)
    {
        LOG4CPLUS_DEBUG(mlog, "Initialising GLEW..." << flush);
        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            LOG4CPLUS_ERROR(mlog, "\tError: " << glewGetErrorString(err) << flush);
        }

        //TODO: Make size of used video memory configurable.
        uint gpuTotalMemory;
        uint gpuAvailableMemory;
        gpuMemoryInfo_kb(gpuTotalMemory, gpuAvailableMemory);
        videoMemoryLimit_kb = gpuTotalMemory;

        // Initialise currently registered actors.
        LOG4CPLUS_DEBUG(mlog, "Initialising actors.." << flush);
        LOG4CPLUS_DEBUG(mlog, "===================================================="
                << "====================================================");

        for (int i = 0; i < actorPool.size(); i++)
        {
            LOG4CPLUS_DEBUG(mlog, "======== ACTOR #" << i << " ========" << flush);
            if (!actorPool[i]->isInitialized()) actorPool[i]->initialize();
        }

        LOG4CPLUS_DEBUG(mlog, "===================================================="
                << "====================================================");
        LOG4CPLUS_DEBUG(mlog, "Actors are initialised." << flush);

        displayMemoryUsage();

        // Start a time to update the displayed memory information every 2 seconds.
        QTimer *timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(displayMemoryUsage()));
        timer->start(5000);

        // Inform the system manager that the application has been initialized.
        systemControl->setApplicationIsInitialized();
    }
    isExternalDataInitialized = true;
}
#endif

MGLResourcesManager* MGLResourcesManager::getInstance()
{
    if (MGLResourcesManager::instance == 0)
        throw MInitialisationError(
                "MGLResourcesManager has not been initialized.",
                __FILE__, __LINE__);

    return MGLResourcesManager::instance;
}


void MGLResourcesManager::registerScene(MSceneControl *scene)
{
    LOG4CPLUS_DEBUG(mlog, "registering scene "
                    << scene->getName().toStdString());
    scenePool.append(scene);
}


MSceneControl *MGLResourcesManager::getScene(QString name)
{
    for (int i = 0; i < scenePool.size(); i++)
    {
        if (scenePool[i]->getName() == name) { return scenePool[i]; }
    }

    return nullptr;
}


QList<MSceneControl *> &MGLResourcesManager::getScenes()
{
    return scenePool;
}


void MGLResourcesManager::deleteScene(QString name)
{
    for (int i = 0; i < scenePool.size(); i++)
    {
        if (scenePool[i]->getName() == name)
        {
            MSceneControl* scene = scenePool.takeAt(i);
            delete scene;
        }
    }
}


void MGLResourcesManager::registerActor(MActor *actor)
{
    LOG4CPLUS_DEBUG(mlog, "registering actor " << actor->getName().toStdString()
                    << " (ID " << actor->getID()
                    << ") with graphics resources manager");
    actorPool.append(actor);

    emit actorCreated(actor);

    // Listen to actorHasChangedItsName() signals of this actor.
    connect(actor, SIGNAL(actorNameChanged(MActor*, QString)),
            this, SLOT(actorHasChangedItsName(MActor*, QString)));

    // Notify the actor when other actors change.
    connect(this, SIGNAL(actorCreated(MActor*)),
            actor, SLOT(actOnOtherActorCreated(MActor*)));
    connect(this, SIGNAL(actorDeleted(MActor*)),
            actor, SLOT(actOnOtherActorDeleted(MActor*)));
    connect(this, SIGNAL(actorRenamed(MActor*, QString)),
            actor, SLOT(actOnOtherActorRenamed(MActor*, QString)));
}


bool MGLResourcesManager::generateEffectProgram(
        QString name, shared_ptr<GL::MShaderEffect>& effectProgram)
{
    bool newShaderHasBeenCreated = false;

    // Only create a new shader program if the shader pool does not alreay
    // contain a program with the specified name.
    if (!effectPool.contains(name))
    {
        effectPool.insert(
                    name,
                    shared_ptr<GL::MShaderEffect>(new GL::MShaderEffect()));
        newShaderHasBeenCreated = true;
    }

    effectProgram = effectPool.value(name);
    return newShaderHasBeenCreated;
}


bool MGLResourcesManager::generateEffectProgramUncached(
        QString name, std::shared_ptr<GL::MShaderEffect>& effectProgram)
{
    effectProgram = shared_ptr<GL::MShaderEffect>(new GL::MShaderEffect());
    return true;
}


bool MGLResourcesManager::generateTexture(QString name, GLuint *textureObjectName)
{
    bool createdNewTexture = false;

    if (!textureObjectPool.contains(name))
    {
        // If the texture pool does not contain a texture object with the given
        // name, insert a new one.
        GLuint texObj;
        glGenTextures(1, &texObj);
        textureObjectPool.insert(name, texObj);
        createdNewTexture = true;
    }

    // Return the pointer to the shader with the specified name.
    *textureObjectName = textureObjectPool.value(name);

    return createdNewTexture;
}


GL::MTexture* MGLResourcesManager::createTexture( QString  key,
                                                 bool    *existed,
                                                 GLenum   target,
                                                 GLint    internalFormat,
                                                 GLsizei  width,
                                                 GLsizei  height,
                                                 GLsizei  depth )
{
    // Does the requested item exist in pool of active or released textures?
    GL::MTexture* texture = checkTexture(key);
    if ( texture )
    {
        *existed = true;
        return texture;
    }

    // Ok, we need to create a new texture object. First, estimate the size of
    // the new texture to see whether existing released texture objects need to
    // be deleted to stay within the memory usage limit.
    uint approxMemoryUsage = GL::MTexture::approxSizeInBytes(
                internalFormat, width, height, depth) / 1024.;

    // Test if the memory limit will be exceeded by adding the new texture. If
    // so, remove some of the released texture objects.
    while ( (videoMemoryUsage_kb + approxMemoryUsage >= videoMemoryLimit_kb)
            && !releasedTexturesQueue.empty())
    {
        QString removeKey = releasedTexturesQueue.takeFirst();
        GL::MTexture* removeTexture = releasedTexturesPool.take(removeKey);
        videoMemoryUsage_kb -= removeTexture->approxSizeInBytes() / 1024.;
        delete removeTexture;
    }

    // If not enough memory could be freed throw an exception.
    if ( videoMemoryUsage_kb + approxMemoryUsage >= videoMemoryLimit_kb )
        throw MMemoryError("GPU memory is full, not enough textures released",
                           __FILE__, __LINE__);

    *existed = false;
    texture = new GL::MTexture(target, internalFormat, width, height, depth);
    texture->setIDKey(key);
    activeTexturesPool.insert(key, texture);

    // Update memory usage.
    videoMemoryUsage_kb += approxMemoryUsage;
    displayMemoryUsage();

    // Create a reference counter entry for this new texture: one client is
    // using the texture.
    referenceCounter[key] = 1;

    // Get base key of the given texture key (remove the trailing ":?:query").
    QString baseKey = key.split(":?:")[0];

    if ( !baseKeyToTextureKeysDict.contains(baseKey) )
        baseKeyToTextureKeysDict.insert(baseKey, QStringList());

    baseKeyToTextureKeysDict[baseKey].append(key);

    return texture;
}


GL::MTexture* MGLResourcesManager::checkTexture( QString key )
{
    // Requested item exists in pool of active textures? Return it..
    if ( activeTexturesPool.contains(key) )
    {
        // .. and increment reference counter.
        referenceCounter[key] += 1;
        return activeTexturesPool.value(key);
    }

    // Requested item exists in pool of released textures, i.e. is still in
    // video memory? Move it to active items and return.
    if ( releasedTexturesPool.contains(key) )
    {
        GL::MTexture* texture = releasedTexturesPool.take(key);
        releasedTexturesQueue.removeOne(key);
        activeTexturesPool.insert(key, texture);
        referenceCounter[key] += 1;
        return texture;
    }

    // Key is not in video memory, return NULL pointer.
    return 0;
}


void MGLResourcesManager::releaseTexture(GL::MTexture *texture)
{
    // Look up the key corresponding to texture and call the sister method
    // accepting the key as argument. Note that looking up the key is a linear
    // time operation in QHash.
    releaseTexture(texture->getIDKey());
}


void MGLResourcesManager::releaseTexture(QString key)
{
    // Decrement reference counter. If it is zero afterwards, we can safely
    // release the data field -- no actor requires it at the moment.
    referenceCounter[key] -= 1;
    if ( (referenceCounter[key] == 0) && activeTexturesPool.contains(key) )
    {
        releasedTexturesPool.insert(key, activeTexturesPool.take(key));
        releasedTexturesQueue.push_back(key);
    }
}


void MGLResourcesManager::deleteReleasedTexturesOfBaseKey(QString baseKey)
{
    const QStringList& keys = baseKeyToTextureKeysDict.value(baseKey);

    for (int i = 0; i < keys.size(); i++)
    {
        const QString& key = keys.at(i);

        if ( releasedTexturesPool.contains(key) )
        {
            GL::MTexture* removeTexture = releasedTexturesPool.take(key);
            releasedTexturesQueue.removeOne(key);
            videoMemoryUsage_kb -= removeTexture->approxSizeInBytes() / 1024.;
            displayMemoryUsage();
            delete removeTexture;
        }

    }
}


void MGLResourcesManager::deleteActor(const QString name)
{
    LOG4CPLUS_DEBUG(mlog, "deleting OpenGL actor ''" << name.toStdString()
                    << "'' from actor pool" << flush);

    for (int i = 0; i < actorPool.size(); ++i)
    {
        MActor* actor = actorPool.at(i);
        if (actor->getName() == name)
        {
            actorPool.remove(i);

            // Disconnect actor from signals.
            disconnect(actor, SIGNAL(actorNameChanged(MActor*, QString)),
                       this, SLOT(actorHasChangedItsName(MActor*, QString)));

            disconnect(this, SIGNAL(actorCreated(MActor*)),
                       actor, SLOT(actOnOtherActorCreated(MActor*)));
            disconnect(this, SIGNAL(actorDeleted(MActor*)),
                       actor, SLOT(actOnOtherActorDeleted(MActor*)));
            disconnect(this, SIGNAL(actorRenamed(MActor*, QString)),
                       actor, SLOT(actOnOtherActorRenamed(MActor*, QString)));

            // Notify other actors that this actor will be deleted.
            emit actorDeleted(actor);

            delete actor;
            break;
        }
    }
}


void MGLResourcesManager::deleteActor(MActor* actor)
{
    deleteActor(actor->getName());
}


bool MGLResourcesManager::tryStoreGPUItem(GL::MAbstractGPUDataItem *item)
{
    MDataRequest key = item->getRequestKey();

    LOG4CPLUS_DEBUG(mlog, "tryStoreGPUItem() for key " << key.toStdString()
                    << flush);

    // Items that are already stored in the cache cannot be stored again.
    if (activeGPUItems.contains(key)
            || releasedGPUItems.contains(key))
    {
        LOG4CPLUS_DEBUG(mlog, "declined, request key already exists." << flush);
        return false;
    }

    // Test if the system memory limit will be exceeded by adding the new data
    // item. If so, remove some of the released data items.
    unsigned int itemMemoryUsage_kb = item->getGPUMemorySize_kb();
    while ( (videoMemoryUsage_kb + itemMemoryUsage_kb >= videoMemoryLimit_kb)
            && !releasedGPUItemsQueue.empty())
    {
        MDataRequest removeKey = releasedGPUItemsQueue.takeFirst();
        gpuItemsReferenceCounter.take(removeKey);
        GL::MAbstractGPUDataItem* removeItem = releasedGPUItems.take(removeKey);
        videoMemoryUsage_kb -= removeItem->getGPUMemorySize_kb();
        delete removeItem;
    }

    // If not enough memory could be freed throw an exception.
    if ( videoMemoryUsage_kb + itemMemoryUsage_kb >= videoMemoryLimit_kb )
    {
        LOG4CPLUS_ERROR(mlog, "Current GPU memory usage: "
                        << videoMemoryUsage_kb << " kb; new item requires: "
                        << itemMemoryUsage_kb << " kb; GPU memory limit: "
                        << videoMemoryLimit_kb << " kb" << flush);
        throw MMemoryError("GPU memory limit exceeded, cannot release"
                           " any further data items", __FILE__, __LINE__);
    }

    // Memory is fine, so insert the new grid into pool of data items.
    activeGPUItems.insert(key, item);
    // Block item until first call of getGPUItem() (see getData()).
    gpuItemsReferenceCounter.insert(key, -1);
    gpuItemsMemoryUsage_kb.insert(key, itemMemoryUsage_kb);
    videoMemoryUsage_kb += itemMemoryUsage_kb;
    displayMemoryUsage();
    return true;
}


GL::MAbstractGPUDataItem* MGLResourcesManager::getGPUItem(MDataRequest key)
{
    // Requested item exists in pool of active textures? Return it.
    if ( activeGPUItems.contains(key) )
    {
        // The data item is available and currently active. If the reference
        // contour is negative, the item has recently be inserted into the
        // cache by tryStoreGPUItem() and is blocked until this (=first) call
        // to getGPUItem(). Hence, set the reference counter to 1. Otherwise
        // increase the reference counter and return the item.
        if (gpuItemsReferenceCounter[key] < 0)
            gpuItemsReferenceCounter[key] = 1;
        else
            gpuItemsReferenceCounter[key] += 1;
        return activeGPUItems.value(key);
    }

    // Requested item exists in pool of released textures, i.e. is still in
    // video memory? Move it to active items and return.
    if ( releasedGPUItems.contains(key) )
    {
        GL::MAbstractGPUDataItem* item = releasedGPUItems.take(key);
        releasedGPUItemsQueue.removeOne(key);
        activeGPUItems.insert(key, item);
        gpuItemsReferenceCounter[key] += 1;
        return item;
    }

    // Key is not in video memory, return NULL pointer.
    return nullptr;
}


void MGLResourcesManager::releaseGPUItem(GL::MAbstractGPUDataItem *item)
{
    releaseGPUItem(item->getRequestKey());
}


void MGLResourcesManager::releaseGPUItem(Met3D::MDataRequest key)
{
    if ( !gpuItemsReferenceCounter.contains(key) ) return;

    // Decrement reference counter. If it is zero afterwards, we can safely
    // release the data field -- no actor requires it at the moment.
    gpuItemsReferenceCounter[key] -= 1;
    if ( (gpuItemsReferenceCounter[key] == 0) && activeGPUItems.contains(key) )
    {
        releasedGPUItems.insert(key, activeGPUItems.take(key));
        releasedGPUItemsQueue.push_back(key);
    }
}


void MGLResourcesManager::deleteReleasedGPUItem(GL::MAbstractGPUDataItem *item)
{
    deleteReleasedGPUItem(item->getRequestKey());
}


void MGLResourcesManager::deleteReleasedGPUItem(MDataRequest removeKey)
{
    bool released = releasedGPUItemsQueue.removeOne(removeKey);
    if (!released)
    {
        LOG4CPLUS_DEBUG(
                mlog, "MGLResourcesManager::deleteReleasedGPUItem failed - no released "
                      << "item with the specified key was found." << flush);
        return;
    }
    gpuItemsReferenceCounter.take(removeKey);
    GL::MAbstractGPUDataItem* removeItem = releasedGPUItems.take(removeKey);
    videoMemoryUsage_kb -= removeItem->getGPUMemorySize_kb();
    delete removeItem;
}


void MGLResourcesManager::releaseAllGPUItemReferences(MDataRequest key)
{
    if (gpuItemsReferenceCounter.empty()
            || !gpuItemsReferenceCounter.contains(key)) return;

    if (gpuItemsReferenceCounter[key] > 1) gpuItemsReferenceCounter[key] = 1;
    releaseGPUItem(key);
}


void MGLResourcesManager::updateGPUItemSize(GL::MAbstractGPUDataItem *item)
{
    Met3D::MDataRequest key = item->getRequestKey();
    if ( !gpuItemsReferenceCounter.contains(key) ) return;

    unsigned int itemMemoryUsage_kb = item->getGPUMemorySize_kb();
    unsigned int oldMemoryUsage_kb = gpuItemsMemoryUsage_kb[key];
    gpuItemsMemoryUsage_kb[key] = itemMemoryUsage_kb;
    videoMemoryUsage_kb += (itemMemoryUsage_kb - oldMemoryUsage_kb);
}


bool MGLResourcesManager::isManagedGPUItem(GL::MAbstractGPUDataItem *item)
{
    Met3D::MDataRequest key = item->getRequestKey();
    return gpuItemsReferenceCounter.contains(key);
}


void MGLResourcesManager::reloadActorShaders()
{
    LOG4CPLUS_DEBUG(mlog, "Reloading actor shaders..." << flush);
    for (int i = 0; i < actorPool.size(); i++)
        if (actorPool[i]->isInitialized()) actorPool[i]->reloadShaderEffects();
}


MActor* MGLResourcesManager::getActorByName(const QString& name) const
{
    for (const auto& actor : actorPool)
    {
        if (actor->getName() == name) return actor;
    }

    return nullptr;
}


void MGLResourcesManager::registerActorFactory(MAbstractActorFactory *factory)
{
    // Only one factory per key (= name).
    if (actorFactoryPool.contains(factory->getName())) return;

    // Initialize factory and add to pool.
    factory->initialize();
    actorFactoryPool.insert(factory->getName(), factory);

    LOG4CPLUS_DEBUG(mlog, "registered actor factory <"
                    << factory->getName().toStdString()
                    << "> with graphics resources manager");
}


void MGLResourcesManager::deleteActorFactory(const QString& name)
{
    // If no key with given name exists return.
    if (!actorFactoryPool.contains(name)) return;

    // Erase instantitation method with given key "name".
    MAbstractActorFactory *factory = actorFactoryPool.take(name);
    delete factory;
}


const QList<QString> MGLResourcesManager::getActorFactoryNames() const
{
    return actorFactoryPool.keys();
}


MAbstractActorFactory* MGLResourcesManager::getActorFactory(const QString &name)
{
    if (actorFactoryPool.contains(name)) return actorFactoryPool[name];

    return nullptr;
}


QList<MAbstractActorFactory*> MGLResourcesManager::getActorFactories()
{
    return actorFactoryPool.values();
}


QList<MActor*> MGLResourcesManager::getActorsConnectedTo(MActor *actor)
{
    QList<MActor*> connectedActors;

    foreach (MActor* a, actorPool)
    {
        if (a->isConnectedTo(actor))
        {
            connectedActors << a;
        }
    }

    return connectedActors;
}


QList<MActor*> MGLResourcesManager::getActorsConnectedToBBox(QString bBoxName)
{
    QList<MActor*> connectedActors;

    foreach (MActor* actor, actorPool)
    {
        // Only actors inheriting from MBoundingBoxInterface can be connected to
        // a bounding box.
        if (MBoundingBoxInterface *bBoxActor =
                dynamic_cast<MBoundingBoxInterface*>(actor))
        {
            if (bBoxActor->getBoundingBoxName() == bBoxName)
            {
                connectedActors << actor;
            }
        }
    }

    return connectedActors;
}


void MGLResourcesManager::gpuMemoryInfo_kb(uint& total, uint& available)
{
    // Get CPU memory usage via NVIDIA extension.

    // See http://www.geeks3d.com/20100531/programming-tips-how-to-know-the-graphics-memory-size-and-usage-in-opengl/
    // and http://developer.download.nvidia.com/opengl/specs/GL_NVX_gpu_memory_info.txt

    GLint totalGPUMemory = 0;
    GLint currentlyAvailableGPUMemory = 0;

    glGetIntegerv(GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX,
                  &totalGPUMemory);
    glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX,
                  &currentlyAvailableGPUMemory);

    total = totalGPUMemory;
    available = currentlyAvailableGPUMemory;
}


void MGLResourcesManager::systemMemoryInfo_kb(uint& total, uint& available)
{
    // Get system memory information on Linux systems by opening /proc/meminfo
    // and parsing the "MemFree: x kB" and "MemTotal:" lines.

    total = 0;      // in kB
    available = 0;

    FILE* meminfo = fopen("/proc/meminfo", "rt");
    // Silently ignore ary errors if the file cannot be found.
    if(!meminfo) return;

#define LINELENGTH 64
    char cLine[LINELENGTH];
    char cMemFreeText[] = "MemFree:";
    char cMemTotalText[] = "MemTotal:";

    while(fgets(cLine, LINELENGTH, meminfo) != 0) {
        if(strncmp(cMemFreeText, cLine, strlen(cMemFreeText)) == 0) {
            if(sscanf(cLine, "%*s %u kB", &available) != 1)
            {
                LOG4CPLUS_ERROR(mlog, "Error parsing \"MemFree\" line in "
                                "/proc/meminfo");
            }
        }
        if(strncmp(cMemTotalText, cLine, strlen(cMemTotalText)) == 0) {
            if(sscanf(cLine, "%*s %u kB", &total) != 1) {
                LOG4CPLUS_ERROR(mlog, "Error parsing \"MemTotal\" line in "
                                "/proc/meminfo");
            }
        }
        if ((total > 0) && (available > 0)) break;
    }

    fclose(meminfo);
}


void MGLResourcesManager::initializeTextManager()
{
    textManager = new MTextManager();

    // Prevent the text manager actor to be deleted by the user at runtime.
    textManager->setActorIsUserDeletable(false);

    registerActor(textManager);
}


MTextManager* MGLResourcesManager::getTextManager()
{
    return textManager;
}


MLabel* MGLResourcesManager::getSceneRotationCentreSelectionLabel()
{
    // Initialize an MLabel for the interactive camera selection.
    if (selectSceneCentreText == nullptr)
    {
        MTextManager* tm = getTextManager();
        selectSceneCentreText = tm->addText(
                    "Drag the pole to the rotation centre you would like to "
                    "use, then hit enter.",
                     MTextManager::CLIPSPACE,-0.5,0.9,0,16,
                     QColor(0,0,0),MTextManager::BASELINELEFT,true);
    }

    return selectSceneCentreText;
}


MMovablePoleActor* MGLResourcesManager::getSceneRotationCentreSelectionPoleActor()
{
    // Initialize an MMovablePoleActor for the interactive camera selection.
    if (selectSceneCentreActor == nullptr)
    {
        MAbstractActorFactory *factory = getActorFactory("Movable poles");
        selectSceneCentreActor =
                dynamic_cast<MMovablePoleActor*>(factory->create());
        selectSceneCentreActor->setName("SelectSceneRotationCentreActor");
        selectSceneCentreActor->setEnabled(true);
        selectSceneCentreActor->initialize();
        registerActor(selectSceneCentreActor);
        selectSceneCentreActor->addPole(QPointF(0,0));
    }
    return selectSceneCentreActor;
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MGLResourcesManager::displayMemoryUsage()
{
    uint total = 0;
    uint available = 0;

    gpuMemoryInfo_kb(total, available);
    systemControl->getStringPropertyManager()->setValue(
                totalVideoMemoryProperty,
                QString("%1 / %2 MiB").arg(available/1024).arg(total/1024));

    systemControl->getStringPropertyManager()->setValue(
                met3DVideoMemoryProperty,
                QString("%1 / %2 MiB")
                .arg(videoMemoryUsage_kb/1024).arg(videoMemoryLimit_kb/1024));

    systemMemoryInfo_kb(total, available);
    systemControl->getStringPropertyManager()->setValue(
                totalSystemMemoryProperty,
                QString("%1 / %2 MiB").arg(available/1024).arg(total/1024));
}


void MGLResourcesManager::dumpMemoryContent(QtProperty *property)
{
    if (property != dumpMemoryContentProperty) return;

    QString s = "\n\nOPENGL MEMORY CACHE CONTENT\n"
            "===========================\n"
            "Active items:\n";

    QHashIterator<Met3D::MDataRequest,
            GL::MAbstractGPUDataItem*> iter(activeGPUItems);
    while (iter.hasNext()) {
        iter.next();
        s += QString("REQUEST: %1, SIZE: %2 kb, REFERENCES: %3\n")
                .arg(iter.key())
                .arg(iter.value()->getGPUMemorySize_kb())
                .arg(gpuItemsReferenceCounter[iter.key()]);
    }

    s += "\nReleased items (in queued order):\n";

    for (int i = 0; i < releasedGPUItemsQueue.size(); i++)
    {
        Met3D::MDataRequest r = releasedGPUItemsQueue[i];
        s += QString("REQUEST: %1, SIZE: %2 kb, REFERENCES: %3\n")
                .arg(r)
                .arg(releasedGPUItems[r]->getGPUMemorySize_kb())
                .arg(gpuItemsReferenceCounter[r]);
    }

    s += "\n\n===========================\n";

    LOG4CPLUS_INFO(mlog, s.toStdString());
}


void MGLResourcesManager::actorHasChangedItsName(MActor *actor, QString oldName)
{
    // Simply pass on the received signal to all registered actors.
    emit actorRenamed(actor, oldName);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

#ifdef USE_QOPENGLWIDGET
bool MGLResourcesManager::isExternalDataInitialized = false;
#endif

void MGLResourcesManager::initializeGL()
{
    cout << endl << endl;
    LOG4CPLUS_DEBUG(mlog, "*** MGLResourcesManager: Initialising OpenGL "
                    << "context. ***" << flush);

    // Print information on the OpenGL context and version.
    LOG4CPLUS_DEBUG(mlog, "*** OpenGL information:");
    LOG4CPLUS_DEBUG(mlog, "OpenGL context is "
                    << (context()->isValid() ? "" : "NOT ") << "valid.");
#ifdef USE_QOPENGLWIDGET
    LOG4CPLUS_DEBUG(mlog, "\tRequested version: "
                    << requestedFormat.majorVersion()
                    << "." << requestedFormat.minorVersion());
#else
    LOG4CPLUS_DEBUG(mlog, "\tRequested version: "
            << context()->requestedFormat().majorVersion()
            << "." << context()->requestedFormat().minorVersion());
#endif
    LOG4CPLUS_DEBUG(mlog, "\tObtained version: "
                    << context()->format().majorVersion() << "."
                    << context()->format().minorVersion());
    LOG4CPLUS_DEBUG(mlog, "\tObtained profile: "
                    << context()->format().profile());
#ifdef USE_QOPENGLWIDGET
    LOG4CPLUS_DEBUG(mlog, "\tShaders are "
                    << (QOpenGLShaderProgram::hasOpenGLShaderPrograms(
                            context()) ? "" : "NOT ") << "supported.");
#else
    LOG4CPLUS_DEBUG(mlog, "\tShaders are "
            << (QGLShaderProgram::hasOpenGLShaderPrograms(
                    context()) ? "" : "NOT ") << "supported.");
#endif
#ifdef USE_QOPENGLWIDGET
    LOG4CPLUS_DEBUG(mlog, "\tMultisampling is "
                    << (context()->format().samples() > 0 ? "" : "NOT ")
                    << "enabled." << flush);
#else
    LOG4CPLUS_DEBUG(mlog, "\tMultisampling is "
            << (context()->format().sampleBuffers() ? "" : "NOT ")
            << "enabled." << flush);
#endif
    LOG4CPLUS_DEBUG(mlog, "\tNumber of samples per pixel: "
                    << context()->format().samples() << flush);

#ifdef USE_QOPENGLWIDGET
    initializeExternal();
#else
    LOG4CPLUS_DEBUG(mlog, "Initialising GLEW..." << flush);
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        LOG4CPLUS_ERROR(mlog, "\tError: " << glewGetErrorString(err) << flush);
    }
#endif

    LOG4CPLUS_DEBUG(mlog, "Using GLEW " << glewGetString(GLEW_VERSION));
    LOG4CPLUS_DEBUG(mlog, "OpenGL version supported by this platform "
                    << "(glGetString): " << glGetString(GL_VERSION));

    LOG4CPLUS_DEBUG(mlog, "Core extensions of OpenGL 4.0 are supported: "
                    << (GLEW_VERSION_4_0 ? "Yes" : "No"));
    LOG4CPLUS_DEBUG(mlog, "Core extensions of OpenGL 4.1 are supported: "
                    << (GLEW_VERSION_4_1 ? "Yes" : "No"));
    LOG4CPLUS_DEBUG(mlog, "Core extensions of OpenGL 4.2 are supported: "
                    << (GLEW_VERSION_4_2 ? "Yes" : "No"));
    LOG4CPLUS_DEBUG(mlog, "Core extensions of OpenGL 4.3 are supported: "
                    << (GLEW_VERSION_4_3 ? "Yes" : "No"));

    propertyGroup->setPropertyName(QString("OpenGL resources (%1)")
                                   .arg((char*) glGetString(GL_VERSION)));

    QString value = "";
#ifdef USE_QOPENGLWIDGET
    QPair<int, int> flags = context()->format().version();
    QString formatString;
    if (context()->format().renderableType() == QSurfaceFormat::OpenGLES) {
        formatString = QString("OpenGL ES %1.%2");
    } else {
        formatString = QString("OpenGL %1.%2");
    }
    value += formatString.arg(flags.first).arg(flags.second);
    LOG4CPLUS_DEBUG(mlog, "QSurfaceFormat::version() returns minimum "
            << "supported version is " << value.toStdString());
#else
    if (QGLFormat::hasOpenGL())
    {
        int flags = QGLFormat::openGLVersionFlags();
        if (flags & QGLFormat::OpenGL_Version_4_0) value += "OpenGL 4.0";
        else if (flags & QGLFormat::OpenGL_Version_3_3) value += "OpenGL 3.3";
        else if (flags & QGLFormat::OpenGL_Version_3_2) value += "OpenGL 3.2";
        else if (flags & QGLFormat::OpenGL_Version_3_1) value += "OpenGL 3.1";
        else if (flags & QGLFormat::OpenGL_Version_3_0) value += "OpenGL 3.0";
        else if (flags & QGLFormat::OpenGL_Version_2_1) value += "OpenGL 2.1";
        else if (flags & QGLFormat::OpenGL_Version_2_0) value += "OpenGL 2.0";
        else if (flags & QGLFormat::OpenGL_Version_1_5) value += "OpenGL 1.5";
        else if (flags & QGLFormat::OpenGL_Version_1_4) value += "OpenGL 1.4";
        else if (flags & QGLFormat::OpenGL_Version_1_3) value += "OpenGL 1.3";
        else if (flags & QGLFormat::OpenGL_Version_1_2) value += "OpenGL 1.2";
        else if (flags & QGLFormat::OpenGL_Version_1_1) value += "OpenGL 1.1";
        if (flags & QGLFormat::OpenGL_ES_CommonLite_Version_1_0) value += " OpenGL ES 1.0 Common Lite";
        else if (flags & QGLFormat::OpenGL_ES_Common_Version_1_0) value += " OpenGL ES 1.0 Common";
        else if (flags & QGLFormat::OpenGL_ES_CommonLite_Version_1_1) value += " OpenGL ES 1.1 Common Lite";
        else if (flags & QGLFormat::OpenGL_ES_Common_Version_1_1) value += " OpenGL ES 1.1 Common";
        else if (flags & QGLFormat::OpenGL_ES_Version_2_0) value += " OpenGL ES 2.0";
    }
    else
    {
        value = "None";
    }
    LOG4CPLUS_DEBUG(mlog, "QGLFormat::openGLVersionFlags() returns minimum "
            << "supported version is " << value.toStdString());
#endif

    LOG4CPLUS_DEBUG(mlog, "*** END OpenGL information.\n");

#ifndef USE_QOPENGLWIDGET
    //TODO: Make size of used video memory configurable.
    uint gpuTotalMemory;
    uint gpuAvailableMemory;
    gpuMemoryInfo_kb(gpuTotalMemory, gpuAvailableMemory);
    videoMemoryLimit_kb = gpuTotalMemory;
#endif

    LOG4CPLUS_DEBUG(mlog, "Maximum GPU video memory to be used: "
                    << videoMemoryLimit_kb / 1024. << " MB." << flush);

#ifndef USE_QOPENGLWIDGET
    // Initialise currently registered actors.
    LOG4CPLUS_DEBUG(mlog, "Initialising actors.." << flush);
    LOG4CPLUS_DEBUG(mlog, "===================================================="
            << "====================================================");

    for (int i = 0; i < actorPool.size(); i++)
    {
        LOG4CPLUS_DEBUG(mlog, "======== ACTOR #" << i << " ========" << flush);
        if (!actorPool[i]->isInitialized()) actorPool[i]->initialize();
    }

    LOG4CPLUS_DEBUG(mlog, "===================================================="
            << "====================================================");
    LOG4CPLUS_DEBUG(mlog, "Actors are initialised." << flush);

    displayMemoryUsage();

    // Start a time to update the displayed memory information every 2 seconds.
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(displayMemoryUsage()));
    timer->start(5000);
#endif

    // Prevent this widget from being shown. It is meant as a hidden widget
    // that manages the OpenGL resources, not for displaying anything.
    setVisible(false);

    LOG4CPLUS_DEBUG(mlog, "GL resources manager initialisation done\n*****\n"
                    << flush);

#ifndef USE_QOPENGLWIDGET
    // Inform the system manager that the application has been initialized.
    systemControl->setApplicationIsInitialized();
#endif
}

} // namespace Met3D
