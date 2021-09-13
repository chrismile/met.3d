/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2016-2018 Bianca Tost
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
#include "transferfunction1d.h"

// standard library imports
#include <iostream>

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <QFileDialog>

// local application imports
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/gl/typedvertexbuffer.h"
#include "util/mutil.h"
#include "transferfunctioneditor/transferfunctioneditor.h"
#include "mainwindow.h"

using namespace std;


namespace Met3D
{

using namespace TFEditor;

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTransferFunction1D::MTransferFunction1D(QObject *parent)
    : MTransferFunction(parent),
      editor(nullptr),
      enableAlpha(true)
{
    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setActorType(staticActorType());
    setName(getActorType());

    // Properties related to labelling the colour bar.
    // ===============================================

    scaleFactorProperty = addProperty(DOUBLE_PROPERTY, "label value scaling",
                                      labelPropertiesSupGroup);
    properties->setDouble(scaleFactorProperty, 1., 5, 0.1);

    // Disable label bbox by default.
    properties->mBool()->setValue(labelBBoxProperty, false);

    // Properties related to data range.
    // =================================

    rangePropertiesSubGroup->removeSubProperty(valueOptionsPropertiesSubGroup);

    numStepsProperty = addProperty(INT_PROPERTY, "steps",
                                   rangePropertiesSubGroup);
    properties->setInt(numStepsProperty, 50, 2, 32768, 1);

    rangePropertiesSubGroup->addSubProperty(valueOptionsPropertiesSubGroup);

    // General properties.
    // ===================

    enableAlphaInTFProperty = addProperty(BOOL_PROPERTY, "display opacity",
                                          actorPropertiesSupGroup);
    properties->mBool()->setValue(enableAlphaInTFProperty, enableAlpha);

    reverseTFRangeProperty = addProperty(BOOL_PROPERTY, "reverse range",
                                        actorPropertiesSupGroup);
    properties->mBool()->setValue(reverseTFRangeProperty, false);

    // Properties related to type of colourmap.
    // ========================================

    colourmapTypeProperty = addProperty(ENUM_PROPERTY, "colourmap type",
                                        actorPropertiesSupGroup);

    QStringList cmapTypes = QStringList() << "HCL" << "Editor" << "predefined"
                                          << "HSV";

    properties->mEnum()->setEnumNames(colourmapTypeProperty, cmapTypes);

    // HCL ...

    hclCMapPropertiesSubGroup = addProperty(GROUP_PROPERTY, "HCL",
                                            actorPropertiesSupGroup);
    hclCMapPropertiesSubGroup->setEnabled(true);

    QStringList hclTypes = QStringList() << "diverging" << "qualitative"
                                         << "sequential single hue"
                                         << "sequential multiple hue";
    hclTypeProperty = addProperty(ENUM_PROPERTY, "type",
                                  hclCMapPropertiesSubGroup);
    properties->mEnum()->setEnumNames(hclTypeProperty, hclTypes);
    properties->mEnum()->setValue(hclTypeProperty,
                                  MHCLType::SEQUENTIAL_MULTIPLE_HUE);

    hclHue1Property = addProperty(DOUBLE_PROPERTY, "hue 1",
                                  hclCMapPropertiesSubGroup);
    properties->setDouble(hclHue1Property, 0., -360., 360., 1., 1.);

    hclHue2Property = addProperty(DOUBLE_PROPERTY, "hue 2",
                                  hclCMapPropertiesSubGroup);
    properties->setDouble(hclHue2Property, 90., -360., 360., 1., 1.);

    hclChroma1Property = addProperty(DOUBLE_PROPERTY, "chroma 1",
                                     hclCMapPropertiesSubGroup);
    properties->setDouble(hclChroma1Property, 80., 0., 100., 1., 1.);

    hclChroma2Property = addProperty(DOUBLE_PROPERTY, "chroma 2",
                                     hclCMapPropertiesSubGroup);
    properties->setDouble(hclChroma2Property, 30., 0., 100., 1., 1.);

    hclLuminance1Property = addProperty(DOUBLE_PROPERTY, "luminance 1",
                                        hclCMapPropertiesSubGroup);
    properties->setDouble(hclLuminance1Property, 30., 0., 100., 1., 1.);

    hclLuminance2Property = addProperty(DOUBLE_PROPERTY, "luminance 2",
                                        hclCMapPropertiesSubGroup);
    properties->setDouble(hclLuminance2Property, 90., 0., 100., 1., 1.);

    hclPower1Property = addProperty(DOUBLE_PROPERTY, "power 1/C",
                                    hclCMapPropertiesSubGroup);
    properties->setDouble(hclPower1Property, .2, 0., 100., 2., 0.1);

    hclPower2Property = addProperty(DOUBLE_PROPERTY, "power 2/L",
                                    hclCMapPropertiesSubGroup);
    properties->setDouble(hclPower2Property, 2., 0., 100., 2., 0.1);

    hclAlpha1Property = addProperty(DOUBLE_PROPERTY, "alpha 1",
                                    hclCMapPropertiesSubGroup);
    properties->setDouble(hclAlpha1Property, 1., 0., 1., 3., 0.01);

    hclAlpha2Property = addProperty(DOUBLE_PROPERTY, "alpha 2",
                                    hclCMapPropertiesSubGroup);
    properties->setDouble(hclAlpha2Property, 1., 0., 1., 3., 0.01);

    hclPowerAlphaProperty = addProperty(DOUBLE_PROPERTY, "power alpha",
                                        hclCMapPropertiesSubGroup);
    properties->setDouble(hclPowerAlphaProperty, 1., 0., 100., 3., 0.01);

    updateHCLProperties();


    // Editor
    editorPropertiesSubGroup = addProperty(GROUP_PROPERTY, "Editor",
                                            actorPropertiesSupGroup);
    editorPropertiesSubGroup->setEnabled(false);

    editorClickProperty = addProperty(CLICK_PROPERTY, "open",
                                      editorPropertiesSubGroup);

    // Predefined ...

    predefCMapPropertiesSubGroup = addProperty(GROUP_PROPERTY, "predefined",
                                               actorPropertiesSupGroup);
    predefCMapPropertiesSubGroup->setEnabled(false);

    QStringList availableColourmaps = colourmapPool.availableColourmaps();
    availableColourmaps.sort();
    predefColourmapProperty = addProperty(ENUM_PROPERTY, "colour map",
                                          predefCMapPropertiesSubGroup);
    properties->mEnum()->setEnumNames(predefColourmapProperty, availableColourmaps);

    predefLightnessAdjustProperty = addProperty(INT_PROPERTY, "lightness",
                                                predefCMapPropertiesSubGroup);
    properties->mInt()->setValue(predefLightnessAdjustProperty, 0);

    predefSaturationAdjustProperty = addProperty(INT_PROPERTY, "saturation",
                                                 predefCMapPropertiesSubGroup);
    properties->mInt()->setValue(predefSaturationAdjustProperty, 0);


    // HSV ...

    hsvCMapPropertiesSubGroup = addProperty(GROUP_PROPERTY, "HSV",
                                            actorPropertiesSupGroup);
    hsvCMapPropertiesSubGroup->setEnabled(false);

    hsvLoadFromVaporXMLProperty = addProperty(CLICK_PROPERTY, "load from Vapor XML file",
                                              hsvCMapPropertiesSubGroup);

    hsvVaporXMLFilenameProperty = addProperty(STRING_PROPERTY, "Vapor XML file",
                                              hsvCMapPropertiesSubGroup);
    properties->mString()->setValue(hsvVaporXMLFilenameProperty, "");
    hsvVaporXMLFilenameProperty->setEnabled(false);


    endInitialiseQtProperties();

    MSystemManagerAndControl *sysMC = MSystemManagerAndControl::getInstance();
    // Assign main window as parent so editor gets closed automatically if the
    // user closes the main window.
    editor = new MTransferFunctionEditor(sysMC->getMainWindow());
    editor->resize(700, 200);

    connect(editor, SIGNAL(transferFunctionChanged()),
            this, SLOT(onEditorTransferFunctionChanged()));
}

MTransferFunction1D::~MTransferFunction1D()
{
    if (editor)
    {
        delete editor;
    }
    editor = nullptr;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE  0
#define SHADER_TEXTURE_ATTRIBUTE 1

void MTransferFunction1D::selectPredefinedColourmap(
        QString name, bool reversed, int saturation, int lightness)
{
    QStringList colourmapNames =
            properties->mEnum()->enumNames(predefColourmapProperty);

    int index = colourmapNames.indexOf(name);
    if (index >= 0)
    {
        enableActorUpdates(false);

        properties->mEnum()->setValue(colourmapTypeProperty, int(PREDEFINED));
        properties->mEnum()->setValue(predefColourmapProperty, index);
        properties->mBool()->setValue(reverseTFRangeProperty, reversed);
        properties->mInt()->setValue(predefSaturationAdjustProperty, saturation);
        properties->mInt()->setValue(predefLightnessAdjustProperty, lightness);
        predefCMapPropertiesSubGroup->setEnabled(true);
        hclCMapPropertiesSubGroup->setEnabled(false);
        hsvCMapPropertiesSubGroup->setEnabled(false);
        editorPropertiesSubGroup->setEnabled(false);

        enableActorUpdates(true);

        if (isInitialized())
        {
            generateTransferTexture();
            generateBarGeometry();
            emit transferFunctionChanged(this);
            emitActorChangedSignal();
        }
    }
}


void MTransferFunction1D::selectHCLColourmap(
        MTransferFunction1D::MHCLType type,
        float hue1, float hue2, float chroma1, float chroma2,
        float luminance1, float luminance2, float power1, float power2,
        float alpha1, float alpha2, float poweralpha, bool reversed)
{
    enableActorUpdates(false);

    properties->mEnum()->setValue(colourmapTypeProperty, int(HCL));
    properties->mEnum()->setValue(hclTypeProperty, int(type));
    properties->mDouble()->setValue(hclHue1Property, hue1);
    properties->mDouble()->setValue(hclHue2Property, hue2);
    properties->mDouble()->setValue(hclChroma1Property, chroma1);
    properties->mDouble()->setValue(hclChroma2Property, chroma2);
    properties->mDouble()->setValue(hclLuminance1Property, luminance1);
    properties->mDouble()->setValue(hclLuminance2Property, luminance2);
    properties->mDouble()->setValue(hclPower1Property, power1);
    properties->mDouble()->setValue(hclPower2Property, power2);
    properties->mDouble()->setValue(hclAlpha1Property, alpha1);
    properties->mDouble()->setValue(hclAlpha2Property, alpha2);
    properties->mDouble()->setValue(hclPowerAlphaProperty, poweralpha);
    properties->mBool()->setValue(reverseTFRangeProperty, reversed);

    predefCMapPropertiesSubGroup->setEnabled(false);
    hclCMapPropertiesSubGroup->setEnabled(true);
    hsvCMapPropertiesSubGroup->setEnabled(false);
    editorPropertiesSubGroup->setEnabled(false);

    enableActorUpdates(true);

    if (isInitialized())
    {
        generateTransferTexture();
        generateBarGeometry();
        emit transferFunctionChanged(this);
        emitActorChangedSignal();
    }
}


void MTransferFunction1D::selectHSVColourmap(
        QString vaporXMLFile, bool reversed)
{
    enableActorUpdates(false);

    properties->mEnum()->setValue(colourmapTypeProperty, int(HSV));

    hsvVaporXMLFilename = vaporXMLFile;
    properties->mString()->setValue(
                hsvVaporXMLFilenameProperty, hsvVaporXMLFilename);

    properties->mBool()->setValue(reverseTFRangeProperty, reversed);

    predefCMapPropertiesSubGroup->setEnabled(false);
    hclCMapPropertiesSubGroup->setEnabled(false);
    hsvCMapPropertiesSubGroup->setEnabled(true);
    editorPropertiesSubGroup->setEnabled(false);

    enableActorUpdates(true);

    if (isInitialized())
    {
        generateTransferTexture();
        generateBarGeometry();
        emit transferFunctionChanged(this);
        emitActorChangedSignal();
    }
}


void MTransferFunction1D::selectEditor()
{
    enableActorUpdates(false);

    properties->mEnum()->setValue(colourmapTypeProperty, int(EDITOR));
    predefCMapPropertiesSubGroup->setEnabled(false);
    hclCMapPropertiesSubGroup->setEnabled(false);
    hsvCMapPropertiesSubGroup->setEnabled(false);
    editorPropertiesSubGroup->setEnabled(true);

    enableActorUpdates(true);
}


void MTransferFunction1D::setSteps(int steps)
{
    properties->mInt()->setValue(numStepsProperty, steps);
}


void MTransferFunction1D::saveConfiguration(QSettings *settings)
{
    MTransferFunction::saveConfiguration(settings);

    settings->beginGroup(MTransferFunction1D::getSettingsID());

    // Properties related to labelling the colour bar.
    // ===============================================
    settings->setValue("labelValueScaling",
                       properties->mDouble()->value(scaleFactorProperty));

    // Properties related to data range.
    // =================================
    settings->setValue("numSteps",
                       properties->mInt()->value(numStepsProperty));

    // General properties.
    // ===================

    // Properties related to type of colourmap.
    // ========================================
    MColourmapType cmaptype = MColourmapType(
                properties->mEnum()->value(colourmapTypeProperty));
    settings->setValue("colourMapType",
                       colourMapTypeToString(cmaptype));
    settings->setValue("displayOpacity",
                       properties->mBool()->value(enableAlphaInTFProperty));
    settings->setValue("reverseColourMap",
                       properties->mBool()->value(reverseTFRangeProperty));

    switch (cmaptype)
    {
    case PREDEFINED:
    {
        int colourmapIndex = properties->mEnum()->value(predefColourmapProperty);
        QString colourmapName = properties->mEnum()->enumNames(
                    predefColourmapProperty)[colourmapIndex];
        settings->setValue("predefinedColourMap", colourmapName);
        settings->setValue("lightnessAdjust",
                           properties->mInt()->value(predefLightnessAdjustProperty));
        settings->setValue("saturationAdjust",
                           properties->mInt()->value(predefSaturationAdjustProperty));
        break;
    }
    case HCL:
    {
        MHCLType hcltype = MHCLType(properties->mEnum()->value(hclTypeProperty));
        settings->setValue("hclType", int(hcltype));

        settings->setValue("hue1",
                           properties->mDouble()->value(hclHue1Property));
        settings->setValue("hue2",
                           properties->mDouble()->value(hclHue2Property));
        settings->setValue("chroma1",
                           properties->mDouble()->value(hclChroma1Property));
        settings->setValue("chroma2",
                           properties->mDouble()->value(hclChroma2Property));
        settings->setValue("luminance1",
                           properties->mDouble()->value(hclLuminance1Property));
        settings->setValue("luminance2",
                           properties->mDouble()->value(hclLuminance2Property));
        settings->setValue("power1",
                           properties->mDouble()->value(hclPower1Property));
        settings->setValue("power2",
                           properties->mDouble()->value(hclPower2Property));
        settings->setValue("alpha1",
                           properties->mDouble()->value(hclAlpha1Property));
        settings->setValue("alpha2",
                           properties->mDouble()->value(hclAlpha2Property));
        settings->setValue("poweralpha",
                           properties->mDouble()->value(hclPowerAlphaProperty));
        break;
    }
    case HSV:
    {
        settings->setValue("vaporXMLFile", hsvVaporXMLFilename);
        break;
    }
    case EDITOR:
    {
        const auto colourNodes = editor->getTransferFunction()->getColourNodes();
        const auto alphaNodes = editor->getTransferFunction()->getAlphaNodes();

        settings->beginWriteArray("colourNode");
        for (int i= 0; i < colourNodes->getNumNodes(); i++)
        {
            settings->setArrayIndex(i);
            settings->setValue("position", colourNodes->xAt(i));

            MColourXYZ64 colour = colourNodes->colourAt(i);
            QByteArray array((char*)&colour, sizeof(MColourXYZ64));
            settings->setValue("colour", array);
        }
        settings->endArray();

        settings->beginWriteArray("alphaNode");
        for (int i= 0; i < alphaNodes->getNumNodes(); i++)
        {
            settings->setArrayIndex(i);
            settings->setValue("position", alphaNodes->xAt(i));
            settings->setValue("alpha", alphaNodes->yAt(i));
        }
        settings->endArray();

        QString type = editor->interpolationCSpaceToString(editor->getCSpaceForCNodeInterpolation());
        settings->setValue("colourSpaceForColourNodeInterpolation", type);

        break;
    }
    case INVALID:
    {
        break;
    }
    }

    settings->endGroup();
}


void MTransferFunction1D::loadConfiguration(QSettings *settings)
{
    MTransferFunction::loadConfiguration(settings);

    settings->beginGroup(MTransferFunction1D::getSettingsID());

    // Properties related to labelling the colour bar.
    // ===============================================
    properties->mDouble()->setValue(
                scaleFactorProperty,
                settings->value("labelValueScaling", 1.).toDouble());

    // Properties related to data range.
    // =================================
    setSteps(settings->value("numSteps", 50).toInt());

    // Properties related to type of colourmap.
    // ========================================
    QString colourMapTypeString =
            settings->value("colourMapType", "hcl").toString();
    MColourmapType cmaptype = stringToColourMapType(colourMapTypeString);

    switch (cmaptype)
    {
    // Display error message and continue with colour map type 'hcl'
    // initialisation.
    case INVALID:
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(QString("Error reading configuration file: "
                               "Could not find colour map type '%1'.\n"
                               "Setting colour map type to 'hcl'.")
                       .arg(colourMapTypeString));
        msgBox.exec();

        cmaptype = stringToColourMapType(QString("hcl"));
        // No break to continue directly with setting type to predefined.
    }
    case HCL:
    {
        selectHCLColourmap(MHCLType(settings->value(
                                        "hclType",
                                        MHCLType::SEQUENTIAL_MULTIPLE_HUE)
                                    .toInt()),
                           settings->value("hue1", 0.f).toFloat(),
                           settings->value("hue2", 90.f).toFloat(),
                           settings->value("chroma1", 80.f).toFloat(),
                           settings->value("chroma2", 30.f).toFloat(),
                           settings->value("luminance1", 30.f).toFloat(),
                           settings->value("luminance2", 90.f).toFloat(),
                           settings->value("power1", .2f).toFloat(),
                           settings->value("power2", 2.f).toFloat(),
                           settings->value("alpha1", 1.f).toFloat(),
                           settings->value("alpha2", 1.f).toFloat(),
                           settings->value("poweralpha", 1.f).toFloat(),
                           settings->value("reverseColourMap", false).toBool());
        break;
    }
    case EDITOR:
    {
        auto colourNodes = editor->getTransferFunction()->getColourNodes();
        auto alphaNodes = editor->getTransferFunction()->getAlphaNodes();

        colourNodes->clear();
        alphaNodes->clear();

        int numColourNodes = settings->beginReadArray("colourNode");
        for (int i = 0; i < numColourNodes; i++)
        {
            settings->setArrayIndex(i);
            float pos = settings->value("position", 0).toFloat();

            QByteArray array =
                    settings->value("colour", QByteArray()).toByteArray();
            MColourXYZ64 colour;
            if (array.size() == sizeof(MColourXYZ64))
                memcpy(&colour, array.data(), sizeof(MColourXYZ64));
            colourNodes->push_back(pos, colour);
        }
        settings->endArray();

        int numAlphaPoints = settings->beginReadArray("alphaNode");
        for (int i= 0; i < numAlphaPoints; i++)
        {
            settings->setArrayIndex(i);
            float pos = settings->value("position", 0).toFloat();
            float alpha = settings->value("alpha", 0.0f).toFloat();

            alphaNodes->push_back(pos, alpha);
        }
        settings->endArray();
        QString typeString =
                settings->value("colourSpaceForColourNodeInterpolation",
                                "hcl").toString();
        ColourSpaceForColourNodeInterpolation type =
                editor->stringToInterpolationCSpace(typeString);
        // Display error message and set type to hcl for strings in configuration
        // not defining a type.
        if (type == ColourSpaceForColourNodeInterpolation::INVALID)
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(QString("Error reading configuration file: "
                                   "Could not find colour space '%1' for interpolation.\n"
                                   "Setting colour space to 'hcl'.")
                           .arg(typeString));
            msgBox.exec();

            type = editor->stringToInterpolationCSpace(QString("hcl"));
        }
        editor->setCSpaceForCNodeInterpolation(type);

        editor->resetUI();
        selectEditor();
        break;
    }
    case PREDEFINED:
    {
        QStringList availableColourmaps = colourmapPool.availableColourmaps();
        availableColourmaps.sort();
        selectPredefinedColourmap(settings->value(
                                      "predefinedColourMap",
                                      availableColourmaps.at(0)).toString(),
                                  settings->value("reverseColourMap",
                                                  false).toBool(),
                                  settings->value("saturationAdjust", 0).toInt(),
                                  settings->value("lightnessAdjust", 0).toInt());
        break;
    }
    case HSV:
    {
        selectHSVColourmap(settings->value("vaporXMLFile", "").toString(),
                           settings->value("reverseColourMap", false).toBool());
        break;
    }
    }

