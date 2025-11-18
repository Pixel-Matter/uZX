# Quick Reference

Fast lookup table for common Tracktion Engine operations.

## Essential Classes

| Class | Purpose | Key Methods |
|-------|---------|-------------|
| `Engine` | Central manager | Constructor, `getPluginManager()`, `getDeviceManager()` |
| `Edit` | Timeline container | `getAllTracks()`, `getTransport()`, `getTempoSequence()` |
| `AudioTrack` | Audio track | `insertNewClip()`, `pluginList`, `getClips()` |
| `MidiTrack` | MIDI track | `insertNewClip()`, `pluginList`, `getClips()` |
| `WaveAudioClip` | Audio clip | `setGainDB()`, `setSpeedRatio()`, `setPitchChange()` |
| `MidiClip` | MIDI clip | `getSequence()` |
| `TransportControl` | Playback | `play()`, `stop()`, `record()`, `setPosition()` |
| `Plugin` | Effect/instrument | `getAutomatableParameters()`, `applyToBuffer()` |
| `TempoSequence` | Tempo | `setTempo()`, `timeToBeats()`, `beatsToTime()` |

## Common Tasks

### Create and Load Edit

```cpp
// New edit
auto edit = te::createEmptyEdit(engine, editFile);

// Load edit
auto edit = te::loadEditFromFile(engine, editFile);

// Save edit
edit->saveAs(newFile);
edit->flushState();  // Save without changing file
```

### Create Track

```cpp
auto audioTrack = te::createTrack<te::AudioTrack>(*edit);
auto midiTrack = te::createTrack<te::MidiTrack>(*edit);
auto folderTrack = te::createTrack<te::FolderTrack>(*edit);
```

### Add Audio Clip

```cpp
auto clip = track->insertWaveClip("Name", audioFile,
                                  { {start, end}, offset });
```

### Add MIDI Clip

```cpp
auto midiClip = dynamic_cast<te::MidiClip*>(
    track->insertNewClip(te::TrackItem::Type::midi,
                        { start, duration }, nullptr));
```

### Add MIDI Notes

```cpp
auto& seq = midiClip->getSequence();
seq.addNote(noteNumber, startBeat, lengthBeats, velocity, 0, nullptr);
```

### Add Plugin

```cpp
auto plugin = track->pluginList.insertPlugin(
    te::Plugin::create(description), index);
```

### Playback Control

```cpp
auto& transport = edit->getTransport();
transport.play(true);
transport.stop(false, false);
transport.setPosition(seconds);
transport.record(false);
```

### Tempo

```cpp
auto& tempo = edit->getTempoSequence();
tempo.setTempo(0.0, 120.0);  // 120 BPM at time 0

// Conversions
auto beats = tempo.timeToBeats(seconds);
auto time = tempo.beatsToTime(beats);
```

### Render

```cpp
te::Renderer::Parameters params(*edit);
params.destFile = outputFile;
params.time = { startTime, endTime };

te::Renderer renderer(params);
renderer.render();  // Or renderInBackground()
```

## Built-in Plugins

| Plugin | Type | Key Parameters |
|--------|------|----------------|
| `VolumeAndPanPlugin` | Utility | `setVolumeDb()`, `setPan()` |
| `EqualizerPlugin` | EQ | `setBandGain()`, `setBandFrequency()` |
| `CompressorPlugin` | Dynamics | `setThresholdDb()`, `setRatio()` |
| `ReverbPlugin` | Space | `setRoomSize()`, `setDamping()` |
| `DelayPlugin` | Time | `setDelayTime()`, `setFeedback()` |
| `ChorusPlugin` | Modulation | `setDepth()`, `setSpeed()` |
| `PhaserPlugin` | Modulation | `setDepth()`, `setSpeed()` |
| `PitchShiftPlugin` | Pitch | `setSemitonesUp()` |
| `LowPassPlugin` | Filter | `setFrequency()`, `setResonance()` |

