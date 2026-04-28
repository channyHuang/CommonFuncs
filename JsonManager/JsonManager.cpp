#include "JsonManager.h"

#include <filesystem>
#include <fstream>

// simdjson: fastest but only parse; REPIDJSON faster; nlohman::json normal but easy use
#include "simdjson.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

JsonManager* JsonManager::m_pInstance = nullptr;

JsonManager* JsonManager::getInstance() {
    if (m_pInstance == nullptr) {
        m_pInstance = new JsonManager();
    }
    return m_pInstance;
}

JsonManager::JsonManager() {}
JsonManager::~JsonManager() {
    if (m_pInstance != nullptr) {
        delete m_pInstance;
        m_pInstance = nullptr;
    }
}

std::string JsonManager::getFileCreateTime(const std::string& sFileName) {
    if (sFileName.empty() || !std::filesystem::exists(sFileName)) {
        return "";
    }

    try {
        std::filesystem::file_status status = std::filesystem::status(sFileName);
        auto ftime = std::filesystem::last_write_time(sFileName);
        
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        
        std::stringstream ss;
        const std::string& format = "%Y-%m-%d %H:%M:%S";
        ss << std::put_time(std::localtime(&tt), format.c_str());
        return ss.str();
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return "";
}

std::string rsaDecrypt(const std::string& sEncryptData, const std::string& sKey) {
    // do something ...
    return sEncryptData;
}

bool JsonManager::parseRequest(const std::string& sRequestBody) {
    simdjson::ondemand::parser stParser;
    simdjson::padded_string stString = simdjson::padded_string(sRequestBody);
    simdjson::ondemand::document stDoc = stParser.iterate(stString);
    auto stObject = stDoc.get_object();
    if (stObject.error()) return false;

    for (auto sField : stObject) {
        std::string_view sKey = sField.unescaped_key();
        if (sKey == "encryptData") {
            std::string sEncryptData;
            sField.value().get(sEncryptData);

            std::string sCipherText = rsaDecrypt(sEncryptData, "privHGRecon.key");
            if (sCipherText.empty()) return false;
            simdjson::padded_string stCipherString = simdjson::padded_string(sCipherText);
            simdjson::ondemand::document stCipherDoc = stParser.iterate(stCipherString);
            stObject = stCipherDoc.get_object();
            if (stObject.error()) return false;

            for (auto sField: stObject) {
                std::string_view sKey = sField.unescaped_key();
                if (sKey == "taskId") {
                    std::string sTaskId;
                    sField.value().get(sTaskId);
                } else if (sKey == "reportId") { // data
                    int nReportId = 0;
                    if (sField.value().get(nReportId)) return false;
                    std::string sReportId = std::to_string(nReportId);
                } else if (sKey == "type") {
                    std::string_view sType;
                    if (sField.value().is_string()) {
                        if (sField.value().get(sType)) return false;
                        std::string key(sType);
                        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
                        // do something ...
                    } else if (sField.value().type() == simdjson::ondemand::json_type::array) {
                        simdjson::ondemand::array stArray;
                        if (sField.value().get(stArray)) return false;
                        for (auto stTypeValue : stArray) {
                            if (stTypeValue.get_string().get(sType)) return false;

                            std::string key(sType);
                            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
                            // do something ...
                        }
                    } else if (sKey == "position") {
                        simdjson::ondemand::array stArray;
                        if (sField.value().get(stArray)) return false;
                        for (auto stTypeObject : stArray) {
                            simdjson::ondemand::object stGPSObject;
                            if (stTypeObject.get(stGPSObject)) return false;
                            for (auto sGPSField : stGPSObject) {
                                std::string_view sGPSKey = sGPSField.unescaped_key();
                                float fLat, fLon;
                                if (sGPSKey == "lat") {
                                    sGPSField.value().get(fLat);
                                } else if (sGPSKey == "lng") {
                                    sGPSField.value().get(fLon);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}

void writeContent(rapidjson::PrettyWriter<rapidjson::StringBuffer> &stWriter, const std::string& sFileName) {
    stWriter.Key("content");
    stWriter.StartObject();
    stWriter.Key("uri");
    stWriter.String(sFileName.c_str());
    stWriter.EndObject();
}

void writeBox(rapidjson::PrettyWriter<rapidjson::StringBuffer> &stWriter, const float box[]) {
    stWriter.Key("boundingVolume");
    stWriter.StartObject();
    {
        stWriter.Key("box");
        stWriter.StartArray();
        for (size_t i = 0; i < 12; ++i) {
            stWriter.Double(box[i]);
        }
        stWriter.EndArray();
    }
    stWriter.EndObject();
}

unsigned writeChildren(rapidjson::PrettyWriter<rapidjson::StringBuffer> &stWriter, const std::vector<StSinglePnts> &vPnts, const std::vector<float> &vErrors, unsigned nParentIdx = 0) {
    stWriter.Key("children");
    stWriter.StartArray();
    unsigned nIdx = nParentIdx + 1;
    while (nIdx < vPnts.size()) {
        if (! vPnts[nIdx].isParent(vPnts[nParentIdx]) ) break;
        
        stWriter.StartObject();
        stWriter.Key("geometricError");
        stWriter.Double(vErrors[vPnts[nIdx].nDepth]);

        writeContent(stWriter, vPnts[nIdx].sFileName);
        writeBox(stWriter, vPnts[nIdx].vBox); 

        unsigned nSubIdx = writeChildren(stWriter, vPnts, vErrors, nIdx);
        nIdx = nSubIdx;

        stWriter.EndObject();
    }
    stWriter.EndArray();
    return nIdx;
}

bool JsonManager::writeTilesetJson(std::vector<StSinglePnts> vPnts, std::array<double, 16> transform, float vBox[], const std::string &sOutFileName, float fRootGeometryError) {
    if (vPnts.size() <= 0) return false;
    std::sort(vPnts.begin(), vPnts.end());
    size_t nMaxLevel = 0;
    for (auto item : vPnts) {
        if (item.nDepth > nMaxLevel) nMaxLevel = item.nDepth;
    }
    std::vector<float> vErrors(nMaxLevel + 1);
    vErrors[nMaxLevel] = 0.001;

    if (nMaxLevel > 0) {
        vErrors[0] = fRootGeometryError;
        for (int i = 1; i < nMaxLevel; ++i) {
            vErrors[i] = vErrors[i - 1] / 2.f;
        }
    }
    
    rapidjson::StringBuffer stBuf;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> stWriter(stBuf);
    stWriter.StartObject();
    {
        stWriter.Key("asset");
        stWriter.StartObject();
        stWriter.Key("version");
        stWriter.String("1.1");
        stWriter.Key("gltfUpAxis");
        stWriter.String("Z");
        stWriter.EndObject();

        stWriter.Key("geometricError");
        stWriter.Double(1000);
        // root
        stWriter.Key("root");
        stWriter.StartObject();
        {
            stWriter.Key("transform");
            stWriter.StartArray();
            for (int i = 0; i < 16; ++i) {
                stWriter.Double(transform[i]);
            }
            stWriter.EndArray();

            stWriter.Key("boundingVolume");
            stWriter.StartObject();
            {
                stWriter.Key("box");
                stWriter.StartArray();
                for (size_t i = 0; i < 12; ++i) {
                    stWriter.Double(vBox[i]);
                }
                stWriter.EndArray();
            }
            stWriter.EndObject();

            stWriter.Key("geometricError");
            stWriter.Double(vErrors[0]);

            stWriter.Key("refine");
            stWriter.String("REPLACE");

            writeContent(stWriter, vPnts[0].sFileName);

            writeChildren(stWriter, vPnts, vErrors, 0);
        }
        stWriter.EndObject();
    }
    stWriter.EndObject();

    std::ofstream file(sOutFileName);
    if (file) {
        file << std::setw(4) << stBuf.GetString();
    } else {
        printf("Write tileset.json failed\n");
    }
    file.close();
    return true;
}