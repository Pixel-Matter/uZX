# File src/CMakeLists.txt

# Target Definition
set(STUDIO_TARGET_NAME uZX)
set(TUNING_TARGET_NAME uZXTuning)
set(PLAYER_TARGET_NAME uZXPlayer)
set(TEST_TARGET_NAME uZXTests)

# IDE and Module Settings
set_property(GLOBAL PROPERTY USE_FOLDERS YES)
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Enable Module Source Groups" ON)

# JUCE Application Setup
juce_add_gui_app(${STUDIO_TARGET_NAME}
    VERSION "${UZX_STUDIO_VERSION}"
    PRODUCT_NAME "μZX Studio"
    COMPANY_NAME "Pixel Matter"
    BUNDLE_ID "org.pixelmatter.${STUDIO_TARGET_NAME}"
    ICON_BIG "${CMAKE_SOURCE_DIR}/resources/icon_studio_big.png"
    ICON_SMALL "${CMAKE_SOURCE_DIR}/resources/icon_studio_small.png"
    DOCUMENT_EXTENSIONS uzx
)

# JUCE Tuning Application Setup
juce_add_gui_app(${TUNING_TARGET_NAME}
    VERSION "${UZX_TUNING_VERSION}"
    PRODUCT_NAME "μZX Tuning"
    COMPANY_NAME "Pixel Matter"
    BUNDLE_ID "org.pixelmatter.${TUNING_TARGET_NAME}"
)

