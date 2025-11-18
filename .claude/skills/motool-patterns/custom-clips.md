# Custom Clip Integration Pattern

## Overview

The MoTool custom clip pattern demonstrates how to extend Tracktion Engine with custom clip types while maintaining seamless integration with the Edit's clip system.

## Problem Statement

Extending Tracktion Engine with custom clip types faces several challenges:

1. **Clip registration**: How to register custom clip types with Edit
2. **ValueTree integration**: Custom clips need persistent state
3. **Type safety**: Need compile-time safety for custom clip APIs
4. **Factory methods**: Creating clips with proper initialization
5. **Inheritance limitations**: Can't modify Tracktion's clip creation logic
6. **ID management**: Custom clips need unique XML type identifiers

## Solution Architecture

### Dual Inheritance Pattern

```cpp
class PsgClip : public te::MidiClip,    // Inherit Tracktion clip behavior
                public CustomClip        // Add custom factory utilities
{
    // Custom clip implementation
};
```

**Key insight**:
- Inherit from Tracktion base for integration
- Mix in utility class for custom factory methods
- Avoid diamond problem (CustomClip has no state)

**Files**:
- `src/models/PsgClip.h` - Custom clip implementation
- `src/models/CustomClip.h` - Factory utilities

## Core Components

### 1. CustomClip Utility Class

A stateless helper providing factory methods for custom clip types:

```cpp
class CustomClip {
public:
    enum class Type {
        unknown,
        psg,      // PSG music file clip
        // Add more custom types here
    };

    static te::Clip* insertClipWithState(
        te::ClipOwner& parent,
        const juce::ValueTree& stateToUse,
        const juce::String& name,
        Type type,
        te::ClipPosition position,
        te::DeleteExistingClips deleteExistingClips,
        bool allowSpottingAdjustment
    );

private:
    static juce::Identifier clipTypeToXMLType(Type t);
    static void updateClipState(ValueTree& state, ...);
    static ValueTree createNewClipState(const String& name, Type type, ...);
};
```

**File**: `src/models/CustomClip.h:14-88`

**Design note**: Consider refactoring to namespace instead of class (see TODO at line 12)

### 2. PsgClip Implementation

Custom clip for PSG (AY chip music) files:

```cpp
class PsgClip : public te::MidiClip, public CustomClip {
public:
    using Ptr = juce::ReferenceCountedObjectPtr<PsgClip>;

    PsgClip(const juce::ValueTree& v, te::EditItemID id, te::ClipOwner& parent_)
        : te::MidiClip(v, id, parent_) {}

    void initialise() override;
    String getSelectableDescription() override;
    Colour getDefaultColour() const override { return Colours::blue; }

    PsgList& getPsg() const noexcept;

    // Factory methods
    static Ptr insertTo(
        te::ClipOwner& owner,
        uZX::PsgFile& psgFile,
        te::ClipPosition position
    );

    static Ptr insertTo(
        te::ClipOwner& owner,
        uZX::PsgData& data,
        te::ClipPosition position,
        String name = {}
    );

private:
    std::unique_ptr<PsgList> psgList;  // Custom data

    void loadFrom(uZX::PsgData& data);
    void loadFrom(uZX::PsgFile& psgFile);
};
```

**File**: `src/models/PsgClip.h:14-55`

## Implementation Details

### Type Registration

#### 1. XML Type Mapping

```cpp
static juce::Identifier clipTypeToXMLType(Type t) {
    switch (t) {
        case Type::psg:     return IDs::PSGCLIP;  // "PSGCLIP" in ValueTree
        case Type::unknown:
        default:            jassertfalse; return nullptr;
    }
}
```

**File**: `src/models/CustomClip.h:63-69`

**Important**: Add `IDs::PSGCLIP` to centralized ID namespace

#### 2. Clip State Creation

```cpp
static ValueTree createNewClipState(
    const String& name,
    Type type,
    te::EditItemID itemID,
    te::ClipPosition position
) {
    ValueTree state(clipTypeToXMLType(type));  // e.g., "PSGCLIP"

    te::addValueTreeProperties(state,
        te::IDs::name, name,
        te::IDs::start, position.getStart().inSeconds(),
        te::IDs::length, position.getLength().inSeconds(),
        te::IDs::offset, position.getOffset().inSeconds()
    );

    itemID.writeID(state, nullptr);  // Add unique ID

    return state;
}
```

**File**: `src/models/CustomClip.h:82-87`

### Factory Method Pattern

#### insertClipWithState Implementation

