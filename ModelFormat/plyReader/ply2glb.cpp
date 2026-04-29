#include "ply2glb.h"

#include <vector>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstring>
#include <filesystem>
#include <webp/encode.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"

namespace HG {
    namespace ply {

// texture compress

std::vector<unsigned char> encodeToPNG_via_file(
    const std::vector<unsigned char>& pixelData,
    int width, int height, int channels) {
    
    std::vector<unsigned char> result;
    
    // 1. 将像素数据写入临时文件
    const char* temp_file = "temp_texture.png";
    
    // 使用 stbi_write_png（写入文件）
    int success = stbi_write_png(
        temp_file,
        width, height,
        channels,
        pixelData.data(),
        width * channels
    );
    
    if (!success) {
        std::cerr << "Failed to write PNG file" << std::endl;
        return result;
    }
    
    // 2. 读取文件内容
    FILE* f = fopen(temp_file, "rb");
    if (!f) {
        std::cerr << "Failed to open temp file" << std::endl;
        return result;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    result.resize(file_size);
    fread(result.data(), 1, file_size, f);
    fclose(f);
    
    // 3. 删除临时文件
    remove(temp_file);
    
    return result;
}

std::vector<unsigned char> encodeToJPG_via_file(
    const std::vector<unsigned char>& pixelData,
    int width, int height, int channels,
    int quality = 85) {  // JPG需要质量参数，默认85（0-100）
    
    std::vector<unsigned char> result;
    
    // 1. 将像素数据写入临时文件
    const char* temp_file = "temp_texture.jpg";
    
    // 使用 stbi_write_jpg（写入文件）
    // JPG只支持3通道(RGB)或1通道(灰度)，不支持4通道(RGBA)
    int output_channels = channels;
    if (channels == 4) {
        // 如果输入是RGBA，需要转换为RGB（JPG不支持透明度）
        std::vector<unsigned char> rgb_data(width * height * 3);
        for (int i = 0; i < width * height; ++i) {
            rgb_data[i * 3] = pixelData[i * 4];         // R
            rgb_data[i * 3 + 1] = pixelData[i * 4 + 1]; // G
            rgb_data[i * 3 + 2] = pixelData[i * 4 + 2]; // B
            // 忽略Alpha通道
        }
        
        int success = stbi_write_jpg(
            temp_file,
            width, height,
            3,  // 输出通道数为3
            rgb_data.data(),
            quality
        );
        
        if (!success) {
            std::cerr << "Failed to write JPG file" << std::endl;
            return result;
        }
    } else {
        // 对于RGB或灰度图，直接写入
        int success = stbi_write_jpg(
            temp_file,
            width, height,
            channels,
            pixelData.data(),
            quality
        );
        
        if (!success) {
            std::cerr << "Failed to write JPG file" << std::endl;
            return result;
        }
    }
    
    // 2. 读取文件内容
    FILE* f = fopen(temp_file, "rb");
    if (!f) {
        std::cerr << "Failed to open temp file" << std::endl;
        return result;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    result.resize(file_size);
    size_t read_count = fread(result.data(), 1, file_size, f);
    if (read_count != static_cast<size_t>(file_size)) {
        std::cerr << "Warning: File read incomplete" << std::endl;
    }
    fclose(f);
    
    // 3. 删除临时文件
    remove(temp_file);
    
    return result;
}

std::vector<unsigned char> encodeToWebP(
    const std::vector<unsigned char>& pixelData,
    int width, int height, int channels,
    float quality = 85.0f,  // WebP质量参数 (0-100)
    bool lossless = false) {  // 是否使用无损压缩
    
    std::vector<unsigned char> result;
    
    // 检查输入参数
    if (pixelData.empty() || width <= 0 || height <= 0) {
        std::cerr << "Invalid input parameters" << std::endl;
        return result;
    }
    
    // WebP编码配置
    WebPConfig config;
    if (!WebPConfigInit(&config)) {
        std::cerr << "WebPConfigInit failed" << std::endl;
        return result;
    }
    
    // 设置编码参数
    config.quality = quality;
    config.method = 6;  // 压缩方法 (0-6, 越高越慢但压缩率越好)
    config.lossless = lossless ? 1 : 0;
    
    if (!WebPValidateConfig(&config)) {
        std::cerr << "Invalid WebP config" << std::endl;
        return result;
    }
    
    // 准备WebP图片数据
    WebPPicture picture;
    if (!WebPPictureInit(&picture)) {
        std::cerr << "WebPPictureInit failed" << std::endl;
        return result;
    }
    
    picture.width = width;
    picture.height = height;
    picture.use_argb = 0;  // 使用YUV格式
    
    // 根据通道数设置格式
    if (channels == 4) {
        // RGBA数据
        uint8_t* webp_data = nullptr;
        size_t webp_size = 0;
        
        if (lossless) {
            webp_size = WebPEncodeLosslessRGBA(
                pixelData.data(), width, height, 
                width * channels, &webp_data);
        } else {
            webp_size = WebPEncodeRGBA(
                pixelData.data(), width, height, 
                width * channels, quality, &webp_data);
        }
        
        if (webp_data && webp_size > 0) {
            result.assign(webp_data, webp_data + webp_size);
            WebPFree(webp_data);
        }
        
    } else if (channels == 3) {
        // RGB数据，需要转换为RGBA（因为libwebp的RGB函数已弃用）
        std::vector<unsigned char> rgba_data(width * height * 4);
        for (int i = 0; i < width * height; ++i) {
            rgba_data[i * 4] = pixelData[i * 3];      // R
            rgba_data[i * 4 + 1] = pixelData[i * 3 + 1]; // G
            rgba_data[i * 4 + 2] = pixelData[i * 3 + 2]; // B
            rgba_data[i * 4 + 3] = 255;  // A（完全透明）
        }
        
        uint8_t* webp_data = nullptr;
        size_t webp_size = 0;
        
        if (lossless) {
            webp_size = WebPEncodeLosslessRGBA(
                rgba_data.data(), width, height, 
                width * 4, &webp_data);
        } else {
            webp_size = WebPEncodeRGBA(
                rgba_data.data(), width, height, 
                width * 4, quality, &webp_data);
        }
        
        if (webp_data && webp_size > 0) {
            result.assign(webp_data, webp_data + webp_size);
            WebPFree(webp_data);
        }
        
    } else if (channels == 1) {
        // 灰度图，需要转换为RGBA
        std::vector<unsigned char> rgba_data(width * height * 4);
        for (int i = 0; i < width * height; ++i) {
            rgba_data[i * 4] = pixelData[i];      // R
            rgba_data[i * 4 + 1] = pixelData[i];  // G
            rgba_data[i * 4 + 2] = pixelData[i];  // B
            rgba_data[i * 4 + 3] = 255;  // A
        }
        
        uint8_t* webp_data = nullptr;
        size_t webp_size = WebPEncodeRGBA(
            rgba_data.data(), width, height, 
            width * 4, quality, &webp_data);
        
        if (webp_data && webp_size > 0) {
            result.assign(webp_data, webp_data + webp_size);
            WebPFree(webp_data);
        }
    } else {
        std::cerr << "Unsupported number of channels: " << channels << std::endl;
    }
    
    return result;
}

bool loadImageData(const std::string& filename, std::vector<unsigned char>& imageData, 
                            int& width, int& height, int& channels) {
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return false;
    }
    
    imageData.assign(data, data + width * height * channels);
    stbi_image_free(data);
    return true;
}

bool addTexture2Model(const std::string& sTextureFile, tinygltf::Model& stModel) {
    // image
    std::vector<unsigned char> vImageData;
    int nWidth = 0, nHeight = 0, nChannel = 0;
    if (!loadImageData(sTextureFile, vImageData, nWidth, nHeight, nChannel)) {
        return false;
    }

    std::vector<unsigned char> vEncodedImageData = encodeToWebP(vImageData, nWidth, nHeight, nChannel);
    if (stModel.buffers.empty()) {
        tinygltf::Buffer buffer;
        buffer.uri = "";
        stModel.buffers.push_back(buffer);
    }

    auto& buffer = stModel.buffers[0];
    size_t nCurrentOffset = buffer.data.size();
    size_t nBufferViewOffset = nCurrentOffset;
    buffer.data.insert(buffer.data.end(), vEncodedImageData.begin(), vEncodedImageData.end());
    size_t nDataSize = vEncodedImageData.size();
    size_t nPadding = (4 - (nDataSize & 3)) & 3;
    for (size_t i = 0; i < nPadding; ++i) {
        buffer.data.push_back(0);
    }

    tinygltf::BufferView bufferView;
    bufferView.buffer = 0;
    bufferView.byteOffset = nBufferViewOffset;
    bufferView.byteLength = nDataSize;
    bufferView.byteStride = 0;

    int nBufferViewIndex = stModel.bufferViews.size();
    stModel.bufferViews.push_back(bufferView);

    tinygltf::Image stImage;
    stImage.width = nWidth;
    stImage.height = nHeight;
    stImage.component = nChannel;
    stImage.bits = 8;
    stImage.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    // stImage.image = vImageData;
    // stImage.mimeType = "image/jpeg";
    stImage.mimeType = "image/webp";
    stImage.bufferView = nBufferViewIndex;

    stModel.images.push_back(stImage);

    // texture
    tinygltf::Texture stTexture;
    stTexture.sampler = 0;
    stTexture.source = stModel.images.size() - 1;

    stModel.textures.push_back(stTexture);

    // material
    tinygltf::Material stMaterial;
    stMaterial.name = "Material_" + std::to_string(stTexture.source);
    stMaterial.pbrMetallicRoughness.baseColorTexture.index = stTexture.source;
    stMaterial.pbrMetallicRoughness.baseColorTexture.texCoord = 0;
    stMaterial.pbrMetallicRoughness.metallicFactor = 0.0;
    stMaterial.pbrMetallicRoughness.roughnessFactor = 1.0;

    stModel.materials.push_back(stMaterial);

    return true;
}

void addMeshToModel(tinygltf::Model& model, 
                   const std::vector<float>& positions,
                //    const std::vector<float>& normals,
                   const std::vector<float>& texCoords,
                   const std::vector<unsigned int>& indices,
                   int materialIndex,
                   int bufferStart,
                   float vVertexMin[], float vVertexMax[]) {
    // 计算字节偏移量
    size_t positionBytes = positions.size() * sizeof(float);
    // size_t normalBytes = normals.size() * sizeof(float);
    size_t texCoordBytes = texCoords.size() * sizeof(float);
    size_t indicesBytes = indices.size() * sizeof(unsigned int);
    
    // 创建缓冲区视图
    // 位置
    tinygltf::BufferView positionView;
    positionView.buffer = 0;
    positionView.byteOffset = bufferStart;
    positionView.byteLength = positionBytes;
    positionView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    int positionViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(positionView);
    
    // 法线
    // tinygltf::BufferView normalView;
    // normalView.buffer = 0;
    // normalView.byteOffset = bufferStart + positionBytes;
    // normalView.byteLength = normalBytes;
    // normalView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    // int normalViewIndex = model.bufferViews.size();
    // model.bufferViews.push_back(normalView);
    
    // 纹理坐标
    tinygltf::BufferView texCoordView;
    texCoordView.buffer = 0;
    texCoordView.byteOffset = bufferStart + positionBytes ;//+ normalBytes;
    texCoordView.byteLength = texCoordBytes;
    texCoordView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    int texCoordViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(texCoordView);
    
    // 索引
    tinygltf::BufferView indicesView;
    indicesView.buffer = 0;
    indicesView.byteOffset = bufferStart + positionBytes + texCoordBytes ; //+ normalBytes;
    indicesView.byteLength = indicesBytes;
    indicesView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    int indicesViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(indicesView);
    
    // 创建访问器
    tinygltf::Accessor positionAccessor;
    positionAccessor.bufferView = positionViewIndex;
    positionAccessor.byteOffset = 0;
    positionAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    positionAccessor.count = positions.size() / 3;
    positionAccessor.type = TINYGLTF_TYPE_VEC3;
    positionAccessor.minValues.push_back(vVertexMin[0]);
    positionAccessor.minValues.push_back(vVertexMin[1]);
    positionAccessor.minValues.push_back(vVertexMin[2]);
    positionAccessor.maxValues.push_back(vVertexMax[0]);
    positionAccessor.maxValues.push_back(vVertexMax[1]);
    positionAccessor.maxValues.push_back(vVertexMax[2]);
    int positionAccessorIndex = model.accessors.size();
    model.accessors.push_back(positionAccessor);
    
    // 法线
    // tinygltf::Accessor normalAccessor;
    // normalAccessor.bufferView = normalViewIndex;
    // normalAccessor.byteOffset = 0;
    // normalAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    // normalAccessor.count = normals.size() / 3;
    // normalAccessor.type = TINYGLTF_TYPE_VEC3;
    // int normalAccessorIndex = model.accessors.size();
    // model.accessors.push_back(normalAccessor);
    
    // 纹理坐标
    tinygltf::Accessor texCoordAccessor;
    texCoordAccessor.bufferView = texCoordViewIndex;
    texCoordAccessor.byteOffset = 0;
    texCoordAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    texCoordAccessor.count = texCoords.size() / 2;
    texCoordAccessor.type = TINYGLTF_TYPE_VEC2;
    int texCoordAccessorIndex = model.accessors.size();
    model.accessors.push_back(texCoordAccessor);
    
    // 索引
    tinygltf::Accessor indicesAccessor;
    indicesAccessor.bufferView = indicesViewIndex;
    indicesAccessor.byteOffset = 0;
    indicesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    indicesAccessor.count = indices.size();
    indicesAccessor.type = TINYGLTF_TYPE_SCALAR;
    int indicesAccessorIndex = model.accessors.size();
    model.accessors.push_back(indicesAccessor);
    
    // 创建 primitive
    tinygltf::Primitive primitive;
    primitive.attributes["POSITION"] = positionAccessorIndex;
    // primitive.attributes["NORMAL"] = normalAccessorIndex;
    primitive.attributes["TEXCOORD_0"] = texCoordAccessorIndex;
    primitive.indices = indicesAccessorIndex;
    primitive.material = materialIndex;
    primitive.mode = TINYGLTF_MODE_TRIANGLES;
    
    // 创建网格
    tinygltf::Mesh mesh;
    mesh.primitives.push_back(primitive);
    model.meshes.push_back(mesh);
}

bool Ply2Glb::ply2glb(const char* pFileIn, const char* pFileOut) {
    bool bRes = stVertexData.readPlyFile(pFileIn);

   // If the result is true means the ply file is successfully read
   if(bRes) {
        tinygltf::Model stModel;
        stModel.scenes.resize(1);

        tinygltf::Sampler stSampler;
        stSampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
        stSampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
        stSampler.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
        stSampler.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
        stModel.samplers.push_back(stSampler);

        std::vector<unsigned char> bufferData;

        if (stVertexData.vTexcoords.size() != stVertexData.vTextureFiles.size()) {
            return false;
        }

        // Create geometry node
        std::vector<size_t> vBufferStartIndex;
        for (int i = 0; i < stVertexData.vTexcoords.size(); ++i ) {
            vBufferStartIndex.push_back(bufferData.size());
            bufferData.insert(bufferData.end(), reinterpret_cast<const unsigned char*>(stVertexData.vVertices.data()),
                     reinterpret_cast<const unsigned char*>(stVertexData.vVertices.data() + stVertexData.vVertices.size()));
            // bufferData.insert(bufferData.end(),
            //          reinterpret_cast<const unsigned char*>(vNormals.data()),
            //          reinterpret_cast<const unsigned char*>(vNormals.data() + vNormals.size()));
            bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(stVertexData.vTexcoords[i].data()),
                     reinterpret_cast<const unsigned char*>(stVertexData.vTexcoords[i].data() + stVertexData.vTexcoords[i].size()));
            bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(stVertexData.vTriangles[i].data()),
                     reinterpret_cast<const unsigned char*>(stVertexData.vTriangles[i].data() + stVertexData.vTriangles[i].size()));

        }
        tinygltf::Buffer stBuffer;
        stBuffer.data = bufferData;
        stModel.buffers.push_back(stBuffer);

        for (auto sFile : stVertexData.vTextureFiles) {
            addTexture2Model(sFile, stModel);
        }

        for (int i = 0; i < stVertexData.vTexcoords.size(); ++i) {
            addMeshToModel(stModel, stVertexData.vVertices, stVertexData.vTexcoords[i], stVertexData.vTriangles[i], i, vBufferStartIndex[i], stVertexData.vVertexMin, stVertexData.vVertexMax);
        }

        for (size_t i = 0; i < stModel.meshes.size(); ++i) {
            tinygltf::Node node;
            node.mesh = i;
            stModel.nodes.push_back(node);

            stModel.scenes[0].nodes.push_back(i);
        }

        tinygltf::TinyGLTF stLoader;
        bool bSuc = stLoader.WriteGltfSceneToFile(&stModel, pFileOut, true, true, true, true);
        return bSuc;
    }

    return false;
}

    } // namespace ply
} // namespace HG