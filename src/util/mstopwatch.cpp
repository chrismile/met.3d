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
#ifdef __unix
#include "sys/time.h"
#elif WIN32

#include <Windows.h>
#include <cstdint>

//http ://stackoverflow.com/questions/10905892/equivalent-of-gettimeday-for-windows
//struct timeval
//{
//	long tv_sec;
//	long tv_usec;
//};

int gettimeofday(struct timeval* tp, struct timezone* tzp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}

#endif



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
