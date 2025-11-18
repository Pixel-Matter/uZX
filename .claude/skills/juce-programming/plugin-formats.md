# JUCE Plugin Formats Guide

Comprehensive guide to audio plugin formats supported by JUCE.

## Supported Plugin Formats

JUCE supports building plugins in these formats:
- **VST3** (Steinberg Virtual Studio Technology 3)
- **AU** (Apple Audio Units) - macOS only
- **AUv3** (Audio Unit Extensions) - iOS/macOS
- **AAX** (Avid Audio Extension) - Pro Tools
- **VST** (VST2 - Legacy, deprecated)
- **LV2** (Linux Audio Plugin)
- **Unity** (Unity game engine audio)
- **Standalone** (Independent application)

---

## VST3 (Recommended)

### Overview
- **Developer**: Steinberg
- **Platforms**: Windows, macOS, Linux
- **License**: Free, included with JUCE
- **File Extension**: `.vst3` (bundle/folder)

### Configuration

```cmake
juce_add_plugin(MyPlugin
    FORMATS VST3
    PLUGIN_MANUFACTURER_CODE "Manu"
    PLUGIN_CODE "Mplu"
    VST3_CATEGORIES "Fx Reverb"
    VST_NUM_MIDI_INS 16
    VST_NUM_MIDI_OUTS 16
)
```

### VST3 Categories

Choose from:
- **Effects**: Fx, Analyzer, Delay, Distortion, Dynamics, EQ, Filter, Mastering, Modulation, Pitch Shift, Restoration, Reverb, Surround, Tools
- **Instruments**: Instrument, Drum, Sampler, Synth
- **Spatial**: Spatial, Surround
- **Processing**: Mono, Stereo, OnlyRT, NoOfflineProcess, OnlyOfflineProcess
- **Generators**: Generator, External
- **Special**: Network, Up-Downmix

**Examples**:
- Reverb effect: `VST3_CATEGORIES "Fx Reverb"`
- Synthesizer: `VST3_CATEGORIES "Instrument Synth"`
- EQ: `VST3_CATEGORIES "Fx EQ"`
- Compressor: `VST3_CATEGORIES "Fx Dynamics"`

### VST3 Manifest (moduleinfo.json)

JUCE auto-generates this file for faster plugin scanning:

```cmake
VST3_AUTO_MANIFEST TRUE  # Default
```

If you need custom post-build steps before manifest generation:
```cmake
VST3_AUTO_MANIFEST FALSE

# Later, after your custom steps:
juce_enable_vst3_manifest_step(MyPlugin_VST3)
```

### Installation Paths

- **Windows**: `C:\Program Files\Common Files\VST3\`
- **macOS**: `/Library/Audio/Plug-Ins/VST3/` or `~/Library/Audio/Plug-Ins/VST3/`
- **Linux**: `~/.vst3/` or `/usr/lib/vst3/`

---

## Audio Unit (AU)

### Overview
- **Developer**: Apple
- **Platforms**: macOS only
- **License**: Free
- **File Extension**: `.component` (bundle)

### Configuration

```cmake
juce_add_plugin(MyPlugin
    FORMATS AU
    PLUGIN_MANUFACTURER_CODE "Manu"  # Must have 1+ uppercase letter
    PLUGIN_CODE "Mplu"               # Must have exactly 1 uppercase letter
    AU_MAIN_TYPE "kAudioUnitType_MusicEffect"
    AU_EXPORT_PREFIX "MyPluginAU"
    AU_SANDBOX_SAFE TRUE
)
```

### AU Main Types

- `kAudioUnitType_Effect` - Standard audio effect
- `kAudioUnitType_MusicEffect` - Music effect (receives MIDI)
- `kAudioUnitType_MusicDevice` - Instrument/synthesizer
- `kAudioUnitType_Generator` - Tone generator
- `kAudioUnitType_MIDIProcessor` - MIDI-only effect
- `kAudioUnitType_Mixer` - Audio mixer
- `kAudioUnitType_Panner` - Panner
- `kAudioUnitType_FormatConverter` - Format converter
- `kAudioUnitType_Output` - Output device

### GarageBand Compatibility

GarageBand 10.3+ requires:
- **Manufacturer code**: First letter uppercase, rest lowercase (e.g., "Manu")
- **Plugin code**: First letter uppercase, rest lowercase (e.g., "Mplu")

### Sandbox Safety

```cmake
AU_SANDBOX_SAFE TRUE  # Plugin works in sandboxed hosts (Logic Pro X, GarageBand)
```

For plugins needing special resources (e.g., PACE protection):
```cmake
SUPPRESS_AU_PLIST_RESOURCE_USAGE TRUE
```

### Installation Path

- **System**: `/Library/Audio/Plug-Ins/Components/`
- **User**: `~/Library/Audio/Plug-Ins/Components/`

---

## AUv3 (Audio Unit Extensions)

### Overview
- **Developer**: Apple
- **Platforms**: iOS, macOS (10.13+)
- **License**: Free
- **Format**: App extension

### Configuration

```cmake
juce_add_plugin(MyPlugin
    FORMATS AUv3
    # Same AU configuration as above
    AU_MAIN_TYPE "kAudioUnitType_MusicDevice"

    # iOS-specific
    IPHONE_SCREEN_ORIENTATIONS "UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight"
    BACKGROUND_AUDIO_ENABLED TRUE
)
```

### AUv3 Specifics

- **Hosted in container app**: Requires a host app (automatically created by JUCE)
- **Sandboxed**: Must be sandbox-safe
- **App Store distribution**: Can be distributed via App Store
- **Inter-app audio**: Works with other iOS music apps

### Code Signing (Required)

```bash
cmake -B build-ios -GXcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="iPhone Developer" \
    -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="<your-team-id>"
