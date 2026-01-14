#include "ReaderWriterPLY.h"

#include <iostream>

#include <osgDB/WriteFile>

#include "vertexData.h"

namespace HG {

osg::Node* readPlySpecial(const std::string& filename) 
{
    ply::VertexData vertexData;
    osg::Node* node = vertexData.readPlyFile(filename.c_str());

    if (node)
        return node;

    return nullptr;
}

bool ply2osgb(const std::string& sPlyFileName, const std::string& sOsgbFileName) {
    osg::Node* pNode = readPlySpecial(sPlyFileName);
    if (pNode == nullptr) return false;
    osgDB::writeNodeFile(*pNode, sOsgbFileName);
    return true;
}

}
