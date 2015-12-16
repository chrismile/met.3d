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
#include "qtproperties.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports

using namespace QtExtensions;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MQtProperties::MQtProperties()
{
    groupPropertyManager           = new QtGroupPropertyManager();
    boolPropertyManager            = new QtBoolPropertyManager();
    intPropertyManager             = new QtIntPropertyManager();
    doublePropertyManager          = new QtDoublePropertyManager();
    decoratedDoublePropertyManager = new QtDecoratedDoublePropertyManager();
    dateTimePropertyManager        = new QtDateTimePropertyManager();
    enumPropertyManager            = new QtEnumPropertyManager();
    rectFPropertyManager           = new QtRectFPropertyManager();
    pointFPropertyManager          = new QtPointFPropertyManager();
    colorPropertyManager           = new QtColorPropertyManager();
    stringPropertyManager          = new QtStringPropertyManager();
    clickPropertyManager           = new QtClickPropertyManager();
}


MQtProperties::~MQtProperties()
{
    delete groupPropertyManager;
    delete boolPropertyManager;
    delete intPropertyManager;
    delete doublePropertyManager;
    delete decoratedDoublePropertyManager;
    delete dateTimePropertyManager;
    delete enumPropertyManager;
    delete rectFPropertyManager;
    delete pointFPropertyManager;
    delete colorPropertyManager;
    delete stringPropertyManager;
    delete clickPropertyManager;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QtGroupPropertyManager* MQtProperties::mGroup()
{
    return groupPropertyManager;
}


QtBoolPropertyManager* MQtProperties::mBool()
{
    return boolPropertyManager;
}


QtIntPropertyManager* MQtProperties::mInt()
{
    return intPropertyManager;
}


QtDoublePropertyManager* MQtProperties::mDouble()
{
    return doublePropertyManager;
}


QtDecoratedDoublePropertyManager* MQtProperties::mDecoratedDouble()
{
    return decoratedDoublePropertyManager;
}


QtDecoratedDoublePropertyManager *MQtProperties::mDDouble()
{
    return decoratedDoublePropertyManager;
}


QtDateTimePropertyManager* MQtProperties::mDateTime()
{
    return dateTimePropertyManager;
}


QtEnumPropertyManager* MQtProperties::mEnum()
{
    return enumPropertyManager;
}


QtRectFPropertyManager* MQtProperties::mRectF()
{
    return rectFPropertyManager;
}


QtPointFPropertyManager* MQtProperties::mPointF()
{
    return pointFPropertyManager;
}


QtColorPropertyManager* MQtProperties::mColor()
{
    return colorPropertyManager;
}


QtStringPropertyManager* MQtProperties::mString()
{
    return stringPropertyManager;
}


QtClickPropertyManager* MQtProperties::mClick()
{
    return clickPropertyManager;
}


void MQtProperties::setDouble(
        QtProperty *prop, double value, double min, double max,
        int decimals, double singlestep)
{
    mDouble()->setRange(prop, min, max);
    mDouble()->setDecimals(prop, decimals);
    mDouble()->setSingleStep(prop, singlestep);
    mDouble()->setValue(prop, value);
}


void MQtProperties::setDouble(
        QtProperty *prop, double value,
        int decimals, double singlestep)
{
    mDouble()->setDecimals(prop, decimals);
    mDouble()->setSingleStep(prop, singlestep);
    mDouble()->setValue(prop, value);
}


void MQtProperties::setDDouble(
        QtProperty *prop, double value, double min, double max,
        int decimals, double singlestep, QString suffix)
{
    mDecoratedDouble()->setRange(prop, min, max);
    mDecoratedDouble()->setDecimals(prop, decimals);
    mDecoratedDouble()->setSingleStep(prop, singlestep);
    mDecoratedDouble()->setValue(prop, value);
    mDecoratedDouble()->setSuffix(prop, suffix);
}


void MQtProperties::setInt(
        QtProperty *prop, int value, int min, int max, int step)
{
    mInt()->setRange(prop, min, max);
    mInt()->setSingleStep(prop, step);
    mInt()->setValue(prop, value);
}


void MQtProperties::setRectF(QtProperty *prop, QRectF value, int decimals)
{
    mRectF()->setValue(prop, value);
    mRectF()->setDecimals(prop, decimals);
}


void MQtProperties::setPointF(QtProperty *prop, QPointF value, int decimals)
{
    mPointF()->setValue(prop, value);
    mPointF()->setDecimals(prop, decimals);
}


bool MQtProperties::setEnumItem(QtProperty *prop, QString entry)
{
    QStringList items = mEnum()->enumNames(prop);
    int index = items.indexOf(entry);
    if (index == -1) return false; // no item has been matched
    mEnum()->setValue(prop, index);
    return true; // item successfully set
}


QString MQtProperties::getEnumItem(QtProperty *prop)
{
    int index = mEnum()->value(prop);
    if (index < 0) return QString();
    return mEnum()->enumNames(prop)[index];
}


QStringList MQtProperties::getEnumItems(QtProperty *prop)
{
    return mEnum()->enumNames(prop);
}


} // namespace Met3D
