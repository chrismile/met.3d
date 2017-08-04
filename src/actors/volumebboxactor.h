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
#ifndef VOLUMEBOXACTOR_H
#define VOLUMEBOXACTOR_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <QtCore>
#include <QtProperty>

// local application imports
#include "gxfw/mactor.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/vertexbuffer.h"

class MGLResourcesManager;
class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief MVolumeBoundingBoxActor draws a bounding box into the scene that
  visualises the limits of a data volume.

  @todo button "get bounding box from actor XY" (checkbox that lists the other
  actors, if one is selected re-set to "choose" or something -- button is not
  possible in properties browser -- that is.. something similar to QtDesigner
  with the toolbuttons?).
  */
class MVolumeBoundingBoxActor : public MActor
{
public:
    MVolumeBoundingBoxActor();

    ~MVolumeBoundingBoxActor();

    void reloadShaderEffects();

    /**
      Set a horizontal bounding box for the region (lonEast, latSouth, width,
      height).
      */
    void setBBox(QRectF bbox);

    /**
      Set the colour of the lines.
     */
    void setColour(QColor c);

    QString getSettingsID() override { return "VolumeBoundingBoxActor"; }

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

    QRectF getBBox();
    double getBottomPressure();
    double getTopPressure();

protected:
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

private:
    void generateGeometry();

    std::shared_ptr<GL::MShaderEffect> geometryEffect;

    QVector<QVector3D> coordinateSystemVertices;
    GL::MVertexBuffer* coordinateVertexBuffer;
    QVector<QVector3D> axisTicks;
    GL::MVertexBuffer* axisVertexBuffer;

    QtProperty *boxCornersProperty;
    QtProperty *bottomPressureProperty;
    QtProperty *topPressureProperty;
    QtProperty *tickLengthProperty;
    float       tickLength;

    QtProperty *colourProperty;
    QColor      lineColour;
};


class MVolumeBoundingBoxActorFactory : public MAbstractActorFactory
{
public:
    MVolumeBoundingBoxActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override { return new MVolumeBoundingBoxActor(); }
};


} // namespace Met3D
#endif // VOLUMEBOXACTOR_H
