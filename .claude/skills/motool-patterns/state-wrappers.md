# State Management Wrappers

## Overview

MoTool's state wrapper pattern provides type-safe, reactive wrappers around JUCE `ValueTree` state for managing complex view state (zoom, pan, track heights, UI preferences).

## Problem Statement

Raw `ValueTree` usage challenges:

1. **Type safety**: Properties are `var` type, lose compile-time checks
2. **Property ID management**: String IDs scattered across codebase
3. **Change notification**: Manual listener registration and cleanup
4. **Value conversion**: Repeated conversion between types and `var`
5. **Default values**: Hard to ensure consistent defaults
6. **Encapsulation**: State structure exposed to all consumers

## Solution Architecture

### Core Pattern

Wrapper classes that:
1. Own a `ValueTree` state
2. Use `CachedValue<T>` for type-safe property access
3. Implement `ValueTree::Listener` for change notification
4. Provide domain-specific API methods
5. Hide property IDs in dedicated namespace

### Three Wrapper Types

| **Wrapper** | **Purpose** | **Lifetime** | **Listeners** |
|-------------|-------------|--------------|---------------|
| **EditViewState** | Global edit view settings | Per-Edit | No |
| **ZoomViewState** | Timeline zoom and pan | Per-Edit | Yes |
| **TrackViewState** | Per-track view state | Per-Track | Yes |

**File**: `src/controllers/EditState.h`

## Pattern 1: ZoomViewState

### Purpose
Manages timeline zoom (time-per-pixel) and pan (start time) with playback scrolling support.

### Implementation

```cpp
class ZoomViewState :
    private FlaggedAsyncUpdater,
    private ValueTree::Listener
{
public:
    // Listener interface for change notifications
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void zoomChanged() = 0;
        virtual void zoomOrPosChanged() {}
    };

    ZoomViewState(tracktion::Edit& e);

    // API methods
    void zoomHorizontally(double factor);
    void setRange(tracktion::TimeRange range);
    tracktion::TimePosition getStart() const noexcept;
    tracktion::TimeDuration getTimePerPixel() const;
    float timeToX(tracktion::TimePosition time) const;
    tracktion::TimePosition xToTime(int x) const;

    // Listener management
    void addListener(Listener* l);
    void removeListener(Listener* l);

    tracktion::Edit& edit;

private:
    ValueTree state;
    CachedValue<tracktion::TimePosition> viewStartTime;
    CachedValue<tracktion::TimeDuration> viewTimePerPixel;
    CachedValue<double> viewY;
    ListenerList<Listener> listeners;

    // Transient state (not persisted)
    std::atomic<int> viewWidthPx = 1024;

    bool updateZoom = false, updatePos = false;

    void valueTreePropertyChanged(ValueTree&, const Identifier& prop) override;
    void handleAsyncUpdate() override;
};
```

**File**: `src/controllers/EditState.h:40-100`

### Key Features

#### 1. Persisted State with CachedValue

```cpp
CachedValue<tracktion::TimePosition> viewStartTime;
CachedValue<tracktion::TimeDuration> viewTimePerPixel;
CachedValue<double> viewY;
```

**Benefits**:
- Type-safe access: `viewStartTime.get()` returns `TimePosition`
- Automatic persistence: Changes written to ValueTree
- No manual conversion: JUCE handles serialization

#### 2. Transient State

```cpp
std::atomic<int> viewWidthPx = 1024;  // not stored in ValueTree
```

**Use case**: Runtime state that shouldn't persist (e.g., window width)

#### 3. Async Change Notification

```cpp
void ZoomViewState::valueTreePropertyChanged(ValueTree&, const Identifier& prop) {
    if (prop == IDs::viewStartTime) updatePos = true;
    if (prop == IDs::viewTimePerPixel) updateZoom = true;

    triggerAsyncUpdate();  // Coalesce multiple changes
}

void ZoomViewState::handleAsyncUpdate() {
    if (updateZoom) listeners.call(&Listener::zoomChanged);
    if (updatePos) listeners.call(&Listener::zoomOrPosChanged);

    updateZoom = updatePos = false;
}
```

