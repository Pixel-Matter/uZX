# Tracktion Engine Programming

Expert guidance for developing audio applications with Tracktion Engine. Provides comprehensive documentation with progressive loading - files are loaded only when needed.

## When to Use This Skill

Use this skill when:
- Building DAW or audio timeline applications
- Working with multi-track audio/MIDI editing
- Implementing audio recording and playback
- Creating plugin hosts or effect processors
- Developing sequencers or clip launchers
- Integrating real-time audio processing
- Questions about Tracktion Engine API, classes, or patterns

**Do not use** for general JUCE questions - use the separate JUCE skill instead.

## What is Tracktion Engine?

Tracktion Engine is a high-level C++ framework for building sequence-based audio applications, from simple file players to complete DAWs. Built on JUCE, it provides:

- Timeline-based audio/MIDI editing
- Multi-track recording and playback
- Plugin hosting (VST, AU, etc.)
- Automation and modulation
- Real-time audio processing with multi-CPU support
- Cross-platform support (macOS, Windows, Linux, iOS, Android, Raspberry Pi)

**Requirements**: C++20, JUCE
**License**: GPL v3.0 / Commercial

## Quick Start

```cpp
// 1. Create Engine
tracktion::engine::Engine engine { "MyApp" };

// 2. Create or load Edit (timeline/session)
auto edit = te::createEmptyEdit(engine, editFile);

// 3. Add track
auto track = te::createTrack<te::AudioTrack>(*edit);

// 4. Add audio clip
auto clip = track->insertNewClip(te::TrackItem::Type::wave,
                                 { startTime, duration });
clip->setAudioFile(audioFile);

// 5. Control playback
auto& transport = edit->getTransport();
transport.play();
```

## Core Architecture

```
Engine (Central entry point)
  ├─ Edit (Timeline container - like a project/session)
  │   ├─ Tracks[] (AudioTrack, MidiTrack, FolderTrack, etc.)
  │   ├─ Clips[] (WaveAudioClip, MidiClip, StepClip, etc.)
  │   ├─ TransportControl (Play/stop/record)
  │   └─ TempoSequence (Tempo automation)
  │
  ├─ DeviceManager (Audio/MIDI hardware I/O)
  ├─ PluginManager (Plugin discovery/loading)
  └─ AudioFileManager (File caching)
```

## Essential Documentation

**For detailed information, see:**
- [Core Concepts](core-concepts.md) - Architecture, namespaces, module structure
- [API Reference](api-reference.md) - Tracks, clips, plugins, devices, tempo
- [Advanced Features](advanced-features.md) - Automation, MIDI, rendering, recording
- [Expert Topics](expert-topics.md) - Threading, optimization, custom plugins
- [Examples](examples.md) - Code examples and common patterns
- [Quick Reference](quick-reference.md) - Fast lookup table

**Extension and Advanced Systems:**
- [Clip Launcher](clip-launcher.md) - Session view, scenes, follow actions (v3.0+)
- [Custom Clips](custom-clips.md) - Creating custom clip types
- [Custom Plugins Complete](custom-plugins-complete.md) - Full plugin development guide
- [Rack System](rack-system.md) - Modular plugin routing
- [Automation Advanced](automation-advanced.md) - Automation modes, curves (v3.2+)
- [ValueTree Patterns](valuetree-patterns.md) - State management best practices
- [Project Management](project-management.md) - Multi-edit projects, media organization

## Key Resources

- **Official Docs**: https://tracktion.github.io/tracktion_engine/modules.html
- **GitHub**: https://github.com/Tracktion/tracktion_engine
- **Forum**: https://forum.juce.com/c/tracktion-engine
- **Tutorials**: Repository `/tutorials/` directory (01-04)
- **Examples**: Repository `/examples/DemoRunner/demos/` directory

## Module Structure

Tracktion Engine consists of 4 main modules:
- **tracktion_engine** - Main DAW engine and data model
- **tracktion_graph** - Real-time audio graph processing
- **tracktion_core** - Utilities and foundation classes
- **3rd_party** - External libraries (CHOC, libsamplerate, etc.)

## Essential Classes

- **Engine** - Central singleton managing all subsystems
- **Edit** - Timeline container with tracks, clips, tempo (like a session/project)
- **Track** - Container for clips (AudioTrack, MidiTrack, FolderTrack, etc.)
- **Clip** - Timed content on tracks (WaveAudioClip, MidiClip, etc.)
- **TransportControl** - Playback control (play/stop/record)
- **Plugin** - Audio/MIDI processing effects and instruments

## Supported Platforms

macOS, Windows, Linux, Raspberry Pi, iOS, Android

## Version Information

- **Current Version**: 3.1.0
- **Minimum C++**: C++20
- **JUCE Dependency**: Required (not included in Tracktion license)

---

*Files load progressively as needed. Start with core-concepts.md for architecture details, or api-reference.md for specific API usage.*