```

---

## AAX (Avid Audio Extension)

### Overview
- **Developer**: Avid
- **Platforms**: Windows, macOS
- **License**: Free SDK, requires Avid developer account
- **File Extension**: `.aaxplugin` (bundle)
- **DAWs**: Pro Tools

### Prerequisites

1. Sign up at https://developer.avid.com/aax/
2. Download AAX SDK
3. Request Pro Tools Developer from devauth@avid.com
4. Later: Request PACE signing tools from audiosdk@avid.com

### SDK Setup

```cmake
juce_set_aax_sdk_path("/path/to/AAX_SDK")

juce_add_plugin(MyPlugin
    FORMATS AAX
    AAX_IDENTIFIER "com.mycompany.myplugin"  # Defaults to BUNDLE_ID
    AAX_CATEGORY "EQ"
    DISABLE_AAX_BYPASS FALSE
    DISABLE_AAX_MULTI_MONO FALSE
)
```

### AAX Categories

Can combine multiple:
- **Effects**: None, EQ, Dynamics, PitchShift, Reverb, Delay, Modulation, Harmonic, NoiseReduction, Dither, SoundField
- **Generators**: HWGenerators, SWGenerators
- **Special**: WrappedPlugin, Effect, MIDIEffect

Examples:
```cmake
AAX_CATEGORY "EQ Dynamics"       # EQ with dynamics
AAX_CATEGORY "SWGenerators"      # Software instrument
AAX_CATEGORY "Reverb Delay"      # Combined effect
```

### Digital Signing (Required for Release)

Unsigned plugins only work in Pro Tools Developer build.

1. Complete testing in Pro Tools Developer
2. Email audiosdk@avid.com with:
   - Plugin overview
   - Screen recording of plugin in use
   - Company name, admin name, phone number
3. Receive PACE signing tools
4. Sign plugin with PACE tools
5. Distribute signed plugin

### Installation Path

- **Windows**: `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\`
- **macOS**: `/Library/Application Support/Avid/Audio/Plug-Ins/`

---

## LV2 (Linux Audio Plugin)

### Overview
- **Developer**: Open source community
- **Platforms**: Linux (primarily), macOS, Windows
- **License**: Free, open standard
- **File Extension**: `.lv2` (directory bundle)

### Configuration

```cmake
juce_add_plugin(MyPlugin
    FORMATS LV2
    LV2URI "https://mycompany.com/plugins/myplugin"
    # Or use URN if no website:
    # LV2URI "urn:mycompany:myplugin"
)
```

### LV2 URI Requirements

- **Must be globally unique**
- **Must remain the same** for compatible versions
- **Must change** if plugin becomes incompatible (parameters change, presets incompatible, etc.)
- **Recommended format**: `https://yourwebsite.com/plugins/pluginname`
- **Alternative (no website)**: `urn:yourcompany:pluginname`

### Installation Path

- `~/.lv2/`
- `/usr/lib/lv2/`
- `/usr/local/lib/lv2/`

---

## VST (VST2 - Legacy)

### Overview
- **Developer**: Steinberg
- **Status**: **Deprecated** (license removed by Steinberg in 2018)
- **Platforms**: Windows, macOS, Linux
- **Not recommended**: Use VST3 instead

### If You Must Use VST2

Requires old VST2 SDK (no longer officially available):

```cmake
juce_set_vst2_sdk_path("/path/to/VST2_SDK")

juce_add_plugin(MyPlugin
    FORMATS VST
    VST2_CATEGORY "kPlugCategEffect"
)
```

### VST2 Categories

- `kPlugCategUnknown`
- `kPlugCategEffect`
- `kPlugCategSynth`
- `kPlugCategAnalysis`
- `kPlugCategMastering`
- `kPlugCategSpacializer`
- `kPlugCategRoomFx`
- `kPlugSurroundFx`
- `kPlugCategRestoration`
- `kPlugCategOfflineProcess`
- `kPlugCategShell`
- `kPlugCategGenerator`

**Note**: Existing VST2 plugins can continue to be distributed, but new VST2 licenses cannot be obtained.

---

## Standalone

### Overview
- Independent executable application
- No host required
- Useful for testing and simple audio tools

### Configuration

```cmake
juce_add_plugin(MyPlugin
    FORMATS Standalone
    # All other plugin options still apply
)
```

### Features

