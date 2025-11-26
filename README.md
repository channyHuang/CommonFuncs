# CommonFuncs
common functions such as multi-thread task, logging, etc...

## MinIOManager
依赖于minio-cpp，但minio-cpp直接使用CMake编译不通过，有很多依赖包无法直接apt install安装，需要vcpkg。而vcpkg的路径又需要特别指定。故CMakeLists.txt头有几句路径设置
```
set(CMAKE_TOOLCHAIN_FILE "/home/channy/Documents/thirdlibs/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_PREFIX_PATH "/home/channy/Documents/thirdlibs/vcpkg/installed/x64-linux;${CMAKE_PREFIX_PATH}")
set(OPENSSL_ROOT_DIR  "/home/channy/Documents/thirdlibs/vcpkg/installed/x64-linux/shared/openssl")
```
需要根据每个机器的vcpkg路径进行修改。

从ＭinIO上下载上传数据。

* MinIO的网页端和API端使用的端口号不一致。如果拿到的是网页端的地址，如`http://{ip}:{port}/browser/sBucket/sObject`格式的，需要先解析出`{ip}:{port}`作为endpoint，而API的端口号一般是网页的端口号-1，直接使用网页端口号下载上传会报错误连接不上什么的。

解析可使用正则表达式
```c++
int parseMinIOAddress(const std::string& sMinIOUrl, std::string &sEndpoint, std::string &sBucket, std::string &sObject, bool &https) {
    std::regex pattern(R"((https?)://([^/:]+)(?::(\d+))?/browser/([^/]+)/(.+))");
    std::smatch matches;
    if (!std::regex_match(sMinIOUrl, matches, pattern)) {
        std::cerr << "URL Error: " << sMinIOUrl << std::endl;
        return EXIT_FAILURE;
    }

    https = ((strcmp(matches[1].str().c_str(), "https") == 0)  ? true : false);
    int nPort = matches[3].matched ? std::stoi(matches[3].str()) - 1 : 9000;
    sEndpoint = matches[1].str() + "://" + matches[2].str() + ":" + std::to_string(nPort);
    sBucket = matches[4].str();
    std::string sObjectBase64 = matches[5].str();

    sObject = base64_decode(sObjectBase64);
    return EXIT_SUCCESS;
}
```

## ThreadPool 
带自定义更新进度的线程池

## httplib
依赖于httplib，http server 监听
