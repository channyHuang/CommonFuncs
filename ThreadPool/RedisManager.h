#pragma once

#include <sw/redis++/redis++.h>

namespace HG {

class RedisManager {
public:
    ~RedisManager();
    static RedisManager* getInstance();
    
    bool set(const char* pKey, const char* pValue);
    void get(const char* pKey);

private:
    RedisManager();

private:
    static RedisManager* m_pInstance;
};

};