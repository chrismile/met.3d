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
#ifndef LONLATHYBVSECACTOR_H
#define LONLATHYBVSECACTOR_H

// standard library imports
#include <memory>


// related third party imports
#include <GL/glew.h>
#include <QtProperty>

// local application imports
#include "gxfw/nwpmultivaractor.h"
#include "gxfw/nwpactorvariable.h"
#include "gxfw/gl/texture.h"
#include "gxfw/gl/typedvertexbuffer.h"
#include "gxfw/gl/shadereffect.h"
#include "actors/transferfunction1d.h"
#include "actors/graticuleactor.h"
#include "data/waypoints/waypointstablemodel.h"

class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief MNWPVerticalSectionActor renders a vertical cross section from
  multiple model level or pressure level data variables.

  @todo Make the horizontal pressure isolines customizable.
  */
class MNWPVerticalSectionActor : public MNWPMultiVarActor
{
    Q_OBJECT

public:

    MNWPVerticalSectionActor();

    ~MNWPVerticalSectionActor();

    void reloadShaderEffects();

    /**
      Implements MActor::checkIntersectionWithHandle().

      Checks is the mouse position in clip space @p clipX and @p clipY
      "touches" one of the waypoints or midpoints of this cross-section
      (midpoints are located between two waypoints; if a midpoint is moved the
      entire segment is moved). If a way- or midpoint is matched, its index is
      returned.

      Approach: Simply test each way-/midpoint. Loops over all way-/midpoints.
      The world coordinates of the waypoint are transformed to clip space using
      the scene view's MVP matrix and assuming the point to be located on the
      worldZ == 0 plane. If the distance between the waypoint's clip
      coordinates and the mouse position is smaller than @p clipRadius, the
      waypoint is considered to be matched. (@p clipRadius is typically on the
      order of a few pixels; set in the scene view.)
      */
    virtual int checkIntersectionWithHandle(MSceneViewGLWidget *sceneView,
                                  float clipX, float clipY, float clipRadius);

    void addPositionLabel(MSceneViewGLWidget *sceneView, int handleID,
                          float clipX, float clipY);

    /**
      Implements MActor::dragEvent().

      Drags the way-/midpoint at index @p handleID to the position on the
      worldZ == 0 plane that the mouse cursor points at, updates the vertical
      section path and triggers a redraw of the scene.

      The mouse position in world space is found by computing the intersection
      point of the ray (camera origin - mouse position) with the worldZ == 0
      plane. The section path is updated by calling @ref
      generatePathFromWaypoints(). Expensive, because the scene view's MVP
      matrix is inverted and the vertical section's path is interpolated.
      */
    virtual void dragEvent(MSceneViewGLWidget *sceneView,
                           int handleID, float clipX, float clipY);

    /**
     Set the @ref MWaypointsTableModel instance from which the waypoints for
     the vertical section path are taken.
     */
    void setWaypointsModel(MWaypointsTableModel *model);

    MWaypointsTableModel* getWaypointsModel();

    double getBottomPressure();

    double getTopPressure();

    QString getSettingsID() override { return "NWPVerticalSectionActor"; }

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

    const QList<MVerticalLevelType> supportedLevelTypes() override;

    MNWPActorVariable *createActorVariable(
            const MSelectableDataSource& dataSource) override;

public slots:
    /**
      Generate a new set of interpolated points from the waypoints in
      the waypoints table model.

      @note This method assumes that all variables are on the same grid!
      @todo Make this work for multiple variables on different grids.
      @todo Switch between linear lat/lon connections and great circles.
      */
    void generatePathFromWaypoints(QModelIndex mindex1=QModelIndex(),
                                   QModelIndex mindex2=QModelIndex(),
                                   QGLWidget* currentGLContext = nullptr);

    void actOnWaypointInsertDelete(const QModelIndex &parent,
                                   int start, int end);

protected:
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    /**
      Renders
      A) the vertical section mesh, coloured by the data variable,
      B) contour lines (geometry shader marching squares implemenentation)
      C) isopressure lines along the vertical section,
      D) (only in modification mode) a circle to highlight a selected waypoint.
      */
    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

    /**
     Implements MNWPActor::dataFieldChangedEvent(). If the data field rendered
     in this section changes, an update of the target grid is triggered and the
     range of vertical levels that are rendered is recomputed (@see
     updateVerticalLevelRange()).
     */
    void dataFieldChangedEvent();

    /**
     Computes a list of pressure levels at which iso-pressure lines are plotted
     along the vertical section. The pressure levels are uploaded to a texture
     buffer.
     */
    void generateIsoPressureLines();

    /**
     For each variable, the vertical levels that need to be rendered to cover
     the range p_bot .. p_top are computed. The computed bounds are used to
     discard non-visible levels in @ref renderToCurrentContext().
     */
    void updateVerticalLevelRange();

    void generateLabels();

    void onDeleteActorVariable(MNWPActorVariable* var) override;

    void onAddActorVariable(MNWPActorVariable* var) override;

private:
    std::shared_ptr<GL::MShaderEffect> sectionGridShader;
    std::shared_ptr<GL::MShaderEffect> pressureLinesShader;
    std::shared_ptr<GL::MShaderEffect> marchingSquaresShader;
    std::shared_ptr<GL::MShaderEffect> simpleGeometryShader;
    std::shared_ptr<GL::MShaderEffect> positionSpheresShader;

    /**
      Each variable owns a "target grid", a 2D texture that stores the scalar
      values of the variable interpolated to the vertical section. It is used
      to speed up rendering (so that the interpolation doesn't have to be
      performed in every frame). If "targetGridToBeUpdated" is true,
      interpolation is carried out in the next frame and the target grid is
      re-computed.
      */
    bool targetGridToBeUpdated;

    QtProperty *labelDistanceProperty;
    int labelDistance;

    QtProperty *waypointsModelProperty;
    MWaypointsTableModel *waypointsModel;
    QVector<QVector4D> path;

    // This integer stores the ID of a waypoint to be highlighted. If the value
    // is -1, no waypoint will be highlighted. modifyWaypoint_worldZ stores the
    // worldZ coordinate of the selected waypoint, so that bot/top handles
    // can be distinguished.
    int modifyWaypoint;
    double modifyWaypoint_worldZ;

    GL::MTexture *textureVerticalSectionPath;
    int          textureUnitVerticalSectionPath;
    GL::MTexture *texturePressureLevels;
    int          textureUnitPressureLevels;

    GL::MVector3DVertexBuffer *vbVerticalWaypointLines;
    uint                       numVerticesVerticalWaypointLines;
    GL::MVector3DVertexBuffer *vbInteractionHandlePositions;
    uint                       numInteractionHandlePositions;

    double p_top_hPa;
    double p_bot_hPa;
    QtProperty *upperLimitProperty;
    QtProperty *lowerLimitProperty;
    QVector<float> pressureLineLevels;
    QVector<float> selectedPressureLineLevels;
    QtProperty *pressureLineLevelsProperty;

    float opacity;
    QtProperty *opacityProperty;

    float interpolationNodeSpacing;
    QtProperty *interpolationNodeSpacingProperty;

    bool updatePath;
};


class MNWPVerticalSectionActorFactory : public MAbstractActorFactory
{
public:
    MNWPVerticalSectionActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override
    { return new MNWPVerticalSectionActor(); }
};


} // namespace Met3D

#endif // LONLATHYBVSECACTOR_H
