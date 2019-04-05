/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2017      Michael Kern
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
#include <QVector2D>
#include <QtProperty>

// local application imports
#include "gxfw/mactor.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/vertexbuffer.h"

class MGLResourcesManager;

namespace Met3D
{
	class MSceneViewGLWidget;

class MovablePole
{
public:
    MovablePole(MActor *actor = nullptr);
    QtProperty *groupProperty;
    QtProperty *positionProperty;
    QtProperty *topPressureProperty;
    QtProperty *bottomPressureProperty;
    QtProperty *removePoleProperty;
};

/**
  @brief MMovablePoleActor implements vertical axes ("poles") that are labelled
  and can be interactively moved by the user in interaction mode.
  */
class MMovablePoleActor : public MActor
{
public:
    MMovablePoleActor();
    ~MMovablePoleActor();

    static QString staticActorType() { return "Movable poles"; }

    void reloadShaderEffects();

    void addPole(QPointF pos);
    void addPole(const QVector3D& lonlatP);

    void removeAllPoles();

    QString getSettingsID() override { return "PressurePoleActor"; }

    int checkIntersectionWithHandle(MSceneViewGLWidget *sceneView,
                                    float clipX, float clipY);

    void addPositionLabel(MSceneViewGLWidget *sceneView, int handleID,
                          float clipX, float clipY);

    /**
      @todo Currently uses the worldZ==0 plane, make this work with the
            worldZ==arbitrary.
     */
    void dragEvent(MSceneViewGLWidget *sceneView,
                   int handleID, float clipX, float clipY);

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

    const QVector<QVector3D>& getPoleVertices() const;

    void setMovement(bool enabled);

    void setIndividualPoleHeightsEnabled(bool enabled);

    /**
      Programatically enable/disable the "add pole" property and the properties
      that control position and vertical extent of a pole. Used e.g. by the
      SkewT-Actor that keep a pole as a subactor that should only have exactly
      one pole.
     */
    void enablePoleProperties(bool enabled);

    void setTubeRadius(float radius);

    void setVerticalExtent(float pbot_hPa, float ptop_hPa);

    void setPolePosition(int index, QPointF lonlatPos);

    void setTicksOnRightSide(bool rightSide);

    const QVector<QVector3D>& getAxisTicks() { return axisTicks; }

protected:
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

    void generatePole();

    void generateGeometry();

    std::shared_ptr<GL::MShaderEffect> simpleGeometryEffect;
    std::shared_ptr<GL::MShaderEffect> positionSpheresShader;

    QVector<QVector3D> axisTicks;
    GL::MVertexBuffer* axisVertexBuffer;

    // Contain the first and last indices of the values stored in axisTicks
    // respectively labels vector per pole (pole0First, pole0Last, pole1First,
    // pole1Last, ...).
    QVector<int> axisTicksFirstLastIndices;
    QVector<int> labelsFirstLastIndices;

    QtProperty *ticksGroupProperty;
    QtProperty *tickLengthProperty;
    float       tickLength;
    QtProperty *ticksOnRightSideProperty;
    bool        ticksOnRightSide;

    QtProperty *colourProperty;
    QColor      lineColour;

    QtProperty *addPoleProperty;

    QtProperty *tickIntervalAboveThreshold;
    QtProperty *tickIntervalBelowThreshold;
    QtProperty *tickPressureThresholdProperty;
    QtProperty *labelSpacingProperty;

    QtProperty *bottomPressureProperty;
    QtProperty *topPressureProperty;
    float       bottomPressure_hPa;
    float       topPressure_hPa;

    QtProperty *renderModeProperty;
    enum class RenderModes : uint8_t { TUBES = 0, LINES = 1 };
    RenderModes renderMode;

    QtProperty *tubeRadiusProperty;
    float       tubeRadius;

    QtProperty *individualPoleHeightsProperty;
    bool        individualPoleHeightsEnabled;

    bool        movementEnabled;

    QVector<std::shared_ptr<MovablePole>> poles;

    QVector<QVector3D> poleVertices;
    GL::MVertexBuffer* poleVertexBuffer;

    /** This variable stores the ID of a pole to be highlighted. If the value
        is -1, no waypoint will be highlighted. */
    int highlightPole;

    // If the user picks the handle not in its centre, we cannot move the handle
    // by setting the centre point to the mouse position so we need this offset
    // to place the handle relative to the mouse position.
    QVector2D offsetPickPositionToHandleCentre;
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
