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


QStringList readConfigVersionID(QSettings *settings)
{
    QStringList groupList = settings->group().split("/",
                                                    QString::SkipEmptyParts);
    // To get to FileFormat group end all current groups.
    for (int i = 0; i < groupList.size(); i++)
    {
        settings->endGroup();
    }

    settings->beginGroup("FileFormat");
    QString versionString = (settings->value("met3dVersion",
                                             defaultConfigVersion).toString());
    settings->endGroup();

    // Remove appendix.
    QStringList versionList = versionString.split("-");
    versionList = versionList.first().split(".");

    // Restore group state from before entering this function.
    foreach (QString groupName, groupList)
    {
        settings->beginGroup(groupName);
    }

    return versionList;
}


QString expandEnvironmentVariables(QString path)
{
    QRegExp regExpEnvVar("\\$([A-Za-z0-9_]+)");

    int i;
    while ((i = regExpEnvVar.indexIn(path)) != -1)
    {
        QString envVar = regExpEnvVar.cap(1);
        QString expansion;
        if (envVar == "$HOME")
        {
            // Special case "$HOME" environment variable: use QDir::homePath()
            // to obtain correct home directory on all platforms.
            expansion = QDir::homePath();
        }
        else
        {
            expansion = QProcessEnvironment::systemEnvironment().value(
                                envVar);
        }

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


bool isValidObjectName(QString name)
{
    return name != "None";
}


QVector<float> parsePressureLevelString(QString levels)
{
    // Clear the current list of pressure line levels. If pLevelStr does not
    // match any accepted format, no pressure lines are drawn.
    QVector<float> pressureLevels;

    // Empty strings, i.e. no pressure lines, are accepted.
    if (levels.isEmpty())
    {
        return pressureLevels;
    }

    // Match strings of format "[0,100,10]" or "[0.5,10,0.5]".
    QRegExp rxRange("^\\[([\\-|\\+]*\\d+\\.*\\d*),([\\-|\\+]*\\d+\\.*\\d*),"
                            "([\\-|\\+]*\\d+\\.*\\d*)\\]$");
    // Match strings of format "1,2,3,4,5" or "0,0.5,1,1.5,5,10" (number of
    // values is arbitrary).
    QRegExp rxList("^([\\-|\\+]*\\d+\\.*\\d*,*)+$");

    if (rxRange.exactMatch(levels))
    {
        QStringList rangeValues = rxRange.capturedTexts();

        bool ok;
        double from = rangeValues.value(1).toFloat(&ok);
        double to   = rangeValues.value(2).toFloat(&ok);
        double step = rangeValues.value(3).toFloat(&ok);

        if (step > 0)
        {
            for (float d = from; d <= to; d += step)
            {
                pressureLevels << d;
            }
        }
        else if (step < 0)
        {
            for (float d = from; d >= to; d += step)
            {
                pressureLevels << d;
            }
        }
    }
    else if (rxList.exactMatch(levels))
    {
        QStringList listValues = levels.split(",");

        bool ok;
        for (int i = 0; i < listValues.size(); i++)
        {
            pressureLevels << listValues.value(i).toFloat(&ok);
        }
    }

    return pressureLevels;
}


QString encodePressureLevels(QVector<float> levels, QString delimiter)
{
    // Create empty string.
    QString stringValue = QString("");

    // Add values to string with given delimiter.
    for (int i = 0; i < levels.size(); i++)
    {
        if (i == 0)
        {
            stringValue.append(QString("%1").arg(levels.at(i)));
        }
        else
        {
            stringValue.append(QString("%1%2").arg(delimiter).arg(levels.at(i)));
        }
    }

    return stringValue;
}
