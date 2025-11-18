# Core Concepts

Fundamental architecture and concepts of Tracktion Engine.

## Key Namespaces

- `tracktion::engine` or `te` - Main engine classes
- `tracktion::graph` - Audio graph processing
- `tracktion::core` - Utilities and helpers

## Module Organization

### tracktion_engine (Main Module)
Located in `/modules/tracktion_engine/`, provides:

```
model/                    # Core data model
├── edit/                # Edit container, tempo, time signatures
├── tracks/              # Track types (Audio, MIDI, Tempo, etc.)
├── clips/               # Clip types (Audio, MIDI, Step, etc.)
├── automation/          # Automation curves and modifiers
└── export/              # Rendering and export

playback/                # Playback engine
├── devices/             # Input/Output device management
└── graph/               # Audio graph integration

plugins/                 # Plugin system
├── internal/            # Built-in plugins (EQ, Compressor, etc.)
├── external/            # VST/AU plugin wrapper
├── effects/             # Effect plugins
└── ARA/                 # ARA (Audio Random Access) support

audio_files/             # Audio file handling
midi/                    # MIDI processing
utilities/               # Core utilities (37+ header files)
project/                 # Project management
control_surfaces/        # External controller support
```

### tracktion_graph (Audio Graph Module)
Real-time audio processing with:
- Multi-CPU threading algorithms
- Node-based audio graph
- Lock-free multi-threaded node players
- Playhead and timing synchronization

### tracktion_core (Core Utilities)
Foundation utilities:
- Time and timing (`tracktion_Time.h`, `tracktion_TimeRange.h`, `tracktion_Tempo.h`)
- Audio abstractions (`tracktion_AudioReader.h`)
- Thread synchronization (`tracktion_MultipleWriterSeqLock.h`)
- Math utilities and benchmarking

### 3rd_party (External Libraries)
- CHOC library (audio/MIDI processing)
- Expected library (error handling)
- RpMalloc (memory allocation)
- libsamplerate (sample rate conversion)
- SoundTouch/Elastique (time-stretching - optional)
- AirWindows plugins (optional)

## Core Class Hierarchy

```cpp
Engine (Central entry point)
  ├─ Edit (Playable timeline arrangement)
  │   ├─ Track[] (multiple track types)
  │   │   ├─ AudioTrack (audio playback)
  │   │   ├─ MidiTrack (MIDI data)
  │   │   ├─ FolderTrack (grouping)
  │   │   ├─ TempoTrack (tempo automation)
  │   │   ├─ MarkerTrack (markers/cues)
  │   │   ├─ AutomationTrack (parameter automation)
  │   │   ├─ ChordTrack (chord information)
  │   │   └─ ArrangerTrack (arrangement sections)
  │   │
  │   ├─ Clip[] (per track)
  │   │   ├─ WaveAudioClip (audio playback)
  │   │   ├─ MidiClip (MIDI notes/CC)
  │   │   ├─ StepClip (step sequencing)
  │   │   ├─ ChordClip (chord sequences)
  │   │   ├─ EditClip (nested Edit)
  │   │   ├─ MarkerClip (markers)
  │   │   └─ CollectionClip (clip collections)
  │   │
  │   ├─ TransportControl (playback/recording control)
  │   ├─ TempoSequence (tempo automation)
  │   ├─ PitchSequence (key/pitch automation)
  │   ├─ MarkerManager (markers/cues)
  │   ├─ Plugin[] (master track effects)
  │   └─ RenderManager (background rendering)
  │
  ├─ DeviceManager (Audio/MIDI I/O)
  │   ├─ OutputDevice[] (speakers, etc.)
  │   └─ InputDevice[] (microphones, MIDI controllers)
  │
  ├─ PluginManager (Plugin discovery/loading)
  ├─ AudioFileManager (Audio file caching/reading)
  ├─ ProjectManager (Project management)
  └─ [Other managers...]
```

## Key Design Patterns

