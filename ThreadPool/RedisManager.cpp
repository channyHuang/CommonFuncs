#include "RedisManager.h"

// simdjson: fastest but only parse; REPIDJSON faster; nlohman::json normal but easy use
#include "simdjson.h"
#include "rapidjson/writer.h"

namespace Haige {

RedisManager::RedisManager() {
    m_stConnectionPoolOpt.size = 50;
    m_stConnectionPoolOpt.connection_idle_time = std::chrono::minutes(5);
    m_stConnectionPoolOpt.connection_lifetime = std::chrono::hours(8);
}

bool RedisManager::registRedisAddr(const std::string& sTaskId, const std::string& sRedisAddr, const std::string& sRedisPs, 
                                int nRedisDB, const std::vector<std::string>& vTaskIds) {
    std::regex pattern(R"((https?)://([^/:]+)(?::(\d+))?)");
    std::smatch matches;
    if (!std::regex_match(sRedisAddr, matches, pattern)) {
        std::cerr << "Redis URL Error: " << sRedisAddr << std::endl;
        return false;
    }

    if (sTaskId.empty()) {
        return false;
    } 

    RedisConnectInfo info;
    info.stRedisOptions.host = matches[2].str();
    info.stRedisOptions.port = matches[3].matched ? std::stoi(matches[3].str()) : 6379;
    info.stRedisOptions.password = sRedisPs;
    info.stRedisOptions.db = nRedisDB;

    m_mapRedisOptions.insert(std::make_pair(sTaskId, info));
    printf("addAddr : %s %d [%s(%u)] .\n", info.stRedisOptions.host.c_str(), info.stRedisOptions.port, sTaskId.c_str(), vTaskIds.size());
    if (vTaskIds.size() > 0) {
        m_mapSubTaskIds.insert({sTaskId, vTaskIds});
        for (auto sId : vTaskIds) {
            printf("subTaskId: %s\n", sId.c_str());
        }
    }
    return true;
}

void RedisManager::unregistRedisAddr(const std::string& sTaskId) {
    auto itr = m_mapRedisOptions.find(sTaskId);
    if (itr == m_mapRedisOptions.end()) return;
    m_mapRedisOptions.erase(itr);

    auto stNow = std::chrono::steady_clock::now();
    auto stKeepTime = std::chrono::hours(24);
    for (auto itr = m_mapRedisOptions.begin(); itr != m_mapRedisOptions.end();) {
        auto diff = stNow - itr->second.stLastUsedTime;
        if (diff > stKeepTime) {
            itr = m_mapRedisOptions.erase(itr);
        } else {
            itr++;
        }
    }
    {
        auto itr = m_mapSubTaskIds.find(sTaskId);
        if (itr != m_mapSubTaskIds.end()) {
            m_mapSubTaskIds.erase(itr);
        }
    }
}

bool RedisManager::updateProgress(const std::string& sTaskId, float fProgress, const std::string& sMessage) {
    rapidjson::StringBuffer stBuf;
    rapidjson::Writer<rapidjson::StringBuffer> stWriter(stBuf);
    stWriter.StartObject();
    stWriter.Key("progress");
    stWriter.Int(std::ceil(fProgress));
    stWriter.Key("message");
    stWriter.String(sMessage.c_str());
    stWriter.EndObject();

    std::string sValue = stBuf.GetString();
    // printf("RedisManager::updateProgress %s %f\n", sTaskId.c_str(), fProgress);
    bool bres = set("GEN_MODELING:", sValue, sTaskId);
    if (fProgress < 0 || fProgress >= 100.0f) {
        m_mapRedisOptions.erase(sTaskId);
    }
    return bres;
}

bool RedisManager::updateProgressAssync(const std::string& sTaskId, float fProgress, const std::string& sMessage) {
    std::thread mThread = std::thread([this, sTaskId, fProgress, sMessage]() {
        this->updateProgress(sTaskId, fProgress, sMessage);
    });
    mThread.detach();
    return true;
}

std::shared_ptr<sw::redis::Redis> RedisManager::getRedis(const std::string& sTaskId) {
    bool bIsMainId = true;
    std::string sMainTaskId = sTaskId;
    auto itr = m_mapRedisOptions.find(sMainTaskId);
    if (itr == m_mapRedisOptions.end()) {
        bIsMainId = false;
        sMainTaskId = sTaskId.substr(0, sTaskId.length() - 2);
        itr = m_mapRedisOptions.find(sMainTaskId);
        if (itr == m_mapRedisOptions.end()) {
            printf("set redis value failed: task id [%s] not found!\n", sTaskId.c_str());
            return nullptr;
        }
    }

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto itrRedis = m_mapRedis.find(sMainTaskId);
        if (itrRedis == m_mapRedis.end()) {
            std::shared_ptr<sw::redis::Redis> pRedis = std::make_shared<sw::redis::Redis>(itr->second.stRedisOptions, m_stConnectionPoolOpt);
            m_mapRedis.insert({sMainTaskId, pRedis});
            return pRedis;
        } else {
            return itrRedis->second;
        }
    }
    return nullptr;
}