## Track Types

- `AudioTrack` - Audio playback/recording
- `MidiTrack` - MIDI notes/CC
- `FolderTrack` - Group tracks
- `TempoTrack` - Tempo automation
- `MarkerTrack` - Markers/cues
- `ChordTrack` - Chord progressions
- `ArrangerTrack` - Arrangement sections
- `AutomationTrack` - Parameter automation

## Clip Types

- `WaveAudioClip` - Audio files
- `MidiClip` - MIDI notes
- `StepClip` - Step sequencer
- `ChordClip` - Chord sequences
- `EditClip` - Nested Edit
- `MarkerClip` - Markers
- `CollectionClip` - Clip collections

## Time Conversions

```cpp
auto& tempo = edit->getTempoSequence();

// Seconds ↔ Beats
auto beats = tempo.timeToBeats(seconds);
auto time = tempo.beatsToTime(beats);

// Beats ↔ Bars:Beats
auto barBeat = tempo.toBarBeatFraction(beats);
auto beats = tempo.fromBarBeatFraction(barBeat);

// Format timecode
auto tc = te::TimecodeDisplayFormat::formatTimecode(
    time, tempo, te::TimecodeDisplayFormat::barsBeats);
```

## File Locations

| What | Where |
|------|-------|
| Main header | `/modules/tracktion_engine/tracktion_engine.h` |
| Examples | `/examples/DemoRunner/demos/*.h` |
| Tutorials | `/tutorials/*.md` |
| Tests | `/modules/tracktion_engine/tracktion_engine_tests.cpp` |
| API docs | https://tracktion.github.io/tracktion_engine/modules.html |

## Key IDs (for ValueTree)

```cpp
te::IDs::name      // Track/clip name
te::IDs::colour    // Colour
te::IDs::mute      // Mute state
te::IDs::solo      // Solo state
te::IDs::volume    // Volume
te::IDs::pan       // Pan
```

## Configuration Flags

```cpp
TRACKTION_ENABLE_ARA                // Audio Random Access
TRACKTION_ENABLE_CMAJOR             // Cmajor plugins
TRACKTION_ENABLE_REWIRE             // ReWire
TRACKTION_ENABLE_VIDEO              // Video support
TRACKTION_ENABLE_REX                // REX format
TRACKTION_ENABLE_CONTROL_SURFACES   // Controllers
TRACKTION_ENABLE_TIMESTRETCH_*      // Time stretching
TRACKTION_ENABLE_ABLETON_LINK       // Ableton Link
TRACKTION_AIR_WINDOWS               // AirWindows effects
TRACKTION_UNIT_TESTS                // Unit tests
TRACKTION_BENCHMARKS                // Benchmarks
```

## Threading Rules

| Thread | Can Do | Cannot Do |
|--------|--------|-----------|
| Message | Modify Edit, create objects, UI updates | Block for long time |
| Audio | Process audio, lock-free ops | Allocate, block, modify Edit |
| Recording | Write to disk | Heavy processing |
| Background | Slow operations, scanning | Modify Edit without sync |

## Namespaces

- `tracktion::engine` or `te` - Main engine
- `tracktion::graph` - Audio graph
- `tracktion::core` - Utilities

## Version Info

- **Version**: 3.1.0
- **C++**: C++20 minimum
- **License**: GPL v3 / Commercial
- **Platforms**: macOS, Windows, Linux, iOS, Android, Raspberry Pi

## Parameter Modifier Types

- `LFOModifier` - Low-frequency oscillation
- `EnvelopeFollowerModifier` - Follow audio envelope
- `BreakpointOscillatorModifier` - Custom curve
- `StepModifier` - Step sequencer modulation
- `RandomModifier` - Random values
- `MIDITrackerModifier` - MIDI velocity/aftertouch

## Transport State Queries

```cpp
transport.isPlaying();
transport.isRecording();
transport.getCurrentPosition();
transport.looping;
```

