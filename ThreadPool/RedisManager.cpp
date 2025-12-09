#include "RedisManager.h"

namespace HG {

RedisManager* RedisManager::m_pInstance = nullptr;

RedisManager* RedisManager::getInstance() {
    if (m_pInstance == nullptr) {
        m_pInstance = new RedisManager();
    }
    return m_pInstance;
}

RedisManager::RedisManager() {

}
RedisManager::~RedisManager() {
    if (m_pInstance != nullptr) {
        delete m_pInstance;
    }
    m_pInstance = nullptr;
}

bool RedisManager::set(const char* pKey, const char* pValue) {
    try {
        std::unique_ptr<sw::redis::Redis> redis;
        redis = std::make_unique<sw::redis::Redis>("tcp://127.0.0.1:6379");
        redis->set(pKey, pValue);
        redis.reset();
    } catch (const sw::redis::Error& e) {
        printf("%s\n", e.what());
        return false;
    }
    return true;
}

void RedisManager::get(const char* pKey) {
    try {
        std::unique_ptr<sw::redis::Redis> redis;
        redis = std::make_unique<sw::redis::Redis>("tcp://127.0.0.1:6379");
        auto pValue = redis->get(pKey);
        if (pValue) {
            int retrieved_value = std::stoi(*pValue);

            printf("get %s = %s %d\n", pKey, (*pValue).c_str(), retrieved_value);
        }
        redis.reset();
    } catch (const sw::redis::Error& e) {
        printf("%s\n", e.what());
    }
}

};