/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
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
#include "system/qtproperties.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "gxfw/mscenecontrol.h"

namespace Met3D
{

/*
 Template methods need to be located in (additional) header files in order to
 be visible to the compiler. See:
 https://stackoverflow.com/questions/10632251/undefined-reference-to-template-function
 */

template<typename T> bool MQtProperties::setEnumPropertyClosest(
        const QList<T>& availableValues, const T& value,
        QtProperty *property, bool setSyncColour,
        const QList<MSceneControl*>& scenes)
{
    // Find the value closest to "value" in the list of available values.
    int i = -1; // use of "++i" below
    bool exactMatch = false;
    while (i < availableValues.size()-1)
    {
        // Loop as long as "value" is larger that the currently inspected
        // element (use "++i" to have the same i available for the remaining
        // statements in this block).
        if (value > availableValues.at(++i)) continue;

        // We'll only get here if "value" <= availableValues.at(i). If we
        // have an exact match, break the loop. This is our value.
        if (availableValues.at(i) == value)
        {
            exactMatch = true;
            break;
        }

        // If "value" cannot be exactly matched it lies between indices i-1
        // and i in availableValues. Determine which is closer.
        if (i == 0) break; // if there's no i-1 we're done

//        LOG4CPLUS_DEBUG(mlog, value << " " << availableValues.at(i-1) << " "
//                        << availableValues.at(i) << " "
//                        << abs(value - availableValues.at(i-1)) << " "
//                        << abs(availableValues.at(i) - value) << " ");

        T v1;
        if (value < availableValues.at(i-1)) v1 = availableValues.at(i-1) - value;
        else v1 = value - availableValues.at(i-1);

        T v2;
        if (availableValues.at(i) < value) v2 = value - availableValues.at(i);
        else v2 = availableValues.at(i) - value;

        if ( v1 <= v2 ) i--;

        // "i" now contains the index of the closest available value.
        break;
    }

    if (i > -1)
    {
        // ( Also see updateSyncPropertyColourHints() ).

        // Update background colour of the property in the connected
        // scene's property browser: green if "value" is an
        // exact match with one of the available values, red otherwise.
        if (setSyncColour)
        {
            QColor colour = exactMatch ? QColor(0, 255, 0) : QColor(255, 0, 0);
            foreach (MSceneControl* scene, scenes)
            {
                scene->setPropertyColour(property, colour);
            }
        }

        // Get the currently selected index.
        int currentIndex = static_cast<QtEnumPropertyManager*> (
                    property->propertyManager())->value(property);

        if (i == currentIndex)
        {
            // Index i is already the current one. Nothing needs to be done.
            return false;
        }
        else
        {
            // Set the new value.
            static_cast<QtEnumPropertyManager*> (property->propertyManager())
                    ->setValue(property, i);
            // A new index was set. Return true.
            return true;
        }
    }

    return false;
}

}
