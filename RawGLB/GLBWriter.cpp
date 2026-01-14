#include "GLBWriter.h"

#include <vector>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
// #include "stb_image.h"
// #include "stb_image_write.h"
#include "tiny_gltf.h"

GLBWriter::GLBWriter() {
    m_stModel.scenes.resize(1);

    tinygltf::Sampler stSampler;
    stSampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
    stSampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
    stSampler.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
    stSampler.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;

    m_stModel.samplers.push_back(stSampler);
}

void GLBWriter::writeModel2File(const std::string &sGLFName) {


// 为每个材质创建独立的顶点数据
// 材质0的顶点数据 (前面、上面、右面)
const std::vector<float> cubePositions0 = {
    // 前面
    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    // 上面
    -0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f, -0.5f,
    // 右面
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f, -0.5f,  0.5f
};

const std::vector<float> cubeNormals0 = {
    // 前面
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    // 上面
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    // 右面
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f
};

const std::vector<float> cubeTexCoords0 = {
    // 前面
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
    // 上面
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
    // 右面
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f
};

const std::vector<unsigned short> indices0 = {
    0, 1, 2, 2, 3, 0,    // 前面
    4, 5, 6, 6, 7, 4,    // 上面
    8, 9, 10, 10, 11, 8  // 右面
};

// 材质1的顶点数据 (后面、下面、左面)
const std::vector<float> cubePositions1 = {
    // 后面
    -0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
    // 下面
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,
    // 左面
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f
};

const std::vector<float> cubeNormals1 = {
    // 后面
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f,
    // 下面
    0.0f, -1.0f, 0.0f,
    0.0f, -1.0f, 0.0f,
    0.0f, -1.0f, 0.0f,
    0.0f, -1.0f, 0.0f,
    // 左面
    -1.0f, 0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f
};

const std::vector<float> cubeTexCoords1 = {
    // 后面
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
    // 下面
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
    // 左面
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f
};

const std::vector<unsigned short> indices1 = {
    0, 1, 2, 2, 3, 0,    // 后面
    4, 5, 6, 6, 7, 4,    // 下面
    8, 9, 10, 10, 11, 8  // 左面
};


     // 创建缓冲区数据
    std::vector<unsigned char> bufferData;
    
    // 添加第一个网格的数据 (材质0)
    size_t bufferStart0 = bufferData.size();
    bufferData.insert(bufferData.end(), 
                     reinterpret_cast<const unsigned char*>(cubePositions0.data()),
                     reinterpret_cast<const unsigned char*>(cubePositions0.data() + cubePositions0.size()));
    bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(cubeNormals0.data()),
                     reinterpret_cast<const unsigned char*>(cubeNormals0.data() + cubeNormals0.size()));
    bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(cubeTexCoords0.data()),
                     reinterpret_cast<const unsigned char*>(cubeTexCoords0.data() + cubeTexCoords0.size()));
    bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(indices0.data()),
                     reinterpret_cast<const unsigned char*>(indices0.data() + indices0.size()));
    
    // 添加第二个网格的数据 (材质1)
    size_t bufferStart1 = bufferData.size();
    bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(cubePositions1.data()),
                     reinterpret_cast<const unsigned char*>(cubePositions1.data() + cubePositions1.size()));
    bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(cubeNormals1.data()),
                     reinterpret_cast<const unsigned char*>(cubeNormals1.data() + cubeNormals1.size()));
    bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(cubeTexCoords1.data()),
                     reinterpret_cast<const unsigned char*>(cubeTexCoords1.data() + cubeTexCoords1.size()));
    bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(indices1.data()),
                     reinterpret_cast<const unsigned char*>(indices1.data() + indices1.size()));
    

    // 创建缓冲区
    tinygltf::Buffer buffer;
    buffer.data = bufferData;
    m_stModel.buffers.push_back(buffer);

    addTexture2Model("../data/texture0.png");
    addTexture2Model("../data/texture1.png");

    addMeshToModel(m_stModel, cubePositions0, cubeNormals0, cubeTexCoords0, indices0, 0, bufferStart0);
    addMeshToModel(m_stModel, cubePositions1, cubeNormals1, cubeTexCoords1, indices1, 1, bufferStart1);

    for (size_t i = 0; i < m_stModel.meshes.size(); ++i) {
        tinygltf::Node node;
        node.mesh = i;
        m_stModel.nodes.push_back(node);

        m_stModel.scenes[0].nodes.push_back(i);
    }

    write("textured_cube.glb");
}

