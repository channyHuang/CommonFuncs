#pragma once

#include <fstream>
#include <vector>
#include <cstdint>

// #include "GLBWriter.h"

#pragma pack(push, 1)
struct B3DMHeader {
    char cMagic[4] = {'b', '3', 'd', 'm'};
    uint32_t nVersion = 1;
    uint32_t nByteLength = 0;
    uint32_t nFeatureTableJsonLength = 0;
    uint32_t nFeatureTableBinaryLength = 0;
    uint32_t nBatchTableJsonLength = 0;
    uint32_t nBatchTableBinaryLength = 0;
};
#pragma pack(pop)

class B3DMWriter {
public:
    bool writeB3DM(const std::string &sGLBFileName, const std::vector<double> &vPosition = {0, 0, 0});
    bool parseB3DM2GLB(const std::string& sB3DMFileName, const std::string &sGLBFileName);
 
    bool writeGLB2B3DM(const std::string &sGLBFileName, size_t nMeshCount = 1, const std::string& sOutName = "haige.b3dm");

private:
    std::vector<uint8_t> readGLBFile(const std::string& sGLBFileName);
    void writePadding(std::ofstream& file, size_t currentSize);

    std::vector<double> calculateRegion(const std::vector<double>& position);

    bool writeB3DMFile(const std::string& outputPath, const std::vector<uint8_t>& glbData);
    bool writeTilesetJSON(const std::string& outputPath, const std::vector<double>& position);

       

private:
    const size_t m_nHeaderLength = 28;
};
