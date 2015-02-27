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
#ifndef PRESSUREPOLEACTOR_H
#define PRESSUREPOLEACTOR_H

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
  @brief MMovablePoleActor implements vertical axes ("poles") that are labelled
  and can be interactively moved by the user in interaction mode.
  */
class MMovablePoleActor : public MActor
{
public:
    MMovablePoleActor();
    ~MMovablePoleActor();

    void reloadShaderEffects();

    int checkIntersectionWithHandle(MSceneViewGLWidget *sceneView,
                          float clipX, float clipY, float clipRadius);

    /**
      @todo Currently uses the worldZ==0 plane, make this work with the
            worldZ==arbitrary.
     */
    void dragEvent(MSceneViewGLWidget *sceneView,
                   int handleID, float clipX, float clipY);

    void addPole(QPointF pos);

    QString getSettingsID() override { return "PressurePoleActor"; }

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

protected:
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

private:
    void generateGeometry();

    std::shared_ptr<GL::MShaderEffect> simpleGeometryEffect;
    std::shared_ptr<GL::MShaderEffect> positionSpheresShader;

    QVector<QVector3D> poleVertices;
    GL::MVertexBuffer* poleVertexBuffer;
    QVector<QVector3D> axisTicks;
    GL::MVertexBuffer* axisVertexBuffer;

    QtProperty *bottomPressureProperty;
    QtProperty *topPressureProperty;
    float       pbot_hPa;
    float       ptop_hPa;
    QtProperty *tickLengthProperty;
    float       tickLength;

    QtProperty *colourProperty;
    QColor      lineColour;

    QtProperty *addPoleProperty;

    struct PoleSettings
    {
        PoleSettings(MActor *actor = nullptr);
        QtProperty *groupProperty;
        QtProperty *positionProperty;
        QtProperty *removePoleProperty;
    };

    QVector<PoleSettings> poleProperties;

    /** This variable stores the ID of a pole to be highlighted. If the value
        is -1, no waypoint will be highlighted. */
    int highlightPole;
};


class MPressurePoleActorFactory : public MAbstractActorFactory
{
public:
    MPressurePoleActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override { return new MMovablePoleActor(); }
};


} // namespace Met3D

#endif // PRESSUREPOLEACTOR_H
