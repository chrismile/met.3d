/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2017      Bianca Tost
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
#ifndef CAMERA_H
#define CAMERA_H

// standard library imports
#include <limits>

// related third party imports
#include <QVector3D>
#include <QMatrix4x4>
#include <QtCore>

// local application imports

namespace Met3D
{

/**
  MCamera implements ... a camera class!
  */
class MCamera
{
public:
    /**
      The default constructor places the camera at the origin, looking down the
      positive z axis (right handed coordinate system).
      */
    MCamera()
        : origin(QVector3D(0.,0.,0.)),
          yAxis(QVector3D(0.,1.,0.)),
          zAxis(QVector3D(0.,0.,-1.)) { }

    /**
      Origin (i.e. position) of the camera in world space.
      */
    QVector3D getOrigin() { return origin; }

    /**
      Upward pointing axis of the camera system in world space coordinates.
      */
    QVector3D getYAxis() { return yAxis; }

    /**
      Forward looking axis of the camera system in world space coordinates.
      */
    QVector3D getZAxis() { return zAxis; }

    /**
      Rightward pointing axis of the camera system in world space coordinates.
      */
    QVector3D getXAxis() { return QVector3D::crossProduct(zAxis, yAxis); }

    /**
      Matrix that transforms coordinates in world space to coordinates in
      camera space.
      */
    QMatrix4x4 getViewMatrix();

    /**
      Matrix that transforms coordinates in camera space to coordinates in
      world space.

      @param rotationOnly If set to true, omit the translation.
      */
    QMatrix4x4 getInverseViewMatrix(bool rotationOnly=false);

    /**
      Origin (i.e. position) of the camera in world space.
      */
    void setOrigin(QVector3D v) { origin = v; }

    /**
      Upward pointing axis of the camera system in world space coordinates.
      */
    void setYAxis(QVector3D v) { yAxis = v.normalized(); }

    /**
      Forward looking axis of the camera system in world space coordinates.
      */
    void setZAxis(QVector3D v) { zAxis = v.normalized(); }

    /**
      Move the camera forward in the viewing direction.
      */
    void moveForward(float delta) { origin += zAxis * delta; }

    /**
      Move the camera upward.
      */
    void moveUp(float delta, float minHeight=std::numeric_limits<float>::min());

    /**
      Move the camera to the right.
      */
    void moveRight(float delta);

    /**
      Rotate the camera system around a local rotation axis. The vector (x,y,z)
      denotes the rotation axis in camera space, e.g. (1,0,0) would be a
      rotation around the x-axis of the camera (pitch if considered as a Euler
      angle).

      @param angle Rotation angle in degrees.
      @param x X-component of local rotation axis.
      @param y Y-component of local rotation axis.
      @param z Z-component of local rotation axis.
      */
    void rotate(float angle, float x, float y, float z);

    /**
      Rotate the camera system around a rotation axis defined in world space.
      The vector (x,y,z) denotes the rotation axis, e.g. (0,0,1) would be a
      rotation around the z-axis of the world.

      @param angle Rotation angle in degrees.
      @param x X-component of rotation axis, defined in world space.
      @param y Y-component of rotation axis, defined in world space.
      @param z Z-component of rotation axis, defined in world space.
      */
    void rotateWorldSpace(float angle, float x, float y, float z);

    void saveConfigurationToFile(QString filename = "");
    void loadConfigurationFromFile(QString filename  = "");

    void saveConfiguration(QSettings *settings);
    void loadConfiguration(QSettings *settings);

protected:
    QVector3D origin;  // Position of the camera in world space.
    QVector3D yAxis;   // y axis of camera space = upward direction.
    QVector3D zAxis;   // z axis of camera space = foward direction.
};

} // namespace Met3D

#endif // CAMERA_H