    settings->endGroup();

    if (isInitialized())
    {
        generateTransferTexture();
        generateBarGeometry();
    }
}


QColor MTransferFunction1D::getColorValue(const float scalar) const
{
    float t = (scalar - minimumValue) / (maximumValue - minimumValue);
    t = std::min(std::max(t, 0.0f), 1.0f);
    const int numColors = colorValues.size() / 4;

    float index = 0;
    float fract = std::modf(t * (numColors - 1), &index);
    index = std::max(std::min(index, float(numColors - 1)), 0.0f);

    int minIndex = static_cast<int>(index) * 4;
    int maxIndex = std::min(static_cast<int>(index + 1), numColors - 1) * 4;

    auto r = static_cast<unsigned char>((fract * colorValues[maxIndex++]
                                 + (1 - fract) * colorValues[minIndex++]));
    auto g = static_cast<unsigned char>((fract * colorValues[maxIndex++]
                                 + (1 - fract) * colorValues[minIndex++]));
    auto b = static_cast<unsigned char>((fract * colorValues[maxIndex++]
                                 + (1 - fract) * colorValues[minIndex++]));
    auto a = static_cast<unsigned char>((fract * colorValues[maxIndex]
                                 + (1 - fract) * colorValues[minIndex]));

    return QColor(r, g, b, a);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MTransferFunction1D::generateTransferTexture()
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    // The number of steps into which the range minValue..maxValue of this
    // colourbar is divided. A 1D texture of width "numSteps" is generated.
    // It can be used as lookup table for actors using the colourbar.
    int numSteps = properties->mInt()->value(numStepsProperty);

    // Generate an RGBA * numSteps float array to accomodate the texture.
    colorValues.resize(4 * numSteps);

    // Which type of colourmap are we using?
    MColourmapType cmaptype = MColourmapType(
                properties->mEnum()->value(colourmapTypeProperty));

    MColourmap *cmap = nullptr;
    bool deleteColourMap = false;
    bool reverse = properties->mBool()->value(reverseTFRangeProperty);

    int lightnessAdjust = 0;
    int saturationAdjust = 0;

    switch (cmaptype)
    {
    case PREDEFINED:
    {
        // Get the selected colourmap and info on whether it shall be reversed.
        int colourmapIndex = properties->mEnum()->value(predefColourmapProperty);
        QString colourmapName = properties->mEnum()->enumNames(
                    predefColourmapProperty)[colourmapIndex];
        cmap = colourmapPool.colourmap(colourmapName);
        lightnessAdjust = properties->mInt()->value(predefLightnessAdjustProperty);
        saturationAdjust = properties->mInt()->value(predefSaturationAdjustProperty);

        // Fill the texture with the chosen colourmap.
        int n = 0;
        int hslH, hslS, hslL, alpha;

        for (int i = 0; i < numSteps; i++)
        {
            float  scalar = float(i) / float(numSteps-1);
            QColor rgba   = cmap->scalarToColour(reverse ? 1.-scalar : scalar);

            rgba.getHsl(&hslH, &hslS, &hslL, &alpha);
            rgba.setHsl(hslH,
                        max(min(hslS + saturationAdjust, 255), 0),
                        max(min(hslL + lightnessAdjust, 255), 0),
                        alpha);

            colorValues[n++] = (unsigned char)(rgba.redF() * 255);
            colorValues[n++] = (unsigned char)(rgba.greenF() * 255);
            colorValues[n++] = (unsigned char)(rgba.blueF() * 255);
            colorValues[n++] = (unsigned char)(rgba.alphaF() * 255);
        }

        break;
    }
    case HCL:
    {
        float hue1 = properties->mDouble()->value(hclHue1Property);
        float hue2 = properties->mDouble()->value(hclHue2Property);
        float chroma1 = properties->mDouble()->value(hclChroma1Property);
        float chroma2 = properties->mDouble()->value(hclChroma2Property);
        float luminance1 = properties->mDouble()->value(hclLuminance1Property);
        float luminance2 = properties->mDouble()->value(hclLuminance2Property);
        float power1 = properties->mDouble()->value(hclPower1Property);
        float power2 = properties->mDouble()->value(hclPower2Property);
        float alpha1 = properties->mDouble()->value(hclAlpha1Property);
        float alpha2 = properties->mDouble()->value(hclAlpha2Property);
        float poweralpha = properties->mDouble()->value(hclPowerAlphaProperty);
        deleteColourMap = true;

        // Get type of HCL map and instantiate corresponding class.
        // The types are the same as on http://hclwizard.org.
        MHCLType hcltype = MHCLType(properties->mEnum()->value(hclTypeProperty));
        switch (hcltype)
        {
        case DIVERGING:
            cmap = new MHCLColourmap(hue1, hue2, chroma1, chroma1,
                                     luminance1, luminance2, power1, power1,
                                     alpha1, alpha2, poweralpha,
                                     true); // enable divergence equations
            break;
        case QUALITATIVE:
            cmap = new MHCLColourmap(hue1, hue2, chroma1, chroma1,
                                     luminance1, luminance1, 1., 1.,
                                     alpha1, alpha2, poweralpha);
            break;
        case SEQENTIAL_SINGLE_HUE:
            cmap = new MHCLColourmap(hue1, hue1, chroma1, chroma2,
                                     luminance1, luminance2, power1, power1,
                                     alpha1, alpha2, poweralpha);
            break;
        case SEQUENTIAL_MULTIPLE_HUE:
            cmap = new MHCLColourmap(hue1, hue2, chroma1, chroma2,
                                     luminance1, luminance2, power1, power2,
                                     alpha1, alpha2, poweralpha);
            break;
        }

        // Fill the texture with the chosen colourmap.
        int n = 0;
        for (int i = 0; i < numSteps; i++)
        {
            float  scalar = float(i) / float(numSteps-1);
            QColor rgba   = cmap->scalarToColour(reverse ? 1.-scalar : scalar);
            colorValues[n++] = (unsigned char)(rgba.redF() * 255);
            colorValues[n++] = (unsigned char)(rgba.greenF() * 255);
            colorValues[n++] = (unsigned char)(rgba.blueF() * 255);
            colorValues[n++] = (unsigned char)(rgba.alphaF() * 255);
        }

        break;
    }
    case HSV:
    {
        // Instantiate HSV colourmap class with filename pointing to an
        // XML file containing a Vapor transfer function.
        try
        {
            cmap = new MHSVColourmap(hsvVaporXMLFilename);
        }
        catch (MInitialisationError)
        {
            break;
        }

        // Fill the texture with the chosen colourmap.
        int n = 0;
        for (int i = 0; i < numSteps; i++)
        {
            float  scalar = float(i) / float(numSteps-1);
            QColor rgba   = cmap->scalarToColour(reverse ? 1.-scalar : scalar);
            colorValues[n++] = (unsigned char)(rgba.redF() * 255);
            colorValues[n++] = (unsigned char)(rgba.greenF() * 255);
            colorValues[n++] = (unsigned char)(rgba.blueF() * 255);
            colorValues[n++] = (unsigned char)(rgba.alphaF() * 255);
        }

        break;
    }
    case EDITOR:
    {
        editor->updateNumSteps(numSteps);
        const auto& tex = *editor->getTransferFunction()->getSampledBuffer();

        for (int i = 0; i < numSteps; i++)
        {
            colorValues[i * 4 + 0] = (unsigned char)qRed(tex[i]);
            colorValues[i * 4 + 1] = (unsigned char)qGreen(tex[i]);
            colorValues[i * 4 + 2] = (unsigned char)qBlue(tex[i]);
            colorValues[i * 4 + 3] = (unsigned char)qAlpha(tex[i]);
        }
        break;
    }
    case INVALID:
            break;
    }

    if (deleteColourMap && (cmap != nullptr)) delete cmap;

    // Upload the texture to GPU memory:
    if (tfTexture == nullptr)
    {
        // No texture exists. Create a new one and register with memory manager.
        QString textureID = QString("transferFunction_#%1").arg(getID());
        tfTexture = new GL::MTexture(textureID, GL_TEXTURE_1D, GL_RGBA8UI,
                                     numSteps);

        if ( !glRM->tryStoreGPUItem(tfTexture) )
        {
            delete tfTexture;
            tfTexture = nullptr;
        }
    }

    if ( tfTexture )
    {
        tfTexture->updateSize(numSteps);

        glRM->makeCurrent();
        tfTexture->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering.
        // NOTE: GL_NEAREST is required here to avoid interpolation between
        // discrete colour levels -- the colour bar should reflect the actual
        // number of colour levels in the texture.
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // Upload data array to GPU.
        glTexImage1D(GL_TEXTURE_1D,             // target
                     0,                         // level of detail
                     GL_RGBA,                   // internal format
//TODO (mr, 01Feb2015) -- why does GL_RGBA8UI not work?
//                     GL_RGBA8UI,                // internal format
                     numSteps,                  // width
                     0,                         // border
                     GL_RGBA,                   // format
                     GL_UNSIGNED_BYTE,          // data type of the pixel data
                     colorValues.data()); CHECK_GL_ERROR;

#ifdef USE_QOPENGLWIDGET
        glActiveTexture(GL_TEXTURE0);
        glRM->doneCurrent();
#endif
    }
}


