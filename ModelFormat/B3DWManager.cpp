#include "B3DWManager.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <iomanip>
#include <cstring>

std::vector<uint8_t> B3DMWriter::readGLBFile(const std::string& sGLBFileName) {
    std::ifstream file(sGLBFileName, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Open GLB file failed: " << sGLBFileName << std::endl;
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Read GLB file failed: " << sGLBFileName << std::endl;
        return {};
    }

    std::cout << "Read GLB file success: " << sGLBFileName << " (" << size << " bytes)" << std::endl;
    return buffer;
}

void B3DMWriter::writePadding(std::ofstream& file, size_t currentSize) {
    size_t padding = (8 - (currentSize % 8)) % 8;
    for (size_t i = 0; i < padding; ++i) {
        file.put(0x20); 
    }
}

bool B3DMWriter::writeB3DM(const std::string &sGLBFileName, const std::vector<double> &vPosition) {
    auto glbData = readGLBFile(sGLBFileName);
    if (glbData.empty()) {
        std::cerr << "GLB file read failed: " << sGLBFileName << std::endl;
        return false;
    }

    auto npos = sGLBFileName.find_last_of('.');
    std::string sB3DMFileName = sGLBFileName.substr(0, npos) + ".b3dm";
    if (!writeB3DMFile(sB3DMFileName, glbData)) {
        std::cerr << "B3DM write failed" << std::endl;
        return false;
    }

    // tileset.json
    std::string tilesetPath = "tileset.json";
    if (!writeTilesetJSON(tilesetPath, vPosition)) {
        std::cerr << "tileset.json write failed" << std::endl;
        return false;
    }
    
    return true;
}

bool B3DMWriter::writeGLB2B3DM(const std::string &sGLBFileName, size_t nMeshCount, const std::string& sOutName) {
    // auto glbData = readGLBFile(sGLBFileName);
    // uint32_t glbLength = static_cast<uint32_t>(glbData.size());

    // B3DMHeader stHeader;

    // nlohmann::json stJsonFeatureTable;
    // stJsonFeatureTable["BATCH_LENGTH"] = std::to_string(nMeshCount);

    // std::string sFeatureTableJson = stJsonFeatureTable.dump();
    // while (((sFeatureTableJson.length() + m_nHeaderLength) & 7) != 0) {
    //     sFeatureTableJson.push_back(' ');
    // }

    // nlohmann::json stJsonBatchTable;
    // std::vector<uint32_t> vIds;
    // std::vector<std::string> vNames;
    // for (size_t i = 0; i < nMeshCount; ++i) {
    //     vIds.push_back(i);
    //     vNames.push_back("mesh_" + std::to_string(i));
    // }
    // stJsonBatchTable["batchId"] = vIds;
    // stJsonBatchTable["name"] = vNames;

    // std::string sBatchTableJson = stJsonBatchTable.dump();
    // while ((sBatchTableJson.length() & 7) != 0) {
    //     sBatchTableJson.push_back(' ');
    // }

    // stHeader.nFeatureTableJsonLength = sFeatureTableJson.length();
    // stHeader.nBatchTableJsonLength = sBatchTableJson.length();
    // stHeader.nByteLength = m_nHeaderLength + stHeader.nFeatureTableJsonLength + stHeader.nBatchTableJsonLength + glbLength;


    // std::ofstream file(sOutName, std::ios::binary);
    // file.write(reinterpret_cast<const char*>(&stHeader), sizeof(stHeader));
    // file.write(sFeatureTableJson.c_str(), sFeatureTableJson.length());
    // file.write(sBatchTableJson.c_str(), sBatchTableJson.length());
    // file.write(reinterpret_cast<const char*>(glbData.data()), glbData.size());
    // file.close();
    return true;
}

