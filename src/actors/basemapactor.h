/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015      Michael Kern
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
#ifndef MAPACTOR_H
#define MAPACTOR_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <QtProperty>
#include <ogr_geometry.h>

// local application imports
#include "gxfw/rotatedgridsupportingactor.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/texture.h"
#include "gxfw/boundingbox/boundingbox.h"


class MGLResourcesManager;
class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief MBaseMapActor draws a map into the scene. Map raster data is loaded
  from a GeoTiff file.
  */
class MBaseMapActor : public MRotatedGridSupportingActor,
        public MBoundingBoxInterface
{
public:
    MBaseMapActor();
    ~MBaseMapActor();

    static QString staticActorType() { return "Base map"; }

    void reloadShaderEffects();

    /**
      Set the filename for a GeoTIFF file from which the map data is loaded.
     */
    void setFilename(QString filename);

    QString getSettingsID() override { return "BaseMapActor"; }

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

    void onBoundingBoxChanged() override;

protected:
    /**
      Loads the shader programs and generates the graticule geometry. The
      geometry is uploaded to the GPU into a vertex buffer object.
     */
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty* property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

private:
    void loadMap(std::string filename);
    /**
      @brief getBBoxOfRotatedBBox calcuates a rectangle bounding box in
      rotated coordinates covering the region of a complete map.

      @return QVector4D storing coordinates of the corners of the bounding box
      computed stored in the order leftX, lowerY, rightX, upperY.
     */
    QVector4D getBBoxOfRotatedBBox();

    /**
      adaptBBoxForRotatedGrids changes @ref bboxForRotatedGrids to contain
      bounding box coordinates which can be used to determine the fragments of
      the map to be drawn for rotated base map when the bounding box also needs
      to be rotated. (Fragments because the map can fall appart in two or more
      part because of the rotation.)

      Since the rotation maps to the intervals [-180, 180] for longitudes and
      [-90, 90] for latitudes the coordinates of the bounding box also needs to
      be lie in these intervals.
     */
    void adaptBBoxForRotatedGrids();

    std::shared_ptr<GL::MShaderEffect> shaderProgram;

    //GLuint textureObjectName;
    GL::MTexture* texture;
    int textureUnit;

    uint numVertices;

    QtProperty* filenameProperty;
    QtProperty* loadMapProperty;

    // Bounding box
    QVector4D   bboxForRotatedGrids;

    // Bounding box of the loaded map.
    float llcrnrlon;
    float llcrnrlat;
    float urcrnrlon;
    float urcrnrlat;

    // Map image colour (de)saturation.
    GLfloat     colourSaturation;
    QtProperty *colourSaturationProperty;
};


class MBaseMapActorFactory : public MAbstractActorFactory
{
public:
    MBaseMapActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override { return new MBaseMapActor(); }
};


} // namespace Met3D

#endif // MAPACTOR_H
