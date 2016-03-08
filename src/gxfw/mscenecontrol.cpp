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
#include "mscenecontrol.h"
#include "ui_mscenecontrol.h"

// standard library imports
#include <iostream>

// related third party imports
#include "qteditorfactory.h"
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "qt_extensions/qtpropertymanager_extensions.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

// Property managers are only required once for all scenes. Initialised in
// MSceneControl constructor or getQtProperties(), whichever is called first.
MQtProperties* MSceneControl::qtProperties = nullptr;

MSceneControl::MSceneControl(QString name, QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MSceneControl),
      name(name),
      blockRedrawRequests(0),
      actorChangeDuringBlock(false)
{
    ui->setupUi(this);

    // The actorPropertiesBrowser needs "GUI editor factories" that provide the
    // required GUI elements (spin boxes, line edits, combo boxes, ...) for
    // editing the properties. NOTE: The factories should be deleted by Qt
    // (automatic child destruction..).
    QtCheckBoxFactory *checkBoxFactory
            = new QtCheckBoxFactory(this);
    QtSpinBoxFactory *spinBoxFactory
            = new QtSpinBoxFactory(this);
    QtDoubleSpinBoxFactory *doubleSpinBoxFactory
            = new QtDoubleSpinBoxFactory(this);
    QtExtensions::QtDecoratedDoubleSpinBoxFactory *decoratedDoubleSpinBoxFactory
            = new QtExtensions::QtDecoratedDoubleSpinBoxFactory(this);
    QtDateTimeEditFactory *dateTimeEditFactory
            = new QtDateTimeEditFactory(this);
    QtEnumEditorFactory *enumEditorFactory
            = new QtEnumEditorFactory(this);
    QtColorEditorFactory *colorEditorFactory
            = new QtColorEditorFactory(this);
    QtLineEditFactory *lineEditFactory
            = new QtLineEditFactory(this);
    QtExtensions::QtToolButtonFactory *toolButtonFactory
            = new QtExtensions::QtToolButtonFactory(this);

    // Scene and actor properties are displayed in a tree property browser
    // widget. Connect with the necessary property managers (see
    // http://doc.qt.nokia.com/solutions/4/qtpropertybrowser/index.html).

    if (qtProperties == nullptr) qtProperties = new MQtProperties();

    actorPropertiesBrowser = new QtTreePropertyBrowser();
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mBool(),
                checkBoxFactory);
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mInt(),
                spinBoxFactory);
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mDouble(),
                doubleSpinBoxFactory);
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mDecoratedDouble(),
                decoratedDoubleSpinBoxFactory);
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mDateTime(),
                dateTimeEditFactory);
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mEnum(),
                enumEditorFactory);
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mRectF()->subDoublePropertyManager(),
                doubleSpinBoxFactory);
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mPointF()->subDoublePropertyManager(),
                doubleSpinBoxFactory);
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mColor(),
                colorEditorFactory);
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mString(),
                lineEditFactory);
    actorPropertiesBrowser->setFactoryForManager(
                qtProperties->mClick(),
                toolButtonFactory);

    // Mode for resizing the columns. Useful are QtTreePropertyBrowser::
    // ::ResizeToContents and ::Interactive
    actorPropertiesBrowser->setResizeMode(QtTreePropertyBrowser::ResizeToContents);
    actorPropertiesBrowser->setAlternatingRowColors(true);

    // Add the actor properties browser to the GUI.
    ui->actorPropertiesLayout->addWidget(actorPropertiesBrowser);
}


