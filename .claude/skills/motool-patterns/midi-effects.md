# Custom MIDI Effect Pattern

## Overview

The MoTool MIDI effect pattern provides a type-safe, real-time safe framework for creating MIDI processing effects that integrate with Tracktion Engine's plugin system.

## Problem Statement

Traditional MIDI effect implementations face several challenges:

1. **Type safety**: Virtual inheritance loses compile-time type information
2. **Real-time safety**: Easy to accidentally allocate or lock in audio callback
3. **Boilerplate**: Lots of plugin wrapper code for simple effects
4. **Context tracking**: Managing playback position and sample rate
5. **Integration**: Connecting to Tracktion's plugin system

## Solution Architecture

### Core Components

#### 1. MidiBufferContext

A lightweight context object passed to MIDI effects:

```cpp
struct MidiBufferContext {
    tracktion::MidiMessageArray& buffer;  // MIDI events to process
    const int start;                      // Start sample in buffer
    const int length;                     // Length to process (samples)
    const tracktion::TimePosition playPosition;  // Edit position
    const double sampleRate;              // For time conversions

    // Helper methods for time/sample conversion
    int getSampleForTimeRel(double time) const;
    tracktion::TimePosition processStartTime() const;
    tracktion::TimeDuration duration() const;
};
```

**Key insight**: Passing context by reference avoids allocations in the audio thread.

**File**: `src/plugins/uZX/midi_effects/MidiEffect.h:16-93`

#### 2. MidiEffectConcept (C++20 Concept)

Compile-time interface for MIDI effects:

```cpp
template<typename T>
concept MidiEffectConcept = requires(T& effect, MidiBufferContext& context) {
    effect(context);  // Must be callable as functor
};
```

**Benefits**:
- No virtual calls = better optimization
- Type information preserved for templates
- Clear compile-time error messages
- Zero runtime overhead

**File**: `src/plugins/uZX/midi_effects/MidiEffect.h:99-121`

#### 3. MidiFxPluginBase<MIDIFX> Template

Plugin wrapper that integrates MIDI effects with Tracktion Engine:

```cpp
template <MidiEffectConcept MIDIFX>
class MidiFxPluginBase : public PluginBase {
public:
    MidiFxPluginBase(tracktion::PluginCreationInfo info, MIDIFX& fx)
        : PluginBase(info), midiEffect(fx) {}

    void applyToBuffer(const tracktion::PluginRenderContext& fc) override {
        // Convert Tracktion context to MidiBufferContext
        MidiBufferContext context {
            .buffer = *fc.bufferForMidiMessages,
            .start = fc.bufferStartSample,
            .length = fc.bufferNumSamples,
            .playPosition = playPosition,
            .sampleRate = sampleRate
        };

        // Call effect
        midiEffect(context);
    }

protected:
    MIDIFX& midiEffect;
};
```

**Key features**:
- Template accepts any type satisfying `MidiEffectConcept`
- Handles playback position tracking (Edit vs emulated)
- Provides MIDI-only plugin interface to Tracktion
- Zero-cost abstraction

**File**: `src/plugins/uZX/midi_effects/MidiEffect.h:212-288`

## Usage Examples

### Example 1: Simple MIDI Transpose Effect

```cpp
class TransposeEffect {
public:
    void operator()(MidiBufferContext& context) {
        for (auto& msg : context.buffer) {
            if (msg.isNoteOnOrOff()) {
                int newNote = std::clamp(msg.getNoteNumber() + semitones, 0, 127);
                msg.setNoteNumber(newNote);
            }
        }
    }

    int semitones = 0;  // Parameter
};

// Create plugin wrapper
class TransposePlugin : public MidiFxPluginBase<TransposeEffect> {
public:
    TransposePlugin(tracktion::PluginCreationInfo info)
        : MidiFxPluginBase(info, effect) {}

    String getName() override { return "Transpose"; }

private:
    TransposeEffect effect;
};
```

### Example 2: Timing-aware Effect with Context

```cpp
class ArpeggiatorEffect {
public:
    void operator()(MidiBufferContext& context) {
        // Access playback time
        auto startTime = context.processStartTime();
        auto duration = context.duration();

        // Process buffer within time range
        for (auto& msg : context.buffer) {
            auto msgTime = startTime + tracktion::TimeDuration::fromSeconds(
                msg.getTimeStamp()
            );

            // Time-based processing...
            if (shouldArpeggiateAt(msgTime)) {
                // Modify or add notes
            }
        }
    }

private:
    bool shouldArpeggiateAt(tracktion::TimePosition time);
};
```

### Example 3: Stateful Effect with Parameter Binding

```cpp
class VelocityScaler {
public:
    void operator()(MidiBufferContext& context) {
        if (context.isAllNotesOff()) {
            // Reset state on all-notes-off
            return;
        }

        for (auto& msg : context.buffer) {
            if (msg.isNoteOn()) {
                int scaled = std::clamp(
                    int(msg.getVelocity() * scale.getLiveValue()),
                    1, 127
                );
                msg.setVelocity(scaled);
            }
        }
    }

    ParameterValue<float> scale;  // Bound to automation
};
```

## Playback Position Tracking

The plugin base class automatically tracks playback position:

### Edit-based Position
When playing along Edit timeline:
```cpp
if (!fc.editTime.isEmpty()) {
    positionSource = PositionSource::Edit;
    playPosition = fc.editTime.getStart();
}
```

### Emulated Position
When playing without Edit (e.g., testing, MIDI input):
```cpp
else {
    if (positionSource != PositionSource::Emulated) {
        positionSource = PositionSource::Emulated;
        playPosition = fc.editTime.getStart();  // Reset
    }
}

// After processing
if (positionSource == PositionSource::Emulated) {
    playPosition = playPosition + context.duration();
}
```

