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
/******************************************************************************
**
** NOTE: This file is based on code from the QtPropertyBrowser framework. Parts
** have been copied and partly modified from the "decoration" example, from
** qtpropertymanager.h and .cpp, qtpropertybrowserutils_p.h and .cpp,
** qteditorfactory.h and .cpp.
**
** This file incorporates work covered by the following copyright and
** permission notice:
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Solutions component.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
*******************************************************************************/
#include "qtpropertymanager_extensions.h"

// standard library imports

// related third party imports
#include <QApplication>
#include <QPainter>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QCheckBox>
#include <QLineEdit>
#include <QMenu>
#include <QStyleOption>

// local application imports

namespace QtExtensions
{

/******************************************************************************
***             Property manager for "decorated" double values              ***
*******************************************************************************/

QtDecoratedDoublePropertyManager::QtDecoratedDoublePropertyManager(QObject *parent)
    : QtDoublePropertyManager(parent)
{
}


QtDecoratedDoublePropertyManager::~QtDecoratedDoublePropertyManager()
{
}


QString QtDecoratedDoublePropertyManager::prefix(const QtProperty *property) const
{
    if (!propertyToData.contains(property))
        return QString();
    return propertyToData[property].prefix;
}


QString QtDecoratedDoublePropertyManager::suffix(const QtProperty *property) const
{
    if (!propertyToData.contains(property))
        return QString();
    return propertyToData[property].suffix;
}


void QtDecoratedDoublePropertyManager::setPrefix(QtProperty *property, const QString &prefix)
{
    if (!propertyToData.contains(property))
        return;

    QtDecoratedDoublePropertyManager::Data data = propertyToData[property];
    if (data.prefix == prefix)
        return;

    data.prefix = prefix;
    propertyToData[property] = data;

    emit propertyChanged(property);
    emit prefixChanged(property, prefix);
}


void QtDecoratedDoublePropertyManager::setSuffix(QtProperty *property, const QString &suffix)
{
    if (!propertyToData.contains(property))
        return;

    QtDecoratedDoublePropertyManager::Data data = propertyToData[property];
    if (data.suffix == suffix)
        return;

    data.suffix = suffix;
    propertyToData[property] = data;

    emit propertyChanged(property);
    emit suffixChanged(property, suffix);
}


QString QtDecoratedDoublePropertyManager::valueText(const QtProperty *property) const
{
    QString text = QtDoublePropertyManager::valueText(property);
    if (!propertyToData.contains(property))
        return text;

    QtDecoratedDoublePropertyManager::Data data = propertyToData[property];
    text = data.prefix + text + data.suffix;

    return text;
}


void QtDecoratedDoublePropertyManager::initializeProperty(QtProperty *property)
{
    propertyToData[property] = QtDecoratedDoublePropertyManager::Data();
    QtDoublePropertyManager::initializeProperty(property);
}


void QtDecoratedDoublePropertyManager::uninitializeProperty(QtProperty *property)
{
    propertyToData.remove(property);
    QtDoublePropertyManager::uninitializeProperty(property);
}


QtDecoratedDoubleSpinBoxFactory::QtDecoratedDoubleSpinBoxFactory(QObject *parent)
    : QtAbstractEditorFactory<QtDecoratedDoublePropertyManager>(parent)
{
    originalFactory = new QtDoubleSpinBoxFactory(this);
}


QtDecoratedDoubleSpinBoxFactory::~QtDecoratedDoubleSpinBoxFactory()
{
    // not need to delete editors because they will be deleted by originalFactory in its destructor
}


void QtDecoratedDoubleSpinBoxFactory::connectPropertyManager(QtDecoratedDoublePropertyManager *manager)
{
    originalFactory->addPropertyManager(manager);
    connect(manager, SIGNAL(prefixChanged(QtProperty *, const QString &)), this, SLOT(slotPrefixChanged(QtProperty *, const QString &)));
    connect(manager, SIGNAL(suffixChanged(QtProperty *, const QString &)), this, SLOT(slotSuffixChanged(QtProperty *, const QString &)));
}


QWidget *QtDecoratedDoubleSpinBoxFactory::createEditor(QtDecoratedDoublePropertyManager *manager, QtProperty *property,
        QWidget *parent)
{
    QtAbstractEditorFactoryBase *base = originalFactory;
    QWidget *w = base->createEditor(property, parent);
    if (!w)
        return 0;

    QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox *>(w);
    if (!spinBox)
        return 0;

    spinBox->setPrefix(manager->prefix(property));
    spinBox->setSuffix(manager->suffix(property));

    createdEditors[property].append(spinBox);
    editorToProperty[spinBox] = property;

    return spinBox;
}


void QtDecoratedDoubleSpinBoxFactory::disconnectPropertyManager(QtDecoratedDoublePropertyManager *manager)
{
    originalFactory->removePropertyManager(manager);
    disconnect(manager, SIGNAL(prefixChanged(QtProperty *, const QString &)), this, SLOT(slotPrefixChanged(QtProperty *, const QString &)));
    disconnect(manager, SIGNAL(suffixChanged(QtProperty *, const QString &)), this, SLOT(slotSuffixChanged(QtProperty *, const QString &)));
}


void QtDecoratedDoubleSpinBoxFactory::slotPrefixChanged(QtProperty *property, const QString &prefix)
{
    if (!createdEditors.contains(property))
        return;

    QtDecoratedDoublePropertyManager *manager = propertyManager(property);
    if (!manager)
        return;

    QList<QDoubleSpinBox *> editors = createdEditors[property];
    QListIterator<QDoubleSpinBox *> itEditor(editors);
    while (itEditor.hasNext()) {
        QDoubleSpinBox *editor = itEditor.next();
        editor->setPrefix(prefix);
    }
}


void QtDecoratedDoubleSpinBoxFactory::slotSuffixChanged(QtProperty *property, const QString &prefix)
{
    if (!createdEditors.contains(property))
        return;

    QtDecoratedDoublePropertyManager *manager = propertyManager(property);
    if (!manager)
        return;

    QList<QDoubleSpinBox *> editors = createdEditors[property];
    QListIterator<QDoubleSpinBox *> itEditor(editors);
    while (itEditor.hasNext()) {
        QDoubleSpinBox *editor = itEditor.next();
        editor->setSuffix(prefix);
    }
}


void QtDecoratedDoubleSpinBoxFactory::slotEditorDestroyed(QObject *object)
{
    QMap<QDoubleSpinBox *, QtProperty *>::ConstIterator itEditor =
                editorToProperty.constBegin();
    while (itEditor != editorToProperty.constEnd()) {
        if (itEditor.key() == object) {
            QDoubleSpinBox *editor = itEditor.key();
            QtProperty *property = itEditor.value();
            editorToProperty.remove(editor);
            createdEditors[property].removeAll(editor);
            if (createdEditors[property].isEmpty())
                createdEditors.remove(property);
            return;
        }
        itEditor++;
    }
}


/******************************************************************************
***                       QtClickPropertyManager                            ***
*******************************************************************************/

class QtClickPropertyManagerPrivate
{
    QtClickPropertyManager *q_ptr;
    Q_DECLARE_PUBLIC(QtClickPropertyManager)
public:

