/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2016-2018 Bianca Tost
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
#include "mactor.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msystemcontrol.h"
#include "mainwindow.h"
#include "gxfw/mscenecontrol.h"
#include "util/mutil.h"
#include "gxfw/gl/typedvertexbuffer.h"

using namespace std;

namespace Met3D
{

// Static counter for all actors that derive from MActor.
unsigned int MActor::idCounter = 0;


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MActor::MActor(QObject *parent)
    : QObject(parent),
      actorIsEnabled(true),
      labelsAreEnabled(true),
      renderAsWireFrame(false),
      actorIsPickable(false), // by default actors are not pickable
      shaderCompilationProgressDialog(nullptr),
      shaderCompilationProgress(0),
      positionLabel(nullptr),
      actorName("Default actor"),
      actorType(staticActorType()),
      addPropertiesCounter(0),
      actorIsInitialized(false),
      actorChangedSignalDisabledCounter(0),
      actorUpdatesDisabledCounter(0),
      actorIsUserDeletable(true),
      actorSupportsFullScreenVisualisation(false),
      actorSupportsMultipleEnsembleMemberVisualization(false)
{
    // Get a pointer to the property managers used for the GUI properties.
    properties = MSceneControl::getQtProperties();

    // Obtain a "personal" identification number from the static ID counter.
    myID = MActor::idCounter++;

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    propertyGroup = addProperty(GROUP_PROPERTY, actorName);

    actorEnabledProperty = addProperty(BOOL_PROPERTY, "enabled",
                                       propertyGroup);
    properties->mBool()->setValue(actorEnabledProperty, actorIsEnabled);

    // Actor configuration properties.
    actorConfigurationSupGroup = addProperty(GROUP_PROPERTY, "configuration",
                                             propertyGroup);

    loadConfigProperty = addProperty(CLICK_PROPERTY, "load",
                                     actorConfigurationSupGroup);

    saveConfigProperty = addProperty(CLICK_PROPERTY, "save",
                                     actorConfigurationSupGroup);

    // Rendering properties.
    actorRenderingSupGroup = addProperty(GROUP_PROPERTY, "rendering",
                                          propertyGroup);

    wireFrameProperty = addProperty(BOOL_PROPERTY, "wire frame",
                                    actorRenderingSupGroup);
    properties->mBool()->setValue(wireFrameProperty, renderAsWireFrame);

    reloadShaderProperty = addProperty(CLICK_PROPERTY, "reload shaders",
                                       actorRenderingSupGroup);

    // Actor properties.
    actorPropertiesSupGroup = addProperty(GROUP_PROPERTY, "actor properties",
                                          propertyGroup);


    // Label properties.
    labelPropertiesSupGroup = addProperty(GROUP_PROPERTY, "labels",
                                          actorPropertiesSupGroup);

    labelsEnabledProperty = addProperty(BOOL_PROPERTY, "enabled",
                                        labelPropertiesSupGroup);
    properties->mBool()->setValue(labelsEnabledProperty, labelsAreEnabled);

    labelSizeProperty = addProperty(INT_PROPERTY, "font size",
                                    labelPropertiesSupGroup);
    properties->mInt()->setValue(labelSizeProperty, 16);

    labelColourProperty = addProperty(COLOR_PROPERTY, "font colour",
                                      labelPropertiesSupGroup);
    properties->mColor()->setValue(labelColourProperty, QColor(0, 0, 100));

    labelBBoxProperty = addProperty(BOOL_PROPERTY, "bounding box",
                                    labelPropertiesSupGroup);
    properties->mBool()->setValue(labelBBoxProperty, true);

    labelBBoxColourProperty = addProperty(COLOR_PROPERTY, "bbox colour",
                                          labelPropertiesSupGroup);
    properties->mColor()->setValue(labelBBoxColourProperty, QColor(255, 255, 255, 200));

    endInitialiseQtProperties();

    // Register the actor with the specified resources manager. Note that the
    // resources manager will take care of deleting the actor on program exit.
//    MGLResourcesManager::getInstance()->registerActor(this);
    // NOTE: When registerActor() is called from here, MGLResourcesManager gets
    // only the type information of MActor, not of the derived class (whose
    // constructor has not been executed at this point). Hence, for methods
    // reacting to the actorCreated() signal, there is no way for the actors to
    // determine the correct actor type. The only solution I see at the moment
    // is to call registerActor() externally...
}


MActor::~MActor()
{
    delete shaderCompilationProgressDialog;
    if (positionLabel != nullptr)
    {
        delete positionLabel;
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MActor::initialize()
{
    LOG4CPLUS_DEBUG(mlog, "Initialising actor ["
                    << getSettingsID().toStdString() << "] ...");
    if (actorIsInitialized)
    {
        LOG4CPLUS_DEBUG(mlog, "\tactor has already been initialised, skipping.");
        return;
    }

    // Determine number of available texture/image units. Required for
    // assignTextureUnit() and assignImageUnit().
    GLint numUnits = 0;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB, &numUnits);
    for (GLint u = 0; u < numUnits; u++)
    {
        availableTextureUnits << u;
        availableImageUnits << u;
    }

    initializeActorResources();

    actorIsInitialized = true;
    LOG4CPLUS_DEBUG(mlog, "... finished initialisation of actor ["
                    << getSettingsID().toStdString() << "].");
}


void MActor::render(MSceneViewGLWidget *sceneView)
{
    if (!actorIsEnabled) return;
    renderToCurrentContext(sceneView);
}


void MActor::renderToFullScreen(MSceneViewGLWidget *sceneView)
{
    if (!actorIsEnabled) return;
    renderToCurrentFullScreenContext(sceneView);
}


bool MActor::isInitialized()
{
    return actorIsInitialized;
}


bool MActor::isEnabled()
{
    return actorIsEnabled;
}


unsigned int MActor::getID()
{
    return myID;
}


void MActor::setName(const QString name)
{
    QString oldName = actorName;
    actorName = name;
    propertyGroup->setPropertyName(actorName);

    emit actorNameChanged(this, oldName);
}


QString MActor::getName()
{
    return actorName;
}


void MActor::registerScene(MSceneControl *scene)
{
    if (!scenes.contains(scene))
    {
        scenes.append(scene);
    }
}

void MActor::deregisterScene(MSceneControl *scene)
{
    for (int i = 0; i < scenes.size(); ++i)
    {
        if (scene == scenes[i])
        {
            scenes.removeAt(i);
            break;
        }
    }
}


const QList<MSceneViewGLWidget*> MActor::getViews()
{
    QSet<MSceneViewGLWidget*> views;
    QListIterator<MSceneControl*> sceneIterator(scenes);
    while (sceneIterator.hasNext())
        views.unite(sceneIterator.next()->getRegisteredSceneViews());
    return views.toList();
}


QList<MLabel*> MActor::getLabelsToRender()
{
    // If either labels or the actor itself are disabled return an empty list.
    if (actorIsEnabled && labelsAreEnabled)
    {
        return labels;
    }
    else
    {
        return QList<MLabel*>();
    }
}


QList<MLabel*> MActor::getPositionLabelToRender()
{
    QList<MLabel*> labelList = QList<MLabel*>();
    // Add position label to empty list if the lable is present and the actor
    // and the lable are enabled.
    if (positionLabel != nullptr && actorIsEnabled)
    {
        labelList.append(positionLabel);
    }
    return labelList;
}


void MActor::removePositionLabel()
{
    if (positionLabel != nullptr)
    {
        MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
        MTextManager* tm = glRM->getTextManager();
        tm->removeText(positionLabel);
        positionLabel = nullptr;
    }
    emitActorChangedSignal();
}


void MActor::setEnabled(bool enabled)
{
    properties->mBool()->setValue(actorEnabledProperty, enabled);
}


void MActor::setLabelsEnabled(bool enabled)
{
    properties->mBool()->setValue(labelsEnabledProperty, enabled);
}


void MActor::saveConfigurationToFile(QString filename)
{
    if (filename.isEmpty())
    {
        QString directory =
                MSystemManagerAndControl::getInstance()
                ->getMet3DWorkingDirectory().absoluteFilePath("config/actors");
        QDir().mkpath(directory);
        filename = QFileDialog::getSaveFileName(
                    MGLResourcesManager::getInstance(),
                    "Save actor configuration",
                    QDir(directory).absoluteFilePath(getName() + ".actor.conf"),
                    "Actor configuration files (*.actor.conf)");

        if (filename.isEmpty())
        {
            return;
        }
    }

    LOG4CPLUS_DEBUG(mlog, "Saving configuration to " << filename.toStdString());

    // Overwrite if the file exists.
    if (QFile::exists(filename))
    {
        QSettings* settings = new QSettings(filename, QSettings::IniFormat);
        QStringList groups = settings->childGroups();
        // Only overwrite file if it contains already configuration for the
        // actor to save.
        if ( !groups.contains(getSettingsID()) )
        {
            QMessageBox msg;
            msg.setWindowTitle("Error");
            msg.setText(QString("The selected file contains a configuration"
                                " other than %1.\n"
                                "This file will NOT be overwritten -- have you"
                                " selected the correct file?")
                        .arg(getSettingsID()));
            msg.setIcon(QMessageBox::Warning);
            msg.exec();
            delete settings;
            return;
        }

        QFile::remove(filename);
    }
    QSettings* settings = new QSettings(filename, QSettings::IniFormat);

    settings->beginGroup("FileFormat");
    // Save version id of Met.3D.
    settings->setValue("met3dVersion", met3dVersionString);
    settings->endGroup();

    // Save actor settings.
    saveActorConfiguration(settings);

    delete settings;

    LOG4CPLUS_DEBUG(mlog, "... configuration has been saved.");
}


void MActor::loadConfigurationFromFile(QString filename)
{
    if (filename.isEmpty())
    {
        filename = QFileDialog::getOpenFileName(
                    MGLResourcesManager::getInstance(),
                    "Load actor configuration",
                    MSystemManagerAndControl::getInstance()
                    ->getMet3DWorkingDirectory().absoluteFilePath("config/actors"),
                    "Actor configuration files (*.actor.conf)");

        if (filename.isEmpty())
        {
            return;
        }
    }

    LOG4CPLUS_DEBUG(mlog, "Loading configuration from "
                    << filename.toStdString());

    // While settings are loaded actor updates are disabled.
    enableActorUpdates(false);

    QSettings* settings = new QSettings(filename, QSettings::IniFormat);

    QStringList groups = settings->childGroups();
    if ( !groups.contains(getSettingsID()) )
    {
        QMessageBox msg;
        msg.setWindowTitle("Error");
        msg.setText("The selected file does not contain configuration data "
                    "for this actor.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        delete settings;
        return;
    }

    // Load actor settings.
    loadActorConfiguration(settings);

    delete settings;

    // Re-enable actor updates.
    enableActorUpdates(true);

    LOG4CPLUS_DEBUG(mlog, "... configuration has been loaded.");

    // Signal that the actor properties have changed.
    emitActorChangedSignal();
}


void MActor::saveActorConfiguration(QSettings *settings)
{
    settings->beginGroup(MActor::getSettingsID());

    settings->setValue("actorName", actorName);
    settings->setValue("actorIsEnabled", actorIsEnabled);
    settings->setValue("labelsAreEnabled", labelsAreEnabled);
    settings->setValue("renderAsWireFrame", renderAsWireFrame);
    settings->setValue("labelColour", properties->mColor()
                       ->value(labelColourProperty));
    settings->setValue("labelSize", properties->mInt()
                       ->value(labelSizeProperty));
    settings->setValue("labelBBox", properties->mBool()
                       ->value(labelBBoxProperty));
    settings->setValue("labelBBoxColour", properties->mColor()
                       ->value(labelBBoxColourProperty));

    settings->endGroup();

    // Save derived classes settings.
    saveConfiguration(settings);
}


void MActor::loadActorConfiguration(QSettings *settings)
{
    settings->beginGroup(MActor::getSettingsID());

    const QString name = settings->value("actorName").toString();
    setName(name);

    properties->mBool()->setValue(
                actorEnabledProperty,
                settings->value("actorIsEnabled", true).toBool());

    properties->mBool()->setValue(
                labelsEnabledProperty,
                settings->value("labelsAreEnabled", true).toBool());

    properties->mBool()->setValue(
                wireFrameProperty,
                settings->value("renderAsWireFrame", false).toBool());

    properties->mColor()->setValue(
                labelColourProperty,
                settings->value("labelColour", QColor(0, 0, 100)).value<QColor>());

    properties->mInt()->setValue(
                labelSizeProperty,
                settings->value("labelSize", 16).toInt());

    properties->mBool()->setValue(
                labelBBoxProperty,
                settings->value("labelBBox", true).toBool());

    properties->mColor()->setValue(
                labelBBoxColourProperty,
                settings->value("labelBBoxColour",
                                QColor(255, 255, 255, 200)).value<QColor>());

    settings->endGroup();

    // Load derived classes settings.
    loadConfiguration(settings);
}


MQtProperties *MActor::getQtProperties()
{
    return properties;
}


void MActor::beginInitialiseQtProperties()
{
    addPropertiesCounter++;
}


void MActor::endInitialiseQtProperties()
{
    addPropertiesCounter--;
    if (addPropertiesCounter < 0) addPropertiesCounter = 0;
}


QtProperty* MActor::addProperty(
        MQtPropertyType type, const QString& name, QtProperty *group)
{
    QtAbstractPropertyManager *manager = nullptr;

    switch (type)
    {
    case (GROUP_PROPERTY):
        manager = properties->mGroup();
        break;
    case (BOOL_PROPERTY):
        manager = properties->mBool();
        break;
    case (INT_PROPERTY):
        manager = properties->mInt();
        break;
    case (DOUBLE_PROPERTY):
        manager = properties->mDouble();
        break;
    case (DECORATEDDOUBLE_PROPERTY):
        manager = properties->mDecoratedDouble();
        break;
    case (SCIENTIFICDOUBLE_PROPERTY):
        manager = properties->mScientificDouble();
        break;
    case (DATETIME_PROPERTY):
        manager = properties->mDateTime();
        break;
    case (ENUM_PROPERTY):
        manager = properties->mEnum();
        break;
    case (RECTF_LONLAT_PROPERTY):
        manager = properties->mRectF();
        break;
    case (RECTF_CLIP_PROPERTY):
        manager = properties->mRectF();
        break;
    case (POINTF_PROPERTY):
        manager = properties->mPointF();
        break;
    case (POINTF_LONLAT_PROPERTY):
        manager = properties->mPointF();
        break;
    case (COLOR_PROPERTY):
        manager = properties->mColor();
        break;
    case (STRING_PROPERTY):
        manager = properties->mString();
        break;
    case (CLICK_PROPERTY):
        manager = properties->mClick();
        break;
    default:
        break;
    }

    if (manager == nullptr) return nullptr;

    QtProperty *p = manager->addProperty(name);

    if (!connectedPropertyManagers.contains(manager))
    {
        // The added property has a manager of a type that has not been used
        // for any other property of this actor before. Connect to the signals
        // of this new manager.
        connectedPropertyManagers.insert(manager);
        listenToPropertyManager(manager);
    }

    // If a property group has been specified place the new property into
    // this group.
    if (group != nullptr) group->addSubProperty(p);

    // Change names of subProperties for RECTF_LONLAT_PROPERTY properties.
    if (type == RECTF_LONLAT_PROPERTY)
    {
        p->subProperties().at(0)->setPropertyName("western longitude");
        p->subProperties().at(1)->setPropertyName("southern latitude");
        p->subProperties().at(2)->setPropertyName("east-west extend");
        p->subProperties().at(3)->setPropertyName("north-south extend");
    }

    // Change names of subProperties for POINTF_LONLAT_PROPERTY properties.
    if (type == POINTF_LONLAT_PROPERTY)
    {
        p->subProperties().at(0)->setPropertyName("lon");
        p->subProperties().at(1)->setPropertyName("lat");
    }

    return p;
}


void MActor::collapseActorPropertyTree()
{
    for (auto it = scenes.begin(); it != scenes.end(); ++it)
        (*it)->collapseActorPropertyTree(this);
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MActor::actOnQtPropertyChanged(QtProperty *property)
{
    // See beginInitialiseQtProperties() / endInitialiseQtProperties(): If the
    // number of currently active "add properties" sections is larger zero
    // ignore all property change signals (they are emitted when properties are
    // creates and intialised, but usually lead to errors when considered
    // during the property intialisation phase).
    if (addPropertiesCounter > 0) return;

    if (property == actorEnabledProperty)
    {
        actorIsEnabled = properties->mBool()->value(actorEnabledProperty);

        // emitActorChangedSignal() cannot be used here as no signal would
        // be emitted due to the disabled actor.
        if ((actorChangedSignalDisabledCounter == 0) && isInitialized())
        {
            emit actorChanged();
        }
    }

    else if (property == labelsEnabledProperty)
    {
        labelsAreEnabled = properties->mBool()->value(labelsEnabledProperty);
        emitActorChangedSignal();
    }

    else if (property == wireFrameProperty)
    {
        renderAsWireFrame = properties->mBool()->value(wireFrameProperty);
        emitActorChangedSignal();
    }

    else if (property == loadConfigProperty)
    {
        loadConfigurationFromFile();
    }

    else if (property == saveConfigProperty)
    {
        saveConfigurationToFile();
    }

    else if (property == reloadShaderProperty)
    {
        LOG4CPLUS_DEBUG(mlog, "reloading actor shaders...");
        if (isInitialized())
        {
            reloadShaderEffects();
            emitActorChangedSignal();
        }
    }

    // Invoke signal handling of derived classes.
    onQtPropertyChanged(property);
}


void MActor::actOnOtherActorCreated(MActor *actor)
{
    onOtherActorCreated(actor);
}


void MActor::actOnOtherActorDeleted(MActor *actor)
{
    onOtherActorDeleted(actor);
}


void MActor::actOnOtherActorRenamed(MActor *actor, QString oldName)
{
    onOtherActorRenamed(actor, oldName);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

bool MActor::suppressActorUpdates()
{
    return ( !((actorUpdatesDisabledCounter == 0) && isInitialized()) );
}


void MActor::enableEmissionOfActorChangedSignal(bool enabled)
{
    if (enabled)
        actorChangedSignalDisabledCounter = min(0, actorChangedSignalDisabledCounter + 1);
    else
        actorChangedSignalDisabledCounter--;
}


void MActor::beginCompileShaders(int numberOfShaders)
{
    if (shaderCompilationProgressDialog == nullptr)
    {
        shaderCompilationProgressDialog = new QProgressDialog(
                    "Compiling OpenGL GLSL shaders...", "",
                    0, numberOfShaders);
        // Hide cancel button.
        shaderCompilationProgressDialog->setCancelButton(0);
        // Hide close, resize and minimize buttons.
        shaderCompilationProgressDialog->setWindowFlags(
                    Qt::Dialog | Qt::CustomizeWindowHint);
        shaderCompilationProgressDialog->setMinimumDuration(0);
    }

    // Set progress to 0%.
    shaderCompilationProgress = 0;
    shaderCompilationProgressDialog->setValue(0);

    // Block access to active widget while progress dialog is active.
    // NOTE: This only works if the main window has already been shown; hence
    // only after the application has been fully initialized.
    if (MSystemManagerAndControl::getInstance()->applicationIsInitialized())
    {
        shaderCompilationProgressDialog->setWindowModality(Qt::ApplicationModal);
    }

    // Always show progress dialog right away.
    shaderCompilationProgressDialog->show();
    shaderCompilationProgressDialog->repaint();
}


void MActor::endCompileShaders()
{
    shaderCompilationProgressDialog->hide();
}


void MActor::compileShadersFromFileWithProgressDialog(
        std::shared_ptr<GL::MShaderEffect> shader,
        const QString filename)
{
    shader->compileFromFile_Met3DHome(filename);
    shaderCompilationProgressDialog->setValue(++shaderCompilationProgress);
    shaderCompilationProgressDialog->repaint();
}


void MActor::emitActorChangedSignal()
{
    if ((actorChangedSignalDisabledCounter == 0) && actorIsEnabled
            && isInitialized() && (actorUpdatesDisabledCounter == 0))
    {
        emit actorChanged();
    }
}


void MActor::listenToPropertyManager(const QObject *sender)
{
    connect(sender,
            SIGNAL(propertyChanged(QtProperty*)),
            SLOT(actOnQtPropertyChanged(QtProperty*)));
}


void MActor::enableActorUpdates(bool enable)
{
    if (enable)
        actorUpdatesDisabledCounter = min(0, actorUpdatesDisabledCounter + 1);
    else
        actorUpdatesDisabledCounter--;
}


GLint MActor::assignTextureUnit()
{
    if (availableTextureUnits.size() > 0)
    {
        GLint unit = availableTextureUnits.takeFirst();
        assignedTextureUnits << unit;

        LOG4CPLUS_TRACE(mlog, "Assigning texture unit " << unit);

        return unit;
    }
    else
    {
        LOG4CPLUS_ERROR(mlog, "No texture units available anymore!");
        return -1;
    }
}


void MActor::releaseTextureUnit(GLint unit)
{
    LOG4CPLUS_TRACE(mlog, "Releasing texture unit " << unit);

    if (assignedTextureUnits.contains(unit))
    {
        assignedTextureUnits.removeOne(unit);
        availableTextureUnits << unit;
        // Sort the available units so that the lowest value is chosen next
        // time requestTextureUnit() is called.
        qSort(availableTextureUnits);
    }
    else
    {
        LOG4CPLUS_ERROR(mlog, "Failure at attempt to release not assigned "
                        "texture unit!");
    }
}


GLint MActor::assignImageUnit()
{
    if (availableImageUnits.size() > 0)
    {
        GLint unit = availableImageUnits.takeFirst();
        assignedImageUnits << unit;

        LOG4CPLUS_TRACE(mlog, "Assigning image unit " << unit);

        return unit;
    }
    else
    {
        LOG4CPLUS_ERROR(mlog, "No image units available anymore!");
        return -1;
    }
}


void MActor::releaseImageUnit(GLint unit)
{
    LOG4CPLUS_TRACE(mlog, "Releasing image unit " << unit);

    if (assignedImageUnits.contains(unit))
    {
        assignedImageUnits.removeOne(unit);
        availableImageUnits << unit;
        // Sort the available units so that the lowest value is chosen next
        // time requestImageUnit() is called.
        qSort(availableImageUnits);
    }
    else
    {
        LOG4CPLUS_ERROR(mlog, "Failure at attempt to release not assigned "
                        "image unit!");
    }
}


void MActor::removeAllLabels()
{
    // Remove all text labels of the old geometry.
    while ( !labels.isEmpty() )
        MGLResourcesManager::getInstance()->getTextManager()
                ->removeText(labels.takeLast());
}


void MActor::uploadVec3ToVertexBuffer(const QVector<QVector3D>& data,
                              const QString requestKey,
                              GL::MVertexBuffer** vbo,
                              QGLWidget* currentGLContext)
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    GL::MVertexBuffer* vb = static_cast<GL::MVertexBuffer*>(
                                            glRM->getGPUItem(requestKey));

    if (vb)
    {
        *vbo = vb;
        GL::MVector3DVertexBuffer* buf = dynamic_cast<GL::MVector3DVertexBuffer*>(vb);
        // reallocate buffer if size has changed
        buf->reallocate(nullptr, data.size(), 0, false, currentGLContext);
        buf->update(data, 0, 0, currentGLContext);

    } else {

        GL::MVector3DVertexBuffer* newVB = nullptr;
        newVB = new GL::MVector3DVertexBuffer(requestKey, data.size());
        if (glRM->tryStoreGPUItem(newVB)) { newVB->upload(data, currentGLContext); }
        else { delete newVB; }
        *vbo = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(requestKey));
    }
}


void MActor::uploadVec2ToVertexBuffer(const QVector<QVector2D>& data,
                              const QString requestKey,
                              GL::MVertexBuffer** vbo,
                              QGLWidget* currentGLContext)
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    GL::MVertexBuffer* vb = static_cast<GL::MVertexBuffer*>(
                                            glRM->getGPUItem(requestKey));

    if (vb)
    {
        *vbo = vb;
        GL::MVector2DVertexBuffer* buf = dynamic_cast<GL::MVector2DVertexBuffer*>(vb);
        // reallocate buffer if size has changed
        buf->reallocate(nullptr, data.size(), 0, false, currentGLContext);
        buf->update(data, 0, 0, currentGLContext);

    } else {

        GL::MVector2DVertexBuffer* newVB = nullptr;
        newVB = new GL::MVector2DVertexBuffer(requestKey, data.size());
        if (glRM->tryStoreGPUItem(newVB)) { newVB->upload(data, currentGLContext); }
        else { delete newVB; }
        *vbo = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(requestKey));
    }
}


void MActor::uploadVec3ToVertexBuffer(const QVector<QVector3D> *data, GLuint *vbo)
{
    // Delete the old VBO. If this is the first time the method has been called
    // (i.e. no VBO exists), "vbo" is set to 0 (constructor!); glDeleteBuffers
    // ignores 0 buffers.
    glDeleteBuffers(1, vbo); CHECK_GL_ERROR;

    // Generate new VBO and upload geometry data to GPU.
    glGenBuffers(1, vbo); CHECK_GL_ERROR;
    glBindBuffer(GL_ARRAY_BUFFER, *vbo); CHECK_GL_ERROR;
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * data->size() * 3,
                 data->constData(),
                 GL_STATIC_DRAW); CHECK_GL_ERROR;
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


double MActor::computePositionLabelDistanceWeight(
        MCamera* camera, QVector3D mousePosWorldSpace)
{

    // Calculate distance of label to handle with repsect to distance of
    // handle and camera. (Avoid too large distance for camera near handle
    // and too small distance for camera far away from handle.)
    QVector3D zAxis = camera->getZAxis();
    zAxis.normalize();
    QVector3D camPos = camera->getOrigin();
    double dist = -(QVector3D::dotProduct(zAxis, camPos)
                    - sqrt(QVector3D::dotProduct(mousePosWorldSpace,
                                                 mousePosWorldSpace)));
    dist *= dist * 0.00003;

    return dist;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/




/******************************************************************************
***                         MAbstractActorFactory                           ***
*******************************************************************************/

MAbstractActorFactory::MAbstractActorFactory()
{
}


MAbstractActorFactory::~MAbstractActorFactory()
{
}


void MAbstractActorFactory::initialize()
{
    // Determine the actor type and settings ID by instantiating
    // a default actor instance.
    MActor* actor = createInstance();
    if (actor)
    {
        name = actor->getActorType();
        settingsID = actor->getSettingsID();
        delete actor;
    }
}


QString MAbstractActorFactory::getName()
{
    return name;
}


MActor *MAbstractActorFactory::create(const QString &configfile)
{
    LOG4CPLUS_DEBUG(mlog, "Creating new default instance of <"
                    << getName().toStdString() << "> ...");

    MActor* actor = createInstance();

    if (!configfile.isEmpty()) actor->loadConfigurationFromFile(configfile);

    LOG4CPLUS_DEBUG(mlog, "... instance of <"
                    << getName().toStdString() << "> has been created.");

    return actor;
}


bool MAbstractActorFactory::acceptSettings(QSettings *settings)
{
    QStringList groups = settings->childGroups();
    return groups.contains(settingsID);
}


bool MAbstractActorFactory::acceptSettings(const QString& configfile)
{
    QSettings* settings = new QSettings(configfile, QSettings::IniFormat);
    bool accept = acceptSettings(settings);
    delete settings;
    return accept;
}


} // namespace Met3D

