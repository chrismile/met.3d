/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2015      Michael Kern
**  Copyright 2017-2018      Bianca Tost
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
#ifndef LONLATMULTIPURPOSEACTOR_H
#define LONLATMULTIPURPOSEACTOR_H

// standard library imports
#include <unordered_map>
#include <bitset>
#include <cstdint>
#include <memory>
#include <vector>

// related third party imports
#include <GL/glew.h>
#include <QtProperty>
#include <QVector>
#include <QVector3D>
#include <QMatrix4x4>
#include <QHash>

// local application imports
#include "gxfw/nwpmultivaractor.h"
#include "gxfw/nwpactorvariable.h"
#include "gxfw/gl/shadereffect.h"
#include "gxfw/gl/shaderstoragebufferobject.h"
#include "actors/transferfunction1d.h"
#include "actors/graticuleactor.h"

class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief 3D volume isosurface raycaster. This actor renders (multiple)
  isosurfaces of hybrid sigma-pressure and pressure-level grids via GPU-based
  raycasting. Normal curves are implemented in this actor as well. Analysis
  plug-ins (@ref MAnalysisControl) are supported; for example, for the region
  contribution module (@ref MRegionContributionAnalysisControl).
 */
class MNWPVolumeRaycasterActor : public MNWPMultiVarActor,
        public MBoundingBoxInterface
{
    Q_OBJECT

public:
    MNWPVolumeRaycasterActor();

    ~MNWPVolumeRaycasterActor();

    static QString staticActorType() { return "Volume raycaster"; }

    void reloadShaderEffects();

    QString getSettingsID() override { return "NWPVolumeRaycasterActor"; }

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

    /**
      Isosurface "picking". Identifies the isosurface "object" that intersects
      a ray from camera to mouse position (where the user has clicked). If
      an @ref MAnalysisControl is connected to this class, triggers the
      analysis of the selected object.
     */
    bool triggerAnalysisOfObjectAtPos(
            MSceneViewGLWidget *sceneView, float clipX, float clipY,
            float clipRadius);

    const QList<MVerticalLevelType> supportedLevelTypes() override;

    MNWPActorVariable *createActorVariable(
            const MSelectableDataSource& dataSource) override;

    /**
      Returns the @ref MNWPActorVariable currently raycasted.
     */
    const MNWPActorVariable* getCurrentRenderVariable() const
    { return var; }

    /**
      Returns the @ref MNWPActorVariable that is used to shade the isosurfaces
      extracted from @ref getCurrentRenderVariable().
     */
    const MNWPActorVariable* getCurrentShadingVariable() const
    { return shadingVar; }

    void onBoundingBoxChanged();

public slots:
    /**
      Sets flag for shadow image to be updated in the next frame.

      Connected to @ref MTransferFunction1D::actorChanged() to update shadow if
      transfer function is changed.
     */
    void updateShadow();

protected:
    void initializeActorResources();

    /**
      Specifies GLSL subroutine names used in the shaders.
     */
    void initializeRenderInformation();

    void onQtPropertyChanged(QtProperty* property);

    void renderToCurrentContext(MSceneViewGLWidget* sceneView);

    void dataFieldChangedEvent();

    /**
      Computes the intersection points of a ray with a box. Returns tnear and
      tfar (packed in a vec2), which are the entry and exit parameters of the
      ray into the box when a position pos on the ray is described by
            pos = ray.origin + t * ray.direction

      boxCrnr1 and boxCrnr2 are two opposite corners of the box.

      Literature: Williams et al. (2005), "An efficient and robust ray-box
      intersection algorithm." (notes 29/03/2012).
      */
    bool rayBoxIntersection(QVector3D rayOrigin, QVector3D rayDirection,
                            QVector3D boxCrnr1, QVector3D boxCrnr2,
                            QVector2D *tNearFar);

    int computeCrossingLevel(float scalar);

    void bisectionCorrection(MSceneViewGLWidget *sceneView,
                             QVector3D *rayPosition, float *lambda,
                             QVector3D prevRayPosition, float prevLambda,
                             int *crossingLevelFront, int *crossingLevelBack);

    void onDeleteActorVariable(MNWPActorVariable* var) override;

    void onAddActorVariable(MNWPActorVariable* var) override;

    void onChangeActorVariable(MNWPActorVariable *var) override;

    void onLoadActorVariableFailure(int varIndex) override;

private:
    // Methods that create/compute graphics resources (volume bounding box,
    // shadow image, normal curves).
    // ====================================================================

    /**
      Generates geometry for a volume bounding box.
     */
    void generateVolumeBoxGeometry();

    /**
      Updates the geometry of the cross that is drawn when the user selects
      an isosurface in interaction mode.

      @see triggerAnalysisOfObjectAtPos()
     */
    void updatePositionCrossGeometry(QVector3D worldSpacePosition);

    /**
      Create a shadow image (renders the scene from the top into a b/w
      texture).
     */
    void createShadowImage(MSceneViewGLWidget* sceneView);

    /**
      Compute normal curve seed points (=initial points).
     */
    void computeNormalCurveInitialPoints(MSceneViewGLWidget* sceneView);

    /**
      Compute normal curve line segments.
     */
    void computeNormalCurves(MSceneViewGLWidget* sceneView);

    // Methods that set shader variables (e.g. uniforms) prior to rendering.
    // =====================================================================

    /**
      Sets shader variables that are common to all shaders used in this
      actor.
     */
    void setCommonShaderVars(
            std::shared_ptr<GL::MShaderEffect>& shader,
            MSceneViewGLWidget* sceneView);

    void setVarSpecificShaderVars(
            std::shared_ptr<GL::MShaderEffect>& shader,
            MSceneViewGLWidget* sceneView,
            MNWP3DVolumeActorVariable* var,
            const QString& structName,
            const QString& volumeName,
            const QString& transferFuncName,
            const QString& pressureTableName,
            const QString& surfacePressureName,
            const QString& hybridCoeffName,
            const QString& lonLatLevAxesName,
            const QString& pressureTexCoordTable2DName,
            const QString& minMaxAccelStructure3DName,
            const QString& dataFlagsVolumeName,
            const QString& auxPressureField3DName);

    void setRayCasterShaderVars(
            std::shared_ptr<GL::MShaderEffect>& shader,
            MSceneViewGLWidget* sceneView);

    void setBoundingBoxShaderVars(
            MSceneViewGLWidget* sceneView);

    void setNormalCurveShaderVars(
            std::shared_ptr<GL::MShaderEffect>& shader,
            MSceneViewGLWidget* sceneView);

    void setNormalCurveComputeShaderVars(
            std::shared_ptr<GL::MShaderEffect>& shader,
            MSceneViewGLWidget* sceneView);

    // Methods that do the OpenGL rendering work.
    // ==========================================

    void renderBoundingBox(MSceneViewGLWidget* sceneView);

    void renderPositionCross(MSceneViewGLWidget* sceneView);

    void renderRayCaster(std::shared_ptr<GL::MShaderEffect>& effect,
                         MSceneViewGLWidget* sceneView);

    void renderShadows(MSceneViewGLWidget* sceneView);

    void renderNormalCurves(MSceneViewGLWidget* sceneView,
                            bool toDepth = false, bool shadow = false);

    void renderToDepthTexture(MSceneViewGLWidget* sceneView);

    void mirrorDepthBufferToTexture(MSceneViewGLWidget* sceneView);

    // Class members.
    // ==============

    /**
      Enums for render settings.
     */
    struct RenderMode
    {
        RenderMode() {}
        enum Type { Original = 0, Bitfield, DVR };
        enum ColorType { IsoColor = 0, TransferFunction };
        enum Resolution { VeryLowRes = 0, LowRes, NormalRes,
                          HighRes, VeryHighRes, UltraRes };
        enum ShadowMode { ShadowOff = 0, ShadowMap, ShadowRay };
    };

    /**
      Struct that accomodates OpenGL rendering resources.
     */
    struct OpenGL
    {
        OpenGL();

        // Shader effects.
        // ===============

        // shader for standard isosurface raycasting
        std::shared_ptr<GL::MShaderEffect> rayCasterEffect;

        // shader for ensemble bitfield isosurface raycasting
        std::shared_ptr<GL::MShaderEffect> bitfieldRayCasterEffect;
        // shader for simple bounding box rendering
        std::shared_ptr<GL::MShaderEffect> boundingBoxShader;
        // shaders to create and render shadow images
        std::shared_ptr<GL::MShaderEffect> shadowImageRenderShader;

        // shader to obtain init points for the normal curves
        std::shared_ptr<GL::MShaderEffect> normalCurveInitPointsShader;
        // shader to compute line segments of each normal curve
        std::shared_ptr<GL::MShaderEffect> normalCurveLineComputeShader;
        // shaders to render normal curves
        std::shared_ptr<GL::MShaderEffect> normalCurveGeometryEffect;

        // Vertex/index buffers.
        // =====================

        // bounding box vertex and index buffer
        GL::MVertexBuffer* vboBoundingBox;
        GLuint             iboBoundingBox;
        // position cross vertex buffer
        GL::MVertexBuffer* vboPositionCross;

        // shadow image render vertex buffer
        GL::MVertexBuffer* vboShadowImageRender;

        // ground image vertex buffer
        GL::MVertexBuffer* vboShadowImage;

        // normal curve vertices stored in a ssbo buffer to allow OpenGL to
        // read and write from/to that buffer - like UAVs in Direct3D 11
        GL::MShaderStorageBufferObject* ssboInitPoints;
        GL::MShaderStorageBufferObject* ssboNormalCurves;

        // Textures.
        // =========

        // shadow image texture2D
        GL::MTexture* tex2DShadowImage;
        GLint         texUnitShadowImage;

        // contains depth data (useful for normal curves)
        GL::MTexture*  tex2DDepthBuffer;
        GLint         texUnitDepthBuffer;

        // Texture that mirrors the OpenGL depth buffer. Used for DVR to
        // store the depths before the raycaster is started, so that the ray
        // can be terminated at the corresponding fragment depth. Each scene
        // view gets its own framebuffer object.
        GL::MTexture* textureDepthBufferMirror;
        GLint         textureUnitDepthBufferMirror;
        QHash<MSceneViewGLWidget*, GLuint> fboDepthBuffer;

        // Subroutines.
        // ============

        std::vector<QList<QString>> rayCasterSubroutines;
        std::vector<QList<QString>> bitfieldRayCasterSubroutines;
        std::vector<QList<QString>> normalCompSubroutines;
        std::vector<QList<QString>> normalInitSubroutines;
    };

    /**
      Properties for isosurface lighting.
     */
    struct LightingSettings
    {
        LightingSettings(MNWPVolumeRaycasterActor *hostActor);

        GLint               lightingMode;
        GLfloat             ambient;
        GLfloat             diffuse;
        GLfloat             specular;
        GLfloat             shininess;
        QColor              shadowColor;

        QtProperty*         groupProp;

        QtProperty*         lightingModeProp;
        QtProperty*         ambientProp;
        QtProperty*         diffuseProp;
        QtProperty*         specularProp;
        QtProperty*         shininessProp;
        QtProperty*         shadowColorProp;
    };

    /**
      Properties for a single isosurface.
     */
    struct IsoValueSettings
    {
        enum ColorType { ConstColor = 0,
                         TransferFuncColor,
                         TransferFuncShadingVar,
                         TransferFuncShadingVarMaxNeighbour };

        IsoValueSettings(MNWPVolumeRaycasterActor *hostActor = nullptr,
                         const uint8_t index = 0,
                         bool _enabled = true,
                         float _isoValue = 0.5,
                         int significantDigits = 1,
                         double singleStep = 0.1,
                         QColor _color = QColor(255,0,0,255),
                         ColorType _colorType = ColorType::ConstColor);

        GLboolean   enabled;
        GLfloat     isoValue;
        QColor      isoColour;
        ColorType   isoColourType;

        QtProperty *groupProp;
        QtProperty *enabledProp;
        QtProperty *isoValueProp;
        QtProperty *isoValueSignificantDigitsProperty;
        QtProperty *isoValueSingleStepProperty;
        QtProperty *isoColourProp;
        QtProperty *isoColourTypeProp;
        QtProperty *isoValueRemoveProp;
    };

    /**
      Raycaster properties.
     */
    struct RayCasterSettings
    {
        RayCasterSettings(MNWPVolumeRaycasterActor *hostActor);

        void sortIsoValues();

        void addIsoValue(
                const bool enabled = true,
                const bool hidden = false,
                const float isoValue = 0.5,
                const int decimals = 1,
                const double singleStep = 0.1,
                const QColor color = QColor(255,0,0,255),
                const IsoValueSettings::ColorType colorType
                = IsoValueSettings::ColorType::ConstColor);

        MNWPVolumeRaycasterActor      *hostActor;
        // iso value settings from GUI
        QVector<IsoValueSettings>  isoValueSetList;
        // list need to be sorted so that crossing levels work correct
        QVector<GLint>                 isoEnabled;
        QVector<GLfloat>               isoValues;
        QVector<QVector4D>             isoColors;
        QVector<GLint>                 isoColorTypes;

        GLfloat                        stepSize;
        GLfloat                        interactionStepSize;
        GLuint                         bisectionSteps;
        GLuint                         interactionBisectionSteps;

        RenderMode::ShadowMode         shadowMode;
        RenderMode::Resolution         shadowsResolution;

        // properties
        QtProperty*                    groupProp;
        QtProperty*                    groupRaycasterSettings;
        QtProperty*                    groupShadowSettings;

        QtProperty*                    isoValuesProp;
        QtProperty*                    addIsoValueProp;

        QtProperty*                    stepSizeProp;
        QtProperty*                    interactionStepSizeProp;
        QtProperty*                    bisectionStepsProp;
        QtProperty*                    interactionBisectionStepsProp;
        QtProperty*                    shadowModeProp;
        QtProperty*                    shadowsResolutionProp;
    };

    /**
      Properties for normal curves.
     */
    struct NormalCurveSettings
    {
        NormalCurveSettings(MNWPVolumeRaycasterActor *hostActor);

        enum GlyphType { Line = 0, Box, Tube };
        enum Threshold { Steps = 0, StopIsoValue = 1 };
        enum CurveColor { ColorSteps = 0, ColorCurve, ColorIsoValue };
        enum Surface { Inner = 0, Outer };
        enum IntegrationDir { Backwards = 0, Forwards = 1, Both = 2};

        GLboolean           normalCurvesEnabled;

        GlyphType           glyph;
        Threshold           threshold;
        CurveColor          colour;
        GLfloat             tubeRadius;

        GLfloat             stepSize;
        GLint               numLineSegments;
        IntegrationDir      integrationDir;

        GLfloat             startIsoValue;
        GLfloat             stopIsoValue;

        GLfloat             initPointResX;
        GLfloat             initPointResY;
        GLfloat             initPointResZ;

        GLfloat             initPointVariance;

        GLuint              numSteps;

        QtProperty*         groupProp;

        QtProperty*         normalCurvesEnabledProp;

        QtProperty*         groupRenderingSettingsProp;
        QtProperty*         glyphProp;
        QtProperty*         tubeRadiusProp;
        QtProperty*         colourProp;

        QtProperty*         thresholdProp;
        QtProperty*         startIsoSurfaceProp;
        QtProperty*         stopIsoSurfaceProp;

        QtProperty*         stepSizeProp;
        QtProperty*         integrationDirProp;
        QtProperty*         numLineSegmentsProp;

        QtProperty*         groupSeedSettingsProp;

        QtProperty*         seedPointResXProp;
        QtProperty*         seedPointResYProp;
        QtProperty*         seedPointResZProp;

        QtProperty*         seedPointVarianceProp;

    };

    enum NextRenderFrameUpdateRequests
    {
        UpdateShadowImage = 0,
        ComputeNCInitPoints,
        RecomputeNCLines,
        Size
    };

    std::bitset<NextRenderFrameUpdateRequests::Size> updateNextRenderFrame;

    // "Top-level" properties.
    RenderMode::Type        renderMode;
    QtProperty*             renderModeProp;

    // List that stores the names of all registered actor variables (for the
    // enum properties that allow the user to select observed and shading
    // variables.
    QList<QString>          varNameList;

    // Variable that is raycasted.
    int8_t                  variableIndex;
    int8_t                  loadedVariableIndex;
    QtProperty*             variableIndexProp;
    MNWP3DVolumeActorVariable* var;

    // Variable that is used to shade an isosurface set to "transfer function
    // shading".
    int8_t                  shadingVariableIndex;
    int8_t                  loadedShadingVariableIndex;
    QtProperty*             shadingVariableIndexProp;
    MNWP3DVolumeActorVariable* shadingVar;

    OpenGL                  gl;
    QtProperty*             bBoxEnabledProperty;
    bool                    bBoxEnabled;
    LightingSettings        *lightingSettings;
    RayCasterSettings       *rayCasterSettings;
    NormalCurveSettings     *normalCurveSettings;
    // number of normal curve vertices to draw
    uint32_t                normalCurveNumVertices;

    /**
      Struct that describes a single normal curve vertex.
     */
    struct NormalCurveLineSegment
    {
        // world position
        float x,y,z;
        // value responsible for normal curve color mapping
        float value;
    };

    // initial seed points for the normal curves
    GLuint                                      numNormalCurveInitPoints;
    // normal curve lines
    std::vector<NormalCurveLineSegment>         normalCurves;
};


class MNWPVolumeRaycasterActorFactory : public MAbstractActorFactory
{
public:
    MNWPVolumeRaycasterActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override
    { return new MNWPVolumeRaycasterActor(); }
};


} // namespace Met3D

#endif // LONLATMULTIPURPOSEACTOR_H
