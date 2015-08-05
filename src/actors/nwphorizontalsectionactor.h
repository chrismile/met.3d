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
#ifndef LONLATHYBHSECACTOR_H
#define LONLATHYBHSECACTOR_H

// standard library imports
#include <memory> // for use of shared_ptr

// related third party imports
#include <GL/glew.h>
#include <QtProperty>

// local application imports
#include "gxfw/nwpmultivaractor.h"
#include "gxfw/nwpactorvariable.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/typedvertexbuffer.h"
#include "actors/transferfunction1d.h"
#include "actors/graticuleactor.h"

class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief MNWPHorizontalSectionActor renders a horizontal cross section from
  multiple model level or pressure level data variables.
  */
class MNWPHorizontalSectionActor : public MNWPMultiVarActor
{
public:
    MNWPHorizontalSectionActor();
    ~MNWPHorizontalSectionActor();

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
      Set the pressure at which the section is rendered.
      */
    void setSlicePosition(double pressure);

    /**
      Set the horizontal bounding box for the region (lonEast, latSouth, width,
      height).
      */
    void setBBox(QRectF bbox);

    void setSurfaceShadowEnabled(bool enable);

    /**
      Get a pointer to the horizontal section's graticule actor.
     */
    MGraticuleActor* getGraticuleActor() const { return graticuleActor; }

    QString getSettingsID() override { return "NWPHorizontalSectionActor"; }

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

    const QList<MVerticalLevelType> supportedLevelTypes() override;

    MNWPActorVariable *createActorVariable(
            const MSelectableDataSource& dataSource) override;

protected:
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView);

    /**
      Implements MNWPActor::dataFieldChangedEvent() to update the target grid
      if the data field has changed.
      */
    void dataFieldChangedEvent();

    /**
      Computes the array indices of the data field covered by the current
      bounding box. Computation is done per-variable.

      @see MNWP2DHorizontalActorVariable::computeRenderRegionParameters()
      */
    void computeRenderRegionParameters();

    /**
      Updates the label that shows the vertical position of the section.
      */
    void updateDescriptionLabel(bool deleteOldLabel=true);

    void onDeleteActorVariable(MNWPActorVariable* var) override;

    void onAddActorVariable(MNWPActorVariable* var) override;

private:
    /** Methods to render the horizontal section. */
    void renderVerticalInterpolation(MNWP2DHorizontalActorVariable* var);

    void renderVerticalInterpolationDifference(
                                MNWP2DHorizontalActorVariable* var,
                                MNWP2DHorizontalActorVariable* varDiff);

    void renderFilledContours(MSceneViewGLWidget *sceneView,
                              MNWP2DHorizontalActorVariable* var);

    void renderPseudoColour(MSceneViewGLWidget *sceneView,
                            MNWP2DHorizontalActorVariable* var);

    void renderLineCountours(MSceneViewGLWidget *sceneView,
                             MNWP2DHorizontalActorVariable* var);

    void renderWindBarbs(MSceneViewGLWidget *sceneView);

    void renderShadow(MSceneViewGLWidget* sceneView);

    void renderContourLabels(MSceneViewGLWidget *sceneView,
                             MNWP2DHorizontalActorVariable* var);

    void updateMouseHandlePositions();

    std::shared_ptr<GL::MShaderEffect> glVerticalInterpolationEffect;
    std::shared_ptr<GL::MShaderEffect> glFilledContoursShader;
    std::shared_ptr<GL::MShaderEffect> glPseudoColourShader;
    std::shared_ptr<GL::MShaderEffect> glMarchingSquaresShader;
    std::shared_ptr<GL::MShaderEffect> glWindBarbsShader;
    std::shared_ptr<GL::MShaderEffect> glShadowQuad;
    std::shared_ptr<GL::MShaderEffect> positionSpheresShader;

    QtProperty *slicePosProperty;
    double      slicePosition_hPa;

    bool crossSectionGridsNeedUpdate;
    bool updateRenderRegion;

    /** Mouse handles */
    QVector<QVector3D> mouseHandlePoints;
    GL::MVector3DVertexBuffer *vbMouseHandlePoints;
    int selectedMouseHandle;

    /** Horizontal bounding box in which section is drawn */
    QtProperty *boundingBoxProperty;
    QRectF horizontalBBox;
    double llcrnrlon;
    double llcrnrlat;
    double urcrnrlon;
    double urcrnrlat;

    QtProperty *differenceModeProperty;
    short       differenceMode;

    /** Graticule that is overlaid on the horizontal section. */
    MGraticuleActor *graticuleActor;

    /** Settings for windbarb labels */
    GL::MVertexBuffer* windBarbsVertexBuffer;

    struct WindBarbsSettings
    {
        WindBarbsSettings(MNWPHorizontalSectionActor* hostActor);

        QtProperty* groupProperty;

        bool        enabled;
        QtProperty* enabledProperty;

        bool        automaticEnabled;
        QtProperty* automaticEnabledProperty;
        float       oldScale;

        float       lineWidth;
        QtProperty* lineWidthProperty;

        int         numFlags;
        QtProperty* numFlagsProperty;

        QColor      color;
        QtProperty* colorProperty;

        bool        showCalmGlyphs;
        QtProperty* showCalmGlyphsProperty;

        // reduction factor c
        float       reduceFactor;
        QtProperty* reduceFactorProperty;

        // slope factor b
        float       reduceSlope;
        QtProperty* reduceSlopeProperty;

        // sensibility of glyph res-change
        float       sensibility;
        QtProperty* sensibilityProperty;

        int8_t      uComponentVarIndex;
        QtProperty* uComponentVarProperty;

        int8_t      vComponentVarIndex;
        QtProperty* vComponentVarProperty;

        QList<QString> varNameList;
    };

    friend struct WindBarbsSettings;
    WindBarbsSettings *windBarbsSettings;

    /** Properties for shadow quad. */
    QtProperty* shadowPropGroup;

    bool        renderShadowQuad;
    QtProperty* shadowEnabledProp;

    QColor      shadowColor;
    QtProperty* shadowColorProp;

    GLfloat     shadowHeight;
    QtProperty* shadowHeightProp;
};


class MNWPHorizontalSectionActorFactory : public MAbstractActorFactory
{
public:
    MNWPHorizontalSectionActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override
    { return new MNWPHorizontalSectionActor(); }
};


} // namespace Met3D

#endif // LONLATHYBHSECACTOR_H
