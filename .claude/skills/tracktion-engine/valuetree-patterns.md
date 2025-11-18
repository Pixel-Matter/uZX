# ValueTree State Management Patterns

Best practices and patterns for managing state in Tracktion Engine using JUCE's ValueTree.

## Overview

Tracktion Engine uses JUCE's ValueTree extensively for:
- Edit/Clip/Track state persistence
- Undo/redo support
- Change notification
- Automatic serialization

## Core Concepts

**ValueTree**: Hierarchical tree structure for data
**CachedValue**: Automatic synchronization between variable and ValueTree
**Listener**: Notification system for state changes
**Identifier**: Type-safe property names

## Basic Patterns

### CachedValue for Automatic Sync

```cpp
class MyClass
{
public:
    MyClass(const juce::ValueTree& v, juce::UndoManager* um)
        : state(v)
    {
        // Bind cached values to state properties
        volume.referTo(state, IDs::volume, um, 0.75f);  // default: 0.75
        pan.referTo(state, IDs::pan, um, 0.0f);
        muted.referTo(state, IDs::mute, um, false);
    }

    // Accessors automatically read/write to state
    float getVolume() const { return volume; }
    void setVolume(float v) { volume = v; }  // Automatically updates state

private:
    juce::ValueTree state;
    juce::CachedValue<float> volume, pan;
    juce::CachedValue<bool> muted;
};
```

### State Wrappers

```cpp
class TrackState : private juce::ValueTree::Listener
{
public:
    TrackState(juce::ValueTree trackTree, juce::UndoManager* um)
        : state(trackTree)
    {
        height.referTo(state, IDs::height, um, 100);
        collapsed.referTo(state, IDs::collapsed, um, false);

        state.addListener(this);
    }

    ~TrackState() override
    {
        state.removeListener(this);
    }

    int getHeight() const { return height; }
    void setHeight(int h) { height = h; }

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void stateChanged() = 0;
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

private:
    void valueTreePropertyChanged(juce::ValueTree&,
                                 const juce::Identifier&) override
    {
        listeners.call(&Listener::stateChanged);
    }

    juce::ValueTree state;
    juce::CachedValue<int> height;
    juce::CachedValue<bool> collapsed;
    juce::ListenerList<Listener> listeners;
};
```

## Listening to Changes

### Property Change Listener

```cpp
struct MyListener : juce::ValueTree::Listener
{
    void valueTreePropertyChanged(juce::ValueTree& tree,
                                 const juce::Identifier& property) override
    {
        if (property == IDs::volume)
        {
            float newVolume = tree[IDs::volume];
            updateVolume(newVolume);
        }
    }

    void valueTreeChildAdded(juce::ValueTree& parent,
                            juce::ValueTree& child) override
    {
        // Child added
    }

    void valueTreeChildRemoved(juce::ValueTree& parent,
                              juce::ValueTree& child,
                              int index) override
    {
        // Child removed
    }

    void valueTreeChildOrderChanged(juce::ValueTree& parent,
                                   int oldIndex,
                                   int newIndex) override
    {
        // Child moved
    }
};
```

### Batch Updates

```cpp
// Suppress notifications during batch changes
{
    juce::ValueTree::ScopedEventSuppressor suppressor(state);

    state.setProperty(IDs::volume, 0.5f, nullptr);
    state.setProperty(IDs::pan, 0.0f, nullptr);
    state.setProperty(IDs::mute, false, nullptr);

    // All changes notified once when suppressor destroyed
}
```

## Child Management

### Adding/Removing Children

```cpp
// Add child
juce::ValueTree child(IDs::CLIP);
child.setProperty(IDs::name, "My Clip", nullptr);
parent.appendChild(child, undoManager);

// Remove child
parent.removeChild(childIndex, undoManager);

// Get or create child
auto child = state.getOrCreateChildWithName(IDs::settings, undoManager);

// Find child
auto clip = state.getChildWithName(IDs::CLIP);
```

### Iterating Children

```cpp
// Iterate all children
for (auto child : state)
{
    if (child.hasType(IDs::CLIP))
        processClip(child);
}

// Iterate specific type
for (int i = 0; i < state.getNumChildren(); ++i)
{
    auto child = state.getChild(i);
    if (child.hasType(IDs::TRACK))
        processTrack(child);
}
```

## Advanced Patterns

### Lazy Initialization

```cpp
class EditState
{
public:
    EditState(te::Edit& e)
        : edit(e)
        , state(e.state.getOrCreateChildWithName(IDs::EDITVIEWSTATE,
                                                 e.getUndoManager()))
    {
        // Initialize if new
        if (state.getNumProperties() == 0)
        {
            state.setProperty(IDs::zoom, 1.0, nullptr);
            state.setProperty(IDs::scrollPos, 0.0, nullptr);
        }

        // Bind cached values
        zoom.referTo(state, IDs::zoom, e.getUndoManager());
        scrollPos.referTo(state, IDs::scrollPos, e.getUndoManager());
    }

private:
    te::Edit& edit;
    juce::ValueTree state;
    juce::CachedValue<double> zoom, scrollPos;
};
```

