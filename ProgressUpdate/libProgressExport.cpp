#include "libProgressExport.h"

#include "ProgressManager.h"

bool HG_registRedisAddr(const std::string& sTaskId, const std::string& sRedisAddr, int nRedisDB) {
    return ProgressManager::getInstance().registRedisAddr(sTaskId, sRedisAddr, nRedisDB);
}

bool HG_updateProgress(const std::string& sTaskId, float fProgress, const std::string& sMessage) {
    return ProgressManager::getInstance().updateProgress(sTaskId, fProgress, sMessage);
}

bool HG_set(const std::string& pKey, const std::string& pValue, const std::string& sTaskId) {
    return ProgressManager::getInstance().set(pKey, pValue, sTaskId);
}