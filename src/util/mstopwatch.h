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
#ifndef MSTOPWATCH_H
#define MSTOPWATCH_H

// standard library imports

// related third party imports

namespace Met3D
{

/**
   @brief MStopwatch implements a stopwatch to measure the elapsed time at
   different points in a program.

   It can be used to obtain information about the execution time of individual
   methods or the entire program. Uses the Linux gettimeofday() method to
   provide microsecond resolution.
 */
class MStopwatch
{
public:
    enum TimeUnits
    {
        MICROSECONDS = 0,
        MILLISECONDS = 1,
        SECONDS = 2
    };

    /**
     Constructs a new stopwatch and starts it. Time elapsed from the time the
     constructor was called can be obtained with a call to @ref split(),
     followed by a call to @ref getElapsedTime().
     */
    MStopwatch();

    /**
     Returns the current time of the day.
     */
    static double getTimeOfDay(const TimeUnits units);

    /**
     Record the time of a split mark that can be queried with @ref
     getElapsedTime() or @ref getLastSplitTime().
     */
    void split();

    /**
     Returns the time elapsed between the call to the constructor and the last
     call to @ref split().

     @param units TimeUnits enum that specifies the units of the returned time
                  value (e.g. seconds or microseconds).
     */
    double getElapsedTime(const TimeUnits units);

    /**
     Returns the time elapsed between the last two calls to @ref split(). If
     @ref split() has only been called once since object contruction, the time
     elapsed between object contruction and the call to @ref split() is
     returned.

     @param units TimeUnits enum that specifies the units of the returned time
                  value (e.g. seconds or microseconds).
     */
    double getLastSplitTime(const TimeUnits units);

private:
    long long startTime, lastSplitTime, secondToLastSplitTime;
};

} // namespace Met3D

#endif // MSTOPWATCH_H
