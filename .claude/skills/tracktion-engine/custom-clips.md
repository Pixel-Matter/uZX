# Custom Clip Types

Complete guide to creating and integrating custom clip types in Tracktion Engine.

## Overview

Tracktion Engine allows you to extend the clip system with custom clip types for specialized data formats or workflows. Custom clips inherit from existing clip classes and integrate seamlessly with the Edit system.

## Why Custom Clips?

Use custom clips when you need to:
- Handle proprietary or specialized file formats
- Implement domain-specific playback behavior
- Store custom metadata or parameters
- Create new types of musical/audio content

## Core Concepts

**Clip Base Classes**: Choose appropriate base (Clip, AudioClipBase, MidiClip, etc.)
**ValueTree Integration**: All state stored in JUCE ValueTree for persistence
**EditItemID**: Unique identifier system for all edit objects
**ClipOwner**: Parent container (Track) that owns the clip

## Creating a Custom Clip

### Step 1: Define XML Type Identifier

```cpp
namespace IDs
{
    const juce::Identifier MYCUSTOMCLIP("MYCUSTOMCLIP");
}
```

### Step 2: Create Clip Class

```cpp
class MyCustomClip : public te::Clip  // or te::MidiClip, te::AudioClipBase
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<MyCustomClip>;

    // Constructor receives ValueTree state and parent
    MyCustomClip(const juce::ValueTree& v,
                 te::EditItemID id,
                 te::ClipOwner& parent)
        : te::Clip(v, parent, id, te::ClipTypeID::custom)
    {
        // Initialize cached values from state
        myParameter.referTo(state, IDs::myParameter, nullptr, 0.5f);
    }

    // Required: Unique type name
    static const char* getClipTypeName() { return "My Custom Clip"; }

    // Required: XML type identifier
    juce::Identifier getTypeIdentifier() const override
    {
        return IDs::MYCUSTOMCLIP;
    }

    // Optional: Default color
    juce::Colour getDefaultColour() const override
    {
        return juce::Colours::purple;
    }

    // Optional: Description for UI
    juce::String getSelectableDescription() override
    {
        return "Custom Clip: " + getName();
    }

    // Custom data access
    float getMyParameter() const { return myParameter; }
    void setMyParameter(float value)
    {
        myParameter = value;
        changed();  // Notify listeners
    }

private:
    juce::CachedValue<float> myParameter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyCustomClip)
};
```

### Step 3: Extending MidiClip for MIDI-Based Custom Clips

```cpp
class MyMidiBasedClip : public te::MidiClip
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<MyMidiBasedClip>;

    MyMidiBasedClip(const juce::ValueTree& v,
                    te::EditItemID id,
                    te::ClipOwner& parent)
        : te::MidiClip(v, id, parent)
    {
    }

    // Override to customize MIDI generation
    void initialise() override
    {
        te::MidiClip::initialise();

        // Add custom MIDI data
        auto& sequence = getSequence();
        // ... populate sequence
    }

    juce::Identifier getTypeIdentifier() const override
    {
        return IDs::MYMIDICLIP;
    }

    juce::Colour getDefaultColour() const override
    {
        return juce::Colours::green;
    }

    // Custom initialization from file/data
    void loadFromFile(const juce::File& file)
    {
        // Parse file
        // Populate MIDI sequence
        auto& sequence = getSequence();
        sequence.clear(nullptr);

        // Add notes based on file data
        for (auto& noteData : parseFile(file))
        {
            sequence.addNote(noteData.pitch,
                           noteData.startBeat,
                           noteData.lengthBeats,
                           noteData.velocity,
                           0, nullptr);
        }
    }

private:
    std::vector<NoteData> parseFile(const juce::File& file)
    {
        // Implementation specific
        return {};
    }
};
```

## Registering Custom Clip Type

### Using EngineBehaviour (Requires Tracktion Engine Patch)

**IMPORTANT**: Stock Tracktion Engine does not support custom clip types. You need to patch the engine to add the extension point.

