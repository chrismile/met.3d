/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020-2021 Christoph Neuhauser
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

enum class MultiVarRadiusMappingMode {
    GLOBAL,
    LINE
};

class MMultiVarData
{
public:
    MMultiVarData();

    void setProperties(MActor* actor, MQtProperties *properties, QtProperty *multiVarGroupProperty);

    QtProperty *addProperty(MQtPropertyType type, const QString &name, QtProperty *group = nullptr);
    void setEnabled(bool isEnabled);

    bool hasProperty(QtProperty *property);
    void onQtPropertyChanged(QtProperty *property);

    void saveConfiguration(QSettings *settings);
    void loadConfiguration(QSettings *settings);

    void onBezierTrajectoriesLoaded(const QStringList& auxDataVarNames);

    void initTransferFunctionsMultiVar(uint32_t numVariables);

    bool getSelectedVariablesChanged() { return selectedVariablesChanged; }
    bool resetSelectedVariablesChanged() { selectedVariablesChanged = false; }
    const QVector<uint32_t>& getSelectedVariables() { return selectedVariables; }

    /**
      Set a transfer function to map vertical position (pressure) to colour.
      */
    void setTransferFunctionMultiVar(int varIdx, MTransferFunction1D *tf);

    /**
      Set a transfer function by its name. Set to 'none' if @oaram tfName does
      not exit.
      */
    bool setTransferFunctionMultiVar(int varIdx, QString tfName);

    void setTransferFunctionMultiVarFromProperty(int varIdx);

    void setUniformData(int textureUnitTransferFunction);
    std::shared_ptr<GL::MShaderEffect> getShaderEffect();

    /**
     * Interface for the trajectory actor.
     */
    void onActorCreated(MActor *actor);
    void onActorDeleted(MActor *actor);
    void onActorRenamed(MActor *actor, QString oldName);

private:
    void setPropertiesRenderingSettings();
    void setPropertiesVarSelected();
    void reloadShaderEffect();
    void updateModeEnabledProperties();
    void updateNumVariablesSelected();

    MActor* actor;
    MQtProperties *properties;
    QtProperty *multiVarGroupProperty;

    QVector<QtProperty*> propertyList;
    QtProperty *renderTechniqueProperty;
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

    // TODO
    QStringList varNames;
    MMultiVarTf multiVarTf;
    QVector<QtProperty*> tfPropertiesMultiVar;
    QVector<MTransferFunction1D*> transferFunctionsMultiVar;

    QtProperty *renderingSettingsGroupProperty;
    QtProperty *numLineSegmentsProperty;
    QtProperty *fiberRadiusProperty;
    QtProperty *minRadiusFactorProperty;
    QtProperty *rollWidthProperty;

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

    std::shared_ptr<GL::MShaderEffect> shaderEffect;
    bool isDirty = true;

    // Rendering modes.
    MultiVarRenderMode multiVarRenderMode = MultiVarRenderMode::ORIENTED_COLOR_BANDS;
    MultiVarRadiusMappingMode multiVarRadiusMappingMode = MultiVarRadiusMappingMode::GLOBAL;

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
    std::vector<uint32_t> varSelected;
    //std::vector<glm::vec4> varColors;
    std::string comboValue = "";
    int32_t numVariablesSelected = 0;
    int32_t maxNumVariables = 6;
    int32_t numLineSegments = 8;
    int32_t numInstances = 12;
    int32_t rollWidth = 1;
    float separatorWidth = 0.15f;
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

    // Lighting settings.
    bool useColorIntensity = true;
    float materialConstantAmbient = 0.1f;
    float materialConstantDiffuse = 0.85f;
    float materialConstantSpecular = 0.05f;
    float materialConstantSpecularExp = 10.0f;
    bool drawHalo = true;
    float haloFactor = 1.2f;
};

}


#endif //MET_3D_MULTIVARDATA_H
