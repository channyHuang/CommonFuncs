
#ifndef DLLPROGRESSEXPORT_H
#define DLLPROGRESSEXPORT_H

#include <string.h>
#include <string>

#ifdef __cplusplus
#define D_EXTERN_C extern "C"
#else
#define D_EXTERN_C
#endif

#define __SHARE_EXPORT

#ifdef __SHARE_EXPORT
#define D_SHARE_EXPORT D_DECL_EXPORT
#else
#define D_SHARE_EXPORT D_DECL_IMPORT
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__WIN32__)
#define D_CALLTYPE __stdcall
#define D_DECL_EXPORT __declspec(dllexport)
#define D_DECL_IMPORT
#else
#define __stdcall
#define D_CALLTYPE
#define D_DECL_EXPORT __attribute__((visibility("default")))
#define D_DECL_IMPORT __attribute__((visibility("default")))
#endif

D_EXTERN_C D_SHARE_EXPORT bool HG_registRedisAddr(const std::string& sTaskId, const std::string& sRedisAddr, int nRedisDB = 0);
D_EXTERN_C D_SHARE_EXPORT bool HG_updateProgress(const std::string& sTaskId, float fProgress, const std::string& sMessage);
D_EXTERN_C D_SHARE_EXPORT bool HG_set(const std::string& pKey, const std::string& pValue, const std::string& sTaskId);

#endif // DLLPROGRESSEXPORT_H