```cpp
static te::Clip* insertClipWithState(
    te::ClipOwner& parent,
    const juce::ValueTree& stateToUse,
    const juce::String& name,
    Type type,
    te::ClipPosition position,
    te::DeleteExistingClips deleteExistingClips,
    bool allowSpottingAdjustment
) {
    auto& edit = parent.getClipOwnerEdit();

    // 1. Validate position
    if (position.getStart() >= Edit::getMaximumEditEnd())
        return {};

    if (position.time.getEnd() > Edit::getMaximumEditEnd())
        position.time.getEnd() = Edit::getMaximumEditEnd();

    // 2. Unfreeze track if needed
    if (auto track = dynamic_cast<Track*>(&parent))
        track->setFrozen(false, Track::groupFreeze);

    // 3. Delete existing clips in range if requested
    if (deleteExistingClips == DeleteExistingClips::yes)
        deleteRegion(parent, position.time);

    // 4. Create new clip ID
    auto newClipID = edit.createNewItemID();

    // 5. Create or update state
    ValueTree newState;
    if (stateToUse.isValid()) {
        jassert(stateToUse.hasType(clipTypeToXMLType(type)));
        newState = stateToUse;
        updateClipState(newState, name, newClipID, position);
    } else {
        newState = createNewClipState(name, type, newClipID, position);
    }

    // 6. Insert into Tracktion's clip system
    if (auto newClip = te::insertClipWithState(parent, newState)) {
        // 7. Adjust for spotting point if needed
        if (allowSpottingAdjustment) {
            newClip->setStart(
                std::max(0_tp, newClip->getPosition().getStart() -
                         toDuration(newClip->getSpottingPoint())),
                false, false
            );
        }

        return newClip;
    }

    return {};
}
```

**File**: `src/models/CustomClip.h:21-60`