void MTransferFunction1D::onQtPropertyChanged(QtProperty *property)
{
    if ( (property == minimumValueProperty)    ||
         (property == maximumValueProperty)    ||
         (property == numStepsProperty)        ||
         (property == maxNumTicksProperty)     ||
         (property == maxNumLabelsProperty)    ||
         (property == positionProperty)        ||
         (property == tickWidthProperty)       ||
         (property == labelSpacingProperty)    ||
         (property == labelSizeProperty)       ||
         (property == labelColourProperty)     ||
         (property == labelBBoxProperty)       ||
         (property == labelBBoxColourProperty) ||
         (property == scaleFactorProperty)     ||
         (property == predefColourmapProperty) ||
         (property == reverseTFRangeProperty)  ||
         (property == predefLightnessAdjustProperty)  ||
         (property == predefSaturationAdjustProperty) ||
         (property == hclHue1Property)         ||
         (property == hclHue2Property)         ||
         (property == hclChroma1Property)      ||
         (property == hclChroma2Property)      ||
         (property == hclLuminance1Property)   ||
         (property == hclLuminance2Property)   ||
         (property == hclPower1Property)       ||
         (property == hclPower2Property)       ||
         (property == hclAlpha1Property)       ||
         (property == hclAlpha2Property)       ||
         (property == hclPowerAlphaProperty)      )
    {
        if (suppressActorUpdates()) return;

        generateTransferTexture();
        generateBarGeometry();
        emit transferFunctionChanged(this);
        emitActorChangedSignal();
    }

    else if (property == colourmapTypeProperty)
    {
        MColourmapType cmaptype = MColourmapType(
                    properties->mEnum()->value(colourmapTypeProperty));

        switch (cmaptype)
        {
        case PREDEFINED:
            predefCMapPropertiesSubGroup->setEnabled(true);
            hclCMapPropertiesSubGroup->setEnabled(false);
            hsvCMapPropertiesSubGroup->setEnabled(false);
            editorPropertiesSubGroup->setEnabled(false);
            break;
        case HCL:
            predefCMapPropertiesSubGroup->setEnabled(false);
            hclCMapPropertiesSubGroup->setEnabled(true);
            hsvCMapPropertiesSubGroup->setEnabled(false);
            editorPropertiesSubGroup->setEnabled(false);
            break;
        case HSV:
            predefCMapPropertiesSubGroup->setEnabled(false);
            hclCMapPropertiesSubGroup->setEnabled(false);
            hsvCMapPropertiesSubGroup->setEnabled(true);
            editorPropertiesSubGroup->setEnabled(false);

            if (hsvVaporXMLFilename.isEmpty())
            {
                hsvVaporXMLFilename = QFileDialog::getOpenFileName(
                            MGLResourcesManager::getInstance(),
                            "Load Vapor transfer function",
                            "/",
                            "Vapor transfer function XML (*.vtf)");
                updateHSVProperties();
            }
            break;
        case EDITOR:
            predefCMapPropertiesSubGroup->setEnabled(false);
            hclCMapPropertiesSubGroup->setEnabled(false);
            hsvCMapPropertiesSubGroup->setEnabled(false);
            editorPropertiesSubGroup->setEnabled(true);
            break;
        case INVALID:
                break;
        }

        if (suppressActorUpdates()) return;

        generateTransferTexture();
        generateBarGeometry();
        emit transferFunctionChanged(this);
        emitActorChangedSignal();
    }

    else if (property == hclTypeProperty)
    {
        updateHCLProperties();

        if (suppressActorUpdates()) return;

        generateTransferTexture();
        generateBarGeometry();
        emit transferFunctionChanged(this);
        emitActorChangedSignal();
    }

    else if (property == valueSignificantDigitsProperty)
    {
        int significantDigits =
                properties->mInt()->value(valueSignificantDigitsProperty);
        properties->mSciDouble()->setSignificantDigits(minimumValueProperty,
                                                     significantDigits);
        properties->mSciDouble()->setSignificantDigits(maximumValueProperty,
                                                     significantDigits);
        properties->mSciDouble()->setSignificantDigits(valueStepProperty,
                                                     significantDigits);

        if (suppressActorUpdates()) return;

        // Texture remains unchanged; only geometry needs to be updated.
        generateBarGeometry();
        emitActorChangedSignal();
    }

    else if (property == valueStepProperty)
    {
        double step = properties->mSciDouble()->value(valueStepProperty);
        properties->mSciDouble()->setSingleStep(minimumValueProperty, step);
        properties->mSciDouble()->setSingleStep(maximumValueProperty, step);
    }

    else if (property == hsvLoadFromVaporXMLProperty)
    {
        QString filename = QFileDialog::getOpenFileName(
                    MGLResourcesManager::getInstance(),
                    "Load Vapor transfer function",
                    "/",
                    "Vapor transfer function XML (*.vtf)");

        if (filename.isEmpty()) return;

        hsvVaporXMLFilename = filename;
        LOG4CPLUS_DEBUG(mlog, "Loading Vapor transfer function from "
                        << hsvVaporXMLFilename.toStdString());
        updateHSVProperties();

        if (suppressActorUpdates()) return;

        generateTransferTexture();
        generateBarGeometry();
        emit transferFunctionChanged(this);
        emitActorChangedSignal();
    }

    else if (property == enableAlphaInTFProperty)
    {
        enableAlpha = properties->mBool()->value(enableAlphaInTFProperty);

        emitActorChangedSignal();
    }
    else if (property == editorClickProperty)
    {
        editor->show();
    }
}


