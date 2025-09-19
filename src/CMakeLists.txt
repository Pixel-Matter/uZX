# File src/CMakeLists.txt

# Target Definition
set(MAIN_TARGET_NAME uZX)
set(TUNING_TARGET_NAME uZXTuning)

# IDE and Module Settings
set_property(GLOBAL PROPERTY USE_FOLDERS YES)
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Enable Module Source Groups" ON)

# JUCE Application Setup
juce_add_gui_app(${MAIN_TARGET_NAME}
    PRODUCT_NAME "μZX"
    COMPANY_NAME "Pixel Matter"
    BUNDLE_ID "org.pixelmatter.${MAIN_TARGET_NAME}"
)

# JUCE Tuning Application Setup
juce_add_gui_app(${TUNING_TARGET_NAME}
    PRODUCT_NAME "μZXTuning"
    COMPANY_NAME "Pixel Matter"
    BUNDLE_ID "org.pixelmatter.${TUNING_TARGET_NAME}"
)

juce_generate_juce_header(${MAIN_TARGET_NAME})
juce_generate_juce_header(${TUNING_TARGET_NAME})

# Compiler and Language Settings
target_compile_features(${MAIN_TARGET_NAME} PRIVATE cxx_std_23)
target_compile_features(${TUNING_TARGET_NAME} PRIVATE cxx_std_23)

set_target_properties(${MAIN_TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

set_target_properties(${TUNING_TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

include(sources.cmake)

target_sources(${MAIN_TARGET_NAME} PRIVATE
    ${SHARED_SOURCES}
    ${GUI_SOURCES}
)

target_sources(${TUNING_TARGET_NAME} PRIVATE
    ${SHARED_SOURCES}
    ${GUI_SOURCES}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/.. PREFIX Source FILES ${SHARED_SOURCES} ${GUI_SOURCES})

# Include Directories
target_include_directories(${MAIN_TARGET_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/examples
)

target_include_directories(${TUNING_TARGET_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/examples
)

# Tracktion Utils
# include("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tracktion_engine/tests/utils.cmake")
# addModuleSourceTarget()

# Compile Definitions
set(JUCE_DEFINITIONS
    JUCE_USE_FLAC=1       # Needed for Tracktion
    JUCE_USE_OGGVORBIS=1  # Needed for Tracktion

    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_VST3=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_LAME_AUDIO_FORMAT=0
    JUCE_USE_CDBURNER=0
    JUCE_USE_CDREADER=0
    JUCE_USE_CAMERA=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1

    TRACKTION_ENABLE_CONTROL_SURFACES=0
    TRACKTION_ENABLE_VIDEO=0
    TRACKTION_ENABLE_ARA=0
    TRACKTION_ENABLE_REWIRE=0
    TRACKTION_ENABLE_AUTOMAP=0
    TRACKTION_ENABLE_REX=0
    TRACKTION_ENABLE_CMAJOR=0
    TRACKTION_ENABLE_ABLETON_LINK=0
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=0
    TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE=0
    TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND=0
    TRACKTION_ENABLE_FFMPEG=0
    TRACKTION_ENABLE_CONTROL_SURFACE_MACKIEC4=0
    TRACKTION_ENABLE_SINGLETONS=0
)

target_compile_definitions(${MAIN_TARGET_NAME} PRIVATE ${JUCE_DEFINITIONS})
target_compile_definitions(${TUNING_TARGET_NAME} PRIVATE ${JUCE_DEFINITIONS})

# Dependencies
target_link_libraries(${MAIN_TARGET_NAME} PRIVATE
    ayumi
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_warning_flags
)

target_link_libraries(${TUNING_TARGET_NAME} PRIVATE
    ayumi
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_warning_flags
)

# Platform-Specific Settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_compile_options(${MAIN_TARGET_NAME} PRIVATE "-fno-aligned-allocation")
    target_compile_options(${TUNING_TARGET_NAME} PRIVATE "-fno-aligned-allocation")

    # Code Signing Configuration
    # Set from environment variable or use default
    if(DEFINED ENV{CODESIGN_IDENTITY})
        set(CODESIGN_IDENTITY "$ENV{CODESIGN_IDENTITY}")
        message(STATUS "Code signing with identity: ${CODESIGN_IDENTITY}")

        # Create codesign script to handle identity with spaces and parentheses
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/codesign.sh.in"
            "${CMAKE_CURRENT_BINARY_DIR}/codesign.sh"
            @ONLY
        )

        # Make the script executable
        file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/codesign.sh" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

        # Add custom command to sign μZX after build
        add_custom_command(TARGET ${MAIN_TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/codesign.sh $<TARGET_BUNDLE_DIR:${MAIN_TARGET_NAME}>
            COMMENT "Code signing ${MAIN_TARGET_NAME}"
        )

        # Add custom command to sign μZXTuning after build
        add_custom_command(TARGET ${TUNING_TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/codesign.sh $<TARGET_BUNDLE_DIR:${TUNING_TARGET_NAME}>
            COMMENT "Code signing ${TUNING_TARGET_NAME}"
        )
    else()
        message(STATUS "Code signing disabled. Set CODESIGN_IDENTITY environment variable to enable.")
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${MAIN_TARGET_NAME} PRIVATE "-latomic")
    target_link_libraries(${TUNING_TARGET_NAME} PRIVATE "-latomic")
    if (NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "aarch64")
        target_link_options(${MAIN_TARGET_NAME} PRIVATE "-m64")
        target_link_options(${TUNING_TARGET_NAME} PRIVATE "-m64")
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