bool B3DMWriter::writeB3DMFile(const std::string& outputPath, const std::vector<uint8_t>& glbData) {
    if (glbData.empty()) {
        std::cerr << "GLB data empty!" << std::endl;
        return false;
    }

    std::string featureTableJSON = "{\"BATCH_LENGTH\":2}";
    std::string batchTableJSON = "{}";

    uint32_t featureTableJSONLength = static_cast<uint32_t>(featureTableJSON.length());
    uint32_t batchTableJSONLength = static_cast<uint32_t>(batchTableJSON.length());
    uint32_t glbLength = static_cast<uint32_t>(glbData.size());

    uint32_t featureTableJSONPadded = featureTableJSONLength + ((8 - (featureTableJSONLength % 8)) % 8);
    uint32_t batchTableJSONPadded = batchTableJSONLength + ((8 - (batchTableJSONLength % 8)) % 8);

    uint32_t totalLength = sizeof(B3DMHeader) + featureTableJSONPadded + batchTableJSONPadded + glbLength;

    B3DMHeader header;
    std::memset(&header, 0, sizeof(header)); 
    std::memcpy(header.cMagic, "b3dm", 4);
    header.nVersion = 1;
    header.nByteLength = totalLength;
    header.nFeatureTableJsonLength = featureTableJSONLength;
    header.nFeatureTableBinaryLength = 0;
    header.nBatchTableJsonLength = batchTableJSONLength;
    header.nBatchTableBinaryLength = 0;

    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "write B3DM file failed: " << outputPath << std::endl;
        return false;
    }

    // 1. header
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    // 2. Feature Table JSON
    file.write(featureTableJSON.c_str(), featureTableJSONLength);
    // Feature Table JSON fill
    size_t featureWritten = sizeof(header) + featureTableJSONLength;
    writePadding(file, featureTableJSONLength);
    // 3. Batch Table JSON
    file.write(batchTableJSON.c_str(), batchTableJSONLength);
    // Batch Table JSON fill
    writePadding(file, batchTableJSONLength);
    // 4. 写入GLB数据
    file.write(reinterpret_cast<const char*>(glbData.data()), glbData.size());
    file.close();

    std::ifstream check(outputPath, std::ios::binary | std::ios::ate);
    size_t actualSize = check.tellg();
    check.close();
    
    if (actualSize != totalLength) {
        std::cerr << "Failed: actualSize - totalLength = " << (actualSize - totalLength) << " 字节" << std::endl;
        return false;
    }

    return true;
}

bool B3DMWriter::writeTilesetJSON(const std::string& outputPath, const std::vector<double>& position) {
    auto region = calculateRegion(position);
    
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        std::cerr << "tileset.json create failed: " << outputPath << std::endl;
        return false;
    }

    file << std::setprecision(15);
    file << "{" << std::endl;
    file << "  \"asset\": {" << std::endl;
    file << "    \"version\": \"1.0\"" << std::endl;
    file << "  }," << std::endl;
    file << "  \"geometricError\": 256," << std::endl;
    file << "  \"root\": {" << std::endl;
    file << "    \"boundingVolume\": {" << std::endl;
    file << "      \"region\": [" << std::endl;
    file << "        " << region[0] << "," << std::endl;
    file << "        " << region[1] << "," << std::endl;
    file << "        " << region[2] << "," << std::endl;
    file << "        " << region[3] << "," << std::endl;
    file << "        " << region[4] << "," << std::endl;
    file << "        " << region[5] << std::endl;
    file << "      ]" << std::endl;
    file << "    }," << std::endl;
    file << "    \"geometricError\": 64," << std::endl;
    file << "    \"refine\": \"ADD\"," << std::endl;
    file << "    \"content\": {" << std::endl;
    file << "      \"uri\": \"tile.b3dm\"" << std::endl;
    file << "    }" << std::endl;
    file << "  }" << std::endl;
    file << "}" << std::endl;

    file.close();
    return true;
}


std::vector<double> B3DMWriter::calculateRegion(const std::vector<double>& position) {
    double size = 0.01;
    std::vector<double> region = {
        position[0] - size/2, // west
        position[1] - size/2, // south  
        position[0] + size/2, // east
        position[1] + size/2, // north
        position[2] - 100.0,  // minHeight
        position[2] + 100.0   // maxHeight
    };
    return region;
}

bool B3DMWriter::parseB3DM2GLB(const std::string& sB3DMFileName, const std::string &sGLBFileName) {
    std::ifstream ifs(sB3DMFileName, std::ios::binary | std::ios::in);
    if (!ifs.is_open()) {
        std::cout << "Open B3DM file failed: " << sB3DMFileName << std::endl;
        return false;
    }
    B3DMHeader stHeader;
    ifs.read(reinterpret_cast<char *>(&stHeader), 28);
    std::cout << stHeader.cMagic << " " << stHeader.nVersion << " " << stHeader.nByteLength << std::endl;
    size_t nLen = std::max(stHeader.nBatchTableJsonLength, stHeader.nFeatureTableJsonLength);
    char buffer[nLen];
    memset(buffer, 0, nLen);
    ifs.read(buffer, stHeader.nFeatureTableJsonLength);
    std::cout << "feature " << buffer << std::endl;
    memset(buffer, 0, nLen);
    ifs.read(buffer, stHeader.nBatchTableJsonLength);
    std::cout << "batch " << buffer << std::endl;

    nLen = stHeader.nByteLength - 28 - stHeader.nBatchTableJsonLength - stHeader.nFeatureTableJsonLength;
    char data[nLen];
    ifs.read(data, nLen);
    ifs.close();

    // auto npos = sB3DMFileName.find_last_of(".");
    // std::string sGLBFileName = sB3DMFileName.substr(0, npos) + ".glb";
    std::ofstream ofs(sGLBFileName.c_str(), std::ios::binary | std::ios::out);
    ofs.write(data, nLen);
    ofs.close();

    return true;
}