void MTransferFunction1D::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    Q_UNUSED(sceneView);

    tfTexture->bindToTextureUnit(textureUnit);

    // First draw the colourbar itself. glPolygonOffset is used to displace
    // the colourbar's z-value slightly to the back, to that the frame drawn
    // afterwards is rendered correctly.
    colourbarShader->bindProgram("colourbarTF");
    colourbarShader->setUniformValue("transferTexture", textureUnit);
    colourbarShader->setUniformValue("enableAlpha", GLboolean(enableAlpha));

    vertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE, 3, false,
                                          4 * sizeof(float), 
                                          (const GLvoid*)(0 * sizeof(float)));
    vertexBuffer->attachToVertexAttribute(SHADER_TEXTURE_ATTRIBUTE, 1, false,
                                          4 * sizeof(float),
                                          (const GLvoid*)(3 * sizeof(float)));

    glPolygonOffset(.01f, 1.0f);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); CHECK_GL_ERROR;
    glDrawArrays(GL_TRIANGLE_STRIP, 0, numVertices); CHECK_GL_ERROR;
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Next draw a black frame around the colourbar.
    simpleGeometryShader->bindProgram("Simple"); CHECK_GL_ERROR;
    simpleGeometryShader->setUniformValue("colour", QColor(0, 0, 0)); CHECK_GL_ERROR;
    vertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE, 3, false,
                                          4 * sizeof(float),
                                          (const GLvoid*)(8 * sizeof(float)));

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
    glLineWidth(1);
    glDrawArrays(GL_LINE_LOOP, 0, numVertices); CHECK_GL_ERROR;

    // Finally draw the tick marks.
    vertexBuffer->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE, 3, false,
                                          0,
                                          (const GLvoid*)(24 * sizeof(float)));

    glDrawArrays(GL_LINES, 0, 2 * numTicks); CHECK_GL_ERROR;

    // Unbind VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
}


