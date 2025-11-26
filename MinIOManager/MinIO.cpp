#include <iostream>
#include <unordered_set>
#include <thread>

#include "MinIO.h"

MinIOManager::MinIOManager(const std::string& endpoint, 
                 bool bHttps,
                 const std::string& access_key, 
                 const std::string& secret_key) : 
                 m_stBaseUrl(endpoint, bHttps), 
                 m_stProvider(access_key, secret_key),
                 m_stClient(m_stBaseUrl, &m_stProvider) {
}

MinIOManager::~MinIOManager() {}

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), 
                    [](unsigned char c) { return std::tolower(c); });
    return str;
}

bool MinIOManager::bucketExists(const std::string& sBucket) {
    try {
        minio::s3::BucketExistsArgs args;
        args.bucket = sBucket;
        minio::s3::BucketExistsResponse response = m_stClient.BucketExists(args);
        if (!response) {
            std::cerr << "Check bucket [" << sBucket  << "] failed: " << response.Error().String() << std::endl;
            return false;
        }
        return response.exist;
    } catch (const std::exception& e) {
        std::cerr << "Check Bucket Exception: " << e.what() << std::endl;
    }
    return false;
}

bool MinIOManager::makeBucket(const std::string& sBucket) {
    try {
        minio::s3::MakeBucketArgs args;
        args.bucket = sBucket;
        minio::s3::MakeBucketResponse response = m_stClient.MakeBucket(args);
    } catch (const std::exception& e) {
        return false;
    }
    return true;
}