Apply this patch to `tracktion_engine/model/clips/tracktion_Clip.cpp` in the `createNewClipObject()` function:

```cpp
static Clip::Ptr createNewClipObject (const juce::ValueTree& v, EditItemID newClipID, ClipOwner& targetParent)
{
    auto type = v.getType();

    // Built-in clip types
    if (type == IDs::AUDIOCLIP)     return new WaveAudioClip (v, newClipID, targetParent);
    if (type == IDs::MIDICLIP)      return new MidiClip (v, newClipID, targetParent);
    if (type == IDs::MARKERCLIP)    return new MarkerClip (v, newClipID, targetParent);
    if (type == IDs::STEPCLIP)      return new StepClip (v, newClipID, targetParent);
    if (type == IDs::CHORDCLIP)     return new ChordClip (v, newClipID, targetParent);
    if (type == IDs::ARRANGERCLIP)  return new ArrangerClip (v, newClipID, targetParent);
    if (type == IDs::CONTAINERCLIP) return new ContainerClip (v, newClipID, targetParent);
    if (type == IDs::EDITCLIP)      return createNewEditClip (v, newClipID, targetParent);

    // ADD THIS CUSTOM CLIP EXTENSION POINT:
    auto& edit = targetParent.getClipOwnerEdit();
    if (auto customClip = edit.engine.getEngineBehaviour().createCustomClipForState (v, newClipID, targetParent))
        return customClip;
    // END PATCH

    jassertfalse;
    return {};
}
```

Then add the virtual method to `EngineBehaviour` class in `tracktion_engine/utilities/tracktion_EngineBehaviour.h`:

```cpp
class EngineBehaviour
{
public:
    virtual ~EngineBehaviour() = default;

    // ADD THIS METHOD:
    /** Override to create custom clip types. Return nullptr for unknown types. */
    virtual Clip* createCustomClipForState (const juce::ValueTree&, EditItemID, ClipOwner&) { return nullptr; }

    // ... existing methods
};
```

### Using the Patch

Once patched, register custom clips via `EngineBehaviour`:

```cpp
// Create custom EngineBehaviour
struct MyEngineBehaviour : te::EngineBehaviour
{
    te::Clip* createCustomClipForState(const juce::ValueTree& v,
                                       te::EditItemID id,
                                       te::ClipOwner& parent) override
    {
        auto type = v.getType();

        // Register your custom clip types
        if (type == IDs::MYCUSTOMCLIP)
            return new MyCustomClip(v, id, parent);

        if (type == IDs::PATTERNCLIP)
            return new PatternClip(v, id, parent);

        // Return nullptr for unknown types
        return nullptr;
    }

    // ... other EngineBehaviour overrides
};

// Set behaviour during engine initialization
engine.setEngineBehaviour(std::make_unique<MyEngineBehaviour>());
```

**How it works:**
- Tracktion Engine's `createNewClipObject()` checks built-in clip types first
- If no match, it calls `engine.getEngineBehaviour().createCustomClipForState()`
- Your custom behaviour creates and returns your clip type
- Returning `nullptr` indicates the type is unknown

### Alternative: Fork Tracktion Engine

If you need extensive customization, consider maintaining a fork of Tracktion Engine with your modifications. This makes the patch more maintainable and version-controllable.

## Inserting Custom Clips

### Helper Function for Insertion

```cpp
static MyCustomClip::Ptr insertMyClip(
    te::ClipOwner& owner,
    const juce::String& name,
    te::ClipPosition position)
{
    auto& edit = owner.getClipOwnerEdit();

    // Check bounds
    if (position.getStart() >= te::Edit::getMaximumEditEnd())
        return {};

    if (position.time.getEnd() > te::Edit::getMaximumEditEnd())
        position.time.getEnd() = te::Edit::getMaximumEditEnd();

    // Generate unique ID
    auto newClipID = edit.createNewItemID();

    // Create state
    juce::ValueTree state(IDs::MYCUSTOMCLIP);
    te::addValueTreeProperties(state,
        te::IDs::name, name,
        te::IDs::start, position.getStart().inSeconds(),
        te::IDs::length, position.getLength().inSeconds(),
        te::IDs::offset, position.getOffset().inSeconds());

    newClipID.writeID(state, nullptr);

    // Insert using Tracktion's insertion system
    if (auto clip = te::insertClipWithState(owner, state))
        return dynamic_cast<MyCustomClip*>(clip);

    return {};
}
```