**Benefits**:
- **Coalescing**: Multiple rapid changes trigger one update
- **Message thread**: Listeners called on message thread (UI safe)
- **Flag-based**: Distinguish zoom vs pan changes

#### 4. Time/Space Conversion

```cpp
float ZoomViewState::timeToX(tracktion::TimePosition time) const {
    auto offset = time - getStart();
    return (float)(offset.inSeconds() / getTimePerPixel().inSeconds());
}

tracktion::TimePosition ZoomViewState::xToTime(int x) const {
    return getStart() + getTimePerPixel() * x;
}
```

**Use case**: Convert between timeline time and pixel coordinates

### Usage Example

```cpp
class TimelineComponent : public Component,
                          public ZoomViewState::Listener
{
public:
    TimelineComponent(EditViewState& evs)
        : zoomState(evs.zoom)
    {
        zoomState.addListener(this);
    }

    ~TimelineComponent() override {
        zoomState.removeListener(this);
    }

    void paint(Graphics& g) override {
        // Convert time to pixels
        float x = zoomState.timeToX(clipStart);
        float width = zoomState.durationToPixels(clipLength);

        g.fillRect(x, 0, width, getHeight());
    }

    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override {
        // Zoom with mouse wheel
        zoomState.zoomHorizontally(1.0 + wheel.deltaY);
    }

    void zoomChanged() override {
        repaint();  // Redraw timeline on zoom
    }

private:
    ZoomViewState& zoomState;
};
```

## Pattern 2: EditViewState

### Purpose
Manages global edit view preferences and tempo/frame-rate calculations.

### Implementation

```cpp
class EditViewState {
public:
    EditViewState(tracktion::Edit& e, tracktion::SelectionManager& s);

    // Persisted boolean flags
    CachedValue<bool> showMasterTrack, showGlobalTrack, showMarkerTrack,
                      showChordTrack, showArrangerTrack, drawWaveforms,
                      showHeaders, showMidiDevices, showWaveDevices;

    // Persisted layout values
    CachedValue<int> headersWidth;

    ValueTree state;
    ZoomViewState zoom;  // Nested zoom state
    tracktion::SelectionManager& selectionManager;
    tracktion::Edit& edit;

    // Domain-specific calculations
    tracktion::TimeDuration getBeatLengthFor(double bpm) const;
    double getFramesPerBeatFor(double bpm) const;
    double getCurrentFramesPerBeat() const;
    double getFramesPerNote(size_t divider) const;
    double getBpmForBeatLength(tracktion::TimeDuration beatLen) const;
    double getBpmSnappedToFps(double bpm) const;
    double setBpmSnappedToFps(double bpm);

    void setBeatLength(tracktion::TimeDuration beatLen);
    void setFramesPerBeat(int fpb);
};
```

**File**: `src/controllers/EditState.h:103-131`

### Key Features

#### 1. Flat CachedValue Properties

```cpp
CachedValue<bool> showMasterTrack, showGlobalTrack, showMarkerTrack;
```

**No listeners**: UI binds directly to `CachedValue::Value` or polls values

**Usage**:
```cpp
// In UI component
checkbox.getToggleStateValue().referTo(editViewState.showMasterTrack.getPropertyAsValue());
```

#### 2. Composition over Inheritance

```cpp
ZoomViewState zoom;  // Nested wrapper
```

**Benefits**:
- Clear ownership hierarchy
- Separate listener interfaces
- Independent lifecycle

#### 3. Domain-Specific Logic

```cpp
double EditViewState::getFramesPerBeatFor(double bpm) const {
    auto beatLen = getBeatLengthFor(bpm);
    return beatLen.inSeconds() * edit.engine.getDeviceManager().getSampleRate();
}

double EditViewState::getFramesPerNote(size_t divider) const {
    return getCurrentFramesPerBeat() / divider;
}
```

**Use case**: MoTool-specific tempo/frame calculations for AY chip music

### Usage Example

