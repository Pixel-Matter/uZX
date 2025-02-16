# File: tests/CMakeLists.txt

# Target Definition
set(TARGET_NAME MoToolTests)

# JUCE Application Setup
juce_add_console_app(${TARGET_NAME}
    PRODUCT_NAME "${TARGET_NAME}"
    COMPANY_NAME "Pixel Matter"
    BUNDLE_ID "org.pixelmatter.${TARGET_NAME}"
)

juce_generate_juce_header(${TARGET_NAME})

# Compiler and Language Settings
target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)

set_target_properties(${TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

# Source Files
set(TestSourceFiles
    main.cpp
    plugins/uXZ/AYChipTest.cpp
    # TestUtils.h
)

target_sources(${TARGET_NAME} PRIVATE ${TestSourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX Tests FILES ${TestSourceFiles})

# Include Directories
target_include_directories(${TARGET_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/examples
)

# Tracktion Utils
include("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tracktion_engine/tests/utils.cmake")

# Compile Definitions
set(JUCE_TEST_DEFINITIONS
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
)

target_compile_definitions(${TARGET_NAME} PRIVATE ${JUCE_TEST_DEFINITIONS})

# Dependencies
target_link_libraries(${TARGET_NAME} PRIVATE
    motool::uZX
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_warning_flags
)

# Platform-Specific Settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_compile_options(${TARGET_NAME} PRIVATE "-fno-aligned-allocation")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TARGET_NAME} PRIVATE "-latomic")
    if (NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "aarch64")
        target_link_options(${TARGET_NAME} PRIVATE "-m64")
    endif()
endif()

# Test-specific settings
enable_testing()
add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})