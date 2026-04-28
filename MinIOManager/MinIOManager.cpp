#include "MinIOManager.h"

#include <iostream>
#include <unordered_set>
#include <thread>
#include <regex>
#include <cstring>

#include "spdlog/spdlog.h"

// minio sdk has noly 5 catchs when download

MinIOManager::MinIOManager(const std::string& endpoint, 
                 bool bHttps,
                 const std::string& access_key, 
                 const std::string& secret_key) : 
                 m_stBaseUrl(endpoint, bHttps), 
                 m_stProvider(access_key, secret_key),
                 m_stClient(m_stBaseUrl, &m_stProvider), 
                stDownUpManager(5, [this](const MinioTask& stTask, const std::atomic<bool>& bStopFlag) {
            return workerDownUp(stTask, bStopFlag);
        }) {}

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
            spdlog::get("MinIOManager")->critical("Check bucket [{}] failed: {}", sBucket, response.Error().String());
            return false;
        }
        return response.exist;
    } catch (const std::exception& e) {
        spdlog::get("MinIOManager")->critical("Check Bucket Exception: {}", e.what());
    }
    return false;
}

bool MinIOManager::makeBucket(const std::string& sBucket) {
    try {
        minio::s3::MakeBucketArgs args;
        args.bucket = sBucket;
        minio::s3::MakeBucketResponse response = m_stClient.MakeBucket(args);
        return true;
    } catch (const std::exception& e) {
        spdlog::get("MinIOManager")->critical("Make Bucket Exception: {}", e.what());
    }
    return false;
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

        std::filesystem::path fsPath(sSavePath);
        if (!std::filesystem::exists(fsPath.parent_path()))
            std::filesystem::create_directories(fsPath.parent_path());

        minio::s3::DownloadObjectArgs args;
        args.bucket = sBucket;
        args.object = sObject;
        args.filename = sSavePath;

        auto response = m_stClient.DownloadObject(args);
        if (!response) {
            spdlog::get("MinIOManager")->critical("Download [{}] failed: {}", sObject, response.Error().String());
            return false;
        }
    } catch (const std::exception& e) {
        spdlog::get("MinIOManager")->critical("Download Exception: {}", e.what());
        return false;
    }
    return true;
}