### Using the Helper

```cpp
// Insert custom clip on track
auto track = edit.getAudioTracks()[0];
auto clip = insertMyClip(*track,
                        "My Clip",
                        { te::TimePosition(0.0), te::TimeDuration(4.0) });

if (clip != nullptr)
{
    clip->setMyParameter(0.75f);
}
```

## State Persistence

### Storing Custom Data

```cpp
class CustomDataClip : public te::Clip
{
public:
    CustomDataClip(const juce::ValueTree& v,
                   te::EditItemID id,
                   te::ClipOwner& parent)
        : te::Clip(v, parent, id, te::ClipTypeID::custom)
    {
        // Bind cached values to state
        parameter1.referTo(state, IDs::param1, nullptr, 0.0f);
        parameter2.referTo(state, IDs::param2, nullptr, 1.0f);
        enableFlag.referTo(state, IDs::enabled, nullptr, true);
        textData.referTo(state, IDs::text, nullptr, juce::String());

        // Complex data as child ValueTree
        dataTree = state.getOrCreateChildWithName(IDs::customData, nullptr);
    }

    // Accessors automatically read/write to state
    float getParameter1() const { return parameter1; }
    void setParameter1(float v)
    {
        parameter1 = v;
        changed();
    }

    // Child tree for complex data
    void addCustomDataPoint(double time, float value)
    {
        juce::ValueTree point(IDs::dataPoint);
        point.setProperty(IDs::time, time, nullptr);
        point.setProperty(IDs::value, value, nullptr);
        dataTree.appendChild(point, nullptr);
    }

    std::vector<DataPoint> getCustomData() const
    {
        std::vector<DataPoint> result;
        for (auto child : dataTree)
        {
            if (child.hasType(IDs::dataPoint))
            {
                result.push_back({
                    child[IDs::time],
                    child[IDs::value]
                });
            }
        }
        return result;
    }

private:
    juce::CachedValue<float> parameter1, parameter2;
    juce::CachedValue<bool> enableFlag;
    juce::CachedValue<juce::String> textData;
    juce::ValueTree dataTree;

    struct DataPoint { double time; float value; };
};
```

### Loading from External Files

```cpp
class FileBasedClip : public te::Clip
{
public:
    void initialise() override
    {
        te::Clip::initialise();

        // Load data from source file on first initialization
        if (!hasLoadedData)
        {
            if (auto file = getSourceFile())
                loadFromFile(*file);

            hasLoadedData = true;
        }
    }

    void loadFromFile(const juce::File& file)
    {
        // Parse file and populate state
        auto data = parseCustomFormat(file);

        for (auto& item : data)
        {
            juce::ValueTree child(IDs::item);
            child.setProperty(IDs::value, item.value, nullptr);
            state.appendChild(child, nullptr);
        }

        // Store source file reference
        te::SourceFileReference::setToProjectFileReference(state, file, edit);
    }

    std::optional<juce::File> getSourceFile() const
    {
        if (auto ref = te::SourceFileReference::findFileFromProjectReference(edit, state))
            return ref;
        return {};
    }

private:
    bool hasLoadedData = false;

    struct CustomData { float value; };
    std::vector<CustomData> parseCustomFormat(const juce::File& file);
};
```

## Playback Integration

### Audio Clip with Custom Processing

