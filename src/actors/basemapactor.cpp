/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015      Michael Kern
**  Copyright 2017      Bianca Tost
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
#include "basemapactor.h"

// standard library imports
#include <iostream>
#include <cstdio>

// related third party imports
#include <log4cplus/loggingmacros.h>
#include <QString>
#include <QFileDialog>
#include <QList>

// local application imports
#include "util/mstopwatch.h"
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"


using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MBaseMapActor::MBaseMapActor()
    : MMapProjectionSupportingActor(QList<MapProjectionType>()
                                    // MBaseMapActor supports cylindrical and
                                    // rotated map projections.
                                    << MAPPROJECTION_CYLINDRICAL
                                    << MAPPROJECTION_ROTATEDLATLON),
      MBoundingBoxInterface(this, MBoundingBoxConnectionType::HORIZONTAL),
      texture(nullptr),
      numVertices(4),
      colourSaturation(0.3)
{
    GDALAllRegister();

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setActorType(staticActorType());
    setName(getActorType());

    loadMapProperty = addProperty(CLICK_PROPERTY, "load map",
                                  actorPropertiesSupGroup);

    filenameProperty = addProperty(STRING_PROPERTY, "map file",
                                   actorPropertiesSupGroup);
    properties->mString()->setValue(filenameProperty, "");
    filenameProperty->setEnabled(false);

    // Bounding box of the actor.
    insertBoundingBoxProperty(actorPropertiesSupGroup);

    colourSaturationProperty = addProperty(DECORATEDDOUBLE_PROPERTY,
                                           "colour saturation",
                                           actorPropertiesSupGroup);
    properties->setDDouble(colourSaturationProperty, colourSaturation,
                           0., 1., 2., 0.1, " (0..1)");

    actorPropertiesSupGroup->addSubProperty(mapProjectionPropertiesSubGroup);

    endInitialiseQtProperties();
}


MBaseMapActor::~MBaseMapActor()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE 0
#define SHADER_TEXTURE_ATTRIBUTE 1

void MBaseMapActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs\n" << flush);
    shaderProgram->compileFromFile_Met3DHome("src/glsl/basemap.fx.glsl");
}


void MBaseMapActor::saveConfiguration(QSettings *settings)
{
    MMapProjectionSupportingActor::saveConfiguration(settings);

    settings->beginGroup(MBaseMapActor::getSettingsID());

    MBoundingBoxInterface::saveConfiguration(settings);
    settings->setValue("colourSaturation", float(colourSaturation));

    QString filename = properties->mString()->value(filenameProperty);
    settings->setValue("filename",  filename);

    settings->endGroup();
}


void MBaseMapActor::loadConfiguration(QSettings *settings)
{
    MMapProjectionSupportingActor::loadConfiguration(settings);

    settings->beginGroup(MBaseMapActor::getSettingsID());

    MBoundingBoxInterface::loadConfiguration(settings);

    colourSaturation = settings->value("colourSaturation").toFloat();
    properties->mDDouble()->setValue(colourSaturationProperty, colourSaturation);

    const QString filename = settings->value("filename").toString();
    properties->mString()->setValue(filenameProperty, filename);

    settings->endGroup();

    if (isInitialized()) loadMap(filename.toStdString());
}


void MBaseMapActor::setFilename(QString filename)
{
    properties->mString()->setValue(filenameProperty, filename);
}


