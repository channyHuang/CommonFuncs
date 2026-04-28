#pragma once

#include "tiny_gltf.h"

class GLBWriter {
public:
    GLBWriter();
    ~GLBWriter() {}

    void writeModel2File(const std::string &sGLFName);

    void addMeshToModel(tinygltf::Model& model, 
                   const std::vector<float>& positions,
                   const std::vector<float>& normals,
                   const std::vector<float>& texCoords,
                   const std::vector<unsigned short>& indices,
                   int materialIndex,
                   int bufferStart);

    bool loadImageData(const std::string& filename, std::vector<unsigned char>& imageData, 
                        int& width, int& height, int& channels);

    bool write(const std::string& sGLBFileName);

    bool addTexture2Model(const std::string& sTextureFile);

private:
    

    bool AddBufferAndAccessor(tinygltf::Model& model, const void* data, 
                        size_t byteLength, int target, int componentType, int type, int count, 
                        std::vector<double>& minValues, std::vector<double>& maxValues);

private:
    tinygltf::Model m_stModel;
    tinygltf::TinyGLTF m_stLoader;
};