1. **ValueTree-based Persistence** - All model objects use JUCE's ValueTree for state
2. **Listener Pattern** - `ChangeListener` callbacks for state changes
3. **Weak References** - Safe object lifetime management
4. **JUCE Integration** - Depends on JUCE for audio, GUI, and utilities
5. **Unique Pointers** - Automatic memory management
6. **Lock-free Queues** - Thread-safe communication without mutexes
7. **RAII** - Resource acquisition in constructors

## Basic Workflow Pattern

```cpp
// 1. Create Engine
tracktion_engine::Engine engine { "MyApp" };

// 2. Create or load Edit
auto edit = te::createEmptyEdit(engine, editFile);
// or: auto edit = te::loadEditFromFile(engine, existingFile);

// 3. Add tracks
auto audioTrack = te::createTrack<te::AudioTrack>(*edit);

// 4. Add clips
auto clip = audioTrack->insertNewClip(
    te::TrackItem::Type::wave,
    { startTime, duration });
clip->setAudioFile(audioFile);

// 5. Add plugins
auto plugin = audioTrack->pluginList.insertPlugin(
    te::ExternalPlugin::create(...), 0);

// 6. Control playback
auto& transport = edit->getTransport();
transport.play();

// 7. Export/render
te::Renderer renderer(engine, *edit, outputFile, options);
renderer.render();
```

## Threading Model Overview

**Thread Types:**
- **Message Thread**: UI updates, Edit modifications, non-real-time ops
- **Audio Thread**: Real-time processing (callback), lock-free only
- **Recording Thread**: Disk I/O for recording
- **Background Threads**: Asset generation, rendering, plugin scanning

**Key Rule**: Never block the audio thread. Use lock-free data structures for cross-thread communication.

## Time and Tempo

Tracktion Engine uses flexible time representation:
- **Seconds** - Absolute time in seconds
- **Beats** - Musical time in quarter notes
- **Bars:Beats** - Musical time as bars and beats
- **Timecode** - SMPTE, frame-based

All conversions handled by `TempoSequence`.

## State Management

All objects use JUCE's `ValueTree` for state:
- Automatic serialization/deserialization
- Change notification system
- Undo/redo support
- Thread-safe access patterns

## Configuration Flags

Key compile-time options:

```cpp
TRACKTION_ENABLE_ARA                // Audio Random Access support
TRACKTION_ENABLE_CMAJOR             // Cmajor plugin format
TRACKTION_ENABLE_REWIRE             // ReWire support
TRACKTION_ENABLE_AUTOMAP            // Novation Automap
TRACKTION_ENABLE_VIDEO              // Video support
TRACKTION_ENABLE_REX                // REX audio format
TRACKTION_ENABLE_CONTROL_SURFACES   // External controller support
TRACKTION_ENABLE_TIMESTRETCH_*      // Various timestretch libraries
TRACKTION_ENABLE_ABLETON_LINK       // Ableton Link sync
TRACKTION_AIR_WINDOWS               // AirWindows effects
TRACKTION_UNIT_TESTS                // Unit test framework
TRACKTION_BENCHMARKS                // Benchmark suite
```

## Example Projects

Located in `/examples/`:
- **DemoRunner** - Multiple demos showcasing features
  - PlaybackDemo.h - Basic audio playback
  - MidiPlaybackDemo.h - MIDI playback
  - RecordingDemo.h - Audio recording
  - PluginDemo.h - Plugin usage
  - StepSequencerDemo.h - Step sequencer
  - And many more...
- **EngineInPluginDemo** - Wrapping Engine inside a plugin
- **TestRunner** - Unit tests
- **Benchmarks** - Performance tests

## Documentation Locations

- **Core headers**: `/modules/tracktion_engine/tracktion_engine.h`
- **Examples**: `/examples/DemoRunner/demos/*.h`
- **Tutorials**: `/tutorials/*.md` (01-04 numbered)
- **Tests**: `/modules/tracktion_engine/tracktion_engine_tests.cpp`
- **API docs**: https://tracktion.github.io/tracktion_engine/modules.html
