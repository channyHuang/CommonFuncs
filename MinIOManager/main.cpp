#include "MinIOManager.h"

int main() {
    std::string sEndpoint, sAccessKey, sSecretKey;
    std::string sBucketInput, sDataPath;
    std::vector<std::string> vObjects;
    // fill vObjects ...
    MinIOManager stManager(sEndpoint, false, sAccessKey, sSecretKey);
    int res = stManager.downloadJsonListInThread(sBucketInput, vObjects, sDataPath);
    return res;
}