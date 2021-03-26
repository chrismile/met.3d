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
#include "multivardata.h"

// standard library imports

// related third party imports
#include <QtCore>
#include <QtProperty>

// local application imports
#include "gxfw/mglresourcesmanager.h"

namespace Met3D
{

const QStringList renderingTechniqueShaderFilenames = {
        "src/glsl/multivar/multivar_rolls.fx.glsl",
        "src/glsl/multivar/multivar_twisted_rolls.fx.glsl",
        "src/glsl/multivar/multivar_color_bands.fx.glsl",
        "src/glsl/multivar/multivar_oriented_color_bands.fx.glsl",
        "src/glsl/multivar/multivar_checkerboard.fx.glsl",
        "src/glsl/multivar/multivar_fibers.fx.glsl"
};


const int MAX_NUM_VARIABLES = 20;


MMultiVarData::MMultiVarData() : multiVarTf(tfPropertiesMultiVar, transferFunctionsMultiVar)
{
    ;
}


static QColor colorFromVec(const QVector4D& vec) {
    int r = static_cast<int>(std::round(vec.x() * 255));
    int g = static_cast<int>(std::round(vec.y() * 255));
    int b = static_cast<int>(std::round(vec.z() * 255));
    int a = static_cast<int>(std::round(vec.w() * 255));
    return QColor(r, g, b, a);
}


static QVector4D vecFromColor(const QColor& color) {
    return QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}


void MMultiVarData::setProperties(MActor* actor, MQtProperties *properties, QtProperty *multiVarGroupProperty)
{
    this->actor = actor;
    this->properties = properties;
    this->multiVarGroupProperty = multiVarGroupProperty;

    renderTechniqueProperty = addProperty(
            ENUM_PROPERTY, "render technique", multiVarGroupProperty);
    QStringList renderingTechniques =
    {
            "Rolls", "Twisted Rolls", "Color Bands", "Oriented Color Bands", "Checkerboard", "Fibers"
    };
    properties->mEnum()->setEnumNames(renderTechniqueProperty, renderingTechniques);
    properties->mEnum()->setValue(renderTechniqueProperty, int(multiVarRenderMode));
    renderTechniqueProperty->setToolTip(
            "What line rendering technique to use for the multiple variables.");
    propertyList.push_back(renderTechniqueProperty);

    mapTubeDiameterProperty = addProperty(
            BOOL_PROPERTY, "map tube diameter", multiVarGroupProperty);
    properties->mBool()->setValue(mapTubeDiameterProperty, mapTubeDiameter);
    mapTubeDiameterProperty->setToolTip("Map variable values to the tube diameter.");
    propertyList.push_back(mapTubeDiameterProperty);

    radiusMappingModeProperty = addProperty(
            ENUM_PROPERTY, "radius mapping mode", multiVarGroupProperty);
    QStringList radiusMappingModes = { "Global", "Local" };
    properties->mEnum()->setEnumNames(radiusMappingModeProperty, radiusMappingModes);
    properties->mEnum()->setValue(radiusMappingModeProperty, int(multiVarRadiusMappingMode));
    radiusMappingModeProperty->setToolTip("Radius mapping mode.");
    propertyList.push_back(radiusMappingModeProperty);

    checkerboardHeightProperty = addProperty(
            INT_PROPERTY, "checkerboard height", multiVarGroupProperty);
    properties->setInt(checkerboardHeightProperty, checkerboardHeight, 1, 10);
    checkerboardHeightProperty->setToolTip("Checkerboard pattern height.");
    propertyList.push_back(checkerboardHeightProperty);

    checkerboardWidthProperty = addProperty(
            INT_PROPERTY, "checkerboard height", multiVarGroupProperty);
    properties->setInt(checkerboardWidthProperty, checkerboardWidth, 1, 20);
    checkerboardWidthProperty->setToolTip("Checkerboard pattern height.");
    propertyList.push_back(checkerboardWidthProperty);

    checkerboardIteratorProperty = addProperty(
            INT_PROPERTY, "checkerboard height", multiVarGroupProperty);
    properties->setInt(checkerboardIteratorProperty, checkerboardIterator, 1, 5);
    checkerboardIteratorProperty->setToolTip("Checkerboard pattern height.");
    propertyList.push_back(checkerboardIteratorProperty);

    twistOffsetProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "twist offset", multiVarGroupProperty);
    properties->setDDouble(
            twistOffsetProperty, twistOffset, 0.0, 1.0, 2, 0.1, " (world space)");
    twistOffsetProperty->setToolTip("Twist offset.");
    propertyList.push_back(twistOffsetProperty);