### State Migration

```cpp
void migrateState(juce::ValueTree& state)
{
    // Check version
    int version = state.getProperty(IDs::version, 1);

    if (version < 2)
    {
        // Migrate v1 → v2
        if (state.hasProperty("oldProperty"))
        {
            auto value = state["oldProperty"];
            state.removeProperty("oldProperty", nullptr);
            state.setProperty(IDs::newProperty, value, nullptr);
        }

        state.setProperty(IDs::version, 2, nullptr);
    }

    if (version < 3)
    {
        // Migrate v2 → v3
        // ...
        state.setProperty(IDs::version, 3, nullptr);
    }
}
```

### Deep Copying

```cpp
// Deep copy ValueTree
juce::ValueTree copyState(const juce::ValueTree& source)
{
    juce::ValueTree copy = source.createCopy();
    return copy;
}

// Selective copying
juce::ValueTree copyPropertiesOnly(const juce::ValueTree& source)
{
    juce::ValueTree copy(source.getType());

    // Copy properties only, not children
    for (int i = 0; i < source.getNumProperties(); ++i)
    {
        auto name = source.getPropertyName(i);
        copy.setProperty(name, source.getProperty(name), nullptr);
    }

    return copy;
}
```

## Thread Safety

### Message Thread Updates

```cpp
// Always modify state on message thread
juce::MessageManager::callAsync([state = this->state]() mutable
{
    state.setProperty(IDs::value, newValue, nullptr);
});
```

### Reading from Any Thread

```cpp
// Safe to read from any thread (atomic)
float value = state[IDs::volume];

// But listeners fire on message thread
```

## Undo/Redo Integration

### UndoManager Usage

```cpp
// Pass UndoManager to make changes undoable
auto um = edit.getUndoManager();

um->beginNewTransaction("Change Volume");
state.setProperty(IDs::volume, 0.75f, um);

// Multiple changes in same transaction
state.setProperty(IDs::pan, 0.0f, um);

// Next transaction
um->beginNewTransaction("Mute Track");
state.setProperty(IDs::mute, true, um);

// Undo
um->undo();  // Unmutes

// Redo
um->redo();  // Mutes again
```

### Transaction Grouping

```cpp
void applyPreset(const Preset& preset, juce::UndoManager* um)
{
    um->beginNewTransaction("Apply Preset");

    // All changes grouped in single undo step
    state.setProperty(IDs::param1, preset.value1, um);
    state.setProperty(IDs::param2, preset.value2, um);
    state.setProperty(IDs::param3, preset.value3, um);

    um->beginNewTransaction();  // Commits transaction
}
```

## Serialization

### Save/Load

```cpp
// Save to file
bool saveState(const juce::ValueTree& state, const juce::File& file)
{
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    if (xml != nullptr)
        return xml->writeTo(file);

    return false;
}

// Load from file
juce::ValueTree loadState(const juce::File& file)
{
    if (auto xml = juce::parseXML(file))
        return juce::ValueTree::fromXml(*xml);

    return {};
}
```

### Binary Format

```cpp
// Save as binary (more efficient)
void saveBinary(const juce::ValueTree& state, juce::OutputStream& stream)
{
    state.writeToStream(stream);
}

// Load from binary
juce::ValueTree loadBinary(juce::InputStream& stream)
{
    return juce::ValueTree::readFromStream(stream);
}
```

## Best Practices

1. **Use CachedValue**: Automatic synchronization, less code
2. **Identifiers**: Use const Identifier for type safety
3. **UndoManager**: Always pass for user-facing changes
4. **Message Thread**: Modify state only on message thread
5. **Batch Updates**: Use ScopedEventSuppressor for multiple changes
6. **Listeners**: Remove listeners in destructor
7. **Defaults**: Always provide default values in referTo()
8. **Child Management**: Use getOrCreateChildWithName() for lazy init
9. **Type Checks**: Use hasType() when iterating mixed children
10. **Deep Copies**: Use createCopy() for cloning entire trees

## Common Patterns

### Transient State (Not Persisted)

```cpp
class EditViewState
{
public:
    EditViewState(juce::ValueTree persistentState)
        : state(persistentState)
    {
        // Persistent
        zoom.referTo(state, IDs::zoom, nullptr, 1.0);

        // Transient (not in ValueTree)
        // viewWidthPx stored as regular member
    }

    int getViewWidthPx() const { return viewWidthPx; }
    void setViewWidthPx(int w) { viewWidthPx = w; }

private:
    juce::ValueTree state;
    juce::CachedValue<double> zoom;  // Persistent
    int viewWidthPx = 1024;           // Transient
};
```

### Computed Properties

```cpp
class ClipState
{
public:
    // Stored properties
    double getStart() const { return start; }
    double getLength() const { return length; }

    // Computed property (not stored)
    double getEnd() const { return start.get() + length.get(); }

    void setEnd(double newEnd)
    {
        length = newEnd - start.get();
    }

private:
    juce::CachedValue<double> start, length;
};
```
