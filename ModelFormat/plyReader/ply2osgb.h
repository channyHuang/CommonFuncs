#pragma once

#include <vector>

#include <osg/Node>
#include <osg/PrimitiveSet>
#include <osgDB/WriteFile>

#include "VertexData.h"

namespace HG {

namespace ply {

class Ply2Osgb {
public:
    Ply2Osgb();
    bool ply2osgb(const char* pFileIn, const char* pFileOut);

private:
    osg::Node* pack2osgb(VertexData &stVertexData);
};

} // end of namespace ply

} // end of namespace HG
