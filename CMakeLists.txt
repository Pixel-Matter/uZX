cmake_minimum_required(VERSION 3.16)

set(TARGET_NAME MoTool)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(READ VERSION.md CURRENT_VERSION)
project(${TARGET_NAME} VERSION ${CURRENT_VERSION})
message ("Project name: ${PROJECT_NAME}, version: ${CURRENT_VERSION}")
add_subdirectory(third_party/tracktion_engine/modules/juce ./cmake_build_juce)
add_subdirectory(third_party/tracktion_engine/modules ./cmake_build_tracktion)

# Add our source
add_subdirectory(src)

# Add option to build examples
option(BUILD_TRACKTION_EXAMPLES "Build Tracktion Engine examples" ON)
if(BUILD_TRACKTION_EXAMPLES)
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/examples)
endif()