void MTransferFunction1D::generateBarGeometry()
{
    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    // Create geometry for a box filled with the colourbar texture and for tick
    // marks, and place labels at the tick marks.

    // Get user-defined upper left corner x, y, z and width, height in clip
    // space.
    QRectF positionRect = properties->mRectF()->value(positionProperty);
    float ulcrnr[3] = { float(positionRect.x()), float(positionRect.y()), -1. };
    float width     = positionRect.width();
    float height    = positionRect.height();

    // This array accomodates the geometry for two filled triangles showing the
    // actual colourbar (GL_TRIANGLE_STRIP). The 3rd, 4th, and the additional
    // 5th and 6th vertices are used to draw a box around the colourbar
    // (GL_LINE_LOOP).
    float coordinates[24] =
    {
        ulcrnr[0]        , ulcrnr[1]         , ulcrnr[2], 1, // ul
        ulcrnr[0]        , ulcrnr[1] - height, ulcrnr[2], 0, // ll
        ulcrnr[0] + width, ulcrnr[1]         , ulcrnr[2], 1, // ur
        ulcrnr[0] + width, ulcrnr[1] - height, ulcrnr[2], 0, // lr
        ulcrnr[0]        , ulcrnr[1] - height, ulcrnr[2], 0, // ll for frame
        ulcrnr[0]        , ulcrnr[1]         , ulcrnr[2], 1  // ul for frame
    };

    // ========================================================================
    // Next, generate the tickmarks. maxNumTicks tickmarks are drawn, but not
    // more than colour steps.
    int numSteps = properties->mInt()->value(numStepsProperty);
    int maxNumTicks = properties->mInt()->value(maxNumTicksProperty);
    numTicks = min(numSteps + 1, maxNumTicks);

    // This array accomodates the tickmark geometry.
	// float tickmarks[6 * numTicks]; VLAs not supported in msvc
    float* tickmarks = new float[6 * numTicks];

    // Width of the tickmarks in clip space.
    float tickwidth = properties->mDouble()->value(tickWidthProperty);

    int n = 0;
    // Treat numTicks equals 1 as a special case to avoid divison by zero.
    if (numTicks != 1)
    {
        for (uint i = 0; i < numTicks; i++)
        {
            tickmarks[n++] = ulcrnr[0];
            tickmarks[n++] = ulcrnr[1] - i * (height / (numTicks - 1));
            tickmarks[n++] = ulcrnr[2];
            tickmarks[n++] = ulcrnr[0] - tickwidth;
            tickmarks[n++] = ulcrnr[1] - i * (height / (numTicks - 1));
            tickmarks[n++] = ulcrnr[2];
        }
    }
    else
    {
        tickmarks[n++] = ulcrnr[0];
        tickmarks[n++] = ulcrnr[1];
        tickmarks[n++] = ulcrnr[2];
        tickmarks[n++] = ulcrnr[0] - tickwidth;
        tickmarks[n++] = ulcrnr[1];
        tickmarks[n++] = ulcrnr[2];
    }

    // ========================================================================
    // Now we can upload the two geometry arrays to the GPU. Switch to the
    // shared background context so all views can access the VBO generated
    // here.
    glRM->makeCurrent();

    QString requestKey = QString("vbo_transfer_function_actor_%1").arg(getID());

    GL::MVertexBuffer* vb =
            static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(requestKey));


    if (vb)
    {
        vertexBuffer = vb;
        GL::MFloatVertexBuffer* buf = dynamic_cast<GL::MFloatVertexBuffer*>(vb);
        // reallocate buffer if size has changed
        buf->reallocate(nullptr, 24 + numTicks * 6);
        buf->update(coordinates, 0, 0, sizeof(coordinates));
        buf->update(tickmarks, 0, sizeof(coordinates),
                    sizeof(float) * 6 * numTicks);

    }
    else
    {
        GL::MFloatVertexBuffer* newVB = nullptr;
        newVB = new GL::MFloatVertexBuffer(requestKey, 24 + numTicks * 6);
        if (glRM->tryStoreGPUItem(newVB))
        {
            newVB->reallocate(nullptr, 24 + numTicks * 6, 0, true);
            newVB->update(coordinates, 0, 0, sizeof(coordinates));
            newVB->update(tickmarks, 0, sizeof(coordinates),
                          sizeof(float) * 6 * numTicks);

        }
        else
        {
            delete newVB;
        }
        vertexBuffer = static_cast<GL::MVertexBuffer*>(glRM->getGPUItem(requestKey));
    }