# The Player's file types on macOS. The rank tells Finder how strong its claim is: the default app
# for .psg and .taym; for tracker modules (zxtune's players or a tracker may be the user's choice) and for .uzx
# (Studio's), an alternate one, which opens them only when no app claims them more strongly. Not
# DOCUMENT_EXTENSIONS: JUCE gives all of those one entry ranked Default.
# The modules are getTrackerModuleExtensions() (src/formats/tracker/TrackerModule.cpp) without
# early Vortex Tracker's .txt, so that the Player isn't offered for every text file.
set(PLAYER_MODULE_EXTENSIONS pt3 pt2 pt1 stc st1 st3 stp asc as0 sqt psc psm gtr ftc vt2)
list(TRANSFORM PLAYER_MODULE_EXTENSIONS PREPEND "<string>")
list(TRANSFORM PLAYER_MODULE_EXTENSIONS APPEND "</string>")
list(JOIN PLAYER_MODULE_EXTENSIONS "" PLAYER_MODULE_EXTENSIONS)
set(PLAYER_DOCUMENT_TYPES "<plist><dict><key>CFBundleDocumentTypes</key><array>
  <dict><key>CFBundleTypeName</key><string>PSG File</string>
    <key>CFBundleTypeExtensions</key><array><string>psg</string></array>
    <key>CFBundleTypeRole</key><string>Viewer</string><key>LSHandlerRank</key><string>Default</string></dict>
  <dict><key>CFBundleTypeName</key><string>TAYM File</string>
    <key>CFBundleTypeExtensions</key><array><string>taym</string></array>
    <key>CFBundleTypeRole</key><string>Viewer</string><key>LSHandlerRank</key><string>Default</string></dict>
  <dict><key>CFBundleTypeName</key><string>Tracker Module</string>
    <key>CFBundleTypeExtensions</key><array>${PLAYER_MODULE_EXTENSIONS}</array>
    <key>CFBundleTypeRole</key><string>Viewer</string><key>LSHandlerRank</key><string>Alternate</string></dict>
  <dict><key>CFBundleTypeName</key><string>µZX Project</string>
    <key>CFBundleTypeExtensions</key><array><string>uzx</string></array>
    <key>CFBundleTypeRole</key><string>Editor</string><key>LSHandlerRank</key><string>Alternate</string></dict>
</array></dict></plist>")

# JUCE Player Application Setup
juce_add_gui_app(${PLAYER_TARGET_NAME}
    VERSION "${UZX_PLAYER_VERSION}"
    PRODUCT_NAME "μZX Player"
    COMPANY_NAME "Pixel Matter"
    BUNDLE_ID "org.pixelmatter.${PLAYER_TARGET_NAME}"
    ICON_BIG "${CMAKE_SOURCE_DIR}/resources/icon_player_big.png"
    ICON_SMALL "${CMAKE_SOURCE_DIR}/resources/icon_player_small.png"
    PLIST_TO_MERGE "${PLAYER_DOCUMENT_TYPES}"
)

juce_add_console_app(${TEST_TARGET_NAME})

juce_generate_juce_header(${STUDIO_TARGET_NAME})
juce_generate_juce_header(${TUNING_TARGET_NAME})
juce_generate_juce_header(${PLAYER_TARGET_NAME})
juce_generate_juce_header(${TEST_TARGET_NAME})

# Compiler and Language Settings
target_compile_features(${STUDIO_TARGET_NAME} PRIVATE cxx_std_23)
target_compile_features(${TUNING_TARGET_NAME} PRIVATE cxx_std_23)
target_compile_features(${PLAYER_TARGET_NAME} PRIVATE cxx_std_23)
target_compile_features(${TEST_TARGET_NAME} PRIVATE cxx_std_23)

set_target_properties(${STUDIO_TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

set_target_properties(${TUNING_TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

set_target_properties(${PLAYER_TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

set_target_properties(${TEST_TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

include(sources.cmake)

add_library(motool_common STATIC)
target_sources(motool_common PRIVATE
    ${SHARED_SOURCES}
    ${GUI_SOURCES}
)
target_compile_features(motool_common PUBLIC cxx_std_23)
# Tracker module import: only zxtune's uzx/ay_module.h is visible here.
target_link_libraries(motool_common PRIVATE zxtune_aym)
# TAYM: its headers are in the models' and the AY chip's own, so everything that includes those sees them.
target_link_libraries(motool_common PUBLIC taym)
target_include_directories(motool_common
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party/ayumi
        ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/examples
        ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules
        ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules
        # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/VST3_SDK
        # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK
        # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/lv2
        # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/serd
        # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/sord
        # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/sord/src
        # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/sratom
        # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/lilv
        # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/lilv/src
        ${CMAKE_BINARY_DIR}/src/${STUDIO_TARGET_NAME}_artefacts/JuceLibraryCode
        ${CMAKE_BINARY_DIR}/src/${TUNING_TARGET_NAME}_artefacts/JuceLibraryCode
        ${CMAKE_BINARY_DIR}/src/${PLAYER_TARGET_NAME}_artefacts/JuceLibraryCode
        ${CMAKE_BINARY_DIR}/src/${TEST_TARGET_NAME}_artefacts/JuceLibraryCode
)

target_sources(${STUDIO_TARGET_NAME} PRIVATE
    controllers/Main.cpp
)

target_sources(${TUNING_TARGET_NAME} PRIVATE
    controllers/Main.cpp
)

target_sources(${PLAYER_TARGET_NAME} PRIVATE
    controllers/Main.cpp
)


# Per-target ID so Main.cpp can resolve the app target at link time.
# motool_common cannot use ProjectInfo::projectName because it is compiled
# once with the first JuceHeader found (uZX Studio).
target_compile_definitions(${STUDIO_TARGET_NAME} PRIVATE MOTOOL_APP_TARGET=0 MOTOOL_TARGET_VERSION="${UZX_STUDIO_VERSION}")
target_compile_definitions(${TUNING_TARGET_NAME} PRIVATE MOTOOL_APP_TARGET=1 MOTOOL_TARGET_VERSION="${UZX_TUNING_VERSION}")
target_compile_definitions(${PLAYER_TARGET_NAME} PRIVATE MOTOOL_APP_TARGET=2 MOTOOL_TARGET_VERSION="${UZX_PLAYER_VERSION}")

target_sources(${TEST_TARGET_NAME} PRIVATE
    ${TEST_SOURCES}
    ${CMAKE_SOURCE_DIR}/tests/main.cpp
)
# zxtune's sample modules with its register dumps, for the TrackerModule tests.
target_compile_definitions(${TEST_TARGET_NAME} PRIVATE
    UZX_ZXTUNE_REGRESSION_DIR="${CMAKE_SOURCE_DIR}/third_party/zxtune/regression")
# The TAYM repository's example tunes, for the TAYM import tests.
target_compile_definitions(${TEST_TARGET_NAME} PRIVATE
    UZX_TAYM_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/third_party/taym/examples")

set(PROJECT_DISCOVERY_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/DiscoverJuceTests.cmake")
set(PROJECT_DISCOVERY_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TEST_TARGET_NAME}_discovered.cmake")

if (NOT EXISTS "${PROJECT_DISCOVERY_FILE}")
    file(WRITE "${PROJECT_DISCOVERY_FILE}"
        "# Placeholder. Populated after building ${TEST_TARGET_NAME}.\n")
endif()

set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${PROJECT_DISCOVERY_FILE}")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/.. PREFIX Source FILES
    ${SHARED_SOURCES}
    ${GUI_SOURCES}
    ${TEST_SOURCES}
)

# Include Directories
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/ayumi
    ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/examples
    ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules
    ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules
    # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/VST3_SDK
    # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK
    # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/lv2
    # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/serd
    # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/sord
    # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/sord/src
    # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/sratom
    # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/lilv
    # ${CMAKE_SOURCE_DIR}/third_party/tracktion_engine/modules/juce/modules/juce_audio_processors/format_types/LV2_SDK/lilv/src
    ${CMAKE_BINARY_DIR}/src/${STUDIO_TARGET_NAME}_artefacts/JuceLibraryCode
    ${CMAKE_BINARY_DIR}/src/${TUNING_TARGET_NAME}_artefacts/JuceLibraryCode
    ${CMAKE_BINARY_DIR}/src/${PLAYER_TARGET_NAME}_artefacts/JuceLibraryCode
    ${CMAKE_BINARY_DIR}/src/${TEST_TARGET_NAME}_artefacts/JuceLibraryCode
)

target_include_directories(${STUDIO_TARGET_NAME} PRIVATE ${PROJECT_INCLUDE_DIRS})
target_include_directories(${TUNING_TARGET_NAME} PRIVATE ${PROJECT_INCLUDE_DIRS})
target_include_directories(${PLAYER_TARGET_NAME} PRIVATE ${PROJECT_INCLUDE_DIRS})
target_include_directories(${TEST_TARGET_NAME}   PRIVATE ${PROJECT_INCLUDE_DIRS})

# TODO maybe we should make this motool_common library juce modules?
# Ensure JUCE headers exist before building our common static library
set(PROJECT_JUCE_HEADER_STUDIO
    ${CMAKE_BINARY_DIR}/src/${STUDIO_TARGET_NAME}_artefacts/JuceLibraryCode/JuceHeader.h
)
set(PROJECT_JUCE_HEADER_TUNING
    ${CMAKE_BINARY_DIR}/src/${TUNING_TARGET_NAME}_artefacts/JuceLibraryCode/JuceHeader.h
)
set(PROJECT_JUCE_HEADER_PLAYER
    ${CMAKE_BINARY_DIR}/src/${PLAYER_TARGET_NAME}_artefacts/JuceLibraryCode/JuceHeader.h
)
set(PROJECT_JUCE_HEADER_TESTS
    ${CMAKE_BINARY_DIR}/src/${TEST_TARGET_NAME}_artefacts/JuceLibraryCode/JuceHeader.h
)

add_custom_target(project_generate_juce_headers
    DEPENDS
        ${PROJECT_JUCE_HEADER_STUDIO}
        ${PROJECT_JUCE_HEADER_TUNING}
        ${PROJECT_JUCE_HEADER_PLAYER}
        ${PROJECT_JUCE_HEADER_TESTS}
)

add_dependencies(motool_common project_generate_juce_headers)

# Build metadata (git hash, dirty flag, build time) lives in a generated version.cpp in its own tiny library,
# so a new stamp costs a relink, not a recompile. The stamp is rewritten only when the code (motool_common)
# or git's HEAD changed, so a build with nothing to do stays a no-op.
set(VERSION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")
set(_version_depends
    ${CMAKE_SOURCE_DIR}/cmake/version.cpp.in
    ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
    ${CMAKE_SOURCE_DIR}/versions.cmake
    motool_common
)
set(_version_command
    ${CMAKE_COMMAND}
    "-DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DINPUT_TEMPLATE=${CMAKE_SOURCE_DIR}/cmake/version.cpp.in"
    "-DOUTPUT_FILE=${VERSION_SOURCE}"
    "-DPROJECT_VERSION_BASE=${PROJECT_CORE_VERSION}"
    "-DPROJECT_CHANNEL=${PROJECT_CHANNEL}"
)
if(GIT_EXECUTABLE)
    list(APPEND _version_command "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --absolute-git-dir
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _git_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    # Appended to on every commit, checkout and reset
    if(_git_dir AND EXISTS "${_git_dir}/logs/HEAD")
        list(APPEND _version_depends "${_git_dir}/logs/HEAD")
    endif()
endif()
add_custom_command(
    OUTPUT ${VERSION_SOURCE}
    COMMAND ${_version_command} -P "${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake"
    DEPENDS ${_version_depends}
    COMMENT "Generating version.cpp"
    VERBATIM
)
add_library(uzx_version STATIC ${VERSION_SOURCE})
target_include_directories(uzx_version PRIVATE ${CMAKE_SOURCE_DIR}/src)
unset(_version_command)
unset(_version_depends)

# Tracktion Utils
# include("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tracktion_engine/tests/utils.cmake")
# addModuleSourceTarget()

# Compile Definitions
set(JUCE_DEFINITIONS
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
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1

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
)

# Dependencies
set(COMMON_APP_LINK_LIBS
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

target_compile_definitions(motool_common PUBLIC
    ${JUCE_DEFINITIONS}

    # libraries bypass juce_add_gui_app's generated flags, so keep the module availability macros explicit.
    $<$<CONFIG:Debug>:DEBUG=1>
    $<$<CONFIG:Debug>:_DEBUG=1>
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_audio_devices=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_audio_processors=1
    JUCE_MODULE_AVAILABLE_juce_audio_utils=1
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_data_structures=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_MODULE_AVAILABLE_juce_events=1
    JUCE_MODULE_AVAILABLE_juce_graphics=1
    JUCE_MODULE_AVAILABLE_juce_gui_basics=1
    JUCE_MODULE_AVAILABLE_juce_gui_extra=1
    JUCE_MODULE_AVAILABLE_juce_osc=1
    JUCE_MODULE_AVAILABLE_tracktion_core=1
    JUCE_MODULE_AVAILABLE_tracktion_engine=1
    JUCE_MODULE_AVAILABLE_tracktion_graph=1
    JUCE_STANDALONE_APPLICATION=1
)

target_compile_definitions(${STUDIO_TARGET_NAME} PRIVATE ${JUCE_DEFINITIONS})
target_compile_definitions(${TUNING_TARGET_NAME} PRIVATE ${JUCE_DEFINITIONS})
target_compile_definitions(${PLAYER_TARGET_NAME} PRIVATE ${JUCE_DEFINITIONS})
target_compile_definitions(${TEST_TARGET_NAME}   PRIVATE ${JUCE_DEFINITIONS})
target_compile_definitions(${TEST_TARGET_NAME}   PRIVATE UZX_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")

target_link_libraries(${STUDIO_TARGET_NAME} PRIVATE motool_common uzx_version ${COMMON_APP_LINK_LIBS})
target_link_libraries(${TUNING_TARGET_NAME} PRIVATE motool_common uzx_version ${COMMON_APP_LINK_LIBS})
target_link_libraries(${PLAYER_TARGET_NAME} PRIVATE motool_common uzx_version ${COMMON_APP_LINK_LIBS})
target_link_libraries(${TEST_TARGET_NAME}   PRIVATE motool_common uzx_version ${COMMON_APP_LINK_LIBS})

# Platform-Specific Settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_compile_options(motool_common         PRIVATE "-fno-aligned-allocation")
    target_compile_options(${STUDIO_TARGET_NAME} PRIVATE "-fno-aligned-allocation")
    target_compile_options(${TUNING_TARGET_NAME} PRIVATE "-fno-aligned-allocation")
    target_compile_options(${PLAYER_TARGET_NAME} PRIVATE "-fno-aligned-allocation")
    target_compile_options(${TEST_TARGET_NAME}   PRIVATE "-fno-aligned-allocation")
    target_link_options(${STUDIO_TARGET_NAME}    PRIVATE "-Wl,-force_load,$<TARGET_FILE:motool_common>")
    target_link_options(${TUNING_TARGET_NAME}    PRIVATE "-Wl,-force_load,$<TARGET_FILE:motool_common>")
    target_link_options(${PLAYER_TARGET_NAME}    PRIVATE "-Wl,-force_load,$<TARGET_FILE:motool_common>")
    target_link_options(${TEST_TARGET_NAME}      PRIVATE "-Wl,-force_load,$<TARGET_FILE:motool_common>")

    # Regenerate resources/icon_<name>.icns from the source PNGs when they change and install it into
    # the bundle over the JUCE-generated AppIcon.icns (the name JUCE puts in CFBundleIconFile).
    # Called before code signing below.
    function(uzx_add_app_icons target name)
        set(source_prefix "${CMAKE_SOURCE_DIR}/resources/icon_${name}")
        set(icns_file  "${CMAKE_SOURCE_DIR}/resources/icon_${name}.icns")

        add_custom_command(
            OUTPUT "${icns_file}"
            COMMAND ${CMAKE_COMMAND}
                -DSOURCE_PREFIX=${source_prefix}
                -DICONSET_DIR=${CMAKE_SOURCE_DIR}/resources/icon_${name}.iconset
                -DICNS_OUTPUT=${icns_file}
                -P "${CMAKE_SOURCE_DIR}/cmake/GenerateIconset.cmake"
            DEPENDS
                "${source_prefix}_big.png"
                "${source_prefix}_small.png"
                "${source_prefix}_tiny.png"
                "${CMAKE_SOURCE_DIR}/cmake/GenerateIconset.cmake"
            COMMENT "Generating ${name} iconset from source PNGs"
        )

        add_custom_target(${name}_icons DEPENDS "${icns_file}")
        add_dependencies(${target} ${name}_icons)

        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${icns_file}"
                "$<TARGET_BUNDLE_DIR:${target}>/Contents/Resources/AppIcon.icns"
            # macOS caches bundle icons by the .app's mtime, which the copy above doesn't change
            COMMAND ${CMAKE_COMMAND} -E touch "$<TARGET_BUNDLE_DIR:${target}>"
            COMMENT "Installing complete icon set for ${target}"
        )
    endfunction()

    uzx_add_app_icons(${PLAYER_TARGET_NAME} player)
    uzx_add_app_icons(${STUDIO_TARGET_NAME} studio)

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
        add_custom_command(TARGET ${STUDIO_TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/codesign.sh $<TARGET_BUNDLE_DIR:${STUDIO_TARGET_NAME}>
            COMMENT "Code signing ${STUDIO_TARGET_NAME}"
        )

        # Add custom command to sign μZXTuning after build
        add_custom_command(TARGET ${TUNING_TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/codesign.sh $<TARGET_BUNDLE_DIR:${TUNING_TARGET_NAME}>
            COMMENT "Code signing ${TUNING_TARGET_NAME}"
        )

        # Add custom command to sign μZXPlayer after build
        add_custom_command(TARGET ${PLAYER_TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/codesign.sh $<TARGET_BUNDLE_DIR:${PLAYER_TARGET_NAME}>
            COMMENT "Code signing ${PLAYER_TARGET_NAME}"
        )
    else()
        message(STATUS "Code signing disabled. Set CODESIGN_IDENTITY environment variable to enable.")
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Link all of motool_common, as -force_load and /WHOLEARCHIVE do, or the linker drops the
    # self-registering device UIs (REGISTER_PLUGIN_UI_ADAPTER) that nothing references
    foreach(_target ${STUDIO_TARGET_NAME} ${TUNING_TARGET_NAME} ${PLAYER_TARGET_NAME} ${TEST_TARGET_NAME})
        target_link_options(${_target} PRIVATE "SHELL:-Wl,--whole-archive $<TARGET_FILE:motool_common> -Wl,--no-whole-archive")
    endforeach()
    target_link_libraries(${STUDIO_TARGET_NAME} PRIVATE "-latomic")
    target_link_libraries(${TUNING_TARGET_NAME} PRIVATE "-latomic")
    target_link_libraries(${PLAYER_TARGET_NAME} PRIVATE "-latomic")
    target_link_libraries(${TEST_TARGET_NAME}   PRIVATE "-latomic")
    if (NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "aarch64")
        target_link_options(${STUDIO_TARGET_NAME} PRIVATE "-m64")
        target_link_options(${TUNING_TARGET_NAME} PRIVATE "-m64")
        target_link_options(${PLAYER_TARGET_NAME} PRIVATE "-m64")
        target_link_options(${TEST_TARGET_NAME}   PRIVATE "-m64")
    endif()
endif()

if(MSVC)
    foreach(_target ${STUDIO_TARGET_NAME} ${TUNING_TARGET_NAME} ${PLAYER_TARGET_NAME} ${TEST_TARGET_NAME})
        target_link_options(${_target} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:motool_common>")
        # Increase stack size from 1MB default to 8MB to match Linux/macOS defaults
        target_link_options(${_target} PRIVATE "/STACK:8388608")
    endforeach()
endif()

add_test(NAME AllTests COMMAND ${TEST_TARGET_NAME})

if(APPLE)
    set_tests_properties(AllTests PROPERTIES
        ENVIRONMENT "MallocScribble=1"
    )
endif()

add_custom_command(TARGET ${TEST_TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:${TEST_TARGET_NAME}>
        -DOUTPUT_FILE=${PROJECT_DISCOVERY_FILE}
        -P "${PROJECT_DISCOVERY_SCRIPT}"
    BYPRODUCTS "${PROJECT_DISCOVERY_FILE}"
    COMMENT "Discovering JUCE unit tests for ${TEST_TARGET_NAME}"
)

# Binary Data Resources (embedded fonts)
juce_add_binary_data(MoToolBinaryData SOURCES
    ${CMAKE_SOURCE_DIR}/resources/fonts/Inter-Regular.ttf
    ${CMAKE_SOURCE_DIR}/resources/fonts/Inter-Bold.ttf
    ${CMAKE_SOURCE_DIR}/resources/fonts/IosevkaAile-Regular.ttf
    ${CMAKE_SOURCE_DIR}/resources/fonts/IosevkaAile-SemiBold.ttf
)

target_link_libraries(motool_common PUBLIC MoToolBinaryData)

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
