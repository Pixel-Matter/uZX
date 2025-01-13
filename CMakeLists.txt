cmake_minimum_required(VERSION 3.16)

set(TARGET_NAME MoTool)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(READ VERSION.md CURRENT_VERSION)
project(${TARGET_NAME} VERSION ${CURRENT_VERSION})
message ("Project name: ${PROJECT_NAME}, version: ${CURRENT_VERSION}")
add_subdirectory(third_party/tracktion_engine/modules/juce ./cmake_build_juce)
add_subdirectory(third_party/tracktion_engine/modules ./cmake_build_tracktion)

# Add our source
add_subdirectory(src)
