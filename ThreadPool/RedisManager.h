#pragma once 

#include <chrono>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>

#include <sw/redis++/redis++.h>

namespace Haige {

struct RedisConnectInfo {
    sw::redis::ConnectionOptions stRedisOptions;
    std::chrono::steady_clock::time_point stLastUsedTime;

    RedisConnectInfo() {
        stLastUsedTime = std::chrono::steady_clock::now();
    }
};

class RedisManager {
public:
    static RedisManager& getInstance() {
        static RedisManager m_pInstance;
        return m_pInstance;
    }

    RedisManager(const RedisManager&) = delete;
    RedisManager& operator=(const RedisManager&) = delete;

    bool registRedisAddr(const std::string& sTaskId, const std::string& sRedisAddr, const std::string& sRedisPs, 
                        int nRedisDB = 0, const std::vector<std::string>& vTaskIds = std::vector<std::string>());
    void unregistRedisAddr(const std::string& sTaskId);
    bool updateProgress(const std::string& sTaskId, float fProgress, const std::string& sMessage);
    bool updateProgressAssync(const std::string& sTaskId, float fProgress, const std::string& sMessage);
    bool set(const std::string& pKey, const std::string& pValue, const std::string& sTaskId);
    bool set_v2(const std::string& pKey, const std::string& pValue, const std::string& sTaskId);
    std::string get(const std::string& pKey, const std::string& sTaskId);
    bool del(const std::string& pKey, const std::string& sTaskId);


    void setValid(bool bValid = true);
private:
    RedisManager();
    ~RedisManager() {}
    
    std::shared_ptr<sw::redis::Redis> getRedis(const std::string& sTaskId); 

private:
    sw::redis::ConnectionPoolOptions m_stConnectionPoolOpt;
    std::unordered_map<std::string, RedisConnectInfo> m_mapRedisOptions;
    std::unordered_map<std::string, std::shared_ptr<sw::redis::Redis> > m_mapRedis;
    std::unordered_map<std::string, std::vector<std::string> > m_mapSubTaskIds;
    bool m_bValid = true;
    size_t m_nKeepHour = 72;
    std::mutex m_mutex;
};

} // end of namespace Haige