    constantTwistOffsetProperty = addProperty(
            BOOL_PROPERTY, "constant twist offset", multiVarGroupProperty);
    properties->mBool()->setValue(constantTwistOffsetProperty, constantTwistOffset);
    constantTwistOffsetProperty->setToolTip("Whether to use constant twist offset.");
    propertyList.push_back(constantTwistOffsetProperty);

    orientedRibbonModeProperty = addProperty(
            ENUM_PROPERTY, "oriented ribbon mode", multiVarGroupProperty);
    QStringList orientedRibbonModes =
    {
            "Fixed Band Width", "Varying Band Width", "Varying Band Ratio", "Varying Ribbon Width"
    };
    properties->mEnum()->setEnumNames(orientedRibbonModeProperty, orientedRibbonModes);
    properties->mEnum()->setValue(orientedRibbonModeProperty, int(orientedRibbonMode));
    orientedRibbonModeProperty->setToolTip(
            "Oriented ribbon mode (only when render technique 'oriented color bands' is used.");
    propertyList.push_back(orientedRibbonModeProperty);

    bandBackgroundColorProperty = addProperty(
            COLOR_PROPERTY, "band background color", multiVarGroupProperty);
    properties->mColor()->setValue(bandBackgroundColorProperty, colorFromVec(bandBackgroundColor));
    bandBackgroundColorProperty->setToolTip("The background color of the band.");
    propertyList.push_back(bandBackgroundColorProperty);

    separatorWidthProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "separator width", multiVarGroupProperty);
    properties->setDDouble(
            separatorWidthProperty, separatorWidth, 0.0, 1.0, 2, 0.1, " (ratio)");
    separatorWidthProperty->setToolTip("Separator width.");
    propertyList.push_back(separatorWidthProperty);

    useColorIntensityProperty = addProperty(
            BOOL_PROPERTY, "use color intensity", multiVarGroupProperty);
    properties->mBool()->setValue(useColorIntensityProperty, useColorIntensity);
    useColorIntensityProperty->setToolTip("Whether map the variables to color intensity.");
    propertyList.push_back(useColorIntensityProperty);


    // --- Group: Rendering settings ---
    renderingSettingsGroupProperty = addProperty(
            GROUP_PROPERTY, "rendering settings", multiVarGroupProperty);
    setPropertiesRenderingSettings();


    // --- Group: Selected variables ---
    selectedVariablesGroupProperty = addProperty(
            GROUP_PROPERTY, "selected variables", multiVarGroupProperty);
    selectedVariablesGroupProperty->setEnabled(false);


    updateModeEnabledProperties();
}