bool MinIOManager::uploadObject(const std::string &sBucket, const std::string &sObject, const std::string &sLocalPathName) {
    if (!bucketExists(sBucket)) {
        if (!makeBucket(sBucket) ) {
            return false;
        }
    }
    try {
        minio::s3::UploadObjectArgs args;
        args.bucket = sBucket;
        args.object = sObject;
        args.filename = sLocalPathName;

        minio::s3::UploadObjectResponse response = m_stClient.UploadObject(args);
        if (!response) {
            spdlog::get("MinIOManager")->critical("Upload [{}] failed: {}", sLocalPathName, response.Error().String());
            return false;
        }
    } catch (const std::exception& e) {
        spdlog::get("MinIOManager")->critical("Upload Exception: {}", e.what());
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
                    spdlog::get("MinIOManager")->critical("Download {} / {} failed: {} ", sBucket, sObject, response.Error().String());
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
                    spdlog::get("MinIOManager")->critical("Upload {} -> {} failed : {} ", sSaveName, sObject, response.Error().String());
                    continue;
                }
                // std::cout << "Upload [" << sSaveName << "] success, save to [" << sBucket << "." << sObject << "]" << std::endl; 
            }
        } catch (const std::runtime_error& e) {
            std::cerr << "MinIOManager::worker " << e.what() << std::endl;
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
    try {
        if (!std::filesystem::exists(sSavePath))
            std::filesystem::create_directories(sSavePath);

        auto vObjects = listObjects(sBucket, sObject);
        if (vObjects.empty()) {
            return EXIT_SUCCESS;
        }
        std::cout << "Need download " << vObjects.size() << std::endl;

        for (const auto& sObjectName : vObjects) {
            std::string sSaveName = sSavePath + "/" + sObjectName.substr(sObject.length());
        
            stDownUpManager.addTask({sBucket, sObjectName, sSaveName});
        }

        stDownUpManager.start();
        stDownUpManager.waitForComplete();
    } catch (const std::exception &e) {
        spdlog::get("MinIOManager")->critical("download exception: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int MinIOManager::uploadDirectoryInThread(const std::string& sBucket,
                        const std::string& sObject,
                        const std::string& sLocalPath,
                        bool bRecursive) {

    if (!std::filesystem::exists(sLocalPath)) {
        std::cerr << "local path '" << sLocalPath << "' not found or unaccessable" << std::endl;
        return EXIT_FAILURE;
    }
    if (!bucketExists(sBucket)) {
        if (!makeBucket(sBucket)) {
            return EXIT_FAILURE;
        }
    }
    try {
        std::vector<std::string> vFiles = listFiles(sLocalPath);
        if (vFiles.empty()) return 0;

        std::cout << "Need upload " << vFiles.size() << std::endl;

        for (const auto& sName : vFiles) {
            std::string sSaveName = sLocalPath + "/" + sName;

            stDownUpManager.addTask({sBucket, sObject + "/" + sName, sSaveName, true});
        }

        stDownUpManager.start();
        stDownUpManager.waitForComplete();
    } catch (const std::exception &e) {
        spdlog::get("MinIOManager")->critical("download exception: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

bool MinIOManager::parseBrowserAddr(const std::string& sMinIOUrl) {
    std::regex pattern(R"((https?)://([^/:]+)(?::(\d+))?/browser/([^/]+)/(.+))");
    std::smatch matches;
    if (!std::regex_match(sMinIOUrl, matches, pattern)) {
        std::cerr << "URL Error: " << sMinIOUrl << std::endl;
        return EXIT_FAILURE;
    }

    bool bHttps = ((std::strcmp(matches[1].str().c_str(), "https") == 0)  ? true : false);
    int nPort = matches[3].matched ? std::stoi(matches[3].str()) - 1 : 9000;
    std::string sEndpoint = matches[1].str() + "://" + matches[2].str() + ":" + std::to_string(nPort);
    std::string sBucket = matches[4].str();
    std::string sObjectBase64 = matches[5].str();

    // std::string sObject = base64_decode(sObjectBase64);
    return EXIT_SUCCESS;
}

bool MinIOManager::setValid(bool bValid) {
    m_bValid = bValid;
}

bool MinIOManager::workerDownUp(const MinioTask& stTask, const std::atomic<bool>& bStopFlag) {
    try {
        if (stTask.bUpload) {
            minio::s3::UploadObjectArgs args;
            args.bucket = stTask.sBucket;
            args.object = stTask.sObjectKey;
            args.filename = stTask.sFileFullName;
            auto response = m_stClient.UploadObject(args);
            if (!response) {
                spdlog::get("MinIOManager")->critical("Upload {} -> {}/{} failed : {} ", stTask.sFileFullName, stTask.sBucket, stTask.sObjectKey, response.Error().String());
                return false;
            }
        } else {
            minio::s3::DownloadObjectArgs args;
            args.bucket = stTask.sBucket;
            args.object = stTask.sObjectKey;
            args.filename = stTask.sFileFullName;
            args.overwrite = true;
            auto response = m_stClient.DownloadObject(args);
            if (!response) {
                spdlog::get("MinIOManager")->error("Download {} / {} failed: {} ", stTask.sBucket, stTask.sObjectKey, response.Error().String());
                return false;
            }   
        }
    } catch (const std::exception& e) {
        spdlog::get("MinIOManager")->critical("Download Exception: {} ", e.what());
        return false;
    }
    return true;
}

int MinIOManager::downloadJsonListInThread(const std::string& sBucket, const std::vector<std::string> &vObjects, const std::string& sLocalPath) {
    if (!bucketExists(sBucket)) {
        spdlog::get("MinIOManager")->info("Bucket '{}' not found or unaccessable", sBucket);
        return EXIT_FAILURE;
    }
    try {
        if (!std::filesystem::exists(sLocalPath))
            std::filesystem::create_directories(sLocalPath);

        for (const auto& sObjectName : vObjects) {
            auto pos = sObjectName.find_last_of("/");
            std::string sSaveNameOnly;
            if (pos == std::string::npos) {
                sSaveNameOnly = sObjectName;
            } else {
                sSaveNameOnly = sObjectName.substr(pos + 1);
            }
            std::string sSaveName = sLocalPath + "/" + sSaveNameOnly;
            stDownUpManager.addTask({sBucket, sObjectName, sSaveName});
        }

        stDownUpManager.start();
        stDownUpManager.waitForComplete();
    } catch (const std::exception &e) {
        spdlog::get("MinIOManager")->critical("download exception: {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int MinIOManager::uploadJsonListInThread(const std::string& sBucket, std::unordered_map<std::string, std::string> &mapObjectKey, const std::string& sLocalPath) {
    if (!std::filesystem::exists(sLocalPath)) {
        std::cerr << "local path '" << sLocalPath << "' not found or unaccessable" << std::endl;
        return EXIT_FAILURE;
    }
    if (!m_bValid) {
        std::cout << "invalid " << std::endl;
        return EXIT_SUCCESS;
    }
    if (!bucketExists(sBucket)) {
        if (!makeBucket(sBucket)) {
            std::cerr << "make bucket failed" << std::endl;
            return EXIT_FAILURE;
        }
    }
    try {
        for (auto item : mapObjectKey) {
            stDownUpManager.addTask({sBucket, item.second, item.first, true});
        }
        stDownUpManager.start();
        stDownUpManager.waitForComplete();
    } catch (const std::exception &e) {
        spdlog::get("MinIOManager")->critical("upload exception: {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void MinIOManager::cancelDownUp() {
    stDownUpManager.stop();
}