**Key steps**:
1. Validate clip position against edit limits
2. Unfreeze track (frozen tracks can't have clips added)
3. Optionally delete overlapping clips
4. Generate unique EditItemID
5. Create ValueTree state with proper XML type
6. Delegate to Tracktion's `insertClipWithState()`
7. Adjust for spotting point (audio alignment)

### PsgClip Factory Methods

#### Inserting from PSG File

```cpp
PsgClip::Ptr PsgClip::insertTo(
    te::ClipOwner& owner,
    uZX::PsgFile& psgFile,
    te::ClipPosition position
) {
    // Create clip using CustomClip factory
    auto clip = dynamic_cast<PsgClip*>(
        CustomClip::insertClipWithState(
            owner,
            {},  // No pre-existing state
            psgFile.name,
            CustomClip::Type::psg,
            position,
            te::DeleteExistingClips::no,
            false  // No spotting adjustment
        )
    );

    if (clip != nullptr) {
        clip->loadFrom(psgFile);  // Load PSG-specific data
    }

    return clip;
}
```

**Usage**:
```cpp
uZX::PsgFile psgFile = loadPsgFile("music.psg");
auto position = te::ClipPosition { te::TimeRange { 0_tp, 10_tp } };
auto clip = PsgClip::insertTo(*track, psgFile, position);
```

## Edit Integration

### Clip Type Registration

For Tracktion to recognize custom clips, register them in Edit initialization:

```cpp
// In Edit setup or plugin initialization
edit.getClipTypeManager().registerClipType<PsgClip>(IDs::PSGCLIP);
```

**Note**: Exact registration API may vary by Tracktion version

### ValueTree Structure

Example PsgClip state in Edit's ValueTree:

```
TRACK
└── PSGCLIP
    ├── id: "1234567890"
    ├── name: "MyMusic.psg"
    ├── start: 0.0
    ├── length: 30.5
    ├── offset: 0.0
    ├── colour: "0xff0000ff"
    └── [custom PSG data...]
```

## Advanced Features

### Custom Clip Data

PsgClip extends MidiClip to add PSG-specific data:

```cpp
private:
    std::unique_ptr<PsgList> psgList;  // PSG frame list

void PsgClip::loadFrom(uZX::PsgData& data) {
    psgList = std::make_unique<PsgList>();
    psgList->load(data);

    // Convert PSG data to MIDI sequence
    auto& sequence = getSequence();
    for (const auto& frame : psgList->frames) {
        // Add MIDI notes based on PSG register data
        sequence.addNote(pitch, beatPos, beatDuration, velocity, colour, nullptr);
    }
}
```

**Pattern**: Store custom data, sync to MIDI for Tracktion playback

### Custom Rendering

Override `MidiClip` methods for custom behavior:

```cpp
String PsgClip::getSelectableDescription() override {
    return "PSG Clip: " + getName();
}

Colour PsgClip::getDefaultColour() const override {
    return Colours::blue;  // Visual distinction in timeline
}
```

## Usage Examples

### Example 1: Creating PSG Clip from File

```cpp
void importPsgFile(te::AudioTrack& track, const File& file) {
    uZX::PsgFile psgFile;
    if (!psgFile.load(file)) {
        // Handle error
        return;
    }

    // Calculate duration from PSG frame count
    double durationSecs = psgFile.frameCount / 50.0;  // 50 Hz frame rate

    auto position = te::ClipPosition {
        te::TimeRange {
            te::TimePosition::fromSeconds(0.0),
            te::TimeDuration::fromSeconds(durationSecs)
        }
    };

    auto clip = PsgClip::insertTo(track, psgFile, position);

    if (clip != nullptr) {
        DBG("PSG clip created: " << clip->getName());
    }
}
```

### Example 2: Programmatically Creating Clips

```cpp
void createPsgClipFromData(te::AudioTrack& track, uZX::PsgData& data) {
    auto position = te::ClipPosition {
        te::TimeRange { 5_tp, 15_tp }  // 5-15 seconds
    };

    auto clip = PsgClip::insertTo(
        track,
        data,
        position,
        "Generated PSG"
    );

    // Customize clip
    if (clip != nullptr) {
        clip->setColour(Colours::green);
        clip->setSpeedRatio(1.5);  // Play 1.5x faster
    }
}
```

### Example 3: Accessing Custom Clip Data

```cpp
void processPsgClips(te::Edit& edit) {
    for (auto track : te::getAudioTracks(edit)) {
        for (auto clip : track->getClips()) {
            if (auto psgClip = dynamic_cast<PsgClip*>(clip)) {
                // Access PSG-specific data
                auto& psgList = psgClip->getPsg();

                // Process PSG frames
                for (const auto& frame : psgList.frames) {
                    // ...
                }
            }
        }
    }
}
```

## Best Practices

### ✅ DO

1. **Inherit from appropriate base**: Use `MidiClip` for musical clips, `AudioClip` for audio
2. **Provide factory methods**: Static `insertTo()` methods for type-safe creation
3. **Validate position**: Check against `Edit::getMaximumEditEnd()`
4. **Generate unique IDs**: Use `edit.createNewItemID()`
5. **Register XML types**: Add custom Identifiers to centralized `IDs` namespace
6. **Override getSelectableDescription()**: Provide meaningful clip names
7. **Custom default color**: Use `getDefaultColour()` for visual distinction
8. **Test serialization**: Ensure clips save/load correctly from Edit files

### ❌ DON'T

1. **Forget clip registration**: Must register custom types with Edit
2. **Skip ID writing**: Always call `itemID.writeID(state, nullptr)`
3. **Ignore position limits**: Clips beyond `getMaximumEditEnd()` crash
4. **Modify frozen tracks**: Check/unfreeze before adding clips
5. **Forget reference counting**: Use `Ptr` typedef for clip pointers
6. **Break Tracktion invariants**: Follow `ClipPosition` rules (start, length, offset)

## Testing Strategy

```cpp
class PsgClipTest : public juce::UnitTest {
public:
    PsgClipTest() : UnitTest("PsgClip", "MoTool") {}

    void runTest() override {
        beginTest("Create PSG clip");

        // Setup
        auto engine = createTestEngine();
        auto edit = createTestEdit(engine);
        auto track = createTestTrack(*edit);

        uZX::PsgData data;
        data.frames = { /* test data */ };

        // Create clip
        auto clip = PsgClip::insertTo(
            *track,
            data,
            te::ClipPosition { te::TimeRange { 0_tp, 10_tp } },
            "Test Clip"
        );

        // Verify
        expect(clip != nullptr);
        expectEquals(clip->getName(), juce::String("Test Clip"));
        expectEquals(clip->getPosition().getStart().inSeconds(), 0.0);
        expectEquals(clip->getPosition().getLength().inSeconds(), 10.0);

        beginTest("PSG data loaded");
        expect(clip->getPsg().frames.size() > 0);
    }
};
```

## Performance Considerations

- **Clip creation**: `O(1)` for state creation, `O(n)` for MIDI conversion
- **Lookup overhead**: Dynamic cast for type checking (consider alternative registry)
- **Memory**: Custom data stored separately from Tracktion state
- **Serialization**: ValueTree handles persistence automatically

## Common Pitfalls

### 1. Forgetting Clip Registration

**Problem**: Custom clips not recognized when loading Edit from file

**Solution**:
```cpp
edit.getClipTypeManager().registerClipType<PsgClip>(IDs::PSGCLIP);
```

### 2. ID Collision

**Problem**: Multiple clips with same ID crash Edit

**Solution**: Always use `edit.createNewItemID()` for new clips

### 3. Position Validation

**Problem**: Clips beyond edit limit cause crashes

**Solution**:
```cpp
if (position.getStart() >= Edit::getMaximumEditEnd())
    return {};  // Reject invalid position
```

## Future Enhancements

Consider refactoring CustomClip to namespace:

```cpp
namespace CustomClip {
    enum class Type { unknown, psg };

    Clip* insertClipWithState(...);
    Identifier clipTypeToXMLType(Type t);
}
```

**Benefits**: No dual inheritance, clearer utility role

## Related Patterns

- **State Wrappers**: Custom clips can have view state wrappers
- **Plugin UI Adapters**: UI components can specialize for custom clips
- **Parameter Binding**: Clip properties can use parameter system

## References

- **Implementation**: `src/models/PsgClip.h`, `src/models/CustomClip.h`
- **PSG Data**: `src/formats/psg/PsgFile.h`, `src/models/PsgList.h`
- **Tracktion Clip API**: See `tracktion-engine` skill documentation
- **Edit Structure**: Tracktion Engine docs on Edit/Track/Clip hierarchy
