#pragma once

#include <string>
#include <iostream>
#include <vector>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include "base64.h"

namespace Haige {

class HGCrypto {
public:
    static HGCrypto& getInstance() {
        static HGCrypto m_pInstance;
        return m_pInstance;
    }

    HGCrypto(const HGCrypto&) = delete;
    HGCrypto& operator=(const HGCrypto&) = delete;

    void init() {
        ERR_load_crypto_strings();
        OpenSSL_add_all_algorithms();
    }

    void deinit() {
        EVP_cleanup();
        ERR_free_strings();
    }

    RSA* loadPublicKey(const std::string& sPubKeyFile) {
        BIO* bio = BIO_new_file(sPubKeyFile.c_str(), "r");
        if (!bio) {
            std::cerr << "Cannot open public key file: " << sPubKeyFile << std::endl;
            return nullptr;
        }
        RSA* rsa = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        if (!rsa) {
            std::cerr << "Failed to load public key." << std::endl;
            ERR_print_errors_fp(stderr);
        }
        return rsa;
    }

    RSA* loadPrivateKey(const std::string& sPrivKeyFile) {
        BIO* bio = BIO_new_file(sPrivKeyFile.c_str(), "r");
        if (!bio) {
            std::cerr << "Cannot open private key file: " << sPrivKeyFile << std::endl;
            return nullptr;
        }
        RSA* rsa = PEM_read_bio_RSAPrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        if (!rsa) {
            std::cerr << "Failed to load private key." << std::endl;
            ERR_print_errors_fp(stderr);
        }
        return rsa;
    }

    std::vector<unsigned char> base64Decode(const std::string& input) {
        // 移除空白字符（包括换行、空格等）
        std::string cleanInput;
        for (char c : input) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                cleanInput += c;
            }
        }

        // 计算解码后最大长度
        int len = static_cast<int>(cleanInput.size());
        if (len == 0) return {};

        // OpenSSL 要求输入为 null-terminated C string
        std::vector<char> b64Data(cleanInput.begin(), cleanInput.end());
        b64Data.push_back('\0');

        // 分配输出缓冲区（最大长度为 3/4 * 输入长度 + padding）
        std::vector<unsigned char> decoded(len); // 保守分配

        // 使用 EVP_DecodeBlock（不处理换行，需提前清理）
        int decodedLen = EVP_DecodeBlock(decoded.data(), 
                                        reinterpret_cast<const unsigned char*>(b64Data.data()), 
                                        len);
        if (decodedLen < 0) {
            std::cerr << "Base64 decode failed!" << std::endl;
            return {};
        }

        // 处理 padding：EVP_DecodeBlock 不自动减去 padding 字节
        // 统计 '=' 的数量来修正长度
        int padCount = 0;
        if (len >= 2) {
            if (cleanInput[len - 1] == '=') padCount++;
            if (cleanInput[len - 2] == '=') padCount++;
        }
        decodedLen -= padCount;

