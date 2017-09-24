//
// Created by kerninator on 02.05.17.
//

#ifndef JETCOREDETECTIONACTOR_H
#define JETCOREDETECTIONACTOR_H

#include "isosurfaceintersectionactor.h"
#include "../data/multivarpartialderivativefilter.h"
#include "../data/hessiantrajectoryfilter.h"
#include "../data/angletrajectoryfilter.h"
#include "../data/endpressuredifferencetrajectoryfilter.h"
#include "../data/trajectoryarrowheadssource.h"

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
         * @param var
         */
        void refreshEnumsProperties(MNWPActorVariable *var) override;

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

        /** Settings to filter the jet-cores with respect to their Hessian
         *  eigenvalue, line-segment angle, and pressure difference */
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

        /** Pointers to the settings instances */
        std::shared_ptr<VariableSettingsJetcores> variableSettingsCores;
        std::shared_ptr<FilterSettingsJetcores> lineFilterSettingsCores;
        std::shared_ptr<AppearanceSettingsJetcores> appearanceSettingsCores;

        /** Points to the current partial derivative filters, one for each variable. */
        std::array<MMultiVarPartialDerivativeFilter*, 2> partialDerivFilters;
        /** Points to the current Hessian eigenvalue filter. */
        std::shared_ptr<MHessianTrajectoryFilter>        hessianFilter;
        /** Points to the current line segment angle filter. */
        std::shared_ptr<MAngleTrajectoryFilter>          angleFilter;
        /** Points to the current pressure difference filter. */
        std::shared_ptr<MEndPressureDifferenceTrajectoryFilter> pressureDiffFilter;
        /** Points to the current arrow head filter. */
        std::shared_ptr<MTrajectoryArrowHeadsSource>     arrowHeadsSource;

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

}

#endif //JETCOREDETECTIONACTOR_H
