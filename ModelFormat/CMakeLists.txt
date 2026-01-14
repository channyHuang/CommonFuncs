CMAKE_MINIMUM_REQUIRED(VERSION 3.8.2)

project(ModelFormat)

find_package(OpenSceneGraph REQUIRED osg osgViewer osgUtil osgDB osgGA osgText)

set(EIGEN_DIR "/usr/local/include/eigen3")
find_package(Assimp REQUIRED)
find_package(glm REQUIRED)
find_package(GDAL CONFIG REQUIRED)
add_subdirectory(plySpecial)
add_subdirectory(meshoptimizer)

FILE(GLOB FILES_C "*.cpp" "*.cc")
FILE(GLOB FILES_H "*.h" "*.hpp" "*.inl")

include_directories(${PROJECT_NAME}
  ${EIGEN_DIR}
  ${ASSIMP_INCLUDE_DIRS}
    ${OpenSceneGraph_INCLUDE_DIRS}
    ${GDAL_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/meshoptimizer
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
    ${FILES_H}
)

target_link_libraries(${PROJECT_NAME}
    pthread
    ${ASSIMP_LIBRARIES}  
    ${OPENSCENEGRAPH_LIBRARIES}
    ${GLM_LIBRARIES}
    meshoptimizer
    gdal
    plySpecial
)
