---
name: motool-patterns
description: MoTool-specific design patterns for JUCE/Tracktion Engine development including custom MIDI effects (MidiFxPluginBase), state management wrappers (EditViewState, ZoomViewState, TrackViewState), custom clip integration (PsgClip), plugin UI adapters (PluginUIAdapterRegistry), and parameter binding system (BindedAutoParameter). Use when working on MoTool codebase or implementing similar patterns in JUCE/Tracktion projects.
allowed-tools: Read, Grep, Glob, Edit, Write, Bash, WebFetch, TodoWrite, Task
---

# MoTool Design Patterns

This skill documents architectural patterns developed for the MoTool project - a JUCE/Tracktion Engine application for AY-3-8910 sound chip music creation. These patterns solve common problems in audio application development and can be adapted for similar projects.

## When to Use This Skill

Use this skill when:
- Working directly on the MoTool codebase
- Implementing custom MIDI effects in Tracktion Engine
- Creating state management wrappers for ValueTree-based state
- Extending Tracktion Engine with custom clip types
- Building plugin UI systems with weak coupling
- Implementing parameter binding and automation systems
- Seeking reference implementations for JUCE/Tracktion patterns

## Core Patterns Overview

### 1. Custom MIDI Effect Pattern
**Problem**: How to create reusable MIDI processing effects that work with Tracktion Engine's plugin system while staying real-time safe and type-safe?

**Solution**: `MidiFxPluginBase<MIDIFX>` template pattern
- Uses C++20 concepts (`MidiEffectConcept`) instead of inheritance
- Functor-based processing with `MidiBufferContext`
- Template-based plugin wrapper for Tracktion Engine integration
- Automatic playback position tracking (Edit-based or emulated)

**Key Files**:
- `src/plugins/uZX/midi_effects/MidiEffect.h` - Core pattern implementation

**See**: [midi-effects.md](midi-effects.md) for detailed documentation

### 2. State Management Wrappers
**Problem**: How to manage complex view state (zoom, pan, track heights) with ValueTree persistence while providing type-safe APIs and change notification?

**Solution**: Wrapper classes with `CachedValue<T>` and listener patterns
- **EditViewState**: Global edit view settings and tempo management
- **ZoomViewState**: Timeline zoom/pan with async updates and playback scrolling
- **TrackViewState**: Per-track view state with bounds constraints

**Key Features**:
- Type-safe API over raw ValueTree
- Automatic persistence via CachedValue
- Change notification through ListenerList
- Encapsulation of property IDs

**Key Files**:
- `src/controllers/EditState.h` - All three state wrapper implementations

**See**: [state-wrappers.md](state-wrappers.md) for detailed documentation

### 3. Custom Clip Integration
**Problem**: How to extend Tracktion Engine with custom clip types that require specialized data handling?

**Solution**: Dual inheritance from Tracktion clip + CustomClip utility
- **PsgClip** extends `te::MidiClip` for PSG music file playback
- **CustomClip** provides factory methods for clip creation
- Type-safe clip insertion with `ClipPosition` management
- Integration with Edit's clip registry

**Key Pattern**: Dual inheritance for extension + helper
```cpp
class PsgClip : public te::MidiClip, public CustomClip {
    // Inherits Tracktion clip behavior + custom utilities
};
```

**Key Files**:
- `src/models/PsgClip.h` - Custom clip implementation
- `src/models/CustomClip.h` - Clip factory utilities

**See**: [custom-clips.md](custom-clips.md) for detailed documentation

### 4. Plugin UI Adapter Registry
**Problem**: How to provide custom UI for plugins without creating tight coupling between the track panel and specific plugin types?

**Solution**: Type-indexed registry with factory functions
- **PluginUIAdapterRegistry**: Singleton registry mapping plugin types to UI factories
- **PluginUIAdapterRegistrar**: RAII helper for auto-registration
- `REGISTER_PLUGIN_UI_ADAPTER` macro for convenient registration
- Weak coupling: plugins register themselves, panel queries registry

**Key Pattern**: Dependency inversion via registry
```cpp
// Plugin registers itself
REGISTER_PLUGIN_UI_ADAPTER(MyPlugin, MyPluginUI);

// Panel queries registry
auto ui = registry.createDeviceUI(plugin);
```

**Key Files**:
- `src/gui/devices/PluginUIAdapterRegistry.h` - Registry implementation

**See**: [plugin-ui-adapters.md](plugin-ui-adapters.md) for detailed documentation

### 5. Parameter Binding System
**Problem**: How to connect UI widgets to both stored ValueTree state AND live automation parameters while supporting MIDI learn, type safety, and format conversion?

**Solution**: Multi-layer parameter binding architecture
- **ParameterDef<T>**: Metadata (range, labels, formatting)
- **ParameterValue<T>**: Storage (`CachedValue<T>`) + live reader interface
- **BindedAutoParameter<T>**: Automation bridge (extends `AutomatableParameter`)
- **ParameterEndpoint**: Unified interface for widgets
- **ParamValueEndpoint<T>** / **AutomatableParamEndpoint**: Adapters
- **SliderParamEndpointBinding** / **ButtonParamEndpointBinding**: Widget bindings

