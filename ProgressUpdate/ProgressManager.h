#pragma once 

#include <string>
#include <regex>
#include <iostream>

#include <sw/redis++/redis++.h>
#include <unordered_map>

class ProgressManager {
public:
    static ProgressManager& getInstance() {
        static ProgressManager m_pInstance;
        return m_pInstance;
    }

    ProgressManager(const ProgressManager&) = delete;
    ProgressManager& operator=(const ProgressManager&) = delete;

    bool registRedisAddr(const std::string& sTaskId, const std::string& sRedisAddr, int nRedisDB = 0);
    bool updateProgress(const std::string& sTaskId, float fProgress, const std::string& sMessage);
    bool set(const std::string& pKey, const std::string& pValue, const std::string& sTaskId);

private:
    ProgressManager() {}
    ~ProgressManager() {}

private:
    std::unordered_map<std::string, sw::redis::ConnectionOptions> m_mapRedisOptions;
    sw::redis::ConnectionOptions m_stOptions;
    std::string m_sRedisAddr = "http://127.0.0.1:6379";

};
