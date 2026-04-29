cmake_minimum_required(VERSION 3.10)

project(plyReader)

FILE(GLOB FILES_C "*.cpp" "*.hpp")
FILE(GLOB FILES_H "*.h" "*.inl")

find_package(OpenSceneGraph REQUIRED osg osgViewer osgUtil osgDB osgGA osgText )

include_directories(${PROJECT_NAME}
${OpenSceneGraph_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Includes
)

add_library(${PROJECT_NAME}
    ${FILES_C} 
    ${FILES_H}
    ${OpenSceneGraph_LIBRARIES}
)
