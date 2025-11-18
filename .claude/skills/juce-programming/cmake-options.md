# JUCE CMake Options Reference

Comprehensive reference for all JUCE CMake functions and options.

## Global CMake Options

These are set before including JUCE or when configuring the project.

### JUCE_BUILD_EXTRAS
**Type**: Boolean (ON/OFF)
**Default**: OFF
**Description**: Build Projucer, AudioPluginHost, and other tools in the 'extras' folder.

```cmake
set(JUCE_BUILD_EXTRAS ON)
# OR
cmake -B build -DJUCE_BUILD_EXTRAS=ON
```

### JUCE_BUILD_EXAMPLES
**Type**: Boolean
**Default**: OFF
**Description**: Build example projects from the 'examples' folder.

### JUCE_ENABLE_MODULE_SOURCE_GROUPS
**Type**: Boolean
**Default**: OFF
**Description**: Make module source files browsable in IDE projects. Increases project generation time.

**Note**: Requires `set_property(GLOBAL PROPERTY USE_FOLDERS YES)` to work.

### JUCE_COPY_PLUGIN_AFTER_BUILD
**Type**: Boolean
**Default**: OFF
**Description**: Automatically install plugins to system folders after building.

**Warning**: May require elevated permissions on some systems.

### JUCE_MODULES_ONLY
**Type**: Boolean
**Default**: OFF
**Description**: Only import module targets, skip juceaide and helper functions. For advanced use cases.

---

## juce_add_plugin() Options

Create an audio plugin with multiple formats.

### Basic Information

#### PRODUCT_NAME
**Type**: String
**Default**: Target name
**Description**: The display name of the plugin.

```cmake
juce_add_plugin(MyPlugin
    PRODUCT_NAME "My Awesome Synth"
)
```

#### VERSION
**Type**: String (major.minor.patch)
**Default**: Project VERSION
**Description**: User-facing version string.

```cmake
VERSION "1.2.3"
```

#### BUILD_VERSION
**Type**: String
**Default**: Same as VERSION
**Description**: Internal build number (macOS CFBundleVersion).

#### BUNDLE_ID
**Type**: String (reverse DNS)
**Default**: Auto-generated from COMPANY_NAME and target name
**Description**: Unique identifier for the plugin.

```cmake
BUNDLE_ID "com.mycompany.mysynth"
```

#### DESCRIPTION
**Type**: String
**Description**: Short description of the plugin.

### Company Information

#### COMPANY_NAME
**Type**: String
**Inherits from**: `JUCE_COMPANY_NAME` property
**Description**: Plugin manufacturer name.

```cmake
# Set globally for all targets
set_directory_properties(PROPERTIES JUCE_COMPANY_NAME "My Company")

# Or per-target
juce_add_plugin(MyPlugin
    COMPANY_NAME "My Company"
)
```

#### COMPANY_COPYRIGHT
**Type**: String
**Inherits from**: `JUCE_COMPANY_COPYRIGHT` property

#### COMPANY_WEBSITE
**Type**: String
**Inherits from**: `JUCE_COMPANY_WEBSITE` property

#### COMPANY_EMAIL
**Type**: String
**Inherits from**: `JUCE_COMPANY_EMAIL` property

### Plugin Formats

#### FORMATS
**Type**: Space-separated list
**Valid values**: `Standalone Unity VST3 AU AUv3 AAX VST LV2`
**Default**: VST3 Standalone
**Description**: Which plugin formats to build.

```cmake
FORMATS VST3 AU Standalone
```

**Notes**:
- AU and AUv3 only available on macOS/iOS
- AAX requires `juce_set_aax_sdk_path()` call first
- VST (VST2) requires `juce_set_vst2_sdk_path()` call first

### Plugin Identification

#### PLUGIN_MANUFACTURER_CODE
**Type**: 4-character string
**Default**: "Manu"
**Description**: Unique manufacturer ID. Must contain at least one uppercase letter for AU compatibility.

```cmake
PLUGIN_MANUFACTURER_CODE "Acme"
```

#### PLUGIN_CODE
**Type**: 4-character string
**Default**: Random
**Description**: Unique plugin ID. Must contain exactly one uppercase letter for AU.

```cmake
PLUGIN_CODE "Msyn"
```

**Important**: These codes must be unique for each plugin!

### Plugin Characteristics

#### IS_SYNTH
**Type**: Boolean
**Default**: FALSE
**Description**: Whether the plugin is a synthesizer/instrument.

```cmake
IS_SYNTH TRUE
```

#### NEEDS_MIDI_INPUT
**Type**: Boolean
**Default**: FALSE
**Description**: Plugin accepts MIDI input.

#### NEEDS_MIDI_OUTPUT
**Type**: Boolean
**Default**: FALSE
**Description**: Plugin produces MIDI output.

