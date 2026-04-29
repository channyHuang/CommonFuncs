#pragma once

#include <vector>
#include <string>
#include <algorithm>

#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h>
#include <ogr_featurestyle.h>

namespace Haige {
    
struct GeoInfo {
    double fCenterLongitude;
    double fCenterLatitude;
    float fMeterPerPixel = 0.1f; // meter per pixel
    std::string sProjection = "PROJCS[\"WGS 84 / UTM zone 50N\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],PROJECTION[\"Transverse_Mercator\"],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",117],PARAMETER[\"scale_factor\",0.9996],PARAMETER[\"false_easting\",500000],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1]]";

    GeoInfo(double fLon = 110, double fLat = 23, float fMPP = 0.2f) {
        fCenterLongitude = fLon;
        fCenterLatitude = fLat;
        fMeterPerPixel = fMPP;
    }
};

class GeneratorGeoTiff {
public:
    static GeneratorGeoTiff& getInstance() {
        static GeneratorGeoTiff m_pInstance;
        return m_pInstance;
    }

    GeneratorGeoTiff(const GeneratorGeoTiff&) = delete;
    GeneratorGeoTiff& operator=(const GeneratorGeoTiff&) = delete;

    static bool convert(const GeoInfo& stGeoInfo, const std::string& sPngPath, const std::string& sOutTiffPath);
    static bool convert(const GeoInfo& stGeoInfo, 
                    size_t nWidth, size_t nHeight, const std::vector<float>& vHeightData,
                    const std::string& sOutTiffPath,
                    const std::string& sInfo = "");

    static int unittest();

private:
    GeneratorGeoTiff();

    static std::string generateUTMProjectionWKT(double fLon, double fLat);
    static void calculateGeotransform(int nWidth, int nHeight, 
                                const GeoInfo& stGeoInfo, 
                                double fGeoTransform[6]);
    static void setMetadata(GDALDataset* stDataset, const GeoInfo& stGeoInfo);
}; 

} // end of namespace Haige
