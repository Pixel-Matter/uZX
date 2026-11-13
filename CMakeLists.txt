cmake_minimum_required(VERSION 3.16)

# Enable CMAKE_MSVC_DEBUG_INFORMATION_FORMAT (requires CMP0141=NEW, available since CMake 3.25).
# This allows switching from /Zi (shared PDB) to /Z7 (embedded) for sccache compatibility.
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

# Default to Debug for single-config generators (Make, Ninja) when not specified.
# Multi-config generators (Xcode, Visual Studio) ignore CMAKE_BUILD_TYPE.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Debug")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Global compiler settings

# Address Sanitizer for debugging memory issues
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
if(ENABLE_ASAN)
    if(APPLE OR CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        message(STATUS "Address Sanitizer enabled")
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
endif()

# If we are using MSVC we want static runtime linkage
if (MSVC)
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE INTERNAL "")
    set (CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif()

# Global project settings

include(versions.cmake)

set(PROJECT_CHANNEL "alpha" CACHE STRING "Release channel; empty for a stable release")
set_property(CACHE PROJECT_CHANNEL PROPERTY STRINGS alpha beta rc demo dev)
string(TOLOWER "${PROJECT_CHANNEL}" PROJECT_CHANNEL_LOWER)
set(_version_valid_channels alpha beta rc demo dev)
if(PROJECT_CHANNEL_LOWER AND NOT PROJECT_CHANNEL_LOWER IN_LIST _version_valid_channels)
    message(FATAL_ERROR "Invalid release channel: ${PROJECT_CHANNEL}. Allowed values: alpha, beta, rc, demo, dev, or empty for stable")
endif()
set(PROJECT_CHANNEL "${PROJECT_CHANNEL_LOWER}" CACHE STRING "Release channel; empty for a stable release" FORCE)
unset(_version_valid_channels)

find_package(Git QUIET)

project(MoTool VERSION ${PROJECT_CORE_VERSION})
message("Project name: ${PROJECT_NAME}, version: ${PROJECT_VERSION}, channel: ${PROJECT_CHANNEL}")

enable_testing()
set(BUILD_TESTING ON CACHE BOOL "Enable CTest" FORCE)

option(ENABLE_LTO "Enable Link Time Optimisation (IPO) for optimized configs" ON)
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_error)
    if(_ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
        message(STATUS "IPO/LTO enabled for optimized configurations")
    else()
        message(WARNING "IPO/LTO requested but not supported: ${_ipo_error}")
    endif()
    unset(_ipo_supported)
    unset(_ipo_error)
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/ayumi)

# TAYM, the timer-tricks interchange format: its C reader and timer engine, as the format's own repository
# has them. That repository has no CMake of its own, and its frame walker and tests are not needed here.
add_library(taym STATIC
    ${CMAKE_SOURCE_DIR}/third_party/taym/c/src/taym.c
    ${CMAKE_SOURCE_DIR}/third_party/taym/c/src/taym_timers.c
    ${CMAKE_SOURCE_DIR}/third_party/taym/c/src/taym_ay.c
)
target_include_directories(taym PUBLIC ${CMAKE_SOURCE_DIR}/third_party/taym/c/include)
set_target_properties(taym PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/zxtune)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce ./cmake_build_juce)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules ./cmake_build_tracktion)

add_subdirectory(${CMAKE_SOURCE_DIR}/src)

# Add option to build examples
option(BUILD_TRACKTION_EXAMPLES "Build Tracktion Engine examples" OFF)
if(BUILD_TRACKTION_EXAMPLES)
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/examples)
    file(WRITE ${CMAKE_BINARY_DIR}/CTestCustom.cmake
        "set(CTEST_CUSTOM_TESTS_IGNORE Benchmarks TestRunner)\n")
endif()