#### IS_MIDI_EFFECT
**Type**: Boolean
**Default**: FALSE
**Description**: Plugin is a MIDI effect (special DAW channel strip position).

#### EDITOR_WANTS_KEYBOARD_FOCUS
**Type**: Boolean
**Default**: FALSE
**Description**: Plugin editor needs keyboard input (otherwise defers to host).

### Format-Specific Options

#### VST3_CATEGORIES
**Type**: Space-separated list
**Default**: "Synth" if IS_SYNTH, else "Fx"
**Valid values**: Fx, Instrument, Analyzer, Delay, Distortion, Drum, Dynamics, EQ, External, Filter, Generator, Mastering, Modulation, Mono, Network, NoOfflineProcess, OnlyOfflineProcess, OnlyRT, Pitch Shift, Restoration, Reverb, Sampler, Spatial, Stereo, Surround, Synth, Tools, Up-Downmix

```cmake
VST3_CATEGORIES "Fx Reverb Delay"
```

#### VST_NUM_MIDI_INS / VST_NUM_MIDI_OUTS
**Type**: Integer
**Default**: 16
**Description**: Number of MIDI inputs/outputs for VST2/VST3.

#### AU_MAIN_TYPE
**Type**: AU type constant
**Valid values**: kAudioUnitType_Effect, kAudioUnitType_Generator, kAudioUnitType_MusicDevice, kAudioUnitType_MIDIProcessor, etc.

```cmake
AU_MAIN_TYPE "kAudioUnitType_MusicDevice"  # For synths
```

#### AU_EXPORT_PREFIX
**Type**: String
**Default**: Plugin name + "AU"
**Description**: Prefix for AU entry-point functions.

#### AU_SANDBOX_SAFE
**Type**: Boolean
**Default**: FALSE
**Description**: Marks AU as compatible with sandboxed hosts.

#### AAX_CATEGORY
**Type**: Space-separated list
**Default**: Based on IS_SYNTH and IS_MIDI_EFFECT
**Valid values**: None, EQ, Dynamics, PitchShift, Reverb, Delay, Modulation, Harmonic, NoiseReduction, Dither, SoundField, HWGenerators, SWGenerators, WrappedPlugin, Effect, MIDIEffect

```cmake
AAX_CATEGORY "EQ Dynamics"
```

#### AAX_IDENTIFIER
**Type**: String
**Default**: Same as BUNDLE_ID
**Description**: Bundle ID for AAX plugin.

#### LV2URI
**Type**: URI string
**Description**: Unique URI for LV2 plugin. Must change if plugin becomes incompatible with previous versions.

```cmake
LV2URI "https://mycompany.com/plugins/mysynth"
```

### Platform-Specific Options

#### macOS/iOS Permissions

```cmake
MICROPHONE_PERMISSION_ENABLED TRUE
MICROPHONE_PERMISSION_TEXT "This plugin needs mic access for recording"

CAMERA_PERMISSION_ENABLED TRUE
CAMERA_PERMISSION_TEXT "This plugin uses your camera"

BLUETOOTH_PERMISSION_ENABLED TRUE
BLUETOOTH_PERMISSION_TEXT "For MIDI over Bluetooth"

SEND_APPLE_EVENTS_PERMISSION_ENABLED TRUE
SEND_APPLE_EVENTS_PERMISSION_TEXT "To control other apps"
```

#### iOS-Specific

```cmake
FILE_SHARING_ENABLED TRUE
DOCUMENT_BROWSER_ENABLED TRUE
STATUS_BAR_HIDDEN TRUE
REQUIRES_FULL_SCREEN FALSE
BACKGROUND_AUDIO_ENABLED TRUE
BACKGROUND_BLE_ENABLED TRUE

IPHONE_SCREEN_ORIENTATIONS "UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft"
IPAD_SCREEN_ORIENTATIONS "UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft"

TARGETED_DEVICE_FAMILY "1,2"  # 1=iPhone, 2=iPad, 1,2=both
```

#### macOS Security

```cmake
HARDENED_RUNTIME_ENABLED TRUE  # Required for notarization
HARDENED_RUNTIME_OPTIONS "com.apple.security.device.audio-input com.apple.security.files.user-selected.read-write"

APP_SANDBOX_ENABLED TRUE
APP_SANDBOX_OPTIONS "com.apple.security.files.user-selected.read-write"
APP_SANDBOX_FILE_ACCESS_HOME_RW "Music Documents"
```

#### Linux-Specific

```cmake
NEEDS_CURL TRUE          # If using web features
NEEDS_WEB_BROWSER TRUE   # If using WebBrowserComponent
```

#### Windows-Specific

```cmake
NEEDS_WEBVIEW2 TRUE      # For WebBrowserComponent on Windows
```

