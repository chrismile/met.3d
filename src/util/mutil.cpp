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

#include "mutil.h"

// standard library imports
#include <iostream>

// related third party imports
#include "GL/glew.h"
#include <log4cplus/loggingmacros.h>

// local application imports

using namespace std;


/******************************************************************************
***                             LOGGING                                     ***
*******************************************************************************/

// Global reference to the Met3D application logger (defined in mutil.cpp).
log4cplus::Logger mlog =
        log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("met3d"));


/******************************************************************************
***                          UTILITY FUNCTIONS                              ***
*******************************************************************************/

void checkOpenGLError(const char* file, int line)
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        if (line >= 0)
        {
            LOG4CPLUS_ERROR(mlog, "OPENGL ERROR # " << gluErrorString(error)
                            << " (line " << line << " in " << file << ")");
        }
        else
        {
            LOG4CPLUS_ERROR(mlog, "OPENGL ERROR # " << gluErrorString(error));
        }
    }
}


QString expandEnvironmentVariables(QString path)
{
    QRegExp regExpEnvVar("\\$([A-Za-z0-9_]+)");

    int i;
    while ((i = regExpEnvVar.indexIn(path)) != -1)
    {
        QString envVar = regExpEnvVar.cap(1);
        QString expansion = QProcessEnvironment::systemEnvironment().value(
                    envVar);

        if (expansion.isEmpty())
        {
            LOG4CPLUS_ERROR(mlog, "ERROR: Environment variable "
                            << envVar.toStdString()
                            << " has not been defined. Cannot expand variable.");
            break;
        }
        else
        {
            path.remove(i, regExpEnvVar.matchedLength());
            path.insert(i, expansion);
        }
    }

    return path;
}
