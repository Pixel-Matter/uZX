# JUCE Modules Reference Guide

Complete reference for all JUCE modules with descriptions and use cases.

## Core Modules

### juce_core
**Purpose**: Foundation module with essential utilities
**Key Features**:
- File I/O and filesystem operations
- Threading primitives (Thread, CriticalSection, WaitableEvent)
- Memory management utilities
- XML and JSON parsing
- Networking (URL, WebInputStream)
- Time and date handling
- String manipulation
- Math utilities
- Logging system

**When to use**: Required by almost all JUCE projects. Contains fundamental building blocks.

**Link requirement**: Nearly always needed, even for basic applications.

---

### juce_events
**Purpose**: Event handling and message passing
**Key Features**:
- Message thread and message loop
- Timer class for callbacks
- AsyncUpdater for thread-safe updates
- Listener pattern implementations
- Event broadcasting

**When to use**: Any project with asynchronous operations or timers.

**Dependencies**: Requires juce_core

---

### juce_data_structures
**Purpose**: Advanced data structures
**Key Features**:
- ValueTree (hierarchical property storage)
- UndoManager
- ApplicationProperties
- Variant types

**When to use**: Applications needing structured data management, undo/redo, or settings persistence.

**Dependencies**: Requires juce_core, juce_events

---

## Graphics and GUI Modules

### juce_graphics
**Purpose**: 2D graphics rendering
**Key Features**:
- Path and shape drawing
- Image loading and manipulation (PNG, JPG, GIF)
- Fonts and text rendering
- Colour management
- Affine transformations
- Graphics contexts

**When to use**: Any project with custom drawing or image processing.

**Dependencies**: Requires juce_core, juce_events

---

### juce_gui_basics
**Purpose**: Core GUI components and windowing
**Key Features**:
- Component base class
- Button, Slider, Label, ComboBox, etc.
- Layout managers (FlexBox, Grid)
- Mouse and keyboard handling
- Focus management
- Tooltips and popup menus
- LookAndFeel customization
- Desktop window management

**When to use**: Any GUI application or plugin editor.

**Dependencies**: Requires juce_core, juce_events, juce_graphics, juce_data_structures

---

### juce_gui_extra
**Purpose**: Advanced GUI components
**Key Features**:
- WebBrowserComponent
- CodeEditorComponent
- TableListBox
- PropertyPanel
- FileChooser dialogs
- AnimatedPosition
- System tray icons

**When to use**: Applications needing web views, code editors, or advanced UI elements.

**Dependencies**: Requires juce_gui_basics

---

## Audio Modules

### juce_audio_basics
**Purpose**: Fundamental audio data structures
**Key Features**:
- AudioBuffer (multi-channel audio data)
- MidiBuffer and MidiMessage
- AudioSampleBuffer utilities
- Sample rate and channel conversions
- Decibel utilities
- IIR and FIR filter coefficients

**When to use**: Any audio processing application. Foundation for all audio work.

**Dependencies**: Requires juce_core, juce_events

---

### juce_audio_devices
**Purpose**: Audio and MIDI device access
**Key Features**:
- AudioDeviceManager
- AudioIODevice and AudioIODeviceType
- MIDI input/output
- Audio callback handling
- Device enumeration
- Sample rate and buffer size configuration

**When to use**: Standalone audio applications that need direct hardware access.

**Dependencies**: Requires juce_audio_basics

---

### juce_audio_formats
**Purpose**: Audio file reading and writing
**Key Features**:
- AudioFormatReader/Writer
- Supports WAV, AIFF, FLAC, OggVorbis, MP3
- AudioThumbnail for waveform visualization
- AudioFormatManager for format registration
- Streaming and buffered reading

**When to use**: Applications that load/save audio files.

**Dependencies**: Requires juce_audio_basics

---

### juce_audio_processors
**Purpose**: Audio plugin infrastructure
**Key Features**:
- AudioProcessor base class
- AudioProcessorEditor
- AudioProcessorValueTreeState (parameter management)
- Plugin format wrappers (VST3, AU, AAX)
- AudioProcessorGraph for routing
- Preset management

**When to use**: Essential for all audio plugins.

**Dependencies**: Requires juce_audio_basics, juce_gui_basics (for editor)

---

### juce_audio_utils
**Purpose**: Higher-level audio utilities
**Key Features**:
- AudioDeviceSelectorComponent
- MidiKeyboardComponent
- AudioThumbnailCache
- AudioProcessorPlayer
- Built-in audio file playback

**When to use**: Plugins and apps needing ready-made audio UI components.

**Dependencies**: Requires juce_audio_processors, juce_audio_formats, juce_audio_devices

---

## DSP and Audio Processing

### juce_dsp
**Purpose**: Digital signal processing library
**Key Features**:
- ProcessorChain for effect chains
- Filters (IIR, FIR, StateVariableFilter)
- Oscillators (sine, saw, square, triangle)
- Convolution and FFT
- Reverb, Chorus, Phaser effects
- Oversampling
- SIMD optimizations
- Gain, Panner, Limiter, Compressor

