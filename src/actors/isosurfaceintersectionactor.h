/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Michael Kern
**  Copyright 2017 Christoph Heidelmann
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
#ifndef ISOSURFACEINTERSECTIONACTOR_H
#define ISOSURFACEINTERSECTIONACTOR_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <QtProperty>

// local application imports
#include "actors/transferfunction1d.h"

#include "data/datarequest.h"
#include "data/isosurfaceintersectionsource.h"
#include "data/trajectorynormalssource.h"
#include "data/variabletrajectoryfilter.h"
#include "data/geometriclengthtrajectoryfilter.h"
#include "data/multivarpartialderivativefilter.h"
#include "data/trajectoryvaluesource.h"

#include "gxfw/nwpmultivaractor.h"
#include "gxfw/synccontrol.h"
#include "gxfw/gl/texture.h"
#include "gxfw/gl/shadereffect.h"

class MGLResourcesManager;
class MSceneViewGLWidget;


namespace Met3D
{
/**
 * @brief MIsosurfaceIntersectionActor displays intersection lines of two
 * crossed iso-surfaces co-located on the same grid.
 */
class MIsosurfaceIntersectionActor : public MNWPMultiVarIsolevelActor,
        public MBoundingBoxInterface
{
    Q_OBJECT

public:
    explicit MIsosurfaceIntersectionActor();
    virtual ~MIsosurfaceIntersectionActor() override;

    void reloadShaderEffects();

    virtual QString getSettingsID() override
    { return "IsosurfaceIntersectionActor"; }

    virtual void saveConfiguration(QSettings *settings) override;

    virtual void loadConfiguration(QSettings *settings) override;

    MNWPActorVariable* createActorVariable(
            const MSelectableDataSource& dataSource);

    /**
      Returns a list of the vertical level types that can be handled by this
      actor.
     */
    const QList<MVerticalLevelType> supportedLevelTypes();

    /**
     * Set the input source that computes the intersection of two iso-surfaces.
     */
    void setDataSource(MIsosurfaceIntersectionSource* ds);

public slots:

    /**
     Called by the iso-surface intersection source when data request by @ref
     requestIsoSurfaceIntersectionLines() is ready.
     */
    void asynchronousDataAvailable(MDataRequest request);

    /**
     Called by the trajectory filter source when data request by @ref
     requestFilters() is ready.
     */
    void asynchronousFiltersAvailable(MDataRequest request);

    /**
     Called by the trajectory value source when data request by @ref
     buildGPUResources() is ready.
     */
    void asynchronousValuesAvailable(MDataRequest request);

    void isoValueOfVariableChanged();

    void onAddActorVariable(MNWPActorVariable *var);

    void onDeleteActorVariable(MNWPActorVariable *var);

    void onChangeActorVariable(MNWPActorVariable *var);

    /**
     Emit an actor changed signal if pole actor has been modified by the user.
     This causes both, the jetcore and pole actor to redraw on the current scene.
     */
    void onPoleActorChanged();

    /**
     Connects to the MGLResourcesManager::actorCreated() signal. If the new
     actor is a transfer function, it is added to the list of transfer
     functions displayed by the transferFunctionProperty.
     */
    void onActorCreated(MActor *actor);

    void onActorDeleted(MActor *actor);

    void onActorRenamed(MActor *actor, QString oldName);

    void onBoundingBoxChanged();

protected:

    /**
      Loads the shader programs and generates the graticule geometry. The
      geometry is uploaded to the GPU into a vertex buffer object.
     */
    virtual void initializeActorResources() override;

    void onQtPropertyChanged(QtProperty *property);

    virtual void renderToCurrentContext(MSceneViewGLWidget *sceneView) override;

    /**
     * Add a new filter to the intersection line filter pipeline.
     */
    void addFilter(std::shared_ptr<MScheduledDataSource> trajFilter);

    /**
     * Request each filter in the filter chain pipeline successively.
     */
    void requestFilters();

    /**
     * Build the filter chain and the corresponding filter request.
     */
    virtual void buildFilterChain(MDataRequestHelper& rh);

    /**
     * Called after the filter chain pipeline was finished.
     */
    virtual void onFilterChainEnd();

    /**
     * Generate all GPU resources required to display the results.
     */
    virtual void buildGPUResources();

    /**
     * Asynchronously request the crossing lines from the input source.
     */
    virtual void requestIsoSurfaceIntersectionLines();

    /**
     * Place pole actors (vertical drop lines) along the intersection lines.
     */
    void placePoleActors(MIsosurfaceIntersectionLines* intersectionLines);

    /**
     * Refresh all variables select boxes if a variable was added or removed.
     */
    virtual void refreshEnumsProperties(MNWPActorVariable *var);

    void setTransferFunctionFromProperty();

    bool setTransferFunction(const QString& tfName);

    void renderBoundingBox(MSceneViewGLWidget *sceneView);
    virtual void renderToDepthMap(MSceneViewGLWidget *sceneView);
    void renderShadows(MSceneViewGLWidget *sceneView);
    void generateVolumeBoxGeometry();

    /** Points to the current iso-surface intersection source. */
    MIsosurfaceIntersectionSource       *isosurfaceSource;

    /** Points to the current intersection line data. */
    MIsosurfaceIntersectionLines        *intersectionLines;

    /** Layout of generic filter pipeline request. */
    struct Request
    {
        std::shared_ptr<MTrajectoryFilter> filter;
        MTrajectorySelectionSource* inputSelectionSource;
        QString request;
    };

    /** Array of filter requests. */
    QVector<Request> filterRequests;

    QString          arrowRequest; //! TODO: do we really need that variable here

    /** Request to gather value information per intersection line vertex. */
    QString          valueRequest;

    /** Points to the currently used trajectory filter. */
    std::shared_ptr<MTrajectoryFilter> currentTrajectoryFilter;

    /** Points to the by-value trajectory filter. */
    std::shared_ptr<MVariableTrajectoryFilter> varTrajectoryFilter;

    /** Points to the geometric length filter. */
    std::shared_ptr<MGeometricLengthTrajectoryFilter> geomLengthTrajectoryFilter;

    /** Points to the obtain-values trajectory filter / source. */
    std::shared_ptr<MTrajectoryValueSource> valueTrajectorySource;

    /** Points to the current selection of lines after filtering. */
    MTrajectoryEnsembleSelection        *lineSelection;

    /** Vertex buffer object of intersection lines. */
    GL::MVertexBuffer                   *linesVertexBuffer;

    /** Layout of each line vertex. */
    struct lineVertex
    {
        float x, y, z, varColor, varThickness;
    };

    /** Raw data of intersection lines. */
    QVector<lineVertex> linesData;

    /** String containing the full line request passed to the input source. */
    QString lineRequest;

    /**
     * Settings to select the variables that should be intersected and filtered
     * by value.
     */
    struct VariableSettings
    {
        explicit VariableSettings(MIsosurfaceIntersectionActor* hostActor);

        QtProperty *groupProp;

        /** 1st and 2nd variable, value variable. */
        std::array<QtProperty*, 2> varsProperty;
        std::array<int, 2> varsIndex;
    };

    /** Settings to filter the lines by value and geometric length. */
    struct LineFilterSettings
    {
        explicit LineFilterSettings(MIsosurfaceIntersectionActor* hostActor);

        QtProperty *groupProp;

        QtProperty* filterVarProperty;
        int         filterVarIndex;
        QtProperty* valueFilterProperty;
        float       valueFilter;
        QtProperty* lineLengthFilterProperty;
        int         lineLengthFilter;
    };

    /** Settings to adjust the appearance of the lines. */
    struct AppearanceSettings
    {
        explicit AppearanceSettings(MIsosurfaceIntersectionActor* hostActor);

        QtProperty *groupProp;

        QtProperty *colorModeProperty;
        int         colorMode;

        QtProperty *colorVariableProperty;
        int         colorVariableIndex;

        QtProperty *tubeRadiusProperty;
        float       tubeRadius;

        QtProperty *tubeColorProperty;
        QColor      tubeColor;

        QtProperty *transferFunctionProperty;
        /** Pointer to transfer function object and corresponding texture unit. */
        MTransferFunction1D *transferFunction;
        int                 textureUnitTransferFunction;

        QtProperty* enableShadowsProperty;
        bool        enableShadows;

        QtProperty* enableSelfShadowingProperty;
        bool        enableSelfShadowing;

        QtProperty* polesEnabledProperty;
        bool        polesEnabled;
        QtProperty* dropModeProperty;
        int         dropMode;
    };

    QtProperty* thicknessModeProperty;
    int         thicknessMode;

    /** Settings to map variable values to tube thickness at each line vertex. */
    struct TubeThicknessSettings
    {
        explicit TubeThicknessSettings(MIsosurfaceIntersectionActor* hostActor);

        QtProperty* groupProp;

        /** Variable whose values are mapped to thickness at each vertex. */
        QtProperty* mappedVariableProperty;
        int         mappedVariableIndex;

        /** Min / max variable value. */
        QtProperty* minValueProp;
        QtProperty* maxValueProp;
        QVector2D   valueRange;

        /** Min / max thickness value. */
        QtProperty* minProp;
        QtProperty* maxProp;
        QVector2D   thicknessRange;
    };

    /**
     * Settings to select a set of ensemble members whose variables should
     * be intersected and displayed.
     */
    struct EnsembleSelectionSettings
    {
        explicit EnsembleSelectionSettings(
                MIsosurfaceIntersectionActor* hostActor);

        QtProperty* groupProp;

        QtProperty* syncModeEnabledProperty;
        bool        syncModeEnabled;
        QtProperty* ensembleMultiMemberProperty;
        QSet<unsigned int> selectedEnsembleMembers;
        QtProperty* ensembleMultiMemberSelectionProperty;
    };

    /** Properties for volume bounding box. */
    struct BoundingBoxSettings
    {
        explicit BoundingBoxSettings() {}

        bool                enabled;

        // coordinates of bounding box
        GLfloat             llcrnLon;
        GLfloat             llcrnLat;
        GLfloat             urcrnLon;
        GLfloat             urcrnLat;
        // top and bottom pressure for bounding box
        GLfloat             pBot_hPa;
        GLfloat             pTop_hPa;

        // properties
        QtProperty*         enabledProperty;
    };


    /** Buttons to control manual / auto-computation. */
    QtProperty* computeClickProperty;
    QtProperty* enableAutoComputationProperty;
    bool        enableAutoComputation;

    /** Pointers to the settings instances. */
    std::shared_ptr<VariableSettings>           variableSettings;
    std::shared_ptr<LineFilterSettings>         lineFilterSettings;
    std::shared_ptr<AppearanceSettings>         appearanceSettings;
    std::shared_ptr<TubeThicknessSettings>      tubeThicknessSettings;
    std::shared_ptr<EnsembleSelectionSettings>  ensembleSelectionSettings;
    std::shared_ptr<BoundingBoxSettings>        boundingBoxSettings;

    /** Suppress any actor updates until all properties are initialized. */
    bool supressActorUpdates;
    bool isCalculating;

    /** Pole actor to relate jetstream cores to their actual height in pressure. */
    std::shared_ptr<MMovablePoleActor> poleActor;

    /** GLSL shader objects. */
    std::shared_ptr<GL::MShaderEffect> intersectionLinesShader;
    std::shared_ptr<GL::MShaderEffect> boundingBoxShader;
    std::shared_ptr<GL::MShaderEffect> tubeShadowShader;
    std::shared_ptr<GL::MShaderEffect> lineTubeShader;
    GL::MTexture*                      shadowMap;
    int                                shadowMapTexUnit;
    unsigned int                       shadowMapFBO;
    GL::MVertexBuffer*                 shadowImageVBO;
    unsigned int                       shadowMapRes;
    QMatrix4x4                         lightMVP;
    bool                               updateShadowImage;
    GL::MVertexBuffer*                 vboBoundingBox;
    GLuint                             iboBoundingBox;
};


class MIsosurfaceIntersectionActorFactory : public MAbstractActorFactory
{
public:
    MIsosurfaceIntersectionActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override {
        return new MIsosurfaceIntersectionActor(); }
};


} // namespace Met3D

#endif // ISOSURFACEINTERSECTIONACTOR_H