```cpp
class ProcessedAudioClip : public te::WaveAudioClip
{
public:
    // Override to add custom audio processing
    void renderAudio(const te::AudioRenderContext& rc) override
    {
        // First, render base audio
        te::WaveAudioClip::renderAudio(rc);

        // Apply custom processing
        auto& buffer = rc.destBuffer;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* samples = buffer.getWritePointer(ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                // Custom effect (e.g., bit crushing)
                samples[i] = applyEffect(samples[i]);
            }
        }
    }

private:
    float applyEffect(float sample)
    {
        // Custom processing
        return sample;
    }
};
```

### MIDI Clip with Generative Content

```cpp
class GenerativeMidiClip : public te::MidiClip
{
public:
    void initialise() override
    {
        te::MidiClip::initialise();
        regenerateContent();
    }

    void regenerateContent()
    {
        auto& sequence = getSequence();
        sequence.clear(nullptr);

        // Generate notes based on parameters
        auto scale = getScale();
        auto pattern = getPattern();

        for (int step = 0; step < 16; ++step)
        {
            if (pattern[step])
            {
                int note = scale[step % scale.size()];
                double beat = step * 0.25;  // 16th notes

                sequence.addNote(note, beat, 0.2, 100, 0, nullptr);
            }
        }
    }

    void setPattern(const std::vector<bool>& newPattern)
    {
        // Store pattern and regenerate
        regenerateContent();
    }

private:
    std::vector<int> getScale() { return {60, 62, 64, 65, 67}; }  // C major pentatonic
    std::vector<bool> getPattern() { return {true, false, true, false, /*...*/}; }
};
```

## UI Integration

### Custom Clip Component

```cpp
class MyCustomClipComponent : public te::ClipComponent
{
public:
    MyCustomClipComponent(te::EditViewState& evs,
                         te::Clip& c,
                         te::ClipTrack& t)
        : te::ClipComponent(evs, c, t)
    {
        if (auto customClip = dynamic_cast<MyCustomClip*>(&c))
            myClip = customClip;
    }

    void paint(juce::Graphics& g) override
    {
        // Custom rendering
        g.fillAll(myClip->getDefaultColour().withAlpha(0.3f));

        g.setColour(juce::Colours::white);
        g.drawText(myClip->getName(),
                  getLocalBounds(),
                  juce::Justification::centred);

        // Draw custom visualization
        if (myClip)
        {
            float param = myClip->getMyParameter();
            int barHeight = (int)(getHeight() * param);

            g.setColour(juce::Colours::yellow);
            g.fillRect(0, getHeight() - barHeight, getWidth(), barHeight);
        }
    }

private:
    MyCustomClip* myClip = nullptr;
};
```

### Registering Custom Component

```cpp
// In your EditComponent or similar class
te::ClipComponent* createCustomClipComponent(te::EditViewState& evs,
                                            te::Clip& clip,
                                            te::ClipTrack& track)
{
    if (dynamic_cast<MyCustomClip*>(&clip))
        return new MyCustomClipComponent(evs, clip, track);

    // Fall back to default components
    if (auto midiClip = dynamic_cast<te::MidiClip*>(&clip))
        return new te::MidiClipComponent(evs, *midiClip, track);

    if (auto audioClip = dynamic_cast<te::WaveAudioClip*>(&clip))
        return new te::WaveClipComponent(evs, *audioClip, track);

    return new te::ClipComponent(evs, clip, track);
}
```

## Complete Example: Pattern Clip