#ifdef USE_QOPENGLWIDGET
    glRM->doneCurrent();
#endif

    // Required for the glDrawArrays() call in renderToCurrentContext().
    numVertices = 4;

    // ========================================================================
    // Finally, place labels at the tickmarks:

    minimumValue = properties->mSciDouble()->value(minimumValueProperty);
    maximumValue = properties->mSciDouble()->value(maximumValueProperty);
    int maxNumLabels = properties->mInt()->value(maxNumLabelsProperty);

    // Obtain a shortcut to the application's text manager to register the
    // labels generated in the loops below.
    MTextManager* tm = glRM->getTextManager();

    // Remove all text labels of the old geometry.
    while (!labels.isEmpty())
    {
        tm->removeText(labels.takeLast());
    }

    // Draw no labels if either numTicks or maxNumLabels equal 0.
    if (numTicks == 0 || maxNumLabels == 0)
    {
        return;
    }

    // A maximum of maxNumLabels are placed. The approach taken here is to
    // compute a "tick step size" from the number of ticks drawn and the
    // maximum number of labels to be drawn. A label will then be placed
    // every tickStep-th tick. The formula tries to place a label at the
    // lower and upper end of the colourbar, if possible.
    int tickStep = ceil(double(numTicks - 1) / double(maxNumLabels - 1));

    // The (clip-space) distance between the ends of the tick marks and the
    // labels.
    float labelSpacing = properties->mDouble()->value(labelSpacingProperty);

    // Label font size and colour.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);

    // Label bounding box.
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);

    // Scale factor for labels.
    float scaleFactor = properties->mDouble()->value(scaleFactorProperty);

    // Register the labels with the text manager.
    // Treat numTicks equals 1 as a special case to avoid divison by zero.
    if (numTicks != 1)
    {
        for (uint i = 0; i < numTicks; i += tickStep)
        {
            float value = (maximumValue - double(i) / double(numTicks-1)
                           * (maximumValue - minimumValue)) * scaleFactor;
            QString labelText =
                    properties->mSciDouble()->valueAsPropertyFormatedText(
                        minimumValueProperty, value);
            labels.append(tm->addText(
                              labelText,
                              MTextManager::CLIPSPACE,
                              tickmarks[6*i + 3] - labelSpacing,
                              tickmarks[6*i + 4],
                              tickmarks[6*i + 5],
                              labelsize,
                              labelColour, MTextManager::MIDDLERIGHT,
                              labelbbox, labelBBoxColour)
                          );
        }
    }
    else
    {
        float value = maximumValue * scaleFactor;
        QString labelText =
                properties->mSciDouble()->valueAsPropertyFormatedText(
                    maximumValueProperty, value);
        labels.append(tm->addText(
                          labelText,
                          MTextManager::CLIPSPACE,
                          tickmarks[3] - labelSpacing,
                          tickmarks[4],
                          tickmarks[5],
                          labelsize,
                          labelColour, MTextManager::MIDDLERIGHT,
                          labelbbox, labelBBoxColour)
                      );
    }

    int significantDigits =
            properties->mInt()->value(valueSignificantDigitsProperty);
    int minimumExponent =
            properties->mSciDouble()->minimumExponent(minimumValueProperty);
    int switchNotationExponent =
            properties->mSciDouble()->switchNotationExponent(minimumValueProperty);

    editor->setRange(minimumValue, maximumValue, scaleFactor,
                     maxNumTicks, maxNumLabels, numSteps, significantDigits,
                     minimumExponent, switchNotationExponent);
	
	delete[] tickmarks;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


