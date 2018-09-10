/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2017-2018 Bianca Tost
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
** Code of manager and factory handling support of scientific notation are
** partially copied form manager and factory handling double spinboxes.
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
#include <cfloat>
#include <cmath>

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
    delete originalFactory;
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

    connect(spinBox, SIGNAL(destroyed(QObject *)),
            this, SLOT(slotEditorDestroyed(QObject *)));

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
        editor->blockSignals(true);
        editor->setPrefix(prefix);
        editor->blockSignals(false);
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
        editor->blockSignals(true);
        editor->setSuffix(prefix);
        editor->blockSignals(false);
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
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
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


/******************************************************************************
***                    QtScientificDoublePropertyManager                    ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

QtScientificDoublePropertyManager::QtScientificDoublePropertyManager(
        QObject *parent)
    : QtDecoratedDoublePropertyManager(parent)
{
}


QtScientificDoublePropertyManager::~QtScientificDoublePropertyManager()
{
    clear();
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

int QtScientificDoublePropertyManager::significantDigits(
        const QtProperty *property) const
{
    if (!propertyToData.contains(property))
    {
        return 0;
    }
    return propertyToData[property].significantDigits;
}


int QtScientificDoublePropertyManager::switchNotationExponent(
        const QtProperty *property) const
{
    if (!propertyToData.contains(property))
    {
        return 0;
    }
    return propertyToData[property].switchNotationExponent;
}


int QtScientificDoublePropertyManager::minimumExponent(
        const QtProperty *property) const
{
    if (!propertyToData.contains(property))
    {
        return 0;
    }
    return propertyToData[property].minimumExponent;
}


QString QtScientificDoublePropertyManager::valueAsPropertyFormatedText(
        const QtProperty *property, const double value) const
{
    return getTextFromValue(property, value);
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void QtScientificDoublePropertyManager::setSignificantDigits(
        QtProperty *property, const int &significantDigits)
{
    if (!propertyToData.contains(property))
    {
        return;
    }

    QtScientificDoublePropertyManager::Data data = propertyToData[property];
    if (data.significantDigits == significantDigits)
    {
        return;
    }

    // Only in range from 1 to 9 (max significant digits of float value).
    data.significantDigits = qBound(1, significantDigits, 9);

    propertyToData[property] = data;

    emit significantDigitsChanged(property, data.significantDigits);
    emit propertyChanged(property);
}


void QtScientificDoublePropertyManager::setSwitchNotationExponent(
        QtProperty *property, const int &switchNotationExponent)
{
    if (!propertyToData.contains(property))
    {
        return;
    }

    QtScientificDoublePropertyManager::Data data = propertyToData[property];
    if (data.switchNotationExponent == switchNotationExponent)
        return;

    data.switchNotationExponent = switchNotationExponent;
    propertyToData[property] = data;

    emit switchNotationExponentChanged(property, switchNotationExponent);
    emit propertyChanged(property);
}


void QtScientificDoublePropertyManager::setMinimumExponent(
        QtProperty *property, const int &minExponent)
{
    if (!propertyToData.contains(property))
    {
        return;
    }

    int minExp = qBound(0, minExponent, FLT_MAX_10_EXP);

    QtScientificDoublePropertyManager::Data data = propertyToData[property];
    if (data.minimumExponent == minExp)
        return;

    data.minimumExponent = minExp;
    propertyToData[property] = data;

    setDecimals(property, minExp);

    emit minimumExponentChanged(property, minExp);
    emit propertyChanged(property);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

QString QtScientificDoublePropertyManager::valueText(
        const QtProperty *property) const
{
    QString text = QtDoublePropertyManager::valueText(property);

    if (!propertyToData.contains(property))
    {
        return text;
    }

    double val = value(property);

    text = getTextFromValue(property, val);

    return prefix(property) + text + suffix(property);
}


QString QtScientificDoublePropertyManager::getTextFromValue(
        const QtProperty *property, double value) const
{
    if (!propertyToData.contains(property))
    {
        return "";
    }

    int significDigits = std::max(0, significantDigits(property) - 1);
    int switchNotationExp = switchNotationExponent(property);

    QString text = QLocale::system().toString(value, 'E', significDigits);
    value = QLocale::system().toDouble(text);

    // Switch to scientific notation only if the absolute value of the exponent
    // is bigger than the threshold given.
    if (value != 0.
            && (static_cast<int>(fabs(floor(log10(fabs(value)))))
                >= switchNotationExp))
    {
        text = QLocale::system().toString(value, 'E', significDigits);
    }
    else
    {
        int indexExponentialSign = text.indexOf(
                    QLocale::system().exponential(), 0, Qt::CaseInsensitive);
        QString exponentString = text.right(
                    text.length() - (indexExponentialSign + 1));
        int exponent = QLocale::system().toInt(exponentString);
        if (exponent < 0)
        {
            significDigits -= exponent;
        }

        text = QLocale::system().toString(value, 'f', significDigits);
    }

    // Remove trailing zeros after decimal point.
    int decimalPointIndex = text.indexOf(QLocale::system().decimalPoint());
    if (decimalPointIndex > 0)
    {
        int exponentIndex = text.indexOf(QLocale::system().exponential(), 0,
                                         Qt::CaseInsensitive);
        if (exponentIndex >= 0)
        {
            exponentIndex -= 1;
        }
        else
        {
            exponentIndex = text.length();
        }
        int nonZeroIndex = text.lastIndexOf(QRegExp("[^0]"), exponentIndex);
        if (nonZeroIndex >= decimalPointIndex)
        {
            if (nonZeroIndex != decimalPointIndex)
            {
                nonZeroIndex++;
            }
            text.remove(nonZeroIndex, exponentIndex + 1 - nonZeroIndex);
        }
    }

    return text;
}


void QtScientificDoublePropertyManager::initializeProperty(QtProperty *property)
{
    propertyToData[property] = QtScientificDoublePropertyManager::Data();
    QtDecoratedDoublePropertyManager::initializeProperty(property);
}


void QtScientificDoublePropertyManager::uninitializeProperty(QtProperty *property)
{
    propertyToData.remove(property);
    QtDecoratedDoublePropertyManager::uninitializeProperty(property);
}


// TODO (bt, 19Jan2018): Use this class to implement a configurable SDProperty Manager.
//   Idea: cf. system/qtproperties.h
/******************************************************************************
***              QtConfigurableScientificDoublePropertyManager              ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

QtConfigurableScientificDoublePropertyManager
::QtConfigurableScientificDoublePropertyManager(
        QObject *parent)
    : QtScientificDoublePropertyManager(parent)
{
    intPropertyManager = new QtIntPropertyManager(this);
    connect(intPropertyManager, SIGNAL(valueChanged(QtProperty *, int)),
            this, SLOT(slotIntChanged(QtProperty *, int)));
    connect(intPropertyManager, SIGNAL(propertyDestroyed(QtProperty *)),
            this, SLOT(slotPropertyDestroyed(QtProperty *)));

    sciDoublePropertyManager = new QtScientificDoublePropertyManager(this);
    connect(sciDoublePropertyManager, SIGNAL(valueChanged(QtProperty *, int)),
            this, SLOT(slotScientificDoubleChanged(QtProperty *, double)));
    connect(sciDoublePropertyManager, SIGNAL(propertyDestroyed(QtProperty *)),
            this, SLOT(slotPropertyDestroyed(QtProperty *)));
}


QtConfigurableScientificDoublePropertyManager
::~QtConfigurableScientificDoublePropertyManager()
{
    clear();
    delete intPropertyManager;
    delete sciDoublePropertyManager;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QtIntPropertyManager *QtConfigurableScientificDoublePropertyManager::subIntPropertyManager() const
{
    return intPropertyManager;
}


QtScientificDoublePropertyManager *QtConfigurableScientificDoublePropertyManager
::subSciDoublePropertyManager() const
{
    return sciDoublePropertyManager;
}


QString QtConfigurableScientificDoublePropertyManager::configuration(
        const QtProperty *property) const
{
    return QString("%1/%2/%3/%4")
            .arg(value(property))
            .arg(singleStep(property))
            .arg(significantDigits(property))
            .arg(switchNotationExponent(property));
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void QtConfigurableScientificDoublePropertyManager::setConfiguration(
        QtProperty *property, const QString &config)
{
    if (!propertyToData.contains(property))
    {
        return;
    }

    QStringList configurationList = config.split("/");

    if (configurationList.size() < 4)
    {
        return;
    }

    double configValue = QLocale::system().toDouble(configurationList.at(0));
    int configSingleStep = QLocale::system().toInt(configurationList.at(1));
    int configSignificantDigits = QLocale::system().toInt(configurationList.at(2));
    int configMSwitchNotationExponent =
            QLocale::system().toInt(configurationList.at(3));

    QtScientificDoublePropertyManager::Data data = propertyToData[property];
    if ((configValue == value(property))
            && (configSingleStep == singleStep(property))
            && (data.significantDigits == configSignificantDigits)
            && (data.switchNotationExponent == configMSwitchNotationExponent))
    {
        return;
    }

    blockSignals(true);
    setSignificantDigits(property, configSignificantDigits);
    setSwitchNotationExponent(property, configMSwitchNotationExponent);
    setSingleStep(property, configSingleStep);
    setValue(property, configValue);
    blockSignals(false);

    emit singleStepChanged(property, configSingleStep);
    emit significantDigitsChanged(property, configSignificantDigits);
    emit switchNotationExponentChanged(property, configMSwitchNotationExponent);
    emit valueChanged(property, configValue);
    emit propertyChanged(property);
}


void QtConfigurableScientificDoublePropertyManager::setSingleStep(
        QtProperty *property, const double &singleStep)
{
    if (!propertyToData.contains(property))
    {
        return;
    }

    QtScientificDoublePropertyManager::setSingleStep(property, singleStep);

    sciDoublePropertyManager->blockSignals(true);
    sciDoublePropertyManager->setValue(propertyToSingleStep[property], singleStep);
    sciDoublePropertyManager->blockSignals(false);
}


void QtConfigurableScientificDoublePropertyManager::setSignificantDigits(
        QtProperty *property, const int &significantDigits)
{
    if (!propertyToData.contains(property))
    {
        return;
    }

    QtScientificDoublePropertyManager::setSignificantDigits(property,
                                                         significantDigits);

    intPropertyManager->blockSignals(true);
    intPropertyManager->setValue(propertyToSignificantDigits[property],
                                 significantDigits);
    intPropertyManager->blockSignals(false);
}


void QtConfigurableScientificDoublePropertyManager::setSwitchNotationExponent(
        QtProperty *property, const int &switchNotationExponent)
{
    if (!propertyToData.contains(property))
    {
        return;
    }

    QtConfigurableScientificDoublePropertyManager::setSwitchNotationExponent(
                property, switchNotationExponent);

    intPropertyManager->blockSignals(true);
    intPropertyManager->setValue(propertyToSignificantDigits[property],
                                 switchNotationExponent);
    intPropertyManager->blockSignals(false);
}


void QtConfigurableScientificDoublePropertyManager::slotIntChanged(
        QtProperty *property, int value)
{
    if (QtProperty *prop = significantDigitsToProperty.value(property, 0))
    {
        setSignificantDigits(prop, value);
    }
    else if (QtProperty *prop = switchNotationExponentToProperty.value(property,
                                                                       0))
    {
        setSwitchNotationExponent(prop, value);
    }
}


void QtConfigurableScientificDoublePropertyManager::slotScientificDoubleChanged(
        QtProperty *property, double value)
{
    if (QtProperty *prop = singleStepToProperty.value(property, 0))
    {
        setSingleStep(prop, value);
    }
}


void QtConfigurableScientificDoublePropertyManager::slotPropertyDestroyed(
        QtProperty *property)
{
    if (QtProperty *prop = singleStepToProperty.value(property, 0))
    {
        propertyToSingleStep.remove(prop);// = 0;
        singleStepToProperty.remove(property);
    }
    else if (QtProperty *prop = significantDigitsToProperty.value(property, 0))
    {
        propertyToSignificantDigits.remove(prop); // = 0;
        significantDigitsToProperty.remove(property);
    }
    else if (QtProperty *prop = switchNotationExponentToProperty.value(property, 0))
    {
//        propertyToSwitchNotationExponent[prop]  = 0;
        propertyToSwitchNotationExponent.remove(prop); // = 0;
        switchNotationExponentToProperty.remove(property);
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void QtConfigurableScientificDoublePropertyManager::initializeProperty(
        QtProperty *property)
{
    QtScientificDoublePropertyManager::initializeProperty(property);

    QtProperty *singleStepProp = sciDoublePropertyManager->addProperty();
    singleStepProp->setPropertyName(tr("single step"));
    sciDoublePropertyManager->setValue(singleStepProp, 0.1);
    sciDoublePropertyManager->setMinimum(singleStepProp, minimum(property));
    propertyToSingleStep[property] = singleStepProp;
    singleStepToProperty[singleStepProp] = property;
    property->addSubProperty(singleStepProp);

    QtProperty *significantDigitsProp = intPropertyManager->addProperty();
    significantDigitsProp->setPropertyName(tr("significant digits"));
    intPropertyManager->setValue(significantDigitsProp, 2);
    intPropertyManager->setMinimum(significantDigitsProp, 0);
    propertyToSignificantDigits[property] = significantDigitsProp;
    significantDigitsToProperty[significantDigitsProp] = property;
    property->addSubProperty(significantDigitsProp);

    QtProperty *switchNotationExponentProp = intPropertyManager->addProperty();
    switchNotationExponentProp->setPropertyName(tr("switch Notation Exponent"));
    intPropertyManager->setValue(switchNotationExponentProp, 1);
    intPropertyManager->setMinimum(switchNotationExponentProp, 0);
    propertyToSwitchNotationExponent[property] = switchNotationExponentProp;
    switchNotationExponentToProperty[switchNotationExponentProp] = property;
    property->addSubProperty(switchNotationExponentProp);
}


void QtConfigurableScientificDoublePropertyManager::uninitializeProperty(
        QtProperty *property)
{
    QtProperty *singleStepProp = propertyToSingleStep[property];
    if (singleStepProp)
    {
        singleStepToProperty.remove(singleStepProp);
        delete singleStepProp;
    }
    propertyToSingleStep.remove(property);

    QtProperty *significantDigitsProp = propertyToSignificantDigits[property];
    if (significantDigitsProp)
    {
        significantDigitsToProperty.remove(significantDigitsProp);
        delete significantDigitsProp;
    }
    propertyToSignificantDigits.remove(property);

    QtProperty *switchNotationExponentProp =
            propertyToSwitchNotationExponent[property];
    if (switchNotationExponentProp)
    {
        switchNotationExponentToProperty.remove(switchNotationExponentProp);
        delete switchNotationExponentProp;
    }
    propertyToSwitchNotationExponent.remove(property);

    QtScientificDoublePropertyManager::uninitializeProperty(property);
}


/******************************************************************************
***                QtScientificDoubleSpinBoxFactoryPrivate                  ***
*******************************************************************************/
/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void QtScientificDoubleSpinBoxFactoryPrivate::slotSetValue(double value)
{
    QObject *object = q_ptr->sender();
    const QMap<MScientificDoubleSpinBox *, QtProperty *>::ConstIterator itcend =
            m_editorToProperty.constEnd();
    for (QMap<MScientificDoubleSpinBox *, QtProperty *>::ConstIterator itEditor =
         m_editorToProperty.constBegin(); itEditor != itcend; ++itEditor)
    {
        if (itEditor.key() == object)
        {
            QtProperty *property = itEditor.value();
            QtScientificDoublePropertyManager *manager =
                    q_ptr->propertyManager(property);
            if (!manager)
            {
                return;
            }
            manager->setValue(property, value);
            return;
        }
    }
}