- Uses `AudioDeviceManager` for audio I/O
- Can still load/save presets
- Same plugin code, different wrapper
- Great for debugging

### Customization

```cpp
// In PluginProcessor.h
#if JucePlugin_Build_Standalone
class StandaloneFilterWindow : public juce::StandaloneFilterWindow
{
public:
    // Customize standalone behavior
};
#endif
```

---

## Unity Plugin

### Overview
- Audio plugins for Unity game engine
- Cross-platform
- Special Unity-specific features

### Configuration

```cmake
juce_add_plugin(MyPlugin
    FORMATS Unity
    UNITY_COPY_DIR "/path/to/unity/project/Assets/Plugins"
)
```

**Note**: Unity format has specific requirements. Consult JUCE Unity documentation for details.

---

## Multi-Format Build

### Build All Formats

```cmake
juce_add_plugin(MyPlugin
    FORMATS VST3 AU Standalone  # Basic
    # FORMATS VST3 AU AUv3 AAX Standalone  # Full (requires SDKs)
)
```

This creates multiple targets:
- `MyPlugin` - Shared code (static library)
- `MyPlugin_VST3` - VST3 plugin
- `MyPlugin_AU` - AU plugin
- `MyPlugin_Standalone` - Standalone app

### Platform-Specific Builds

```cmake
if (APPLE)
    set(FORMATS VST3 AU Standalone)
elseif (WIN32)
    set(FORMATS VST3 AAX Standalone)
else()
    set(FORMATS VST3 LV2 Standalone)
endif()

juce_add_plugin(MyPlugin
    FORMATS ${FORMATS}
)
```

---

## Plugin Identifiers Best Practices

### Manufacturer Code (4 characters)

```cmake
PLUGIN_MANUFACTURER_CODE "Acme"  # Your company/brand
```

**Rules**:
- Exactly 4 characters
- At least 1 uppercase letter (AU requirement)
- GarageBand: First uppercase, rest lowercase
- **Must be registered** with Apple for AU (avoid conflicts)
- Use the same code for all your plugins

### Plugin Code (4 characters)

```cmake
PLUGIN_CODE "Rvb1"  # Unique per plugin
```

**Rules**:
- Exactly 4 characters
- Exactly 1 uppercase letter (AU requirement)
- GarageBand: First uppercase, rest lowercase
- **Must be unique** for each plugin
- Never reuse codes

### Bundle ID (reverse DNS)

```cmake
BUNDLE_ID "com.acme.reverb"
```

**Rules**:
- Reverse DNS notation
- Globally unique
- Lowercase recommended
- Use company domain

---

## Testing Plugin Builds

### Validation Tools

**VST3**:
- Steinberg VST3 Plugin Test Host
- Reaper, Cubase, Ableton Live

**AU**:
- Apple auval: `auval -v <type> <code> <manu>`
- Logic Pro X, GarageBand, Ableton Live

**AAX**:
- Pro Tools (Developer build for unsigned)

**VST2**:
- Reaper, FL Studio, Ableton Live

### auval Command

```bash
# Validate AU plugin
auval -v aufx Mplu Manu  # Effect
auval -v aumu Mplu Manu  # Instrument

# Types:
# aufx - Effect
# aumu - Music Device (synth)
# aumf - Music Effect
# augn - Generator
```

---

## Distribution

### Code Signing

**macOS** (Required for distribution):
```bash
codesign --deep --force --verify --verbose \
    --sign "Developer ID Application: Your Name" \
    MyPlugin.component

# Verify
codesign --verify --verbose MyPlugin.component

# For notarization (required macOS 10.15+)
xcrun notarytool submit MyPlugin.zip \
    --apple-id your@email.com \
    --team-id TEAMID \
    --password app-specific-password
```

**Windows** (Optional but recommended):
- Use Authenticode signing
- Get code signing certificate from CA

### Installer Creation

**macOS**:
- Packages (`.pkg`) with `pkgbuild` and `productbuild`
- DMG disk images with `hdiutil`

**Windows**:
- NSIS (Nullsoft Scriptable Install System)
- Inno Setup
- WiX Toolset

---

## Common Multi-Format Issues

### Path Separators
- AU and VST3 handle paths differently
- Test file operations on all platforms

### Threading
- Some DAWs are stricter about threading
- Always use message thread for UI updates

### State Saving
- Test preset loading in all formats
- Ensure backward compatibility

### MIDI
- Some hosts send different MIDI timing
- Validate MIDI in multiple DAWs

### Sample Rate Changes
- Test with 44.1kHz, 48kHz, 88.2kHz, 96kHz, 192kHz
- Some hosts change sample rate during playback

---

## Recommended Format Combinations

**For most developers**:
```cmake
FORMATS VST3 AU Standalone
```

**Professional release**:
```cmake
FORMATS VST3 AU AAX Standalone
```

**Linux focus**:
```cmake
FORMATS VST3 LV2 Standalone
```

**iOS**:
```cmake
FORMATS AUv3 Standalone
```

**Maximum compatibility** (if you have all SDKs):
```cmake
FORMATS VST3 AU AAX LV2 Standalone
```
