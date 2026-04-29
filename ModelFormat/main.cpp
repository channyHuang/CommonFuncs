#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>

// #include "png2tiff.hpp"

// int testGeoInfo() {
//     Haige::GeoInfo geoInfo;
//     std::vector<float> vData;
//     Haige::PngToGeoTiffConverter cvt;
//     cvt.convert("xx.png", "out.tif", geoInfo, vData);
//     return 0;
// }

#include "B3DWManager.h"

int main() {
    std::string sFolder = "/home/channy/Documents/projects/HaigeReconstruct/build/Scene/Data/Model/";
    std::string sGlbFolder = "./Model/";
    try {
        if (!std::filesystem::exists(sGlbFolder)) {
            std::filesystem::create_directory(sGlbFolder);
        }
        B3DMWriter man;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sFolder)) {
            if (std::filesystem::is_regular_file(entry.status())) {
                const auto& path = entry.path();
                std::string sExt = path.extension().string();

                if (std::strcmp(sExt.c_str(), ".b3dm") != 0) continue;
                std::string sFileNameWithoutExt = path.stem().string();
                // printf("%s %s %s\n", path.c_str(), sExt.c_str(), sFileNameWithoutExt.c_str());

                man.parseB3DM2GLB(path, sGlbFolder + "/" + sFileNameWithoutExt + ".glb");
            }
        }
    } catch (...) {}
    return 0;
}
