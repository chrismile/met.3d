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
    ORIENTED_COLOR_BANDS
};
enum class MultiVarFocusRenderMode : unsigned int {
    NONE,
    TANGENT,
    GREAT_CIRCLE,
    CROSS_SECTION,
    PIE_CHART_AREA,
    PIE_CHART_COLOR,
    ROLLS
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
    QVector<MTransferFunction1D*>& getTransferFunctionsMultiVar() { return transferFunctionsMultiVar; }
    const QVector<MTransferFunction1D*>& getTransferFunctionsMultiVar() const { return transferFunctionsMultiVar; }

    // When a shader needing a different internal representation was loaded.
    inline bool getInternalRepresentationChanged() const { return internalRepresentationChanged; }
    inline void resetInternalRepresentationChanged() { internalRepresentationChanged = false; }
    inline MultiVarFocusRenderMode getFocusRenderMode() const { return focusRenderMode; }
    inline bool getRenderSpheres() const {
        return focusRenderMode != MultiVarFocusRenderMode::NONE && focusRenderMode != MultiVarFocusRenderMode::ROLLS;
    }
    inline bool getRenderRolls() const { return focusRenderMode == MultiVarFocusRenderMode::ROLLS; }

    inline bool getSelectedVariablesChanged() const { return selectedVariablesChanged; }
    inline void resetSelectedVariablesChanged() { selectedVariablesChanged = false; }
    inline const QVector<uint32_t>& getSelectedVariableIndices() { return selectedVariableIndices; }
    inline void setSelectedVariables(const QVector<uint32_t>& _selectedVariableIndices) {
        this->selectedVariableIndices = _selectedVariableIndices;
        ignorePropertyUpdateMode = true;
        for (uint32_t varIdx = 0; varIdx < uint32_t(maxNumVariables); varIdx++)
        {
            QtProperty* variableProperty = selectedVariablesProperties.at(int(varIdx));
            bool shouldBeSelected = std::find(
                    selectedVariableIndices.begin(), selectedVariableIndices.end(),
                    varIdx) != selectedVariableIndices.end();
            bool isSelected = properties->mBool()->value(variableProperty);
            if (isSelected != shouldBeSelected) {
                properties->mBool()->setValue(variableProperty, shouldBeSelected);
            }
        }
        ignorePropertyUpdateMode = false;
        updateNumVariablesSelected();
    }

    inline bool getShowTargetVariableAndSensitivity() const { return targetVariableAndSensitivity; }

    inline bool getVarDivergingChanged() const { return varDivergingChanged; }
    inline void resetVarDivergingChanged() { varDivergingChanged = false; }
    inline const QVector<uint32_t>& getVarDiverging() { return varDiverging; }

    inline bool getUseTimestepLens() const { return useTimestepLens; }
    inline const QVector<QString>& getVarNames() const { return varNames; }

    inline int getNumLineSegments() const { return numLineSegments; }
    inline float getRollsWidth() const { return rollsWidth; }
    inline bool getMapRollsThickness() const { return mapRollsThickness; }

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
    void setUniformDataRolls(int textureUnitTransferFunction);
    std::shared_ptr<GL::MShaderEffect> getShaderEffect();
    std::shared_ptr<GL::MShaderEffect> getTimeStepSphereShader();
    std::shared_ptr<GL::MShaderEffect> getTimeStepRollsShader();

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
    void reloadRollsShaderEffect();
    void updateModeEnabledProperties();
    void updateNumVariablesSelected();

    MActor* actor;
    MQtProperties *properties;
    QtProperty *multiVarGroupProperty;

    QVector<QtProperty*> propertyList;
    QtProperty *renderTechniqueProperty;
    QtProperty *focusRenderTechniqueProperty;
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
    QVector<uint32_t> selectedVariableIndices;
    bool selectedVariablesChanged = false;
    bool ignorePropertyUpdateMode = true;

    // Show target variable and maximum sensitivity.
    QtProperty *targetVariableAndSensitivityProperty;
    bool targetVariableAndSensitivity = false;

    bool varDivergingChanged = false;
    QVector<uint32_t> varDiverging;

    std::shared_ptr<GL::MShaderEffect> shaderEffect;
    //bool isDirty = true;
    DiagramDisplayType diagramType = DiagramDisplayType::NONE;

    // Time step sphere rendering.
    std::shared_ptr<GL::MShaderEffect> shaderEffectSphere;
    bool shallReloadShaderEffect = true;
    bool shallReloadSphereShaderEffect = true;

    // Time step rolls rendering.
    std::shared_ptr<GL::MShaderEffect> shaderEffectRolls;
    bool shallReloadRollsEffect = true;
    bool shallReloadRollsShaderEffect = true;

    // Rendering modes.
    MultiVarRenderMode multiVarRenderMode = MultiVarRenderMode::ORIENTED_COLOR_BANDS;
    bool internalRepresentationChanged = false; ///< If multiVarRenderMode changes to other mode needing different data.
    MultiVarFocusRenderMode focusRenderMode = MultiVarFocusRenderMode::GREAT_CIRCLE;

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
    int32_t maxNumVariables = 0;
    int32_t numLineSegments = 8;
    float separatorWidth = 0.10f;
    /// For orientedRibbonMode == ORIENTED_RIBBON_MODE_VARYING_BAND_WIDTH
    QVector4D bandBackgroundColor = QVector4D(0.5f, 0.5f, 0.5f, 1.0f);

    // Line settings.
    float minRadiusFactor = 0.5f;
    float fiberRadius = 0.05f;
    bool useTimestepLens = true;

    // Rolls settings.
    bool useColorIntensityRolls = true;
    float rollsWidth = 0.2f;
    bool mapRollsThickness = true;

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
