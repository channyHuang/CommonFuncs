#pragma once

#include <osg/Node>

namespace HG {
osg::Node* readPlySpecial(const std::string& filename);
bool ply2osgb(const std::string& sPlyFileName, const std::string& sOsgbFileName);
}
