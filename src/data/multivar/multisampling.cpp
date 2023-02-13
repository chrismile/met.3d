/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2022 Christoph Neuhauser
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

// standard library imports
#include <cmath>
#include <algorithm>

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "multisampling.h"

#ifdef __linux__
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#endif

namespace Met3D
{

int getMaxSamplesGLImpl(int desiredSamples) {
#ifdef __linux__
    typedef Display* (*PFN_XOpenDisplay)(_Xconst char*);
    typedef GLXFBConfig* (*PFN_glXChooseFBConfig)(Display* dpy, int screen, const int* attribList, int* nitems);
    typedef int (*PFN_glXGetFBConfigAttrib)(Display* dpy, GLXFBConfig config, int attribute, int* value);

    void* libX11so = dlopen("libX11.so", RTLD_NOW | RTLD_LOCAL);
    if (!libX11so) {
        LOG4CPLUS_ERROR(mlog, "Error in getMaxSamplesGLImpl: Could not load libX11.so!");
        return 1;
    }
    void* libGLXso = dlopen("libGLX.so", RTLD_NOW | RTLD_LOCAL);
    if (!libGLXso) {
        LOG4CPLUS_ERROR(mlog, "Error in getMaxSamplesGLImpl: Could not load libGLX.so!");
        dlclose(libX11so);
        return 1;
    }

    auto dyn_XOpenDisplay = PFN_XOpenDisplay(dlsym(libX11so, "XOpenDisplay"));
    if (!dyn_XOpenDisplay) {
        LOG4CPLUS_ERROR(mlog, "Error in getMaxSamplesGLImpl: Could not load function from libX11.so!");
        dlclose(libGLXso);
        dlclose(libX11so);
        return 1;
    }

    auto dyn_glXChooseFBConfig = PFN_glXChooseFBConfig(dlsym(libGLXso, "glXChooseFBConfig"));
    auto dyn_glXGetFBConfigAttrib = PFN_glXGetFBConfigAttrib(dlsym(libGLXso, "glXGetFBConfigAttrib"));
    if (!dyn_glXChooseFBConfig || !dyn_glXGetFBConfigAttrib) {
        LOG4CPLUS_ERROR(mlog, "Error in getMaxSamplesGLImpl: Could not load functions from libGLX.so!");
        dlclose(libGLXso);
        dlclose(libX11so);
        return 1;
    }


    const char* displayName = ":0";
    Display* display;
    if (!(display = dyn_XOpenDisplay(displayName))) {
        LOG4CPLUS_ERROR(mlog, "Error in getMaxSamplesGLImpl: Couldn't open X11 display!");
        dlclose(libGLXso);
        dlclose(libX11so);
        return 1;
    }
    int defscreen = DefaultScreen(display);

    int nitems;
    GLXFBConfig* fbcfg = dyn_glXChooseFBConfig(display, defscreen, NULL, &nitems);
    if (!fbcfg) {
        LOG4CPLUS_ERROR(mlog, "Error in getMaxSamplesGLImpl: Couldn't get FB configs!");
        dlclose(libGLXso);
        dlclose(libX11so);
        return 1;
    }

    // https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glXGetFBConfigAttrib.xml
    int maxSamples = 0;
    for (int i = 0; i < nitems; ++i) {
        int samples = 0;
        dyn_glXGetFBConfigAttrib(display, fbcfg[i], GLX_SAMPLES, &samples);
        if (samples > maxSamples) {
            maxSamples = samples;
        }
    }

    LOG4CPLUS_ERROR(mlog, "Maximum OpenGL multisamples (GLX): " + std::to_string(maxSamples));

    dlclose(libGLXso);
    dlclose(libX11so);

    return std::min(maxSamples, desiredSamples);
#else
    return desiredSamples;
#endif
}

}
