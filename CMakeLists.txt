cmake_minimum_required(VERSION 3.16)

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

set(PROJECT_VERSION_BASE "${PROJECT_CORE_VERSION}")
set(PROJECT_VERSION_SUFFIX "")
set(PROJECT_VERSION_DISPLAY "${PROJECT_VERSION_BASE}")
set(PROJECT_GIT_COMMIT "")
set(PROJECT_GIT_DIRTY 0)
set(PROJECT_CHANNEL "dev" CACHE STRING "Release channel")
set_property(CACHE PROJECT_CHANNEL PROPERTY STRINGS alpha beta rc demo dev)
string(TOLOWER "${PROJECT_CHANNEL}" PROJECT_CHANNEL_LOWER)
set(_version_valid_channels alpha beta rc demo dev)
list(FIND _version_valid_channels "${PROJECT_CHANNEL_LOWER}" _version_channel_index)
if(_version_channel_index EQUAL -1)
    message(FATAL_ERROR "Invalid release channel: ${PROJECT_CHANNEL}. Allowed values: alpha, beta, rc, demo, dev")
endif()
set(PROJECT_CHANNEL "${PROJECT_CHANNEL_LOWER}" CACHE STRING "Release channel" FORCE)
unset(_version_valid_channels)
unset(_version_channel_index)
string(TIMESTAMP VERSION_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

if(PROJECT_CHANNEL)
    string(APPEND PROJECT_VERSION_SUFFIX "-${PROJECT_CHANNEL}")
    set(PROJECT_VERSION_DISPLAY "${PROJECT_VERSION_BASE}${PROJECT_VERSION_SUFFIX}")
endif()

find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE PROJECT_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE PROJECT_GIT_COMMIT_RESULT)

    if(PROJECT_GIT_COMMIT_RESULT EQUAL 0 AND PROJECT_GIT_COMMIT)
        if(PROJECT_CHANNEL STREQUAL "dev")
            if(PROJECT_VERSION_SUFFIX)
                string(APPEND PROJECT_VERSION_SUFFIX "+g${PROJECT_GIT_COMMIT}")
            else()
                set(PROJECT_VERSION_SUFFIX "+g${PROJECT_GIT_COMMIT}")
            endif()
            set(PROJECT_VERSION_DISPLAY "${PROJECT_VERSION_BASE}${PROJECT_VERSION_SUFFIX}")
        endif()
        set(PROJECT_GIT_COMMIT "${PROJECT_GIT_COMMIT}")

        execute_process(
            COMMAND ${GIT_EXECUTABLE} status --porcelain
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE PROJECT_GIT_STATUS_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE PROJECT_GIT_STATUS_RESULT)

        if(PROJECT_GIT_STATUS_RESULT EQUAL 0 AND PROJECT_GIT_STATUS_OUTPUT)
            string(APPEND PROJECT_VERSION_SUFFIX "-dirty")
            set(PROJECT_VERSION_DISPLAY "${PROJECT_VERSION_BASE}${PROJECT_VERSION_SUFFIX}")
            set(PROJECT_GIT_DIRTY 1)
        endif()
    elseif(NOT PROJECT_GIT_COMMIT_RESULT EQUAL 0)
        set(PROJECT_GIT_COMMIT "unknown")
    endif()
endif()

project(MoTool VERSION ${PROJECT_VERSION_BASE})
message("Project name: ${PROJECT_NAME}, version: ${PROJECT_VERSION}")
if(NOT PROJECT_VERSION_DISPLAY STREQUAL PROJECT_VERSION)
    message(STATUS "Project build metadata version: ${PROJECT_VERSION_DISPLAY}")
endif()

set(VERSION_FILE_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/version.h.in")
set(VERSION_FILE_HEADER "${CMAKE_SOURCE_DIR}/src/version.h")
set(_version_command
    ${CMAKE_COMMAND}
    "-DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DINPUT_TEMPLATE=${VERSION_FILE_TEMPLATE}"
    "-DOUTPUT_FILE=${VERSION_FILE_HEADER}"
    "-DPROJECT_VERSION_BASE=${PROJECT_VERSION_BASE}"
    "-DPROJECT_CHANNEL=${PROJECT_CHANNEL}"
)
if(GIT_EXECUTABLE)
    list(APPEND _version_command "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}")
endif()
list(APPEND _version_command "-P" "${CMAKE_SOURCE_DIR}/cmake/GenerateVersionHeader.cmake")

add_custom_target(generate_version_header
    COMMAND ${_version_command}
    BYPRODUCTS ${VERSION_FILE_HEADER}
    DEPENDS
        ${VERSION_FILE_TEMPLATE}
        ${CMAKE_SOURCE_DIR}/cmake/GenerateVersionHeader.cmake
        ${CMAKE_SOURCE_DIR}/versions.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating version header"
    VERBATIM
)
unset(_version_command)

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
