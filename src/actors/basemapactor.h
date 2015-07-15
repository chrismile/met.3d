/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Michael Kern
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

// local application imports
#include "gxfw/mactor.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/texture.h"


class MGLResourcesManager;
class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief MBaseMapActor draws a map into the scene. Map raster data is loaded
  from a GeoTiff file.
  */
class MBaseMapActor : public MActor
{
public:
    MBaseMapActor();
    ~MBaseMapActor();

    void reloadShaderEffects();

    /**
      Set a horizontal bounding box for the region (lonEast, latSouth, width,
      height).
      */
    void setBBox(QRectF bbox);

    /**
      Set the filename for a GeoTIFF file from which the map data is loaded.
     */
    void setFilename(QString filename);

    QString getSettingsID() override { return "BaseMapActor"; }

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

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

    std::shared_ptr<GL::MShaderEffect> shaderProgram;

    //GLuint textureObjectName;
    GL::MTexture* texture;
    int textureUnit;

    uint numVertices;

    QtProperty* filenameProperty;
    QtProperty* loadMapProperty;

    // Bounding box.
    QRectF      bbox;
    QtProperty* bboxProperty;

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
