/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015-2017 Bianca Tost
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
#ifndef GRATICULEACTOR_H
#define GRATICULEACTOR_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <QVector>
#include <QVector3D>
#include <QtProperty>

// local application imports
#include "gxfw/rotatedgridsupportingactor.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/vertexbuffer.h"
#include "data/naturalearthdataloader.h"

class MGLResourcesManager;
class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief GraticuleActor draws a graticule into a scene. Horizontal extent,
  spacing of latitude / longitude lines and colour parameters are customisable
  through the scene's property browser. The vertical position of the graticule
  can be set programmatically through the @ref setVerticalPosition() method
  (cannot be changed by the user; the method is to sync a graticule with
  an hsec actor).
  */
class MGraticuleActor : public MRotatedGridSupportingActor
{
public:
    MGraticuleActor();

    ~MGraticuleActor();

    void reloadShaderEffects();

    /**
      Set a horizontal bounding box for the region (lonEast, latSouth, width,
      height).
      */
    void setBBox(QRectF bbox);

    /**
      Set the vertical position of the graticule in pressure coordinates.
      */
    void setVerticalPosition(double pressure_hPa);

    /**
      Set the colour of the graticule and coast lines.
     */
    void setColour(QColor c);

    QString getSettingsID() override { return "GraticuleActor"; }

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

protected:
    /**
      Loads the shader programs and generates the graticule geometry. The
      geometry is uploaded to the GPU into a vertex buffer object.
      */
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

private:
    /**
      Generates the graticule geometry and uploads it to a vertex buffer.
      */
    void generateGeometry();

    std::shared_ptr<GL::MShaderEffect> shaderProgram;

    GL::MVertexBuffer* graticuleVertexBuffer;
    uint   numVerticesGraticule;

    QVector<int> nLats;
    QVector<int> nLons;

    MNaturalEarthDataLoader *naturalEarthDataLoader;

    GL::MVertexBuffer* coastlineVertexBuffer;
    QVector<int> coastlineStartIndices;
    QVector<int> coastlineVertexCount;

    GL::MVertexBuffer* borderlineVertexBuffer;
    QVector<int> borderlineStartIndices;
    QVector<int> borderlineVertexCount;

    QtProperty *cornersProperty;
    QtProperty *spacingProperty;
    QtProperty *colourProperty;
    QColor graticuleColour;

    bool drawGraticule;
    QtProperty *drawGraticuleProperty;
    bool drawCoastLines;
    QtProperty *drawCoastLinesProperty;
    bool drawBorderLines;
    QtProperty *drawBorderLinesProperty;

    float verticalPosition_hPa;

    // Inidcate whether coastlineVertexCount/borderlineVertexCount contains at
    // least one value greater and therefore is valid to be used as count array
    // during rendering. Using a count array filled with zeros leads to an
    // program crash.
    bool coastLinesCountIsValid;
    bool borderLinesCountIsValid;
};


class MGraticuleActorFactory : public MAbstractActorFactory
{
public:
    MGraticuleActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override { return new MGraticuleActor(); }
};


} // namespace Met3D

#endif // GRATICULEACTOR_H