void MBaseMapActor::onBoundingBoxChanged()
{
    labels.clear();

    // Only adapt bounding box for rotated grids if base map is connected to
    // a bounding box.
    if (bBoxConnection->getBoundingBox() != nullptr)
    {
        adaptBBoxForRotatedGrids();
    }

    if (suppressActorUpdates())
    {
        return;
    }

    emitActorChangedSignal();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MBaseMapActor::initializeActorResources()
{
    // Bind the texture object to this unit.
    textureUnit = assignTextureUnit();

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    QString filename = properties->mString()->value(filenameProperty);
    loadMap(filename.toStdString());

    // Load shader program if the returned program is new.
    bool loadShaders = false;

    loadShaders |= glRM->generateEffectProgram("mapactor_shader", shaderProgram);

    if (loadShaders) reloadShaderEffects();
}


void MBaseMapActor::onQtPropertyChanged(QtProperty* property)
{
    if (property == loadMapProperty)
    {
        // open file dialog to select geotiff file
        QString filename = QFileDialog::getOpenFileName(
                    nullptr, tr("Open GeoTiff Map"),
                    "/home/",
                    tr("Tiff Image Files (*.tif *.geotiff)"));

        properties->mString()->setValue(filenameProperty, filename);
    }

    else if (property == filenameProperty)
    {
        QString filename = properties->mString()->value(filenameProperty);
        if (suppressActorUpdates()) return;
        if (filename.isEmpty()) return;

        loadMap(filename.toStdString());
        emitActorChangedSignal();
    }

    else if (property == colourSaturationProperty)
    {
        colourSaturation =
                properties->mDDouble()->value(colourSaturationProperty);
        emitActorChangedSignal();
    }
    else if (property == mapProjectionTypesProperty)
    {
        updateMapProjectionProperties();
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }
    else if (property == rotateBBoxProperty)
    {
        rotateBBox = properties->mBool()->value(rotateBBoxProperty);
        if (suppressActorUpdates()) return;
        emitActorChangedSignal();
    }

    else if (property == rotatedNorthPoleProperty)
    {
        rotatedNorthPole =
                properties->mPointF()->value(rotatedNorthPoleProperty);
        if (suppressActorUpdates()) return;
        if (mapProjection == MAPPROJECTION_ROTATEDLATLON)
        {
            emitActorChangedSignal();
        }
    }
}


void MBaseMapActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    if (texture != nullptr && bBoxConnection->getBoundingBox() != nullptr)
    {
        QVector4D bboxVec4(bBoxConnection->westLon(), bBoxConnection->southLat(),
                           bBoxConnection->eastLon(), bBoxConnection->northLat());
        // Bind shader program.
        if (mapProjection == MAPPROJECTION_ROTATEDLATLON) // Rotated
        {
            // Bounding box is given in coordinates of the real geographical
            // system.
            if (rotateBBox)
            {
                shaderProgram->bindProgram("BasemapRotation");
                QVector4D rotatedBboxVec4 = getBBoxOfRotatedBBox();
                shaderProgram->setUniformValue("cornersRotatedBox",
                                               rotatedBboxVec4);
                shaderProgram->setUniformValue("cornersBox",
                                               bboxForRotatedGrids);
            }
            // Bounding box is given in rotated coordinates.
            else
            {
                shaderProgram->bindProgram("BasemapRotationRotatedBBox");
                shaderProgram->setUniformValue("cornersRotatedBox",
                                               bboxVec4);
            }
            shaderProgram->setUniformValue(
                        "poleLat", GLfloat(rotatedNorthPole.y()));
            shaderProgram->setUniformValue(
                        "poleLon", GLfloat(rotatedNorthPole.x()));
        }
        else // Cylindrical
        {
            shaderProgram->bindProgram("Basemap");
            shaderProgram->setUniformValue("cornersBox", bboxVec4);
        }

        shaderProgram->setUniformValue(
                    "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

        // Bind texture and use correct texture unit in shader.
        texture->bindToTextureUnit(textureUnit);
        shaderProgram->setUniformValue("mapTexture", textureUnit);

        QVector4D dataVec4(llcrnrlon, llcrnrlat, urcrnrlon, urcrnrlat);

        shaderProgram->setUniformValue("cornersData", dataVec4);

        shaderProgram->setUniformValue("colourIntensity", colourSaturation);

        // Draw map.
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); CHECK_GL_ERROR;

        // Unbind VBO.
        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    }

}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MBaseMapActor::loadMap(std::string filename)
{
#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
#endif

    if (filename.empty())
    {
        LOG4CPLUS_ERROR(mlog, "Given GeoTiff filename is empty. "
                        "Cannot read file.");
        return;
    }

    LOG4CPLUS_DEBUG(mlog, "Reading world map image from GeoTiff file <" <<
                           filename << ">..." << flush);

    // open the raster dataset and store it in a GDALDataset object
    GDALDataset* tiffData = static_cast<GDALDataset*>(
                GDALOpen(filename.c_str(), GA_ReadOnly));

    // GetDescription() <- filename
    // GetRaster Size() <- dimensions lot/lan
    // GetRasterCount() <- number of raster bands (3 for rgb)
    // GetDriver()->GetDescription() <- data type

    if (tiffData == nullptr)
    {
        LOG4CPLUS_ERROR(mlog, "Cannot open GeoTIFF file <"
                        << filename << ">.");
        return;
    }

    std::string datasetType = tiffData->GetDriver()->GetDescription();

    if (datasetType != "GTiff")
    {
        LOG4CPLUS_ERROR(mlog, "Raster dataset <"
                        << filename << "> is not of type GeoTiff.");
        return;
    }

    // get the geo-spatial translation of the current raster dataset
    QVector<double> geoData(6);
    // 0 - top left x
    // 1 - w-e pixel resolution
    // 2 - 0
    // 3 - top left y
    // 4 - 0
    // 5 - n-s pixel resolution (negative)

    tiffData->GetGeoTransform(&geoData[0]);

    // calculate lat/lon corners of loaded map and store them in class
    const int32_t longitudeDim = tiffData->GetRasterXSize();
    const int32_t latitudeDim = tiffData->GetRasterYSize();
    const int32_t colorDim = tiffData->GetRasterCount();

    float scaleFactor =  1.0f;
    if (mapProjection == MAPPROJECTION_PROJ_LIBRARY)
    {
        scaleFactor =  1.0e+6f;
    }

    llcrnrlat = (geoData[3] + geoData[5] * latitudeDim) / scaleFactor;
    llcrnrlon = geoData[0] / scaleFactor;
    urcrnrlat = geoData[3] / scaleFactor;
    urcrnrlon = (geoData[0] + geoData[1] * longitudeDim) / scaleFactor;

    LOG4CPLUS_DEBUG(mlog, "\tLongitude range: " << llcrnrlon << " - "
                    << urcrnrlon << flush);
    LOG4CPLUS_DEBUG(mlog, "\tLatitude range: " << llcrnrlat << " - "
                    << urcrnrlat << flush);

    // read the color data by using bands
    // we need three bands for every color channel
    GDALRasterBand* bandRed;
    GDALRasterBand* bandGreen;
    GDALRasterBand* bandBlue;

    bandRed = tiffData->GetRasterBand(1);
    bandGreen = tiffData->GetRasterBand(2);
    bandBlue = tiffData->GetRasterBand(3);

    // GDALGetDataTypeName(band->GetRasterDataType()) <- datatype
    // DALGetColorInterpretationName(band->GetColorInterpretation()) <- color channel

    std::string dataType = GDALGetDataTypeName(bandRed->GetRasterDataType());

    if (dataType != "Byte")
    {
        LOG4CPLUS_ERROR(mlog, "Raster dataset has no data of type Byte.");
        return;
    }

    // image data
    const int32_t imgSizeX = longitudeDim * colorDim;
    const int32_t imgSize = imgSizeX * latitudeDim;

    std::vector<GLbyte> tiffImg;
	// changed reserve to resize because of "vector subscript out of range" 
	// error on windows
    tiffImg.resize(imgSize * sizeof(GLbyte));
    //GLbyte* tiffImg = new GLbyte[imgSize * sizeof(GLbyte)];

    std::string file = filename.substr(0, filename.find_last_of("."));
    std::string cacheFilename = file + ".ctif";

    // print out some information
    LOG4CPLUS_DEBUG(mlog, "\tMap texture size (lon/lat/col): " << longitudeDim
                    << "x" << latitudeDim << "x" << colorDim);

    // print out some information
    LOG4CPLUS_DEBUG(mlog, "\tParsing color data..." << flush);


    // if we can found a cache file
    FILE* cacheFile = fopen(cacheFilename.c_str(), "rb");
    if (cacheFile != nullptr)
    {
        LOG4CPLUS_DEBUG(mlog, "\tFound and using cache color data <"
                        << cacheFilename << ">...");

        // obtain binary data from the exisiting file
        fread(&tiffImg[0], sizeof(GLbyte), imgSize, cacheFile);
        fclose(cacheFile);
    }
    else
    {
        // fetch the raster data of all color components
        std::vector<GLbyte> rasterRed(longitudeDim * latitudeDim);
        std::vector<GLbyte> rasterGreen(longitudeDim * latitudeDim);
        std::vector<GLbyte> rasterBlue(longitudeDim * latitudeDim);
        // load whole raster data into buffers
        bandRed->RasterIO(GF_Read, 0, 0, longitudeDim, latitudeDim, &rasterRed[0], longitudeDim, latitudeDim, GDT_Byte, 0, 0);
        bandGreen->RasterIO(GF_Read, 0, 0, longitudeDim, latitudeDim, &rasterGreen[0], longitudeDim, latitudeDim, GDT_Byte, 0, 0);
        bandBlue->RasterIO(GF_Read, 0, 0, longitudeDim, latitudeDim, &rasterBlue[0], longitudeDim, latitudeDim, GDT_Byte, 0, 0);

        // copy the color components into the image buffer
        for (int i = 0; i < longitudeDim * latitudeDim; ++i)
        {
            const uint32_t tiffID = i * colorDim;

            tiffImg[tiffID + 0] = rasterRed[i];
            tiffImg[tiffID + 1] = rasterGreen[i];
            tiffImg[tiffID + 2] = rasterBlue[i];
        }

        LOG4CPLUS_DEBUG(mlog, "\tCaching color data into file <"
                        << cacheFilename << ">...");

        // write out a cache file to enhance the loading process
        cacheFile = fopen(cacheFilename.c_str(), "wb");
        fwrite(&tiffImg[0], sizeof(GLbyte), imgSize, cacheFile);
        fclose(cacheFile);
    }

#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "GeoTIFF read in "
                    << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif

    // at the end close the tiff file
    GDALClose(tiffData);

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    if (texture == nullptr)
    {
        QString textureID = QString("baseMap_#%1").arg(getID());
        texture = new GL::MTexture(textureID, GL_TEXTURE_2D, GL_RGB8,
                                   longitudeDim, latitudeDim);

        if (!glRM->tryStoreGPUItem(texture))
        {
            delete texture;
            texture = nullptr;
        }
    }

    if (texture)
    {
        texture->updateSize(longitudeDim, latitudeDim);

        glRM->makeCurrent();
        texture->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering (RTVG p. 64).
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        // DEBUG
        //std::cout << longitudeDim * latitudeDim * colorDim << std::endl;
        //GLint size;
        //glGetIntegerv(GL_MAX_TEXTURE_SIZE,&size);
        //std::cout << size << std::endl << flush;

        // Upload data array to GPU.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                     longitudeDim, latitudeDim,
                     0, GL_RGB, GL_UNSIGNED_BYTE, &tiffImg[0]); CHECK_GL_ERROR;
    }
}


