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

file(READ VERSION.md CURRENT_VERSION)
project(MoTool VERSION ${CURRENT_VERSION})
message ("Project name: ${PROJECT_NAME}, version: ${CURRENT_VERSION}")

enable_testing()
set(BUILD_TESTING ON CACHE BOOL "Enable CTest" FORCE)

# Enable LTO/IPO for optimized builds when supported
include(CheckIPOSupported)
check_ipo_supported(RESULT motool_ipo_supported OUTPUT motool_ipo_error)
if(motool_ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
else()
    message(STATUS "IPO/LTO not enabled: ${motool_ipo_error}")
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/ayumi)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce ./cmake_build_juce)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules ./cmake_build_tracktion)

add_subdirectory(${CMAKE_SOURCE_DIR}/src)

# Add option to build examples
option(BUILD_TRACKTION_EXAMPLES "Build Tracktion Engine examples" ON)
if(BUILD_TRACKTION_EXAMPLES)
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/examples)
    file(WRITE ${CMAKE_BINARY_DIR}/CTestCustom.cmake
        "set(CTEST_CUSTOM_TESTS_IGNORE Benchmarks TestRunner)\n")
endif()
