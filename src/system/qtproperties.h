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
#ifndef QTPROPERTIES_H
#define QTPROPERTIES_H

// standard library imports

// related third party imports

// local application imports
#include "qtpropertymanager.h"
#include "qt_extensions/qtpropertymanager_extensions.h"


namespace Met3D
{

enum MQtPropertyType
{
    GROUP_PROPERTY,
    BOOL_PROPERTY,
    INT_PROPERTY,
    DOUBLE_PROPERTY,
    DECORATEDDOUBLE_PROPERTY,
    SCIENTIFICDOUBLE_PROPERTY,
    DATETIME_PROPERTY,
    ENUM_PROPERTY,
    RECTF_LONLAT_PROPERTY,
    RECTF_CLIP_PROPERTY,
    POINTF_PROPERTY,
    POINTF_LONLAT_PROPERTY,
    COLOR_PROPERTY,
    STRING_PROPERTY,
    CLICK_PROPERTY,
    UNDEFINED_PROPERTY
};


/**
  @brief MQtProperties keeps instances of property managers that are needed to
  use the QtPropertyBrowser framework. In addition, some convenience functions
  to get/set property values are provided.
 */
class MQtProperties
{
public:
    MQtProperties();

    virtual ~MQtProperties();

    QtGroupPropertyManager* mGroup();

    QtBoolPropertyManager* mBool();

    QtIntPropertyManager* mInt();

    QtDoublePropertyManager* mDouble();

    QtExtensions::QtDecoratedDoublePropertyManager* mDecoratedDouble();

    QtExtensions::QtDecoratedDoublePropertyManager* mDDouble();

    QtExtensions::QtScientificDoublePropertyManager* mScientificDouble();

    QtExtensions::QtScientificDoublePropertyManager* mSciDouble();

// TODO (bt, 19Jan2018): Add property manager to handle configurable SDProperties.
//  Idea: Use subproperties to provide the feature to change singleStep,
//    switchNotationExponent, etc. via GUI elements.
//     QtExtensions::QtConfigurableScientificDoublePropertyManager*
//     mConfigScientificDouble();

//     QtExtensions::QtConfigurableScientificDoublePropertyManager* mCSciDouble();

    QtDateTimePropertyManager* mDateTime();

    QtEnumPropertyManager* mEnum();

    QtRectFPropertyManager* mRectF();

    QtPointFPropertyManager* mPointF();

    QtColorPropertyManager* mColor();

    QtStringPropertyManager* mString();

    QtExtensions::QtClickPropertyManager* mClick();

    void setDouble(QtProperty* prop, double value,  double min, double max,
                   int decimals, double singlestep);

    void setDouble(QtProperty* prop, double value,
                   int decimals, double singlestep);

    void setDDouble(QtProperty* prop, double value, double min, double max,
                    int decimals, double singlestep, QString suffix);

    void setSciDouble(QtProperty* prop, double value, int significantDigits,
                    double singlestep, int switchNotationExponent=3);

    void setSciDouble(QtProperty* prop, double value, int significantDigits,
                    int minimumExponent, double singlestep,
                    int switchNotationExponent=3);

    void setSciDouble(QtProperty *prop, double value, double min, double max,
                    int significantDigits, double singlestep,
                    int switchNotationExponent=3, QString suffix="",
                    QString prefix="");

    void setSciDouble(QtProperty *prop, double value, double min, double max,
                    int significantDigits, int minimumExponent,
                    double singlestep, int switchNotationExponent=3,
                    QString suffix="", QString prefix="");

    void setInt(QtProperty* prop, int value = 0,
                int min = -100, int max = 100, int step = 1);

    void setRectF(QtProperty* prop, QRectF value, int decimals);

    void setPointF(QtProperty* prop, QPointF value, int decimals);

    bool setEnumItem(QtProperty* prop, QString entry);

    QString getEnumItem(QtProperty* prop);

    QStringList getEnumItems(QtProperty* prop);

    /**
      Updates the list of enum items in @prop and tries to restores the
      previously set item. If this is successful, @p true is returned; if
      the previous item cannot be restored, @p false is returned.
     */
    bool updateEnumItems(QtProperty* prop, const QStringList& names);

private:
    QtGroupPropertyManager    *groupPropertyManager;
    QtBoolPropertyManager     *boolPropertyManager;
    QtIntPropertyManager      *intPropertyManager;
    QtDoublePropertyManager   *doublePropertyManager;
    QtExtensions::QtDecoratedDoublePropertyManager
                              *decoratedDoublePropertyManager;
    QtExtensions::QtScientificDoublePropertyManager
                              *scientificDoublePropertyManager;
    QtDateTimePropertyManager *dateTimePropertyManager;
    QtEnumPropertyManager     *enumPropertyManager;
    QtRectFPropertyManager    *rectFPropertyManager;
    QtPointFPropertyManager   *pointFPropertyManager;
    QtColorPropertyManager    *colorPropertyManager;
    QtStringPropertyManager   *stringPropertyManager;
    QtExtensions::QtClickPropertyManager
                              *clickPropertyManager;
// TODO (bt, 19Jan2018): Add property manager to handle configurable SDProperties.
//  Idea: cf. previous comment concerning this issue.
//    QtExtensions::QtConfigurableScientificDoublePropertyManager
//                              *configurableScientificDoublePropertyManager;
};

} // namespace Met3D

#endif // QTPROPERTIES_H
