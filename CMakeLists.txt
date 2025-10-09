cmake_minimum_required(VERSION 3.16)

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

# If we are compiling for macOS we want to target OS versions down to 10.13
if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE INTERNAL "")
endif()

# Global project settings

file(READ VERSION.md CURRENT_VERSION_RAW)
string(STRIP "${CURRENT_VERSION_RAW}" CURRENT_VERSION)

set(MOTOOL_VERSION_BASE "${CURRENT_VERSION}")
set(MOTOOL_VERSION_SUFFIX "")
set(MOTOOL_VERSION_DISPLAY "${MOTOOL_VERSION_BASE}")
set(MOTOOL_GIT_COMMIT "")
set(MOTOOL_GIT_DIRTY 0)
set(MOTOOL_CHANNEL "dev" CACHE STRING "Release channel")
set_property(CACHE MOTOOL_CHANNEL PROPERTY STRINGS alpha beta rc demo dev)
string(TOLOWER "${MOTOOL_CHANNEL}" MOTOOL_CHANNEL_LOWER)
set(_motool_valid_channels alpha beta rc demo dev)
list(FIND _motool_valid_channels "${MOTOOL_CHANNEL_LOWER}" _motool_channel_index)
if(_motool_channel_index EQUAL -1)
    message(FATAL_ERROR "Invalid MoTool release channel: ${MOTOOL_CHANNEL}. Allowed values: alpha, beta, rc, demo, dev")
endif()
set(MOTOOL_CHANNEL "${MOTOOL_CHANNEL_LOWER}" CACHE STRING "Release channel" FORCE)
unset(_motool_valid_channels)
unset(_motool_channel_index)
string(TIMESTAMP MOTOOL_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

if(MOTOOL_CHANNEL)
    string(APPEND MOTOOL_VERSION_SUFFIX "-${MOTOOL_CHANNEL}")
    set(MOTOOL_VERSION_DISPLAY "${MOTOOL_VERSION_BASE}${MOTOOL_VERSION_SUFFIX}")
endif()

find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE MOTOOL_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE MOTOOL_GIT_COMMIT_RESULT)

    if(MOTOOL_GIT_COMMIT_RESULT EQUAL 0 AND MOTOOL_GIT_COMMIT)
        if(MOTOOL_VERSION_SUFFIX)
            string(APPEND MOTOOL_VERSION_SUFFIX "+g${MOTOOL_GIT_COMMIT}")
        else()
            set(MOTOOL_VERSION_SUFFIX "+g${MOTOOL_GIT_COMMIT}")
        endif()
        set(MOTOOL_VERSION_DISPLAY "${MOTOOL_VERSION_BASE}${MOTOOL_VERSION_SUFFIX}")
        set(MOTOOL_GIT_COMMIT "${MOTOOL_GIT_COMMIT}")

        execute_process(
            COMMAND ${GIT_EXECUTABLE} status --porcelain
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE MOTOOL_GIT_STATUS_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE MOTOOL_GIT_STATUS_RESULT)

        if(MOTOOL_GIT_STATUS_RESULT EQUAL 0 AND MOTOOL_GIT_STATUS_OUTPUT)
            string(APPEND MOTOOL_VERSION_SUFFIX "-dirty")
            set(MOTOOL_VERSION_DISPLAY "${MOTOOL_VERSION_BASE}${MOTOOL_VERSION_SUFFIX}")
            set(MOTOOL_GIT_DIRTY 1)
        endif()
    elseif(NOT MOTOOL_GIT_COMMIT_RESULT EQUAL 0)
        set(MOTOOL_GIT_COMMIT "unknown")
    endif()
endif()

project(MoTool VERSION ${MOTOOL_VERSION_BASE})
message("Project name: ${PROJECT_NAME}, version: ${PROJECT_VERSION}")
if(NOT MOTOOL_VERSION_DISPLAY STREQUAL PROJECT_VERSION)
    message(STATUS "Project build metadata version: ${MOTOOL_VERSION_DISPLAY}")
endif()

set(MOTOOL_VERSION_HEADER "${CMAKE_SOURCE_DIR}/src/version.h")
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/version.h.in
    ${MOTOOL_VERSION_HEADER}
    @ONLY
)
set_source_files_properties(${MOTOOL_VERSION_HEADER} PROPERTIES GENERATED TRUE)

enable_testing()
set(BUILD_TESTING ON CACHE BOOL "Enable CTest" FORCE)

option(ENABLE_LTO "Enable Link Time Optimisation (IPO) for optimized configs" ON)
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT motool_ipo_supported OUTPUT motool_ipo_error)
    if(motool_ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
        message(STATUS "IPO/LTO enabled for optimized configurations")
    else()
        message(WARNING "IPO/LTO requested but not supported: ${motool_ipo_error}")
    endif()
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