// contain prefix
std::vector<std::string> MinIOManager::listObjects(const std::string& sBucket, 
                                    const std::string& sPrefix) {
    std::vector<std::string> vObjects;
    try {
        minio::s3::ListObjectsArgs args;
        args.bucket = sBucket;
        if (!sPrefix.empty()) {
            args.prefix = sPrefix;
        }
        args.recursive = true;

        auto result = m_stClient.ListObjects(args);
        for (; result; result++) {
            minio::s3::Item item = *result;
            if (!item) {
                break;
            }
            
            size_t nDotPos = item.name.find_last_of('.');
            if (nDotPos == std::string::npos) continue;
            std::string ext = toLower(item.name.substr(nDotPos + 1));
            auto itr = m_setImageExtensions.find(ext);
            if (itr == m_setImageExtensions.end()) continue;

            vObjects.push_back(item.name);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return vObjects;
}

std::vector<std::string> MinIOManager::listFiles(const std::string& sLocalPath, const std::string& sPrefix, bool bRecursive ) {
    std::vector<std::string> sFiles;
    std::string sNewLocalPath = sLocalPath + "/" + sPrefix;
    // printf("%s %s %s\n", sLocalPath.c_str(), sPrefix.c_str(), sNewLocalPath.c_str());
    for (auto& fileName : std::filesystem::directory_iterator(sNewLocalPath))  {
        std::string sNameOnly = fileName.path().filename().string();
        std::string sFullName = fileName.path().string();
            
        std::string sNewPrefix = (sPrefix.empty() ? "" : sPrefix + "/") + sNameOnly;

        if (fileName.is_directory()) {
            if (bRecursive) {
                std::vector<std::string> sSubFiles = listFiles(sLocalPath, sNewPrefix, bRecursive);
                sFiles.insert(sFiles.end(), sSubFiles.begin(), sSubFiles.end());
            } 
            else {
                continue;
            }
        } else {
            sFiles.push_back(sNewPrefix);
        }
    }
    return sFiles;
}

bool MinIOManager::downloadObject(const std::string& sBucket, 
                    const std::string& sObject,
                    const std::string& sSavePath) {
    try {
        if (!bucketExists(sBucket)) return false;

        fs::path fsPath(sSavePath);
        if (!fs::exists(fsPath.parent_path()))
            fs::create_directories(fsPath.parent_path());

        minio::s3::DownloadObjectArgs args;
        args.bucket = sBucket;
        args.object = sObject;
        args.filename = sSavePath;

        auto response = m_stClient.DownloadObject(args);
        if (!response) {
            std::cerr << "Download [" << sObject  << "] failed: " << response.Error().String() << std::endl;
            return false;
        }
    // std::cout << "Download [" << sObject << "] -> " << sSavePath << std::endl;
    } catch (const std::exception& e) {
        // std::cerr << "Download [" << sObject << "] failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

int MinIOManager::downloadDirectory(const std::string& sBucket,
                        const std::string& sRemotePath,
                        const std::string& sSavePath) {
    // std::cout << "Download: " << sRemotePath << " -> " << sSavePath << std::endl;
    auto vObjects = listObjects(sBucket, sRemotePath);
    if (vObjects.empty()) {
        return 0;
    }
    std::cout << "Need download " << vObjects.size() << " from " << sBucket << "|" << sRemotePath << std::endl;

    int nDownloaded = 0;
    int nFailed = 0;

    for (const auto& sObjectName : vObjects) {
        std::string sSaveName = sSavePath + "/" + sObjectName.substr(sRemotePath.length());
        
        if (downloadObject(sBucket, sObjectName, sSaveName)) {
            nDownloaded++;
        } else {
            nFailed++;
        }
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return nDownloaded;
}

bool MinIOManager::uploadObject(const std::string &sBucket, const std::string &sObject, const std::string &sLocalPathName) {
    try {
        if (!bucketExists(sBucket)) {
            if (!makeBucket(sBucket) ) {
                return false;
            }
        }

        minio::s3::UploadObjectArgs args;
        args.bucket = sBucket;
        args.object = sObject;
        args.filename = sLocalPathName;

        minio::s3::UploadObjectResponse response = m_stClient.UploadObject(args);
        if (!response) {
            std::cerr << "Upload [" << sLocalPathName  << "] failed: " << response.Error().String() << std::endl;
            return false;
        }
        // std::cout << "Upload [" << sLocalPathName << "] -> " << sBucket + "/" + sObject << std::endl;
    } catch (const std::exception& e) {
        return false;
    }
    return true;
}

int MinIOManager::uploadDirectory(const std::string &sBucket, const std::string &sObject, const std::string &sLocalPath, bool bRecursive) {
    try {
        if (!bucketExists(sBucket)) {
            if (!makeBucket(sBucket) ) {
                return false;
            }
        }
        for (auto& fileName : std::filesystem::directory_iterator(sLocalPath))  {
            std::string sNameOnly = fileName.path().filename().string();
            std::string sFullName = fileName.path().string();

            if (fileName.is_directory()) {
                std::string sNewObject = sObject + "/" + sNameOnly;
                if (bRecursive) {
                    uploadDirectory(sBucket, sNewObject, fileName.path().string(), bRecursive);                    
                } 
                else {
                    continue;
                }
            } else {
                uploadObject(sBucket, sObject + "/" + sNameOnly, sFullName);
            }
        }
    }
    catch (const std::exception& e) {
        return false;
    }
    return true;
}

void MinIOManager::addTask(const std::string& sBucket, const std::string &sObject, const std::string &sSaveName, bool bDownload) {
    std::lock_guard<std::mutex> locker(m_stMutexQueue);
    if (bDownload)
        m_quDownloadTask.push(std::make_pair(std::make_pair(sBucket, sObject), sSaveName));
    else 
        m_quUploadTask.push(std::make_pair(std::make_pair(sBucket, sObject), sSaveName));
}

void MinIOManager::worker(bool bDownload) {
    while (true) {
        std::pair<std::pair<std::string, std::string>, std::string> pTask;
        {
            std::lock_guard<std::mutex> locker(m_stMutexQueue);
            if (bDownload) {
                if (m_quDownloadTask.empty()) break;
                pTask = m_quDownloadTask.front();
                m_quDownloadTask.pop();
            } else {
                if (m_quUploadTask.empty()) break;
                pTask = m_quUploadTask.front();
                m_quUploadTask.pop();
            }
        }

        std::string sBucket = pTask.first.first;
        std::string sObject = pTask.first.second;
        std::string sSaveName = pTask.second;
        try {
            if (bDownload) {
                minio::s3::DownloadObjectArgs args;
                args.bucket = sBucket;
                args.object = sObject;
                args.filename = sSaveName;
                auto response = m_stClient.DownloadObject(args);
                if (!response) {
                    // std::cerr << "Download [" << sObject  << "] failed: " << response.Error().String() << std::endl;
                    continue;
                }   
            }
            else {
                minio::s3::UploadObjectArgs args;
                args.bucket = sBucket;
                args.object = sObject;
                args.filename = sSaveName;
                auto response = m_stClient.UploadObject(args);
                if (!response) {
                    continue;
                }
                // std::cout << "Upload [" << sSaveName << "] success, save to [" << sBucket << "." << sObject << "]" << std::endl; 
            }
        } catch (const std::runtime_error& e) {
            continue;
        }
    } 
}

void MinIOManager::start(int nNumThreads, bool bDownload) {
    std::vector<std::thread> workers;
    for (int i = 0; i < nNumThreads; ++i) {
        workers.emplace_back(&MinIOManager::worker, this, bDownload);
    }
    
    for (auto& worker : workers) {
        worker.join();
    }
}

int MinIOManager::downloadDirectoryInThread(const std::string& sBucket,
                        const std::string& sObject,
                        const std::string& sSavePath) {

    if (!bucketExists(sBucket)) {
        std::cerr << "Bucket '" << sBucket << "' not found or unaccessable" << std::endl;
        return EXIT_FAILURE;
    }
    if (!fs::exists(sSavePath))
        fs::create_directories(sSavePath);

    auto vObjects = listObjects(sBucket, sObject);
    if (vObjects.empty()) {
        return 0;
    }
    std::cout << "Need download " << vObjects.size() << std::endl;

    for (const auto& sObjectName : vObjects) {
        std::string sSaveName = sSavePath + "/" + sObjectName.substr(sObject.length());
        
        addTask(sBucket, sObjectName, sSaveName);
    }

    start(std::thread::hardware_concurrency(), true);

    return 0;
}

int MinIOManager::uploadDirectoryInThread(const std::string& sBucket,
                        const std::string& sObject,
                        const std::string& sLocalPath,
                        bool bRecursive) {

    if (!fs::exists(sLocalPath)) {
        std::cerr << "local path '" << sLocalPath << "' not found or unaccessable" << std::endl;
        return EXIT_FAILURE;
    }
    if (!bucketExists(sBucket)) {
        if (!makeBucket(sBucket)) {
            return EXIT_FAILURE;
        }
    }

    std::vector<std::string> vFiles = listFiles(sLocalPath);
    if (vFiles.empty()) return 0;

    std::cout << "Need upload " << vFiles.size() << std::endl;

    for (const auto& sName : vFiles) {
        std::string sSaveName = sLocalPath + "/" + sName;
        addTask(sBucket, sObject + "/" + sName, sSaveName, false);
    }

    start(std::thread::hardware_concurrency(), false);

    return 0;
}
