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
#include "camera.h"

// standard library imports

// related third party imports
#include <QMessageBox>

// local application imports
#include "util/mutil.h"


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QMatrix4x4 MCamera::getViewMatrix()
{
    // The matrix that transforms from camera space to world space is available
    // from getInverseViewMatrix(). The matrix that transforms from world to
    // camera space (to be computed here) is its inverse. Rotation and
    // translation alone can be easily inverted (rotation: transpose;
    // translation: multiply by -1). Note that the order in which both matrices
    // are multiplied has to be inverted, too. Here we simply multiply both
    // matrices (cf. notes 04Oct11).

    // Literature: Wright, Jr. et al, OpenGL Superbible (5th), p. 139, or
    // Shirley et al., Fundamentals of Computer Graphics, Ch. 6 (Sect. 6.4 for
    // inversion).

    QVector3D xAxis = QVector3D::crossProduct(yAxis, -zAxis);

    // Option 1: Use Qt convenience functions -- slow but simple.
    //return getInverseViewMatrix().inverted();

    // Option 2: Write down inverse matrix directly.
    QMatrix4x4 rotation( xAxis.x(),  xAxis.y(),  xAxis.z(), 0.,
                         yAxis.x(),  yAxis.y(),  yAxis.z(), 0.,
                        -zAxis.x(), -zAxis.y(), -zAxis.z(), 0.,
                         0.,         0.,         0.,        1.);
    QMatrix4x4 translation(1., 0., 0., -origin.x(),
                           0., 1., 0., -origin.y(),
                           0., 0., 1., -origin.z(),
                           0., 0., 0., 1.);
    return rotation * translation;
}


QMatrix4x4 MCamera::getInverseViewMatrix(bool rotationOnly)
{
    QVector3D xAxis = QVector3D::crossProduct(yAxis, -zAxis);
    if (rotationOnly)
        return QMatrix4x4(xAxis.x(), yAxis.x(), -zAxis.x(), 0.,
                          xAxis.y(), yAxis.y(), -zAxis.y(), 0.,
                          xAxis.z(), yAxis.z(), -zAxis.z(), 0.,
                          0.,        0.,         0.,        1.);
    else
        return QMatrix4x4(xAxis.x(), yAxis.x(), -zAxis.x(), origin.x(),
                          xAxis.y(), yAxis.y(), -zAxis.y(), origin.y(),
                          xAxis.z(), yAxis.z(), -zAxis.z(), origin.z(),
                          0.,        0.,         0.,        1.);
}


void MCamera::moveUp(float delta, float minHeight)
{
    origin -= delta * yAxis;
    // If minHeight was specified, limit origin.z() to this value.
    if ((minHeight > std::numeric_limits<float>::min())
            && (origin.z() < minHeight))
        origin.setZ(minHeight);
}


void MCamera::moveRight(float delta)
{
    origin += delta * QVector3D::crossProduct(yAxis, zAxis);
}


void MCamera::rotate(float angle, float x, float y, float z)
{
    // 1. Transform the rotation axis (x,y,z) to world space -- the camera
    // coordinate system (zAxis and yAxis) is given in world space, hence
    // the rotation matrix to transform the camera axes has to be
    // constructed in world space, too.
    QMatrix4x4 cameraToWorldRotation = getInverseViewMatrix(true);
    QVector3D rotationAxisCameraSpace(x, y, z);
    QVector3D rotationAxisWorldSpace =
            cameraToWorldRotation * rotationAxisCameraSpace;

    // 2. Create a rotation matrix around the rotation axis in world space.
    QMatrix4x4 rotationMatrix;
    rotationMatrix.rotate(angle, rotationAxisWorldSpace);

    // 3. Rotate the camera system axes.
    yAxis = rotationMatrix * yAxis;
    zAxis = rotationMatrix * zAxis;
}


void MCamera::rotateWorldSpace(float angle, float x, float y, float z)
{
    // 1. Create a rotation matrix around the rotation axis in world space.
    QMatrix4x4 rotationMatrix;
    rotationMatrix.rotate(angle, x, y, z);

    // 2. Rotate the camera system axes.
    yAxis = rotationMatrix * yAxis;
    zAxis = rotationMatrix * zAxis;
}


void MCamera::saveToFile(QString filename)
{
    if (filename.isEmpty()) return;

    // Overwrite if the file exists.
    if (QFile::exists(filename))
    {
        QSettings* settings = new QSettings(filename, QSettings::IniFormat);

        QStringList groups = settings->childGroups();
        // Only overwrite file if it contains already configuration for the
        // actor to save.
        if ( !groups.contains("MCamera") )
        {
            QMessageBox msg;
            msg.setWindowTitle("Error");
            msg.setText("The selected file contains a configuration other "
                        "than MCamera.\n"
                        "I will NOT overwrite this file -- have you selected "
                        "the correct file?");
            msg.setIcon(QMessageBox::Warning);
            msg.exec();
            return;
        }

        QFile::remove(filename);
    }

    QSettings settings(filename, QSettings::IniFormat);

    settings.beginGroup("FileFormat");
    // Save version id of Met.3D.
    settings.setValue("met3dVersion", met3dVersionString);
    settings.endGroup();

    settings.beginGroup("MCamera");
    settings.setValue("origin_lon", origin.x());
    settings.setValue("origin_lat", origin.y());
    settings.setValue("origin_worldZ", origin.z());
    settings.setValue("yAxis_lon", yAxis.x());
    settings.setValue("yAxis_lat", yAxis.y());
    settings.setValue("yAxis_worldZ", yAxis.z());
    settings.setValue("zAxis_lon", zAxis.x());
    settings.setValue("zAxis_lat", zAxis.y());
    settings.setValue("zAxis_worldZ", zAxis.z());
    settings.endGroup();
}


void MCamera::loadFromFile(QString filename)
{
    if (filename.isEmpty()) return;

    QSettings settings(filename, QSettings::IniFormat);

    settings.beginGroup("MCamera");
    origin.setX(settings.value("origin_lon").toFloat());
    origin.setY(settings.value("origin_lat").toFloat());
    origin.setZ(settings.value("origin_worldZ").toFloat());
    yAxis.setX(settings.value("yAxis_lon").toFloat());
    yAxis.setY(settings.value("yAxis_lat").toFloat());
    yAxis.setZ(settings.value("yAxis_worldZ").toFloat());
    zAxis.setX(settings.value("zAxis_lon").toFloat());
    zAxis.setY(settings.value("zAxis_lat").toFloat());
    zAxis.setZ(settings.value("zAxis_worldZ").toFloat());
    settings.endGroup();
}

} // namespace Met3D