**When to use**: Any plugin or app needing DSP operations.

**Dependencies**: Requires juce_audio_basics

**Example usage**:
```cpp
juce::dsp::ProcessorChain<
    juce::dsp::Gain<float>,
    juce::dsp::StateVariableTPTFilter<float>,
    juce::dsp::Reverb
> processorChain;
```

---

## Specialized Modules

### juce_opengl
**Purpose**: OpenGL rendering
**Key Features**:
- OpenGLContext
- OpenGLAppComponent
- Shader utilities
- 3D graphics support
- Hardware-accelerated rendering

**When to use**: Applications needing 3D graphics or hardware-accelerated 2D rendering.

**Dependencies**: Requires juce_gui_basics

---

### juce_cryptography
**Purpose**: Cryptographic functions
**Key Features**:
- MD5, SHA-256, SHA-512 hashing
- RSA encryption/decryption
- Blowfish encryption
- BigInteger for crypto math

**When to use**: Applications requiring secure hashing or encryption.

**Dependencies**: Requires juce_core

---

### juce_video
**Purpose**: Video playback
**Key Features**:
- VideoComponent
- Platform-native video players
- Video file playback

**When to use**: Applications displaying video content.

**Dependencies**: Requires juce_gui_basics

---

### juce_osc
**Purpose**: Open Sound Control protocol
**Key Features**:
- OSCReceiver and OSCSender
- OSCMessage and OSCBundle
- Network communication for music/media

**When to use**: Applications communicating with other audio software via OSC.

**Dependencies**: Requires juce_core, juce_events

---

### juce_analytics
**Purpose**: Usage analytics
**Key Features**:
- Google Analytics integration
- Event tracking
- User analytics

**When to use**: Commercial applications tracking user behavior.

**Dependencies**: Requires juce_core, juce_gui_basics

---

### juce_product_unlocking
**Purpose**: License management
**Key Features**:
- OnlineUnlockStatus
- License key validation
- Trial period management
- Integration with marketplaces

**When to use**: Commercial applications requiring licensing.

**Dependencies**: Requires juce_core, juce_cryptography

---

### juce_midi_ci
**Purpose**: MIDI Capability Inquiry
**Key Features**:
- MIDI-CI protocol support
- Profile configuration
- Property exchange

**When to use**: Advanced MIDI 2.0 applications.

**Dependencies**: Requires juce_audio_basics

---

### juce_box2d
**Purpose**: 2D physics engine
**Key Features**:
- Integration of Box2D physics
- Rigid body physics
- Collision detection

**When to use**: Games or interactive applications needing physics.

**Dependencies**: Requires juce_graphics, juce_gui_basics

---

### juce_animation
**Purpose**: Animation utilities
**Key Features**:
- VBlankAttachment for smooth animations
- Animation scheduling
- Easing functions

**When to use**: Applications with smooth animations synced to display refresh.

**Dependencies**: Requires juce_gui_basics

---

### juce_javascript
**Purpose**: JavaScript engine integration
**Key Features**:
- JavaScript interpreter
- C++ to JavaScript bindings
- ChocJS engine integration

**When to use**: Applications with scripting or web integration.

**Dependencies**: Requires juce_core

---

## Module Linking Best Practices

### Always Use PRIVATE Visibility

```cmake
# CORRECT
target_link_libraries(MyPlugin PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
)

# WRONG - Can cause ODR violations
target_link_libraries(MyPlugin PUBLIC
    juce::juce_audio_processors
)
```

### Common Module Combinations

**Basic Audio Plugin**:
```cmake
target_link_libraries(MyPlugin PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
)
```

**Audio Plugin with DSP**:
```cmake
target_link_libraries(MyPlugin PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
)
```

**GUI Application**:
```cmake
target_link_libraries(MyApp PRIVATE
    juce::juce_gui_basics
    juce::juce_gui_extra
)
```

**Audio Editor/DAW**:
```cmake
target_link_libraries(MyDAW PRIVATE
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
)
```

## Module Header Includes

Each module has a main header file:

```cpp
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
```

Or use the convenience header (if juce_generate_juce_header was called):
```cpp
#include <JuceHeader.h>
```

## Compile Definitions for Module Configuration

Many modules have configuration options via preprocessor definitions:

```cmake
target_compile_definitions(MyPlugin PUBLIC
    JUCE_WEB_BROWSER=0              # Disable web browser in juce_gui_extra
    JUCE_USE_CURL=0                 # Disable CURL in juce_core
    JUCE_VST3_CAN_REPLACE_VST2=0   # VST3 compatibility
    JUCE_DISPLAY_SPLASH_SCREEN=0    # Remove JUCE splash (requires license)
)
```

Check each module's header file for available config options (search for "Config:" comments).