**File**: `src/plugins/uZX/midi_effects/MidiEffect.h:251-277`

## Advanced Features

### MidiEffectChain

For chaining multiple effects:

```cpp
MidiEffectChain chain;
chain.add(std::make_unique<TransposeEffect>());
chain.add(std::make_unique<VelocityScaler>());

// Processes all effects in sequence
chain(context);
```

**Features**:
- Thread-safe add/remove with `CriticalSection`
- Dynamic effect ordering
- Works with any `MidiEffectBase` derived class

**File**: `src/plugins/uZX/midi_effects/MidiEffect.h:157-200`

### MidiEffectBase (Virtual Base)

For dynamic polymorphism when needed:

```cpp
class MidiEffectBase {
public:
    virtual void operator()(MidiBufferContext& fc) = 0;
};
```

**Use when**:
- Runtime plugin discovery
- Dynamic effect selection
- Plugin collections

**Trade-off**: Virtual calls vs compile-time type safety

**File**: `src/plugins/uZX/midi_effects/MidiEffect.h:131-147`

## Integration with Tracktion Engine

### Plugin Creation

```cpp
class MyMidiEffect {
    void operator()(MidiBufferContext& ctx) { /* ... */ }
};

class MyMidiPlugin : public MidiFxPluginBase<MyMidiEffect> {
public:
    MyMidiPlugin(tracktion::PluginCreationInfo info)
        : MidiFxPluginBase(info, effect) {}

    // Override plugin metadata
    String getName() override { return "My MIDI Effect"; }
    String getVendor() override { return "MyCompany"; }

private:
    MyMidiEffect effect;
};

// Register with Tracktion
static MyMidiPlugin::Ptr create(tracktion::PluginCreationInfo info) {
    return new MyMidiPlugin(info);
}
```

### Plugin Registration

```cpp
// In plugin manager initialization
pluginManager.registerPluginType<MyMidiPlugin>(
    "MyMidiEffect",
    &MyMidiPlugin::create
);
```

## Best Practices

### ✅ DO

1. **Keep effects pure functors**: Minimal state, focus on processing
2. **Use const for read-only access**: Mark helpers as `const`
3. **Leverage context helpers**: Use `getSampleForTimeRel()`, etc.
4. **Respect sample boundaries**: Only process within `[start, start+length)`
5. **Use concepts over inheritance**: Prefer `MidiEffectConcept` for static polymorphism
6. **Parameter binding**: Use `ParameterValue<T>` for automatable parameters

### ❌ DON'T

1. **Allocate in operator()**: No `new`, `malloc`, `std::vector::push_back`
2. **Lock in operator()**: Avoid `CriticalSection` in audio callback
3. **Log in operator()**: No `DBG()` or `std::cout` (use only for debugging)
4. **Ignore sample range**: Always check `start` and `length`
5. **Modify context members**: Don't change `playPosition`, `sampleRate`, etc.

## Real-Time Safety

The pattern enforces real-time safety:

```cpp
void operator()(MidiBufferContext& context) {
    SCOPED_REALTIME_CHECK  // Asserts if non-RT operation occurs

    // Safe: Stack allocation
    int transposed[128];

    // UNSAFE: Heap allocation
    // std::vector<int> transposed;  // DON'T DO THIS

    // Safe: In-place modification
    for (auto& msg : context.buffer) {
        // Process...
    }
}
```

## Performance Characteristics

- **Template instantiation**: One plugin class per effect type
- **Functor call**: Inlined by compiler (zero overhead)
- **Context passing**: By reference (no copies)
- **Virtual calls**: None (when using concept-based approach)
- **Memory allocation**: None in audio thread

## Comparison with Alternatives

| **Approach** | **Type Safety** | **Performance** | **Flexibility** |
|--------------|-----------------|-----------------|-----------------|
| Virtual inheritance | ❌ Type erased | ⚠️ Virtual calls | ✅ Runtime polymorphism |
| Template + Concept | ✅ Compile-time | ✅ Zero overhead | ⚠️ Compile-time only |
| Function pointers | ❌ Type erased | ⚠️ Indirect call | ✅ Runtime binding |
| **MidiFxPluginBase** | ✅ Compile-time | ✅ Zero overhead | ✅ Both! |

## Testing

Example unit test structure:

```cpp
class MidiEffectTest : public juce::UnitTest {
public:
    MidiEffectTest() : UnitTest("MIDI Effects", "MoTool") {}

    void runTest() override {
        beginTest("Transpose effect");

        // Setup
        TransposeEffect effect;
        effect.semitones = 12;

        tracktion::MidiMessageArray buffer;
        buffer.addMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.8f), 0.0);

        MidiBufferContext context {
            .buffer = buffer,
            .start = 0,
            .length = 512,
            .playPosition = tracktion::TimePosition::fromSeconds(0.0),
            .sampleRate = 44100.0
        };

        // Execute
        effect(context);

        // Verify
        expectEquals(buffer[0].getNoteNumber(), 72);
    }
};
```

## Related Patterns

- **Parameter Binding**: Use with `ParameterValue<T>` for automation
- **State Wrappers**: Store effect state in ValueTree with type safety
- **Plugin UI Adapters**: Register custom UI for MIDI effects

## References

- **Implementation**: `src/plugins/uZX/midi_effects/MidiEffect.h`
- **Examples**: `src/plugins/uZX/midi_effects/*.h` (various effects)
- **Tests**: Look for `.test.cpp` files in the same directory
- **Tracktion Plugin API**: See `tracktion-engine` skill documentation