```cpp
class TrackListComponent : public Component {
public:
    TrackListComponent(EditViewState& evs) : editViewState(evs) {
        // Bind checkbox to state
        showMasterCheckbox.getToggleStateValue().referTo(
            evs.showMasterTrack.getPropertyAsValue()
        );

        addAndMakeVisible(showMasterCheckbox);
    }

    void resized() override {
        // Use persisted header width
        int headerW = editViewState.headersWidth.get();
        headerArea.setBounds(0, 0, headerW, getHeight());
        tracksArea.setBounds(headerW, 0, getWidth() - headerW, getHeight());
    }

private:
    EditViewState& editViewState;
    ToggleButton showMasterCheckbox;
};
```

## Pattern 3: TrackViewState

### Purpose
Manages per-track view state (currently just height, extensible for future properties).

### Implementation

```cpp
class TrackViewState : private ValueTree::Listener {
public:
    TrackViewState(ValueTree trackState, UndoManager* undoManager);
    ~TrackViewState() override;

    int getHeight() const noexcept;
    void setTrackHeight(int h);

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void trackViewStateChanged() = 0;
    };

    void addListener(Listener* l);
    void removeListener(Listener* l);

    ComponentBoundsConstrainer& getConstrainer();

private:
    ValueTree state;
    CachedValue<int> height;
    ComponentBoundsConstrainer constrainer;  // For resizing
    ListenerList<Listener> listeners;

    static ValueTree ensure(ValueTree trackState);
    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;
};
```

**File**: `src/controllers/EditState.h:138-170`

### Key Features

#### 1. Lazy State Creation

```cpp
ValueTree TrackViewState::ensure(ValueTree trackState) {
    auto vs = trackState.getOrCreateChildWithName(IDs::VIEWSTATE, nullptr);
    return vs;
}

TrackViewState::TrackViewState(ValueTree trackState, UndoManager* undoManager)
    : state(ensure(trackState))
{
    height.referTo(state, IDs::height, undoManager, 60);  // Default 60px
}
```

**Benefits**:
- View state created on-demand
- Parent ValueTree (track) owns lifecycle
- Default values provided at binding time

#### 2. Bounds Constrainer Integration

```cpp
ComponentBoundsConstrainer& TrackViewState::getConstrainer() {
    constrainer.setMinimumHeight(30);
    constrainer.setMaximumHeight(500);
    return constrainer;
}
```

**Use case**: Enforce min/max track heights during resize

#### 3. Change Notification

```cpp
void TrackViewState::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    if (v == state && id == IDs::height) {
        listeners.call(&Listener::trackViewStateChanged);
    }
}
```

### Usage Example

```cpp
class TrackHeaderComponent : public Component,
                              public TrackViewState::Listener
{
public:
    TrackHeaderComponent(Track& t, TrackViewState& tvs)
        : track(t), trackViewState(tvs)
    {
        trackViewState.addListener(this);

        // Use constrainer for resize
        resizer.setConstrainer(&trackViewState.getConstrainer());
        addAndMakeVisible(resizer);
    }

    ~TrackHeaderComponent() override {
        trackViewState.removeListener(this);
    }

    void resized() override {
        int h = trackViewState.getHeight();
        setSize(getWidth(), h);
    }

    void trackViewStateChanged() override {
        resized();  // Update layout when height changes
    }

private:
    Track& track;
    TrackViewState& trackViewState;
    ResizableEdgeComponent resizer { this, &trackViewState.getConstrainer(), ResizableEdgeComponent::bottomEdge };
};
```

## Property ID Management

### Centralized ID Declarations

```cpp
namespace IDs {
    #define DECLARE_ID(name)  const Identifier name(#name);

    // EditViewState IDs
    DECLARE_ID(EDITVIEWSTATE)
    DECLARE_ID(showMasterTrack)
    DECLARE_ID(showGlobalTrack)
    DECLARE_ID(headersWidth)

    // ZoomViewState IDs
    DECLARE_ID(ZOOMVIEWSTATE)
    DECLARE_ID(viewStartTime)
    DECLARE_ID(viewTimePerPixel)
    DECLARE_ID(viewY)

    // TrackViewState IDs
    DECLARE_ID(VIEWSTATE)
    DECLARE_ID(height)

    #undef DECLARE_ID
}
```