void QtScientificDoubleSpinBoxFactoryPrivate::slotPropertyChanged(
        QtProperty *property, double value)
{
    QListIterator<MScientificDoubleSpinBox *> itEditor(m_createdEditors[property]);
    while (itEditor.hasNext())
    {
        MScientificDoubleSpinBox *editor = itEditor.next();
        if (editor->value() != value)
        {
            editor->blockSignals(true);
            editor->setValue(value + 1.);
            editor->blockSignals(false);
        }
    }
}


void QtScientificDoubleSpinBoxFactoryPrivate::slotRangeChanged(
        QtProperty *property, double min, double max)
{
    if (!m_createdEditors.contains(property))
    {
        return;
    }

    QtScientificDoublePropertyManager *manager = q_ptr->propertyManager(property);
    if (!manager)
    {
        return;
    }

    QList<MScientificDoubleSpinBox *> editors = m_createdEditors[property];
    QListIterator<MScientificDoubleSpinBox *> itEditor(editors);
    while (itEditor.hasNext())
    {
        MScientificDoubleSpinBox *editor = itEditor.next();
        editor->blockSignals(true);
        editor->setRange(min, max);
        editor->setValue(manager->value(property));
        editor->blockSignals(false);
    }
}


void QtScientificDoubleSpinBoxFactoryPrivate::slotSingleStepChanged(
        QtProperty *property, double step)
{
    if (!m_createdEditors.contains(property))
        return;

    QtScientificDoublePropertyManager *manager = q_ptr->propertyManager(property);
    if (!manager)
        return;

    QList<MScientificDoubleSpinBox *> editors = m_createdEditors[property];
    QListIterator<MScientificDoubleSpinBox *> itEditor(editors);
    while (itEditor.hasNext())
    {
        MScientificDoubleSpinBox *editor = itEditor.next();
        editor->blockSignals(true);
        editor->setSingleStep(step);
        editor->blockSignals(false);
    }
}


