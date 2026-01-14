CMAKE_MINIMUM_REQUIRED(VERSION 3.8.2)

project(ProgressUpdate)
set(Lib True)

FILE(GLOB FILES_C "*.cpp" "*.cc")
FILE(GLOB FILES_H "*.h" "*.hpp" "*.inl")

find_path(HIREDIS_HEADER hiredis)
find_library(HIREDIS_LIB hiredis)
find_path(REDIS_PLUS_PLUS_HEADER sw)
find_library(REDIS_PLUS_PLUS_LIB redis++)

include_directories(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${HIREDIS_HEADER}
    ${REDIS_PLUS_PLUS_HEADER}
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

if (Lib) 
  add_library(${PROJECT_NAME} SHARED
    ${FILES_C} 
    ${FILES_H}
  )
else()
  add_executable(${PROJECT_NAME}
      ${FILES_C}
      ${FILES_CC}
      ${FILES_H}
  )
endif()

target_link_libraries(${PROJECT_NAME}
    pthread
    ${HIREDIS_LIB}
    ${REDIS_PLUS_PLUS_LIB}
)
