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
#include "mstopwatch.h"

// standard library imports
#include "sys/time.h"

// related third party imports

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MStopwatch::MStopwatch()
{
    // Documentation for timeval and gettimeofday can be found in "man
    // gettimeofday". Returns the current time in microseconds resolution.
    timeval tv;
    gettimeofday(&tv, 0);
    startTime = lastSplitTime = secondToLastSplitTime =
            (long long)(tv.tv_sec) * 1000000LL + (long long)(tv.tv_usec);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

double MStopwatch::getTimeOfDay(const TimeUnits units)
{
    timeval tv;
    gettimeofday(&tv, 0);
    long long timeOfDay =
            (long long)(tv.tv_sec) * 1000000LL + (long long)(tv.tv_usec);

    switch (units)
    {
    case MICROSECONDS:
        return double(timeOfDay);
    case MILLISECONDS:
        return double(timeOfDay) / 1.e3;
    case SECONDS:
        return double(timeOfDay) / 1.e6;
    }
    return -1;
}


void MStopwatch::split()
{
    secondToLastSplitTime = lastSplitTime;
    timeval tv;
    gettimeofday(&tv, 0);
    lastSplitTime =
            (long long)(tv.tv_sec) * 1000000LL + (long long)(tv.tv_usec);
}


double MStopwatch::getElapsedTime(const TimeUnits units)
{
    switch (units)
    {
    case MICROSECONDS:
        return double(lastSplitTime - startTime);
    case MILLISECONDS:
        return double(lastSplitTime - startTime) / 1.e3;
    case SECONDS:
        return double(lastSplitTime - startTime) / 1.e6;
    }
    return -1;
}


double MStopwatch::getLastSplitTime(const TimeUnits units)
{
    switch (units)
    {
    case MICROSECONDS:
        return double(lastSplitTime - secondToLastSplitTime);
    case MILLISECONDS:
        return double(lastSplitTime - secondToLastSplitTime) / 1.e3;
    case SECONDS:
        return double(lastSplitTime - secondToLastSplitTime) / 1.e6;
    }
    return -1;
}

} // namespace Met3D