void QtScientificDoubleSpinBoxFactoryPrivate::slotReadOnlyChanged(
        QtProperty *property, bool readOnly)
{
    if (!m_createdEditors.contains(property))
        return;

    QtScientificDoublePropertyManager *manager = q_ptr->propertyManager(property);
    if (!manager)
        return;

    QListIterator<MScientificDoubleSpinBox *> itEditor(m_createdEditors[property]);
    while (itEditor.hasNext())
    {
        MScientificDoubleSpinBox *editor = itEditor.next();
        editor->blockSignals(true);
        editor->setReadOnly(readOnly);
        editor->blockSignals(false);
    }
}


void QtScientificDoubleSpinBoxFactoryPrivate::slotMinimumExponentChanged(
        QtProperty *property, int minimumExponent)
{
    if (!m_createdEditors.contains(property))
        return;

    QtScientificDoublePropertyManager *manager = q_ptr->propertyManager(property);
    if (!manager)
        return;

    QList<MScientificDoubleSpinBox *> editors = m_createdEditors[property];
    QListIterator<MScientificDoubleSpinBox *> itEditor(editors);
    while (itEditor.hasNext())
    {
        MScientificDoubleSpinBox *editor = itEditor.next();
        editor->blockSignals(true);
        editor->setDecimals(minimumExponent);
        editor->setValue(manager->value(property));
        editor->blockSignals(false);
    }
}


