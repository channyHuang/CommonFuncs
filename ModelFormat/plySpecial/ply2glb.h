#ifndef PLY2GLB_H
#define PLY2GLB_H

#include <vector>

namespace HG {
struct PlyFile;

    namespace ply {

class Ply2Glb {
public:
    Ply2Glb();
    bool ply2glb(const char* pFileIn, const char* pFileOut);

    void useInvertedFaces() { _invertFaces = true; }

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

        float vVertexMin[3] = {0};
        float vVertexMax[3] = {0};
        std::vector<float> vVertices;
        std::vector<float> vColors;
        std::vector<float> vAmbient;
        std::vector<float> vDiffuse;
        std::vector<float> vSpecular;
        std::vector<float> vTexcoord;
        std::vector<float> vNormals;
        std::vector<unsigned int> vTriangle;
        std::vector<unsigned int> vQuad;

        std::vector<std::vector<float> > vTexcoords;
        std::vector<std::vector<unsigned int> > vTriangles;
        std::vector<std::vector<int> > vTexFlags; 
};

    } // namespace ply
} // namespace HG

#endif

