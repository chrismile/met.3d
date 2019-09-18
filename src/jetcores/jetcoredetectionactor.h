/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Michael Kern
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
#ifndef JETCOREDETECTIONACTOR_H
#define JETCOREDETECTIONACTOR_H

// standard library imports

// related third party imports

// local application imports
#include "isosurfaceintersectionactor.h"
#include "multivarpartialderivativefilter.h"
#include "hessiantrajectoryfilter.h"
#include "angletrajectoryfilter.h"
#include "endpressuredifferencetrajectoryfilter.h"
#include "trajectoryarrowheadssource.h"


namespace Met3D
{
class MJetcoreDetectionActor : public MIsosurfaceIntersectionActor
{
    Q_OBJECT

public:
    // Do not allow implicit conversion.
    explicit MJetcoreDetectionActor();
    virtual ~MJetcoreDetectionActor() override;

    QString getSettingsID() override { return "JetcoreDetectionActor"; }

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

public slots:
    void asynchronousArrowsAvailable(MDataRequest request);

protected:
    void initializeActorResources() override;

    void onQtPropertyChanged(QtProperty *property);

    void renderToCurrentContext(MSceneViewGLWidget *sceneView) override;

    void renderToDepthMap(MSceneViewGLWidget *sceneView) override;

    /**
     * Generate all GPU resources required to display the results.
     */
    void buildFilterChain(MDataRequestHelper& rh) override;

    /**
     * Called after the filter chain pipeline was finished.
     */
    void onFilterChainEnd() override;

    /**
     * Asnychronously request the crossing lines from the input source.
     */
    virtual void requestIsoSurfaceIntersectionLines() override;

    /**
     * Refresh all variables select boxes if a variable was added or removed.
     */
    void refreshEnumsProperties(MNWPActorVariable *var) override;

    /**
      Implements MNWPActor::dataFieldChangedEvent() to update the target grid
      if the data field has changed.
      */
    void dataFieldChangedEvent() override;

    /** Settings to select the geopotential height variable. */
    struct VariableSettingsJetcores
    {
        VariableSettingsJetcores(MJetcoreDetectionActor* hostActor,
                                 QtProperty* groupProp);

        QtProperty *geoPotVarProperty;
        int         geoPotVarIndex;

        QtProperty *geoPotOnlyProperty;
        bool        geoPotOnly;
    };

    /**
     * Settings to filter the jet-cores with respect to their Hessian
     * eigenvalue, line-segment angle, and pressure difference.
     */
    struct FilterSettingsJetcores
    {
        FilterSettingsJetcores(MJetcoreDetectionActor* hostActor,
                               QtProperty* groupProp);

        QtProperty *lambdaThresholdProperty;
        float       lambdaThreshold;

        QtProperty *angleThresholdProperty;
        float       angleThreshold;

        QtProperty *pressureDiffThresholdProperty;
        float       pressureDiffThreshold;
    };

    /** Appearance settings for jet cores, in particular arrows at the end. */
    struct AppearanceSettingsJetcores
    {
        AppearanceSettingsJetcores(MJetcoreDetectionActor* hostActor,
                                   QtProperty* groupProp);

        QtProperty *arrowsEnabledProperty;
        bool        arrowsEnabled;
    };

    /** Pointers to the settings instances. */
    std::shared_ptr<VariableSettingsJetcores> variableSettingsCores;
    std::shared_ptr<FilterSettingsJetcores> lineFilterSettingsCores;
    std::shared_ptr<AppearanceSettingsJetcores> appearanceSettingsCores;

    /**
     * Points to the current partial derivative filters, one for each
     * variable.
     */
    std::array<MMultiVarPartialDerivativeFilter*, 2> partialDerivFilters;

    /** Points to the current Hessian eigenvalue filter. */
    std::shared_ptr<MHessianTrajectoryFilter> hessianFilter;

    /** Points to the current line segment angle filter. */
    std::shared_ptr<MAngleTrajectoryFilter> angleFilter;

    /** Points to the current pressure difference filter. */
    std::shared_ptr<MEndPressureDifferenceTrajectoryFilter> pressureDiffFilter;

    /** Points to the current arrow head filter. */
    std::shared_ptr<MTrajectoryArrowHeadsSource> arrowHeadsSource;

    /** Vertex buffer object of arrow heads. */
    GL::MVertexBuffer *arrowsVertexBuffer;
    /** Raw data of arrow heads. */
    MTrajectoryArrowHeads* arrowHeads;

};

class MJetcoreDetectionActorFactory : public MAbstractActorFactory
{
public:
    MJetcoreDetectionActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override {
        return new MJetcoreDetectionActor(); }
};


} // namespace Met3D

#endif //JETCOREDETECTIONACTOR_H
