/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020-2022 Christoph Neuhauser
**  Copyright 2020      Michael Kern
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
#ifndef MET_3D_MULTIVARDATA_H
#define MET_3D_MULTIVARDATA_H

// standard library imports

// related third party imports
#include <QtCore>
#include <QtProperty>

// local application imports
#include "data/abstractdataitem.h"
#include "actors/transferfunction1d.h"
#include "gxfw/gl/shaderstoragebufferobject.h"
#include "trajectorypicking.h"
#include "multivartf.h"


namespace Met3D
{

enum class MultiVarRenderMode {
    ROLLS,
    TWISTED_ROLLS,
    COLOR_BANDS,
    ORIENTED_COLOR_BANDS,
    CHECKERBOARD,
    FIBERS
};
enum class MultiVarSphereRenderMode {
    NONE,
    TANGENT,
    GREAT_CIRCLE,
    CROSS_SECTION,
    PIE_CHART_AREA,
    PIE_CHART_COLOR
};

inline bool getMultiVarRenderModeNeedsSubdiv(MultiVarRenderMode multiVarRenderMode) {
    return multiVarRenderMode == MultiVarRenderMode::ROLLS
            || multiVarRenderMode == MultiVarRenderMode::CHECKERBOARD
            || multiVarRenderMode == MultiVarRenderMode::TWISTED_ROLLS;
}

enum class MultiVarRadiusMappingMode {
    GLOBAL,
    LINE
};

class MMultiVarData : public QObject
{
public:
    MMultiVarData();
    ~MMultiVarData() override;

    void setProperties(MActor* actor, MQtProperties* properties, QtProperty* multiVarGroupProperty);

    QtProperty *addProperty(MQtPropertyType type, const QString &name, QtProperty* group = nullptr);
    void removeProperty(QtProperty* property, QtProperty *group = nullptr);
    void setEnabled(bool isEnabled);

    bool hasProperty(QtProperty* property);
    void onQtPropertyChanged(QtProperty* property);

    void saveConfiguration(QSettings* settings);
    void loadConfiguration(QSettings* settings);

    void onBezierTrajectoriesLoaded(MTrajectories* trajectories);
    void clearVariableRanges();
    void updateVariableRanges(const QVector<QVector2D>& ranges);

    void initTransferFunctionsMultiVar(uint32_t numVariables);

    // When a shader needing a different internal representation was loaded.
    inline bool getInternalRepresentationChanged() const { return internalRepresentationChanged; }
    inline void resetInternalRepresentationChanged() { internalRepresentationChanged = false; }
    inline bool getNeedsSubdiv() { return getMultiVarRenderModeNeedsSubdiv(multiVarRenderMode); }
    inline bool getRenderSpheres() { return sphereRenderMode != MultiVarSphereRenderMode::NONE; }

    inline bool getSelectedVariablesChanged() const { return selectedVariablesChanged; }
    inline void resetSelectedVariablesChanged() { selectedVariablesChanged = false; }
    inline const QVector<uint32_t>& getSelectedVariables() { return selectedVariables; }
    inline void setSelectedVariables(const QVector<uint32_t>& selectedVariables) {
        this->selectedVariables = selectedVariables;
        for (int varIdx = 0; varIdx < maxNumVariables; varIdx++)
        {
            QtProperty* variableProperty = selectedVariablesProperties.at(varIdx);
            properties->mBool()->setValue(variableProperty, selectedVariables.at(varIdx));
        }
        updateNumVariablesSelected();
    }

    inline bool getVarDivergingChanged() const { return varDivergingChanged; }
    inline void resetVarDivergingChanged() { varDivergingChanged = false; }
    inline const QVector<uint32_t>& getVarDiverging() { return varDiverging; }

    inline bool getUseTimestepLens() const { return useTimestepLens; }
    inline const QVector<QString>& getVarNames() const { return varNames; }

    /**
      Sets the diagram type currently used by MTrajectoryPicking.
      */
    void setDiagramType(DiagramDisplayType type);

    /**
      Set a transfer function to map attributes to colour.
      */
    void setTransferFunctionMultiVar(int varIdx, MTransferFunction1D *tf);

    /**
      Set a transfer function by its name. Set to 'none' if @param tfName does
      not exist.
      */
    bool setTransferFunctionMultiVar(int varIdx, QString tfName);

    void setTransferFunctionMultiVarFromProperty(int varIdx);

    void registerTransferFunction(MTransferFunction1D *tf);

    void setUniformData(int textureUnitTransferFunction);
    void setUniformDataSpheres(int textureUnitTransferFunction);
    std::shared_ptr<GL::MShaderEffect> getShaderEffect();
    std::shared_ptr<GL::MShaderEffect> getTimeStepSphereShader();

    /**
     * Interface for the trajectory actor.
     */
    void onActorCreated(MActor *actor);
    void onActorDeleted(MActor *actor);
    void onActorRenamed(MActor *actor, QString oldName);

public slots:
    void transferFunctionChanged(MTransferFunction1D *tf);

private:
    void setPropertiesRenderingSettings();
    void setPropertiesVarSelected();
    void reloadShaderEffect();
    void reloadSphereShaderEffect();
    void updateModeEnabledProperties();
    void updateNumVariablesSelected();

