CMAKE_MINIMUM_REQUIRED(VERSION 3.10.2)

# Force the correct values for a Linux system
set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
set(CMAKE_DL_LIBS "dl")
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

message(STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SHARED_LIBRARY_SUFFIX = '${CMAKE_SHARED_LIBRARY_SUFFIX}'")
message(STATUS "CMAKE_DL_LIBS = '${CMAKE_DL_LIBS}'")
get_property(supports GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS)
message(STATUS "TARGET_SUPPORTS_SHARED_LIBS = ${supports}")

project(JsonManager)

FILE(GLOB FILES_C *.cpp)
FILE(GLOB FILES_H *.h)
FILE(GLOB FILE_JSON ${CMAKE_CURRENT_SOURCE_DIR}/../Includes/simdjson.cpp)

include_directories(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/../Includes
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

add_executable(${PROJECT_NAME}
    ${FILES_C}
    ${FILES_H}
    ${FILE_JSON}
)

target_link_libraries(${PROJECT_NAME}
)