**File**: `src/controllers/EditState.h:9-32`

**Benefits**:
- Single source of truth
- No string literal duplication
- Easy to grep for usage
- Namespace prevents collisions

## State Hierarchy Example

```
Edit (ValueTree)
├── EDITVIEWSTATE
│   ├── showMasterTrack: true
│   ├── showGlobalTrack: false
│   ├── headersWidth: 200
│   └── ZOOMVIEWSTATE
│       ├── viewStartTime: 0.0
│       ├── viewTimePerPixel: 0.01
│       └── viewY: 0.0
└── TRACK (multiple)
    ├── name: "Track 1"
    ├── ...
    └── VIEWSTATE
        └── height: 80
```

## Best Practices

### ✅ DO

1. **Use CachedValue**: Leverage type safety and auto-persistence
2. **Centralize IDs**: Define all Identifiers in `IDs` namespace
3. **Provide defaults**: Always specify default values in `referTo()`
4. **Async updates**: Use `FlaggedAsyncUpdater` for listener notification
5. **Transient atomics**: Use `std::atomic<T>` for non-persisted runtime state
6. **Composition**: Nest related wrappers (e.g., zoom inside edit state)
7. **Listener cleanup**: Always remove listeners in destructor

### ❌ DON'T

1. **Raw ValueTree access**: Avoid `state.setProperty()` outside wrapper
2. **String literals**: Don't use `"propertyName"` strings, use `IDs::propertyName`
3. **Synchronous listeners**: Don't call listeners from ValueTree callback (use async)
4. **Forget defaults**: Always provide default values to prevent uninitialized state
5. **Leak listeners**: Always remove listeners to prevent crashes

## Thread Safety

### Message Thread Requirements

- **CachedValue reads/writes**: Message thread only
- **Listener calls**: Always on message thread (via `AsyncUpdater`)
- **ValueTree modifications**: Message thread only

### Atomic Transients

```cpp
std::atomic<int> viewWidthPx = 1024;
```

- Safe to read/write from any thread
- Not persisted to ValueTree
- Use for runtime caches

## Testing Strategy

```cpp
class ZoomViewStateTest : public juce::UnitTest {
public:
    ZoomViewStateTest() : UnitTest("ZoomViewState", "MoTool") {}

    void runTest() override {
        beginTest("Time to pixel conversion");

        // Setup
        auto engine = createTestEngine();
        auto edit = createTestEdit(engine);
        ZoomViewState zoom(*edit);

        zoom.setStart(tracktion::TimePosition::fromSeconds(0.0));
        zoom.setTimePerPixel(tracktion::TimeDuration::fromSeconds(0.01));
        zoom.setViewWidthPx(1000);

        // Test
        auto x = zoom.timeToX(tracktion::TimePosition::fromSeconds(5.0));
        expectEquals(x, 500.0f);

        auto time = zoom.xToTime(250);
        expectEquals(time.inSeconds(), 2.5);
    }
};
```

## Performance Characteristics

- **Property access**: `O(1)` via `CachedValue` (no tree lookup)
- **Change notification**: Coalesced via async update (one UI update per frame)
- **Memory overhead**: Small (one ValueTree + cached values per wrapper)
- **Serialization**: Automatic (ValueTree handles persistence)

## Related Patterns

- **Parameter Binding**: Similar CachedValue usage for parameters
- **Custom Clips**: Clips can have their own view state wrappers
- **Plugin UI Adapters**: UI components consume view state

## References

- **Implementation**: `src/controllers/EditState.h`
- **IDs**: `src/models/Ids.h` (may include additional IDs)
- **JUCE ValueTree docs**: https://docs.juce.com/master/classValueTree.html
- **JUCE CachedValue docs**: https://docs.juce.com/master/classCachedValue.html
