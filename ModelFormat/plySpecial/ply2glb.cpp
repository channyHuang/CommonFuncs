#include "ply2glb.h"

#include <cstring>
#include <filesystem>

#include "typedefs.h"
#include "ply.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "../tinygltf/tiny_gltf.h"

namespace HG {
    namespace ply {

Ply2Glb::Ply2Glb() : _invertFaces(false) {}

void Ply2Glb::readVertices( PlyFile* file, const int nVertices,
                               const int fields )
{
    // temporary vertex structure for ply loading
    struct _Vertex
    {
        float           x;
        float           y;
        float           z;
        float           nx;
        float           ny;
        float           nz;
        unsigned char   red;
        unsigned char   green;
        unsigned char   blue;
        unsigned char   alpha;
        unsigned char   ambient_red;
        unsigned char   ambient_green;
        unsigned char   ambient_blue;
        unsigned char   diffuse_red;
        unsigned char   diffuse_green;
        unsigned char   diffuse_blue;
        unsigned char   specular_red;
        unsigned char   specular_green;
        unsigned char   specular_blue;
        float           specular_coeff;
        float           specular_power;
        float texture_u;
        float texture_v;
    } vertex;

    PlyProperty vertexProps[] =
    {
        { "x", PLY_FLOAT, PLY_FLOAT, offsetof( _Vertex, x ), 0, 0, 0, 0 },
        { "y", PLY_FLOAT, PLY_FLOAT, offsetof( _Vertex, y ), 0, 0, 0, 0 },
        { "z", PLY_FLOAT, PLY_FLOAT, offsetof( _Vertex, z ), 0, 0, 0, 0 },
        { "nx", PLY_FLOAT, PLY_FLOAT, offsetof( _Vertex, nx ), 0, 0, 0, 0 },
        { "ny", PLY_FLOAT, PLY_FLOAT, offsetof(_Vertex, ny), 0, 0, 0, 0 },
        { "nz", PLY_FLOAT, PLY_FLOAT, offsetof(_Vertex, nz), 0, 0, 0, 0 },
        { "red", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, red ), 0, 0, 0, 0 },
        { "green", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, green ), 0, 0, 0, 0 },
        { "blue", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, blue ), 0, 0, 0, 0 },
        { "alpha", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, alpha ), 0, 0, 0, 0 },
        { "ambient_red", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, ambient_red ), 0, 0, 0, 0 },
        { "ambient_green", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, ambient_green ), 0, 0, 0, 0 },
        { "ambient_blue", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, ambient_blue ), 0, 0, 0, 0 },
        { "diffuse_red", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, diffuse_red ), 0, 0, 0, 0 },
        { "diffuse_green", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, diffuse_green ), 0, 0, 0, 0 },
        { "diffuse_blue", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, diffuse_blue ), 0, 0, 0, 0 },
        { "specular_red", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, specular_red ), 0, 0, 0, 0 },
        { "specular_green", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, specular_green ), 0, 0, 0, 0 },
        { "specular_blue", PLY_UCHAR, PLY_UCHAR, offsetof( _Vertex, specular_blue ), 0, 0, 0, 0 },
        { "specular_coeff", PLY_FLOAT, PLY_FLOAT, offsetof( _Vertex, specular_coeff ), 0, 0, 0, 0 },
        { "specular_power", PLY_FLOAT, PLY_FLOAT, offsetof( _Vertex, specular_power ), 0, 0, 0, 0 },
        { "texture_u", PLY_FLOAT, PLY_FLOAT, offsetof(_Vertex, texture_u), 0, 0, 0, 0 },
        { "texture_v", PLY_FLOAT, PLY_FLOAT, offsetof(_Vertex, texture_v), 0, 0, 0, 0 },
    };

    // use all 6 properties when reading colors, only the first 3 otherwise
    for( int i = 0; i < 3; ++i )
        ply_get_property( file, "vertex", &vertexProps[i] );

    if (fields & NORMALS)
      for( int i = 3; i < 6; ++i )
        ply_get_property( file, "vertex", &vertexProps[i] );

    if (fields & RGB)
      for( int i = 6; i < 9; ++i )
        ply_get_property( file, "vertex", &vertexProps[i] );

    if (fields & RGBA)
        ply_get_property( file, "vertex", &vertexProps[9] );

    if (fields & AMBIENT)
      for( int i = 10; i < 13; ++i )
        ply_get_property( file, "vertex", &vertexProps[i] );

    if (fields & DIFFUSE)
      for( int i = 13; i < 16; ++i )
        ply_get_property( file, "vertex", &vertexProps[i] );

    if (fields & SPECULAR)
      for( int i = 16; i < 21; ++i )
        ply_get_property( file, "vertex", &vertexProps[i] );

    if (fields & TEXCOORD)
        for (int i = 21; i < 23; ++i)
            ply_get_property(file, "vertex", &vertexProps[i]);

    for (size_t i = 0; i < 3; ++i) {
        vVertexMax[i] = std::numeric_limits<float>::lowest();
        vVertexMin[i] = std::numeric_limits<float>::max();
    }
    // read in the vertices
    for( int i = 0; i < nVertices; ++i )
    {
        ply_get_element( file, static_cast< void* >( &vertex ) );
        vVertices.push_back(vertex.x);
        vVertices.push_back(vertex.y);
        vVertices.push_back(vertex.z);

        if (vVertexMax[0] < vertex.x) vVertexMax[0] = vertex.x;
        if (vVertexMax[1] < vertex.y) vVertexMax[1] = vertex.y;
        if (vVertexMax[2] < vertex.z) vVertexMax[2] = vertex.z;
        if (vVertexMin[0] > vertex.x) vVertexMin[0] = vertex.x;
        if (vVertexMin[1] > vertex.y) vVertexMin[1] = vertex.y;
        if (vVertexMin[2] > vertex.z) vVertexMin[2] = vertex.z;

        if (fields & NORMALS) {
            vNormals.push_back(vertex.nx);
            vNormals.push_back(vertex.ny);
            vNormals.push_back(vertex.nz);
        }

        if( fields & RGBA ) {
            vColors.push_back( (unsigned int) vertex.red / 255.0);
            vColors.push_back( (unsigned int) vertex.green / 255.0);
            vColors.push_back( (unsigned int) vertex.blue / 255.0);
            vColors.push_back( (unsigned int) vertex.alpha / 255.0);
        }
        else if( fields & RGB ) {
            vColors.push_back( (unsigned int) vertex.red / 255.0);
            vColors.push_back( (unsigned int) vertex.green / 255.0);
            vColors.push_back( (unsigned int) vertex.blue / 255.0);
            vColors.push_back( 1.0 );
        }
        if( fields & AMBIENT ) {
            vAmbient.push_back( (unsigned int) vertex.ambient_red / 255.0 );
            vAmbient.push_back( (unsigned int) vertex.ambient_green / 255.0 );
            vAmbient.push_back( (unsigned int) vertex.ambient_blue / 255.0);
            vAmbient.push_back( 1.0 );
        }

        if( fields & DIFFUSE ) {
            vDiffuse.push_back( (unsigned int) vertex.diffuse_red / 255.0);
            vDiffuse.push_back( (unsigned int) vertex.diffuse_green / 255.0 );
            vDiffuse.push_back( (unsigned int) vertex.diffuse_blue / 255.0);
            vDiffuse.push_back( 1.0 );
        }

        if( fields & SPECULAR ) {
            vSpecular.push_back( (unsigned int) vertex.specular_red / 255.0);
            vSpecular.push_back( (unsigned int) vertex.specular_green / 255.0 );
            vSpecular.push_back( (unsigned int) vertex.specular_blue / 255.0);
            vSpecular.push_back( 1.0 );
        }
        if (fields & TEXCOORD) {
            vTexcoord.push_back(vertex.texture_u);
            vTexcoord.push_back(vertex.texture_v);
        }
        else {
            vTexcoord.push_back(-1.f);
            vTexcoord.push_back(-1.f);
        }
    }
}

