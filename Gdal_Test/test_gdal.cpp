#include<iostream>
#include "gdal_priv.h"
#include "gdalwarper.h"
#include "ogr_spatialref.h"

int main()
{
    GDALDatasetH  hSrcDS, hDstDS;

    GDALAllRegister();
    // Open input file
    hSrcDS = GDALOpen( "HYP_50M_SR_W.tif", GA_ReadOnly );
    CPLAssert( hSrcDS != NULL );


    // Create output with same datatype as first input band.
    GDALDataType eDT;
    eDT = GDALGetRasterDataType(GDALGetRasterBand(hSrcDS,1));

    // Get output driver (GeoTIFF format)
    GDALDriverH hDriver;
    hDriver = GDALGetDriverByName( "GTiff" );
    CPLAssert( hDriver != NULL );

    // Get Source coordinate system.
    const char *pszSrcWKT;
    pszSrcWKT = GDALGetProjectionRef( hSrcDS );
    CPLAssert( pszSrcWKT != NULL && strlen(pszSrcWKT) > 0 );


    // Setup output coordinate system that is UTM 11 WGS84.
    char *pszDstWKT = NULL;
    OGRSpatialReference oSRS;
    oSRS.SetUTM( 11, TRUE );
    oSRS.SetWellKnownGeogCS( "WGS84" );
    oSRS.exportToWkt( &pszDstWKT );


    // Create a transformer that maps from source pixel/line coordinates
    // to destination georeferenced coordinates (not destination
    // pixel line).  We do that by omitting the destination dataset
    // handle (setting it to NULL).
    void *hTransformArg;
    hTransformArg =
    GDALCreateGenImgProjTransformer( hSrcDS, pszSrcWKT, NULL, pszDstWKT,
                                     FALSE, 0, 1 );
    CPLAssert( hTransformArg != NULL );

    // Get approximate output georeferenced bounds and resolution for file.
    double adfDstGeoTransform[6];
    int nPixels=0, nLines=0;
    CPLErr eErr;
    eErr = GDALSuggestedWarpOutput( hSrcDS,
           GDALGenImgProjTransform, hTransformArg,
           adfDstGeoTransform, &nPixels, &nLines );
    CPLAssert( eErr == CE_None );
    GDALDestroyGenImgProjTransformer( hTransformArg );

    // Create output files.
    hDstDS = GDALCreate( hDriver, "out.tif", nPixels, nLines,
                     GDALGetRasterCount(hSrcDS), eDT, NULL );
    CPLAssert( hDstDS != NULL );

    // Write out the projection definition.
    GDALSetProjection( hDstDS, pszDstWKT );
    GDALSetGeoTransform( hDstDS, adfDstGeoTransform );

    // Copy the color table, if required.
    GDALColorTableH hCT;
    hCT = GDALGetRasterColorTable( GDALGetRasterBand(hSrcDS,1) );
    if( hCT != NULL )
        GDALSetRasterColorTable( GDALGetRasterBand(hDstDS,1), hCT );

    // Setup warp options.
    GDALWarpOptions *psWarpOptions = GDALCreateWarpOptions();
    psWarpOptions->hSrcDS = hSrcDS;
    psWarpOptions->hDstDS = hDstDS;
    psWarpOptions->nBandCount = 1;
    psWarpOptions->panSrcBands =
        (int *) CPLMalloc(sizeof(int) * psWarpOptions->nBandCount );
    psWarpOptions->panSrcBands[0] = 1;
    psWarpOptions->panDstBands =
        (int *) CPLMalloc(sizeof(int) * psWarpOptions->nBandCount );
    psWarpOptions->panDstBands[0] = 1;
    psWarpOptions->pfnProgress = GDALTermProgress;

    // Establish reprojection transformer.
    psWarpOptions->pTransformerArg =
        GDALCreateGenImgProjTransformer( hSrcDS,
                                        GDALGetProjectionRef(hSrcDS),
                                        hDstDS,
                                        GDALGetProjectionRef(hDstDS),
                                        FALSE, 0.0, 1 );
    psWarpOptions->pfnTransformer = GDALGenImgProjTransform;

    // Initialize and execute the warp operation.
    GDALWarpOperation oOperation;
    oOperation.Initialize( psWarpOptions );
    oOperation.ChunkAndWarpImage( 0, 0,
                                GDALGetRasterXSize( hDstDS ),
                                GDALGetRasterYSize( hDstDS ) );
    GDALDestroyGenImgProjTransformer( psWarpOptions->pTransformerArg );
    GDALDestroyWarpOptions( psWarpOptions );
    GDALClose( hDstDS );
    GDALClose( hSrcDS );
    GDALDestroyDriverManager();
    return 0;
}
