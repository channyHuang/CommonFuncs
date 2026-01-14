#include "ProgressManager.h"

// simdjson: fastest but only parse; REPIDJSON faster; nlohman::json normal but easy use
#include "simdjson.h"
#include "rapidjson/writer.h"

bool ProgressManager::registRedisAddr(const std::string& sTaskId, const std::string& sRedisAddr, int nRedisDB) {
    std::regex pattern(R"((https?)://([^/:]+)(?::(\d+))?)");
    std::smatch matches;
    if (!std::regex_match(sRedisAddr, matches, pattern)) {
        std::cerr << "Redis URL Error: " << sRedisAddr << std::endl;
        return false;
    }

    if (sTaskId.empty()) {
        m_sRedisAddr = sRedisAddr;
        m_stOptions.host = matches[2].str();
        m_stOptions.port = matches[3].matched ? std::stoi(matches[3].str()) : 6379;
        m_stOptions.password = "@Wf123456";
        m_stOptions.db = nRedisDB;
    } else {
        sw::redis::ConnectionOptions stOptions;
        stOptions.host = matches[2].str();
        stOptions.port = matches[3].matched ? std::stoi(matches[3].str()) : 6379;
        stOptions.password = "@Wf123456";
        stOptions.db = nRedisDB;
        m_mapRedisOptions.insert(std::make_pair(sTaskId, stOptions));
        printf("addAddr : %s %d [%s]\n", stOptions.host.c_str(), stOptions.port, sTaskId.c_str());
    }
    return true;
}

bool ProgressManager::updateProgress(const std::string& sTaskId, float fProgress, const std::string& sMessage) {
    rapidjson::StringBuffer stBuf;
    rapidjson::Writer<rapidjson::StringBuffer> stWriter(stBuf);
    stWriter.StartObject();
    stWriter.Key("progress");
    stWriter.Int(std::ceil(fProgress));
    stWriter.Key("message");
    stWriter.String(sMessage.c_str());
    stWriter.EndObject();

    std::string sValue = stBuf.GetString();
    bool bres = set("GEN_MODELING:" + sTaskId, sValue, sTaskId);
    if (fProgress < 0 || fProgress >= 100.0f) {
        m_mapRedisOptions.erase(sTaskId);
    }
    return bres;
}

bool ProgressManager::set(const std::string& pKey, const std::string& pValue, const std::string& sTaskId) {
    try {
        std::unique_ptr<sw::redis::Redis> redis;
        if (sTaskId.empty()) {
            redis = std::make_unique<sw::redis::Redis>(m_stOptions);
        } else {
            auto itr = m_mapRedisOptions.find(sTaskId);
            if (itr == m_mapRedisOptions.end())
                redis = std::make_unique<sw::redis::Redis>(m_stOptions);
            else 
                redis = std::make_unique<sw::redis::Redis>(itr->second);
        }
        redis->ping();
        redis->set(pKey, pValue, std::chrono::hours(24));
        redis.reset();
    } catch (const sw::redis::Error& e) {
        printf("%s\n", e.what());
        return false;
    }
    return true;
}