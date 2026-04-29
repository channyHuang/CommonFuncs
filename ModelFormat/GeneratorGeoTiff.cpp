#include "GeneratorGeoTiff.h"

namespace Haige {

GeneratorGeoTiff::GeneratorGeoTiff() {
    GDALAllRegister();
}

std::string GeneratorGeoTiff::generateUTMProjectionWKT(double fLon, double fLat) {
    if (fLon < -180 || fLon > 180 || fLat < -80 || fLat > 84) {
        // 超出UTM适用范围，可回退到地理坐标系或报错
        return "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]";
    }

    int zone = static_cast<int>(std::floor((fLon + 180.0) / 6.0)) + 1;
    bool isNorth = (fLat >= 0);
    
    double centralMeridian = zone * 6.0 - 183.0; // 中央经线
    int epsg = isNorth ? (32600 + zone) : (32700 + zone);
    const char* hemisphere = isNorth ? "N" : "S";

    char wkt[1024];
    snprintf(wkt, sizeof(wkt),
        "PROJCS[\"WGS 84 / UTM zone %d%s\","
            "GEOGCS[\"WGS 84\","
                "DATUM[\"WGS_1984\","
                    "SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],"
                    "AUTHORITY[\"EPSG\",\"6326\"]],"
                "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
                "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],"
                "AUTHORITY[\"EPSG\",\"4326\"]],"
            "PROJECTION[\"Transverse_Mercator\"],"
            "PARAMETER[\"latitude_of_origin\",0],"
            "PARAMETER[\"central_meridian\",%.1f],"
            "PARAMETER[\"scale_factor\",0.9996],"
            "PARAMETER[\"false_easting\",500000],"
            "PARAMETER[\"false_northing\",%d],"
            "UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],"
            "AXIS[\"Easting\",EAST],"
            "AXIS[\"Northing\",NORTH],"
            "AUTHORITY[\"EPSG\",\"%d\"]]",
        zone, hemisphere,
        centralMeridian,
        isNorth ? 0 : 10000000,
        epsg
    );

    return std::string(wkt);
}

void GeneratorGeoTiff::calculateGeotransform(int nWidth, int nHeight, 
                                const GeoInfo& stGeoInfo, 
                                double fGeoTransform[6]) {
    double fPixelSize = stGeoInfo.fMeterPerPixel;
    double centerX = stGeoInfo.fCenterLongitude;
    double centerY = stGeoInfo.fCenterLatitude;
    
    double topLeftX = centerX - (nWidth / 2.0) * fPixelSize;
    double topLeftY = centerY + (nHeight / 2.0) * fPixelSize;
    
    fGeoTransform[0] = topLeftX;   // 左上角X
    fGeoTransform[1] = fPixelSize;  // 像素宽度
    fGeoTransform[2] = 0;          // 旋转参数
    fGeoTransform[3] = topLeftY;   // 左上角Y
    fGeoTransform[4] = 0;          // 旋转参数
    fGeoTransform[5] = -fPixelSize; // 像素高度（负值）
}

bool GeneratorGeoTiff::convert(const GeoInfo& stGeoInfo, 
                    size_t nWidth, size_t nHeight, const std::vector<float>& vHeightData,
                    const std::string& sOutTiffPath,
                    const std::string& sInfo) {
    if (nWidth * nHeight != vHeightData.size()) {
        printf("width*height != data size\n");
        return false;
    }
    // 1. create GeoTIFF
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!pDriver) {
        printf("create GDAL driver failed\n");
        return false;
    }
        
    // 2. create datasets
    char** options = nullptr;
    options = CSLSetNameValue(options, "COMPRESS", "DEFLATE");
    options = CSLSetNameValue(options, "TILED", "YES");
    GDALDataset* pTiffDataset = pDriver->Create(
        sOutTiffPath.c_str(), nWidth, nHeight, 1, GDT_Float32, options
    );
    CSLDestroy(options);
    if (!pTiffDataset) {
        printf("create Tiff file failed: %s\n", sOutTiffPath.c_str());
        return false;
    }

    // 3. compute transform
    double fGeotransform[6];
    calculateGeotransform(nWidth, nHeight, stGeoInfo, fGeotransform);
    pTiffDataset->SetGeoTransform(fGeotransform);
        
    // 4. set coord
    pTiffDataset->SetProjection(generateUTMProjectionWKT(stGeoInfo.fCenterLongitude, stGeoInfo.fCenterLatitude).c_str());

    // 5. add height data
    if (!vHeightData.empty() && vHeightData.size() == nWidth * nHeight) {
        GDALRasterBand* pHeightBand = pTiffDataset->GetRasterBand(1);
        pHeightBand->RasterIO(GF_Write, 0, 0, nWidth, nHeight,
                            (void*)vHeightData.data(), nWidth, nHeight,
                            GDT_Float32, 0, 0);
        pHeightBand->SetDescription("Elevation");
        pHeightBand->SetUnitType("meters");
        
        // set statistic data
        float minHeight = *std::min_element(vHeightData.begin(), vHeightData.end());
        float maxHeight = *std::max_element(vHeightData.begin(), vHeightData.end());
        pHeightBand->SetStatistics(minHeight, maxHeight, 0, 0);
        pHeightBand->SetDescription(sInfo.c_str());
    }

    // 6. set metadata
    setMetadata(pTiffDataset, stGeoInfo);
        
    // clear
    GDALClose(pTiffDataset);
    return true;
}