void Ply2Glb::readTriangles( PlyFile* file, const int nFaces )
{
    // temporary face structure for ply loading
    struct _Face
    {
        unsigned int    nVertices;
        unsigned int*   vertices;
        unsigned int   nTexcoords;
        float*          texcoords;
        int             nTexIndex;
    } face;

    PlyProperty faceProps[] =
    {
        { "vertex_indices|vertex_index", PLY_UINT32, PLY_UINT, offsetof( _Face, vertices ),
          1, PLY_UINT8, PLY_UINT, offsetof( _Face, nVertices ) }, 
          { "texcoord", PLY_FLOAT32, PLY_FLOAT, offsetof( _Face, texcoords ),
          1, PLY_UINT8, PLY_UINT, offsetof( _Face, nTexcoords ) },
          { "texnumber", PLY_INT32, PLY_INT32, offsetof(_Face, nTexIndex), 0, 0, 0, 0}
    };

    ply_get_property( file, "face", &faceProps[0] );
    ply_get_property( file, "face", &faceProps[1] );
    ply_get_property( file, "face", &faceProps[2] );


    const char NUM_VERTICES_TRIANGLE(3);
    const char NUM_VERTICES_QUAD(4);

    // read the faces, reversing the reading direction if _invertFaces is true
    for( int i = 0 ; i < nFaces; i++ )
    {
        // initialize face values
        face.nVertices = 0;
        face.vertices = 0;
        face.nTexcoords = 0;
        face.texcoords = 0;
        face.nTexIndex = 0;

        ply_get_element( file, static_cast< void* >( &face ) );
        if (face.vertices)
        {
            if (face.texcoords) {
                while (face.nTexIndex >= vTexcoords.size()) {
                    vTexcoords.push_back(std::vector<float>(vVertices.size() / 3 * 2, 0.f));
                    vTriangles.push_back(std::vector<unsigned int>());
                    vTexFlags.push_back(std::vector<int>(vVertices.size() / 3, 0));
                }

                for(int j = 0 ; j < face.nVertices ; j++)
                {
                    unsigned int vindex = face.vertices[j];
                    if ((vTexFlags[face.nTexIndex][vindex] & 1) == 0) {
                        vTexcoords[face.nTexIndex].at(vindex << 1) = face.texcoords[j << 1];
                        vTexcoords[face.nTexIndex].at((vindex << 1) + 1) = 1.f - face.texcoords[(j << 1) + 1];
                        vTexFlags[face.nTexIndex][vindex] = 1;
                    // } else {
                    } else if ( (fabs(vTexcoords[face.nTexIndex].at(vindex << 1) - face.texcoords[j << 1]) > 1e-4) 
                        && (fabs(vTexcoords[face.nTexIndex].at((vindex << 1) + 1) - 1.f + face.texcoords[(j << 1) + 1]) > 1e-4) ) {
                        face.vertices[j] = vVertices.size() / 3;
                        vVertices.push_back(vVertices.at(vindex * 3));
                        vVertices.push_back(vVertices.at(vindex * 3 + 1));
                        vVertices.push_back(vVertices.at(vindex * 3 + 2));

                        for (int x = 0; x < vTexcoords.size(); x++) {
                            vTexcoords[x].push_back(0.f);
                            vTexcoords[x].push_back(1.f);

                            vTexFlags[x].push_back(0);
                        }

                        vTexcoords[face.nTexIndex].at(face.vertices[j] << 1) = face.texcoords[j << 1];
                        vTexcoords[face.nTexIndex].at((face.vertices[j] << 1) + 1) = 1.f - face.texcoords[(j << 1) + 1];
                        vTexFlags[face.nTexIndex][face.vertices[j]] = 1;
                    }

                }
            }
            
            if (face.nVertices == NUM_VERTICES_TRIANGLE ||  face.nVertices == NUM_VERTICES_QUAD)
            {
                unsigned int index;
                for(int j = 0 ; j < face.nVertices ; j++)
                {
                    index = ( _invertFaces ? face.nVertices - 1 - j : j );
                    if(face.nVertices == 4)
                        vQuad.push_back(face.vertices[index]);
                    else
                        vTriangles[face.nTexIndex].push_back(face.vertices[index] );
                }
            }

            // free the memory that was allocated by ply_get_element
            free( face.vertices );
        }
    }
}

