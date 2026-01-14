#include "fbx.h"

#include <assimp/cimport.h>
#include <assimp/cexport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace HG {
    bool glb2fbx(const std::string& sInputFile) {
        const aiScene* pScene = aiImportFile(sInputFile.c_str(), 0);
        if (pScene == nullptr) {
            return false;
        }
        auto nStPos = sInputFile.find_last_of('/');
        if (nStPos == std::string::npos) {
            nStPos = 0;
        }
        auto nEndPos = sInputFile.find_last_of('.');
        if (nEndPos == std::string::npos || nStPos >= nEndPos) {
            return false;
        }
        std::string sName = sInputFile.substr(nStPos + 1, nEndPos - nStPos);
        aiReturn res = aiExportScene(pScene, "fbx", (sName + ".fbx").c_str(), 0);
        return res;
    }
}
