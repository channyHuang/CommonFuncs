#include <vector>
#include <string>

#include <gdal.h>
#include <gdal_priv.h>
// #include <gdal/gdal_warper.h>
#include <cpl_conv.h>
#include <ogr_featurestyle.h>


class PngToGeoTiffConverter {
public:
    struct GeoInfo {
        double centerLongitude;  // 中心点经度
        double centerLatitude;   // 中心点纬度
        double pixelHeight;      // 每个像素对应的高度（米）
        double groundResolution; // 地面分辨率（米/像素）
        std::string projection;  // 坐标系统
    };

    static bool convert(const std::string& pngPath, 
                       const std::string& tiffPath,
                       const GeoInfo& geoInfo,
                       const std::vector<float>& heightData) {
        
        // 初始化GDAL
        GDALAllRegister();
        
        // 1. 读取PNG图像
        GDALDataset* pngDataset = (GDALDataset*)GDALOpen(pngPath.c_str(), GA_ReadOnly);
        if (!pngDataset) {
            printf("无法打开PNG文件: %s\n", pngPath.c_str());
            return false;
        }
        
        int width = pngDataset->GetRasterXSize();
        int height = pngDataset->GetRasterYSize();
        int bands = pngDataset->GetRasterCount();
        
        printf("图像尺寸: %d x %d, 波段数: %d\n", width, height, bands);
        
        // 2. 创建GeoTIFF
        GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        if (!driver) {
            printf("无法创建GTiff驱动\n");
            GDALClose(pngDataset);
            return false;
        }
        
        // 创建输出数据集（多一个波段存储高度）
        char** options = nullptr;
        options = CSLSetNameValue(options, "COMPRESS", "LZW");
        options = CSLSetNameValue(options, "TILED", "YES");
        
        GDALDataset* tiffDataset = driver->Create(
            tiffPath.c_str(), width, height, bands + 1, GDT_Float32, options
        );
        
        CSLDestroy(options);
        
        if (!tiffDataset) {
            printf("无法创建TIFF文件: %s\n", tiffPath.c_str());
            GDALClose(pngDataset);
            return false;
        }
        
        // 3. 计算地理变换参数
        double geotransform[6];
        calculateGeotransform(width, height, geoInfo, geotransform);
        tiffDataset->SetGeoTransform(geotransform);
        
        // 4. 设置坐标系统
        tiffDataset->SetProjection(geoInfo.projection.c_str());
        
        // 5. 复制RGB数据
        for (int i = 1; i <= bands; i++) {
            GDALRasterBand* srcBand = pngDataset->GetRasterBand(i);
            GDALRasterBand* dstBand = tiffDataset->GetRasterBand(i);
            
            // 读取PNG数据
            std::vector<uint8_t> buffer(width * height);
            CPLErr err = srcBand->RasterIO(GF_Read, 0, 0, width, height, 
                                         buffer.data(), width, height, 
                                         GDT_Byte, 0, 0);
            
            if (err == CE_None) {
                // 写入TIFF
                dstBand->RasterIO(GF_Write, 0, 0, width, height,
                                buffer.data(), width, height,
                                GDT_Byte, 0, 0);
                
                // 设置波段描述
                const char* bandNames[] = {"Red", "Green", "Blue", "Alpha"};
                if (i <= 4) {
                    dstBand->SetDescription(bandNames[i-1]);
                }
            }
        }
        
        // 6. 添加高度数据波段
        if (!heightData.empty() && heightData.size() == width * height) {
            GDALRasterBand* heightBand = tiffDataset->GetRasterBand(bands + 1);
            heightBand->RasterIO(GF_Write, 0, 0, width, height,
                               (void*)heightData.data(), width, height,
                               GDT_Float32, 0, 0);
            heightBand->SetDescription("Elevation");
            heightBand->SetUnitType("meters");
            
            // 设置高度数据的统计信息
            float minHeight = *std::min_element(heightData.begin(), heightData.end());
            float maxHeight = *std::max_element(heightData.begin(), heightData.end());
            heightBand->SetStatistics(minHeight, maxHeight, 0, 0);
        }
        
        // 7. 设置元数据
        setMetadata(tiffDataset, geoInfo);
        
        // 清理
        GDALClose(pngDataset);
        GDALClose(tiffDataset);
        
        printf("成功转换: %s -> %s\n", pngPath.c_str(), tiffPath.c_str());
        return true;
    }

private:
    static void calculateGeotransform(int width, int height, 
                                    const GeoInfo& geoInfo, 
                                    double geotransform[6]) {
        // 计算左上角坐标
        double pixelSize = geoInfo.groundResolution;
        double centerX = geoInfo.centerLongitude;
        double centerY = geoInfo.centerLatitude;
        
        double topLeftX = centerX - (width / 2.0) * pixelSize;
        double topLeftY = centerY + (height / 2.0) * pixelSize;
        
        geotransform[0] = topLeftX;   // 左上角X
        geotransform[1] = pixelSize;  // 像素宽度
        geotransform[2] = 0;          // 旋转参数
        geotransform[3] = topLeftY;   // 左上角Y
        geotransform[4] = 0;          // 旋转参数
        geotransform[5] = -pixelSize; // 像素高度（负值）
    }
    
    static void setMetadata(GDALDataset* dataset, const GeoInfo& geoInfo) {
        // 设置基本的TIFF标签
        dataset->SetMetadataItem("AREA_OR_POINT", "Area");
        dataset->SetMetadataItem("TIFFTAG_SOFTWARE", "PNG to GeoTIFF Converter");
        
        // 设置地理信息元数据
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%.6f", geoInfo.centerLongitude);
        dataset->SetMetadataItem("CENTER_LONGITUDE", buffer);
        
        snprintf(buffer, sizeof(buffer), "%.6f", geoInfo.centerLatitude);
        dataset->SetMetadataItem("CENTER_LATITUDE", buffer);
        
        snprintf(buffer, sizeof(buffer), "%.2f", geoInfo.groundResolution);
        dataset->SetMetadataItem("GROUND_RESOLUTION", buffer);
    }
};

int _main() {
    // 设置地理信息
    PngToGeoTiffConverter::GeoInfo geoInfo;
    geoInfo.centerLongitude = 116.3974;  // 北京经度
    geoInfo.centerLatitude = 39.9093;    // 北京纬度
    geoInfo.groundResolution = 10.0;     // 10米/像素
    geoInfo.pixelHeight = 50.0;          // 每个像素高度50米
    geoInfo.projection = "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]";
    
    // 创建模拟高度数据（实际中从您的数据源读取）
    int width = 1000, height = 800;
    std::vector<float> heightData(width * height, 50.0f); // 所有像素50米高度
    
    // 执行转换
    bool success = PngToGeoTiffConverter::convert(
        "/home/channy/Documents/datasets/HGReconstruct-ref/scene_dense_texture_orthomap.png",
        "output.tif",
        geoInfo,
        heightData
    );
    
    return success ? 0 : 1;
}