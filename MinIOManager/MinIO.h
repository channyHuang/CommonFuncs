#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <queue>
#include <unordered_set>
#include <filesystem>
namespace fs = std::filesystem;

#include <miniocpp/client.h>

class MinIOManager {
public:
    MinIOManager(const std::string& endpoint, 
                 bool bHttps,
                 const std::string& access_key, 
                 const std::string& secret_key);
    ~MinIOManager();

    std::vector<std::string> listObjects(const std::string& sBucket, const std::string& sPrefix = "");
    
    bool downloadObject(const std::string& sBucket, 
                    const std::string& object_name,
                    const std::string& local_path);
    int downloadDirectory(const std::string& sBucket,
                        const std::string& remote_path,
                        const std::string& local_base_dir);
    

    int uploadDirectory(const std::string &sBucket, const std::string &sObject, const std::string &sLocalPath, bool bRecursive = false);
    bool uploadObject(const std::string &sBucket, const std::string &sObject, const std::string &sLocalPathName);

    void addTask(const std::string& sBucket, const std::string &sObject, const std::string &sSaveName, bool bDownload = true);
    void worker(bool bDownload = true);
    void start(int nNumThreads, bool bDownload = true);
    int downloadDirectoryInThread(const std::string& sBucket,
                        const std::string& sObject,
                        const std::string& sLocalPath);
    int uploadDirectoryInThread(const std::string& sBucket,
                        const std::string& sObject,
                        const std::string& sLocalPath,
                        bool bRecursive = false);

public:
    bool bucketExists(const std::string& sBucket);
    bool makeBucket(const std::string& sBucket);

    std::vector<std::string> listFiles(const std::string &sLocalPath, const std::string& sPrefix = "", bool bRecursive = false);

private:
    minio::s3::BaseUrl m_stBaseUrl;
    minio::creds::StaticProvider m_stProvider;
    minio::s3::Client m_stClient;

    std::mutex m_stMutexQueue;
    std::mutex m_stMutexCount;
    std::queue<std::pair<std::pair<std::string, std::string>, std::string> > m_quDownloadTask;
    std::queue<std::pair<std::pair<std::string, std::string>, std::string> > m_quUploadTask;

    std::unordered_set<std::string> m_setImageExtensions = {
        "jpg", "jpeg", "png", "gif", "bmp",
    };
};