void MBaseMapActor::LoadGDALDataset(GDALDataset* tiffData)
{

#ifdef ENABLE_MET3D_STOPWATCH
    MStopwatch stopwatch;
#endif
    // get the geo-spatial translation of the current raster dataset
    QVector<double> geoData(6);
    // 0 - top left x
    // 1 - w-e pixel resolution
    // 2 - 0
    // 3 - top left y
    // 4 - 0
    // 5 - n-s pixel resolution (negative)

    tiffData->GetGeoTransform(&geoData[0]);

    // calculate lat/lon corners of loaded map and store them in class
    const int32_t longitudeDim = tiffData->GetRasterXSize();
    const int32_t latitudeDim = tiffData->GetRasterYSize();
    const int32_t colorDim = tiffData->GetRasterCount();

    llcrnrlat = geoData[3] + geoData[5] * latitudeDim;
    llcrnrlon = geoData[0];
    urcrnrlat = geoData[3];
    urcrnrlon = geoData[0] + geoData[1] * longitudeDim;

    LOG4CPLUS_DEBUG(mlog, "\tLongitude range: " << llcrnrlon << " - "
                    << urcrnrlon << flush);
    LOG4CPLUS_DEBUG(mlog, "\tLatitude range: " << llcrnrlat << " - "
                    << urcrnrlat << flush);

    // read the color data by using bands
    // we need three bands for every color channel
    GDALRasterBand* bandRed;
    GDALRasterBand* bandGreen;
    GDALRasterBand* bandBlue;

    bandRed = tiffData->GetRasterBand(1);
    bandGreen = tiffData->GetRasterBand(2);
    bandBlue = tiffData->GetRasterBand(3);

    // GDALGetDataTypeName(band->GetRasterDataType()) <- datatype
    // DALGetColorInterpretationName(band->GetColorInterpretation()) <- color channel

    std::string dataType = GDALGetDataTypeName(bandRed->GetRasterDataType());

    if (dataType != "Byte")
    {
        LOG4CPLUS_ERROR(mlog, "Raster dataset has no data of type Byte.");
        return;
    }

    // image data
    const int32_t imgSizeX = longitudeDim * colorDim;
    const int32_t imgSize = imgSizeX * latitudeDim;

    std::vector<GLbyte> tiffImg;
    // changed reserve to resize because of "vector subscript out of range"
    // error on windows
    tiffImg.resize(imgSize * sizeof(GLbyte));
    //GLbyte* tiffImg = new GLbyte[imgSize * sizeof(GLbyte)];

//    std::string file = filename.substr(0, filename.find_last_of("."));
//    std::string cacheFilename = file + ".ctif";

    // print out some information
    LOG4CPLUS_DEBUG(mlog, "\tMap texture size (lon/lat/col): " << longitudeDim
                    << "x" << latitudeDim << "x" << colorDim);

    // print out some information
    LOG4CPLUS_DEBUG(mlog, "\tParsing color data..." << flush);


    // if we can found a cache file
//    FILE* cacheFile = fopen(cacheFilename.c_str(), "rb");
//    if (cacheFile != nullptr)
//    {
//        LOG4CPLUS_DEBUG(mlog, "\tFound and using cache color data <"
//                        << cacheFilename << ">...");

//        // obtain binary data from the exisiting file
//        fread(&tiffImg[0], sizeof(GLbyte), imgSize, cacheFile);
//        fclose(cacheFile);
//    }
//    else
//    {
        // fetch the raster data of all color components
        std::vector<GLbyte> rasterRed(longitudeDim * latitudeDim);
        std::vector<GLbyte> rasterGreen(longitudeDim * latitudeDim);
        std::vector<GLbyte> rasterBlue(longitudeDim * latitudeDim);
        // load whole raster data into buffers
        bandRed->RasterIO(GF_Read, 0, 0, longitudeDim, latitudeDim, &rasterRed[0], longitudeDim, latitudeDim, GDT_Byte, 0, 0);
        bandGreen->RasterIO(GF_Read, 0, 0, longitudeDim, latitudeDim, &rasterGreen[0], longitudeDim, latitudeDim, GDT_Byte, 0, 0);
        bandBlue->RasterIO(GF_Read, 0, 0, longitudeDim, latitudeDim, &rasterBlue[0], longitudeDim, latitudeDim, GDT_Byte, 0, 0);

        // copy the color components into the image buffer
        for (int i = 0; i < longitudeDim * latitudeDim; ++i)
        {
            const uint32_t tiffID = i * colorDim;

            tiffImg[tiffID + 0] = rasterRed[i];
            tiffImg[tiffID + 1] = rasterGreen[i];
            tiffImg[tiffID + 2] = rasterBlue[i];
        }

//        LOG4CPLUS_DEBUG(mlog, "\tCaching color data into file <"
//                        << cacheFilename << ">...");

//        // write out a cache file to enhance the loading process
//        cacheFile = fopen(cacheFilename.c_str(), "wb");
//        fwrite(&tiffImg[0], sizeof(GLbyte), imgSize, cacheFile);
//        fclose(cacheFile);
//    }

#ifdef ENABLE_MET3D_STOPWATCH
    stopwatch.split();
    LOG4CPLUS_DEBUG(mlog, "GeoTIFF read in "
                    << stopwatch.getLastSplitTime(MStopwatch::SECONDS)
                    << " seconds.\n" << flush);
#endif

    // at the end close the tiff file
    //GDALClose(tiffData);

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();

    if (texture == nullptr)
    {
        QString textureID = QString("baseMap_#%1").arg(getID());
        texture = new GL::MTexture(textureID, GL_TEXTURE_2D, GL_RGB8,
                                   longitudeDim, latitudeDim);

        if (!glRM->tryStoreGPUItem(texture))
        {
            delete texture;
            texture = nullptr;
        }
    }

    if (texture)
    {
        texture->updateSize(longitudeDim, latitudeDim);

        glRM->makeCurrent();
        texture->bindToLastTextureUnit();

        // Set texture parameters: wrap mode and filtering (RTVG p. 64).
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        // DEBUG
        //std::cout << longitudeDim * latitudeDim * colorDim << std::endl;
        //GLint size;
        //glGetIntegerv(GL_MAX_TEXTURE_SIZE,&size);
        //std::cout << size << std::endl << flush;

        // Upload data array to GPU.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                     longitudeDim, latitudeDim,
                     0, GL_RGB, GL_UNSIGNED_BYTE, &tiffImg[0]); CHECK_GL_ERROR;
    }
}