    QMap<const QtProperty *, bool> m_values;
};


QtClickPropertyManager::QtClickPropertyManager(QObject *parent)
    : QtAbstractPropertyManager(parent)
{
    d_ptr = new QtClickPropertyManagerPrivate;
    d_ptr->q_ptr = this;
}


QtClickPropertyManager::~QtClickPropertyManager()
{
    clear();
    delete d_ptr;
}


QString QtClickPropertyManager::valueText(const QtProperty *property) const
{
    Q_UNUSED(property);
    return QString("(click to execute)");
}


void QtClickPropertyManager::emitClicked(QtProperty *property)
{
    emit propertyChanged(property);
}


void QtClickPropertyManager::initializeProperty(QtProperty *property)
{
    d_ptr->m_values[property] = false;
}


void QtClickPropertyManager::uninitializeProperty(QtProperty *property)
{
    d_ptr->m_values.remove(property);
}


/******************************************************************************
***                         QtToolButtonBoolEdit                            ***
*******************************************************************************/

QtToolButtonBoolEdit::QtToolButtonBoolEdit(QWidget *parent) :
    QWidget(parent),
    m_toolButton(new QToolButton(this))
{
    QHBoxLayout *lt = new QHBoxLayout;
    if (QApplication::layoutDirection() == Qt::LeftToRight)
        lt->setContentsMargins(4, 0, 0, 0);
    else
        lt->setContentsMargins(0, 0, 4, 0);
    lt->addWidget(m_toolButton);
    setLayout(lt);
    connect(m_toolButton, SIGNAL(clicked()), this, SIGNAL(clicked()));
    setFocusProxy(m_toolButton);
    m_toolButton->setText(tr("execute"));
}


void QtToolButtonBoolEdit::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() == Qt::LeftButton)
    {
        m_toolButton->click();
        event->accept();
    }
    else
    {
        QWidget::mousePressEvent(event);
    }
}


void QtToolButtonBoolEdit::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}


/******************************************************************************
***                          QtToolButtonFactory                            ***
*******************************************************************************/

void QtToolButtonFactoryPrivate::forwardClickedSignal()
{
    QObject *object = q_ptr->sender();

    const QMap<QtToolButtonBoolEdit *, QtProperty *>::ConstIterator ecend =
            m_editorToProperty.constEnd();
    for (QMap<QtToolButtonBoolEdit *, QtProperty *>::ConstIterator itEditor =
         m_editorToProperty.constBegin(); itEditor != ecend;  ++itEditor)
        if (itEditor.key() == object)
        {
            QtProperty *property = itEditor.value();
            QtClickPropertyManager *manager = q_ptr->propertyManager(property);
            if (!manager)
                return;
            manager->emitClicked(property);
            return;
        }
}


QtToolButtonFactory::QtToolButtonFactory(QObject *parent)
    : QtAbstractEditorFactory<QtClickPropertyManager>(parent)
{
    d_ptr = new QtToolButtonFactoryPrivate();
    d_ptr->q_ptr = this;

}


QtToolButtonFactory::~QtToolButtonFactory()
{
    qDeleteAll(d_ptr->m_editorToProperty.keys());
    delete d_ptr;
}


// Reimplemented from the QtAbstractEditorFactory class.
QWidget *QtToolButtonFactory::createEditor(
        QtClickPropertyManager *manager, QtProperty *property, QWidget *parent)
{
    Q_UNUSED(manager);
    QtToolButtonBoolEdit *editor = d_ptr->createEditor(property, parent);

    connect(editor, SIGNAL(clicked()),
            this, SLOT(forwardClickedSignal()));
    connect(editor, SIGNAL(destroyed(QObject *)),
            this, SLOT(slotEditorDestroyed(QObject *)));

    return editor;
}

} // namespace QtExtensions
