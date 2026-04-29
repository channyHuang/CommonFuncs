#ifndef PLY2GLB_H
#define PLY2GLB_H

#include <vector>

#include "VertexData.h"

namespace HG {
struct PlyFile;

    namespace ply {

class Ply2Glb {
public:
    Ply2Glb();
    bool ply2glb(const char* pFileIn, const char* pFileOut);

private:
    VertexData stVertexData;

};

    } // namespace ply
} // namespace HG

#endif