QVector4D MBaseMapActor::getBBoxOfRotatedBBox()
{
    return QVector4D(-180., -90., 180., 90.);
}


void MBaseMapActor::adaptBBoxForRotatedGrids()
{
    double right, left, lower, upper;
    // Bounding box covers the full "east wast extend" of the sphere.
    if (bBoxConnection->eastWestExtent() >= 360.)
    {
        left = -180.;
        right = 180.;
    }
    else
    {
        // Map values greater than 360 and smaller than -360 to one of their
        // representative in the interval [-360, 360]

        left = fmod(bBoxConnection->westLon(), 360.);
        right = fmod(bBoxConnection->eastLon(), 360.);

        // Map left and right to the interval [-180, 180]

        if (left > 180.)
        {
            left -= 360.;
        }
        else if (left < -180.)
        {
            left += 360.;
        }
        if (right > 180.)
        {
            right -= 360.;
        }
        else if (right < -180.)
        {
            right += 360.;
        }

    }
    // Bounding box covers the full "north south extend" of the sphere.
    if (bBoxConnection->northSouthExtent() >= 360.)
    {
        lower = -90.;
        upper = 90.;
    }
    else
    {
        // Map values greater than 360 and smaller than -360 to one of their
        // representative in the interval [-360, 360]

        lower = fmod(bBoxConnection->southLat(), 360.);
        upper = fmod(bBoxConnection->northLat(), 360.);

        // Map lower and upper to the interval [-90, 90] by mapping the values
        // at the "backside" of the sphere to -90 and 90 respectively.

        if (lower > 180.)
        {
            lower -= 360.;
        }
        else if (lower < -180.)
        {
            lower += 360.;
        }
        if (upper > 180.)
        {
            upper -= 360.;
        }
        else if (upper < -180.)
        {
            upper += 360.;
        }
        if (lower > 90.)
        {
            lower = 90.;
        }
        else if (lower < -90.)
        {
            lower = -90.;
        }
        if (upper > 90.)
        {
            upper = 90.;
        }
        else if (upper < -90.)
        {
            upper = -90.;
        }
    }
    bboxForRotatedGrids = QVector4D(left, lower, right, upper);
}

} // namespace Met3D