bool GeneratorGeoTiff::convert(const GeoInfo& stGeoInfo, const std::string& sPngPath, const std::string& sOutTiffPath) {
    // 1. read png
    GDALDataset* pPngDataset = (GDALDataset*)GDALOpen(sPngPath.c_str(), GA_ReadOnly);
    if (!pPngDataset) {
        printf("file open failed: %s\n", sPngPath.c_str());
        return false;
    }
    int nWidth = pPngDataset->GetRasterXSize();
    int nHeight = pPngDataset->GetRasterYSize();
    int nBands = pPngDataset->GetRasterCount();
    // printf("GeneratorGeoTiff::convert [%d] %d %d %d\n", i, nWidth, nHeight, nBands);

    // 2. create GeoTIFF
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!pDriver) {
        printf("create GDAL driver failed\n");
        GDALClose(pPngDataset);
        return false;
    }
        
    // 3. create datasets
    char** options = nullptr;
    options = CSLSetNameValue(options, "COMPRESS", "DEFLATE");
    options = CSLSetNameValue(options, "TILED", "YES");
    GDALDataset* pTiffDataset = pDriver->Create(
        sOutTiffPath.c_str(), nWidth, nHeight, nBands, GDT_Byte, options
    );
    CSLDestroy(options);
    if (!pTiffDataset) {
        printf("create Tiff file failed: %s\n", sOutTiffPath.c_str());
        GDALClose(pPngDataset);
        return false;
    }
        
    // 4. compute transform
    double fGeotransform[6];
    calculateGeotransform(nWidth, nHeight, stGeoInfo, fGeotransform);
    pTiffDataset->SetGeoTransform(fGeotransform);
        
    // 5. set coord
    pTiffDataset->SetProjection(generateUTMProjectionWKT(stGeoInfo.fCenterLongitude, stGeoInfo.fCenterLatitude).c_str());
        
    const char* bandNames[] = {"Red", "Green", "Blue", "Alpha"};
    // 6. copy RGB data to float
    for (int i = 1; i <= nBands; i++) {
        GDALRasterBand* pSrcBand = pPngDataset->GetRasterBand(i);
        GDALRasterBand* pDstBand = pTiffDataset->GetRasterBand(i);

        // read Byte data
        std::vector<uint8_t> vByteBuffer(nWidth * nHeight);
        CPLErr err = pSrcBand->RasterIO(GF_Read, 0, 0, nWidth, nHeight,
                                    vByteBuffer.data(), nWidth, nHeight,
                                    GDT_Byte, 0, 0);
        if (err != CE_None) {
            printf("read band %d failed\n", i);
            continue;
        }

        // float（ 0～255  /255.0f ）
        // std::vector<float> vFloatBuffer(nWidth * nHeight);
        // for (size_t j = 0; j < vByteBuffer.size(); ++j) {
        //     vFloatBuffer[j] = static_cast<float>(vByteBuffer[j]); 
        // }

        err = pDstBand->RasterIO(GF_Write, 0, 0, nWidth, nHeight,
                        vByteBuffer.data(), nWidth, nHeight,
                        GDT_Byte, 0, 0);
        if (err != CE_None) {
            printf("write band %d failed\n", i);
            continue;
        }

        // band description
        if (i <= 4) {
            pDstBand->SetDescription(bandNames[i-1]);
        }
    }
        
    // 7. set metadata
    setMetadata(pTiffDataset, stGeoInfo);
        
    // clear
    GDALClose(pTiffDataset);
    GDALClose(pPngDataset);

    if (CPLGetLastErrorNo() != 0) {
        printf("Close error: %s\n", CPLGetLastErrorMsg());
    }
    return true;
}


void GeneratorGeoTiff::setMetadata(GDALDataset* stDataset, const GeoInfo& stGeoInfo) {
    // 设置基本的TIFF标签
    stDataset->SetMetadataItem("AREA_OR_POINT", "Area");
    stDataset->SetMetadataItem("TIFFTAG_SOFTWARE", "广州海格通信集团股份有限公司 haige@haige.com 版权所有");
    
    // 设置地理信息元数据
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%.6f", stGeoInfo.fCenterLongitude);
    stDataset->SetMetadataItem("CENTER_LONGITUDE", buffer);
    
    snprintf(buffer, sizeof(buffer), "%.6f", stGeoInfo.fCenterLatitude);
    stDataset->SetMetadataItem("CENTER_LATITUDE", buffer);
    
    snprintf(buffer, sizeof(buffer), "%.2f", stGeoInfo.fMeterPerPixel);
    stDataset->SetMetadataItem("GROUND_RESOLUTION", buffer);
}

int GeneratorGeoTiff::unittest() {
    Haige::GeoInfo stGeoInfo;
    stGeoInfo.fCenterLongitude = 110.304;  
    stGeoInfo.fCenterLatitude = 21.1211;
    stGeoInfo.fMeterPerPixel = 1.0;
    
    int nWidth = 1024, nHeight = 680;
    std::vector<float> vHeightData(nWidth * nHeight, 50.0f); // 所有像素50米高度
    stGeoInfo.sProjection = generateUTMProjectionWKT(stGeoInfo.fCenterLongitude, stGeoInfo.fCenterLatitude);
    // 执行转换
    bool success = convert(
        stGeoInfo,
        "/home/channy/Documents/datasets/HaigeReconstruct-ref/scene_dense_texture_orthomap.png",
        "output.tif"
    );
    
    return success ? 0 : 1;
}

} // end of namespace Haige
