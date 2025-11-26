CMAKE_MINIMUM_REQUIRED(VERSION 3.8.2)

project(MinIOManager)

set(CMAKE_TOOLCHAIN_FILE "/home/channy/Documents/thirdlibs/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_PREFIX_PATH "/home/channy/Documents/thirdlibs/vcpkg/installed/x64-linux;${CMAKE_PREFIX_PATH}")
set(OPENSSL_ROOT_DIR  "/home/channy/Documents/thirdlibs/vcpkg/installed/x64-linux/shared/openssl")

find_package(miniocpp CONFIG REQUIRED)

FILE(GLOB FILES_C "*.cpp" "*.cc")
FILE(GLOB FILES_H "*.h" "*.hpp" "*.inl")

include_directories(${PROJECT_NAME}
)

# Check C++20 support
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++20" COMPILER_SUPPORTS_CXX20)
if(COMPILER_SUPPORTS_CXX20)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
  add_definitions(-DCOMPILEDWITHC20)
  message(STATUS "Using flag -std=c++20.")
else()
  message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++20 support. Please use a different C++ compiler.")
endif()

link_directories(${PROJECT_NAME}
)

add_executable(${PROJECT_NAME}
    ${FILES_C}
    ${FILES_CC}
    ${FILES_H}
)

target_link_libraries(${PROJECT_NAME}
    pthread
    miniocpp::miniocpp
)
