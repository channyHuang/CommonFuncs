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
## ProgressUpdate
根据sTaskId更新进度到redis上

## ThreadPool 
带自定义更新进度的线程池

## httplib
依赖于httplib，http server 监听

## ModelFormat
三维模型间的相互转换，其中
* gltf -> tinygltf 不同版本的接口有一些差异
* osg/osgb -> OpenSceneGraph

### 3d tiles文件生成
[gltf 2.0 (2022)](https://www.khronos.org/gltf/)

[tinygltf](https://github.com/syoyo/tinygltf)

### 3d tiles模型本地查看

1. 安装[node.js](https://nodejs.org/en/download/)
2. 安装http-server
```sh
npm install http-server -g
```
3. 在模型根文件夹下启动
```sh
http-server -a localhost -p 8003 --cors=http://localhost:8080/
```
4. 根目录
```sh
|--- root
    |--- xxx.b3dm
    |--- xxx.b3dm
    |--- tileset.json
    |--- index.html
    |--- favicon.ico
    |--- .....
```

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>基础 3D 查看器</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/cesium/1.92.0/Cesium.js"></script>
    <link href="https://cdnjs.cloudflare.com/ajax/libs/cesium/1.92.0/Widgets/widgets.css" rel="stylesheet">
    <style>
        #cesiumContainer { width: 100%; height: 100vh; }
        body { margin: 0; font-family: Arial, sans-serif; }
    </style>
</head>
<body>
    <div id="cesiumContainer"></div>
    <script>
        console.log("初始化基础 Cesium 查看器...");
        
        // 最基本的初始化
        const viewer = new Cesium.Viewer('cesiumContainer', {
            terrainProvider: undefined, // 明确设置为 undefined
            baseLayerPicker: false,
            geocoder: false,
            homeButton: false,
            sceneModePicker: false,
            navigationHelpButton: false,
            animation: false,
            timeline: false
        });
        
        console.log("Viewer 创建成功");
        
        // 直接加载模型，不依赖 readyPromise
        setTimeout(() => {
            console.log("开始加载模型...");
            try {
                const tileset = viewer.scene.primitives.add(
                    new Cesium.Cesium3DTileset({
                        url: './tileset.json'
                    })
                );
                
                console.log("Tileset 已添加到场景");
                
                // 5秒后尝试缩放
                setTimeout(() => {
                    viewer.zoomTo(tileset).catch(e => {
                        console.log("缩放失败，但模型可能已加载");
                    });
                }, 5000);
                
            } catch (e) {
                console.error("加载模型失败:", e);
            }
        }, 1000);
    </script>
</body>
</html>
```