    MActor* actor;
    MQtProperties *properties;
    QtProperty *multiVarGroupProperty;

    QVector<QtProperty*> propertyList;
    QtProperty *renderTechniqueProperty;
    QtProperty *sphereRenderTechniqueProperty;
    QtProperty *mapTubeDiameterProperty;
    QtProperty *radiusMappingModeProperty;
    QtProperty *checkerboardHeightProperty;
    QtProperty *checkerboardWidthProperty;
    QtProperty *checkerboardIteratorProperty;
    QtProperty *twistOffsetProperty;
    QtProperty *constantTwistOffsetProperty;
    QtProperty *orientedRibbonModeProperty;
    QtProperty *bandBackgroundColorProperty;
    QtProperty *separatorWidthProperty;
    QtProperty *useColorIntensityProperty;

    QVector<QString> varNames;
    MMultiVarTf multiVarTf;
    QVector<QtProperty*> tfPropertiesMultiVar;
    QVector<MTransferFunction1D*> transferFunctionsMultiVar;
    QVector<QVector2D> variableRanges;

    QtProperty *renderingSettingsGroupProperty;
    QtProperty *numLineSegmentsProperty;
    QtProperty *fiberRadiusProperty;
    QtProperty *minRadiusFactorProperty;
    QtProperty *rollWidthProperty;
    QtProperty *useTimestepLensProperty;

    QtProperty *phongLightingSettingsGroup;
    QtProperty *materialConstantAmbientProperty;
    QtProperty *materialConstantDiffuseProperty;
    QtProperty *materialConstantSpecularProperty;
    QtProperty *materialConstantSpecularExpProperty;
    QtProperty *drawHaloProperty;
    QtProperty *haloFactorProperty;

    QtProperty *selectedVariablesGroupProperty;
    QVector<QtProperty*> selectedVariablesProperties;
    QVector<uint32_t> selectedVariables;
    bool selectedVariablesChanged = false;

    bool varDivergingChanged = false;
    QVector<uint32_t> varDiverging;

    std::shared_ptr<GL::MShaderEffect> shaderEffect;
    //bool isDirty = true;
    bool shallReloadShaderEffect = true;
    bool shallReloadSphereShaderEffect = true;
    DiagramDisplayType diagramType = DiagramDisplayType::NONE;

    // Time step sphere rendering.
    std::shared_ptr<GL::MShaderEffect> shaderEffectSphere;

    // Rendering modes.
    MultiVarRenderMode multiVarRenderMode = MultiVarRenderMode::ORIENTED_COLOR_BANDS;
    MultiVarRadiusMappingMode multiVarRadiusMappingMode = MultiVarRadiusMappingMode::GLOBAL;
    bool internalRepresentationChanged = false; ///< If multiVarRenderMode changes to other mode needing different data.
    MultiVarSphereRenderMode sphereRenderMode = MultiVarSphereRenderMode::CROSS_SECTION;

    // For MULTIVAR_RENDERMODE_ORIENTED_COLOR_BANDS, MULTIVAR_RENDERMODE_ORIENTED_COLOR_BANDS_RIBBON
    enum class OrientedRibbonMode {
        FIXED_BAND_WIDTH,
        VARYING_BAND_WIDTH,
        VARYING_BAND_RATIO,
        VARYING_RIBBON_WIDTH
    };
    OrientedRibbonMode orientedRibbonMode = OrientedRibbonMode::FIXED_BAND_WIDTH;
    bool mapColorToSaturation = true; ///< !mapColorToSaturation -> DIRECT_COLOR_MAPPING in gather shader.

    // Multi-variable settings.
    //std::vector<glm::vec4> varColors;
    int32_t numVariablesSelected = 0;
    int32_t maxNumVariables = 6;
    int32_t numLineSegments = 8;
    int32_t numInstances = 12;
    int32_t rollWidth = 1;
    float separatorWidth = 0.10f;
    bool mapTubeDiameter = false;
    float twistOffset = 0.1f;
    bool constantTwistOffset = false;
    int32_t checkerboardWidth = 3;
    int32_t checkerboardHeight = 2;
    int32_t checkerboardIterator = 2;
    /// For orientedRibbonMode == ORIENTED_RIBBON_MODE_VARYING_BAND_WIDTH
    QVector4D bandBackgroundColor = QVector4D(0.5f, 0.5f, 0.5f, 1.0f);

    // Line settings.
    float minRadiusFactor = 0.5f;
    float fiberRadius = 0.05f;
    bool useTimestepLens = true;

    // Lighting settings.
    bool useColorIntensity = true;
    float materialConstantAmbient = 0.2f;
    float materialConstantDiffuse = 0.7f;
    float materialConstantSpecular = 0.5f;
    float materialConstantSpecularExp = 8.0f;
    bool drawHalo = true;
    float haloFactor = 1.0f;
};

}


#endif //MET_3D_MULTIVARDATA_H