```cpp
// Pattern clip for step sequencing
class PatternClip : public te::MidiClip
{
public:
    static const int NUM_STEPS = 16;

    using Ptr = juce::ReferenceCountedObjectPtr<PatternClip>;

    PatternClip(const juce::ValueTree& v,
                te::EditItemID id,
                te::ClipOwner& parent)
        : te::MidiClip(v, id, parent)
    {
        // Load pattern from state
        patternData = state.getOrCreateChildWithName(IDs::pattern, nullptr);

        // Initialize if empty
        if (patternData.getNumChildren() == 0)
        {
            for (int i = 0; i < NUM_STEPS; ++i)
            {
                juce::ValueTree step(IDs::step);
                step.setProperty(IDs::enabled, false, nullptr);
                step.setProperty(IDs::note, 60, nullptr);
                step.setProperty(IDs::velocity, 100, nullptr);
                patternData.appendChild(step, nullptr);
            }
        }

        rebuildMidi();
    }

    static Ptr insertPatternClip(te::ClipOwner& owner,
                                 const juce::String& name,
                                 te::ClipPosition position)
    {
        auto& edit = owner.getClipOwnerEdit();
        auto id = edit.createNewItemID();

        juce::ValueTree state(IDs::PATTERNCLIP);
        te::addValueTreeProperties(state,
            te::IDs::name, name,
            te::IDs::start, position.getStart().inSeconds(),
            te::IDs::length, position.getLength().inSeconds(),
            te::IDs::offset, position.getOffset().inSeconds());

        id.writeID(state, nullptr);

        if (auto clip = te::insertClipWithState(owner, state))
            return dynamic_cast<PatternClip*>(clip);

        return {};
    }

    void setStep(int index, bool enabled, int note = 60, int velocity = 100)
    {
        if (index < 0 || index >= NUM_STEPS)
            return;

        auto step = patternData.getChild(index);
        step.setProperty(IDs::enabled, enabled, nullptr);
        step.setProperty(IDs::note, note, nullptr);
        step.setProperty(IDs::velocity, velocity, nullptr);

        rebuildMidi();
    }

    bool getStep(int index) const
    {
        if (index < 0 || index >= NUM_STEPS)
            return false;

        return patternData.getChild(index)[IDs::enabled];
    }

    juce::Identifier getTypeIdentifier() const override
    {
        return IDs::PATTERNCLIP;
    }

    juce::Colour getDefaultColour() const override
    {
        return juce::Colours::orange;
    }

private:
    juce::ValueTree patternData;

    void rebuildMidi()
    {
        auto& sequence = getSequence();
        sequence.clear(nullptr);

        double stepLength = 0.25;  // 16th note

        for (int i = 0; i < NUM_STEPS; ++i)
        {
            auto step = patternData.getChild(i);

            if (step[IDs::enabled])
            {
                int note = step[IDs::note];
                int velocity = step[IDs::velocity];
                double beat = i * stepLength;

                sequence.addNote(note, beat, stepLength * 0.9,
                               velocity, 0, nullptr);
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternClip)
};
```

## Best Practices

1. **Use ValueTree**: Store all state in ValueTree for automatic persistence
2. **CachedValue**: Use `juce::CachedValue<T>` for automatic state synchronization
3. **EditItemID**: Always use Edit's ID system for unique identification
4. **Initialization**: Override `initialise()` for one-time setup after construction
5. **Change Notification**: Call `changed()` when clip state changes
6. **Base Classes**: Choose appropriate base class (Clip, MidiClip, AudioClipBase)
7. **Type Identifiers**: Use unique Identifier for each custom clip type
8. **Source References**: Use `SourceFileReference` for external file dependencies
9. **Undo Support**: Pass appropriate UndoManager to ValueTree operations
10. **Thread Safety**: Clip state changes should happen on message thread

## Common Patterns

### Clip with External Data Source

```cpp
class DataSourceClip : public te::Clip
{
    void setDataSource(const juce::File& file)
    {
        dataFile = file;
        te::SourceFileReference::setToProjectFileReference(state, file, edit);
        reloadData();
    }

    void reloadData()
    {
        if (dataFile.existsAsFile())
        {
            // Load and cache data
        }
    }

private:
    juce::File dataFile;
};
```

### Procedural/Algorithmic Clip

```cpp
class AlgorithmicClip : public te::MidiClip
{
    void setSeed(int newSeed)
    {
        seed = newSeed;
        state.setProperty(IDs::seed, seed, nullptr);
        regenerate();
    }

    void regenerate()
    {
        juce::Random rng(seed);
        auto& sequence = getSequence();
        sequence.clear(nullptr);

        // Generate notes algorithmically
    }

private:
    int seed = 0;
};
```
