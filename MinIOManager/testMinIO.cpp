#include "MinIO.h"

struct StConfigs {
	std::string sEndpoint = "play.min.io";
    std::string sAccessKey = "minio";
    std::string sSecretKey = "minio";

    std::string sId = "";

    std::string sBucketInput = "";
    std::string sObjectInput = "";

    std::string sBucketOutput = "";
    std::string sObjectOutput = "";

    std::string sDataPath = "./images";
    std::string sWorkPath = "./results";
};

int downloadImages(StConfigs &stConfigs, bool bHttps) {
    MinIOManager stManager(stConfigs.sEndpoint, bHttps, stConfigs.sAccessKey, stConfigs.sSecretKey);
    std::cout << "Download " << stConfigs.sEndpoint << " " << stConfigs.sBucketInput << " " << stConfigs.sObjectInput << std::endl;
    int res = stManager.downloadDirectoryInThread(stConfigs.sBucketInput, stConfigs.sObjectInput, stConfigs.sDataPath);
    return res;
}

int uploadResults(StConfigs &stConfigs, bool bHttps) {
    MinIOManager stManager(stConfigs.sEndpoint, bHttps, stConfigs.sAccessKey, stConfigs.sSecretKey);
    int res = stManager.uploadDirectoryInThread(stConfigs.sBucketOutput, stConfigs.sId, stConfigs.sWorkPath, true);
    return res;
}

int main() {
    StConfigs stConfigs;
    stConfigs.sId = "id1";
    stConfigs.sBucketInput = "bucket_name";
    stConfigs.sObjectInput = "object_name";
    stConfigs.sBucketOutput = stConfigs.sBucketInput;
    stConfigs.sObjectOutput = stConfigs.sId;

    downloadImages(stConfigs, true);

    uploadResults(stConfigs, true);

    return 0;
}