// simple_texture_encoder.cpp
#include <vector>
#include <iostream>
#include <cstdio>
#include <cstring>

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

    std::vector<unsigned char> vEncodedImageData = encodeToJPG_via_file(vImageData, nWidth, nHeight, nChannel);
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
    stImage.mimeType = "image/jpeg";
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
    int     nPlyElems;
    char**  elemNames;
    int     fileType;
    float   version;
    bool    result = false;
    int     nComments;
    char**  comments;

    PlyFile* file = NULL;

    // Try to open ply file as for reading
    try{
            file  = ply_open_for_reading( const_cast< char* >( pFileIn ),
                                          &nPlyElems, &elemNames,
                                          &fileType, &version );
    }
    // Catch the if any exception thrown
    catch( std::exception& e )
    {
        // MESHERROR << "Unable to read PLY file, an exception occurred:  "
        //             << e.what() << std::endl;
    }

    if( !file )
    {
        std::cerr << "Unable to open PLY file " << pFileIn
                  << " for reading." << std::endl;
        return false;
    }

    MESHASSERT( elemNames != 0 );


    nComments = file->num_comments;
    comments = file->comments;


    // #ifndef NDEBUG
    // MESHINFO << pFileIn << ": " << nPlyElems << " elements, file type = "
    //          << fileType << ", version = " << version << std::endl;
    // #endif

    std::vector<std::string> vTextureFiles;
    for( int i = 0; i < nComments; i++ )
    {
        if( equal_strings( comments[i], "modified by flipply" ) )
        {
            _invertFaces = true;
        }
        if (std::strncmp(comments[i], "TextureFile",11)==0)
        {
            std::string textureFile = comments[i]+12;
            if (!std::filesystem::path(textureFile).is_absolute())
            {
                textureFile = std::filesystem::path(pFileIn).parent_path() / textureFile;
            }
            vTextureFiles.push_back(textureFile);
        }
    }
    for( int i = 0; i < nPlyElems; ++i )
    {
        int nElems;
        int nProps;

        PlyProperty** props = NULL;
        try{
                props = ply_get_element_description( file, elemNames[i],
                                                     &nElems, &nProps );
        }
        catch( std::exception& e )
        {
            // MESHERROR << "Unable to get PLY file description, an exception occurred:  "
            //             << e.what() << std::endl;
        }
        // MESHASSERT( props != 0 );

        // #ifndef NDEBUG
        // MESHINFO << "element " << i << ": name = " << elemNames[i] << ", "
        //          << nProps << " properties, " << nElems << " elements" << std::endl;
        // for( int j = 0; j < nProps; ++j )
        // {
        //     MESHINFO << "element " << i << ", property " << j << ": "
        //              << "name = " << props[j]->name << std::endl;
        // }
        // #endif

        // if the string is vertex means vertex data is started
        if( equal_strings( elemNames[i], "vertex" ) )
        {
            int fields = NONE;
            // determine if the file stores vertex colors
            for( int j = 0; j < nProps; ++j )
            {
                // if the string have the red means color info is there
                if( equal_strings( props[j]->name, "x" ) )
                    fields |= XYZ;
                if( equal_strings( props[j]->name, "nx" ) )
                    fields |= NORMALS;
                if( equal_strings( props[j]->name, "alpha" ) )
                    fields |= RGBA;
                if ( equal_strings( props[j]->name, "red" ) )
                    fields |= RGB;
                if( equal_strings( props[j]->name, "ambient" ) )
                    fields |= AMBIENT;
                if( equal_strings( props[j]->name, "diffuse_red" ) )
                    fields |= DIFFUSE;
                if (equal_strings(props[j]->name, "specular_red"))
                    fields |= SPECULAR;
                if (equal_strings(props[j]->name, "texture_u"))
                    fields |= TEXCOORD;
                if (equal_strings(props[j]->name, "texture_v"))
                    fields |= TEXCOORD;
                else 
                    fields |= TEXCOORD;
            }

            if( false )
            {
                fields &= ~(XYZ | NORMALS);
                    // MESHINFO << "Colors in PLY file ignored per request." << std::endl;
            }

            try {
                // Read vertices and store in a std::vector array
                readVertices( file, nElems, fields );
                // Check whether all vertices are loaded or not
                MESHASSERT( (vVertices.size() / 3) == static_cast< size_t >( nElems ) );

                // Check if all the optional elements were read or not
                if( fields & NORMALS )
                {
                    MESHASSERT( (vNormals.size() / 3) == static_cast< size_t >( nElems ) );
                }
                if( fields & RGB || fields & RGBA)
                {
                    MESHASSERT( (vColors.size() / 4) == static_cast< size_t >( nElems ) );
                }
                if( fields & AMBIENT )
                {
                    MESHASSERT( (vAmbient.size() / 3) == static_cast< size_t >( nElems ) );
                }
                if( fields & DIFFUSE )
                {
                    MESHASSERT( (vDiffuse.size() / 3) == static_cast< size_t >( nElems ) );
                }
                if (fields & SPECULAR)
                {
                    MESHASSERT( (vSpecular.size() / 3) == static_cast< size_t >(nElems));
                }
                if (fields & TEXCOORD)
                {
                    MESHASSERT( (vTexcoord.size() / 2) == static_cast< size_t >(nElems));
                }

                result = true;
            }
            catch( std::exception& e )
            {
                // MESHERROR << "Unable to read vertex in PLY file, an exception occurred:  "
                            // << e.what() << std::endl;
                // stop for loop by setting the loop variable to break condition
                // this way resources still get released even on error cases
                i = nPlyElems;

            }
        }
        // If the string is face means triangle info started
        else if( equal_strings( elemNames[i], "face" ) )
        try
        {
            // Read Triangles
            readTriangles( file, nElems );
            // Check whether all face elements read or not
// #if DEBUG
//             unsigned int nbTriangles = (_triangles.valid() ? _triangles->size() / 3 : 0) ;
//             unsigned int nbQuads = (_quads.valid() ? _quads->size() / 4 : 0 );

//             MESHASSERT( (nbTriangles + nbQuads) == static_cast< size_t >( nElems ) );
// #endif
            result = true;
        }
        catch( std::exception& e )
        {
            // MESHERROR << "Unable to read PLY file, an exception occurred:  "
                    //   << e.what() << std::endl;
            // stop for loop by setting the loop variable to break condition
            // this way resources still get released even on error cases
            i = nPlyElems;
        }

        // free the memory that was allocated by ply_get_element_description
        for( int j = 0; j < nProps; ++j )
            free( props[j] );
        free( props );
    }

    ply_close( file );

    // free the memory that was allocated by ply_open_for_reading
    for( int i = 0; i < nPlyElems; ++i )
        free( elemNames[i] );
    free( elemNames );

   // If the result is true means the ply file is successfully read
   if(result)
   {
        tinygltf::Model stModel;
        stModel.scenes.resize(1);

        tinygltf::Sampler stSampler;
        stSampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
        stSampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
        stSampler.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
        stSampler.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
        stModel.samplers.push_back(stSampler);

        std::vector<unsigned char> bufferData;

        // Create geometry node
        std::vector<size_t> vBufferStartIndex;
        for (int i = 0; i < vTexcoords.size(); ++i ) {
            vBufferStartIndex.push_back(bufferData.size());
            bufferData.insert(bufferData.end(), reinterpret_cast<const unsigned char*>(vVertices.data()),
                     reinterpret_cast<const unsigned char*>(vVertices.data() + vVertices.size()));
            bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(vTexcoords[i].data()),
                     reinterpret_cast<const unsigned char*>(vTexcoords[i].data() + vTexcoords[i].size()));
            bufferData.insert(bufferData.end(),
                     reinterpret_cast<const unsigned char*>(vTriangles[i].data()),
                     reinterpret_cast<const unsigned char*>(vTriangles[i].data() + vTriangles[i].size()));

        }
        tinygltf::Buffer stBuffer;
        stBuffer.data = bufferData;
        stModel.buffers.push_back(stBuffer);

        for (auto sFile : vTextureFiles) {
            addTexture2Model(sFile, stModel);
        }

        for (int i = 0; i < vTexcoords.size(); ++i) {
            addMeshToModel(stModel, vVertices, vTexcoords[i], vTriangles[i], i, vBufferStartIndex[i], vVertexMin, vVertexMax);
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