bool RedisManager::set(const std::string& pKey, const std::string& pValue, const std::string& sTaskId) {
    if (sTaskId.empty()) {
        return false;
    }
    if (!m_bValid) return true;
    // printf("RedisManager::set %s %s %s\n", sTaskId.c_str(), pKey.c_str(), pValue.c_str());
    try {
        std::unique_ptr<sw::redis::Redis> redis = nullptr;

        auto itr = m_mapRedisOptions.find(sTaskId);
        // set for all sub taskid or main taskid if has no sub taskid
        if (itr != m_mapRedisOptions.end()) {
            
            redis = std::make_unique<sw::redis::Redis>(itr->second.stRedisOptions);
            itr->second.stLastUsedTime = std::chrono::steady_clock::now();

            auto nitr = m_mapSubTaskIds.find(sTaskId);
            // only has one main taskid
            if (nitr == m_mapSubTaskIds.end()) {
                redis->set(pKey + sTaskId, pValue, std::chrono::hours(m_nKeepHour));
            } else { // has several sub taskid
                for (auto sSubTaskId : nitr->second) {
                    redis->set(pKey + sSubTaskId, pValue, std::chrono::hours(m_nKeepHour));
                }
            }
            redis.reset();
        } else {
            // set for one sub taskid
            std::string sMainTaskId = sTaskId.substr(0, sTaskId.length() - 2);
            auto mitr = m_mapRedisOptions.find(sMainTaskId);
            if (mitr == m_mapRedisOptions.end()) {
                printf("set redis value [%s -> %s] failed: task id [%s](%s) not found!\n", pKey.c_str(), pValue.c_str(), sMainTaskId.c_str(), sTaskId.c_str());
                return true;
            }

            redis = std::make_unique<sw::redis::Redis>(mitr->second.stRedisOptions);
            mitr->second.stLastUsedTime = std::chrono::steady_clock::now();

            redis->set(pKey + sTaskId, pValue, std::chrono::hours(m_nKeepHour));
            
            redis.reset();
        }
    } catch (const sw::redis::Error& e) {
        printf("%s\n", e.what());
        return false;
    }
    return true;
}

bool RedisManager::set_v2(const std::string& pKey, const std::string& pValue, const std::string& sTaskId) {
    if (sTaskId.empty()) {
        return false;
    }
    if (!m_bValid) return true;
    try {
        std::shared_ptr<sw::redis::Redis> pRedis = getRedis(sTaskId);
        if (pRedis == nullptr) return false;
        auto nitr = m_mapSubTaskIds.find(sTaskId);
        // only has one main taskid
        if (nitr == m_mapSubTaskIds.end()) {
            pRedis->set(pKey + sTaskId, pValue, std::chrono::hours(m_nKeepHour));
        } else { // has several sub taskid
            for (auto sSubTaskId : nitr->second) {
                pRedis->set(pKey + sSubTaskId, pValue, std::chrono::hours(m_nKeepHour));
            }
        }
    } catch (const sw::redis::Error& e) {
        printf("%s\n", e.what());
        return false;
    }
    return true;
}

std::string RedisManager::get(const std::string& pKey, const std::string& sTaskId) {
    if (sTaskId.empty()) {
        return "";
    }
    try {
        std::unique_ptr<sw::redis::Redis> redis;
            
        auto itr = m_mapRedisOptions.find(sTaskId);
        if (itr == m_mapRedisOptions.end()) {
            printf("RedisManager::get sTaskId[%s] not found!\n", sTaskId.c_str());
            return "";
        }
        else 
            redis = std::make_unique<sw::redis::Redis>(itr->second.stRedisOptions);
        
        auto pValue = redis->get(pKey);
        if (pValue) {
            return std::string(*pValue);
        } else {
            printf("RedisManager::get failed, pKey = %s, sTaskId = %s, host = %s, port = %d, db = %d\n", 
                    pKey.c_str(), sTaskId.c_str(), 
                    itr->second.stRedisOptions.host.c_str(),
                    itr->second.stRedisOptions.port,
                    itr->second.stRedisOptions.db);
        }
        redis.reset();

    } catch (const sw::redis::Error& e) {
        printf("%s\n", e.what());
    }
    printf("RedisManager::get empty\n");
    return "";
}

bool RedisManager::del(const std::string& pKey, const std::string& sTaskId) {
    if (sTaskId.empty()) {
        return false;
    }
    try {
        std::unique_ptr<sw::redis::Redis> redis;
        
        auto itr = m_mapRedisOptions.find(sTaskId);
        if (itr == m_mapRedisOptions.end())
            return false;
        else 
            redis = std::make_unique<sw::redis::Redis>(itr->second.stRedisOptions);
        
        redis->ping();
        redis->del(pKey);
        redis.reset();
    } catch (const sw::redis::Error& e) {
        printf("%s\n", e.what());
        return false;
    }
    return true;
}

void RedisManager::setValid(bool bValid) {
    m_bValid = bValid;
}

} // end of namespace Haige