MSceneControl::~MSceneControl()
{
    delete actorPropertiesBrowser;
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

int MSceneControl::addActor(MActor *actor, int index)
{
    LOG4CPLUS_DEBUG(mlog, "adding actor ''" << actor->getName().toStdString()
                    << "'' to scene ''" << name.toStdString() << "''" << flush) ;

//    int insertAtIndex = max(renderQueue.size() - 1, 0);
    int insertAtIndex = renderQueue.size();
    if (index != -1 && renderQueue.size() > index) insertAtIndex = index;

    // Keep track of the connected actors by storing them in a render queue.
    renderQueue.insert(insertAtIndex, actor);

    // Add the item's properties to the property browser.
    QtBrowserItem *item = actorPropertiesBrowser
            ->addProperty(actor->getPropertyGroup());

    browserItems.insert(insertAtIndex, item);
    // Set a light yellow as background colour (the colour is taken from the
    // QtDesigner property editor (screenshot and GIMP..)).
    actorPropertiesBrowser->setBackgroundColor(item, QColor(255, 255, 191));
    // By default collapse the new item..
    actorPropertiesBrowser->setExpanded(item, false);
    // ..and collapse its children items.
    QList<QtBrowserItem*> childrenItems = item->children();
    while (!childrenItems.isEmpty())
    {
        QtBrowserItem* child = childrenItems.takeFirst();
        actorPropertiesBrowser->setExpanded(child, false);
        childrenItems.append(child->children());
    }

    // Collect the "actorChanged()" signals of all registered actors in the
    // "actOnActorChange()" slot of this class.
    connect(actor, SIGNAL(actorChanged()), SLOT(onActorChanged()));

    // Tell the actor that it has been added to this scene.
    actor->registerScene(this);

    // Tell the actor to inform this scene about its synchronized elements.
    actor->provideSynchronizationInfoToScene(this);

    emit sceneChanged();

    return insertAtIndex; // return index of actor in queue
}

void MSceneControl::removeActor(int id)
{
    MActor* actor = renderQueue.at(id);

    LOG4CPLUS_DEBUG(mlog, "removing actor ''" << actor->getName().toStdString()
                    << "'' from scene " << name.toStdString());

    actorPropertiesBrowser->removeProperty(actor->getPropertyGroup());

    renderQueue.remove(id);
}


void MSceneControl::removeActorByName(const QString& actorName)
{
    MActor* actor = nullptr;
    int id = -1;

    for (int i = 0; i < renderQueue.size(); ++i)
    {
        if (renderQueue[i]->getName() == actorName)
        {
            id = i;
            actor = renderQueue[i];
            break;
        }
    }

    if (!actor) return;

    LOG4CPLUS_DEBUG(mlog, "removing actor ''" << actorName.toStdString()
                    << "'' from scene " << name.toStdString());

    actorPropertiesBrowser->removeProperty(actor->getPropertyGroup());

    actor->deregisterScene(this);

    renderQueue.remove(id);

    emit sceneChanged();
}


int MSceneControl::getActorRenderID(const QString& actorName) const
{
    for (int i = 0; i < renderQueue.size(); ++i)
    {
        if (renderQueue[i]->getName() == actorName)
        {
            return i;
        }
    }

    return -1;
}


void MSceneControl::setPropertyColour(QtProperty *property, QColor colour)
{
    foreach (QtBrowserItem *item, actorPropertiesBrowser->items(property))
        actorPropertiesBrowser->setBackgroundColor(item, colour);
}


void MSceneControl::resetPropertyColour(QtProperty *property)
{
    foreach (QtBrowserItem *item, actorPropertiesBrowser->items(property))
        actorPropertiesBrowser->setBackgroundColor(item, QColor(255, 255, 191));
}


void MSceneControl::variableSynchronizesWith(MSyncControl *sync)
{
    syncControlCounter[sync]++;

    connect(sync, SIGNAL(beginSynchronization()),
            this, SLOT(startBlockingRedraws()));
    connect(sync, SIGNAL(endSynchronization()),
            this, SLOT(endBlockingRedraws()));
}


void MSceneControl::variableDeletesSynchronizationWith(MSyncControl *sync)
{
    syncControlCounter[sync]--;

    if (syncControlCounter[sync] == 0)
    {
        disconnect(sync, SIGNAL(beginSynchronization()),
                   this, SLOT(startBlockingRedraws()));
        disconnect(sync, SIGNAL(endSynchronization()),
                   this, SLOT(endBlockingRedraws()));
    }
}


void MSceneControl::registerSceneView(MSceneViewGLWidget *view)
{
    registeredSceneViews.insert(view);
}


void MSceneControl::unregisterSceneView(MSceneViewGLWidget *view)
{
    registeredSceneViews.remove(view);
}


MQtProperties *MSceneControl::getQtProperties()
{
    if (qtProperties == nullptr) qtProperties = new MQtProperties();
    return qtProperties;
}


void MSceneControl::collapsePropertySubTree(QtProperty *property)
{
    foreach (QtBrowserItem *item, actorPropertiesBrowser->items(property))
    {
        // Collapse the item..
        actorPropertiesBrowser->setExpanded(item, false);
        // ..and collapse its child items.
        QList<QtBrowserItem*> childrenItems = item->children();
        while (!childrenItems.isEmpty())
        {
            QtBrowserItem* child = childrenItems.takeFirst();
            actorPropertiesBrowser->setExpanded(child, false);
            childrenItems.append(child->children());
        }
    }
}


void MSceneControl::setSingleInteractionActor(MActor *actor)
{
    foreach (MSceneViewGLWidget* view, registeredSceneViews)
        view->setSingleInteractionActor(actor);
}


void MSceneControl::collapseActorPropertyTree(MActor *actor)
{
    collapsePropertySubTree(actor->getPropertyGroup());
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MSceneControl::onActorChanged()
{
    if (blockRedrawRequests == 0)
    {
        // If no blocking requests are currently registered, emit a signal to
        // redraw the scene.
        emit sceneChanged();
    }
    else
        // Remember that something has changed while redrawing was blocked.
        actorChangeDuringBlock = true;
}


void MSceneControl::reloadActorShaders()
{
    for (int i = 0; i < renderQueue.size(); i++)
        renderQueue[i]->reloadShaderEffects();

    emit sceneChanged(); // trigger redraw of the scene
}


void MSceneControl::startBlockingRedraws()
{
    // Increase the number of blocking requests. See notes 23Jan2013.
    blockRedrawRequests++;
}


void MSceneControl::endBlockingRedraws()
{
    // Decrease the number of blocking requests.
    if (blockRedrawRequests > 0) blockRedrawRequests--;

    // If something has changed since the last call to start/end blocking,
    // redraw the scene.
    if (actorChangeDuringBlock)
    {
        actorChangeDuringBlock = false;
        emit sceneChanged();
    }
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

} // namespace Met3D