void QtScientificDoubleSpinBoxFactoryPrivate::slotSwitchNotationExponentChanged(
        QtProperty *property, int exponent)
{
    if (!m_createdEditors.contains(property))
    {
        return;
    }

    QtScientificDoublePropertyManager *manager = q_ptr->propertyManager(property);
    if (!manager)
    {
        return;
    }

    QList<MScientificDoubleSpinBox *> editors = m_createdEditors[property];
    QListIterator<MScientificDoubleSpinBox *> itEditor(editors);
    while (itEditor.hasNext())
    {
        MScientificDoubleSpinBox *editor = itEditor.next();
        editor->blockSignals(true);
        editor->setSwitchNotationExponent(exponent);
        editor->setValue(manager->value(property));
        editor->blockSignals(false);
    }
}


void QtScientificDoubleSpinBoxFactoryPrivate::slotSignificantDigitsChanged(
        QtProperty *property, int significantDigits)
{
    if (!m_createdEditors.contains(property))
    {
        return;
    }

    QtScientificDoublePropertyManager *manager = q_ptr->propertyManager(property);
    if (!manager)
    {
        return;
    }

    QList<MScientificDoubleSpinBox *> editors = m_createdEditors[property];
    QListIterator<MScientificDoubleSpinBox *> itEditor(editors);
    while (itEditor.hasNext())
    {
        MScientificDoubleSpinBox *editor = itEditor.next();
        editor->blockSignals(true);
        editor->setSignificantDigits(significantDigits);
        editor->setValue(manager->value(property));
        editor->blockSignals(false);
    }
}


