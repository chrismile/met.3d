/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015      Michael Kern
**  Copyright 2016-2017 Bianca Tost
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
#include "gxfw/boundingbox/boundingbox.h"

class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief MNWPHorizontalSectionActor renders a horizontal cross section from
  multiple model level or pressure level data variables.
  */
class MNWPHorizontalSectionActor : public MNWPMultiVarActor,
        public MBoundingBoxInterface
{
    Q_OBJECT

public:
    MNWPHorizontalSectionActor();
    ~MNWPHorizontalSectionActor();

    static QString staticActorType() { return "Horizontal cross-section"; }

    void reloadShaderEffects();

    /**
      Implements MActor::checkIntersectionWithHandle().

      Checks is the mouse position in clip space @p clipX and @p clipY
      "touches" one of the four corner points of this cross-section. If a
      corner point is matched, its index is returned.
      */
    int checkIntersectionWithHandle(MSceneViewGLWidget *sceneView,
                                    float clipX, float clipY) override;

    void addPositionLabel(MSceneViewGLWidget *sceneView, int handleID,
                          float clipX, float clipY);

    /**
      Implements MActor::dragEvent().

      Drags the corner point at index @p handleID to the vertical position that
      the mouse cursor points at on a plane perpendicular to the x/y plane,
      updates the vertical slice position and triggers a redraw of the scene.
      */
    void dragEvent(MSceneViewGLWidget *sceneView,
                   int handleID, float clipX, float clipY) override;

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

    bool isConnectedTo(MActor *actor) override;

    void onBoundingBoxChanged() override;


    const QRectF getHorizontalBBox()
    { return bBoxConnection->horizontal2DCoords(); }

    const double getSlicePosition() { return slicePosition_hPa; }

public slots:
    /**
      Set the pressure at which the section is rendered.
      */
    void setSlicePosition(double pressure_hPa);

signals:
    void slicePositionChanged(double pressure_hPa);

protected:
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property) override;

    void onOtherActorCreated(MActor *actor) override;

    void onOtherActorDeleted(MActor *actor) override;

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

    void onChangeActorVariable(MNWPActorVariable *var) override;

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

    void renderTexturedContours(MSceneViewGLWidget *sceneView,
                             MNWP2DHorizontalActorVariable* var);

    void renderVectorGlyphs(
            MSceneViewGLWidget *sceneView,
            std::shared_ptr<GL::MShaderEffect> glyphsShader,
            bool renderShadow = false);

    void renderShadow(MSceneViewGLWidget* sceneView);

    void renderContourLabels(MSceneViewGLWidget *sceneView,
                             MNWP2DHorizontalActorVariable* var);

    void updateMouseHandlePositions();

    std::shared_ptr<GL::MShaderEffect> glVerticalInterpolationEffect;
    std::shared_ptr<GL::MShaderEffect> glFilledContoursShader;
    std::shared_ptr<GL::MShaderEffect> glTexturedContoursShader;
    std::shared_ptr<GL::MShaderEffect> glPseudoColourShader;
    std::shared_ptr<GL::MShaderEffect> glMarchingSquaresShader;
    std::shared_ptr<GL::MShaderEffect> glVectorGlyphsShader;
    std::shared_ptr<GL::MShaderEffect> gl3DVectorGlyphsShader;
    std::shared_ptr<GL::MShaderEffect> glShadowQuad;
    std::shared_ptr<GL::MShaderEffect> positionSpheresShader;

    QtProperty *slicePosProperty;
    double      slicePosition_hPa;

    QtProperty *slicePosGranularityProperty;
    double      slicePositionGranularity_hPa;

    QtProperty *synchronizeSlicePosWithOtherActorProperty;
    MNWPHorizontalSectionActor *slicePosSynchronizationActor;

    bool crossSectionGridsNeedUpdate;
    bool updateRenderRegion;

    /** Mouse handles */
    QVector<QVector3D> mouseHandlePoints;
    GL::MVector3DVertexBuffer *vbMouseHandlePoints;
    int selectedMouseHandle;

    QtProperty *differenceModeProperty;
    short       differenceMode;

    /** Graticule that is overlaid on the horizontal section. */
    MGraticuleActor *graticuleActor;

    /** Settings for windbarb labels */
    GL::MVertexBuffer* vectorGlyphsVertexBuffer;

    struct VectorGlyphsSettings
    {
        VectorGlyphsSettings(MNWPHorizontalSectionActor* hostActor);

        QtProperty *groupProperty;

        bool        enabled;
        QtProperty *enabledProperty;

        int8_t      lonComponentVarIndex;
        QtProperty *lonComponentVarProperty;

        int8_t      latComponentVarIndex;
        QtProperty *latComponentVarProperty;

        QList<QString> varNameList;

        QtProperty *appearanceGroupProperty;

        enum class GlyphType
        {
            Invalid = -1,
            WindBarbs = 0,
            Arrows = 1,
            Arrows3D = 2
        };

        enum class PivotPos
        {
            Invalid = -1,
            Tip = 0,
            Centre = 1,
            Tail = 2
        };

        GlyphType   glyphType;
        QtProperty *glyphTypeProperty;

        PivotPos    pivotPosition;
        QtProperty *pivotPositionProperty;

        float       lineWidth;
        QtProperty *lineWidthProperty;

        QColor      color;
        QtProperty *colorProperty;

        QtProperty *windBarbsGroupProperty;
        bool        showCalmGlyphs;
        QtProperty *showCalmGlyphsProperty;
        int         pennantTilt;
        QtProperty *pennantTiltProperty;

        QtProperty *arrowsGroupProperty;
        double      arrowHeadHeight;
        QtProperty *arrowHeadHeightProperty;
        QtProperty *scaleArrowGroupProperty;
        bool        scaleArrowWithMagnitude;
        QtProperty *scaleArrowWithMagnitudeProperty;
        double      scaleArrowMinScalar;
        QtProperty *scaleArrowMinScalarProperty;
        double      scaleArrowMinLength;
        QtProperty *scaleArrowMinLengthProperty;
        bool        scaleArrowDiscardBelow;
        QtProperty *scaleArrowDiscardBelowProperty;
        double      scaleArrowMaxScalar;
        QtProperty *scaleArrowMaxScalarProperty;
        double      scaleArrowMaxLength;
        QtProperty *scaleArrowMaxLengthProperty;
        bool        scaleArrowDiscardAbove;
        QtProperty *scaleArrowDiscardAboveProperty;

        QtProperty *arrows2DGroupProperty;
        bool        arrowDrawOutline;
        QtProperty *arrowDrawOutlineProperty;
        double      arrowOutlineWidth;
        QtProperty *arrowOutlineWidthProperty;
        QColor      arrowOutlineColour;
        QtProperty *arrowOutlineColourProperty;

        QtProperty *arrows3DGroupProperty;
        bool        arrows3DShadowEnabled;
        QtProperty *arrows3DShadowEnabledProperty;
        QColor      arrows3DShadowColour;
        QtProperty *arrows3DShadowColourProperty;

        float       deltaGlyphsLonLat;
        QtProperty *deltaGlyphsLonLatProperty;

        bool        clampDeltaGlyphsToGrid;
        QtProperty *clampDeltaGlyphsToGridProperty;

        bool        automaticScalingEnabled;
        QtProperty *automaticScalingEnabledProperty;
        float       oldScale;

        QtProperty *scalingGroupProperty;

        float       reduceFactor;
        QtProperty *reduceFactorProperty;

        float       reduceSlope;
        QtProperty *reduceSlopeProperty;

        float       sensitivity;
        QtProperty *sensitivityProperty;
    };

    friend struct VectorGlyphsSettings;
    VectorGlyphsSettings *vectorGlyphsSettings;

    /** Properties for shadow quad. */
    QtProperty* shadowPropGroup;

    bool        renderShadowQuad;
    QtProperty* shadowEnabledProp;

    QColor      shadowColor;
    QtProperty* shadowColorProp;

    GLfloat     shadowHeight;
    QtProperty* shadowHeightProp;

    // If the user picks the handle not in its centre, we cannot move the handle
    // by setting the centre point to the mouse position so we need this offset
    // to place the handle relative to the mouse position.
    QVector2D offsetPickPositionToHandleCentre;
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