void MMultiVarData::setPropertiesRenderingSettings()
{
    numLineSegmentsProperty = addProperty(
            INT_PROPERTY, "num line segments", renderingSettingsGroupProperty);
    properties->setInt(numLineSegmentsProperty, numLineSegments, 3, 20);
    numLineSegmentsProperty->setToolTip("Number of line segments used for the tube rendering.");
    propertyList.push_back(numLineSegmentsProperty);

    fiberRadiusProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "fiber radius", renderingSettingsGroupProperty);
    properties->setDDouble(
            fiberRadiusProperty, fiberRadius, 0.01, 1.0, 4, 0.1, " (world space)");
    fiberRadiusProperty->setToolTip("Fiber radius.");
    propertyList.push_back(fiberRadiusProperty);

    minRadiusFactorProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "min radius factor", renderingSettingsGroupProperty);
    properties->setDDouble(
            minRadiusFactorProperty, minRadiusFactor, 0.0, 1.0, 3, 0.1, " (world space)");
    minRadiusFactorProperty->setToolTip("Minimum radius factor.");
    propertyList.push_back(minRadiusFactorProperty);

    rollWidthProperty = addProperty(
            INT_PROPERTY, "num line segments", renderingSettingsGroupProperty);
    properties->setInt(rollWidthProperty, rollWidth, 1, 4);
    rollWidthProperty->setToolTip("Number of line segments used for the tube rendering.");
    propertyList.push_back(rollWidthProperty);


    // --- Phong lighting settings ---
    phongLightingSettingsGroup = addProperty(
            GROUP_PROPERTY, "phong lighting settings", renderingSettingsGroupProperty);

    materialConstantAmbientProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "material ambient", phongLightingSettingsGroup);
    properties->setDDouble(
            materialConstantAmbientProperty, materialConstantAmbient,
            0.0, 1.0, 2, 0.1, " (factor)");
    materialConstantAmbientProperty->setToolTip("Ambient material factor.");
    propertyList.push_back(materialConstantAmbientProperty);

    materialConstantDiffuseProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "material diffuse", phongLightingSettingsGroup);
    properties->setDDouble(
            materialConstantDiffuseProperty, materialConstantDiffuse,
            0.0, 1.0, 2, 0.1, " (factor)");
    materialConstantDiffuseProperty->setToolTip("Diffuse material factor.");
    propertyList.push_back(materialConstantDiffuseProperty);

    materialConstantSpecularProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "material specular", phongLightingSettingsGroup);
    properties->setDDouble(
            materialConstantSpecularProperty, materialConstantSpecular,
            0.0, 1.0, 2, 0.1, " (factor)");
    materialConstantSpecularProperty->setToolTip("Specular material factor.");
    propertyList.push_back(materialConstantSpecularProperty);

    materialConstantSpecularExpProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "material specular exp", phongLightingSettingsGroup);
    properties->setDDouble(
            materialConstantSpecularExpProperty, materialConstantSpecularExp,
            0.0, 100.0, 2, 0.1, " (factor)");
    materialConstantSpecularExpProperty->setToolTip("Specular material exponent.");
    propertyList.push_back(materialConstantSpecularExpProperty);

    drawHaloProperty = addProperty(
            BOOL_PROPERTY, "draw halo", phongLightingSettingsGroup);
    properties->mBool()->setValue(drawHaloProperty, drawHalo);
    drawHaloProperty->setToolTip("Whether to use a halo effect when rendering the tubes.");
    propertyList.push_back(drawHaloProperty);

    haloFactorProperty = addProperty(
            DECORATEDDOUBLE_PROPERTY, "halo factor", phongLightingSettingsGroup);
    properties->setDDouble(
            haloFactorProperty, haloFactor, 0.0, 4.0, 1, 0.1, " (factor)");
    haloFactorProperty->setToolTip("Halo factor.");
    propertyList.push_back(haloFactorProperty);
}


void MMultiVarData::setPropertiesVarSelected()
{
    selectedVariablesGroupProperty->setEnabled(true);
    for (int varIdx = 0; varIdx < maxNumVariables; varIdx++)
    {
        QString name = QString("use variable #%1 (%2)").arg(QString::number(varIdx + 1), varNames.at(varIdx));
        QtProperty* variableProperty = addProperty(
                BOOL_PROPERTY, name, selectedVariablesGroupProperty);
        properties->mBool()->setValue(variableProperty, true);
        variableProperty->setToolTip(QString("Whether to display the variable '%2'").arg(varNames.at(varIdx)));
        selectedVariablesProperties.push_back(variableProperty);
        propertyList.push_back(variableProperty);
    }
}


void MMultiVarData::updateNumVariablesSelected()
{
    numVariablesSelected = 0;
    foreach (uint32_t isSelected, selectedVariables)
    {
        if (isSelected)
        {
            numVariablesSelected++;
        }
    }
}