void QtScientificDoubleSpinBoxFactoryPrivate::slotPrefixChanged(
        QtProperty *property, const QString &prefix)
{
    if (!m_createdEditors.contains(property))
    {
        return;
    }

    QtScientificDoublePropertyManager *manager = q_ptr->propertyManager(property);
    if (!manager)
    {
        return;
    }

    QList<MScientificDoubleSpinBox *> editors = m_createdEditors[property];
    QListIterator<MScientificDoubleSpinBox *> itEditor(editors);
    while (itEditor.hasNext())
    {
        MScientificDoubleSpinBox *editor = itEditor.next();
        editor->blockSignals(true);
        editor->setPrefix(prefix);
        editor->setValue(manager->value(property));
        editor->blockSignals(false);
    }
}


void QtScientificDoubleSpinBoxFactoryPrivate::slotSuffixChanged(
        QtProperty *property, const QString &suffix)
{
    if (!m_createdEditors.contains(property))
    {
        return;
    }

    QtScientificDoublePropertyManager *manager = q_ptr->propertyManager(property);
    if (!manager)
    {
        return;
    }

    QList<MScientificDoubleSpinBox *> editors = m_createdEditors[property];
    QListIterator<MScientificDoubleSpinBox *> itEditor(editors);
    while (itEditor.hasNext())
    {
        MScientificDoubleSpinBox *editor = itEditor.next();
        editor->blockSignals(true);
        editor->setSuffix(suffix);
        editor->setValue(manager->value(property));
        editor->blockSignals(false);
    }
}