void MTransferFunction1D::updateHCLProperties()
{
    MHCLType hcltype = MHCLType(properties->mEnum()->value(hclTypeProperty));

    switch (hcltype)
    {
    case DIVERGING:
        hclHue1Property->setEnabled(true);
        hclHue2Property->setEnabled(true);
        hclChroma1Property->setEnabled(true);
        hclChroma2Property->setEnabled(false);
        hclLuminance1Property->setEnabled(true);
        hclLuminance2Property->setEnabled(true);
        hclPower1Property->setEnabled(true);
        hclPower2Property->setEnabled(false);
        hclAlpha1Property->setEnabled(true);
        hclAlpha2Property->setEnabled(true);
        hclPowerAlphaProperty->setEnabled(true);
        break;
    case QUALITATIVE:
        hclHue1Property->setEnabled(true);
        hclHue2Property->setEnabled(true);
        hclChroma1Property->setEnabled(true);
        hclChroma2Property->setEnabled(false);
        hclLuminance1Property->setEnabled(true);
        hclLuminance2Property->setEnabled(false);
        hclPower1Property->setEnabled(false);
        hclPower2Property->setEnabled(false);
        hclAlpha1Property->setEnabled(true);
        hclAlpha2Property->setEnabled(true);
        hclPowerAlphaProperty->setEnabled(true);
        break;
    case SEQENTIAL_SINGLE_HUE:
        hclHue1Property->setEnabled(true);
        hclHue2Property->setEnabled(false);
        hclChroma1Property->setEnabled(true);
        hclChroma2Property->setEnabled(true);
        hclLuminance1Property->setEnabled(true);
        hclLuminance2Property->setEnabled(true);
        hclPower1Property->setEnabled(true);
        hclPower2Property->setEnabled(false);
        hclAlpha1Property->setEnabled(true);
        hclAlpha2Property->setEnabled(true);
        hclPowerAlphaProperty->setEnabled(true);
        break;
    case SEQUENTIAL_MULTIPLE_HUE:
        hclHue1Property->setEnabled(true);
        hclHue2Property->setEnabled(true);
        hclChroma1Property->setEnabled(true);
        hclChroma2Property->setEnabled(true);
        hclLuminance1Property->setEnabled(true);
        hclLuminance2Property->setEnabled(true);
        hclPower1Property->setEnabled(true);
        hclPower2Property->setEnabled(true);
        hclAlpha1Property->setEnabled(true);
        hclAlpha2Property->setEnabled(true);
        hclPowerAlphaProperty->setEnabled(true);
        break;
    }
}