        decoded.resize(decodedLen);
        return decoded;
    }

    // = "./keys/privHGRecon.key"
    std::string rsaDecryptBlock(const unsigned char* pCipherBlock, size_t nBlockSize, RSA* pPrivKey, int nPrivKeySize) {
        
        std::vector<unsigned char> vPlainText(nPrivKeySize, '\0');
        // RSA decrypt
        int nResultLen = RSA_private_decrypt(
            static_cast<int>(nBlockSize),
            pCipherBlock,
            vPlainText.data(),
            pPrivKey,
            RSA_PKCS1_PADDING
        );
        // clean rsa
        if (nResultLen == -1) {
            std::cerr << "RSA decryption failed!" << std::endl;
            ERR_print_errors_fp(stderr);
            return "";
        }
        return std::string(reinterpret_cast<char*>(vPlainText.data()), nResultLen);    
    }

    std::string rsaDecrypt(const std::string& sMessage, const std::string& sPrivKeyFile) {
        std::vector<unsigned char> vCipherData = base64Decode(sMessage);

        const size_t nBlockSize = 256;
        if (vCipherData.size() % 256 != 0) {
            std::cerr << "Ciphertext length is not a multiple of 256 bytes." << vCipherData.size() << std::endl;
            return "";
        }

        RSA* pPrivKey = loadPrivateKey(sPrivKeyFile);
        if (!pPrivKey) {
            std::cerr << "Failed to load private key!" << std::endl;
            return "";
        }
        int nPrivKeySize = RSA_size(pPrivKey);

        size_t nNumBlocks = vCipherData.size() / nBlockSize;
        // std::cout << "cipher len = " << vCipherData.size() << ", nNumBlocks = " << nNumBlocks << std::endl;
        std::vector<unsigned char> vPlainText(nNumBlocks * 245);
        std::string sResult = "";
        for (size_t i = 0; i < nNumBlocks; ++i) {
            const unsigned char* pBlockBegin = vCipherData.data() + i * nBlockSize;

            std::string sPlain = rsaDecryptBlock(pBlockBegin, nBlockSize, pPrivKey, nPrivKeySize);
            // printf("block %u = [%s]\n", i, sPlain.c_str());
            sResult += sPlain;
        }

        RSA_free(pPrivKey);

        return sResult;
    }

    std::string base64Encode(const std::vector<unsigned char>& data) {
        if (data.empty()) {
            return "";
        }

        // Base64 编码后最大长度：每 3 字节输入 → 4 字节输出，向上取整 + 终止符
        size_t inputLen = data.size();
        size_t encodedLen = ((inputLen + 2) / 3) * 4; // 确保足够空间
        std::vector<char> encoded(encodedLen + 1);    // +1 for null terminator

        // 使用 EVP_EncodeBlock 进行编码（不会插入换行）
        int outputLen = EVP_EncodeBlock(
            reinterpret_cast<unsigned char*>(encoded.data()),
            data.data(),
            static_cast<int>(inputLen)
        );

        // EVP_EncodeBlock 返回的是不含 null terminator 的有效长度，
        // 但已保证 encoded[outputLen] == '\0'
        return std::string(encoded.data(), outputLen);
    }

    std::vector<unsigned char> rsaEncryptBlock(const unsigned char* pPlainBlock, size_t nPlainLen, RSA* pPubKey, int nKeySize) {
        std::vector<unsigned char> cipherText(nKeySize, 0);

        int resultLen = RSA_public_encrypt(
            static_cast<int>(nPlainLen),
            pPlainBlock,
            cipherText.data(),
            pPubKey,
            RSA_PKCS1_PADDING
        );

        if (resultLen == -1) {
            std::cerr << "RSA encryption failed for a block!" << std::endl;
            ERR_print_errors_fp(stderr);
            return {};
        }

        // 理论上 resultLen 应等于 nKeySize（如 256）
        return std::vector<unsigned char>(cipherText.begin(), cipherText.begin() + resultLen);
    }

    std::string rsaEncrypt(const std::string& sMessage, const std::string& sPubKeyFile) {
        if (sMessage.empty()) {
            return base64Encode(std::vector<unsigned char>{}); // 或直接返回 ""
        }

        RSA* pPubKey = loadPublicKey(sPubKeyFile);
        if (!pPubKey) {
            return "";
        }

        int nKeySize = RSA_size(pPubKey); // 通常为 256（对应 2048-bit）
        // PKCS#1 v1.5 最大明文长度 = nKeySize - 11
        const int MAX_PLAIN_BLOCK_SIZE = nKeySize - 11;

        if (MAX_PLAIN_BLOCK_SIZE <= 0) {
            std::cerr << "Invalid RSA key size!" << std::endl;
            RSA_free(pPubKey);
            return "";
        }

        const unsigned char* pData = reinterpret_cast<const unsigned char*>(sMessage.data());
        size_t totalLen = sMessage.size();
        std::vector<unsigned char> allCipher;

        size_t offset = 0;
        while (offset < totalLen) {
            size_t currentBlockSize = (totalLen - offset) > static_cast<size_t>(MAX_PLAIN_BLOCK_SIZE)
                                    ? static_cast<size_t>(MAX_PLAIN_BLOCK_SIZE)
                                    : totalLen - offset;

            auto encryptedBlock = rsaEncryptBlock(pData + offset, currentBlockSize, pPubKey, nKeySize);
            if (encryptedBlock.empty()) {
                RSA_free(pPubKey);
                return "";
            }

            allCipher.insert(allCipher.end(), encryptedBlock.begin(), encryptedBlock.end());
            offset += currentBlockSize;
        }

        RSA_free(pPubKey);

        // 返回 Base64 编码的密文
        return base64Encode(allCipher);
    }

private:
    HGCrypto() {
        init();
    }
    ~HGCrypto() {
        deinit();
    }
};

} // end of namespace Haige