/******************************************************************************
***                    QtScientificDoubleSpinBoxFactory                     ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

QtScientificDoubleSpinBoxFactory::QtScientificDoubleSpinBoxFactory(
        QObject *parent)
    : QtAbstractEditorFactory<QtScientificDoublePropertyManager>(parent)
{
    d_ptr = new QtScientificDoubleSpinBoxFactoryPrivate();
    d_ptr->q_ptr = this;
}


QtScientificDoubleSpinBoxFactory::~QtScientificDoubleSpinBoxFactory()
{
    qDeleteAll(d_ptr->m_editorToProperty.keys());
    delete d_ptr;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void QtScientificDoubleSpinBoxFactory::connectPropertyManager(
        QtScientificDoublePropertyManager *manager)
{
    connect(manager, SIGNAL(valueChanged(QtProperty *, double)),
            this, SLOT(slotPropertyChanged(QtProperty *, double)));
    connect(manager, SIGNAL(rangeChanged(QtProperty *, double, double)),
            this, SLOT(slotRangeChanged(QtProperty *, double, double)));
    connect(manager, SIGNAL(singleStepChanged(QtProperty *, double)),
            this, SLOT(slotSingleStepChanged(QtProperty *, double)));
    connect(manager, SIGNAL(minimumExponentChanged(QtProperty *, int)),
            this, SLOT(slotMinimumExponentChanged(QtProperty *, int)));
    connect(manager, SIGNAL(switchNotationExponentChanged(QtProperty *, int)),
            this, SLOT(slotSwitchNotationExponentChanged(QtProperty *, int)));
    connect(manager, SIGNAL(significantDigitsChanged(QtProperty *, int)),
            this, SLOT(slotSignificantDigitsChanged(QtProperty *, int)));
    connect(manager, SIGNAL(prefixChanged(QtProperty *, const QString &)),
            this, SLOT(slotPrefixChanged(QtProperty *, const QString &)));
    connect(manager, SIGNAL(suffixChanged(QtProperty *, const QString &)),
            this, SLOT(slotSuffixChanged(QtProperty *, const QString &)));
    connect(manager, SIGNAL(readOnlyChanged(QtProperty *, bool)),
            this, SLOT(slotReadOnlyChanged(QtProperty *, bool)));
}


QWidget *QtScientificDoubleSpinBoxFactory::createEditor(
        QtScientificDoublePropertyManager *manager,
        QtProperty *property, QWidget *parent)
{
    MScientificDoubleSpinBox *editor = d_ptr->createEditor(property, parent);
    editor->setSingleStep(manager->singleStep(property));
    editor->setDecimals(manager->minimumExponent(property)); // Minimum Exponent.
    editor->setSwitchNotationExponent(manager->switchNotationExponent(property));
    editor->setSignificantDigits(manager->significantDigits(property));
    editor->setRange(manager->minimum(property), manager->maximum(property));
    editor->setPrefix(manager->prefix(property));
    editor->setSuffix(manager->suffix(property));
    editor->setValue(manager->value(property));
    editor->setKeyboardTracking(false);
    editor->setReadOnly(manager->isReadOnly(property));

    connect(editor, SIGNAL(valueChanged(double)),
            this, SLOT(slotSetValue(double)));
    connect(editor, SIGNAL(destroyed(QObject *)),
            this, SLOT(slotEditorDestroyed(QObject *)));
    return editor;
}


void QtScientificDoubleSpinBoxFactory::disconnectPropertyManager(
        QtScientificDoublePropertyManager *manager)
{
    disconnect(manager, SIGNAL(valueChanged(QtProperty *, double)),
               this, SLOT(slotPropertyChanged(QtProperty *, double)));
    disconnect(manager, SIGNAL(rangeChanged(QtProperty *, double, double)),
               this, SLOT(slotRangeChanged(QtProperty *, double, double)));
    disconnect(manager, SIGNAL(singleStepChanged(QtProperty *, double)),
               this, SLOT(slotSingleStepChanged(QtProperty *, double)));
    disconnect(manager, SIGNAL(minimumExponentChanged(QtProperty *, int)),
               this, SLOT(slotMinimumExponentChanged(QtProperty *, int)));
    disconnect(manager, SIGNAL(switchNotationExponentChanged(QtProperty*,int)),
               this, SLOT(slotSwitchNotationExponentChanged(QtProperty *, int)));
    disconnect(manager, SIGNAL(significantDigitsChanged(QtProperty *, int)),
               this, SLOT(slotSignificantDigitsChanged(QtProperty *, int)));
    disconnect(manager, SIGNAL(prefixChanged(QtProperty *, const QString &)),
               this, SLOT(slotPrefixChanged(QtProperty *, const QString &)));
    disconnect(manager, SIGNAL(suffixChanged(QtProperty *, const QString &)),
               this, SLOT(slotSuffixChanged(QtProperty *, const QString &)));
    disconnect(manager, SIGNAL(readOnlyChanged(QtProperty *, bool)),
               this, SLOT(slotReadOnlyChanged(QtProperty *, bool)));
}

} // namespace QtExtensions
