#include <cstring>
#include <string>
#include <fstream>
#include <iostream>

#include "plySpecial/ply2glb.h"
#include "osgb2glb.h"

int main() {
    if (0) {
        std::string sPlyFile = "/home/channy/Documents/projects/HGReconstruct/build/results/20260114120044090/ply/scene_chunk_00.ply";
        std::string sGlbFile = "./out.glb";
        auto cPly2Glb = HG::ply::Ply2Glb();
        bool bres = cPly2Glb.ply2glb(sPlyFile.c_str(), sGlbFile.c_str());
    }

    {
        std::string sOsgbFile = "/home/channy/Documents/projects/HGReconstruct/build/results/20251224140000000/Data/scene_chunk_00.osgb";
        std::string sGlbFile = "./out.glb";
        HG::osgb2glb(sOsgbFile.c_str(), sGlbFile.c_str());
    }

    return 0;
}