void MMultiVarData::updateModeEnabledProperties()
{
    mapTubeDiameterProperty->setEnabled(
            multiVarRenderMode != MultiVarRenderMode::ORIENTED_COLOR_BANDS);
    radiusMappingModeProperty->setEnabled(
            multiVarRenderMode != MultiVarRenderMode::ORIENTED_COLOR_BANDS && mapTubeDiameter);
    checkerboardHeightProperty->setEnabled(multiVarRenderMode == MultiVarRenderMode::CHECKERBOARD);
    checkerboardWidthProperty->setEnabled(multiVarRenderMode == MultiVarRenderMode::CHECKERBOARD);
    checkerboardIteratorProperty->setEnabled(multiVarRenderMode == MultiVarRenderMode::CHECKERBOARD);
    twistOffsetProperty->setEnabled(multiVarRenderMode == MultiVarRenderMode::TWISTED_ROLLS);
    constantTwistOffsetProperty->setEnabled(multiVarRenderMode == MultiVarRenderMode::TWISTED_ROLLS);
    orientedRibbonModeProperty->setEnabled(multiVarRenderMode == MultiVarRenderMode::ORIENTED_COLOR_BANDS);
    bandBackgroundColorProperty->setEnabled(
            multiVarRenderMode == MultiVarRenderMode::ORIENTED_COLOR_BANDS
            && orientedRibbonMode == OrientedRibbonMode::VARYING_BAND_WIDTH);

    // --- Group: Rendering settings ---
    fiberRadiusProperty->setEnabled(multiVarRenderMode == MultiVarRenderMode::FIBERS);
    minRadiusFactorProperty->setEnabled(multiVarRenderMode != MultiVarRenderMode::ORIENTED_COLOR_BANDS);
    rollWidthProperty->setEnabled(multiVarRenderMode == MultiVarRenderMode::ROLLS);
}


void MMultiVarData::setEnabled(bool isEnabled)
{
    this->multiVarGroupProperty->setEnabled(isEnabled);
}


void MMultiVarData::saveConfiguration(QSettings *settings)
{
    settings->setValue("numVariables", transferFunctionsMultiVar.size());
    int varIdx = 0;
    foreach (QtProperty *tfProperty, tfPropertiesMultiVar)
    {
        settings->setValue(
                QString("transferFunction#%1").arg(varIdx + 1),
                properties->getEnumItem(tfProperty));
        settings->setValue(
                QString("varName#%1").arg(varIdx + 1),
                varNames.at(varIdx));
        settings->setValue(
                QString("varSelected#%1").arg(varIdx + 1),
                static_cast<bool>(selectedVariables.at(varIdx)));
        varIdx++;
    }

    // Multi-variable settings.
    settings->setValue(QString("numLineSegments"), numLineSegments);
    settings->setValue(QString("numInstances"), numInstances);
    settings->setValue(QString("rollWidth"), rollWidth);
    settings->setValue(QString("separatorWidth"), separatorWidth);
    settings->setValue(QString("mapTubeDiameter"), mapTubeDiameter);
    settings->setValue(QString("twistOffset"), twistOffset);
    settings->setValue(QString("constantTwistOffset"), constantTwistOffset);
    settings->setValue(QString("checkerboardWidth"), checkerboardWidth);
    settings->setValue(QString("checkerboardHeight"), checkerboardHeight);
    settings->setValue(QString("checkerboardIterator"), checkerboardIterator);
    settings->setValue(QString("bandBackgroundColor"), QVariant::fromValue<QVector4D>(bandBackgroundColor));

    // Line settings.
    settings->setValue(QString("minRadiusFactor"), minRadiusFactor);
    settings->setValue(QString("fiberRadius"), fiberRadius);

    // Lighting settings.
    settings->setValue(QString("useColorIntensity"), useColorIntensity);
    settings->setValue(QString("materialConstantAmbient"), materialConstantAmbient);
    settings->setValue(QString("materialConstantDiffuse"), materialConstantDiffuse);
    settings->setValue(QString("materialConstantSpecular"), materialConstantSpecular);
    settings->setValue(QString("materialConstantSpecularExp"), materialConstantSpecularExp);
    settings->setValue(QString("drawHalo"), drawHalo);
    settings->setValue(QString("haloFactor"), haloFactor);
}