**Key Features**:
- Type-safe parameters with template-based conversion
- Separation of stored vs live (automation) values
- AsyncUpdater for thread-safe automation -> ValueTree updates
- MIDI learn support via `MidiParameterMapping`
- Supports bool, int, float, and enum types

**Architecture Layers**:
1. **Definition Layer**: `ParameterDef<T>` describes parameter metadata
2. **Storage Layer**: `ParameterValue<T>` manages ValueTree persistence
3. **Automation Layer**: `BindedAutoParameter<T>` bridges to Tracktion automation
4. **Widget Layer**: `ParameterEndpoint` + bindings connect UI controls

**Key Files**:
- `src/controllers/Parameters.h` - Definitions, values, conversion traits
- `src/controllers/BindedAutoParameter.h` - Automation bridge
- `src/controllers/ParamEndpoint.h` - Widget endpoints
- `src/gui/common/ParamBindings.h` - Widget bindings
- `docs/Parameter binding.md` - Comprehensive architecture documentation

**See**: [parameter-binding.md](parameter-binding.md) for detailed documentation

## Pattern Selection Guide

| **Use Case** | **Pattern** | **Key Benefit** |
|--------------|-------------|-----------------|
| MIDI processing effect | MidiFxPluginBase<T> | Type safety + real-time safety |
| View state persistence | State Wrappers | Type safety + change notification |
| Custom content on timeline | Custom Clip Integration | Extends Tracktion clip system |
| Plugin-specific UI | UI Adapter Registry | Decoupling + extensibility |
| Parameter automation | Parameter Binding | Type safety + automation + UI |

## Integration with JUCE/Tracktion

All patterns follow JUCE/Tracktion best practices:
- **ValueTree** for hierarchical state
- **CachedValue<T>** for type-safe property access
- **ListenerList** for change notification
- **ReferenceCountedObjectPtr** for plugin/clip lifetime
- **UndoManager** integration throughout
- **Message thread** vs **audio thread** separation

## Supporting Documentation

For detailed implementation guides, code examples, and best practices:

- **[midi-effects.md](midi-effects.md)** - Custom MIDI effect pattern deep dive
- **[state-wrappers.md](state-wrappers.md)** - State management wrappers guide
- **[custom-clips.md](custom-clips.md)** - Custom clip integration guide
- **[plugin-ui-adapters.md](plugin-ui-adapters.md)** - UI adapter registry guide
- **[parameter-binding.md](parameter-binding.md)** - Parameter binding system guide

## MoTool Context

MoTool (μZX) is a JUCE/Tracktion Engine application for creating music for the AY-3-8910 sound chip (used in ZX Spectrum and other retro computers). The application includes:

- **PSG format support**: Loading/saving PSG music files
- **AY chip emulation**: Integration with Ayumi emulator
- **MIDI-based workflow**: MIDI clips trigger AY chip voices
- **Custom plugins**: AY instrument and effects as Tracktion plugins
- **Tuning systems**: Just intonation and microtonal support
- **Export to hardware**: Generate audio for real AY chips

These patterns emerged from solving real problems in this domain but are generalizable to other audio applications.

## Best Practices Summary

1. **Use C++20 concepts** over inheritance for compile-time polymorphism
2. **Wrap ValueTree state** with type-safe APIs and CachedValue
3. **Separate concerns**: storage, live values, automation, UI
4. **Use registries** for plugin/extension discovery (avoid hard dependencies)
5. **Thread safety**: AsyncUpdater for audio -> message thread communication
6. **RAII registration**: Auto-register adapters/factories on startup
7. **Template-based binding**: Avoid type erasure until necessary
8. **Document patterns**: Explain "why" not just "what"

## When NOT to Use These Patterns

- **Simple parameter**: Use `AudioProcessorValueTreeState` directly
- **Standard Tracktion clips**: Use built-in `WaveAudioClip`, `MidiClip`, etc.
- **Basic UI**: JUCE's built-in parameter attachments may suffice
- **No automation needed**: Simpler state management may be adequate

## Additional Resources

- **Project documentation**: `docs/Parameter binding.md`, `docs/Design.md`
- **JUCE patterns**: Use the `juce-programming` skill
- **Tracktion patterns**: Use the `tracktion-engine` skill
- **Repository**: All examples from working code at `github.com/ruguevara/MoTool`

## Workflow Tips

When implementing similar patterns:

1. **Start with definition**: What data needs persistence?
2. **Add storage layer**: CachedValue + ValueTree
3. **Add live layer**: If automation needed, add live reader
4. **Add widget layer**: Bindings for UI controls
5. **Test incrementally**: Unit tests for each layer
6. **Document patterns**: Help future maintainers

---

*These patterns are battle-tested in production code. They balance type safety, performance, and maintainability while staying true to JUCE/Tracktion idioms.*