### Icons and Assets

```cmake
ICON_BIG "path/to/icon_512.png"
ICON_SMALL "path/to/icon_128.png"

CUSTOM_XCASSETS_FOLDER "Resources/Assets.xcassets"  # iOS/macOS
LAUNCH_STORYBOARD_FILE "Resources/LaunchScreen.storyboard"  # iOS
```

### Plugin Installation

```cmake
COPY_PLUGIN_AFTER_BUILD TRUE

VST3_COPY_DIR "/custom/path/to/vst3"
AU_COPY_DIR "/custom/path/to/au"
AAX_COPY_DIR "/custom/path/to/aax"
```

### Advanced Options

```cmake
DISABLE_AAX_BYPASS FALSE
DISABLE_AAX_MULTI_MONO FALSE

SUPPRESS_AU_PLIST_RESOURCE_USAGE FALSE  # For PACE-protected plugins

PLUGINHOST_AU TRUE  # Enable AU plugin hosting

VST3_AUTO_MANIFEST TRUE  # Generate moduleinfo.json automatically

IS_ARA_EFFECT TRUE  # Enable ARA support
ARA_FACTORY_ID "com.mycompany.mysynth.ara:1.0.0"
```

---

## juce_add_gui_app() / juce_add_console_app() Options

Most options are shared with `juce_add_plugin()`, except plugin-specific ones.

### Common Options

```cmake
juce_add_gui_app(MyApp
    PRODUCT_NAME "My Application"
    COMPANY_NAME "My Company"
    VERSION "1.0.0"
    BUNDLE_ID "com.mycompany.myapp"

    ICON_BIG "Resources/icon_512.png"
    ICON_SMALL "Resources/icon_128.png"

    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "For audio recording"
)
```

---

## Other JUCE CMake Functions

### juce_add_binary_data()

Embed files as binary data in your project.

```cmake
juce_add_binary_data(MyBinaryData
    HEADER_NAME "BinaryData.h"
    NAMESPACE "BinaryData"
    SOURCES
        Resources/logo.png
        Resources/background.jpg
        Presets/default.xml
)

target_link_libraries(MyPlugin PRIVATE MyBinaryData)
```

**Access in code**:
```cpp
auto imageData = BinaryData::logo_png;
auto imageSize = BinaryData::logo_pngSize;
```

### juce_generate_juce_header()

Generate a JuceHeader.h convenience header.

```cmake
juce_generate_juce_header(MyPlugin)
```

**Usage**:
```cpp
#include <JuceHeader.h>  // Includes all linked module headers
```

### juce_add_bundle_resources_directory()

Copy a directory into app bundle resources (macOS/iOS).

```cmake
juce_add_bundle_resources_directory(MyApp "Resources")
```

---

## SDK Path Configuration

Call these before adding plugin targets.

```cmake
juce_set_aax_sdk_path("/path/to/AAX_SDK")
juce_set_vst2_sdk_path("/path/to/VST2_SDK")  # VST2 SDK (legacy)
juce_set_vst3_sdk_path("/path/to/VST3_SDK")  # Usually not needed (bundled)
juce_set_ara_sdk_path("/path/to/ARA_SDK")    # For ARA plugins
```

---

## Compile Definitions

Common JUCE preprocessor flags:

```cmake
target_compile_definitions(MyPlugin PUBLIC
    # Web/Network
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0

    # Plugin compatibility
    JUCE_VST3_CAN_REPLACE_VST2=0

    # Licensing (requires GPL or commercial license)
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0

    # Debug features
    JUCE_DEBUG=1  # Enable debugging features

    # Module-specific
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)
```

---

## Example: Complete Plugin CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyAwesomeSynth VERSION 1.0.0)

# Add JUCE
add_subdirectory(JUCE)

# Create plugin
juce_add_plugin(MyAwesomeSynth
    PRODUCT_NAME "My Awesome Synth"
    COMPANY_NAME "My Company"
    COMPANY_COPYRIGHT "Copyright 2025"
    COMPANY_WEBSITE "https://mycompany.com"

    BUNDLE_ID "com.mycompany.awesomesynth"
    PLUGIN_MANUFACTURER_CODE "Mcmp"
    PLUGIN_CODE "Masy"

    FORMATS VST3 AU Standalone

    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE

    VST3_CATEGORIES "Instrument Synth"
    AU_MAIN_TYPE "kAudioUnitType_MusicDevice"

    COPY_PLUGIN_AFTER_BUILD TRUE
)

# Add source files
target_sources(MyAwesomeSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# Include directories
target_include_directories(MyAwesomeSynth PRIVATE Source)

# Link JUCE modules
target_link_libraries(MyAwesomeSynth PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Compile definitions
target_compile_definitions(MyAwesomeSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)
```