void MMultiVarData::loadConfiguration(QSettings *settings)
{
    int numVariables = settings->value("numVariables", 0).toInt();
    initTransferFunctionsMultiVar(numVariables);

    varNames.clear();
    selectedVariables.clear();
    for (int varIdx = 0; varIdx < numVariables; varIdx++)
    {
        QString tfName = settings->value(QString("transferFunction#%1").arg(varIdx + 1)).toString();
        QString varName = settings->value(QString("varName#%1").arg(varIdx + 1)).toString();
        bool varSelected = settings->value(QString("varSelected#%1").arg(varIdx + 1)).toBool();
        varNames.push_back(varName);
        selectedVariables.push_back(varSelected);
        while (!setTransferFunctionMultiVar(varIdx, tfName))
        {
            if (!MTransferFunction::loadMissingTransferFunction(
                    tfName, MTransferFunction1D::staticActorType(),
                    "Trajectories Actor ", actor->getName(), settings))
            {
                break;
            }
        }
    }
    maxNumVariables = varNames.size();
    updateNumVariablesSelected();
    selectedVariablesChanged = true;

    // Multi-variable settings.
    numLineSegments = settings->value("numLineSegments", 8).toInt();
    numInstances = settings->value("numInstances", 12).toInt();
    rollWidth = settings->value("rollWidth", 1).toInt();
    separatorWidth = settings->value("separatorWidth", 0.15f).toFloat();
    mapTubeDiameter = settings->value("mapTubeDiameter", false).toBool();
    twistOffset = settings->value("twistOffset", 0.1f).toFloat();
    constantTwistOffset = settings->value("constantTwistOffset", false).toBool();
    checkerboardWidth = settings->value("checkerboardWidth", 3).toInt();
    checkerboardHeight = settings->value("checkerboardHeight", 2).toInt();
    checkerboardIterator = settings->value("checkerboardIterator", 2).toInt();
    bandBackgroundColor = settings->value(
            "bandBackgroundColor",
            QVector4D(0.5f, 0.5f, 0.5f, 1.0f)).value<QVector4D>();

    // Line settings.
    minRadiusFactor = settings->value("minRadiusFactor", 0.5f).toFloat();
    fiberRadius = settings->value("fiberRadius", 0.05f).toFloat();

    // Lighting settings.
    useColorIntensity = settings->value("useColorIntensity", true).toBool();
    materialConstantAmbient = settings->value("materialConstantAmbient", 0.2f).toFloat();
    materialConstantDiffuse = settings->value("materialConstantDiffuse", 0.7f).toFloat();
    materialConstantSpecular = settings->value("materialConstantSpecular", 0.5f).toFloat();
    materialConstantSpecularExp = settings->value("materialConstantSpecularExp", 8.0f).toFloat();
    drawHalo = settings->value("drawHalo", true).toBool();
    haloFactor = settings->value("haloFactor", 1.2f).toFloat();
}


void MMultiVarData::onActorCreated(MActor *actor)
{
    // If the new actor is a transfer function, add it to the list of
    // available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        // Don't render while the properties are being updated.
        actor->enableEmissionOfActorChangedSignal(false);

        foreach(QtProperty *tfProperty, tfPropertiesMultiVar)
        {
            int index = properties->mEnum()->value(tfProperty);
            QStringList availableTFs = properties->mEnum()->enumNames(tfProperty);
            availableTFs << tf->transferFunctionName();
            properties->mEnum()->setEnumNames(tfProperty, availableTFs);
            properties->mEnum()->setValue(tfProperty, index);
        }

        actor->enableEmissionOfActorChangedSignal(true);
    }
}


void MMultiVarData::onActorDeleted(MActor *actor)
{
    // If the deleted actor is a transfer function, remove it from the list of
    // available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        this->actor->enableEmissionOfActorChangedSignal(false);

        foreach(QtProperty *tfProperty, tfPropertiesMultiVar)
        {
            QString tFName = properties->getEnumItem(tfProperty);
            QStringList availableTFs = properties->mEnum()->enumNames(tfProperty);

            availableTFs.removeOne(tf->getName());

            // Get the current index of the transfer function selected. If the
            // transfer function is the one to be deleted, the selection is set to
            // 'None'.
            int index = availableTFs.indexOf(tFName);

            properties->mEnum()->setEnumNames(tfProperty, availableTFs);
            properties->mEnum()->setValue(tfProperty, index);
        }

        this->actor->enableEmissionOfActorChangedSignal(true);
    }
}


void MMultiVarData::onActorRenamed(MActor *actor, QString oldName)
{
    // If the renamed actor is a transfer function, change its name in the list
    // of available transfer functions.
    if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
    {
        this->actor->enableEmissionOfActorChangedSignal(false);

        foreach(QtProperty *tfProperty, tfPropertiesMultiVar)
        {
            int index = properties->mEnum()->value(tfProperty);
            QStringList availableTFs = properties->mEnum()->enumNames(tfProperty);

            // Replace affected entry.
            availableTFs[availableTFs.indexOf(oldName)] = tf->getName();

            properties->mEnum()->setEnumNames(tfProperty, availableTFs);
            properties->mEnum()->setValue(tfProperty, index);
        }

        this->actor->enableEmissionOfActorChangedSignal(true);
    }
}