## Audio File Formats

Supported: WAV, AIFF, FLAC, OGG, MP3, CAF, REX

```cpp
te::AudioFile audioFile(engine, file);
auto length = audioFile.getLength();
auto sampleRate = audioFile.getSampleRate();
auto channels = audioFile.getNumChannels();
```

## MIDI Operations

```cpp
// Add note
sequence.addNote(note, beat, length, vel, colour, undo);

// Add controller
sequence.addControllerEvent(beat, type, value, meta, undo);

// Quantize
te::Quantisation quant;
quant.setType(te::Quantisation::Type::sixteenth);
sequence.quantise(quant, undo);
```

## Selection

```cpp
auto& sel = edit->getSelectionManager();
sel.selectOnly(object);
sel.addToSelection(object);
sel.deselectAll();

auto clips = sel.getItemsOfType<te::Clip>();
auto tracks = sel.getItemsOfType<te::Track>();
```

## Undo/Redo

```cpp
auto& undo = edit->getUndoManager();
undo.beginNewTransaction("Operation");
// ... perform operations ...
undo.undo();
undo.redo();
```

## Markers

```cpp
auto& markers = edit->getMarkerManager();
auto marker = markers.createMarker(-1, pos, length, undo);
marker->name = "Intro";
marker->colour = juce::Colours::red;

auto next = markers.getNextMarker(time);
auto prev = markers.getPrevMarker(time);
```

## Devices

```cpp
auto& devMgr = engine.getDeviceManager();
auto& audioOut = devMgr.getDefaultWaveOutDevice();
auto& audioIn = devMgr.getDefaultWaveInDevice();

auto midiIns = te::MidiInputDevice::getAvailableDevices(engine);
auto midiOuts = te::MidiOutputDevice::getAvailableDevices(engine);
```

## Clip Launching

```cpp
auto slots = track->getClipSlotList();
auto slot = slots->getClipSlot(index);

slot->launch();
slot->stop();
slot->setFollowAction(te::ClipSlot::FollowAction::playNext);
```

## Common Patterns

### Find track by name
```cpp
edit->getTrackByName("Track Name");
```

### Get all audio tracks
```cpp
auto audioTracks = edit->getAudioTracks();
```

### Iterate clips
```cpp
for (auto clip : track->getClips())
    processClip(clip);
```

### Get plugin parameters
```cpp
for (auto param : plugin->getAutomatableParameters())
    processParam(param);
```

### Check clip type
```cpp
if (auto audioClip = dynamic_cast<te::WaveAudioClip*>(clip))
    // Handle audio clip

if (auto midiClip = dynamic_cast<te::MidiClip*>(clip))
    // Handle MIDI clip
```

## Useful Utilities

```cpp
// Scratch buffer
auto scratch = engine.getAudioFileManager()
    .getScratchBuffer(channels, samples);

// Audio fade curves
te::AudioFadeCurve::linear
te::AudioFadeCurve::sCurve
te::AudioFadeCurve::convex
te::AudioFadeCurve::concave

// ADSR envelope
te::Envelope env;
env.attackTime = 0.01;
env.decayTime = 0.1;
env.sustainLevel = 0.7;
env.releaseTime = 0.2;
```

## Error Checking

```cpp
// Check valid edit
if (edit == nullptr)
    return;

// Check audio file
if (!audioFile.isValid() || !audioFile.hasFile())
    return;

// Check plugin loaded
if (plugin == nullptr)
    return;

// Check clip type
if (auto midiClip = dynamic_cast<te::MidiClip*>(clip))
{
    // Safe to use as MIDI clip
}
```

## Resources

- **API Docs**: https://tracktion.github.io/tracktion_engine/modules.html
- **GitHub**: https://github.com/Tracktion/tracktion_engine
- **Forum**: https://forum.juce.com/c/tracktion-engine
- **Company**: https://www.tracktion.com/develop/tracktion-engine