bool GLBWriter::loadImageData(const std::string& filename, std::vector<unsigned char>& imageData, 
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

// add data to model.buffer, return BufferView and Accessor index
bool GLBWriter::AddBufferAndAccessor(tinygltf::Model& model, const void* data, 
                        size_t byteLength, int target, int componentType, int type, int count, 
                        std::vector<double>& minValues, std::vector<double>& maxValues) {
    if (model.buffers.empty()) {
        tinygltf::Buffer buffer;
        model.buffers.push_back(buffer);
    }
    
    tinygltf::Buffer& buffer = model.buffers[0];
    size_t startOffset = buffer.data.size();
    
    // add data to buffer
    const unsigned char* byteData = static_cast<const unsigned char*>(data);
    buffer.data.insert(buffer.data.end(), byteData, byteData + byteLength);
    
    // create BufferView
    tinygltf::BufferView bufferView;
    bufferView.buffer = 0; // first buffer
    bufferView.byteOffset = startOffset;
    bufferView.byteLength = byteLength;
    bufferView.target = target; // TARGET_ARRAY_BUFFER 或 TARGET_ELEMENT_ARRAY_BUFFER
    
    int bufferViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(bufferView);
    
    // create Accessor
    tinygltf::Accessor accessor;
    accessor.bufferView = bufferViewIndex;
    accessor.byteOffset = 0;
    accessor.componentType = componentType; // COMPONENT_TYPE_FLOAT, COMPONENT_TYPE_UNSIGNED_SHORT 等
    accessor.count = count;
    accessor.type = type; // TYPE_VEC3, TYPE_SCALAR 等
    
    // bounding
    accessor.minValues = minValues;
    accessor.maxValues = maxValues;
    
    int accessorIndex = model.accessors.size();
    model.accessors.push_back(accessor);
    
    return true;
}

void GLBWriter::addMeshToModel(tinygltf::Model& model, 
                   const std::vector<float>& positions,
                   const std::vector<float>& normals,
                   const std::vector<float>& texCoords,
                   const std::vector<unsigned short>& indices,
                   int materialIndex,
                   int bufferStart) {
    // 计算字节偏移量
    size_t positionBytes = positions.size() * sizeof(float);
    size_t normalBytes = normals.size() * sizeof(float);
    size_t texCoordBytes = texCoords.size() * sizeof(float);
    size_t indicesBytes = indices.size() * sizeof(unsigned short);
    
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
    tinygltf::BufferView normalView;
    normalView.buffer = 0;
    normalView.byteOffset = bufferStart + positionBytes;
    normalView.byteLength = normalBytes;
    normalView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    int normalViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(normalView);
    
    // 纹理坐标
    tinygltf::BufferView texCoordView;
    texCoordView.buffer = 0;
    texCoordView.byteOffset = bufferStart + positionBytes + normalBytes;
    texCoordView.byteLength = texCoordBytes;
    texCoordView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    int texCoordViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(texCoordView);
    
    // 索引
    tinygltf::BufferView indicesView;
    indicesView.buffer = 0;
    indicesView.byteOffset = bufferStart + positionBytes + normalBytes + texCoordBytes;
    indicesView.byteLength = indicesBytes;
    indicesView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    int indicesViewIndex = model.bufferViews.size();
    model.bufferViews.push_back(indicesView);
    
    // 创建访问器
    // 位置
    tinygltf::Accessor positionAccessor;
    positionAccessor.bufferView = positionViewIndex;
    positionAccessor.byteOffset = 0;
    positionAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    positionAccessor.count = positions.size() / 3;
    positionAccessor.type = TINYGLTF_TYPE_VEC3;
    positionAccessor.minValues.push_back(-0.5f);
    positionAccessor.minValues.push_back(-0.5f);
    positionAccessor.minValues.push_back(-0.5f);
    positionAccessor.maxValues.push_back(0.5f);
    positionAccessor.maxValues.push_back(0.5f);
    positionAccessor.maxValues.push_back(0.5f);
    int positionAccessorIndex = model.accessors.size();
    model.accessors.push_back(positionAccessor);
    
    // 法线
    tinygltf::Accessor normalAccessor;
    normalAccessor.bufferView = normalViewIndex;
    normalAccessor.byteOffset = 0;
    normalAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    normalAccessor.count = normals.size() / 3;
    normalAccessor.type = TINYGLTF_TYPE_VEC3;
    int normalAccessorIndex = model.accessors.size();
    model.accessors.push_back(normalAccessor);
    
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
    indicesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
    indicesAccessor.count = indices.size();
    indicesAccessor.type = TINYGLTF_TYPE_SCALAR;
    int indicesAccessorIndex = model.accessors.size();
    model.accessors.push_back(indicesAccessor);
    
    // 创建 primitive
    tinygltf::Primitive primitive;
    primitive.attributes["POSITION"] = positionAccessorIndex;
    primitive.attributes["NORMAL"] = normalAccessorIndex;
    primitive.attributes["TEXCOORD_0"] = texCoordAccessorIndex;
    primitive.indices = indicesAccessorIndex;
    primitive.material = materialIndex;
    primitive.mode = TINYGLTF_MODE_TRIANGLES;
    
    // 创建网格
    tinygltf::Mesh mesh;
    mesh.primitives.push_back(primitive);
    model.meshes.push_back(mesh);
}

bool GLBWriter::addTexture2Model(const std::string& sTextureFile) {
    // image
    std::vector<unsigned char> vImageData;
    int nWidth = 0, nHeight = 0, nChannel = 0;
    if (!loadImageData(sTextureFile, vImageData, nWidth, nHeight, nChannel)) {
        return false;
    }
    tinygltf::Image stImage;
    stImage.width = nWidth;
    stImage.height = nHeight;
    stImage.component = nChannel;
    stImage.bits = 8;
    stImage.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    stImage.image = vImageData;
    stImage.mimeType = "image/png";

    m_stModel.images.push_back(stImage);

    // texture
    tinygltf::Texture stTexture;
    stTexture.sampler = 0;
    stTexture.source = m_stModel.images.size() - 1;

    m_stModel.textures.push_back(stTexture);

    // material
    tinygltf::Material stMaterial;
    stMaterial.name = "Material_" + std::to_string(stTexture.source);
    stMaterial.pbrMetallicRoughness.baseColorTexture.index = stTexture.source;
    stMaterial.pbrMetallicRoughness.baseColorTexture.texCoord = 0;
    stMaterial.pbrMetallicRoughness.metallicFactor = 0.0;
    stMaterial.pbrMetallicRoughness.roughnessFactor = 1.0;

    m_stModel.materials.push_back(stMaterial);

    return true;
}

bool GLBWriter::write(const std::string& sGLBFileName) {
    bool bSuc = m_stLoader.WriteGltfSceneToFile(&m_stModel, sGLBFileName, true, true, true, true);
    if (bSuc) {
        std::cout << "Write glb file success: " << sGLBFileName << std::endl;
    } else {
        std::cout << "Write failed! " << std::endl;
    }
    return bSuc;
}