void MMultiVarData::initTransferFunctionsMultiVar(uint32_t numVariables)
{
    tfPropertiesMultiVar.resize(numVariables);
    transferFunctionsMultiVar.resize(numVariables);
    for (uint32_t varIdx = 0; varIdx < numVariables; varIdx++)
    {
        // Transfer function.
        // Scan currently available actors for transfer functions. Add TFs to
        // the list displayed in the combo box of the transferFunctionProperty.
        QStringList availableTFs;
        availableTFs << "None";
        MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
        foreach (MActor *mactor, glRM->getActors())
        {
            if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(mactor))
            {
                availableTFs << tf->transferFunctionName();
            }
        }
        QString name = QString("transfer function #%1 (%2)").arg(
                QString::number(varIdx + 1), varNames.at(varIdx));
        tfPropertiesMultiVar[varIdx] = addProperty(ENUM_PROPERTY, name, multiVarGroupProperty);
        properties->mEnum()->setEnumNames(tfPropertiesMultiVar[varIdx], availableTFs);
        tfPropertiesMultiVar[varIdx]->setToolTip(
                "This transfer function is used for mapping either pressure or the "
                "selected auxiliary variable to the trajectory's colour.");
    }
}


void MMultiVarData::setTransferFunctionMultiVar(int varIdx, MTransferFunction1D *tf)
{
    transferFunctionsMultiVar[varIdx] = tf;
}


bool MMultiVarData::setTransferFunctionMultiVar(int varIdx, QString tfName)
{
    QStringList tfNames = properties->mEnum()->enumNames(
            tfPropertiesMultiVar[varIdx]);
    int tfIndex = tfNames.indexOf(tfName);

    if (tfIndex >= 0)
    {
        properties->mEnum()->setValue(tfPropertiesMultiVar[varIdx], tfIndex);
        return true;
    }

    // Set transfer function property to "None".
    properties->mEnum()->setValue(tfPropertiesMultiVar[varIdx], 0);

    return false; // the given tf name could not be found
}


void MMultiVarData::setTransferFunctionMultiVarFromProperty(int varIdx)
{
    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();

    QString tfName = properties->getEnumItem(tfPropertiesMultiVar[varIdx]);

    if (tfName == "None")
    {
        transferFunctionsMultiVar[varIdx] = nullptr;
        return;
    }

    // Find the selected transfer function in the list of actors from the
    // resources manager. Not very efficient, but works well enough for the
    // small number of actors at the moment..
    foreach (MActor *actor, glRM->getActors())
    {
        if (MTransferFunction1D *tf = dynamic_cast<MTransferFunction1D*>(actor))
        {
            if (tf->transferFunctionName() == tfName)
            {
                transferFunctionsMultiVar[varIdx] = tf;
                return;
            }
        }
    }
}


QtProperty* MMultiVarData::addProperty(MQtPropertyType type, const QString& name, QtProperty *group)
{
    return actor->addProperty(type, name, group);
}


bool MMultiVarData::hasProperty(QtProperty *property)
{
    return propertyList.contains(property) || tfPropertiesMultiVar.contains(property);
}