void MTransferFunction1D::updateHSVProperties()
{
    properties->mString()->setValue(
                hsvVaporXMLFilenameProperty, hsvVaporXMLFilename);
}


void MTransferFunction1D::onEditorTransferFunctionChanged()
{
    generateTransferTexture();
    emit transferFunctionChanged(this);
    emitActorChangedSignal();
}


QString MTransferFunction1D::colourMapTypeToString(MColourmapType colourMapType)
{
    switch (colourMapType)
    {
    case MColourmapType::PREDEFINED:
        return QString("predefined");
    case MColourmapType::HCL:
        return QString("hcl");
    case MColourmapType::HSV:
        return QString("hsv");
    case MColourmapType::EDITOR:
        return QString("editor");
    default:
        return QString("");
    }
}


MTransferFunction1D::MColourmapType
MTransferFunction1D::stringToColourMapType(QString colourMapTypeName)
{
    // NOTE: Colour map type identification was changed in Met.3D version 1.1.
    // For compatibility with version 1.0, the old numeric identifiers are
    // considered here as well.
    if (colourMapTypeName == QString("predefined")
            || colourMapTypeName == QString("0")) // compatibility with Met.3D 1.0
    {
        return MColourmapType::PREDEFINED;
    }
    else if (colourMapTypeName == QString("hcl")
             || colourMapTypeName == QString("1"))
    {
        return MColourmapType::HCL;
    }
    else if (colourMapTypeName == QString("hsv")
             || colourMapTypeName == QString("2"))
    {
        return MColourmapType::HSV;
    }
    else if (colourMapTypeName == QString("editor")
             || colourMapTypeName == QString("3"))
    {
        return MColourmapType::EDITOR;
    }
    else
    {
        return MColourmapType::INVALID;
    }
}

} // namespace Met3D
