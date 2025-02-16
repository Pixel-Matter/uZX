# Target Definition
set(TARGET_NAME MoTool)

# IDE and Module Settings
set_property(GLOBAL PROPERTY USE_FOLDERS YES)
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Enable Module Source Groups" ON)

# JUCE Application Setup
juce_add_gui_app(${TARGET_NAME}
    PRODUCT_NAME "${PROJECT_NAME}"
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
set(SourceFiles
    gui/Main.cpp
    gui/nodes/GraphEditorPanel.cpp
    gui/common/Components.cpp
    gui/timeline/EditComponent.cpp
    plugins/uZX/aychip/aychip.cpp
    util/FileOps.cpp
    util/Midi.cpp
    plugins/uZX/aychip/aychip.cpp
)

target_sources(${TARGET_NAME} PRIVATE ${SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/.. PREFIX Source FILES ${SourceFiles})

# Include Directories
target_include_directories(${TARGET_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/examples
)

# Tracktion Utils
include("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tracktion_engine/tests/utils.cmake")
addModuleSourceTarget()

# Compile Definitions
set(JUCE_DEFINITIONS
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
)

target_compile_definitions(${TARGET_NAME} PRIVATE ${JUCE_DEFINITIONS})

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

# Commented sections moved to bottom for reference
# # VST2 SDK Configuration
# if (DEFINED ENV{VST2_SDK_DIR})
#     MESSAGE(STATUS "Building with VST2 SDK: $ENV{VST2_SDK_DIR}")
#     juce_set_vst2_sdk_path($ENV{VST2_SDK_DIR})
# else()
#     MESSAGE(STATUS "Not building with VST2")
# endif()
# if (DEFINED ENV{VST2_SDK_DIR})
#     list(APPEND JUCE_DEFINITIONS JUCE_PLUGINHOST_VST=1)
# endif()
#
# # Binary Data Resources
# juce_add_binary_data (${TARGET_NAME}BinaryData SOURCES
#     resources/edm_song.ogg
#     resources/drum_loop.wav
#     resources/synth_loop.wav
#     resources/midi_arp.mid
#     resources/midi_arp_2.mid)

# # macOS Bundle Properties
# set_target_properties(${TARGET_NAME} PROPERTIES
# MACOSX_BUNDLE TRUE
# MACOSX_BUNDLE_GUI_IDENTIFIER "${JUCE_BUNDLE_ID}"
# MACOSX_BUNDLE_BUNDLE_NAME "${JUCE_PRODUCT_NAME}"
# MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
# MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
# )

# # Release Build Warnings
# if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
#     CMAKE_CXX_COMPILER_ID MATCHES "GNU")
#     target_compile_options(${TARGET_NAME} PRIVATE "$<$<CONFIG:Release>:-Werror>")
# elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
#     target_compile_options(${TARGET_NAME} PRIVATE "/W4")
#     target_compile_options(${TARGET_NAME} PRIVATE "$<$<CONFIG:Release>:/WX>")
# endif()