void MMultiVarData::onQtPropertyChanged(QtProperty *property)
{
    if (property == renderTechniqueProperty)
    {
        multiVarRenderMode = MultiVarRenderMode(properties->mEnum()->value(renderTechniqueProperty));
        reloadShaderEffect();
    }
    else if (tfPropertiesMultiVar.contains(property))
    {
        setTransferFunctionMultiVarFromProperty(tfPropertiesMultiVar.indexOf(property));
        multiVarTf.generateTexture1DArray();
        if (actor->suppressActorUpdates()) return;
        actor->emitActorChangedSignal();
    }
    else if (property == mapTubeDiameterProperty)
    {
        mapTubeDiameter = properties->mBool()->value(mapTubeDiameterProperty);
    }
    else if (property == radiusMappingModeProperty)
    {
        multiVarRadiusMappingMode = MultiVarRadiusMappingMode(properties->mEnum()->value(radiusMappingModeProperty));
    }
    else if (property == checkerboardHeightProperty)
    {
        checkerboardHeight = properties->mInt()->value(checkerboardHeightProperty);
    }
    else if (property == checkerboardWidthProperty)
    {
        checkerboardWidth = properties->mInt()->value(checkerboardWidthProperty);
    }
    else if (property == checkerboardIteratorProperty)
    {
        checkerboardIterator = properties->mInt()->value(checkerboardIteratorProperty);
    }
    else if (property == twistOffsetProperty)
    {
        twistOffset = properties->mDDouble()->value(twistOffsetProperty);
    }
    else if (property == constantTwistOffsetProperty)
    {
        constantTwistOffset = properties->mBool()->value(constantTwistOffsetProperty);
    }
    else if (property == orientedRibbonModeProperty)
    {
        orientedRibbonMode = OrientedRibbonMode(properties->mEnum()->value(orientedRibbonModeProperty));
        reloadShaderEffect();
    }
    else if (property == bandBackgroundColorProperty)
    {
        bandBackgroundColor = vecFromColor(properties->mColor()->value(bandBackgroundColorProperty));
    }
    else if (property == separatorWidthProperty)
    {
        separatorWidth = properties->mDDouble()->value(separatorWidthProperty);
    }
    else if (property == useColorIntensityProperty)
    {
        useColorIntensity = properties->mBool()->value(useColorIntensityProperty);
    }

    // --- Group: Rendering settings ---
    else if (property == numLineSegmentsProperty)
    {
        numLineSegments = properties->mBool()->value(numLineSegmentsProperty);
        numInstances = properties->mBool()->value(numLineSegmentsProperty);
    }
    else if (property == fiberRadiusProperty)
    {
        fiberRadius = properties->mDDouble()->value(fiberRadiusProperty);
    }
    else if (property == minRadiusFactorProperty)
    {
        minRadiusFactor = properties->mDDouble()->value(minRadiusFactorProperty);
    }
    else if (property == rollWidthProperty)
    {
        rollWidth = properties->mInt()->value(rollWidthProperty);
    }

    else if (property == materialConstantAmbientProperty)
    {
        materialConstantAmbient = properties->mDDouble()->value(materialConstantAmbientProperty);
    }
    else if (property == materialConstantDiffuseProperty)
    {
        materialConstantDiffuse = properties->mDDouble()->value(materialConstantDiffuseProperty);
    }
    else if (property == materialConstantSpecularProperty)
    {
        materialConstantSpecular = properties->mDDouble()->value(materialConstantSpecularProperty);
    }
    else if (property == materialConstantSpecularExpProperty)
    {
        materialConstantSpecularExp = properties->mDDouble()->value(materialConstantSpecularExpProperty);
    }
    else if (property == drawHaloProperty)
    {
        drawHalo = properties->mBool()->value(drawHaloProperty);
    }
    else if (property == haloFactorProperty)
    {
        haloFactor = properties->mDDouble()->value(haloFactorProperty);
    }

    // --- Group: Selected variables ---
    else if (selectedVariablesProperties.contains(property))
    {
        int varIdx = selectedVariablesProperties.indexOf(property);
        selectedVariables[varIdx] = properties->mBool()->value(property);
        updateNumVariablesSelected();
        selectedVariablesChanged = true;
    }


    if (propertyList.contains(property))
    {
        updateModeEnabledProperties();
    }
}


void MMultiVarData::onBezierTrajectoriesLoaded(const QStringList& auxDataVarNames)
{
    if (tfPropertiesMultiVar.empty())
    {
        QStringList varNames = auxDataVarNames;
        if (varNames.empty())
        {
            varNames = QStringList("Pressure");
        }
        this->varNames = varNames;
        maxNumVariables = varNames.size();
        initTransferFunctionsMultiVar(maxNumVariables);

        selectedVariables.resize(maxNumVariables);
        for (int varIdx = 0; varIdx < maxNumVariables; varIdx++)
        {
            selectedVariables[varIdx] = true;
        }
        updateNumVariablesSelected();
        selectedVariablesChanged = true;

        setPropertiesVarSelected();
    }
}


