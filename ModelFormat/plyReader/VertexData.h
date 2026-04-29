#pragma once 

#include <vector>
#include <string>

namespace HG {

struct PlyFile;

namespace ply {

class VertexData {
public:
    VertexData();

    bool readPlyFile(const char*pFileIn);

    void useInvertedFaces() { _invertFaces = true; }

public:
    std::vector<float> vVertices;
    std::vector<float> vNormals;
    std::vector<std::vector<float> > vTexcoords;
    std::vector<std::vector<unsigned int> > vTriangles;
    std::vector<std::vector<int> > vTexFlags; 
    std::vector<std::string> vTextureFiles;

    float vVertexMin[3] = {0};
    float vVertexMax[3] = {0};

private:
    enum VertexFields
    {
        NONE = 0,
        XYZ = 1,
        NORMALS = 2,
        RGB = 4,
        AMBIENT = 8,
        DIFFUSE = 16,
        SPECULAR = 32,
        RGBA = 64,
        TEXCOORD = 128
    };

    // Function which reads all the vertices and colors if color info is
    // given and also if the user wants that information
    void readVertices( PlyFile* file, const int nVertices,
                        const int vertexFields);

    // Reads the triangle indices from the ply file
    void readTriangles( PlyFile* file, const int nFaces );

    bool        _invertFaces;

    
    
    std::vector<float> vColors;
    std::vector<float> vAmbient;
    std::vector<float> vDiffuse;
    std::vector<float> vSpecular;
    std::vector<float> vTexcoord;
    
    std::vector<unsigned int> vTriangle;
    std::vector<unsigned int> vQuad;
};

} // end of namespace ply

} // end of namespace HG
