# File: tests/CMakeLists.txt

set(TARGET_NAME MoToolTests)

juce_add_console_app(${TARGET_NAME})
juce_generate_juce_header(${TARGET_NAME})

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)

set_target_properties(${TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

function(get_full_paths output_var base_dir)
    set(result)
    foreach(source ${ARGN})
        list(APPEND result "${base_dir}/${source}")
    endforeach()
    set(${output_var} ${result} PARENT_SCOPE)
endfunction()

include(${CMAKE_SOURCE_DIR}/src/sources.cmake)
get_full_paths(SHARED_SOURCES ${CMAKE_SOURCE_DIR}/src ${SHARED_SOURCES})
get_full_paths(TEST_SOURCES ${CMAKE_SOURCE_DIR}/src ${TEST_SOURCES})
get_full_paths(GUI_SOURCES ${CMAKE_SOURCE_DIR}/src ${GUI_SOURCES})

# Include the same source files as the main app
target_sources(${TARGET_NAME} PRIVATE
    ${SHARED_SOURCES}
    ${TEST_SOURCES}
    main.cpp
)

source_group(TREE ${CMAKE_SOURCE_DIR} PREFIX "Source Files" FILES ${SHARED_SOURCES} ${GUI_SOURCES} ${TEST_SOURCES})

# Include Directories
target_include_directories(${TARGET_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/ayumi
    ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/examples
)

# Dependencies
target_link_libraries(${TARGET_NAME} PRIVATE
    ayumi
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_warning_flags
)

# Tracktion Utils
# include("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tracktion_engine/tests/utils.cmake")

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
add_test(NAME AllTests COMMAND ${TARGET_NAME})