void MMultiVarData::setUniformData(int textureUnitTransferFunction)
{
    shaderEffect->setUniformValue("numVariables", std::min(numVariablesSelected, MAX_NUM_VARIABLES));
    shaderEffect->setUniformValue("maxNumVariables", maxNumVariables);
    shaderEffect->setUniformValue("materialAmbient", materialConstantAmbient);
    shaderEffect->setUniformValue("materialDiffuse", materialConstantDiffuse);
    shaderEffect->setUniformValue("materialSpecular", materialConstantSpecular);
    shaderEffect->setUniformValue("materialSpecularExp", materialConstantSpecularExp);
    shaderEffect->setUniformValue("drawHalo", drawHalo);
    shaderEffect->setUniformValue("haloFactor", haloFactor);
    shaderEffect->setUniformValue("separatorWidth", separatorWidth);
    shaderEffect->setUniformValue("useColorIntensity", int(useColorIntensity));

    if (multiVarRenderMode != MultiVarRenderMode::ORIENTED_COLOR_BANDS)
    {
        shaderEffect->setUniformValue("mapTubeDiameter", mapTubeDiameter);
        shaderEffect->setUniformValue("mapTubeDiameterMode", int(multiVarRadiusMappingMode));
        shaderEffect->setUniformValue("minRadiusFactor", minRadiusFactor);
    }
    if (multiVarRenderMode == MultiVarRenderMode::ROLLS)
    {
        shaderEffect->setUniformValue("rollWidth", rollWidth);
    }
    if (multiVarRenderMode == MultiVarRenderMode::FIBERS)
    {
        shaderEffect->setUniformValue("fiberRadius", fiberRadius);
    }
    if (multiVarRenderMode == MultiVarRenderMode::CHECKERBOARD)
    {
        shaderEffect->setUniformValue("checkerboardWidth", checkerboardWidth);
        shaderEffect->setUniformValue("checkerboardHeight", checkerboardHeight);
        shaderEffect->setUniformValue("checkerboardIterator", checkerboardIterator);
    }
    if (multiVarRenderMode == MultiVarRenderMode::TWISTED_ROLLS)
    {
        shaderEffect->setUniformValue("twistOffset", twistOffset);
        shaderEffect->setUniformValue("constantTwistOffset", constantTwistOffset);
    }
    if (multiVarRenderMode == MultiVarRenderMode::ORIENTED_COLOR_BANDS
            && orientedRibbonMode == OrientedRibbonMode::VARYING_BAND_WIDTH)
    {
        shaderEffect->setUniformValue("bandBackgroundColor", bandBackgroundColor);
    }

    multiVarTf.bindTexture1DArray(textureUnitTransferFunction);
    shaderEffect->setUniformValue(
            "transferFunctionTexture", textureUnitTransferFunction);
    multiVarTf.getMinMaxBuffer()->bindToIndex(9);
}


std::shared_ptr<GL::MShaderEffect> MMultiVarData::getShaderEffect()
{
    if (isDirty)
    {
        reloadShaderEffect();
    }
    return shaderEffect;
}


void MMultiVarData::reloadShaderEffect()
{
    QMap<QString, QString> defines =
    {
            {"NUM_INSTANCES", QString::fromStdString(std::to_string(numVariablesSelected)) },
            {"NUM_SEGMENTS", QString::fromStdString(std::to_string(numLineSegments)) },
            {"NUM_LINESEGMENTS", QString::fromStdString(std::to_string(numInstances)) },
            {"MAX_NUM_VARIABLES", QString::fromStdString(std::to_string(MAX_NUM_VARIABLES)) },
            {"USE_MULTI_VAR_TRANSFER_FUNCTION", QString::fromStdString("") },
            {"IS_MULTIVAR_DATA", QString::fromStdString("") }
    };

    if (multiVarRenderMode == MultiVarRenderMode::ORIENTED_COLOR_BANDS)
    {
        if (!mapColorToSaturation)
        {
            defines.insert("DIRECT_COLOR_MAPPING", "");
        }
        defines.insert("ORIENTED_RIBBON_MODE", QString::fromStdString(std::to_string((int)orientedRibbonMode)));
    }

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    glRM->generateEffectProgramUncached(
            "multivar_oriented_color_bands", shaderEffect);
    shaderEffect->compileFromFile_Met3DHome(
            renderingTechniqueShaderFilenames[int(multiVarRenderMode)], defines);
    isDirty = false;
}

}
