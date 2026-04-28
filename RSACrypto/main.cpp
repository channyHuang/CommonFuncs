#include <iostream>
#include <fstream>
#include <sstream>

#include "crypto.hpp"
#include "simdjson.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#include <cmath>

using namespace simdjson;

void pretty_print(const dom::element& e, std::ofstream& ofs, int indent = 0) {
    std::string prefix(indent, ' ');

    switch (e.type()) {
        case dom::element_type::OBJECT: {
            std::cout << "{\n";
            ofs << "{\n";
            bool first = true;
            for (const auto [key, value] : dom::object(e)) {
                if (!first) {
                    std::cout << ",\n";
                    ofs << ",\n";
                }
                std::cout << prefix << "  \"" << key << "\": ";
                ofs << prefix << "  \"" << key << "\": ";
                pretty_print(value, ofs, indent + 2);
                first = false;
            }
            std::cout << "\n" << prefix << "}";
            ofs << "\n" << prefix << "}";
            break;
        }
        case dom::element_type::ARRAY: {
            std::cout << "[\n";
            ofs << "[\n";
            bool first = true;
            for (const auto value : dom::array(e)) {
                if (!first) {
                    std::cout << ",\n";
                    ofs << ",\n";
                }
                std::cout << prefix << "  ";
                ofs << prefix << "  ";
                pretty_print(value, ofs, indent + 2);
                first = false;
            }
            std::cout << "\n" << prefix << "]";
            ofs << "\n" << prefix << "]";
            break;
        }
        case dom::element_type::STRING: {
            // get_string() 返回 simdjson::dom::string，可转为 string_view
            std::string_view sv = e.get_string();
            std::cout << "\"" << sv << "\"";
            ofs << "\"" << sv << "\"";
            break;
        }
        case dom::element_type::INT64:
            std::cout << e.get_int64();
            ofs << e.get_int64();
            break;
        case dom::element_type::UINT64:
            std::cout << e.get_uint64();
            ofs << e.get_uint64();
            break;
        case dom::element_type::DOUBLE:
            std::cout << e.get_double();
            ofs << e.get_double();
            break;
        case dom::element_type::BOOL:
            std::cout << (e.get_bool() ? "true" : "false");
            ofs << (e.get_bool() ? "true" : "false");
            break;
        case dom::element_type::NULL_VALUE:
            std::cout << "null";
            ofs << "null";
            break;
    }

    if (indent == 0) {
        std::cout << std::endl;
        ofs << std::endl;
    }
}

std::string loadDataFromFile(const std::string& sJsonFile) {
    std::ifstream file(sJsonFile);
    if (!file.is_open()) {
        std::cerr << "open file failed:" << sJsonFile.c_str() << std::endl;
        return sJsonFile;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sJsonData = buffer.str();
    file.close();

    return sJsonData;
}

std::string getEncryptDataFromString(const std::string& sJsonData) {
    simdjson::ondemand::parser stParser;
    simdjson::padded_string stString = simdjson::padded_string(sJsonData);
    simdjson::ondemand::document stDoc = stParser.iterate(stString);
    auto stObject = stDoc.get_object();
    if (stObject.error()) {
        std::cout << "getEncryptDataFromString error: " << stObject.error() << std::endl;
        return "";
    }
    for (auto sField : stObject) {
        std::string_view sKey = sField.unescaped_key();

        if (sKey == "encryptData") {
            std::string sEncryptData;
            sField.value().get(sEncryptData);
            return sEncryptData;
        }
    }
    return "";
}

void printUsage() {
    printf("Usage: JsonManager [method] [filename]\n");
    printf("\rmethod: [encode, decode, add]; filename default is settings.json or plain.txt\n");
    printf("\rsample: JsonManager encode settings.json\n");
    printf("\rsample: JsonManager add [input encoded string in file] [id]\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 0;
    }

    if (std::strcmp(argv[1], "encode") == 0) {
        std::string sInputFile = "../settings.json";
        if (argc == 3) {
            sInputFile = std::string(argv[2]);
        }
        std::string sPlainText = loadDataFromFile(sInputFile);
        std::string sEncryptData = Haige::HGCrypto::getInstance().rsaEncrypt(sPlainText, "../pubHGRecon.key");
        printf("{\"encryptData\": \"%s\"}\n", sEncryptData.c_str());
        std::string sDecodeData = Haige::HGCrypto::getInstance().rsaDecrypt(sEncryptData, "../privHGRecon.key");
        
        simdjson::dom::parser parser;
        simdjson::dom::element doc;
        auto error = parser.parse(sDecodeData).get(doc);
        if (error) {
            std::cerr << "JSON 解析错误: " << error << std::endl;
            return 1;
        }

        // size_t nLen = sInputFile.length();
        // std::string sOutputFile = sInputFile.substr(0, nLen - 4) + "txt";
        // std::ofstream ofs(sOutputFile);
        // pretty_print(doc, ofs);
        // ofs.close();
    } else if (std::strcmp(argv[1], "decode") == 0) {
        std::string sInputFile = "../plain3d.txt";
        if (argc == 3) {
            sInputFile = std::string(argv[2]);
        }
        std::string sEncryptData = loadDataFromFile(sInputFile);
        std::string sDecodeData = Haige::HGCrypto::getInstance().rsaDecrypt(sEncryptData, "../privHGRecon.key");
        
        simdjson::dom::parser parser;
        simdjson::dom::element doc;
        auto error = parser.parse(sDecodeData).get(doc);
        if (error) {
            std::cerr << "JSON 解析错误: " << error << std::endl;
            return 1;
        }
        size_t nLen = sInputFile.length();
        std::string sOutputFile = sInputFile.substr(0, nLen - 3) + "json";
        
        std::ofstream ofs(sOutputFile);
        pretty_print(doc, ofs);
        ofs.close();
    } else {
        if (argc < 4) {
            printUsage();
            return 1;
        }

        std::string sEncryptJsonFile = std::string(argv[2]);
        std::string sTaskId = std::string(argv[3]);

        std::string sEncryptJson = loadDataFromFile(sEncryptJsonFile);
        std::string sEncryptData = getEncryptDataFromString(sEncryptJson);
        std::string sDecodeData = Haige::HGCrypto::getInstance().rsaDecrypt(sEncryptData, "../privHGRecon.key");
        rapidjson::Document doc;
        doc.SetObject();
        doc.Parse(sDecodeData.c_str());

        rapidjson::Value val;
        val.SetString(sTaskId.c_str(), doc.GetAllocator());
        doc.AddMember("taskId", val, doc.GetAllocator());

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        
        std::string sModifiedData = buffer.GetString();
        std::cout << "new JSON: " << sModifiedData << std::endl;
        size_t nLen = sEncryptJsonFile.length();
        std::string sOutputFile = sEncryptJsonFile.substr(0, nLen - 3) + "json";
        
        std::ofstream ofs(sOutputFile);
        ofs << buffer.GetString();
        ofs.close();


        std::string sEncryptDataNew = Haige::HGCrypto::getInstance().rsaEncrypt(sModifiedData, "../pubHGRecon.key");
        printf("{\"encryptData\": \"%s\"}\n", sEncryptDataNew.c_str());
    }
    
    return 0;
}